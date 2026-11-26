// Tests for ToolTipLabel (src/utils/tooltiplabel.cpp): right-elided text, with the full
// text moved into a tooltip only while it's elided.

#include "utils/tooltiplabel.h"

#include <QTest>

#include <memory>

using namespace Qt::StringLiterals;

class TestToolTipLabel : public QObject
{
    Q_OBJECT

private:
    const QString long_text = u"/home/user/Music/Some Artist/Some Album (Deluxe Edition)"_s;

    // Labels live in a shown parent so resizes deliver resizeEvent() synchronously
    std::unique_ptr<QWidget> host;
    ToolTipLabel*            label = nullptr; // Owned by host

    // What the label actually draws. ToolTipLabel::text() hides QLabel::text() and
    // returns the full, unelided string instead.
    QString shown() const
    {
        return label->QLabel::text();
    }

    int text_width(const QString& s) const
    {
        return label->fontMetrics().horizontalAdvance(s);
    }

    void set_width(int w)
    {
        label->resize(w, label->height());
    }

    void fit(const QString& s)
    {
        set_width(text_width(s) + 20);
    }

    void squeeze(const QString& s)
    {
        set_width(text_width(s) / 2);
    }

    static QString tooltip_for(const QString& s)
    {
        return u"<p>"_s + s + u"</p>"_s;
    }

private slots:
    void init()
    {
        host  = std::make_unique<QWidget>();
        label = new ToolTipLabel(host.get());
        host->resize(1000, 100);
        host->show();
        QVERIFY(QTest::qWaitForWindowExposed(host.get()));
    }

    void cleanup()
    {
        host.reset();
        label = nullptr;
    }

    void fitting_text_is_shown_in_full()
    {
        fit(long_text);
        label->setText(long_text);

        QCOMPARE(shown(), long_text);
        QCOMPARE(label->text(), long_text);
        QVERIFY(label->toolTip().isEmpty());
    }

    void long_text_is_elided_with_tooltip()
    {
        squeeze(long_text);
        label->setText(long_text);

        const QString s = shown();
        QVERIFY(s != long_text);
        QVERIFY(long_text.startsWith(s.first(2))); // ElideRight keeps the start
        QVERIFY(text_width(s) <= label->contentsRect().width());
        QCOMPARE(label->text(), long_text); // Callers still get the full text
        QCOMPARE(label->toolTip(), tooltip_for(long_text));
    }

    // The <p> wrapper makes the tooltip rich text, so directory names containing
    // markup characters must be escaped rather than rendered
    void tooltip_escapes_html()
    {
        const QString text = u"<b>Rock & Roll</b> Greatest Hits (Remastered)"_s;
        squeeze(text);
        label->setText(text);

        QCOMPARE(label->toolTip(),
                 u"<p>&lt;b&gt;Rock &amp; Roll&lt;/b&gt; Greatest Hits (Remastered)</p>"_s);
    }

    void widening_removes_elision()
    {
        squeeze(long_text);
        label->setText(long_text);
        QVERIFY(!label->toolTip().isEmpty());

        fit(long_text);
        QCOMPARE(shown(), long_text);
        QVERIFY(label->toolTip().isEmpty());
    }

    void narrowing_adds_elision()
    {
        fit(long_text);
        label->setText(long_text);
        QVERIFY(label->toolTip().isEmpty());

        squeeze(long_text);
        QVERIFY(shown() != long_text);
        QCOMPARE(label->toolTip(), tooltip_for(long_text));
    }

    void replacing_text_clears_stale_tooltip()
    {
        squeeze(long_text);
        label->setText(long_text);
        label->setText(u"Music"_s);

        QCOMPARE(shown(), u"Music"_s);
        QVERIFY(label->toolTip().isEmpty());
    }

    // A long path must never force the surrounding layout wider
    void minimum_size_hint_has_zero_width()
    {
        label->setText(long_text);
        QCOMPARE(label->minimumSizeHint().width(), 0);
        QVERIFY(label->minimumSizeHint().height() > 0);
    }
};

QTEST_MAIN(TestToolTipLabel)
#include "tst_tooltiplabel.moc"
