#include "songtablemodel.h"
#include <QIcon>
#include <algorithm>
#include <chrono>
#include <format>

namespace
{
    // Helper to convert seconds to min:sec format
    QString format_duration_seconds(uint32_t sec)
    {
        std::chrono::seconds duration(sec);
        if(sec >= 3600)
            return QString(std::format("{:%H:%M:%S}", duration).c_str());
        return QString(std::format("{:%M:%S}", duration).c_str());
    }
}

SongTableModel::SongTableModel(QObject* parent)
    : QAbstractTableModel(parent) { };

int SongTableModel::rowCount(const QModelIndex& parent) const
{
    return (parent.isValid()) ? 0 : static_cast<int>(songs.size());
}

int SongTableModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return Columns;
}

QVariant SongTableModel::data(const QModelIndex& idx, int role) const
{
    if(!idx.isValid() || idx.row() >= songs.size() || idx.column() >= columnCount())
        return {};
    const Song&  song = songs[idx.row()];
    const Column col  = static_cast<Column>(idx.column());

    // Return different cell information based on role passed
    switch(role)
    {
    case Qt::DecorationRole:
        if(col == AlbumArt && !song.cover.isNull())
            return QIcon(song.cover);
        return {};
    case Qt::DisplayRole:
        switch(col)
        {
        case Title:
            return song.title;
        case Artist:
            return song.artist;
        case Album:
            return song.album;
        case Length:
            return format_duration_seconds(song.duration_sec);
        case Track:
            return (song.track) ? song.track : QVariant{};
        default:
            return QVariant{};
        }
    }

    return QVariant{};
}

QVariant SongTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation != Qt::Horizontal || role != Qt::DisplayRole || section < 0
       || section >= Columns)
        return {};
    Column col = static_cast<Column>(section);
    switch(col)
    {
    case AlbumArt:
        return "Cover";
    case Title:
        return "Title";
    case Artist:
        return "Artist";
    case Album:
        return "Album";
    case Length:
        return "Length";
    case Track:
        return "Track";
    default:
        return {};
    }
}

void SongTableModel::sort(int column, Qt::SortOrder order)
{
    Column     col = static_cast<Column>(column);
    const bool asc = order == Qt::AscendingOrder;
    std::sort(songs.begin(), songs.end(), [&](const Song& a, const Song& b) {
        switch(col)
        {
        case Title:
            return asc ? a.title < b.title : b.title < a.title;
        case Artist:
            return asc ? a.artist < b.artist : b.artist < a.artist;
        case Album:
            return asc ? a.album < b.album : b.album < a.album;
        case Length:
            return asc ? a.duration_sec < b.duration_sec : b.duration_sec < a.duration_sec;
        case Track:
            return asc ? a.track < b.track : b.track < a.track;
        default:
            return false;
        }
    });

    emit layoutChanged();
}

// Replace the currently stored songs with other_songs
void SongTableModel::set_songs(QVector<Song> other_songs)
{
    beginResetModel();
    songs = std::move(other_songs);
    endResetModel();
}

// Append other_songs onto songs
void SongTableModel::add_songs(QVector<Song> other_songs)
{
    if(other_songs.empty())
        return;
    // Inform model of first and last indices of new items to be added
    int first = static_cast<int>(songs.size());
    int last  = static_cast<int>(other_songs.size()) + first - 1;
    beginInsertRows(QModelIndex(), first, last);
    songs += other_songs;
    endInsertRows();
}
