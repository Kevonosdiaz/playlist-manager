#include "playlist.h"
#include "tag.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QTextStream>

using namespace Qt::StringLiterals;

Playlist::Playlist(const QString& filepath)
    : fp(filepath)
{
    // TODO: Construct from saved information in QSettings?
    num_playlists++;
    // name = QString("Playlist %1").arg(num_playlists);
    QFileInfo fi(filepath);
    name = fi.baseName();
    parse_m3u();
    tag_all_songs();
}

bool Playlist::empty() const
{
    return songs.empty();
}

const QVector<Song>& Playlist::get_songs() const
{
    return songs;
}

const QString Playlist::get_name() const
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

        for(const auto& line : lines)
        {
            // #EXTINF:123,Artist Name – Track Title\n
            // artist - title.mp3
            if(line.startsWith(u"#"_s) && !is_ext)
            {
                // Log out info that # was found without beginning extension
                // Could be due to added byte order mark or other data causing #EXTM3U to not show at start of first line?
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
                // Attempt to split by Artist - Title, other just Title
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
                continue;
            }
            else
            {
                // Treat entire line as file path, add current song info to songs
                s.path = line.trimmed();
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

// Override data in Playlist.songs with TagLib provided metadata
void Playlist::tag_all_songs()
{
    if(songs.isEmpty())
        return;

    QFileInfo playlist_info(fp);
    QDir      base_dir(playlist_info.absolutePath());

    // TODO: Call TagLib with multiple threads
    for(auto& song : songs)
    {
        QFileInfo song_info(base_dir, song.path);
        auto      res = path_to_song(song_info.absoluteFilePath());
        if(!res)
        {
            continue;
        }
        // Use TagLib metadata with M3U metadata as fallback
        if(!res->title.isEmpty())
            song.title = res->title;
        if(!res->artist.isEmpty())
            song.artist = res->artist;
        if(res->duration_sec)
            song.duration_sec = res->duration_sec;
        song.cover = res->cover;
        song.album = res->album;
        song.track = res->track;
    }

    // Emit signal that playlist has changed (if we call this after modification?)
}
