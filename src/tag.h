#ifndef TAG_H
#define TAG_H
#include <QList>
#include <QPixmap>
#include <QStandardItem>
#include <taglib/fileref.h>

// Extract tags for provided song filepath and returns
// list of QStandardItem with columns populated with metadata
QList<QStandardItem*> song_to_item(const QString& filepath);

// Take in path to an audio file and retrieve embedded art
// using TagLib library
// Expects `filepath` to be a valid filepath
// Returns empty QPixmap if null FileRef or no front cover image embedded
QPixmap get_image_data(const TagLib::FileRef& f);

#endif // TAG_H
