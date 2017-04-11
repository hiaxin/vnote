#ifndef HGMARKDOWNHIGHLIGHTER_H
#define HGMARKDOWNHIGHLIGHTER_H

#include <QTextCharFormat>
#include <QSyntaxHighlighter>
#include <QAtomicInt>
#include <QSet>
#include <QList>
#include <QString>

extern "C" {
#include <pmh_parser.h>
}

QT_BEGIN_NAMESPACE
class QTextDocument;
QT_END_NAMESPACE

struct HighlightingStyle
{
    pmh_element_type type;
    QTextCharFormat format;
};

enum HighlightBlockState
{
    Normal = 0,
    CodeBlock = 1,
};

// One continuous region for a certain markdown highlight style
// within a QTextBlock.
// Pay attention to the change of HighlightingStyles[]
struct HLUnit
{
    // Highlight offset @start and @length with style HighlightingStyles[styleIndex]
    // within a QTextBlock
    unsigned long start;
    unsigned long length;
    unsigned int styleIndex;
};

struct VCodeBlock
{
    int m_startBlock;
    int m_endBlock;
    QString m_lang;

    QString m_text;
};

class HGMarkdownHighlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    HGMarkdownHighlighter(const QVector<HighlightingStyle> &styles, int waitInterval,
                          QTextDocument *parent = 0);
    ~HGMarkdownHighlighter();
    void setStyles(const QVector<HighlightingStyle> &styles);
    // Request to update highlihgt (re-parse and re-highlight)
    void updateHighlight();

signals:
    void highlightCompleted();
    void imageBlocksUpdated(QSet<int> p_blocks);
    void codeBlocksUpdated(const QList<VCodeBlock> &p_codeBlocks);

protected:
    void highlightBlock(const QString &text) Q_DECL_OVERRIDE;

private slots:
    void handleContentChange(int position, int charsRemoved, int charsAdded);
    void timerTimeout();

private:
    QRegExp codeBlockStartExp;
    QRegExp codeBlockEndExp;
    QTextCharFormat codeBlockFormat;
    QTextCharFormat m_linkFormat;
    QTextCharFormat m_imageFormat;

    QTextDocument *document;
    QVector<HighlightingStyle> highlightingStyles;
    QVector<QVector<HLUnit> > blockHighlights;
    // Block numbers containing image link(s).
    QSet<int> imageBlocks;
    QAtomicInt parsing;
    QTimer *timer;
    int waitInterval;

    // All the code blocks info.
    QList<VCodeBlock> m_codeBlocks;

    char *content;
    int capacity;
    pmh_element **result;

    static const int initCapacity;

    void resizeBuffer(int newCap);
    void highlightCodeBlock(const QString &text);
    void highlightLinkWithSpacesInURL(const QString &p_text);
    void parse();
    void parseInternal();
    void initBlockHighlightFromResult(int nrBlocks);
    void initBlockHighlihgtOne(unsigned long pos, unsigned long end,
                               int styleIndex);
    void updateImageBlocks();
    void updateCodeBlocks();
};

#endif
