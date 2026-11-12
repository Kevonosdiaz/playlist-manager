#include "playlistmodel.h"

PlaylistModel::PlaylistModel(QObject* parent)
    : QAbstractListModel(parent) { };

int PlaylistModel::rowCount(const QModelIndex& parent) const
{
    return (parent.isValid()) ? 0 : static_cast<int>(playlists.size());
}

QVariant PlaylistModel::data(const QModelIndex& idx, int role) const
{
    if(!idx.isValid() || idx.row() >= playlists.size())
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

void PlaylistModel::append(Playlist pl)
{
    int first = static_cast<int>(pl.size());
    int last  = first;
    beginInsertRows(QModelIndex(), first, last);
    playlists.append(pl);
    endInsertRows();
}

const Playlist& PlaylistModel::at(int row) const
{
    return playlists.at(row);
}

void PlaylistModel::set_songs(int row, QVector<Song> other_songs)
{
    if(row >= playlists.size())
        return;
    Playlist& pl = playlists[row];
    pl.set_songs(std::move(other_songs));
}
