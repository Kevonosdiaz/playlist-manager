#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "media/playlist.h"
#include "media/songtablemodel.h"

#include <QFileSystemModel>
#include <QFutureWatcher>
#include <QMainWindow>
#include <QQueue>
#include <QStandardItemModel>

#include <optional>

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
    Ui::MainWindow*     ui;
    QStandardItemModel* playlist_model;
    SongTableModel*     song_model;
    QFileSystemModel*   fs_model;

    QFutureWatcher<std::optional<Song>> tag_fwatcher;
    // Used to get diff for # of new songs added
    int prev_song_count = 0;

    QFutureWatcher<Song> playlist_fwatcher;
    // Values to handle open two playlists simultaneously
    std::optional<Playlist> pending_playlist;
    std::optional<QString>  next_pl_path;

    // Track open dirs in song viewer (or dirs in append mode)
    QSet<QString>       open_dirs;
    QList<Playlist>     playlists;
    QQueue<QStringList> pending_tag_scans;

    void        setup_models();
    void        setup_views();
    void        tag_list(QStringList paths);
    void        tag_playlist(const QString& path);
    QStringList scan_dir(const QDirListing& dir);
    void        load_dir_to_songviewer(const QString& dirpath);
    void        load_playlist_to_songviewer(const Playlist& pl);
    void        fit_song_columns_to_contents();
    void        closeEvent(QCloseEvent* event) override;
    void        restore_window_settings();
    void        restore_menubar_settings();
};
#endif // MAINWINDOW_H
