#include "tag.h"
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
    return QString(std::format("{:%H:%M:%S}", duration).c_str());
}

// NOTE: Ensure a model takes ownership of returned item
// to handle it's memory
QList<QStandardItem*> song_to_item(const QString& filepath)
{
    TagLib::FileRef f{filepath.toUtf8().constData()};
    if(f.isNull())
        return QList<QStandardItem*>();
    // Hold album art, title, artist, album, length, track number
    QList<QStandardItem*> row(6);
    row[0] = new QStandardItem(QIcon(get_image_data(f)), "");
    row[0]->setSizeHint(QSize(64, 64));
    row[1] = new QStandardItem(QString(f.tag()->title().toCString(true)));
    row[2] = new QStandardItem(QString(f.tag()->artist().toCString(true)));
    row[3] = new QStandardItem(QString(f.tag()->album().toCString(true)));
    row[4] = new QStandardItem(format_duration_seconds(f.audioProperties()->lengthInSeconds()));
    row[5] = new QStandardItem(QString(std::to_string((f.tag()->track())).c_str()));
    return row;
}

QPixmap get_image_data(const TagLib::FileRef& f)
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
