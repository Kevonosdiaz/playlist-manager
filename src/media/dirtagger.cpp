#include "dirtagger.h"
#include "../utils/constants.h"
#include "dirscan.h"
#include "tag.h"

#include <QDirListing>
#include <QtConcurrentMap>

DirTagger::DirTagger(SongTableModel* song_model, QObject* parent)
    : QObject(parent)
    , model(song_model)
{
    connect(
        &watcher, &QFutureWatcher<std::optional<Song>>::finished, this, &DirTagger::on_finished);
    connect(&watcher,
            &QFutureWatcher<std::optional<Song>>::resultsReadyAt,
            this,
            &DirTagger::add_song_batch);
}

DirTagger::~DirTagger()
{
    cancel();
}

bool DirTagger::request_dir(const QString& dirpath,
                            const bool     recursive_mode,
                            const bool     append_mode)
{
    // If recursive mode is on, add flag to scan subdirs for songs
    using F                               = QDirListing::IteratorFlag;
    QDirListing::IteratorFlags iter_flags = F::ExcludeOther;
    if(recursive_mode)
        iter_flags |= F::Recursive;

    QDirListing dir(dirpath, VALID_FILETYPES, iter_flags);

    // Exit if empty
    if(dir.begin() == dir.end())
    {
        emit empty_dir();
        return false;
    }

    // Skip reopening if already opened/scanned dir
    // Note: does not prevent recursive scan from finding repeats; open_dirs handles those
    if(open_dirs.contains(dirpath + "/"))
    {
        emit repeat_dir();
        return false;
    }

    // If not in append mode we only consider one directory open at a time, so clear old paths from in-progress scans/open_dirs
    // Also clear is open_dirs.empty() to catch playlist -> dir transition in append mode
    if(!append_mode || open_dirs.empty())
    {
        pending_tag_scans.clear();
        watcher.cancel();
        open_dirs.clear();
        emit clear_songviewer();
        prev_song_count = 0;
    }

    // Extract song metadata using TagLib before replacing/appending to table
    QStringList song_paths = scan_dir(dir, open_dirs);

    // Also exit here if nothing to do, if skipped within scan_dir itself
    if(song_paths.empty())
    {
        emit repeat_dir();
        return false;
    }
    open_dirs.insert(dirpath + "/");

    // Enqueue current dir if another dir is currently being tagged, else tag immediately
    if(append_mode && watcher.isRunning())
        pending_tag_scans.enqueue(std::move(song_paths));
    else
    {
        prev_song_count = model->rowCount();
        start(std::move(song_paths));
    }

    return true;
}

void DirTagger::cancel()
{
    open_dirs.clear();
    pending_tag_scans.clear();
    watcher.cancel();
    prev_song_count = 0;
}

// To be called by toggling recursive/appending mode
void DirTagger::clear_open_dirs()
{
    open_dirs.clear();
}

void DirTagger::start(QStringList paths)
{
    watcher.setFuture(QtConcurrent::mapped(std::move(paths), path_to_song));
    emit started();
}

void DirTagger::add_song_batch(int begin, int end)
{
    QVector<Song> batch;
    for(int i = begin; i < end; i++)
    {
        if(auto s = watcher.resultAt(i))
            batch.push_back(std::move(*s));
    }
    model->add_songs(std::move(batch));
}

void DirTagger::on_finished()
{
    // Run next directory (if added with append mode)
    if(!pending_tag_scans.empty())
    {
        start(pending_tag_scans.dequeue());
        return;
    }

    if(watcher.isCanceled())
        return;

    emit dir_tagged(model->rowCount() - prev_song_count);
}
