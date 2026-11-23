#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "media/dirtagger.h"
#include "media/playlist.h"
#include "media/playlistmodel.h"
#include "media/playlisttagger.h"
#include "media/songtablemodel.h"

#include <QFileSystemModel>
#include <QItemSelection>
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
    void create_new_playlist();
    void open_music_dir_selector();
    void open_explorer_dir(const QModelIndex& idx);
    void set_file_explorer_root();
    void toggle_recursive_directory_scan(bool on);
    void toggle_appending_directory_scan(bool on);
    void on_playlist_selected(const QItemSelection& selected);
    void show_playlist_ctx_menu(const QPoint& pos);

private:
    Ui::MainWindow*   ui;
    PlaylistModel*    playlist_model;
    SongTableModel*   song_model;
    QFileSystemModel* fs_model;
    PlaylistTagger*   playlist_tagger;
    DirTagger*        dir_tagger;

    void setup_models();
    void setup_views();
    void setup_menu_action_connections();
    void setup_model_view_connections();
    void tag_list(QStringList paths);
    void select_playlist(const QModelIndex& idx);
    void remove_playlist(const QModelIndex& idx);
    void delete_playlist(const QModelIndex& idx);
    void load_dir_to_songviewer(const QString& dirpath);
    void load_playlist_to_songviewer(const Playlist& pl);
    void clear_songviewer();
    void fit_song_columns_to_contents();
    void closeEvent(QCloseEvent* event) override;
    void restore_window_settings();
    void restore_menubar_settings();
};
#endif // MAINWINDOW_H
