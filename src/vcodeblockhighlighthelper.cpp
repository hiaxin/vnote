#include "vcodeblockhighlighthelper.h"

#include "hgmarkdownhighlighter.h"
#include "vdocument.h"

VCodeBlockHighlightHelper::VCodeBlockHighlightHelper(HGMarkdownHighlighter *p_highlighter,
                                                     VDocument *p_vdoc,
                                                     MarkdownConverterType p_type)
    : QObject(p_highlighter)
{

}
