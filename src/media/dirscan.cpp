#include "dirscan.h"

QStringList scan_dir(const QDirListing& dir, QSet<QString>& open_dirs)
{
    QStringList   song_paths;
    QSet<QString> new_exclusions;
    for(const auto& item : dir)
    {
        // What should we do with symlinks eventually? Resolve them?
        if(item.isSymLink())
            continue;

        // Skip item if it begins with a directory path we already opened
        QString fp        = item.absoluteFilePath();
        bool    skip_flag = false;
        for(const QString& dirname : std::as_const(open_dirs))
        {
            if(fp.startsWith(dirname))
            {
                skip_flag = true;
                break;
            }
        }
        if(skip_flag)
            continue;

        if(item.isDir())
        {
            // Append a '/' to avoid potential false positives during exclusion
            new_exclusions.insert(fp + "/");
            continue;
        }

        song_paths.push_back(std::move(fp));
    }

    open_dirs += new_exclusions;
    return song_paths;
}
