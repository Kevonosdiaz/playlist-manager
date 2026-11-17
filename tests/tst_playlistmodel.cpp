// Tests for PlaylistModel (src/media/playlistmodel.cpp): list model contract, row
// removal and QSettings persistence.

#include "media/playlistmodel.h"
#include "utils/settings.h"

#include <QAbstractItemModelTester>
#include <QCoreApplication>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace Qt::StringLiterals;

class TestPlaylistModel : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> tmp;
    QTemporaryDir                  settings_dir;

    QString path(const QString& name) const
    {
        return tmp->filePath(name);
    }

    // Playlist files don't need to exist: a missing file parses as an empty playlist,
    // and the model only cares about name and path
    void fill(PlaylistModel& model, const QStringList& names)
    {
        for(const QString& n : names)
            model.append(Playlist{path(n + u".m3u"_s)});
    }

    static QStringList names(const PlaylistModel& model)
    {
        QStringList res;
        for(int i = 0; i < model.rowCount(); ++i)
            res << model.data(model.index(i)).toString();
        return res;
    }

private slots:
    void initTestCase()
    {
        // Keep save_playlists() away from the user's real settings file
        QVERIFY(settings_dir.isValid());
        QCoreApplication::setOrganizationName(u"playlist-manager-tests"_s);
        QCoreApplication::setApplicationName(u"tst_playlistmodel"_s);
        QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, settings_dir.path());
    }

    void init()
    {
        tmp = std::make_unique<QTemporaryDir>();
        QVERIFY(tmp->isValid());
        QSettings{}.clear();
    }

    void cleanup()
    {
        tmp.reset();
    }

    // --- Model contract ---------------------------------------------------------

    // QAbstractItemModelTester re-checks the model's invariants after every change
    // signal, so it catches mismatched begin/end calls and bad rowCount/index logic
    void passes_model_tester()
    {
        PlaylistModel            model;
        QAbstractItemModelTester tester(&model,
                                        QAbstractItemModelTester::FailureReportingMode::QtTest);
        fill(model, {u"a"_s, u"b"_s, u"c"_s});
        model.removeRows(1, 1);
        model.removeRows(0, 2);
    }

    void starts_empty()
    {
        PlaylistModel model;
        QCOMPARE(model.rowCount(), 0);
    }

    void flat_list_has_no_children()
    {
        PlaylistModel model;
        fill(model, {u"a"_s});
        QCOMPARE(model.rowCount(model.index(0)), 0);
    }

    void data_roles()
    {
        PlaylistModel model;
        fill(model, {u"Road Trip"_s});
        const QModelIndex idx = model.index(0);
        const QString     p   = path(u"Road Trip.m3u"_s);

        QCOMPARE(model.data(idx, Qt::DisplayRole).toString(), u"Road Trip"_s);
        QCOMPARE(model.data(idx, PlaylistModel::PathRole).toString(), p);
        QCOMPARE(model.data(idx, Qt::ToolTipRole).toString(), p);
        QVERIFY(!model.data(idx, Qt::DecorationRole).isValid());
    }

    void data_out_of_range_is_null()
    {
        PlaylistModel model;
        fill(model, {u"a"_s});
        QVERIFY(!model.data(QModelIndex{}).isValid());
        QVERIFY(!model.data(model.index(5)).isValid());
    }

    void append_emits_rows_inserted()
    {
        PlaylistModel model;
        fill(model, {u"a"_s});
        QSignalSpy spy(&model, &QAbstractItemModel::rowsInserted);
        fill(model, {u"b"_s});

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).toInt(), 1);
        QCOMPARE(spy.at(0).at(2).toInt(), 1);
    }

    void find_by_path()
    {
        PlaylistModel model;
        fill(model, {u"a"_s, u"b"_s});
        QCOMPARE(model.find(path(u"b.m3u"_s)), 1);
        QCOMPARE(model.find(path(u"missing.m3u"_s)), -1);
    }

    // --- removeRows -------------------------------------------------------------

    void remove_rows_data()
    {
        QTest::addColumn<int>("row");
        QTest::addColumn<int>("count");
        QTest::addColumn<QStringList>("expected");

        QTest::newRow("first") << 0 << 1 << QStringList{u"b"_s, u"c"_s, u"d"_s};
        QTest::newRow("middle") << 1 << 1 << QStringList{u"a"_s, u"c"_s, u"d"_s};
        // row + count == size(): the half-open end of the range may equal size()
        QTest::newRow("last") << 3 << 1 << QStringList{u"a"_s, u"b"_s, u"c"_s};
        QTest::newRow("several") << 1 << 2 << QStringList{u"a"_s, u"d"_s};
        QTest::newRow("all") << 0 << 4 << QStringList{};
    }

    void remove_rows()
    {
        QFETCH(int, row);
        QFETCH(int, count);
        QFETCH(QStringList, expected);

        PlaylistModel model;
        fill(model, {u"a"_s, u"b"_s, u"c"_s, u"d"_s});
        QSignalSpy about(&model, &QAbstractItemModel::rowsAboutToBeRemoved);
        QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);

        QVERIFY(model.removeRows(row, count));
        QCOMPARE(names(model), expected);

        // Views rely on the signal range matching what was actually removed
        QCOMPARE(about.count(), 1);
        QCOMPARE(removed.count(), 1);
        QCOMPARE(removed.at(0).at(1).toInt(), row);
        QCOMPARE(removed.at(0).at(2).toInt(), row + count - 1);
    }

    // removeRow() is QAbstractItemModel's non-virtual wrapper; MainWindow calls this path
    void remove_row_convenience_wrapper()
    {
        PlaylistModel model;
        fill(model, {u"a"_s, u"b"_s});
        QVERIFY(model.removeRow(0));
        QCOMPARE(names(model), QStringList{u"b"_s});
    }

    void remove_rows_rejects_invalid_args_data()
    {
        QTest::addColumn<int>("row");
        QTest::addColumn<int>("count");
        QTest::addColumn<bool>("valid_parent");

        // Model under test holds 3 rows: a, b, c
        QTest::newRow("valid parent") << 0 << 1 << true;
        QTest::newRow("negative row") << -1 << 1 << false;
        QTest::newRow("negative count") << 1 << -1 << false;
        QTest::newRow("zero count") << 1 << 0 << false;
        QTest::newRow("row equals size") << 3 << 1 << false;
        QTest::newRow("row + count exceeds") << 1 << 3 << false;
    }

    void remove_rows_rejects_invalid_args()
    {
        QFETCH(int, row);
        QFETCH(int, count);
        QFETCH(bool, valid_parent);

        PlaylistModel model;
        fill(model, {u"a"_s, u"b"_s, u"c"_s});
        const QModelIndex parent = valid_parent ? model.index(0) : QModelIndex{};
        QSignalSpy        about(&model, &QAbstractItemModel::rowsAboutToBeRemoved);

        QVERIFY(!model.removeRows(row, count, parent));
        // A rejected call must not touch the model or announce a change
        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(about.count(), 0);
    }

    // MainWindow keeps tagging_row/next_row as persistent indexes and relies on
    // them being invalidated (removed row) or shifted (rows after it)
    void remove_updates_persistent_indexes()
    {
        PlaylistModel model;
        fill(model, {u"a"_s, u"b"_s, u"c"_s});
        const QPersistentModelIndex removed_idx{model.index(1)};
        const QPersistentModelIndex shifted_idx{model.index(2)};

        QVERIFY(model.removeRows(1, 1));
        QVERIFY(!removed_idx.isValid());
        QCOMPARE(shifted_idx.row(), 1);
        QCOMPARE(shifted_idx.data().toString(), u"c"_s);
    }

    // --- Persistence ------------------------------------------------------------

    static QStringList saved_paths()
    {
        QSettings   s;
        QStringList res;
        const int   size = s.beginReadArray(Settings::p_playlists);
        for(int i = 0; i < size; ++i)
        {
            s.setArrayIndex(i);
            res << s.value(Settings::p_playlist_path).toString();
        }
        s.endArray();
        return res;
    }

    void save_playlists_writes_paths_in_order()
    {
        PlaylistModel model;
        fill(model, {u"a"_s, u"b"_s});
        model.save_playlists();
        QCOMPARE(saved_paths(), (QStringList{path(u"a.m3u"_s), path(u"b.m3u"_s)}));
    }

    // Overwriting a longer saved array must not resurrect the removed entry
    void save_after_remove_drops_entry()
    {
        PlaylistModel model;
        fill(model, {u"a"_s, u"b"_s, u"c"_s});
        model.save_playlists();

        QVERIFY(model.removeRows(1, 1));
        model.save_playlists();
        QCOMPARE(saved_paths(), (QStringList{path(u"a.m3u"_s), path(u"c.m3u"_s)}));
    }
};

QTEST_MAIN(TestPlaylistModel)
#include "tst_playlistmodel.moc"
