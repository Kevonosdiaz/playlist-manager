#ifndef SONG_H
#define SONG_H

#include <QPixmap>
#include <QString>
#include <QUrl>

// Consider replacing QPixmap in the future for an alternative
struct Song
{
    QString  title, artist, album;
    QUrl     url;
    QPixmap  cover;
    uint32_t duration_sec = 0;
    uint32_t track        = 0;
};

#endif /* SONG_H */
