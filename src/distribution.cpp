#include "distribution.h"
#include "piserver.h"
#include <algorithm>

Distribution::Distribution(const std::string &name, const std::string &version, const std::string &path)
    : _name(name), _version(version), _path(path)
{
    if (path.empty())
    {
        _path = std::string(PISERVER_DISTROROOT)+"/"+_name;
        if (!_version.empty())
            _path += "-"+_version;

        replace(_path.begin(), _path.end(), ' ', '_');
    }
}

std::string Distribution::distroPath()
{
    return _path;
}

void Distribution::setName(const std::string &name)
{
    _name = name;
}

void Distribution::setVersion(const std::string &version)
{
    _version = version;
}

void Distribution::setLatestVersion(const std::string &latest)
{
    _latestVersion = latest;
}
