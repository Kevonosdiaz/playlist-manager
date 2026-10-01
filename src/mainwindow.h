#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void open_about_qt();
    void open_playlist_file_selector();
    void open_music_dir_selector();

private:
    Ui::MainWindow *ui;
    QStandardItemModel* model;
};
#endif // MAINWINDOW_H
