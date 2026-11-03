// Tests for path_to_song() (src/media/tag.cpp): reading metadata and cover art via TagLib.

#include "media/tag.h"
#include "testutils.h"
#include "utils/constants.h"

#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

class TestTag : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir tmp;

    QString path(const QString& name) const { return tmp.filePath(name); }

private slots:
    void initTestCase()
    {
        QVERIFY(tmp.isValid());
    }

    void missing_file_returns_nullopt()
    {
        QVERIFY(!path_to_song(path(u"does-not-exist.mp3"_s)).has_value());
    }

    void unsupported_extension_returns_nullopt()
    {
        const QString p = path(u"notes.txt"_s);
        QVERIFY(testutils::write_file(p, "plain text"));
        QVERIFY(!path_to_song(p).has_value());
    }

    void non_audio_file_returns_nullopt()
    {
        // Extension says audio, contents do not
        const QString p = path(u"not-really.mp3"_s);
        QVERIFY(testutils::write_file(p, "this is plain text, not audio"));
        QEXPECT_FAIL("",
                     "TagLib opens a .mp3 with no MPEG frames as valid, so a 0:00 'song' is "
                     "returned (could check audioProperties()->lengthInMilliseconds() / bitrate())",
                     Abort);
        QVERIFY(!path_to_song(p).has_value());
    }

    void untagged_file_reads_audio_properties()
    {
        const QString p = path(u"untagged.wav"_s);
        QVERIFY(testutils::write_silent_wav(p, 2));

        const auto song = path_to_song(p);
        QVERIFY(song.has_value());
        QCOMPARE(song->path, p);
        QCOMPARE(song->duration_sec, 2u);
        QVERIFY(song->title.isEmpty());
        QVERIFY(song->artist.isEmpty());
        QVERIFY(song->album.isEmpty());
        QCOMPARE(song->track, 0u);
        QVERIFY(song->cover.isNull());
    }

    void tagged_file_reads_all_fields()
    {
        const QString p = path(u"tagged.wav"_s);
        QVERIFY(testutils::write_silent_wav(p, 3));
        QVERIFY(testutils::write_tags(
            p, {.title = u"Title"_s, .artist = u"Artist"_s, .album = u"Album"_s, .track = 7}));

        const auto song = path_to_song(p);
        QVERIFY(song.has_value());
        QCOMPARE(song->title, u"Title"_s);
        QCOMPARE(song->artist, u"Artist"_s);
        QCOMPARE(song->album, u"Album"_s);
        QCOMPARE(song->track, 7u);
        QCOMPARE(song->duration_sec, 3u);
    }

    // Tag text passes through TagLib::String -> UTF-8 -> QString; make sure nothing is mangled
    void non_ascii_tags_round_trip()
    {
        const QString p = path(u"unicode.wav"_s);
        QVERIFY(testutils::write_silent_wav(p, 1));
        QVERIFY(testutils::write_tags(p,
                                      {.title  = u"Jóga – 日本語"_s,
                                       .artist = u"Björk"_s,
                                       .album  = u"Homogénic"_s}));

        const auto song = path_to_song(p);
        QVERIFY(song.has_value());
        QCOMPARE(song->title, u"Jóga – 日本語"_s);
        QCOMPARE(song->artist, u"Björk"_s);
        QCOMPARE(song->album, u"Homogénic"_s);
    }

    void non_ascii_file_path()
    {
        const QString p = path(u"Sigur Rós – Hoppípolla.wav"_s);
        QVERIFY(testutils::write_silent_wav(p, 1));
        QVERIFY(testutils::write_tags(p, {.title = u"Hoppípolla"_s}));

        const auto song = path_to_song(p);
        QVERIFY(song.has_value());
        QCOMPARE(song->title, u"Hoppípolla"_s);
    }

    void front_cover_is_loaded_at_icon_size()
    {
        const QString p = path(u"cover.wav"_s);
        QVERIFY(testutils::write_silent_wav(p, 1));

        QImage img(500, 500, QImage::Format_RGB32);
        img.fill(Qt::red);
        QVERIFY(testutils::write_picture(p, img, u"Front Cover"_s));

        const auto song = path_to_song(p);
        QVERIFY(song.has_value());
        QVERIFY(!song->cover.isNull());
        QCOMPARE(song->cover.size(), QSize(ART_SIZE, ART_SIZE));
        QCOMPARE(song->cover.pixelColor(0, 0), QColor(Qt::red));
    }

    void non_square_cover_keeps_aspect_ratio()
    {
        const QString p = path(u"wide-cover.wav"_s);
        QVERIFY(testutils::write_silent_wav(p, 1));
        QVERIFY(testutils::write_picture(p, QImage(400, 200, QImage::Format_RGB32), u"Front Cover"_s));

        const auto song = path_to_song(p);
        QVERIFY(song.has_value());
        QEXPECT_FAIL("",
                     "setScaledSize(ART_SIZE x ART_SIZE) stretches non-square art to a square",
                     Abort);
        QCOMPARE(song->cover.size(), QSize(ART_SIZE, ART_SIZE / 2));
    }

    void no_cover_without_front_cover_picture()
    {
        // Documents current behaviour (see TODO in tag.cpp): only "Front Cover" is used.
        // Update this test when the fallback to other picture types is added.
        const QString p = path(u"other-pic.wav"_s);
        QVERIFY(testutils::write_silent_wav(p, 1));
        QVERIFY(testutils::write_picture(p, QImage(10, 10, QImage::Format_RGB32), u"Other"_s));

        const auto song = path_to_song(p);
        QVERIFY(song.has_value());
        QVERIFY(song->cover.isNull());
    }
};

QTEST_MAIN(TestTag)
#include "tst_tag.moc"
