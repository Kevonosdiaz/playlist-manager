#ifndef PLAYLISTTAGGER_H
#define PLAYLISTTAGGER_H

#include "playlistmodel.h"
#include "song.h"

#include <QFutureWatcher>
#include <QPersistentModelIndex>

class PlaylistTagger : public QObject
{
    Q_OBJECT
public:
    explicit PlaylistTagger(PlaylistModel* playlist_model, QObject* parent = nullptr);
    ~PlaylistTagger();
    void request_playlist(const QModelIndex& idx);
    void cancel();

signals:
    void started();
    void playlist_tagged(const QPersistentModelIndex& idx);

private:
    PlaylistModel*        model;
    QFutureWatcher<Song>  watcher;
    QPersistentModelIndex tagging_row;
    QPersistentModelIndex next_row;

    void start(const QModelIndex& idx);
    void on_finished();
};

#endif /* PLAYLISTTAGGER_H */
