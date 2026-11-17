#include "playlistmodel.h"
#include "../utils/settings.h"

PlaylistModel::PlaylistModel(QObject* parent)
    : QAbstractListModel(parent) { };

int PlaylistModel::rowCount(const QModelIndex& parent) const
{
    return (parent.isValid()) ? 0 : static_cast<int>(playlists.size());
}

QVariant PlaylistModel::data(const QModelIndex& idx, int role) const
{
    if(!idx.isValid() || idx.row() < 0 || idx.row() >= playlists.size())
    {
        return {};
    }

    const Playlist& pl = playlists[idx.row()];

    switch(role)
    {
    case Qt::ToolTipRole:
    case Role::PathRole:
        return pl.get_path();
    case Qt::DisplayRole:
        return pl.get_name();
    }

    return QVariant{};
}

bool PlaylistModel::removeRows(int row, int count, const QModelIndex& parent)
{
    // NOTE: Flat lists should have no valid parent
    if(parent.isValid() || count <= 0 || row < 0 || row + count > playlists.size())
        return false;
    beginRemoveRows(QModelIndex(), row, row + count - 1);
    playlists.remove(row, count);
    endRemoveRows();
    return true;
}

void PlaylistModel::append(Playlist pl)
{
    int row = static_cast<int>(playlists.size());
    beginInsertRows(QModelIndex(), row, row);
    playlists.append(std::move(pl));
    endInsertRows();
}

const Playlist& PlaylistModel::at(int row) const
{
    return playlists.at(row);
}

int PlaylistModel::find(const QString& path) const
{
    for(int i = 0; i < playlists.size(); ++i)
    {
        if(playlists.at(i).get_path() == path)
            return i;
    }
    return -1;
}

void PlaylistModel::set_songs(int row, QVector<Song> other_songs)
{
    // TODO: emit and use dataChanged signal
    if(row >= playlists.size())
        return;
    Playlist& pl = playlists[row];
    pl.set_songs(std::move(other_songs));
}

void PlaylistModel::save_playlists()
{
    QSettings s;
    s.beginWriteArray(Settings::p_playlists);
    for(int i = 0; i < playlists.size(); ++i)
    {
        s.setArrayIndex(i);
        s.setValue(Settings::p_playlist_path, playlists.at(i).get_path());
    }
    s.endArray();
}

void PlaylistModel::restore_playlists()
{
    QSettings s;
    int       size = s.beginReadArray(Settings::p_playlists);
    for(int i = 0; i < size; ++i)
    {
        s.setArrayIndex(i);
        QString   path = s.value(Settings::p_playlist_path).toString();
        QFileInfo fi{path};
        // Remove missing files & dupes
        if(fi.exists() && find(path) == -1)
            append(Playlist{path});
    }
    s.endArray();
}
