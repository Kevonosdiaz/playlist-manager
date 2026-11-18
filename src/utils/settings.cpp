#include "settings.h"

namespace Settings
{
    bool get_recursive_mode()
    {
        QSettings s;
        return s.value(Settings::f_recursive, false).toBool();
    }

    bool get_append_mode()
    {
        QSettings s;
        return s.value(Settings::f_appending, false).toBool();
    }

    void set_recursive_mode(bool new_val)
    {
        QSettings s;
        s.setValue(Settings::f_recursive, new_val);
    }
    void set_append_mode(bool new_val)
    {
        QSettings s;
        s.setValue(Settings::f_appending, new_val);
    }
}
