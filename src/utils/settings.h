#ifndef SETTINGS_H
#define SETTINGS_H

#include <QSettings>

namespace Settings
{
    inline constexpr QLatin1StringView w_geometry{"window/geometry"};
    inline constexpr QLatin1StringView w_state{"window/state"};
    inline constexpr QLatin1StringView f_recursive{"files/useRecursiveScan"};
    inline constexpr QLatin1StringView f_appending{"files/useAppendingScan"};
    inline constexpr QLatin1StringView f_explorer_root("files/fileExplorerRoot");
    inline constexpr QLatin1StringView p_playlists("media/playlists");
    inline constexpr QLatin1StringView p_playlist_path("path");

}

#endif /* SETTINGS_H */
