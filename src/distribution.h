#ifndef DISTRIBUTION_H
#define DISTRIBUTION_H

#include <string>

class Distribution
{
public:
    Distribution(const std::string &name, const std::string &version, const std::string &path = "");
    std::string distroPath();
    void setName(const std::string &name);
    void setVersion(const std::string &name);
    void setLatestVersion(const std::string &latest);

    inline const std::string &name()
    {
        return _name;
    }

    inline const std::string &version()
    {
        return _version;
    }

    inline const std::string &latestVersion()
    {
        return _latestVersion;
    }


protected:
    std::string _name, _version, _latestVersion, _path;
};

#endif // DISTRIBUTION_H
