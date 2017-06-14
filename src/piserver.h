#ifndef PISERVER_H
#define PISERVER_H

#include "config.h"
#include "json/json.hpp"
#include <string>
#include <set>
#include <map>
#include <ldap.h>

#ifdef LOCALEDIR
#include <libintl.h>
#define _(str)    gettext(str)
#else
#define _(str)    str
#endif


class Host;
class Distribution;
struct FTW;
struct stat;

class PiServer
{
public:
    PiServer();
    virtual ~PiServer();

    /* User management functions */
    void addUser(const std::string &name, const std::string &password, bool forcePasswordChange = false, int gid = PISERVER_GID);
    void addGroup(const std::string &name, const std::string &description, int gid = -1);
    void deleteGroup(const std::string &name);
    void deleteUser(const std::string &name);
    void setUserPassword(const std::string &name, const std::string &password);
    bool isUsernameValid(const std::string &name);
    bool isUsernameAvailable(const std::string &name);
    bool isUsernameAvailableLocally(const std::string &name);
    std::set<std::string> searchUsernames(const std::string &query = "");

    /* Host management functions */
    void addHosts(std::set<std::string> macs, Distribution *d, const std::string &description = "");
    void addHost(Host *h);
    void updateHost(Host *h, bool saveConfigs = true);
    void deleteHost(Host *h);
    Host *getHost(const std::string &mac);
    std::map<std::string,Host *> *getHosts();

    /* OS management functions */
    std::map<std::string,Distribution *> *getDistributions();
    void deleteDistribution(const std::string &name);
    void addDistribution(Distribution *distro);
    void startChrootTerminal(const std::string &distribution);

    std::string currentIP(const std::string &iface = "");
    std::map<std::string,std::string> getInterfaces();
    void updateIP();

    void setSetting(const std::string &key, const std::string &value);
    void setSetting(const std::string &key, int value);
    std::string getSetting(const std::string &key, const std::string &defaultValue = "");
    int getSetting(const std::string &key, int defaultValue);
    void saveSettings();
    void regenDnsmasqConf(bool restartDnsmasqIfChanged = true);
    void addToExports(const std::string &line);
    double availableDiskSpace(const std::string &path = PISERVER_DISTROROOT);
    bool hasArmCpu();

protected:
    std::map<std::string,Host *> _hosts;
    std::map<std::string,Distribution *> _distro;
    LDAP *_ldap;
    nlohmann::json _settings;

    void _saveHosts();
    void _saveDistributions();
    void _patchDistributions();
    void _restartService(const char *name);
    void _connectToLDAP();
    std::string _ldapEscape(const std::string &input);
    int _getHighestUID(const char *objectClass = "posixAccount", const char *attrname = "uidNumber");
    void _ldapAdd(const std::string &dn, const std::multimap<std::string,std::string> &attr, bool modify = false);
    void _ldapModify(const std::string &dn, const std::multimap<std::string,std::string> &attr);
    void _ldapDelete(const std::string &dn);
    static int _unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf);
    std::string _fileGetContents(const std::string &filename);
    void _filePutContents(const std::string &filename, const std::string &contents);
};

#endif // PISERVER_H
