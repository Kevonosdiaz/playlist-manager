#ifndef DIRSCAN_H
#define DIRSCAN_H

#include <QDirListing>
#include <QSet>
#include <QStringList>

QStringList scan_dir(const QDirListing& dir, QSet<QString>& open_dirs);

#endif /* DIRSCAN_H */
