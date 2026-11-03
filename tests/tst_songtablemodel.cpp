// Tests for SongTableModel (src/media/songtablemodel.cpp).
//
// Every test that mutates the model attaches a QAbstractItemModelTester, which
// checks the Qt model/view contract (index validity, row-insert signals, resets...)
// after each change and fails the test on any violation.

#include "media/songtablemodel.h"

#include <QAbstractItemModelTester>
#include <QImage>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;
using Col = SongTableModel::Column;

namespace
{
    Song make_song(const QString& title,
                   const QString& artist   = {},
                   const QString& album    = {},
                   uint32_t       duration = 0,
                   uint32_t       track    = 0)
    {
        Song s;
        s.title        = title;
        s.artist       = artist;
        s.album        = album;
        s.duration_sec = duration;
        s.track        = track;
        s.path         = u"/music/"_s + title + u".mp3"_s;
        return s;
    }

    // Titles in current row order, to make sort assertions readable
    QStringList titles(const SongTableModel& m)
    {
        QStringList res;
        for(int row = 0; row < m.rowCount(); ++row)
            res << m.index(row, Col::Title).data().toString();
        return res;
    }

    // Three songs whose order differs for every sortable column
    QVector<Song> sample_songs()
    {
        return {
            make_song(u"B"_s, u"Zed"_s, u"Middle"_s, 300, 1),
            make_song(u"C"_s, u"Alpha"_s, u"Last"_s, 100, 3),
            make_song(u"A"_s, u"Mid"_s, u"First"_s, 200, 2),
        };
    }
}

class TestSongTableModel : public QObject
{
    Q_OBJECT

private slots:
    // --- Shape and headers ------------------------------------------------------

    void empty_model()
    {
        SongTableModel           m;
        QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QCOMPARE(m.rowCount(), 0);
        QCOMPARE(m.columnCount(), int(Col::Columns));
    }

    void table_has_no_children()
    {
        SongTableModel m;
        m.set_songs(sample_songs());
        QCOMPARE(m.rowCount(m.index(0, 0)), 0);
    }

    void horizontal_headers()
    {
        SongTableModel m;
        const QStringList expected{
            u"Cover"_s, u"Title"_s, u"Artist"_s, u"Album"_s, u"Length"_s, u"Track"_s};
        for(int c = 0; c < m.columnCount(); ++c)
            QCOMPARE(m.headerData(c, Qt::Horizontal).toString(), expected.at(c));

        QVERIFY(!m.headerData(Col::Columns, Qt::Horizontal).isValid());
        QVERIFY(!m.headerData(-1, Qt::Horizontal).isValid());
        QVERIFY(!m.headerData(0, Qt::Vertical).isValid());
        QVERIFY(!m.headerData(0, Qt::Horizontal, Qt::DecorationRole).isValid());
    }

    // --- data() -------------------------------------------------------------------

    void display_data_per_column()
    {
        SongTableModel m;
        m.set_songs({make_song(u"Title"_s, u"Artist"_s, u"Album"_s, 125, 4)});

        QVERIFY(!m.index(0, Col::AlbumArt).data().isValid());
        QCOMPARE(m.index(0, Col::Title).data().toString(), u"Title"_s);
        QCOMPARE(m.index(0, Col::Artist).data().toString(), u"Artist"_s);
        QCOMPARE(m.index(0, Col::Album).data().toString(), u"Album"_s);
        QCOMPARE(m.index(0, Col::Length).data().toString(), u"02:05"_s);
        QCOMPARE(m.index(0, Col::Track).data().toUInt(), 4u);
    }

    void zero_track_shows_blank()
    {
        SongTableModel m;
        m.set_songs({make_song(u"No Track"_s)});
        QVERIFY(!m.index(0, Col::Track).data().isValid());
    }

    void duration_format_data()
    {
        QTest::addColumn<uint>("seconds");
        QTest::addColumn<QString>("expected");
        QTest::newRow("zero") << 0u << u"00:00"_s;
        QTest::newRow("seconds only") << 5u << u"00:05"_s;
        QTest::newRow("minute boundary") << 60u << u"01:00"_s;
        QTest::newRow("typical song") << 245u << u"04:05"_s;
        QTest::newRow("just under an hour") << 3599u << u"59:59"_s;
        QTest::newRow("one hour") << 3600u << u"01:00:00"_s;
        QTest::newRow("over an hour") << 3661u << u"01:01:01"_s;
    }

    void duration_format()
    {
        QFETCH(uint, seconds);
        QFETCH(QString, expected);
        SongTableModel m;
        m.set_songs({make_song(u"x"_s, {}, {}, seconds)});
        QCOMPARE(m.index(0, Col::Length).data().toString(), expected);
    }

    void decoration_only_for_cover_column()
    {
        Song   with_cover = make_song(u"Covered"_s);
        QImage img(8, 8, QImage::Format_RGB32);
        img.fill(Qt::blue);
        with_cover.cover = img;

        SongTableModel m;
        m.set_songs({with_cover, make_song(u"Bare"_s)});

        // The default delegate draws a QImage directly, so no QIcon wrapping is needed
        const QVariant deco = m.index(0, Col::AlbumArt).data(Qt::DecorationRole);
        QCOMPARE(deco.metaType(), QMetaType::fromType<QImage>());
        QCOMPARE(deco.value<QImage>(), img);

        QVERIFY(!m.index(0, Col::Title).data(Qt::DecorationRole).isValid());
        QVERIFY(!m.index(1, Col::AlbumArt).data(Qt::DecorationRole).isValid());
    }

    void unhandled_role_is_invalid()
    {
        SongTableModel m;
        m.set_songs({make_song(u"x"_s)});
        QVERIFY(!m.index(0, Col::Title).data(Qt::ToolTipRole).isValid());
    }

    // --- set_songs / add_songs ---------------------------------------------------

    void set_songs_resets_model()
    {
        SongTableModel           m;
        QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
        m.set_songs(sample_songs());

        QSignalSpy reset_spy(&m, &QAbstractItemModel::modelReset);
        m.set_songs({make_song(u"Only"_s)});
        QCOMPARE(reset_spy.count(), 1);
        QCOMPARE(titles(m), QStringList{u"Only"_s});

        m.set_songs({});
        QCOMPARE(m.rowCount(), 0);
    }

    void add_songs_appends_and_signals_range()
    {
        SongTableModel           m;
        QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
        m.set_songs({make_song(u"First"_s)});

        QSignalSpy insert_spy(&m, &QAbstractItemModel::rowsInserted);
        m.add_songs({make_song(u"Second"_s), make_song(u"Third"_s)});

        QCOMPARE(titles(m), (QStringList{u"First"_s, u"Second"_s, u"Third"_s}));
        QCOMPARE(insert_spy.count(), 1);
        const auto args = insert_spy.takeFirst();
        QVERIFY(!args.at(0).value<QModelIndex>().isValid()); // parent
        QCOMPARE(args.at(1).toInt(), 1);                     // first
        QCOMPARE(args.at(2).toInt(), 2);                     // last
    }

    void add_songs_to_empty_model()
    {
        SongTableModel           m;
        QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
        m.add_songs(sample_songs());
        QCOMPARE(m.rowCount(), 3);
    }

    void add_empty_list_is_noop()
    {
        SongTableModel m;
        m.set_songs(sample_songs());
        QSignalSpy insert_spy(&m, &QAbstractItemModel::rowsAboutToBeInserted);
        m.add_songs({});
        QCOMPARE(insert_spy.count(), 0);
        QCOMPARE(m.rowCount(), 3);
    }

    // --- sort() -------------------------------------------------------------------

    void sort_by_column_data()
    {
        QTest::addColumn<int>("column");
        QTest::addColumn<Qt::SortOrder>("order");
        QTest::addColumn<QStringList>("expected_titles");

        const auto asc  = Qt::AscendingOrder;
        const auto desc = Qt::DescendingOrder;
        // sample_songs(): B(Zed, Middle, 300, #1)  C(Alpha, Last, 100, #3)  A(Mid, First, 200, #2)
        QTest::newRow("title asc") << int(Col::Title) << asc << QStringList{"A", "B", "C"};
        QTest::newRow("title desc") << int(Col::Title) << desc << QStringList{"C", "B", "A"};
        QTest::newRow("artist asc") << int(Col::Artist) << asc << QStringList{"C", "A", "B"};
        QTest::newRow("artist desc") << int(Col::Artist) << desc << QStringList{"B", "A", "C"};
        QTest::newRow("album asc") << int(Col::Album) << asc << QStringList{"A", "C", "B"};
        QTest::newRow("album desc") << int(Col::Album) << desc << QStringList{"B", "C", "A"};
        QTest::newRow("length asc") << int(Col::Length) << asc << QStringList{"C", "A", "B"};
        QTest::newRow("length desc") << int(Col::Length) << desc << QStringList{"B", "A", "C"};
        QTest::newRow("track asc") << int(Col::Track) << asc << QStringList{"B", "A", "C"};
        QTest::newRow("track desc") << int(Col::Track) << desc << QStringList{"C", "A", "B"};
    }

    void sort_by_column()
    {
        QFETCH(int, column);
        QFETCH(Qt::SortOrder, order);
        QFETCH(QStringList, expected_titles);

        // Only checks the resulting order. No QAbstractItemModelTester here yet: it
        // (correctly) fails every row on review #3, and QEXPECT_FAIL can't target a
        // check inside the tester. Once sort() emits layoutAboutToBeChanged, add:
        //   QAbstractItemModelTester tester(&m, QAbstractItemModelTester::FailureReportingMode::QtTest);
        SongTableModel m;
        m.set_songs(sample_songs());
        m.sort(column, order);
        QCOMPARE(titles(m), expected_titles);
    }

    void sort_cover_column_keeps_all_rows()
    {
        // Cover art has no ordering; sorting by it must not lose or duplicate rows
        SongTableModel m;
        m.set_songs(sample_songs());
        m.sort(Col::AlbumArt, Qt::AscendingOrder);
        auto result = titles(m);
        result.sort();
        QCOMPARE(result, (QStringList{u"A"_s, u"B"_s, u"C"_s}));
    }

    void sort_emits_layout_about_to_be_changed()
    {
        SongTableModel m;
        m.set_songs(sample_songs());
        QSignalSpy about_spy(&m, &QAbstractItemModel::layoutAboutToBeChanged);
        QSignalSpy changed_spy(&m, &QAbstractItemModel::layoutChanged);

        m.sort(Col::Title, Qt::AscendingOrder);

        QCOMPARE(changed_spy.count(), 1);
        QEXPECT_FAIL("", "Review #3: sort() emits layoutChanged without layoutAboutToBeChanged", Abort);
        QCOMPARE(about_spy.count(), 1);
    }

    void sort_keeps_persistent_indexes_on_same_song()
    {
        // Views store the selection and current item as persistent indexes; after
        // a sort they must still point at the same song, not the same row number
        SongTableModel m;
        m.set_songs(sample_songs()); // rows: B, C, A
        const QPersistentModelIndex song_a(m.index(2, Col::Title));
        QCOMPARE(song_a.data().toString(), u"A"_s);

        m.sort(Col::Title, Qt::AscendingOrder); // rows: A, B, C

        QEXPECT_FAIL("", "Review #3: sort() never updates persistent indexes", Abort);
        QCOMPARE(song_a.data().toString(), u"A"_s);
    }
};

QTEST_MAIN(TestSongTableModel)
#include "tst_songtablemodel.moc"
