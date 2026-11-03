#include "tag.h"
#include "../utils/constants.h"
#include "../utils/logger.h"
#include <QBuffer>
#include <QByteArray>
#include <QImageReader>
#include <QPixmap>
#include <QString>
#include <taglib/fileref.h>

namespace
{

    // Retrieves embedded album art from file using TagLib
    QImage get_image_data(const TagLib::FileRef& f)
    {
        QImage                           res;
        TagLib::List<TagLib::VariantMap> pictures = f.complexProperties("PICTURE");
        if(pictures.isEmpty())
            return res;
        TagLib::VariantMap cover_pic;
        // TODO: Check for other types of pictures as fallback
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

        // TODO: Do we need to scale ART_SIZE by monitor scaling?
        // Load in image data, scaled down to icon size
        TagLib::ByteVector img_data = cover_pic["data"].value<TagLib::ByteVector>();
        QImageReader       reader;
        QByteArray         qba = QByteArray::fromRawData(img_data.data(), img_data.size());
        QBuffer            buf(&qba);
        buf.open(QIODevice::ReadOnly);
        reader.setDevice(&buf);
        reader.setScaledSize(
            reader.size().scaled(ART_SIZE, ART_SIZE, Qt::AspectRatioMode::KeepAspectRatio));
        res = reader.read();

        if(res.isNull())
        {
            logger::log(
                QString("Encountered error when fetching album art: %1").arg(reader.errorString()),
                LogLevel::Info);
            return {};
        }

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
    song.duration_sec = static_cast<uint32_t>(prop->lengthInSeconds());
    song.track        = tag->track();

    return song;
}
