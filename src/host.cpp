#include "host.h"
#include "piserver.h"
#include <algorithm>

Host::Host(const std::string &m, const std::string &desc, Distribution *distr)
    : _mac(m), _description(desc), _distro(distr)
{
}

void Host::setDescription(const std::string &descr)
{
    _description = descr;
}

void Host::setDistro(Distribution *distro)
{
    _distro = distro;
}

std::string Host::tftpPath()
{
    std::string dashedMac = _mac;
    replace(dashedMac.begin(), dashedMac.end(), ':', '-');

    return std::string(PISERVER_TFTPROOT)+"/"+dashedMac;
}
