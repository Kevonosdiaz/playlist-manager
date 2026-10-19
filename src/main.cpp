#include "mainwindow.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    QApplication::setOrganizationName("Kevonosdiaz");
    QApplication::setApplicationName("playlist-manager");
    QApplication::setApplicationDisplayName("Playlist Manager");

    // Set higher global font size
    QFont global_font = QApplication::font();
    global_font.setPointSize(14);
    QApplication::setFont(global_font);
    MainWindow w;
    w.show();
    return QApplication::exec();
}
