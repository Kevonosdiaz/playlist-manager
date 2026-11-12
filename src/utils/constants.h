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

#endif /* CONSTANTS_H */
