#ifndef SONG_H
#define SONG_H

#include <QPixmap>
#include <QString>

struct Song
{
    QString  title, artist, album, path;
    QImage   cover;
    uint32_t duration_sec = 0;
    uint32_t track        = 0;
};

#endif /* SONG_H */
