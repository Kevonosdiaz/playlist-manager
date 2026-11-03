#ifndef TESTUTILS_H
#define TESTUTILS_H

// Helpers shared by the test executables. Test audio files are generated at
// runtime instead of being committed as binary fixtures.

#include <QBuffer>
#include <QByteArray>
#include <QDataStream>
#include <QFile>
#include <QImage>
#include <QString>

#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tvariant.h>

namespace testutils
{
    // Write raw bytes to a file, returning false on failure
    inline bool write_file(const QString& path, const QByteArray& contents)
    {
        QFile f(path);
        if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return false;
        return f.write(contents) == contents.size();
    }

    // Write a silent, 8-bit mono PCM WAV file lasting `seconds` at 8 kHz.
    // Small enough to generate per test, but a real file TagLib can parse and tag.
    inline bool write_silent_wav(const QString& path, quint32 seconds)
    {
        constexpr quint32 sample_rate = 8000;
        constexpr quint16 channels    = 1;
        constexpr quint16 bits        = 8;
        const quint32     data_size   = sample_rate * seconds * channels * (bits / 8);

        QByteArray  bytes;
        QDataStream out(&bytes, QIODevice::WriteOnly);
        out.setByteOrder(QDataStream::LittleEndian);

        out.writeRawData("RIFF", 4);
        out << quint32(36 + data_size);
        out.writeRawData("WAVE", 4);

        out.writeRawData("fmt ", 4);
        out << quint32(16);                                 // fmt chunk size
        out << quint16(1);                                  // PCM
        out << channels;
        out << sample_rate;
        out << quint32(sample_rate * channels * bits / 8);  // byte rate
        out << quint16(channels * bits / 8);                // block align
        out << bits;

        out.writeRawData("data", 4);
        out << data_size;
        bytes.append(QByteArray(static_cast<qsizetype>(data_size), char(0x80))); // 8-bit silence

        return write_file(path, bytes);
    }

    inline TagLib::String to_tstring(const QString& s)
    {
        return TagLib::String(s.toUtf8().constData(), TagLib::String::UTF8);
    }

    struct TagData
    {
        QString  title, artist, album;
        unsigned track = 0;
    };

    // Write basic tags to an existing audio file using TagLib directly
    inline bool write_tags(const QString& path, const TagData& data)
    {
        TagLib::FileRef f(QFile::encodeName(path).constData());
        if(f.isNull() || !f.tag())
            return false;
        f.tag()->setTitle(to_tstring(data.title));
        f.tag()->setArtist(to_tstring(data.artist));
        f.tag()->setAlbum(to_tstring(data.album));
        f.tag()->setTrack(data.track);
        return f.save();
    }

    // Embed `image` as PNG picture with the given TagLib picture type (e.g. "Front Cover")
    inline bool write_picture(const QString& path, const QImage& image, const QString& picture_type)
    {
        QByteArray png;
        QBuffer    buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        if(!image.save(&buffer, "PNG"))
            return false;

        TagLib::FileRef f(QFile::encodeName(path).constData());
        if(f.isNull())
            return false;

        TagLib::VariantMap picture;
        picture["data"]        = TagLib::ByteVector(png.constData(), static_cast<unsigned>(png.size()));
        picture["mimeType"]    = TagLib::String("image/png");
        picture["pictureType"] = to_tstring(picture_type);
        picture["description"] = TagLib::String("test");

        if(!f.setComplexProperties("PICTURE", {picture}))
            return false;
        return f.save();
    }
}

#endif /* TESTUTILS_H */
