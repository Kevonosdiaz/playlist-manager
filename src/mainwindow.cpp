#include "mainwindow.h"
#include "logger.h"
#include "tag.h"
#include "ui_mainwindow.h"
#include "util.h"

#include <QDirListing>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QHeaderView>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTableView>
#include <QTreeView>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // Set default splitter sizes for left/right sides
    ui->mainSplitter->setSizes({400, 784});

    connect(
        ui->actionOpenFile, &QAction::triggered, this, &MainWindow::open_playlist_file_selector);
    connect(
        ui->actionOpenDirectory, &QAction::triggered, this, &MainWindow::open_music_dir_selector);
    connect(ui->actionAbout_Qt, &QAction::triggered, this, &MainWindow::open_about_qt);

    // Initialize song viewer QTableView headers
    // Empty last element to allow blank space to be stretched to occupy empty space
    song_model = new QStandardItemModel(0, 7, this);
    song_model->setHorizontalHeaderLabels(
        {"Album Art", "Title", "Artist", "Album", "Length", "Track", ""});

    ui->songViewer->setIconSize(QSize(ART_SIZE, ART_SIZE));
    ui->songViewer->setModel(song_model);

    // Fix row height based on album art size and resize columns dynamically
    ui->songViewer->verticalHeader()->setDefaultSectionSize(ART_SIZE + 8);
    ui->songViewer->horizontalHeader()->setMinimumSectionSize(30);
    ui->songViewer->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->songViewer->horizontalHeader()->setStretchLastSection(true);

    // Enable sorting by column
    ui->songViewer->setSortingEnabled(true);
    ui->songViewer->horizontalHeader()->setSortIndicatorShown(true);

    // Initialize file explorer to music directory if it exists, otherwise home
    QDir default_dir = QDir(QStandardPaths::writableLocation(QStandardPaths::MusicLocation));
    if(!default_dir.exists())
    {
        default_dir = QDir::homePath();
    }
    fs_model = new QFileSystemModel(ui->fileExplorer);
    fs_model->setRootPath(default_dir.absolutePath());
    fs_model->setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    ui->fileExplorer->setModel(fs_model);
    ui->fileExplorer->setRootIndex(fs_model->index(default_dir.absolutePath()));

    // Hide header and hide all columns except filename column
    ui->fileExplorer->setHeaderHidden(true);
    ui->fileExplorer->setColumnHidden(1, true);
    ui->fileExplorer->setColumnHidden(2, true);
    ui->fileExplorer->setColumnHidden(3, true);

    // On single click file explorer item: open dir in song viewer
    connect(ui->fileExplorer, &QTreeView::clicked, this, &MainWindow::open_explorer_dir);
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
        this, "Open Playlist File", "", "M3U Media Playlist (*.m3u *.m3u8)");
    logger::log(filename, LogLevel::Debug);
}

void MainWindow::open_music_dir_selector()
{
    QString dirpath = QFileDialog::getExistingDirectory(
        this, "Open Directory", QStandardPaths::writableLocation(QStandardPaths::MusicLocation));

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

void MainWindow::load_dir_to_songviewer(const QString& dirpath)
{
    QDirListing dir(dirpath, VALID_FILETYPES, QDirListing::IteratorFlag::FilesOnly);

    // Exit if empty
    if(dir.begin() == dir.end())
    {
        logger::sb_log(ui->statusbar, "No files found in directory, skipping", LogLevel::Info);
        logger::log("No files found in directory, skipping", LogLevel::Info);
        return;
    }

    // Reset model data prior to filling out QFileSystemModel
    if(song_model->rowCount() > 0)
        song_model->removeRows(0, song_model->rowCount());

    // Extract song metadata using TagLib before appending to table
    int count = 0;
    for(const auto& f : dir)
    {
        auto items = song_to_item(f.absoluteFilePath());
        if(!items.empty())
        {
            song_model->appendRow(items);
            count++;
        }
    }
    ui->songViewer->sortByColumn(1, Qt::AscendingOrder);
    logger::sb_log(ui->statusbar, QString("Loaded %1 songs").arg(count), LogLevel::Info);
}
