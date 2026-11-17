#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <QStringList>

inline constexpr int ART_SIZE         = 64;
inline constexpr int MAX_COLUMN_CHARS = 24;

inline const QStringList VALID_FILETYPES{
    "*.mp3",
    "*.ogg",
    "*.oga",
    "*.opus",
    "*.flac",
    "*.m4a",
    "*.mp4",
    "*.aac",
    "*.wav",
    "*.wave",
    "*.wv",
};

inline constexpr QLatin1StringView M3U_FILE_FILTER{"M3U Media Playlist (*.m3u *.m3u8)"};

#endif /* CONSTANTS_H */
