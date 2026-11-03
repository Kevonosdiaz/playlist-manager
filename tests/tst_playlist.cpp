// Tests for Playlist (src/media/playlist.cpp): M3U parsing and TagLib metadata merging.
//
// Known bugs are recorded with QEXPECT_FAIL so the suite stays green while still
// documenting them. When a bug gets fixed, the check unexpectedly passes (XPASS),
// which Qt Test reports as a failure: delete the QEXPECT_FAIL line at that point.

#include "media/playlist.h"
#include "testutils.h"

#include <QDir>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace Qt::StringLiterals;

class TestPlaylist : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> tmp;

    QString path(const QString& name) const { return tmp->filePath(name); }

    // Write `contents` to a playlist file in this test's temp dir and parse it
    Playlist make_playlist(const QByteArray& contents, const QString& filename = u"test.m3u"_s)
    {
        const QString p = path(filename);
        if(!testutils::write_file(p, contents))
            qFatal("Could not write test playlist %s", qPrintable(p));
        return Playlist(p);
    }

    static QStringList paths(const Playlist& pl)
    {
        QStringList res;
        for(const auto& song : pl.get_songs())
            res << song.path;
        return res;
    }

private slots:
    // Fresh directory per test so song files from one test can't affect another
    void init()
    {
        tmp = std::make_unique<QTemporaryDir>();
        QVERIFY(tmp->isValid());
    }

    void cleanup() { tmp.reset(); }

    // --- File-level behaviour ---------------------------------------------------

    void missing_file_is_empty()
    {
        Playlist pl(path(u"nope.m3u"_s));
        QVERIFY(pl.empty());
    }

    void empty_file_is_empty()
    {
        QVERIFY(make_playlist("").empty());
    }

    void header_only_is_empty()
    {
        QVERIFY(make_playlist("#EXTM3U\n").empty());
    }

    void name_is_file_name_without_extension()
    {
        QCOMPARE(make_playlist("a.mp3\n", u"Road Trip.m3u8"_s).get_name(), u"Road Trip"_s);
    }

    void name_keeps_inner_dots()
    {
        auto pl = make_playlist("a.mp3\n", u"mix.v2.m3u"_s);
        QEXPECT_FAIL("", "Review #9: QFileInfo::baseName() cuts at the first dot", Abort);
        QCOMPARE(pl.get_name(), u"mix.v2"_s);
    }

    // --- Plain (non-extended) M3U -----------------------------------------------

    void plain_m3u_lists_paths_in_order()
    {
        auto pl = make_playlist("one.mp3\nsub/two.flac\n/abs/three.ogg\n");
        QCOMPARE(paths(pl), (QStringList{u"one.mp3"_s, u"sub/two.flac"_s, u"/abs/three.ogg"_s}));
        for(const auto& song : pl.get_songs())
        {
            QVERIFY(song.title.isEmpty());
            QCOMPARE(song.duration_sec, 0u);
        }
    }

    void plain_m3u_ignores_comments_and_extinf()
    {
        // Without the #EXTM3U header, every '#' line is a comment, including #EXTINF
        auto pl = make_playlist("# my playlist\n#EXTINF:10,A - B\none.mp3\n");
        QCOMPARE(paths(pl), QStringList{u"one.mp3"_s});
        QVERIFY(pl.get_songs().first().title.isEmpty());
    }

    void missing_trailing_newline()
    {
        QCOMPARE(paths(make_playlist("one.mp3\ntwo.mp3")),
                 (QStringList{u"one.mp3"_s, u"two.mp3"_s}));
    }

    void crlf_line_endings()
    {
        auto pl = make_playlist("#EXTM3U\r\n#EXTINF:10,Artist - Title\r\none.mp3\r\n");
        QCOMPARE(paths(pl), QStringList{u"one.mp3"_s});
        QCOMPARE(pl.get_songs().first().title, u"Title"_s);
    }

    void blank_lines_are_skipped()
    {
        auto pl = make_playlist("one.mp3\n\ntwo.mp3\n\n");
        QEXPECT_FAIL("", "Review #4: blank lines are added as songs with an empty path", Abort);
        QCOMPARE(paths(pl), (QStringList{u"one.mp3"_s, u"two.mp3"_s}));
    }

    // --- Extended M3U (#EXTINF) ---------------------------------------------------

    void extinf_artist_and_title()
    {
        auto pl = make_playlist("#EXTM3U\n#EXTINF:215,Daft Punk - One More Time\none.mp3\n");
        QCOMPARE(pl.get_songs().size(), 1);
        const Song& s = pl.get_songs().first();
        QCOMPARE(s.artist, u"Daft Punk"_s);
        QCOMPARE(s.title, u"One More Time"_s);
        QCOMPARE(s.duration_sec, 215u);
        QCOMPARE(s.path, u"one.mp3"_s);
    }

    void extinf_title_only()
    {
        auto pl = make_playlist("#EXTM3U\n#EXTINF:60,Just A Title\none.mp3\n");
        const Song& s = pl.get_songs().first();
        QVERIFY(s.artist.isEmpty());
        QCOMPARE(s.title, u"Just A Title"_s);
    }

    void extinf_negative_duration_is_zero()
    {
        // -1 means "unknown length" in the extended M3U convention
        auto pl = make_playlist("#EXTM3U\n#EXTINF:-1,Stream Title\none.mp3\n");
        QCOMPARE(pl.get_songs().first().duration_sec, 0u);
        QCOMPARE(pl.get_songs().first().title, u"Stream Title"_s);
    }

    void extinf_without_comma_is_ignored()
    {
        auto pl = make_playlist("#EXTM3U\n#EXTINF:123\none.mp3\n");
        QCOMPARE(paths(pl), QStringList{u"one.mp3"_s});
        QCOMPARE(pl.get_songs().first().duration_sec, 0u);
    }

    void extinf_applies_only_to_next_entry()
    {
        auto pl = make_playlist("#EXTM3U\n"
                                "#EXTINF:10,A - First\n"
                                "one.mp3\n"
                                "two.mp3\n");
        QCOMPARE(pl.get_songs().size(), 2);
        QCOMPARE(pl.get_songs().at(0).title, u"First"_s);
        QVERIFY(pl.get_songs().at(1).title.isEmpty());
        QCOMPARE(pl.get_songs().at(1).duration_sec, 0u);
    }

    void url_entries_are_skipped_without_leaking_metadata()
    {
        // Regression test for 96883c6: the EXTINF info for a skipped URL must not
        // end up on the next local file
        auto pl = make_playlist("#EXTM3U\n"
                                "#EXTINF:-1,Radio - Stream\n"
                                "https://example.com/stream\n"
                                "#EXTINF:5,Local - Song\n"
                                "http://example.com/a.mp3\n"
                                "rtsp://example.com/b\n"
                                "local.mp3\n");
        QCOMPARE(paths(pl), QStringList{u"local.mp3"_s});
        QVERIFY(pl.get_songs().first().title.isEmpty());
    }

    void utf8_bom_before_header()
    {
        auto pl = make_playlist("\xEF\xBB\xBF#EXTM3U\n#EXTINF:10,Artist - Title\none.mp3\n");
        QCOMPARE(paths(pl), QStringList{u"one.mp3"_s});
        QCOMPARE(pl.get_songs().first().title, u"Title"_s);
    }

    void utf8_text_and_paths()
    {
        auto pl = make_playlist("#EXTM3U\n#EXTINF:10,Björk - Jóga\nBjörk/Jóga.flac\n",
                                u"ünïcode.m3u8"_s);
        QCOMPARE(pl.get_name(), u"ünïcode"_s);
        const Song& s = pl.get_songs().first();
        QCOMPARE(s.artist, u"Björk"_s);
        QCOMPARE(s.title, u"Jóga"_s);
        QCOMPARE(s.path, u"Björk/Jóga.flac"_s);
    }

    void hyphenated_artist()
    {
        auto pl = make_playlist("#EXTM3U\n#EXTINF:10,Jay-Z - 99 Problems\none.mp3\n");
        const Song& s = pl.get_songs().first();
        QEXPECT_FAIL("", "Review #7: splits on the first '-' rather than on \" - \"", Continue);
        QCOMPARE(s.artist, u"Jay-Z"_s);
        QEXPECT_FAIL("", "Review #7: splits on the first '-' rather than on \" - \"", Continue);
        QCOMPARE(s.title, u"99 Problems"_s);
    }

    void unknown_directive_keeps_pending_extinf()
    {
        auto pl = make_playlist("#EXTM3U\n"
                                "#EXTINF:10,Artist - Title\n"
                                "#EXTALB:Some Album\n"
                                "one.mp3\n");
        QCOMPARE(paths(pl), QStringList{u"one.mp3"_s});
        QEXPECT_FAIL("", "Review #6: any other '#' line discards the pending #EXTINF data", Abort);
        QCOMPARE(pl.get_songs().first().title, u"Title"_s);
    }

    // Not tested: "#EXTINF,x:y" (comma before colon). It currently hits a negative
    // QString::sliced() length (review #5), which asserts in debug builds and is UB in release.

    // --- Merging TagLib metadata (tag_all_songs) ----------------------------------

    void tags_override_m3u_metadata()
    {
        QVERIFY(testutils::write_silent_wav(path(u"song.wav"_s), 2));
        QVERIFY(testutils::write_tags(
            path(u"song.wav"_s),
            {.title = u"Tag Title"_s, .artist = u"Tag Artist"_s, .album = u"Tag Album"_s, .track = 3}));

        auto pl = make_playlist("#EXTM3U\n#EXTINF:999,M3U Artist - M3U Title\nsong.wav\n");
        const Song& s = pl.get_songs().first();
        QCOMPARE(s.title, u"Tag Title"_s);
        QCOMPARE(s.artist, u"Tag Artist"_s);
        QCOMPARE(s.album, u"Tag Album"_s);
        QCOMPARE(s.track, 3u);
        QCOMPARE(s.duration_sec, 2u);
    }

    void m3u_metadata_is_fallback_for_empty_tags()
    {
        QVERIFY(testutils::write_silent_wav(path(u"untagged.wav"_s), 2));

        auto pl = make_playlist("#EXTM3U\n#EXTINF:999,M3U Artist - M3U Title\nuntagged.wav\n");
        const Song& s = pl.get_songs().first();
        QCOMPARE(s.title, u"M3U Title"_s);
        QCOMPARE(s.artist, u"M3U Artist"_s);
        QCOMPARE(s.duration_sec, 2u); // the real length beats the playlist's claim
    }

    void missing_song_file_keeps_m3u_metadata()
    {
        auto pl = make_playlist("#EXTM3U\n#EXTINF:999,M3U Artist - M3U Title\nmissing.wav\n");
        const Song& s = pl.get_songs().first();
        QCOMPARE(s.title, u"M3U Title"_s);
        QCOMPARE(s.duration_sec, 999u);
    }

    void relative_paths_resolve_against_playlist_dir()
    {
        // The playlist lives in a subdirectory and the process cwd is elsewhere,
        // so this only passes if paths resolve relative to the playlist file
        QVERIFY(QDir(tmp->path()).mkpath(u"lists/music"_s));
        QVERIFY(testutils::write_silent_wav(path(u"lists/music/song.wav"_s), 1));
        QVERIFY(testutils::write_tags(path(u"lists/music/song.wav"_s), {.title = u"Found"_s}));

        auto pl = make_playlist("music/song.wav\n", u"lists/list.m3u"_s);
        QCOMPARE(pl.get_songs().first().title, u"Found"_s);
    }

    void absolute_paths_are_used_as_is()
    {
        QTemporaryDir other;
        QVERIFY(other.isValid());
        const QString song_path = other.filePath(u"elsewhere.wav"_s);
        QVERIFY(testutils::write_silent_wav(song_path, 1));
        QVERIFY(testutils::write_tags(song_path, {.title = u"Absolute"_s}));

        auto pl = make_playlist(song_path.toUtf8() + "\n");
        QCOMPARE(pl.get_songs().first().title, u"Absolute"_s);
    }
};

QTEST_MAIN(TestPlaylist)
#include "tst_playlist.moc"
