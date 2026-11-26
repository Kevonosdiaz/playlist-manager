#ifndef PLAYLISTMODEL_H
#define PLAYLISTMODEL_H

#include "playlist.h"

#include <QAbstractListModel>
#include <QString>

// Custom model for listing and interacting with playlists
class PlaylistModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role
    {
        PathRole         = Qt::UserRole + 1,
        IsEditTargetRole = Qt::UserRole + 2
    };
    explicit PlaylistModel(QObject* parent = nullptr);

    int      rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& idx, int role = Qt::DisplayRole) const override;
    bool     setData(const QModelIndex& idx, const QVariant& val, int role = Qt::EditRole) override;
    bool     removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
    void     append(Playlist pl);
    const Playlist& at(int row) const;
    QModelIndex     first() const;
    QModelIndex     last() const;
    int             find(const QString& path) const;
    void            set_songs(int row, QVector<Song> other_songs);
    void            save_playlists();
    void            restore_playlists();

private:
    QList<Playlist>       playlists;
    QPersistentModelIndex edit_target;
};

#endif /* PLAYLISTMODEL_H */
