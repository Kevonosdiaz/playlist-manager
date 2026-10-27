#ifndef PLAYLIST_H
#define PLAYLIST_H

#include "song.h"
#include <QString>
#include <QVector>

class Playlist
{
public:
    explicit Playlist(QString& filepath);

private:
    inline static uint32_t num_playlists = 0;
    QString                fp;
    std::optional<QString> name;
    bool                   is_ext = false;
    QVector<Song>          songs;

    void parse_m3u();
    void tag_all_songs();
};

#endif /* PLAYLIST_H */
