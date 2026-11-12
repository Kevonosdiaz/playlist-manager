#ifndef PLAYLIST_H
#define PLAYLIST_H

#include "song.h"
#include <QDir>
#include <QFuture>
#include <QString>
#include <QVector>

class Playlist
{
public:
    explicit Playlist(const QString& filepath);
    bool                 empty() const;
    const QVector<Song>& get_songs() const;
    void                 set_songs(QVector<Song> other);
    QString              get_name() const;
    QString              get_path() const;
    QFuture<Song>        tag_all_songs() const;
    size_t               size() const;

private:
    inline static uint32_t num_playlists = 0;
    QString                fp;
    QString                name;
    bool                   is_ext = false;
    QVector<Song>          songs;

    void        parse_m3u();
    static Song tag_song(Song s, const QDir& base_dir);
};

#endif /* PLAYLIST_H */
