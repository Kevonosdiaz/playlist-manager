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

    // Restore font size setting or default to 14pt
    QSettings settings;
    QFont     global_font = QApplication::font();
    global_font.setPointSize(settings.value("fontSize", 14).toInt());
    QApplication::setFont(global_font);
    MainWindow w;
    w.show();
    return QApplication::exec();
}
