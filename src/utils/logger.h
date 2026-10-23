#ifndef LOGGER_H
#define LOGGER_H

#include <QDebug>
#include <QStatusBar>

// Convenience helpers/wrappers around Qt logging to stderr & statusbar
enum class LogLevel
{
    Debug,
    Info,
    Warn,
    Critical,
    Fatal
};

inline QString level_to_string(LogLevel level)
{
    using namespace Qt::StringLiterals;
    switch(level)
    {
    case LogLevel::Debug:
        return u"[Debug] "_s;
    case LogLevel::Info:
        return u"[Info] "_s;
    case LogLevel::Warn:
        return u"[Warning] "_s;
    case LogLevel::Critical:
        return u"[Critical] "_s;
    default:
        return u"[Fatal] "_s;
    }
}

namespace logger
{
    void log(const QString& msg, LogLevel level = LogLevel::Info);
    void sb_log(QStatusBar* sb, const QString& msg, LogLevel level, int timeout = 2000);
};

#endif /* LOGGER_H */
