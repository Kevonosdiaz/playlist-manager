#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "utils/constants.h"
#include "utils/logger.h"
#include "utils/settings.h"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTableView>
#include <QTreeView>

#include <algorithm>
#include <utility>

// Allow ""_s operator on string literals for compile time QString construction
using namespace Qt::StringLiterals;

namespace
{
    QString default_music_dir()
    {
        QString dir = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
        return dir.isEmpty() ? QDir::homePath() : dir;
    }
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // Set default splitter sizes for left/right sides
    ui->mainSplitter->setSizes({350, 814});

    // Restore state from QSettings
    restore_window_settings();
    restore_menubar_settings();

    setup_models();
    setup_views();

    // Handle signal-slot connections
    setup_menu_action_connections();
    setup_model_view_connections();
    playlist_model->restore_playlists();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::open_about_qt()
{
    QApplication::aboutQt();
}

void MainWindow::open_playlist_file_selector()
{
    QString filename = QFileDialog::getOpenFileName(
        this, u"Open Playlist File"_s, default_music_dir(), M3U_FILE_FILTER);
    if(filename.isEmpty())
        return;

    // If playlist saved in playlist list already, select that instead
    int find_res = playlist_model->find(filename);
    if(find_res != -1)
    {
        select_playlist(playlist_model->index(find_res));
        return;
    }

    // Create a new Playlist which will be tagged
    playlist_model->append(Playlist{filename});
    auto idx = playlist_model->index(playlist_model->rowCount() - 1);
    ui->playlistList->setCurrentIndex(idx);
}

// TODO: Extract logic out of MainWindow into Playlist or PlaylistModel
void MainWindow::create_new_playlist()
{
    QFileDialog dialog(this, u"New Playlist File"_s, default_music_dir(), M3U_FILE_FILTER);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDefaultSuffix(".m3u");
    if(dialog.exec() != QDialog::Accepted)
        return;
    QString filename = dialog.selectedFiles().constFirst();

    if(filename.isEmpty())
        return;

    QFile file(filename);
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        logger::dual_log(ui->statusbar, file.errorString(), LogLevel::Warn);
        return;
    }
    QTextStream out(&file);
    out << "#EXTM3U\n";
    file.close();
    Playlist pl{filename};
    playlist_model->append(std::move(pl));

    // Set UI to select new idx & trigger playlist open
    auto idx = playlist_model->last();
    if(idx.isValid())
        select_playlist(idx);

    logger::dual_log(ui->statusbar, QString("Created playlist at %1").arg(filename));
}

void MainWindow::open_music_dir_selector()
{
    QString dirpath
        = QFileDialog::getExistingDirectory(this, u"Open Directory"_s, default_music_dir());

    if(dirpath.isEmpty())
        return;
    load_dir_to_songviewer(dirpath);
}

void MainWindow::open_explorer_dir(const QModelIndex& idx)
{
    if(!idx.isValid())
        return;
    QString dirpath = fs_model->filePath(idx);
    load_dir_to_songviewer(dirpath);
}

// Prompt user to select root for the QFileSystemModel file explorer
void MainWindow::set_file_explorer_root()
{
    QString dirpath
        = QFileDialog::getExistingDirectory(this, u"Open Directory"_s, default_music_dir());

    if(dirpath.isEmpty())
        return;

    QString oldpath = fs_model->rootPath();
    if(oldpath == dirpath)
    {
        logger::dual_log(ui->statusbar, u"Same file explorer root chosen, skipping"_s);
        return;
    }

    QSettings s;
    s.setValue(Settings::f_explorer_root, dirpath);
    fs_model->setRootPath(dirpath);
    ui->fileExplorer->setRootIndex(fs_model->index(fs_model->rootPath()));
    logger::dual_log(ui->statusbar, u"Set file explorer root to "_s + dirpath);
}

// Toggle setting to cause QDirListing to scan directories recursively for song files
void MainWindow::toggle_recursive_directory_scan(bool on)
{
    Settings::set_recursive_mode(on);
    if(!ui->actionToggleAppendDirectoryScan->isChecked())
        dir_tagger->clear_open_dirs();
}

// Toggle setting to append song files to song viewer
void MainWindow::toggle_appending_directory_scan(bool on)
{
    Settings::set_append_mode(on);

    // We should clear open dirs if append mode got turned off, so
    // a user can reopen a directory while clearing other open dirs
    if(!on)
        dir_tagger->clear_open_dirs();
}

void MainWindow::on_playlist_selected(const QItemSelection& selected)
{
    if(selected.empty())
        return;
    const QModelIndex curr = selected.indexes().front();
    const Playlist&   pl   = playlist_model->at(curr.row());
    playlist_tagger->request_playlist(curr);
    // Either shows fully tagged, or bare playlist info from M3U (temporarily)
    load_playlist_to_songviewer(pl);
}

// TODO: Consider adding rename, "edit mode" options?
void MainWindow::show_playlist_ctx_menu(const QPoint& pos)
{
    const QModelIndex idx = ui->playlistList->indexAt(pos);
    if(!idx.isValid())
        return;
    QMenu                       menu(this);
    const QPersistentModelIndex target{idx};
    menu.addAction(u"Remove from List"_s, this, [this, target] {
        if(!target.isValid())
            return;
        remove_playlist(target);
    });
    menu.addAction(u"Delete Playlist"_s, this, [this, target] {
        if(!target.isValid())
            return;
        const auto reply
            = QMessageBox::question(this,
                                    u"Confirm Action"_s,
                                    u"Do you want to delete playlist \"%1\" from disk?"_s.arg(
                                        playlist_model->at(target.row()).get_name()));
        if(reply == QMessageBox::Yes && target.isValid())
            delete_playlist(target);
    });

    menu.exec(ui->playlistList->viewport()->mapToGlobal(pos));
}

// Initialize data models and configure properties
void MainWindow::setup_models()
{
    // Initialize playlist QListView and playlist tagger
    playlist_model  = new PlaylistModel(this);
    playlist_tagger = new PlaylistTagger(playlist_model, this);

    // Initialize song viewer QTableView headers
    song_model = new SongTableModel(this);
    dir_tagger = new DirTagger(song_model, this);

    // Initialize file explorer to saved root, otherwise Music dir or home
    QSettings s;
    QString   dir = s.value(Settings::f_explorer_root, default_music_dir()).toString();
    fs_model      = new QFileSystemModel(this);
    fs_model->setRootPath(dir);
    fs_model->setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
}

// Set models for views + configure view properties
void MainWindow::setup_views()
{
    // Setup playlist viewer (QListView)
    ui->playlistList->setModel(playlist_model);
    ui->playlistList->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);

    // Setup song viewer (QTableView)
    ui->songViewer->setIconSize(QSize(ART_SIZE, ART_SIZE));
    ui->songViewer->setModel(song_model);

    // Fix row height based on album art size, let non art columns be resized
    ui->songViewer->verticalHeader()->setDefaultSectionSize(ART_SIZE + 8);
    ui->songViewer->setSortingEnabled(true);
    auto* hh = ui->songViewer->horizontalHeader();
    hh->setMinimumSectionSize(30);
    hh->setSectionResizeMode(QHeaderView::Interactive);
    hh->setSectionResizeMode(SongTableModel::AlbumArt, QHeaderView::Fixed);
    hh->setSectionResizeMode(SongTableModel::Title, QHeaderView::Stretch);
    hh->resizeSection(SongTableModel::AlbumArt, ART_SIZE + 8);

    hh->setDefaultAlignment(Qt::AlignLeft);
    hh->setSortIndicatorShown(true);
    ui->songViewer->setWordWrap(false);

    // Setup file explorer (QTreeView)
    ui->fileExplorer->setModel(fs_model);
    ui->fileExplorer->setRootIndex(fs_model->index(fs_model->rootPath()));

    // Hide header and hide all columns except filename column
    ui->fileExplorer->setHeaderHidden(true);
    ui->fileExplorer->setColumnHidden(1, true);
    ui->fileExplorer->setColumnHidden(2, true);
    ui->fileExplorer->setColumnHidden(3, true);
}

void MainWindow::setup_menu_action_connections()
{
    connect(
        ui->actionOpenFile, &QAction::triggered, this, &MainWindow::open_playlist_file_selector);
    connect(
        ui->actionOpenDirectory, &QAction::triggered, this, &MainWindow::open_music_dir_selector);
    connect(ui->actionAboutQt, &QAction::triggered, this, &MainWindow::open_about_qt);
    connect(ui->actionSetFileExplorerRoot,
            &QAction::triggered,
            this,
            &MainWindow::set_file_explorer_root);
    connect(ui->actionToggleRecursiveDirectoryScan,
            &QAction::toggled,
            this,
            &MainWindow::toggle_recursive_directory_scan);
    connect(ui->actionToggleAppendDirectoryScan,
            &QAction::toggled,
            this,
            &MainWindow::toggle_appending_directory_scan);
    connect(ui->actionCreatePlaylist, &QAction::triggered, this, &MainWindow::create_new_playlist);
}

void MainWindow::setup_model_view_connections()
{
    connect(ui->playlistList->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this,
            &MainWindow::on_playlist_selected);
    connect(ui->playlistList,
            &QWidget::customContextMenuRequested,
            this,
            &MainWindow::show_playlist_ctx_menu);

    connect(ui->fileExplorer, &QTreeView::clicked, this, &MainWindow::open_explorer_dir);

    connect(dir_tagger, &DirTagger::started, this, [this]() {
        logger::sb_log(ui->statusbar, QString(u"Loading directory..."_s), LogLevel::Info, 0);
    });

    connect(dir_tagger, &DirTagger::empty_dir, this, [this]() {
        logger::dual_log(ui->statusbar, u"No files found in directory, skipping"_s, LogLevel::Info);
    });

    connect(dir_tagger, &DirTagger::repeat_dir, this, [this]() {
        logger::dual_log(
            ui->statusbar, u"Attempted to reopen same directory, skipping"_s, LogLevel::Info);
    });

    connect(dir_tagger, &DirTagger::clear_songviewer, this, &MainWindow::clear_songviewer);

    connect(playlist_tagger, &PlaylistTagger::started, this, [this]() {
        logger::sb_log(ui->statusbar, QString(u"Loading playlist..."_s), LogLevel::Info, 0);
    });

    connect(dir_tagger, &DirTagger::dir_tagged, this, [this](int song_count) {
        // Sort, resize cols, etc.
        ui->songViewer->sortByColumn(SongTableModel::Title, Qt::AscendingOrder);
        fit_song_columns_to_contents();
        logger::sb_log(ui->statusbar, QString("Loaded %1 song(s)").arg(song_count), LogLevel::Info);
    });

    connect(playlist_tagger,
            &PlaylistTagger::playlist_tagged,
            this,
            [this](const QPersistentModelIndex& idx) {
                const auto& playlist = playlist_model->at(idx.row());
                if(ui->playlistList->selectionModel()->isSelected(idx))
                    load_playlist_to_songviewer(playlist);

                if(playlist.empty())
                    logger::sb_log(ui->statusbar, u"Playlist is empty"_s, LogLevel::Info);
                else
                    logger::sb_log(ui->statusbar,
                                   QString("Loaded %1 song(s)").arg(playlist.size()),
                                   LogLevel::Info);
            });
}

void MainWindow::select_playlist(const QModelIndex& idx)
{
    ui->playlistList->setFocus();
    ui->playlistList->setCurrentIndex(idx);
}

void MainWindow::remove_playlist(const QModelIndex& idx)
{
    // Clear open playlist from song viewer if it is being removed
    if(ui->playlistList->selectionModel()->isSelected(idx))
        clear_songviewer();
    bool res = playlist_model->removeRows(idx.row(), 1);
    if(!res)
        logger::dual_log(ui->statusbar, u"Failed to remove playlist"_s, LogLevel::Warn);
    playlist_model->save_playlists();
}

void MainWindow::delete_playlist(const QModelIndex& idx)
{
    // Try deleting file first, then remove Playlist object from model if successful or already removed from disk
    QFile f(playlist_model->at(idx.row()).get_path());
    if(!f.remove() && f.exists())
    {
        logger::dual_log(
            ui->statusbar, u"Failed to delete playlist: "_s + f.errorString(), LogLevel::Warn);
        return;
    }
    remove_playlist(idx);
}

// Scan song files into Song structs and load into SongTableModel
void MainWindow::load_dir_to_songviewer(const QString& dirpath)
{
    // Clear any open playlist selection since a directory is opened
    ui->playlistList->selectionModel()->clear();

    // Cancel pending work for opening a playlist earlier
    playlist_tagger->cancel();

    bool recursive_mode = Settings::get_recursive_mode();
    bool append_mode    = Settings::get_append_mode();

    bool res = dir_tagger->request_dir(dirpath, recursive_mode, append_mode);

    // Display text if directory is being tagged
    if(res)
    {
        QFileInfo     fi(dirpath);
        const QString text = ui->songViewerLabel->text();
        if(text.isEmpty())
            ui->songViewerLabel->setText(fi.fileName());
        else
            ui->songViewerLabel->setText(text + u", "_s + fi.fileName());
    }
}

// Pass along Songs in Playlist to load into SongTableModel
void MainWindow::load_playlist_to_songviewer(const Playlist& pl)
{
    ui->fileExplorer->selectionModel()->clear();
    dir_tagger->cancel();

    song_model->set_songs(pl.get_songs());
    ui->songViewer->sortByColumn(SongTableModel::Title, Qt::AscendingOrder);
    fit_song_columns_to_contents();
    ui->songViewerLabel->setText(pl.get_name());
}

void MainWindow::clear_songviewer()
{
    song_model->set_songs({});
    ui->songViewerLabel->setText({});
}

// Resize columns based on text content, up to a max cap
void MainWindow::fit_song_columns_to_contents()
{
    auto*              hh = ui->songViewer->horizontalHeader();
    const QFontMetrics fm = ui->songViewer->fontMetrics();
    // Measure out the advance of MAX_COLUMN_CHARS copies of 'M'
    const int max_cap = fm.horizontalAdvance(QString(MAX_COLUMN_CHARS, u'M'));

    // Skip title since it stretches to fill
    for(int col : {SongTableModel::Artist,
                   SongTableModel::Album,
                   SongTableModel::Length,
                   SongTableModel::Track})
    {
        ui->songViewer->resizeColumnToContents(col);
        const int width = hh->sectionSize(col);
        hh->resizeSection(col, std::min(width, max_cap));
    }
}

// Save window geometry and app state on app close
void MainWindow::closeEvent(QCloseEvent* event)
{
    QSettings s;
    s.setValue(Settings::w_geometry, saveGeometry());
    s.setValue(Settings::w_state, saveState());
    playlist_model->save_playlists();
    QMainWindow::closeEvent(event);
}

void MainWindow::restore_window_settings()
{
    QSettings s;
    if(s.contains(Settings::w_geometry))
        restoreGeometry(s.value(Settings::w_geometry).toByteArray());
    if(s.contains(Settings::w_state))
        restoreState(s.value(Settings::w_state).toByteArray());
}

// Restore state of check boxes in menubar
void MainWindow::restore_menubar_settings()
{
    QSettings s;
    bool      recursive_checked = Settings::get_recursive_mode();
    ui->actionToggleRecursiveDirectoryScan->setChecked(recursive_checked);
    bool append_checked = Settings::get_append_mode();
    ui->actionToggleAppendDirectoryScan->setChecked(append_checked);
}
