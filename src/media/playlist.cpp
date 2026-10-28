#include "playlist.h"

#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QTextStream>

Playlist::Playlist(QString& filepath)
    : fp(filepath)
{
    num_playlists++;
    // name = QString("Playlist %1").arg(num_playlists);
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

const QString Playlist::get_name() const
{
    return name;
}

void Playlist::parse_m3u()
{
    QFile       f(fp);
    QStringList lines;
    is_ext = false;
    if(f.open(QFile::ReadOnly))
    {
        // Currently use default encoding (UTF-8) only
        // expected for .m3u8 but also most newer files
        QTextStream in(&f);

        // One pass just to load into a list/vector
        while(!in.atEnd())
            lines.append(in.readLine());

        if(lines.isEmpty())
        {
            // Log out info/error to user
            return;
        }

        if(lines.at(0).startsWith("#EXTM3U"))
        {
            is_ext = true;
        }

        Song s;

        for(const auto& line : lines)
        {
            // #EXTINF:123,Artist Name – Track Title\n
            // artist - title.mp3
            if(line.startsWith("#") && !is_ext)
            {
                // Log out info that # was found without beginning extension
                // Could be due to added byte order mark or other data causing #EXTM3U to not show at start of first line?
                continue;
            }
            else if(line.startsWith("#EXTINF"))
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
                if(hyphen_pos != -1 && remainder.contains(" - "))
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
            else if(line.startsWith("#"))
            {
                // Log unsupported directive, or comment
                continue;
            }
            else if(line.startsWith("http://") || line.startsWith("https://")
                    || line.startsWith("rtsp://"))
            {
                // Log unsupported URI type
                continue;
            }
            else
            {
                // Treat line as URI, add current song info to songs
                // TODO: Account for relative vs absolute path
                s.url = QUrl(line);
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

// TODO: Add infrastructure to get absolute paths of URLs

// Override data in Playlist.songs with TagLib provided metadata
void Playlist::tag_all_songs()
{
    if(songs.isEmpty())
        return;
    // Call TagLib with multiple threads
    // Emit signal that playlist has changed
}
