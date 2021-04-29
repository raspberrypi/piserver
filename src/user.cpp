#include "user.h"
#include <glibmm.h>

User::User(const std::string &dn, const std::string &name, const std::string &description, std::string lastLoginStr)
    : _dn(dn), _name(name), _description(description), _lastLoginTime(nullptr)
{
    if (lastLoginStr.size() > 14)
    {
        if (lastLoginStr[8] != 'T')
        {
            /* LDAP typically uses dates like 20190607130317Z
               glib expects 20190607T130317Z */
            lastLoginStr.insert(8, 1, 'T');
        }
        _lastLoginTime = g_date_time_new_from_iso8601(lastLoginStr.c_str(), NULL);
    }
}

User::~User()
{
    if (_lastLoginTime)
    {
        g_date_time_unref(_lastLoginTime);
    }
}

const std::string User::lastlogin(const char *format) const
{
    if (!_lastLoginTime)
        return "";

    auto dt = Glib::DateTime(_lastLoginTime);
    return dt.format(format);
}
