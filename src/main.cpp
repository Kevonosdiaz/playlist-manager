#include "mainwindow.h"

#include <QApplication>
#include <QSettings>

int main(int argc, char* argv[])
{
    using namespace Qt::StringLiterals;
    QApplication::setOrganizationName(u"Kevonosdiaz"_s);
    QApplication::setApplicationName(u"playlist-manager"_s);
    QApplication::setApplicationDisplayName(u"Playlist Manager"_s);
    QApplication a(argc, argv);

    QSettings settings;

    // Set higher global font size
    QFont global_font = QApplication::font();
    global_font.setPointSize(14);
    QApplication::setFont(global_font);
    MainWindow w;
    w.show();
    return QApplication::exec();
}
