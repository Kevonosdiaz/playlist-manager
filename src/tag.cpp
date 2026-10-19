#include "tag.h"
#include "logger.h"
#include "util.h"
#include <QByteArray>
#include <QPixmap>
#include <QStandardItem>
#include <QString>
#include <chrono>
#include <format>
#include <taglib/fileref.h>

// Helper to convert seconds to min:sec format
QString format_duration_seconds(const int sec)
{
    std::chrono::seconds duration(sec);
    if(sec >= 3600)
        return QString(std::format("{:%H:%M:%S}", duration).c_str());
    return QString(std::format("{:%M:%S}", duration).c_str());
}

// NOTE: Ensure a model takes ownership of returned item
// to handle it's memory
// Fetch song metadata into row of items to append to a QStandardItemModel
QList<QStandardItem*> song_to_item(const QString& filepath)
{
    TagLib::FileRef f{filepath.toUtf8().constData()};
    if(f.isNull())
        return QList<QStandardItem*>();
    // Hold album art, title, artist, album, length, track number
    QList<QStandardItem*> row(6);

    const TagLib::Tag*             tag  = f.tag();
    const TagLib::AudioProperties* prop = f.audioProperties();

    if(!tag || !prop)
    {
        logger::log(QString("Unable to read song metadata for %1").arg(filepath), LogLevel::Info);
        return {};
    }

    row[0] = new QStandardItem(QIcon(get_image_data(f)), "");
    row[0]->setSizeHint(QSize(ART_SIZE, ART_SIZE));
    row[1] = new QStandardItem(QString(tag->title().toCString(true)));
    row[2] = new QStandardItem(QString(tag->artist().toCString(true)));
    row[3] = new QStandardItem(QString(tag->album().toCString(true)));

    // Set integer property to use for sorting instead of string
    int sec = prop->lengthInSeconds();
    row[4]  = new QStandardItem(format_duration_seconds(sec));
    row[4]->setData(sec, Qt::UserRole);

    int track = tag->track();
    row[5]    = new QStandardItem(QString::number(track));
    row[5]->setData(track, Qt::UserRole);

    // Disable editing items
    for(const auto& item : std::as_const(row))
    {
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    }
    return row;
}

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
