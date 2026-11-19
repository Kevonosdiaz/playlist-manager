#include "mainwindow.h"
#include "media/dirscan.h"
#include "media/tag.h"
#include "ui_mainwindow.h"
#include "utils/constants.h"
#include "utils/logger.h"
#include "utils/settings.h"

#include <QDirListing>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFutureWatcher>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTableView>
#include <QTreeView>
#include <QtConcurrentMap>

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

    setup_models();
    setup_views();

    // Handle signal-slot connections
    setup_menu_action_connections();
    setup_model_view_connections();

    // Restore state from QSettings
    restore_window_settings();
    restore_menubar_settings();
    playlist_model->restore_playlists();
}

MainWindow::~MainWindow()
{
    tag_fwatcher.cancel();
    playlist_fwatcher.cancel();
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
    // Setting new index causes selectionChanged signal to fire, leading to tag_playlist() being called in slot
    ui->playlistList->setCurrentIndex(idx);
}

void MainWindow::create_new_playlist()
{
    QFileDialog dialog(this, u"New Playlist File"_s, default_music_dir(), M3U_FILE_FILTER);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDefaultSuffix(".m3u");
    if(dialog.exec() != QDialog::Accepted)
        return;
    QString filename = dialog.selectedFiles().front();

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
void MainWindow::toggle_recursive_directory_scan()
{
    bool new_val = !Settings::get_recursive_mode();
    Settings::set_recursive_mode(new_val);
    ui->actionToggleRecursiveDirectoryScan->setChecked(new_val);

    if(!ui->actionToggleAppendDirectoryScan->isChecked())
        open_dirs.clear();
}

// Toggle setting to append song files to song viewer
void MainWindow::toggle_appending_directory_scan()
{
    bool new_val = !Settings::get_append_mode();
    Settings::set_append_mode(new_val);
    ui->actionToggleAppendDirectoryScan->setChecked(new_val);

    // We should clear open dirs if append mode got turned off, so
    // a user can reopen a directory while clearing other open dirs
    if(!new_val)
        open_dirs.clear();
}

void MainWindow::on_playlist_selected(const QItemSelection& selected)
{
    if(selected.empty())
        return;
    const QModelIndex curr = selected.indexes().front();
    const Playlist&   pl   = playlist_model->at(curr.row());
    // Do not repeat work if re-selected
    if(curr == tagging_row && !playlist_fwatcher.isCanceled())
        next_row = QModelIndex{};
    // NOTE: Restored playlists show up, but are not tagged until loaded
    else if(!pl.tagged())
    {
        if(tagging_row.isValid())
        {
            next_row = curr;
            playlist_fwatcher.cancel();
        }
        else
        {
            tag_playlist(curr);
        }
    }
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
    // Initialize playlist QListView
    playlist_model = new PlaylistModel(this);

    // Initialize song viewer QTableView headers
    song_model = new SongTableModel(this);

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
            &QAction::triggered,
            this,
            &MainWindow::toggle_recursive_directory_scan);
    connect(ui->actionToggleAppendDirectoryScan,
            &QAction::triggered,
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

    // Hook up watcher to concurrent song tagging incoming results
    connect(&tag_fwatcher,
            &QFutureWatcher<std::optional<Song>>::resultsReadyAt,
            this,
            [this](int begin, int end) {
                QVector<Song> batch;
                for(int i = begin; i < end; i++)
                {
                    if(auto s = tag_fwatcher.resultAt(i))
                        batch.push_back(std::move(*s));
                }
                song_model->add_songs(std::move(batch));
            });
    connect(&tag_fwatcher, &QFutureWatcher<std::optional<Song>>::finished, this, [this]() {
        // Run next directory (if added with append mode)
        if(!pending_tag_scans.empty())
        {
            logger::sb_log(ui->statusbar, QString(u"Loading directory..."_s), LogLevel::Info, 0);
            tag_list(pending_tag_scans.dequeue());
            return;
        }

        if(tag_fwatcher.isCanceled())
            return;

        // Sort, resize cols, etc.
        ui->songViewer->sortByColumn(SongTableModel::Title, Qt::AscendingOrder);
        fit_song_columns_to_contents();
        auto curr_song_count = song_model->rowCount();
        logger::sb_log(ui->statusbar,
                       QString("Loaded %1 song(s)").arg(curr_song_count - prev_song_count),
                       LogLevel::Info);
    });

    // Watcher for handling concurrent playlist tagging
    connect(&playlist_fwatcher, &QFutureWatcher<Song>::finished, this, [this]() {
        // If another playlist was opened by user, skip loading this playlist
        QPersistentModelIndex row = std::exchange(tagging_row, QModelIndex{});
        if(next_row.isValid())
        {
            tag_playlist(std::exchange(next_row, QModelIndex{}));
            return;
        }
        if(playlist_fwatcher.isCanceled() || !row.isValid())
            return;

        // Get QFuture result and set target playlist's songs to it, then display it
        auto results = playlist_fwatcher.future().results();
        playlist_model->set_songs(row.row(), results);
        if(ui->playlistList->selectionModel()->isSelected(row))
            load_playlist_to_songviewer(playlist_model->at(row.row()));

        if(results.empty())
            logger::sb_log(ui->statusbar, u"Playlist is empty"_s, LogLevel::Info);
        else
            logger::sb_log(
                ui->statusbar, QString("Loaded %1 song(s)").arg(results.size()), LogLevel::Info);
    });
}

// Concurrently tag song at each path in paths
void MainWindow::tag_list(QStringList paths)
{
    // Connected slot will add scanned songs to song_model
    tag_fwatcher.setFuture(QtConcurrent::mapped(std::move(paths), path_to_song));
}

void MainWindow::tag_playlist(const QModelIndex& idx)
{
    pending_tag_scans.clear();
    tag_fwatcher.cancel();
    logger::sb_log(ui->statusbar, QString(u"Loading playlist..."_s), LogLevel::Info, 0);
    tagging_row = idx;
    playlist_fwatcher.setFuture(playlist_model->at(tagging_row.row()).tag_all_songs());
}

void MainWindow::select_playlist(const QModelIndex& idx)
{
    ui->playlistList->setFocus();
    ui->playlistList->setCurrentIndex(idx);
}

void MainWindow::remove_playlist(const QModelIndex& idx)
{
    // Cancel unnecessary tagging work if currently tagging
    if(idx == tagging_row)
        playlist_fwatcher.cancel();
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
    QSettings s;
    bool      recursive_mode = Settings::get_recursive_mode();
    bool      append_mode    = Settings::get_append_mode();

    // Clear any open playlist selection since a directory is opened
    ui->playlistList->selectionModel()->clear();

    // If recursive mode is on, add flag to scan subdirs for songs
    using F                               = QDirListing::IteratorFlag;
    QDirListing::IteratorFlags iter_flags = F::ExcludeOther;
    if(recursive_mode)
        iter_flags |= F::Recursive;

    QDirListing dir(dirpath, VALID_FILETYPES, iter_flags);

    // Exit if empty
    if(dir.begin() == dir.end())
    {
        logger::dual_log(ui->statusbar, u"No files found in directory, skipping"_s, LogLevel::Info);
        return;
    }

    // Skip reopening if already opened/scanned dir
    // Note: does not prevent recursive scan from finding repeats; open_dirs handles those
    if(open_dirs.contains(dirpath + "/"))
    {
        logger::dual_log(
            ui->statusbar, u"Attempted to reopen same directory, skipping"_s, LogLevel::Info);
        return;
    }

    // If not in append mode we only consider one directory open at a time, so clear old paths from in-progress scans/open_dirs
    // Also clear is open_dirs.empty() to catch playlist -> dir transition in append mode
    if(!append_mode || open_dirs.empty())
    {
        pending_tag_scans.clear();
        tag_fwatcher.cancel();
        open_dirs.clear();
        clear_songviewer();
    }

    // Cancel pending work for opening a playlist earlier
    playlist_fwatcher.cancel();
    next_row = QModelIndex{};

    // Extract song metadata using TagLib before replacing/appending to table
    QStringList song_paths = scan_dir(dir, open_dirs);
    open_dirs.insert(dirpath + "/");

    // Enqueue current dir if another dir is currently being tagged, else tag immediately
    if(append_mode && tag_fwatcher.isRunning())
        pending_tag_scans.enqueue(std::move(song_paths));
    else
    {
        logger::sb_log(ui->statusbar, QString(u"Loading directory..."_s), LogLevel::Info, 0);
        prev_song_count = song_model->rowCount();
        tag_list(std::move(song_paths));
    }

    QFileInfo     fi(dirpath);
    const QString text = ui->songViewerLabel->text();
    if(text.isEmpty())
        ui->songViewerLabel->setText(fi.fileName());
    else
        ui->songViewerLabel->setText(text + u", "_s + fi.fileName());
}

// Pass along Songs in Playlist to load into SongTableModel
void MainWindow::load_playlist_to_songviewer(const Playlist& pl)
{
    ui->fileExplorer->selectionModel()->clear();
    pending_tag_scans.clear();
    tag_fwatcher.cancel();
    open_dirs.clear();

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
