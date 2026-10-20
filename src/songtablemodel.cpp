#include "songtablemodel.h"
#include <QIcon>
#include <algorithm>
#include <functional>

SongTableModel::SongTableModel(QObject* parent)
    : QAbstractTableModel(parent) { };

int SongTableModel::rowCount(const QModelIndex& parent) const
{
    return (parent.isValid()) ? 0 : static_cast<int>(songs.size());
};
int SongTableModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return Columns;
};
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
        if(col == AlbumArt && !song.img.isNull())
            return QIcon(song.img);
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
            return song.duration_sec;
        case Track:
            return (song.track) ? song.track : QVariant{};
        default:
            return QVariant{};
        }
    }

    return QVariant{};
};
QVariant SongTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    Column col = static_cast<Column>(section);
    switch(col)
    {
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
        return "";
    }
};

void SongTableModel::sort(int column, Qt::SortOrder order)
{
    Column col = static_cast<Column>(column);
    // Use type erasure to store either std::less or std::greater in cmp
    std::function<bool(QString, QString)> str_cmp
        = (order == Qt::AscendingOrder)
              ? std::function<bool(QString, QString)>(std::less<QString>{})
              : std::function<bool(QString, QString)>(std::greater<QString>{});
    std::function<bool(int, int)> int_cmp
        = (order == Qt::AscendingOrder) ? std::function<bool(int, int)>(std::less<int>{})
                                        : std::function<bool(int, int)>(std::greater<int>{});
    std::sort(songs.begin(), songs.end(), [&](const Song& a, const Song& b) {
        switch(col)
        {
        case Title:
            return str_cmp(a.title, b.title);
        case Artist:
            return str_cmp(a.artist, b.artist);
        case Album:
            return str_cmp(a.album, b.album);
        case Length:
            return int_cmp(a.duration_sec, b.duration_sec);
        case Track:
            return int_cmp(a.track, b.track);
        default:
            return false;
        }
    });
}
