#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "songtablemodel.h"
#include <QFileSystemModel>
#include <QMainWindow>

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
    void set_file_explorer_root();
    void toggle_recursive_directory_scan();
    void toggle_appending_directory_scan();

private:
    Ui::MainWindow*   ui;
    SongTableModel*   song_model;
    QFileSystemModel* fs_model;
    const QStringList VALID_FILETYPES{
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

    void setup_models();
    void setup_views();
    void load_dir_to_songviewer(const QString& dirpath);
    void closeEvent(QCloseEvent* event) override;
    void restore_window_settings();
    void restore_menubar_settings();
};
#endif // MAINWINDOW_H
