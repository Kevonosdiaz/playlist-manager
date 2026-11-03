#ifndef TAG_H
#define TAG_H
#include "song.h"
#include <optional>

// Extract tags for provided song filepath and returns Song struct
std::optional<Song> path_to_song(const QString& filepath);

#endif // TAG_H
