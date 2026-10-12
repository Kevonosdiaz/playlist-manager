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
    model = new QStandardItemModel(0, 6);
    model->setHorizontalHeaderLabels(
        {"Album Art", "Title", "Artist", "Album", "Length", "Track Number"});

    ui->songViewer->setIconSize(QSize(32, 32));
    ui->songViewer->setModel(model);

    // Initialize file explorer to music directory if it exists, otherwise home
    QDir default_dir = QDir(QStandardPaths::writableLocation(QStandardPaths::MusicLocation));
    qDebug() << default_dir;
    if(!default_dir.exists())
    {
        default_dir = QDir::homePath();
    }
    qDebug() << default_dir;
    auto* filesystem_model = new QFileSystemModel(ui->fileExplorer);
    filesystem_model->setRootPath(default_dir.absolutePath());
    filesystem_model->setFilter(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
    ui->fileExplorer->setModel(filesystem_model);
    ui->fileExplorer->setRootIndex(filesystem_model->index(default_dir.absolutePath()));

    // Hide header and hide all columns except filename column
    ui->fileExplorer->setHeaderHidden(true);
    ui->fileExplorer->setColumnHidden(1, true);
    ui->fileExplorer->setColumnHidden(2, true);
    ui->fileExplorer->setColumnHidden(3, true);
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
    QString dirpath = QFileDialog::getExistingDirectory(this, tr("Open Directory"), "/home/Music");
    QDirListing dir(dirpath, VALID_FILETYPES, QDirListing::IteratorFlag::FilesOnly);

    // Reset model data prior to filling out
    if(model->rowCount() > 0)
        model->removeRows(0, model->rowCount());

    // Extract song metadata using TagLib before appending to table
    for(const auto& f : dir)
    {
        auto items = song_to_item(f.absoluteFilePath());
        model->appendRow(items);
    }
}
