#ifndef PLAYLIST_DELEGATE_H
#define PLAYLIST_DELEGATE_H

#include <QIcon>
#include <QStyledItemDelegate>

// Override of QStyledItemDelegate for PlaylistList to display
// edit icons next to playlists

class PlaylistDelegate : public QStyledItemDelegate
{
public:
    explicit PlaylistDelegate(QObject* parent = nullptr);
    bool  editorEvent(QEvent*                     event,
                      QAbstractItemModel*         model,
                      const QStyleOptionViewItem& option,
                      const QModelIndex&          idx) override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& idx) const override;

protected:
    void initStyleOption(QStyleOptionViewItem* opt, const QModelIndex& idx) const override;

private:
    QIcon pencil;
    QIcon pencil_filled;
};

#endif /* PLAYLIST_DELEGATE_H */
