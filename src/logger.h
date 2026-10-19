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
    switch(level)
    {
    case LogLevel::Debug:
        return "[Debug] ";
    case LogLevel::Info:
        return "[Info] ";
    case LogLevel::Warn:
        return "[Warning] ";
    case LogLevel::Critical:
        return "[Critical] ";
    default:
        return "[Fatal] ";
    }
}

namespace logger
{
    void log(const QString& msg, LogLevel level = LogLevel::Info);
    void sb_log(QStatusBar* sb, const QString& msg, LogLevel level, int timeout = 2000);
};

#endif /* LOGGER_H */
