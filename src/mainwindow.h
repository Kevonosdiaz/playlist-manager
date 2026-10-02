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
    const QStringList   VALID_FILETYPES{
        "*.mp3", "*.mp2", "*.ogg",  "*.oga",  "*.opus", "*.flac", "*.m4a",
        "*.mp4", "*.aac", "*.wav",  "*.wave", "*.wv",   "*.mpc",  "*.ape",
        "*.wma", "*.aif", "*.aiff", "*.dsf",  "*.dff",  "*.sacd",
    };
};
#endif // MAINWINDOW_H
