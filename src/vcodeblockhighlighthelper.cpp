#include "vcodeblockhighlighthelper.h"

#include <QDebug>
#include "vdocument.h"
#include "hgmarkdownhighlighter.h"
#include "vdocument.h"

VCodeBlockHighlightHelper::VCodeBlockHighlightHelper(HGMarkdownHighlighter *p_highlighter,
                                                     VDocument *p_vdoc,
                                                     MarkdownConverterType p_type)
    : QObject(p_highlighter), m_highlighter(p_highlighter), m_vdocument(p_vdoc),
      m_type(p_type)
{
    connect(p_highlighter, &HGMarkdownHighlighter::codeBlocksUpdated,
            this, &VCodeBlockHighlightHelper::handleCodeBlocksUpdated);
    connect(m_vdocument, &VDocument::textHighlighted,
            this, &VCodeBlockHighlightHelper::handleTextHighlightResult);
}

void VCodeBlockHighlightHelper::handleCodeBlocksUpdated(const QList<VCodeBlock> &p_codeBlocks)
{
    for (int i = 0; i < p_codeBlocks.size(); ++i) {
        const VCodeBlock &item = p_codeBlocks[i];
        m_vdocument->highlightTextAsync(item.m_text + "\n", item.m_startBlock);
    }
}

void VCodeBlockHighlightHelper::handleTextHighlightResult(const QString &p_html,
                                                          int p_id)
{
    qDebug() << "highlighted result" << p_html << p_id;
}
