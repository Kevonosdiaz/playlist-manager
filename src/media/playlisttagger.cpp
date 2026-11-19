#include "playlisttagger.h"
#include <utility>

// TODO: We can check whenever rows are deleted in the model to cancel useless work
PlaylistTagger::PlaylistTagger(PlaylistModel* playlist_model, QObject* parent)
    : QObject(parent)
    , model(playlist_model)
{
    connect(&watcher, &QFutureWatcher<Song>::finished, this, &PlaylistTagger::on_finished);
}

PlaylistTagger::~PlaylistTagger()
{
    cancel();
}

// Request playlist to be tagged
// Handles cases such as repeat request, currently tagging playlist, etc.
void PlaylistTagger::request_playlist(const QModelIndex& idx)
{
    if(!idx.isValid())
        return;

    const Playlist& pl = model->at(idx.row());
    if(idx == tagging_row && !watcher.isCanceled())
        next_row = QModelIndex{};
    else if(!pl.tagged())
    {
        // Queue it after current playlist job is finished
        if(tagging_row.isValid())
        {
            next_row = idx;
            watcher.cancel();
        }
        else
        {
            start(idx);
        }
    }
    else
    {
        // Let latest selection win/override
        next_row = QModelIndex{};
    }
}

void PlaylistTagger::cancel()
{
    watcher.cancel();
    next_row = QModelIndex{};
}

void PlaylistTagger::start(const QModelIndex& idx)
{
    const Playlist& pl = model->at(idx.row());
    tagging_row        = idx;
    watcher.setFuture(pl.tag_all_songs());
    emit started();
}

void PlaylistTagger::on_finished()
{
    // If another playlist was opened by user, skip loading this playlist
    QPersistentModelIndex idx = std::exchange(tagging_row, QModelIndex{});
    if(next_row.isValid())
    {
        start(std::exchange(next_row, QModelIndex{}));
        return;
    }
    if(watcher.isCanceled() || !idx.isValid())
        return;

    model->set_songs(idx.row(), watcher.future().results());

    emit playlist_tagged(idx);
}
