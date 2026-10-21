#include "tag.h"
#include "logger.h"
#include <QByteArray>
#include <QPixmap>
#include <QString>
#include <taglib/fileref.h>

namespace
{

    // Retrieves embedded album art from file using TagLib
    QPixmap get_image_data(const TagLib::FileRef& f)
    {
        QPixmap                          res;
        TagLib::List<TagLib::VariantMap> pictures = f.complexProperties("PICTURE");
        if(pictures.isEmpty())
            return res;
        TagLib::VariantMap cover_pic;
        for(const auto& pic : pictures)
        {
            if(pic["pictureType"].value<TagLib::String>() == "Front Cover")
            {
                cover_pic = pic;
                break;
            }
        }
        if(cover_pic.isEmpty())
            return res;

        // Load in image data to QPixmap
        TagLib::ByteVector img_data = cover_pic["data"].value<TagLib::ByteVector>();
        QByteArray         qba{img_data.data(), img_data.size()};
        res.loadFromData(qba);
        return res;
    }
}

// Fetch song metadata into Song struct
std::optional<Song> path_to_song(const QString& filepath)
{
    TagLib::FileRef f{filepath.toUtf8().constData()};
    if(f.isNull())
        return {};

    Song song;

    const TagLib::Tag*             tag  = f.tag();
    const TagLib::AudioProperties* prop = f.audioProperties();

    if(!tag || !prop)
    {
        logger::log(QString("Unable to read song metadata for %1").arg(filepath), LogLevel::Info);
        return {};
    }

    song.path         = filepath;
    song.cover        = get_image_data(f);
    song.title        = QString(tag->title().toCString(true));
    song.artist       = QString(tag->artist().toCString(true));
    song.album        = QString(tag->album().toCString(true));
    song.duration_sec = prop->lengthInSeconds();
    song.track        = tag->track();

    return song;
}
