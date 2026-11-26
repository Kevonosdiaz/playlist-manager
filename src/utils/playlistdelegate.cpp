#include "playlistdelegate.h"
#include "../media/playlistmodel.h"

#include <QApplication>
#include <QMouseEvent>
#include <QPainter>

namespace
{
    constexpr int icon_size = 24;
}

using namespace Qt::StringLiterals;
PlaylistDelegate::PlaylistDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
    , pencil(u":/icons/pencil-svgrepo-com.svg"_s)
    , pencil_filled(u":/icons/pencil-svgrepo-com-filled.svg"_s)
{
}

// Left clicks within icon's rect should trigger edit mode
bool PlaylistDelegate::editorEvent(QEvent*                     event,
                                   QAbstractItemModel*         model,
                                   const QStyleOptionViewItem& option,
                                   const QModelIndex&          idx)
{
    // Only handle/pass on mouse button events
    const auto etype = event->type();
    if(etype != QEvent::MouseButtonPress && etype != QEvent::MouseButtonRelease
       && etype != QEvent::MouseButtonDblClick)
        return QStyledItemDelegate::editorEvent(event, model, option, idx);
    QMouseEvent* me = static_cast<QMouseEvent*>(event);

    // NOTE: Returning true here swallows right click to avoid playlist load
    if(me->button() == Qt::RightButton)
        return true;
    if(me->button() != Qt::LeftButton)
        return false;

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, idx);
    // Determine where in the row the icon is drawn
    const QWidget* w     = opt.widget;
    const QStyle*  style = w ? w->style() : QApplication::style();
    const QRect    rect  = style->subElementRect(QStyle::SE_ItemViewItemDecoration, &opt, w);
    if(!rect.contains(me->position().toPoint()))
        return QStyledItemDelegate::editorEvent(event, model, option, idx);

    // Toggle edit mode of this playlist row (@ idx) since click release was on icon
    // PlaylistModel::setData should handle interaction with existing playlist being edited, and repainting edit icons as appropriate
    if(etype == QEvent::MouseButtonRelease)
    {
        bool old_val = idx.data(PlaylistModel::IsEditTargetRole).toBool();
        model->setData(idx, !old_val, PlaylistModel::IsEditTargetRole);
    }
    return true;
}

// Force rows exactly viewport-wide, so names elide (...) instead of scrolling
QSize PlaylistDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& idx) const
{
    return {0, QStyledItemDelegate::sizeHint(option, idx).height()};
}

void PlaylistDelegate::initStyleOption(QStyleOptionViewItem* opt, const QModelIndex& idx) const
{
    QStyledItemDelegate::initStyleOption(opt, idx); // text, font, palette, selection state

    const bool target = idx.data(PlaylistModel::IsEditTargetRole).toBool();
    opt->icon         = target ? pencil_filled : pencil;
    opt->features |= QStyleOptionViewItem::HasDecoration;
    opt->decorationPosition = QStyleOptionViewItem::Right;
    opt->decorationSize     = QSize(icon_size, icon_size);
    // Hide line that appears on focus, since it does not span across icon area
    opt->state &= ~QStyle::State_HasFocus;
}
