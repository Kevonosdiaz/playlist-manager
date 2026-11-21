// Tests for DirTagger (src/media/dirtagger.cpp): accepting/rejecting directory
// requests, replace vs. append mode, recursive scans, queueing and cancellation.
//
// DirTagger tags files on QtConcurrent's global pool. Tests that need a job held in
// flight close a Gate first: the pool is shrunk to one thread and that thread is
// parked, so the tagging job stays queued until the gate opens.

#include "media/dirtagger.h"
#include "media/songtablemodel.h"
#include "testutils.h"

#include <QDir>
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

    // Owns a model + tagger pair wired the way MainWindow wires them, plus spies
    // on every signal so tests can assert on exactly what was emitted
    struct Fixture
    {
        SongTableModel model;
        DirTagger      tagger{&model};
        QSignalSpy     started{&tagger, &DirTagger::started};
        QSignalSpy     empty_dir{&tagger, &DirTagger::empty_dir};
        QSignalSpy     repeat_dir{&tagger, &DirTagger::repeat_dir};
        QSignalSpy     cleared{&tagger, &DirTagger::clear_songviewer};
        QSignalSpy     tagged{&tagger, &DirTagger::dir_tagged};

        Fixture()
        {
            // MainWindow::clear_songviewer empties the model in response
            QObject::connect(&tagger, &DirTagger::clear_songviewer, &model, [this] {
                model.set_songs({});
            });
        }

        int last_tagged_count() const
        {
            return tagged.constLast().at(0).toInt();
        }
    };
}

class TestDirTagger : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> tmp;

    QString path(const QString& rel) const
    {
        return tmp->filePath(rel);
    }

    // Create `rel` (and parents) under the temp dir, returning its absolute path
    QString make_dir(const QString& rel) const
    {
        QDir(tmp->path()).mkpath(rel);
        return path(rel);
    }

    // Write `count` readable WAV files named <prefix>0.wav, <prefix>1.wav, ... into dir
    static void add_songs(const QString& dir, int count, const QString& prefix = u"song"_s)
    {
        for(int i = 0; i < count; ++i)
        {
            const QString file = dir + u"/"_s + prefix + QString::number(i) + u".wav"_s;
            QVERIFY(testutils::write_silent_wav(file, 1));
        }
    }

    // Let every queued job finish and deliver the watcher's callouts.
    // on_finished() may start a queued dir, so loop until the pool stays idle.
    static void drain()
    {
        for(int i = 0; i < 5; ++i)
        {
            QThreadPool::globalInstance()->waitForDone();
            QCoreApplication::processEvents();
            QCoreApplication::sendPostedEvents();
        }
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

    // --- Rejected requests ------------------------------------------------------

    void empty_dir_is_rejected()
    {
        Fixture f;
        QVERIFY(!f.tagger.request_dir(make_dir(u"empty"_s), false, false));
        QCOMPARE(f.empty_dir.count(), 1);
        QCOMPARE(f.started.count(), 0);
        QCOMPARE(f.cleared.count(), 0);
    }

    void dir_without_audio_files_is_rejected()
    {
        Fixture       f;
        const QString dir = make_dir(u"docs"_s);
        QVERIFY(testutils::write_file(dir + u"/notes.txt"_s, "hello"));

        QVERIFY(!f.tagger.request_dir(dir, false, false));
        QCOMPARE(f.empty_dir.count(), 1);
        QCOMPARE(f.started.count(), 0);
    }

    void reopening_open_dir_is_rejected()
    {
        Fixture       f;
        const QString dir = make_dir(u"a"_s);
        add_songs(dir, 2);

        QVERIFY(f.tagger.request_dir(dir, false, false));
        QVERIFY(f.tagged.wait());

        QVERIFY(!f.tagger.request_dir(dir, false, false));
        QCOMPARE(f.repeat_dir.count(), 1);
        QCOMPARE(f.started.count(), 1);
        QCOMPARE(f.cleared.count(), 1); // only from the first request
        QCOMPARE(f.model.rowCount(), 2);
    }

    // Review issue #2: a directory *named* like a song (QDirListing's name filters
    // match it) passes the empty check, but scan_dir finds no files inside. By then
    // request_dir has already run its clear block in replace mode.
    void rejected_request_leaves_viewer_untouched()
    {
        Fixture       f;
        const QString a = make_dir(u"a"_s);
        add_songs(a, 2);
        const QString trap = make_dir(u"trap"_s);
        make_dir(u"trap/fake.mp3"_s); // a directory, not a file

        QVERIFY(f.tagger.request_dir(a, false, false));
        QVERIFY(f.tagged.wait());

        const bool accepted = f.tagger.request_dir(trap, false, false);
        drain();

        // TODO(human): assert the contract for a rejected request.
        Q_UNUSED(accepted);
    }

    // --- Basic tagging ----------------------------------------------------------

    void dir_is_tagged_into_model()
    {
        Fixture       f;
        const QString dir = make_dir(u"a"_s);
        add_songs(dir, 3);

        QVERIFY(f.tagger.request_dir(dir, false, false));
        QCOMPARE(f.started.count(), 1); // emitted synchronously from start()
        QCOMPARE(f.cleared.count(), 1); // first open always replaces the viewer
        QVERIFY(f.tagged.wait());

        QCOMPARE(f.tagged.count(), 1);
        QCOMPARE(f.last_tagged_count(), 3);
        QCOMPARE(f.model.rowCount(), 3);
    }

    // path_to_song returns nullopt for files TagLib can't open; they are dropped.
    // (Junk *contents* aren't enough: TagLib opens those as 0:00 songs, see tst_tag.)
    void unreadable_files_are_skipped()
    {
        Fixture       f;
        const QString dir = make_dir(u"a"_s);
        add_songs(dir, 1);
        add_songs(dir, 1, u"locked"_s);
        QFile locked(dir + u"/locked0.wav"_s);
        QVERIFY(locked.setPermissions(QFile::WriteOwner));
        if(locked.open(QIODevice::ReadOnly))
            QSKIP("Running with privileges that ignore file permissions");

        QVERIFY(f.tagger.request_dir(dir, false, false));
        QVERIFY(f.tagged.wait());
        QCOMPARE(f.last_tagged_count(), 1);
        QCOMPARE(f.model.rowCount(), 1);
    }

    void non_recursive_ignores_subdirs()
    {
        Fixture       f;
        const QString dir = make_dir(u"a"_s);
        add_songs(dir, 1);
        add_songs(make_dir(u"a/sub"_s), 2);

        QVERIFY(f.tagger.request_dir(dir, false, false));
        QVERIFY(f.tagged.wait());
        QCOMPARE(f.model.rowCount(), 1);
    }

    void recursive_includes_subdirs()
    {
        Fixture       f;
        const QString dir = make_dir(u"a"_s);
        add_songs(dir, 1);
        add_songs(make_dir(u"a/sub/deeper"_s), 2);

        QVERIFY(f.tagger.request_dir(dir, true, false));
        QVERIFY(f.tagged.wait());
        QCOMPARE(f.last_tagged_count(), 3);
        QCOMPARE(f.model.rowCount(), 3);
    }

    // --- Replace mode (append off) ----------------------------------------------

    void opening_new_dir_replaces_songs()
    {
        Fixture       f;
        const QString a = make_dir(u"a"_s);
        const QString b = make_dir(u"b"_s);
        add_songs(a, 2);
        add_songs(b, 3);

        QVERIFY(f.tagger.request_dir(a, false, false));
        QVERIFY(f.tagged.wait());
        QVERIFY(f.tagger.request_dir(b, false, false));
        QVERIFY(f.tagged.wait());

        QCOMPARE(f.cleared.count(), 2);
        QCOMPARE(f.last_tagged_count(), 3);
        QCOMPARE(f.model.rowCount(), 3);
    }

    // User clicks A, then B before A finishes: A's job is canceled, only B lands
    void switching_dir_mid_scan_cancels_previous()
    {
        Fixture       f;
        const QString a = make_dir(u"a"_s);
        const QString b = make_dir(u"b"_s);
        add_songs(a, 2);
        add_songs(b, 3);

        {
            Gate gate;
            QVERIFY(f.tagger.request_dir(a, false, false));
            QVERIFY(f.tagger.request_dir(b, false, false));
        }
        drain();

        QCOMPARE(f.started.count(), 2);
        QCOMPARE(f.tagged.count(), 1);
        QCOMPARE(f.last_tagged_count(), 3);
        QCOMPARE(f.model.rowCount(), 3);
    }

    // Previously-open A can be reopened once B has replaced it
    void replaced_dir_can_be_reopened()
    {
        Fixture       f;
        const QString a = make_dir(u"a"_s);
        const QString b = make_dir(u"b"_s);
        add_songs(a, 2);
        add_songs(b, 1);

        QVERIFY(f.tagger.request_dir(a, false, false));
        QVERIFY(f.tagged.wait());
        QVERIFY(f.tagger.request_dir(b, false, false));
        QVERIFY(f.tagged.wait());
        QVERIFY(f.tagger.request_dir(a, false, false));
        QVERIFY(f.tagged.wait());

        QCOMPARE(f.repeat_dir.count(), 0);
        QCOMPARE(f.model.rowCount(), 2);
    }

    // --- Append mode ------------------------------------------------------------

    void append_mode_adds_to_existing_songs()
    {
        Fixture       f;
        const QString a = make_dir(u"a"_s);
        const QString b = make_dir(u"b"_s);
        add_songs(a, 2);
        add_songs(b, 3);

        QVERIFY(f.tagger.request_dir(a, false, true));
        QVERIFY(f.tagged.wait());
        QVERIFY(f.tagger.request_dir(b, false, true));
        QVERIFY(f.tagged.wait());

        QCOMPARE(f.cleared.count(), 1); // only the first open clears
        QCOMPARE(f.last_tagged_count(), 3); // count is per request, not cumulative
        QCOMPARE(f.model.rowCount(), 5);
    }

    // B requested while A is still tagging: B is queued behind A, and the single
    // dir_tagged at the end reports both dirs' songs together
    void append_mode_queues_while_tagging()
    {
        Fixture       f;
        const QString a = make_dir(u"a"_s);
        const QString b = make_dir(u"b"_s);
        add_songs(a, 2);
        add_songs(b, 3);

        {
            Gate gate;
            QVERIFY(f.tagger.request_dir(a, false, true));
            QVERIFY(f.tagger.request_dir(b, false, true));
            QCOMPARE(f.started.count(), 1); // B is waiting in the queue
        }
        drain();

        QCOMPARE(f.started.count(), 2);
        QCOMPARE(f.tagged.count(), 1);
        QCOMPARE(f.last_tagged_count(), 5);
        QCOMPARE(f.model.rowCount(), 5);
    }

    // Opening a subdir of an already-open recursive dir would only add duplicates
    void append_mode_rejects_subdir_of_open_dir()
    {
        Fixture       f;
        const QString root = make_dir(u"root"_s);
        add_songs(root, 1);
        add_songs(make_dir(u"root/sub"_s), 2);

        QVERIFY(f.tagger.request_dir(root, true, true));
        QVERIFY(f.tagged.wait());

        QVERIFY(!f.tagger.request_dir(path(u"root/sub"_s), true, true));
        QCOMPARE(f.repeat_dir.count(), 1);
        QCOMPARE(f.model.rowCount(), 3);
    }

    // Opening the parent of an open subdir only adds the songs not already shown
    void append_mode_parent_skips_open_subdir()
    {
        Fixture       f;
        const QString root = make_dir(u"root"_s);
        const QString sub  = make_dir(u"root/sub"_s);
        add_songs(root, 1);
        add_songs(sub, 2);

        QVERIFY(f.tagger.request_dir(sub, true, true));
        QVERIFY(f.tagged.wait());
        QVERIFY(f.tagger.request_dir(root, true, true));
        QVERIFY(f.tagged.wait());

        QCOMPARE(f.last_tagged_count(), 1);
        QCOMPARE(f.model.rowCount(), 3);
    }

    // --- cancel() / clear_open_dirs() -------------------------------------------

    void cancel_discards_result()
    {
        Fixture       f;
        const QString dir = make_dir(u"a"_s);
        add_songs(dir, 2);

        {
            Gate gate;
            QVERIFY(f.tagger.request_dir(dir, false, false));
            f.tagger.cancel();
        }
        drain();

        QCOMPARE(f.tagged.count(), 0);
        QCOMPARE(f.model.rowCount(), 0);
    }

    void cancel_drops_queued_dirs()
    {
        Fixture       f;
        const QString a = make_dir(u"a"_s);
        const QString b = make_dir(u"b"_s);
        add_songs(a, 2);
        add_songs(b, 3);

        {
            Gate gate;
            QVERIFY(f.tagger.request_dir(a, false, true));
            QVERIFY(f.tagger.request_dir(b, false, true));
            f.tagger.cancel();
        }
        drain();

        QCOMPARE(f.started.count(), 1);
        QCOMPARE(f.tagged.count(), 0);
        QCOMPARE(f.model.rowCount(), 0);
    }

    // Regression: opening a playlist cancels DirTagger, after which the same dir must
    // be openable again (and in append mode, must replace the playlist's songs)
    void dir_can_be_reopened_after_cancel()
    {
        Fixture       f;
        const QString dir = make_dir(u"a"_s);
        add_songs(dir, 2);

        QVERIFY(f.tagger.request_dir(dir, false, true));
        QVERIFY(f.tagged.wait());

        f.tagger.cancel();
        f.model.set_songs(QVector<Song>(4)); // stand-in for a loaded playlist

        QVERIFY(f.tagger.request_dir(dir, false, true));
        QVERIFY(f.tagged.wait());
        QCOMPARE(f.repeat_dir.count(), 0);
        QCOMPARE(f.cleared.count(), 2);
        QCOMPARE(f.last_tagged_count(), 2);
        QCOMPARE(f.model.rowCount(), 2);
    }

    // Toggling recursive/append mode calls clear_open_dirs so a dir can be rescanned
    void clear_open_dirs_allows_reopen()
    {
        Fixture       f;
        const QString dir = make_dir(u"a"_s);
        add_songs(dir, 2);

        QVERIFY(f.tagger.request_dir(dir, false, false));
        QVERIFY(f.tagged.wait());

        f.tagger.clear_open_dirs();
        QVERIFY(f.tagger.request_dir(dir, false, false));
        QVERIFY(f.tagged.wait());
        QCOMPARE(f.repeat_dir.count(), 0);
        QCOMPARE(f.model.rowCount(), 2);
    }

    void destroying_tagger_mid_job_is_safe()
    {
        SongTableModel model;
        const QString  dir = make_dir(u"a"_s);
        add_songs(dir, 2);
        {
            Gate      gate;
            DirTagger tagger(&model);
            QVERIFY(tagger.request_dir(dir, false, false));
            // ~DirTagger cancels; ~QFutureWatcher must not deliver into a dead object
        }
        drain();
        QCOMPARE(model.rowCount(), 0);
    }
};

QTEST_MAIN(TestDirTagger)
#include "tst_dirtagger.moc"
