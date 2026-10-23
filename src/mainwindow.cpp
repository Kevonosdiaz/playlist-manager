#include "mainwindow.h"
#include "tag.h"
#include "ui_mainwindow.h"
#include "utils/constants.h"
#include "utils/logger.h"

#include <QDirListing>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QSettings>
#include <QStandardPaths>
#include <QTableView>
#include <QTreeView>

// Allow ""_s operator on string literals for compile time QString construction
using namespace Qt::StringLiterals;

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
    connect(ui->fileExplorer, &QTreeView::clicked, this, &MainWindow::open_explorer_dir);

    // Restore state from QSettings
    restore_window_settings();
    restore_menubar_settings();
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
        this, u"Open Playlist File"_s, QString(), u"M3U Media Playlist (*.m3u *.m3u8)"_s);
    logger::log(filename, LogLevel::Debug);
}

void MainWindow::open_music_dir_selector()
{
    QString dirpath = QFileDialog::getExistingDirectory(
        this, u"Open Directory"_s, QStandardPaths::writableLocation(QStandardPaths::MusicLocation));

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
    QString dirpath = QFileDialog::getExistingDirectory(
        this, u"Open Directory"_s, QStandardPaths::writableLocation(QStandardPaths::MusicLocation));

    if(dirpath.isEmpty())
        return;

    QString oldpath = fs_model->rootPath();
    if(oldpath == dirpath)
    {
        logger::dual_log(ui->statusbar, u"Same file explorer root chosen, skipping"_s);
        return;
    }

    QSettings s;
    s.setValue(u"files/fileExplorerRoot"_s, dirpath);
    fs_model->setRootPath(dirpath);
    ui->fileExplorer->setRootIndex(fs_model->index(fs_model->rootPath()));
    logger::dual_log(ui->statusbar, u"Set file explorer root to "_s + dirpath);
}

// Toggle setting to cause QDirListing to scan directories recursively for song files
void MainWindow::toggle_recursive_directory_scan()
{
    QSettings s;
    bool      new_val = !s.value(u"files/useRecursiveScan"_s, false).toBool();
    s.setValue(u"files/useRecursiveScan"_s, new_val);
    ui->actionToggleRecursiveDirectoryScan->setChecked(new_val);
}

// Toggle setting to append song files to song viewer
void MainWindow::toggle_appending_directory_scan()
{
    QSettings s;
    bool      new_val = !s.value(u"files/useAppendingScan"_s, false).toBool();
    s.setValue(u"files/useAppendingScan"_s, new_val);
    ui->actionToggleAppendDirectoryScan->setChecked(new_val);

    // We should clear open dirs if append mode got turned off, so
    // a user can reopen a directory while clearing other open dirs
    if(!new_val)
        open_dirs.clear();
}

// Initialize data models and configure properties
void MainWindow::setup_models()
{
    // Initialize song viewer QTableView headers
    song_model = new SongTableModel(this);

    // Initialize file explorer to saved root, otherwise Music dir or home
    QSettings s;
    QString   default_dir = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    if(default_dir.isEmpty())
    {
        default_dir = QDir::homePath();
    }
    QString dir = s.value("files/fileExplorerRoot", default_dir).toString();
    fs_model    = new QFileSystemModel(ui->fileExplorer);
    fs_model->setRootPath(dir);
    fs_model->setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
}

// Set models for views + configure view properties
void MainWindow::setup_views()
{
    // Setup song viewer (QTableView)
    ui->songViewer->setIconSize(QSize(ART_SIZE, ART_SIZE));
    ui->songViewer->setModel(song_model);

    // Fix row height based on album art size and resize columns dynamically
    ui->songViewer->verticalHeader()->setDefaultSectionSize(ART_SIZE + 8);
    ui->songViewer->horizontalHeader()->setMinimumSectionSize(30);
    ui->songViewer->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->songViewer->horizontalHeader()->setStretchLastSection(true);
    ui->songViewer->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    ui->songViewer->setSortingEnabled(true);
    ui->songViewer->horizontalHeader()->setSortIndicatorShown(true);

    // Setup file explorer (QTreeView)
    ui->fileExplorer->setModel(fs_model);
    ui->fileExplorer->setRootIndex(fs_model->index(fs_model->rootPath()));

    // Hide header and hide all columns except filename column
    ui->fileExplorer->setHeaderHidden(true);
    ui->fileExplorer->setColumnHidden(1, true);
    ui->fileExplorer->setColumnHidden(2, true);
    ui->fileExplorer->setColumnHidden(3, true);
}

// Scan song files into Song structs and load into SongTableModel
// TODO: Break down this function into smaller pieces?
void MainWindow::load_dir_to_songviewer(const QString& dirpath)
{
    QSettings s;
    bool      recursive_mode = s.value(u"files/useRecursiveScan"_s, false).toBool();
    bool      append_mode    = s.value(u"files/useAppendingScan"_s, false).toBool();

    // If recursive mode is on, add flag to scan subdirs for songs
    using F                               = QDirListing::IteratorFlag;
    QDirListing::IteratorFlags iter_flags = F::ExcludeOther;
    if(recursive_mode)
        iter_flags |= F::Recursive;

    QDirListing dir(dirpath, VALID_FILETYPES, iter_flags);

    // Exit if empty
    if(dir.begin() == dir.end())
    {
        logger::sb_log(ui->statusbar, u"No files found in directory, skipping"_s, LogLevel::Info);
        logger::log(u"No files found in directory, skipping"_s, LogLevel::Info);
        return;
    }

    // Skip reopening if already opened/scanned dir
    // Note: does not prevent recursive scan from finding repeats; open_dirs handles those
    if(open_dirs.contains(dirpath))
    {
        logger::sb_log(
            ui->statusbar, u"Attempted to reopen same directory, skipping"_s, LogLevel::Info);
        logger::log(u"Attempted to reopen same directory, skipping"_s, LogLevel::Info);
        return;
    }

    // If not in append mode we only consider one directory open at a time, so clear old paths
    if(!append_mode)
        open_dirs.clear();

    // Extract song metadata using TagLib before replacing/appending to table
    QVector<Song> songs;
    QSet<QString> new_exclusions;
    for(const auto& item : dir)
    {
        // What should we do with symlinks eventually? Resolve them?
        if(item.isSymLink())
            continue;

        // Skip item if it begins with a directory path we already opened
        QString fp        = item.absoluteFilePath();
        bool    skip_flag = false;
        for(const QString& dirname : open_dirs)
        {
            if(fp.startsWith(dirname))
            {
                qDebug() << "fp: " << fp << "begins with dirname: " << dirname;
                skip_flag = true;
                break;
            }
        }
        if(skip_flag)
            continue;

        if(item.isDir())
        {
            // Append a '/' to avoid potential false positives during exclusion
            new_exclusions.insert(fp + "/");
            continue;
        }

        auto song = path_to_song(fp);
        if(song)
            songs.push_back(*song);
    }

    open_dirs.insert(dirpath);
    open_dirs += new_exclusions;

    logger::sb_log(ui->statusbar, QString("Loaded %1 song(s)").arg(songs.size()), LogLevel::Info);

    if(append_mode)
        song_model->add_songs(std::move(songs));
    else
        song_model->set_songs(std::move(songs));
    ui->songViewer->sortByColumn(1, Qt::AscendingOrder);
}

// Save window geometry and app state on app close
void MainWindow::closeEvent(QCloseEvent* event)
{
    QSettings s;
    s.setValue("window/geometry", saveGeometry());
    s.setValue("window/state", saveState());
    QMainWindow::closeEvent(event);
}

void MainWindow::restore_window_settings()
{
    QSettings s;
    if(s.contains("window/geometry"))
        restoreGeometry(s.value("window/geometry").toByteArray());
    if(s.contains("window/state"))
        restoreState(s.value("window/state").toByteArray());
}

// Restore state of check boxes in menubar
void MainWindow::restore_menubar_settings()
{
    QSettings s;
    bool      recursive_checked = s.value(u"files/useRecursiveScan"_s, false).toBool();
    ui->actionToggleRecursiveDirectoryScan->setChecked(recursive_checked);
    bool append_checked = s.value(u"files/useAppendingScan"_s, false).toBool();
    ui->actionToggleAppendDirectoryScan->setChecked(append_checked);
}
