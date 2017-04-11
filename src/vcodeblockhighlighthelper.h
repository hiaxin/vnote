#ifndef VCODEBLOCKHIGHLIGHTHELPER_H
#define VCODEBLOCKHIGHLIGHTHELPER_H

#include <QObject>
#include "vconfigmanager.h"

class HGMarkdownHighlighter;
class VDocument;

class VCodeBlockHighlightHelper : public QObject
{
    Q_OBJECT
public:
    VCodeBlockHighlightHelper(HGMarkdownHighlighter *p_highlighter,
                              VDocument *p_vdoc, MarkdownConverterType p_type);

signals:

public slots:
};

#endif // VCODEBLOCKHIGHLIGHTHELPER_H
