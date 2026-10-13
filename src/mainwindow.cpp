#include "mainwindow.h"
#include "tag.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QDirListing>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTableView>
#include <QTreeView>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // Set default splitter ratio for left/right sides
    ui->mainSplitter->setStretchFactor(1, 3);

    connect(
        ui->actionOpenFile, &QAction::triggered, this, &MainWindow::open_playlist_file_selector);
    connect(
        ui->actionOpenDirectory, &QAction::triggered, this, &MainWindow::open_music_dir_selector);
    connect(ui->actionAbout_Qt, &QAction::triggered, this, &MainWindow::open_about_qt);

    // Initialize song viewer QTableView headers
    song_model = new QStandardItemModel(0, 6);
    song_model->setHorizontalHeaderLabels(
        {"Album Art", "Title", "Artist", "Album", "Length", "Track"});

    ui->songViewer->setIconSize(QSize(32, 32));
    ui->songViewer->setModel(song_model);

    // Initialize file explorer to music directory if it exists, otherwise home
    QDir default_dir = QDir(QStandardPaths::writableLocation(QStandardPaths::MusicLocation));
    qDebug() << default_dir;
    if(!default_dir.exists())
    {
        default_dir = QDir::homePath();
    }
    qDebug() << default_dir;
    fs_model = new QFileSystemModel(ui->fileExplorer);
    fs_model->setRootPath(default_dir.absolutePath());
    fs_model->setFilter(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
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
    qDebug() << filename;
}

void MainWindow::open_music_dir_selector()
{
    QString dirpath = QFileDialog::getExistingDirectory(
        this,
        tr("Open Directory"),
        QStandardPaths::writableLocation(QStandardPaths::MusicLocation));

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
        return;

    // Reset model data prior to filling out QFileSystemModel
    if(song_model->rowCount() > 0)
        song_model->removeRows(0, song_model->rowCount());

    // Extract song metadata using TagLib before appending to table
    for(const auto& f : dir)
    {
        auto items = song_to_item(f.absoluteFilePath());
        song_model->appendRow(items);
    }
}
