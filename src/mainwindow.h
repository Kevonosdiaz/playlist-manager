#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QFileSystemModel>
#include <QMainWindow>
#include <QStandardItemModel>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void open_about_qt();
    void open_playlist_file_selector();
    void open_music_dir_selector();
    void open_explorer_dir(const QModelIndex& idx);

private:
    Ui::MainWindow*     ui;
    QStandardItemModel* song_model;
    QFileSystemModel*   fs_model;
    const QStringList   VALID_FILETYPES{
        "*.mp3",
        "*.ogg",
        "*.oga",
        "*.opus",
        "*.flac",
        "*.m4a",
        "*.mp4",
        "*.aac",
        "*.wav",
        "*.wave",
        "*.wv",
    };

    void load_dir_to_songviewer(const QString& dirpath);
};
#endif // MAINWINDOW_H
