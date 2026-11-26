// Tests for PlaylistDelegate (src/utils/playlistdelegate.cpp): edit-icon style options,
// row sizing, and mouse handling on the icon vs. the rest of the row.

#include "media/playlistmodel.h"
#include "utils/playlistdelegate.h"

#include <QListView>
#include <QMouseEvent>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace Qt::StringLiterals;

namespace
{
    constexpr int icon_size = 24; // Mirrors the constant in playlistdelegate.cpp

    // Exposes the protected style-option hook so tests can inspect what gets painted
    class TestableDelegate : public PlaylistDelegate
    {
    public:
        using PlaylistDelegate::initStyleOption;
        using PlaylistDelegate::PlaylistDelegate;
    };

    // Exposes the option the view itself hands to its delegate (widget, font, alignment)
    class TestView : public QListView
    {
    public:
        using QListView::initViewItemOption;
    };
}

class TestPlaylistDelegate : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir                  tmp;
    std::unique_ptr<PlaylistModel> model;
    std::unique_ptr<TestView>      view;
    TestableDelegate*              delegate = nullptr; // Owned by view

    QModelIndex row(int r) const
    {
        return model->index(r);
    }

    bool is_edit_target(int r) const
    {
        return row(r).data(PlaylistModel::IsEditTargetRole).toBool();
    }

    // The option QAbstractItemView passes to editorEvent() for idx
    QStyleOptionViewItem view_option(const QModelIndex& idx) const
    {
        QStyleOptionViewItem opt;
        view->initViewItemOption(&opt);
        opt.rect = view->visualRect(idx);
        return opt;
    }

    QStyleOptionViewItem styled_option(int r) const
    {
        QStyleOptionViewItem opt = view_option(row(r));
        delegate->initStyleOption(&opt, row(r));
        return opt;
    }

    // Where the edit icon is drawn for row r, in viewport coordinates
    QRect icon_rect(int r) const
    {
        const QStyleOptionViewItem opt = styled_option(r);
        return view->style()->subElementRect(QStyle::SE_ItemViewItemDecoration, &opt, view.get());
    }

    QPoint icon_pos(int r) const
    {
        return icon_rect(r).center();
    }

    // Left end of the row, where the playlist name is drawn
    QPoint name_pos(int r) const
    {
        const QRect rect = view->visualRect(row(r));
        return {rect.left() + 4, rect.center().y()};
    }

    QImage rendered_icon(int r) const
    {
        return styled_option(r).icon.pixmap(icon_size).toImage();
    }

    // Total opacity of an image: the filled pencil is the outline with its holes
    // filled in, so it always carries more ink
    static qint64 ink(const QImage& img)
    {
        qint64 sum = 0;
        for(int y = 0; y < img.height(); ++y)
            for(int x = 0; x < img.width(); ++x)
                sum += qAlpha(img.pixel(x, y));
        return sum;
    }

    // Deliver a mouse event straight to the delegate, as QAbstractItemView would
    bool send(QEvent::Type type, Qt::MouseButton button, QPoint pos, int r)
    {
        const Qt::MouseButtons held = type == QEvent::MouseButtonRelease ? Qt::NoButton : button;
        QMouseEvent ev(type, pos, view->viewport()->mapToGlobal(pos), button, held, Qt::NoModifier);
        return delegate->editorEvent(&ev, model.get(), view_option(row(r)), row(r));
    }

private slots:
    void initTestCase()
    {
        QVERIFY(tmp.isValid());
    }

    void init()
    {
        // Playlist files don't need to exist: missing files parse as empty playlists
        model = std::make_unique<PlaylistModel>();
        for(const QString& name : {u"Road Trip"_s, u"Focus"_s, u"Workout"_s})
            model->append(Playlist{tmp.filePath(name + u".m3u"_s)});

        view     = std::make_unique<TestView>();
        delegate = new TestableDelegate(view.get());
        view->setModel(model.get());
        view->setItemDelegate(delegate);
        view->resize(300, 200);
        view->show();
        QVERIFY(QTest::qWaitForWindowExposed(view.get()));
    }

    void cleanup()
    {
        view.reset();
        model.reset();
    }

    // --- Style option -------------------------------------------------------------

    void icon_decoration_on_right()
    {
        const QStyleOptionViewItem opt = styled_option(0);

        QCOMPARE(opt.text, u"Road Trip"_s); // Base class still fills in the name
        QVERIFY(opt.features.testFlag(QStyleOptionViewItem::HasDecoration));
        QCOMPARE(opt.decorationPosition, QStyleOptionViewItem::Right);
        QCOMPARE(opt.decorationSize, QSize(icon_size, icon_size));
    }

    // The view flags the current row with State_HasFocus; its focus frame wouldn't
    // span the icon area, so the delegate drops it
    void focus_frame_is_suppressed()
    {
        QStyleOptionViewItem opt = view_option(row(0));
        opt.state |= QStyle::State_HasFocus;
        delegate->initStyleOption(&opt, row(0));
        QVERIFY(!opt.state.testFlag(QStyle::State_HasFocus));
    }

    // Both SVGs must be compiled into the core library's resources and renderable
    // (which needs Qt SVG), and the edit target must get the filled one, not the outline
    void icons_load_from_resources()
    {
        const QImage outline = rendered_icon(0);
        QVERIFY(model->setData(row(0), true, PlaylistModel::IsEditTargetRole));
        const QImage filled = rendered_icon(0);

        QVERIFY(!outline.isNull());
        QVERIFY(!filled.isNull());
        QVERIFY(ink(outline) > 0);
        QVERIFY(ink(filled) > ink(outline));
    }

    // The filled pencil marks the playlist being edited; every other row shows the outline
    void icon_tracks_edit_target()
    {
        const QImage outline = rendered_icon(0);
        QCOMPARE(rendered_icon(1), outline);

        QVERIFY(model->setData(row(1), true, PlaylistModel::IsEditTargetRole));
        QVERIFY(rendered_icon(1) != outline);
        QCOMPARE(rendered_icon(0), outline);
        QCOMPARE(rendered_icon(2), outline);

        QVERIFY(model->setData(row(1), false, PlaylistModel::IsEditTargetRole));
        QCOMPARE(rendered_icon(1), outline);
    }

    // --- Size hint ----------------------------------------------------------------

    // Zero width lets QListView stretch rows to the viewport; height must fit the icon
    void size_hint_has_zero_width()
    {
        const QSize hint = delegate->sizeHint(view_option(row(0)), row(0));
        QCOMPARE(hint.width(), 0);
        QVERIFY(hint.height() >= icon_size);
    }

    void long_names_elide_instead_of_scrolling()
    {
        model->append(Playlist{tmp.filePath(QString(200, u'x') + u".m3u"_s)});
        const QScrollBar* hbar = view->horizontalScrollBar();

        // Control: the stock delegate sizes rows to their text, so a long name scrolls.
        // This also guarantees the check below can't pass before the view relayouts.
        QStyledItemDelegate stock;
        view->setItemDelegate(&stock);
        QTRY_VERIFY(hbar->maximum() > 0);

        view->setItemDelegate(delegate);
        QTRY_COMPARE(hbar->maximum(), 0);
        QCOMPARE(view->visualRect(model->last()).width(), view->viewport()->width());
    }

    // --- Clicks through the view ----------------------------------------------------

    // Sanity check the geometry the click tests rely on
    void icon_sits_at_right_end_of_row()
    {
        const QRect row_rect = view->visualRect(row(0));
        const QRect icon     = icon_rect(0);

        QCOMPARE(icon.size(), QSize(icon_size, icon_size));
        QVERIFY(row_rect.contains(icon));
        QVERIFY(icon.left() > row_rect.center().x());
        QVERIFY(!icon.contains(name_pos(0)));
    }

    void clicking_icon_toggles_edit_target()
    {
        QTest::mouseClick(view->viewport(), Qt::LeftButton, {}, icon_pos(1));
        QVERIFY(is_edit_target(1));

        QTest::mouseClick(view->viewport(), Qt::LeftButton, {}, icon_pos(1));
        QVERIFY(!is_edit_target(1));
    }

    void clicking_another_icon_moves_edit_target()
    {
        QTest::mouseClick(view->viewport(), Qt::LeftButton, {}, icon_pos(0));
        QTest::mouseClick(view->viewport(), Qt::LeftButton, {}, icon_pos(2));
        QVERIFY(!is_edit_target(0));
        QVERIFY(is_edit_target(2));
    }

    // MainWindow opens a playlist whenever the selection changes, so toggling edit
    // mode must not select the row as a side effect
    void clicking_icon_does_not_select_row()
    {
        QSignalSpy selection(view->selectionModel(), &QItemSelectionModel::selectionChanged);
        QTest::mouseClick(view->viewport(), Qt::LeftButton, {}, icon_pos(1));

        QCOMPARE(selection.count(), 0);
        QVERIFY(!view->selectionModel()->hasSelection());
    }

    // Toggling happens on release, so a press alone (e.g. the start of a drag) doesn't
    void toggle_happens_on_release()
    {
        QTest::mousePress(view->viewport(), Qt::LeftButton, {}, icon_pos(1));
        QVERIFY(!is_edit_target(1));

        QTest::mouseRelease(view->viewport(), Qt::LeftButton, {}, icon_pos(1));
        QVERIFY(is_edit_target(1));
    }

    void clicking_name_selects_without_toggling()
    {
        QTest::mouseClick(view->viewport(), Qt::LeftButton, {}, name_pos(1));

        QCOMPARE(view->selectionModel()->selectedIndexes(), QModelIndexList{row(1)});
        QVERIFY(!is_edit_target(1));
    }

    // Right clicks only open the context menu: they must neither select (and so open)
    // the playlist nor toggle edit mode, wherever they land in the row
    void right_click_neither_selects_nor_toggles_data()
    {
        QTest::addColumn<bool>("on_icon");

        QTest::newRow("on name") << false;
        QTest::newRow("on icon") << true;
    }

    void right_click_neither_selects_nor_toggles()
    {
        QFETCH(bool, on_icon);

        QSignalSpy selection(view->selectionModel(), &QItemSelectionModel::selectionChanged);
        QTest::mouseClick(
            view->viewport(), Qt::RightButton, {}, on_icon ? icon_pos(1) : name_pos(1));

        QCOMPARE(selection.count(), 0);
        QVERIFY(!is_edit_target(1));
    }

    // --- Events sent straight to editorEvent() ---------------------------------------

    // A double click arrives as press, release, press, dblclick, release. Each release
    // already toggles, so the dblclick itself must be swallowed without toggling again.
    void double_click_event_is_swallowed()
    {
        QVERIFY(send(QEvent::MouseButtonDblClick, Qt::LeftButton, icon_pos(1), 1));
        QVERIFY(!is_edit_target(1));
    }

    // Moves fall through to QStyledItemDelegate, which ignores them for plain items
    void non_button_events_pass_through()
    {
        QVERIFY(!send(QEvent::MouseMove, Qt::NoButton, icon_pos(1), 1));
        QVERIFY(!is_edit_target(1));
    }

    void middle_click_on_icon_is_ignored()
    {
        QVERIFY(!send(QEvent::MouseButtonRelease, Qt::MiddleButton, icon_pos(1), 1));
        QVERIFY(!is_edit_target(1));
    }

    void release_off_icon_does_not_toggle()
    {
        QVERIFY(!send(QEvent::MouseButtonRelease, Qt::LeftButton, name_pos(1), 1));
        QVERIFY(!is_edit_target(1));
    }
};

QTEST_MAIN(TestPlaylistDelegate)
#include "tst_playlistdelegate.moc"
