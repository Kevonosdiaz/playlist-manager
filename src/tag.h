#ifndef TAG_H
#define TAG_H
#include <QList>
#include <QPixmap>
#include <QStandardItem>
#include <taglib/fileref.h>

// Extract tags for provided song filepath and returns
// list of QStandardItem with columns populated with metadata
QList<QStandardItem*> song_to_item(const QString& filepath);

#endif // TAG_H
