#include "vdocument.h"
#include "vfile.h"
#include <QDebug>

VDocument::VDocument(const VFile *v_file, QObject *p_parent)
    : QObject(p_parent), m_file(v_file)
{
}

void VDocument::updateText()
{
    emit textChanged(m_file->getContent());
}

void VDocument::setToc(const QString &toc)
{
    if (toc == m_toc) {
        return;
    }
    m_toc = toc;
    emit tocChanged(m_toc);
}

QString VDocument::getToc()
{
    return m_toc;
}

void VDocument::scrollToAnchor(const QString &anchor)
{
    emit requestScrollToAnchor(anchor);
}

void VDocument::setHeader(const QString &anchor)
{
    if (anchor == m_header) {
        return;
    }
    m_header = anchor;
    emit headerChanged(m_header);
}

void VDocument::setHtml(const QString &html)
{
    if (html == m_html) {
        return;
    }
    m_html = html;
    emit htmlChanged(m_html);
}

void VDocument::setLog(const QString &p_log)
{
    qDebug() << "JS:" << p_log;
    emit logChanged(p_log);
}

void VDocument::keyPressEvent(int p_key, bool p_ctrl, bool p_shift)
{
    emit keyPressed(p_key, p_ctrl, p_shift);
}

void VDocument::highlightTextAsync(const QString &p_text, int p_id)
{
    qDebug() << "highlightTextAsync" << p_text << p_id;
    emit requestHighlightText(p_text, p_id);
}

void VDocument::highlightTextCB(const QString &p_html, int p_id)
{
    emit textHighlighted(p_html, p_id);
}
