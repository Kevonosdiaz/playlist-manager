// Tests for PlaylistTagger (src/media/playlisttagger.cpp): request coalescing,
// cancellation and applying tagged results back to the PlaylistModel.
//
// The tagger runs Playlist::tag_all_songs() on QtConcurrent's global pool. To make
// "request B while A is still running" deterministic, tests that need a job held in
// flight close a Gate first: the pool is shrunk to one thread and that thread is
// parked, so the tagging job stays queued until the gate opens.

#include "media/playlistmodel.h"
#include "media/playlisttagger.h"
#include "testutils.h"

#include <QSemaphore>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThreadPool>

#include <memory>

using namespace Qt::StringLiterals;

namespace
{
    // Occupies the global pool's only thread until open() (or destruction)
    class Gate
    {
    public:
        Gate()
            : old_max(QThreadPool::globalInstance()->maxThreadCount())
        {
            QThreadPool* pool = QThreadPool::globalInstance();
            pool->waitForDone();
            pool->setMaxThreadCount(1);
            pool->start([this] {
                parked.release();
                release.acquire();
            });
            parked.acquire(); // the single worker is now blocked
        }
        ~Gate()
        {
            open();
            QThreadPool::globalInstance()->waitForDone();
            QThreadPool::globalInstance()->setMaxThreadCount(old_max);
        }
        Gate(const Gate&)            = delete;
        Gate& operator=(const Gate&) = delete;

        void open()
        {
            if(!opened)
                release.release();
            opened = true;
        }

    private:
        int        old_max;
        bool       opened = false;
        QSemaphore parked;
        QSemaphore release;
    };
}

class TestPlaylistTagger : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> tmp;

    QString path(const QString& name) const
    {
        return tmp->filePath(name);
    }

    // Write a one-song playlist `name`.m3u whose WAV carries `title` as its tag, so
    // the M3U title ("M3U <name>") and the tagged title are distinguishable
    void add_playlist(PlaylistModel& model, const QString& name, const QString& title)
    {
        const QString wav = name + u".wav"_s;
        QVERIFY(testutils::write_silent_wav(path(wav), 1));
        QVERIFY(testutils::write_tags(path(wav), {.title = title}));
        const QByteArray m3u
            = "#EXTM3U\n#EXTINF:1,M3U " + name.toUtf8() + "\n" + wav.toUtf8() + "\n";
        QVERIFY(testutils::write_file(path(name + u".m3u"_s), m3u));
        model.append(Playlist{path(name + u".m3u"_s)});
    }

    // Let every queued job finish and deliver the watcher's finished() callouts.
    // on_finished() may start a follow-up job, so loop until the pool stays idle.
    static void drain()
    {
        for(int i = 0; i < 5; ++i)
        {
            QThreadPool::globalInstance()->waitForDone();
            QCoreApplication::processEvents();
            QCoreApplication::sendPostedEvents();
        }
    }

    static QString first_title(const PlaylistModel& model, int row)
    {
        return model.at(row).get_songs().constFirst().title;
    }

private slots:
    void init()
    {
        tmp = std::make_unique<QTemporaryDir>();
        QVERIFY(tmp->isValid());
    }

    void cleanup()
    {
        drain();
        tmp.reset();
    }

    // --- Basic requests ---------------------------------------------------------

    void invalid_index_is_ignored()
    {
        PlaylistModel  model;
        PlaylistTagger tagger(&model);
        QSignalSpy     started(&tagger, &PlaylistTagger::started);

        tagger.request_playlist(QModelIndex{});
        drain();
        QCOMPARE(started.count(), 0);
    }

    void untagged_playlist_is_tagged_and_applied()
    {
        PlaylistModel model;
        add_playlist(model, u"a"_s, u"Tag A"_s);
        PlaylistTagger tagger(&model);
        QSignalSpy     started(&tagger, &PlaylistTagger::started);
        QSignalSpy     tagged(&tagger, &PlaylistTagger::playlist_tagged);

        QVERIFY(!model.at(0).tagged());
        QCOMPARE(first_title(model, 0), u"M3U a"_s);

        tagger.request_playlist(model.index(0));
        QCOMPARE(started.count(), 1); // emitted synchronously from start()
        QVERIFY(tagged.wait());

        QCOMPARE(tagged.count(), 1);
        QCOMPARE(tagged.at(0).at(0).value<QPersistentModelIndex>(),
                 QPersistentModelIndex{model.index(0)});
        QVERIFY(model.at(0).tagged());
        QCOMPARE(first_title(model, 0), u"Tag A"_s);
    }

    void empty_playlist_still_reports_tagged()
    {
        PlaylistModel model;
        QVERIFY(testutils::write_file(path(u"empty.m3u"_s), "#EXTM3U\n"));
        model.append(Playlist{path(u"empty.m3u"_s)});
        PlaylistTagger tagger(&model);
        QSignalSpy     tagged(&tagger, &PlaylistTagger::playlist_tagged);

        // MainWindow relies on this signal to show "Playlist is empty"
        tagger.request_playlist(model.index(0));
        QVERIFY(tagged.wait());
        QVERIFY(model.at(0).tagged());
        QVERIFY(model.at(0).empty());
    }

    void already_tagged_playlist_is_not_retagged()
    {
        PlaylistModel model;
        add_playlist(model, u"a"_s, u"Tag A"_s);
        PlaylistTagger tagger(&model);
        QSignalSpy     started(&tagger, &PlaylistTagger::started);
        QSignalSpy     tagged(&tagger, &PlaylistTagger::playlist_tagged);

        tagger.request_playlist(model.index(0));
        QVERIFY(tagged.wait());

        tagger.request_playlist(model.index(0));
        drain();
        QCOMPARE(started.count(), 1);
        QCOMPARE(tagged.count(), 1);
    }

    // --- Requests while a job is in flight --------------------------------------

    void repeat_request_while_tagging_is_deduplicated()
    {
        PlaylistModel model;
        add_playlist(model, u"a"_s, u"Tag A"_s);
        PlaylistTagger tagger(&model);
        QSignalSpy     started(&tagger, &PlaylistTagger::started);
        QSignalSpy     tagged(&tagger, &PlaylistTagger::playlist_tagged);

        Gate gate;
        tagger.request_playlist(model.index(0));
        tagger.request_playlist(model.index(0));
        gate.open();
        drain();

        QCOMPARE(started.count(), 1);
        QCOMPARE(tagged.count(), 1);
    }

    // User clicks A, then B before A finishes: A is abandoned and B is tagged
    void switching_playlist_cancels_previous()
    {
        PlaylistModel model;
        add_playlist(model, u"a"_s, u"Tag A"_s);
        add_playlist(model, u"b"_s, u"Tag B"_s);
        PlaylistTagger tagger(&model);
        QSignalSpy     started(&tagger, &PlaylistTagger::started);
        QSignalSpy     tagged(&tagger, &PlaylistTagger::playlist_tagged);

        Gate gate;
        tagger.request_playlist(model.index(0));
        tagger.request_playlist(model.index(1));
        gate.open();
        drain();

        // Verify A started, but not tagged (only M3U placeholder data)
        QCOMPARE(started.count(), 2);
        QVERIFY(!model.at(0).tagged());
        QCOMPARE(first_title(model, 0), u"M3U a"_s);

        QCOMPARE(tagged.count(), 1);
        QVERIFY(model.at(1).tagged());
        QCOMPARE(first_title(model, 1), u"Tag B"_s);

        // Check playlist_tagged signal corresponds with B's index
        QCOMPARE(tagged.at(0).at(0).value<QPersistentModelIndex>().row(), 1);
    }

    // A -> B -> back to A before anything finishes: B's queued request is dropped
    void returning_to_in_flight_playlist_drops_queued_one()
    {
        PlaylistModel model;
        add_playlist(model, u"a"_s, u"Tag A"_s);
        add_playlist(model, u"b"_s, u"Tag B"_s);
        PlaylistTagger tagger(&model);
        QSignalSpy     tagged(&tagger, &PlaylistTagger::playlist_tagged);

        Gate gate;
        tagger.request_playlist(model.index(0));
        tagger.request_playlist(model.index(1)); // cancels A, queues B
        tagger.request_playlist(model.index(0)); // A's watcher is canceled, so A is re-queued
        gate.open();
        drain();

        QCOMPARE(tagged.count(), 1);
        QCOMPARE(tagged.at(0).at(0).value<QPersistentModelIndex>().row(), 0);
        QVERIFY(model.at(0).tagged());
        QVERIFY(!model.at(1).tagged());
    }

    // A -> B -> C (already tagged): the latest selection needs no work, so the queued
    // B is dropped and the already-canceled A is not applied either
    void selecting_tagged_playlist_clears_queue()
    {
        PlaylistModel model;
        add_playlist(model, u"a"_s, u"Tag A"_s);
        add_playlist(model, u"b"_s, u"Tag B"_s);
        add_playlist(model, u"c"_s, u"Tag C"_s);
        model.set_songs(2, model.at(2).get_songs()); // mark C tagged
        PlaylistTagger tagger(&model);
        QSignalSpy     tagged(&tagger, &PlaylistTagger::playlist_tagged);

        Gate gate;
        tagger.request_playlist(model.index(0));
        tagger.request_playlist(model.index(1)); // queue B
        tagger.request_playlist(model.index(2)); // C already tagged: drop B
        gate.open();
        drain();

        QCOMPARE(tagged.count(), 0);
        QVERIFY(!model.at(0).tagged());
        QVERIFY(!model.at(1).tagged());
    }

    // --- cancel() ---------------------------------------------------------------

    void cancel_discards_result()
    {
        PlaylistModel model;
        add_playlist(model, u"a"_s, u"Tag A"_s);
        PlaylistTagger tagger(&model);
        QSignalSpy     tagged(&tagger, &PlaylistTagger::playlist_tagged);

        Gate gate;
        tagger.request_playlist(model.index(0));
        tagger.cancel();
        gate.open();
        drain();

        QCOMPARE(tagged.count(), 0);
        QVERIFY(!model.at(0).tagged());
        QCOMPARE(first_title(model, 0), u"M3U a"_s);
    }

    void cancel_drops_queued_request()
    {
        PlaylistModel model;
        add_playlist(model, u"a"_s, u"Tag A"_s);
        add_playlist(model, u"b"_s, u"Tag B"_s);
        PlaylistTagger tagger(&model);
        QSignalSpy     started(&tagger, &PlaylistTagger::started);

        Gate gate;
        tagger.request_playlist(model.index(0));
        tagger.request_playlist(model.index(1));
        tagger.cancel();
        gate.open();
        drain();

        QCOMPARE(started.count(), 1);
        QVERIFY(!model.at(1).tagged());
    }

    void can_tag_again_after_cancel()
    {
        PlaylistModel model;
        add_playlist(model, u"a"_s, u"Tag A"_s);
        PlaylistTagger tagger(&model);
        QSignalSpy     tagged(&tagger, &PlaylistTagger::playlist_tagged);

        {
            Gate gate;
            tagger.request_playlist(model.index(0));
            tagger.cancel();
        }
        drain();

        tagger.request_playlist(model.index(0));
        QVERIFY(tagged.wait());
        QVERIFY(model.at(0).tagged());
    }

    // --- Interaction with model row changes -------------------------------------

    // Persistent indexes follow the row, so results land on the right playlist
    void result_follows_shifted_row()
    {
        PlaylistModel model;
        add_playlist(model, u"a"_s, u"Tag A"_s);
        add_playlist(model, u"b"_s, u"Tag B"_s);
        PlaylistTagger tagger(&model);
        QSignalSpy     tagged(&tagger, &PlaylistTagger::playlist_tagged);

        Gate gate;
        tagger.request_playlist(model.index(1));
        QVERIFY(model.removeRow(0)); // B moves to row 0
        gate.open();
        drain();

        QCOMPARE(tagged.count(), 1);
        QCOMPARE(tagged.at(0).at(0).value<QPersistentModelIndex>().row(), 0);
        QCOMPARE(model.at(0).get_name(), u"b"_s);
        QCOMPARE(first_title(model, 0), u"Tag B"_s);
    }

    // The removed row's persistent index goes invalid, so its result is dropped
    // instead of being written into whichever playlist now occupies that row.
    // (The job itself still runs to completion: see TODO in playlisttagger.cpp.)
    void removed_playlist_result_is_dropped()
    {
        PlaylistModel model;
        add_playlist(model, u"a"_s, u"Tag A"_s);
        add_playlist(model, u"b"_s, u"Tag B"_s);
        PlaylistTagger tagger(&model);
        QSignalSpy     tagged(&tagger, &PlaylistTagger::playlist_tagged);

        Gate gate;
        tagger.request_playlist(model.index(0));
        QVERIFY(model.removeRow(0));
        gate.open();
        drain();

        QCOMPARE(tagged.count(), 0);
        QCOMPARE(model.at(0).get_name(), u"b"_s);
        QVERIFY(!model.at(0).tagged());
    }

    void destroying_tagger_mid_job_is_safe()
    {
        PlaylistModel model;
        add_playlist(model, u"a"_s, u"Tag A"_s);
        {
            Gate           gate;
            PlaylistTagger tagger(&model);
            tagger.request_playlist(model.index(0));
            // ~PlaylistTagger cancels; ~QFutureWatcher must not deliver into a dead object
        }
        drain();
        QVERIFY(!model.at(0).tagged());
    }
};

QTEST_MAIN(TestPlaylistTagger)
#include "tst_playlisttagger.moc"
