#include "tag.h"
#include <QByteArray>
#include <QPixmap>
#include <QStandardItem>
#include <QString>
#include <taglib/fileref.h>

// NOTE: Ensure a model takes ownership of returned item
// to handle it's memory
QList<QStandardItem*> song_to_item(const QString& filepath)
{
    TagLib::FileRef f{filepath.toUtf8().constData()};
    if(f.isNull())
        return QList<QStandardItem*>();
    // Hold album art, title, artist, album, length, track number
    QList<QStandardItem*> row(6);
    // TODO: Start adding to it
    // row[0] = get_image_data(f);
    return row;
}

QPixmap get_image_data(const TagLib::FileRef f)
{
    TagLib::List<TagLib::VariantMap> pictures = f.complexProperties("PICTURE");
    TagLib::VariantMap               cover_pic;
    for(const auto& pic : pictures)
    {
        if(pic["pictureType"].value<TagLib::String>() == "Front Cover")
            cover_pic = pic;
        break;
    }
    if(cover_pic.isEmpty())
        return QPixmap();
    // Load in image data to QPixmap
    TagLib::ByteVector img_data = cover_pic["data"].value<TagLib::ByteVector>();
    QByteArray         qba{img_data.data(), img_data.size()};
    QPixmap            res;
    res.loadFromData(qba);
    return res;
}