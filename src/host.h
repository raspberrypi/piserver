#ifndef HOST_H
#define HOST_H

#include <string>

class Distribution;

class Host
{
public:
    Host(const std::string &m, const std::string &desc, Distribution *distr);
    void setDescription(const std::string &descr);
    void setDistro(Distribution *distro);
    std::string tftpPath();

    inline const std::string &mac()
    {
        return _mac;
    }

    inline const std::string &description()
    {
        return _description;
    }

    inline Distribution *distro()
    {
        return _distro;
    }

protected:
    std::string _mac, _description;
    Distribution *_distro;

};

#endif // HOST_H
