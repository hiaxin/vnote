#include <QtWidgets>
#include "vmdedit.h"
#include "hgmarkdownhighlighter.h"
#include "vcodeblockhighlighthelper.h"
#include "vmdeditoperations.h"
#include "vnote.h"
#include "vconfigmanager.h"
#include "vtoc.h"
#include "utils/vutils.h"
#include "dialog/vselectdialog.h"

extern VConfigManager vconfig;
extern VNote *g_vnote;

enum ImageProperty { ImagePath = 1 };

VMdEdit::VMdEdit(VFile *p_file, VDocument *p_vdoc, MarkdownConverterType p_type,
                 QWidget *p_parent)
    : VEdit(p_file, p_parent), m_mdHighlighter(NULL), m_previewImage(true)
{
    Q_ASSERT(p_file->getDocType() == DocType::Markdown);

    setAcceptRichText(false);
    m_mdHighlighter = new HGMarkdownHighlighter(vconfig.getMdHighlightingStyles(),
                                                500, document());
    connect(m_mdHighlighter, &HGMarkdownHighlighter::highlightCompleted,
            this, &VMdEdit::generateEditOutline);
    connect(m_mdHighlighter, &HGMarkdownHighlighter::imageBlocksUpdated,
            this, &VMdEdit::updateImageBlocks);

    m_cbHighlighter = new VCodeBlockHighlightHelper(m_mdHighlighter, p_vdoc,
                                                    p_type);

    m_editOps = new VMdEditOperations(this, m_file);
    connect(m_editOps, &VEditOperations::keyStateChanged,
            this, &VMdEdit::handleEditStateChanged);

    connect(this, &VMdEdit::cursorPositionChanged,
            this, &VMdEdit::updateCurHeader);

    connect(this, &VMdEdit::selectionChanged,
            this, &VMdEdit::handleSelectionChanged);
    connect(QApplication::clipboard(), &QClipboard::changed,
            this, &VMdEdit::handleClipboardChanged);

    m_editOps->updateTabSettings();
    updateFontAndPalette();
}

void VMdEdit::updateFontAndPalette()
{
    setFont(vconfig.getMdEditFont());
    setPalette(vconfig.getMdEditPalette());
    m_cursorLineColor = vconfig.getEditorCurrentLineBackground();
}

void VMdEdit::beginEdit()
{
    m_editOps->updateTabSettings();
    updateFontAndPalette();

    Q_ASSERT(m_file->getContent() == toPlainTextWithoutImg());

    initInitImages();

    setReadOnly(false);
    setModified(false);

    // Request update outline.
    generateEditOutline();
}

void VMdEdit::endEdit()
{
    setReadOnly(true);
    clearUnusedImages();
}

void VMdEdit::saveFile()
{
    if (!document()->isModified()) {
        return;
    }
    m_file->setContent(toPlainTextWithoutImg());
    document()->setModified(false);
}

void VMdEdit::reloadFile()
{
    const QString &content = m_file->getContent();
    Q_ASSERT(content.indexOf(QChar::ObjectReplacementCharacter) == -1);
    setPlainText(content);
    setModified(false);
}

void VMdEdit::keyPressEvent(QKeyEvent *event)
{
    if (m_editOps->handleKeyPressEvent(event)) {
        return;
    }
    VEdit::keyPressEvent(event);
}

bool VMdEdit::canInsertFromMimeData(const QMimeData *source) const
{
    return source->hasImage() || source->hasUrls()
           || VEdit::canInsertFromMimeData(source);
}

void VMdEdit::insertFromMimeData(const QMimeData *source)
{
    VSelectDialog dialog(tr("Insert From Clipboard"), this);
    dialog.addSelection(tr("Insert As Image"), 0);
    dialog.addSelection(tr("Insert As Text"), 1);

    if (source->hasImage()) {
        // Image data in the clipboard
        if (source->hasText()) {
            if (dialog.exec() == QDialog::Accepted) {
                if (dialog.getSelection() == 1) {
                    // Insert as text.
                    Q_ASSERT(source->hasText() && source->hasImage());
                    VEdit::insertFromMimeData(source);
                    return;
                }
            } else {
                return;
            }
        }
        m_editOps->insertImageFromMimeData(source);
        return;
    } else if (source->hasUrls()) {
        QList<QUrl> urls = source->urls();
        if (urls.size() == 1 && VUtils::isImageURL(urls[0])) {
            if (dialog.exec() == QDialog::Accepted) {
                // FIXME: After calling dialog.exec(), source->hasUrl() returns false.
                if (dialog.getSelection() == 0) {
                    // Insert as image.
                    m_editOps->insertImageFromURL(urls[0]);
                    return;
                }
                QMimeData newSource;
                newSource.setUrls(urls);
                VEdit::insertFromMimeData(&newSource);
                return;
            } else {
                return;
            }
        }
    } else if (source->hasText()) {
        QString text = source->text();
        if (VUtils::isImageURLText(text)) {
            // The text is a URL to an image.
            if (dialog.exec() == QDialog::Accepted) {
                if (dialog.getSelection() == 0) {
                    // Insert as image.
                    QUrl url(text);
                    if (url.isValid()) {
                        m_editOps->insertImageFromURL(QUrl(text));
                    }
                    return;
                }
            } else {
                return;
            }
        }
        Q_ASSERT(source->hasText());
    }
    VEdit::insertFromMimeData(source);
}

void VMdEdit::imageInserted(const QString &p_name)
{
    m_insertedImages.append(p_name);
}

void VMdEdit::initInitImages()
{
    m_initImages = VUtils::imagesFromMarkdownFile(m_file->retrivePath());
}

void VMdEdit::clearUnusedImages()
{
    QVector<QString> images = VUtils::imagesFromMarkdownFile(m_file->retrivePath());

    if (!m_insertedImages.isEmpty()) {
        QVector<QString> imageNames(images.size());
        for (int i = 0; i < imageNames.size(); ++i) {
            imageNames[i] = VUtils::fileNameFromPath(images[i]);
        }

        QDir dir = QDir(m_file->retriveImagePath());
        for (int i = 0; i < m_insertedImages.size(); ++i) {
            QString name = m_insertedImages[i];
            int j;
            for (j = 0; j < imageNames.size(); ++j) {
                if (name == imageNames[j]) {
                    break;
                }
            }

            // Delete it
            if (j == imageNames.size()) {
                QString imagePath = dir.filePath(name);
                QFile(imagePath).remove();
                qDebug() << "delete inserted image" << imagePath;
            }
        }
        m_insertedImages.clear();
    }

    for (int i = 0; i < m_initImages.size(); ++i) {
        QString imagePath = m_initImages[i];
        int j;
        for (j = 0; j < images.size(); ++j) {
            if (imagePath == images[j]) {
                break;
            }
        }

        // Delete it
        if (j == images.size()) {
            QFile(imagePath).remove();
            qDebug() << "delete existing image" << imagePath;
        }
    }
    m_initImages.clear();
}

void VMdEdit::updateCurHeader()
{
    int curHeader = 0;
    QTextCursor cursor(this->textCursor());
    int curLine = cursor.block().firstLineNumber();
    int i = 0;
    for (i = m_headers.size() - 1; i >= 0; --i) {
        if (m_headers[i].lineNumber <= curLine) {
            curHeader = m_headers[i].lineNumber;
            break;
        }
    }
    emit curHeaderChanged(curHeader, i == -1 ? 0 : i);
}

void VMdEdit::generateEditOutline()
{
    QTextDocument *doc = document();
    m_headers.clear();
    // Assume that each block contains only one line
    // Only support # syntax for now
    QRegExp headerReg("(#{1,6})\\s*(\\S.*)");  // Need to trim the spaces
    int lastLevel = 0;
    for (QTextBlock block = doc->begin(); block != doc->end(); block = block.next()) {
        Q_ASSERT(block.lineCount() == 1);
        if ((block.userState() == HighlightBlockState::Normal) &&
            headerReg.exactMatch(block.text())) {
            int level = headerReg.cap(1).length();
            VHeader header(level, headerReg.cap(2).trimmed(),
                           "", block.firstLineNumber());
            while (level > lastLevel + 1) {
                // Insert empty level.
                m_headers.append(VHeader(++lastLevel, "[EMPTY]",
                                         "", block.firstLineNumber()));
            }
            m_headers.append(header);
            lastLevel = level;
        }
    }

    emit headersChanged(m_headers);
    updateCurHeader();
}

void VMdEdit::scrollToHeader(int p_headerIndex)
{
    Q_ASSERT(p_headerIndex >= 0);
    if (p_headerIndex < m_headers.size()) {
        int line = m_headers[p_headerIndex].lineNumber;
        qDebug() << "scroll editor to" << p_headerIndex << "line" << line;
        scrollToLine(line);
    }
}

void VMdEdit::updateImageBlocks(QSet<int> p_imageBlocks)
{
    if (!m_previewImage) {
        return;
    }
    // We need to handle blocks backward to avoid shifting all the following blocks.
    // Inserting the preview image block may cause highlighter to emit signal again.
    QList<int> blockList = p_imageBlocks.toList();
    std::sort(blockList.begin(), blockList.end(), std::greater<int>());
    auto it = blockList.begin();
    while (it != blockList.end()) {
        previewImageOfBlock(*it);
        ++it;
    }

    // Clean up un-referenced QChar::ObjectReplacementCharacter.
    clearOrphanImagePreviewBlock();

    emit statusChanged();
}

void VMdEdit::clearOrphanImagePreviewBlock()
{
    QTextDocument *doc = document();
    QTextBlock block = doc->begin();
    while (block.isValid()) {
        if (isOrphanImagePreviewBlock(block)) {
            qDebug() << "remove orphan image preview block" << block.blockNumber();
            QTextBlock nextBlock = block.next();
            removeBlock(block);
            block = nextBlock;
        } else {
            clearCorruptedImagePreviewBlock(block);
            block = block.next();
        }
    }
}

bool VMdEdit::isOrphanImagePreviewBlock(QTextBlock p_block)
{
    if (isImagePreviewBlock(p_block)) {
        // It is an orphan image preview block if previous block is not
        // a block need to preview (containing exactly one image) or the image
        // paths are not equal to each other.
        QTextBlock prevBlock = p_block.previous();
        if (prevBlock.isValid()) {
            QString imageLink = fetchImageToPreview(prevBlock.text());
            if (imageLink.isEmpty()) {
                return true;
            }
            QString imagePath = QDir(m_file->retriveBasePath()).filePath(imageLink);

            // Get image preview block's image path.
            QTextCursor cursor(p_block);
            int shift = p_block.text().indexOf(QChar::ObjectReplacementCharacter);
            if (shift > 0) {
                cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor,
                                    shift + 1);
            }
            QTextImageFormat format = cursor.charFormat().toImageFormat();
            Q_ASSERT(format.isValid());
            QString curPath = format.property(ImagePath).toString();

            return curPath != imagePath;
        } else {
            return true;
        }
    }
    return false;
}

void VMdEdit::clearCorruptedImagePreviewBlock(QTextBlock p_block)
{
    if (!p_block.isValid()) {
        return;
    }
    QString text = p_block.text();
    QVector<int> replacementChars;
    bool onlySpaces = true;
    for (int i = 0; i < text.size(); ++i) {
        if (text[i] == QChar::ObjectReplacementCharacter) {
            replacementChars.append(i);
        } else if (!text[i].isSpace()) {
            onlySpaces = false;
        }
    }
    if (!onlySpaces && !replacementChars.isEmpty()) {
        // ObjectReplacementCharacter mixed with other non-space texts.
        // Users corrupt the image preview block. Just remove the char.
        QTextCursor cursor(p_block);
        int blockPos = p_block.position();
        for (int i = replacementChars.size() - 1; i >= 0; --i) {
            int pos = replacementChars[i];
            cursor.setPosition(blockPos + pos);
            cursor.deleteChar();
        }
        Q_ASSERT(text.remove(QChar::ObjectReplacementCharacter) == p_block.text());
    }
}

void VMdEdit::clearAllImagePreviewBlocks()
{
    QTextDocument *doc = document();
    QTextBlock block = doc->begin();
    bool modified = isModified();
    while (block.isValid()) {
        if (isImagePreviewBlock(block)) {
            QTextBlock nextBlock = block.next();
            removeBlock(block);
            block = nextBlock;
        } else {
            clearCorruptedImagePreviewBlock(block);
            block = block.next();
        }
    }
    setModified(modified);
    emit statusChanged();
}

QString VMdEdit::fetchImageToPreview(const QString &p_text)
{
    QRegExp regExp("\\!\\[[^\\]]*\\]\\((images/[^/\\)]+)\\)");
    int index = regExp.indexIn(p_text);
    if (index == -1) {
        return QString();
    }
    int lastIndex = regExp.lastIndexIn(p_text);
    if (lastIndex != index) {
        return QString();
    }
    return regExp.capturedTexts()[1];
}

void VMdEdit::previewImageOfBlock(int p_block)
{
    QTextDocument *doc = document();
    QTextBlock block = doc->findBlockByNumber(p_block);
    if (!block.isValid()) {
        return;
    }

    QString text = block.text();
    QString imageLink = fetchImageToPreview(text);
    if (imageLink.isEmpty()) {
        return;
    }
    QString imagePath = QDir(m_file->retriveBasePath()).filePath(imageLink);
    qDebug() << "block" << p_block << "image" << imagePath;

    if (isImagePreviewBlock(p_block + 1)) {
        updateImagePreviewBlock(p_block + 1, imagePath);
        return;
    }
    insertImagePreviewBlock(p_block, imagePath);
}

bool VMdEdit::isImagePreviewBlock(int p_block)
{
    QTextDocument *doc = document();
    QTextBlock block = doc->findBlockByNumber(p_block);
    if (!block.isValid()) {
        return false;
    }
    QString text = block.text().trimmed();
    return text == QString(QChar::ObjectReplacementCharacter);
}

bool VMdEdit::isImagePreviewBlock(QTextBlock p_block)
{
    if (!p_block.isValid()) {
        return false;
    }
    QString text = p_block.text().trimmed();
    return text == QString(QChar::ObjectReplacementCharacter);
}

void VMdEdit::insertImagePreviewBlock(int p_block, const QString &p_image)
{
    QTextDocument *doc = document();

    QImage image(p_image);
    if (image.isNull()) {
        return;
    }

    // Store current status.
    bool modified = isModified();
    int pos = textCursor().position();

    QTextCursor cursor(doc->findBlockByNumber(p_block));
    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::EndOfBlock);
    cursor.insertBlock();

    QTextImageFormat imgFormat;
    imgFormat.setName(p_image);
    imgFormat.setProperty(ImagePath, p_image);
    cursor.insertImage(imgFormat);
    Q_ASSERT(cursor.block().text().at(0) == QChar::ObjectReplacementCharacter);
    cursor.endEditBlock();

    QTextCursor tmp = textCursor();
    tmp.setPosition(pos);
    setTextCursor(tmp);
    setModified(modified);
    emit statusChanged();
}

void VMdEdit::updateImagePreviewBlock(int p_block, const QString &p_image)
{
    Q_ASSERT(isImagePreviewBlock(p_block));
    QTextDocument *doc = document();
    QTextBlock block = doc->findBlockByNumber(p_block);
    if (!block.isValid()) {
        return;
    }
    QTextCursor cursor(block);
    int shift = block.text().indexOf(QChar::ObjectReplacementCharacter);
    if (shift > 0) {
        cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, shift + 1);
    }
    QTextImageFormat format = cursor.charFormat().toImageFormat();
    Q_ASSERT(format.isValid());
    QString curPath = format.property(ImagePath).toString();

    if (curPath == p_image) {
        return;
    }
    // Update it with the new image.
    QImage image(p_image);
    if (image.isNull()) {
        // Delete current preview block.
        removeBlock(block);
        qDebug() << "remove invalid image in block" << p_block;
        return;
    }
    format.setName(p_image);
    qDebug() << "update block" << p_block << "to image" << p_image;
}

void VMdEdit::removeBlock(QTextBlock p_block)
{
    QTextCursor cursor(p_block);
    cursor.select(QTextCursor::BlockUnderCursor);
    cursor.removeSelectedText();
}

QString VMdEdit::toPlainTextWithoutImg() const
{
    QString text = toPlainText();
    int start = 0;
    do {
        int index = text.indexOf(QChar::ObjectReplacementCharacter, start);
        if (index == -1) {
            break;
        }
        start = removeObjectReplacementLine(text, index);
    } while (start > -1 && start < text.size());
    return text;
}

int VMdEdit::removeObjectReplacementLine(QString &p_text, int p_index) const
{
    Q_ASSERT(p_text.size() > p_index && p_text.at(p_index) == QChar::ObjectReplacementCharacter);
    int prevLineIdx = p_text.lastIndexOf('\n', p_index);
    if (prevLineIdx == -1) {
        prevLineIdx = 0;
    }
    // Remove [\n....?]
    p_text.remove(prevLineIdx, p_index - prevLineIdx + 1);
    return prevLineIdx - 1;
}

void VMdEdit::handleEditStateChanged(KeyState p_state)
{
    qDebug() << "edit state" << (int)p_state;
    if (p_state == KeyState::Normal) {
        m_cursorLineColor = vconfig.getEditorCurrentLineBackground();
    } else {
        m_cursorLineColor = vconfig.getEditorCurrentLineVimBackground();
    }
    highlightCurrentLine();
}

void VMdEdit::handleSelectionChanged()
{
    QString text = textCursor().selectedText();
    if (text.isEmpty() && !m_previewImage) {
        m_previewImage = true;
        m_mdHighlighter->updateHighlight();
    } else if (m_previewImage) {
        if (text.trimmed() == QString(QChar::ObjectReplacementCharacter)) {
            // Select the image and some whitespaces.
            // We can let the user copy the image.
            return;
        } else if (text.contains(QChar::ObjectReplacementCharacter)) {
            m_previewImage = false;
            clearAllImagePreviewBlocks();
        }
    }
}

void VMdEdit::handleClipboardChanged(QClipboard::Mode p_mode)
{
    if (!hasFocus()) {
        return;
    }
    if (p_mode == QClipboard::Clipboard) {
        QClipboard *clipboard = QApplication::clipboard();
        const QMimeData *mimeData = clipboard->mimeData();
        if (mimeData->hasText()) {
            QString text = mimeData->text();
            if (clipboard->ownsClipboard() &&
                (text.trimmed() == QString(QChar::ObjectReplacementCharacter))) {
                QString imagePath = selectedImage();
                qDebug() <<  "clipboard" << imagePath;
                Q_ASSERT(!imagePath.isEmpty());
                QImage image(imagePath);
                Q_ASSERT(!image.isNull());
                clipboard->clear(QClipboard::Clipboard);
                clipboard->setImage(image, QClipboard::Clipboard);
            }
        }
    }
}

QString VMdEdit::selectedImage()
{
    QString imagePath;
    QTextCursor cursor = textCursor();
    if (!cursor.hasSelection()) {
        return imagePath;
    }
    int start = cursor.selectionStart();
    int end = cursor.selectionEnd();
    QTextDocument *doc = document();
    QTextBlock startBlock = doc->findBlock(start);
    QTextBlock endBlock = doc->findBlock(end);
    QTextBlock block = startBlock;
    while (block.isValid()) {
        if (isImagePreviewBlock(block)) {
            QString image = fetchImageToPreview(block.previous().text());
            imagePath = QDir(m_file->retriveBasePath()).filePath(image);
            break;
        }
        if (block == endBlock) {
            break;
        }
        block = block.next();
    }
    return imagePath;
}
