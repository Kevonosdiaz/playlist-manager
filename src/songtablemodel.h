#ifndef SONGTABLEMODEL_H
#define SONGTABLEMODEL_H

#include "song.h"
#include <QAbstractTableModel>
#include <QPixmap>
#include <QString>
#include <QVector>

// Custom model for specific sorting features in song viewer
class SongTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    // NOTE: Columns is a sentinel value
    enum Column
    {
        AlbumArt = 0,
        Title,
        Artist,
        Album,
        Length,
        Track,
        Columns
    };

    explicit SongTableModel(QObject* parent = nullptr);

    int      rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int      columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int             section,
                        Qt::Orientation orientation,
                        int             role = Qt::DisplayRole) const override;
    void     sort(int column, Qt::SortOrder order) override;

    void set_songs(QVector<Song> other_songs);

private:
    QVector<Song> songs;
};

#endif /* SONGTABLEMODEL_H */
