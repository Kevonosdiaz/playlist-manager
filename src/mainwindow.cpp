#include "mainwindow.h"
#include "tag.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QDirListing>
#include <QFileDialog>
#include <QStandardItemModel>
#include <QTableView>

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

    model = new QStandardItemModel(0, 6);
    model->setHorizontalHeaderLabels(
        {"Album Art", "Title", "Artist", "Album", "Length", "Track Number"});
    // for(int row = 0; row < model->rowCount(); ++row)
    // {
    //     for(int column = 0; column < model->columnCount(); ++column)
    //     {
    //         QStandardItem* item
    //             = new QStandardItem(QString("row %0, column %1").arg(row).arg(column));
    //         model->setItem(row, column, item);
    //     }
    // }

    ui->songViewer->setIconSize(QSize(32, 32));
    ui->songViewer->setModel(model);
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
