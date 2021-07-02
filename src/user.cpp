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

User::User(const User &u)
{
    _dn = u._dn;
    _name = u._name;
    _description = u._description;
    _lastLoginTime = u._lastLoginTime;
    if (_lastLoginTime)
    {
        g_date_time_ref(_lastLoginTime);
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

    gchar *gstr = g_date_time_format(_lastLoginTime, format);
    if (!gstr)
	return "";

    std::string result(gstr);
    g_free(gstr);

    return result;
}
