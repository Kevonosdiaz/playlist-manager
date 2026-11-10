#include "tooltiplabel.h"

#include <QFontMetrics>
#include <algorithm>

// Ensure long text does not force layout to change
QSize ToolTipLabel::minimumSizeHint() const
{
    QSize s = QLabel::minimumSizeHint();
    s.setWidth(0);
    return s;
}

void ToolTipLabel::resizeEvent(QResizeEvent* e)
{
    QLabel::resizeEvent(e);
    update_elision();
}

void ToolTipLabel::setText(const QString& s)
{
    ftext = s;
    update_elision();
}

const QString& ToolTipLabel::text() const
{
    return ftext;
}

void ToolTipLabel::update_elision()
{
    using namespace Qt::StringLiterals;
    const int available_width      = contentsRect().width() - 2 * margin() - std::max(indent(), 0);
    const QFontMetrics fm          = this->fontMetrics();
    const QString      elided_text = fm.elidedText(ftext, Qt::ElideRight, available_width);
    QLabel::setText(elided_text);

    // Only set tooltip when elision is in effect
    if(elided_text != ftext)
        // Wrap tooltip in HTML tags to force rich text wrapping
        setToolTip(u"<p>"_s + ftext.toHtmlEscaped() + u"</p>"_s);
    else
        setToolTip({});
}
