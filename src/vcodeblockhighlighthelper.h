#ifndef VCODEBLOCKHIGHLIGHTHELPER_H
#define VCODEBLOCKHIGHLIGHTHELPER_H

#include <QObject>
#include <QList>
#include "vconfigmanager.h"

class VDocument;

class VCodeBlockHighlightHelper : public QObject
{
    Q_OBJECT
public:
    VCodeBlockHighlightHelper(HGMarkdownHighlighter *p_highlighter,
                              VDocument *p_vdoc, MarkdownConverterType p_type);

signals:

private slots:
    void handleCodeBlocksUpdated(const QList<VCodeBlock> &p_codeBlocks);
    void handleTextHighlightResult(const QString &p_html, int p_id);

private:
    HGMarkdownHighlighter *m_highlighter;
    VDocument *m_vdocument;
    MarkdownConverterType m_type;
};

#endif // VCODEBLOCKHIGHLIGHTHELPER_H
