#ifndef USER_H
#define USER_H

#include <string>
#include <glib.h>

class User
{
public:
    User(const std::string &dn, const std::string &name, const std::string &description, std::string lastLoginStr);

    User(const User &u);

    virtual ~User();

    inline const std::string &name() const
    {
        return _name;
    }

    inline const std::string &dn() const
    {
        return _dn;
    }

    inline const std::string &description() const
    {
        return _description;
    }

    const std::string lastlogin(const char *format = "%Y-%m-%d") const;

protected:
    std::string _dn, _name, _description, _lastLogin;
    GDateTime *_lastLoginTime;
};

#endif // USER_H
