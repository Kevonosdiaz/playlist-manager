#include "playlist.h"
#include "tag.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFuture>
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <QtConcurrentMap>

#include <utility>

using namespace Qt::StringLiterals;

Playlist::Playlist(const QString& filepath)
    : fp(filepath)
{
    // TODO: Construct from saved information in QSettings?
    num_playlists++;
    QFileInfo fi(filepath);
    name = fi.baseName();
    parse_m3u();
}

bool Playlist::empty() const
{
    return songs.empty();
}

const QVector<Song>& Playlist::get_songs() const
{
    return songs;
}

void Playlist::set_songs(QVector<Song> other)
{
    songs = std::move(other);
}

QString Playlist::get_name() const
{
    return name;
}

void Playlist::parse_m3u()
{
    QFile       f(fp);
    QStringList lines;
    is_ext = false;
    if(f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        // Currently use UTF-8 encoding only
        // expected for .m3u8 but also most newer files
        QTextStream in(&f);
        in.setEncoding(QStringConverter::Utf8);

        // One pass just to load into a list/vector
        while(!in.atEnd())
            lines.append(in.readLine());

        if(lines.isEmpty())
        {
            // Log out info/error to user
            return;
        }

        if(lines.at(0).startsWith(u"#EXTM3U"_s))
        {
            is_ext = true;
        }

        Song s;

        for(const auto& line : std::as_const(lines))
        {
            // #EXTINF:123,Artist Name – Track Title\n
            // artist - title.mp3
            if(line.startsWith(u"#"_s) && !is_ext)
            {
                // Log out info that # was found without beginning extension
                continue;
            }
            else if(line.startsWith(u"#EXTINF"_s))
            {
                // Capture number between ':' and ',' + text after ','
                auto colon_pos = line.indexOf(':');
                auto comma_pos = line.indexOf(',');
                // Ignore malformed EXTINF line
                if(colon_pos == -1 || comma_pos == -1)
                {
                    continue;
                }
                s.duration_sec = line.sliced(colon_pos + 1, comma_pos - colon_pos - 1).toUInt();
                // NOTE: remainder may contain extraneous information for other properties besides artist and title
                QString remainder = line.sliced(comma_pos + 1);

                // Attempt to split by Artist - Title, otherwise just Title
                auto hyphen_pos = remainder.indexOf('-');
                if(hyphen_pos != -1 && remainder.contains(u" - "_s))
                {
                    // Assumes first '-' pos is same as one in " - " pattern
                    s.artist = remainder.sliced(0, hyphen_pos).trimmed();
                    s.title  = remainder.sliced(hyphen_pos + 1).trimmed();
                }
                else
                {
                    s.title = remainder;
                }
            }
            else if(line.startsWith(u"#"_s))
            {
                // Log unsupported directive, or comment
                continue;
            }
            else if(line.startsWith(u"http://"_s) || line.startsWith(u"https://"_s)
                    || line.startsWith(u"rtsp://"_s))
            {
                // Log unsupported URI type
                s = {};
                continue;
            }
            else
            {
                // Treat entire line as file path, add current song info to songs
                const auto l = line.trimmed();
                if(l.isEmpty())
                    continue;

                s.path = l;
                songs.push_back(s);
                s = {};
            }
        }
    }
    else
    {
        // Log out error to user
    }
}

// Retag an individual song, to be called concurrently when tagging all songs
Song Playlist::tag_song(Song s, const QDir& base_dir)
{
    QFileInfo song_info(base_dir, s.path);
    auto      res = path_to_song(song_info.absoluteFilePath());

    // Could log/keep count of failed tag attempts
    if(!res)
        return s;

    // Use TagLib metadata with existing M3U metadata as fallback
    if(!res->title.isEmpty())
        s.title = res->title;
    if(!res->artist.isEmpty())
        s.artist = res->artist;
    if(res->duration_sec)
        s.duration_sec = res->duration_sec;
    s.cover = res->cover;
    s.album = res->album;
    s.track = res->track;

    return s;
}

// Override data in Playlist.songs concurrently with TagLib provided metadata
// Hooks up to a QFutureWatcher on GUI side in mainwindow to replace playlist songs
QFuture<Song> Playlist::tag_all_songs() const
{
    const QDir base_dir(QFileInfo(fp).absolutePath());
    return QtConcurrent::mapped(songs, [base_dir](const Song& s) { return tag_song(s, base_dir); });
}
