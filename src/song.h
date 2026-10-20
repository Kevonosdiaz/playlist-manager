#ifndef SONG_H
#define SONG_H

#include <QAbstractTableModel>
#include <QPixmap>
#include <QString>

// Consider replacing QPixmap in the future for an alternative
struct Song
{
    QString  title, artist, album, path;
    QPixmap  img;
    uint32_t duration_sec;
    uint32_t track = 0;
};

#endif /* SONG_H */
