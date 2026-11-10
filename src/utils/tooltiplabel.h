#ifndef TOOLTIPLABEL_H
#define TOOLTIPLABEL_H

#include <QLabel>
#include <QString>

class ToolTipLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ToolTipLabel(QWidget* parent = nullptr)
        : QLabel(parent) { };
    QSize minimumSizeHint() const override;
    // Below two hide the original QLabel non-virtual functions to
    // ensure this widget is not used without elision/tooltip support
    void           setText(const QString& s);
    const QString& text() const;

private:
    QString ftext;
    void    update_elision();

protected:
    void resizeEvent(QResizeEvent* e) override;
};

#endif /* TOOLTIPLABEL_H */
