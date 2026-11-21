#ifndef DIRTAGGER_H
#define DIRTAGGER_H

#include "song.h"
#include "songtablemodel.h"

#include <QFutureWatcher>
#include <QList>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QStringList>

#include <optional>

class DirTagger : public QObject
{
    Q_OBJECT
public:
    explicit DirTagger(SongTableModel* song_model, QObject* parent = nullptr);
    ~DirTagger();
    bool request_dir(const QString& dirpath, const bool recursive_mode, const bool append_mode);
    void cancel();
    void clear_open_dirs();

signals:
    void started();
    void empty_dir();
    void repeat_dir();
    void clear_songviewer();
    void dir_tagged(const int num_songs);

private:
    SongTableModel*                     model;
    QFutureWatcher<std::optional<Song>> watcher;
    int                                 prev_song_count = 0;

    // Track open dirs in song viewer (or dirs in append mode)
    QSet<QString>       open_dirs;
    QQueue<QStringList> pending_tag_scans;

    void start(QStringList paths);
    void add_song_batch(int begin, int end);
    void on_finished();
};

#endif /* DIRTAGGER_H */
