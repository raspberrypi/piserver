#include "user.h"
#include <glibmm.h>

User::User(const std::string &dn, const std::string &name, const std::string &description, std::string lastLoginStr)
    : _dn(dn), _name(name), _description(description)
{
    _lastLoginTime.tv_sec = 0;

    if (lastLoginStr.size() > 14)
    {
        if (lastLoginStr[8] != 'T')
        {
            /* LDAP typically uses dates like 20190607130317Z
               glib expects 20190607T130317Z */
            lastLoginStr.insert(8, 1, 'T');
        }
        g_time_val_from_iso8601(lastLoginStr.c_str(), &_lastLoginTime);
    }
}

const std::string User::lastlogin(const char *format) const
{
    if (!_lastLoginTime.tv_sec)
        return "";

    auto dt = Glib::DateTime::create_now_local(_lastLoginTime);
    return dt.format(format);
}
