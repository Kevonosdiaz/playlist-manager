#include "logger.h"
#include <QtLogging>

namespace logger
{
    // Log out message to stdout
    void log(const QString& msg, LogLevel level)
    {
        switch(level)
        {
        case LogLevel::Debug:
            qDebug() << level_to_string(level) + msg;
            break;
        case LogLevel::Info:
            qInfo() << level_to_string(level) + msg;
            break;
        case LogLevel::Warn:
            qWarning() << level_to_string(level) + msg;
            break;
        case LogLevel::Critical:
            qCritical() << level_to_string(level) + msg;
            break;
        case LogLevel::Fatal:
            qFatal() << level_to_string(level) + msg;
            break;
        }
    }

    // Log out message to status bar
    void sb_log(QStatusBar* sb, const QString& msg, LogLevel level, int timeout)
    {
        sb->showMessage(level_to_string(level) + msg, timeout);
    }

}
