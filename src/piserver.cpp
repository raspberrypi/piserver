#include "piserver.h"
#include "distribution.h"
#include "host.h"
#include <fstream>
#include <exception>
#include <stdexcept>
#include <regex>
#include <unistd.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <glib.h> // g_spawn_async()
#include <sys/types.h>
#include <pwd.h>
#include <crypt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <ftw.h>
#include <sys/utsname.h>
#include <giomm.h>
#include <fcntl.h>
#include <sys/capability.h>

using namespace std;
using nlohmann::json;

PiServer::PiServer()
    : _ldap(NULL), _settings({})
{
#ifdef LOCALEDIR
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    /* Parse installed_distros.json if it exists */
    if (::access(PISERVER_DISTROFILE, R_OK) != -1)
    {
        try
        {
            std::ifstream i(PISERVER_DISTROFILE);
            json j;
            i >> j;

            for (auto &entry : j)
            {
                string name = entry.at("name").get<string>();
                string version = entry.at("version").get<string>();
                string path;
                if (entry.count("path"))
                    path = entry.at("path").get<string>();
                _distro[name] = new Distribution(name, version, path);
            }
        }
        catch (exception &e)
        {
            cerr << "Error parsing installed_distros.json: " << e.what() << endl;
        }
    }

    /* Parse hosts.json if it exists */
    if (::access(PISERVER_HOSTSFILE, R_OK) != -1)
    {
        try
        {
            ifstream i(PISERVER_HOSTSFILE);
            json j;
            i >> j;

            for (auto &entry : j)
            {
                string mac   = entry.at("mac").get<string>();
                string descr = entry.at("description").get<string>();
                string distroname = entry.at("distro").get<string>();
                Distribution *d = nullptr;

                try
                {
                    d = _distro.at(distroname);
                } catch (out_of_range) { }

                _hosts[mac] = new Host(mac, descr, d);
            }
        }
        catch (exception &e)
        {
            cerr << "Error parsing hosts.json: " << e.what() << endl;
        }
    }

    if (::access(PISERVER_SETTINGSFILE, R_OK) != -1)
    {
        try
        {
            ifstream i(PISERVER_SETTINGSFILE);
            i >> _settings;
        }
        catch (exception &e)
        {
            cerr << "Error parsing settings.json: " << e.what() << endl;
        }
    }
}

PiServer::~PiServer()
{
    for (auto &kv : _hosts)
        delete kv.second;
    for (auto &kv : _distro)
        delete kv.second;

    if (_ldap)
    {
        ldap_unbind_ext(_ldap, NULL, NULL);
    }
}

/* User management functions */
void PiServer::addUser(const std::string &name, const std::string &password, bool forcePasswordChange, int gid)
{
    if (!isUsernameValid(name)) /* Should not happen. UI should check earlier */
        throw runtime_error("Username not well-formed");

    int uid = max(_getHighestUID()+1, PISERVER_MIN_UID);

    string dn = "cn="+name+","+_ldapDN();
    string homedir = string(PISERVER_HOMEROOT)+"/"+name;
    multimap<string,string> attr;
    struct crypt_data cd;

    attr.insert(make_pair("uid", name));
    attr.insert(make_pair("cn", name));
    attr.insert(make_pair("sn", name));
    attr.insert(make_pair("objectClass", "inetOrgPerson"));
    attr.insert(make_pair("objectClass", "posixAccount"));
    attr.insert(make_pair("objectClass", "shadowAccount"));
    attr.insert(make_pair("uidNumber", to_string(uid)));
    attr.insert(make_pair("gidNumber", to_string(gid)));
    attr.insert(make_pair("homeDirectory", homedir));
    attr.insert(make_pair("loginShell", string(PISERVER_SHELL)));
    attr.insert(make_pair("userPassword", string("{CRYPT}")+string(crypt_r(password.c_str(), "$5$", &cd))));
    if (forcePasswordChange)
    {
        attr.insert(make_pair("shadowLastChange", "0"));
        attr.insert(make_pair("shadowMax", "10000"));
    }
    _ldapAdd(dn, attr);

    if (::access("/usr/share/pam-configs/mkhomedir-piserver", F_OK) == -1)
    {
        /* Only create home directory manually if not using mkhomedir */
        if (::mkdir(homedir.c_str(), 0700) == -1)
            throw runtime_error("Error creating home directory "+homedir+": "+ strerror(errno));
        if (::chown(homedir.c_str(), uid, gid) == -1)
            throw runtime_error("Error setting ownership for "+homedir+": "+ strerror(errno));
    }
}

void PiServer::addGroup(const std::string &name, const std::string &description, int gid)
{
    if (!isUsernameValid(name))
        throw runtime_error("Group name not well-formed");
    if (gid == -1)
        gid =  max(_getHighestUID("posixGroup", "gidNumber")+1, PISERVER_MIN_UID);

    string dn = "cn="+name+",ou=groups,"+_ldapDN();
    multimap<string,string> attr = {
        {"gidNumber", to_string(gid)},
        {"description", description},
        {"objectClass", "posixGroup"},
        {"objectClass", "top"}
    };
    _connectToLDAP();
    _ldapAdd(dn, attr);
}

void PiServer::deleteGroup(const std::string &name)
{
    string dn = "cn="+name+",ou=groups,"+_ldapDN();

    if (!isUsernameValid(name))
        throw runtime_error("Group name not well-formed");

    _connectToLDAP();
    _ldapDelete(dn);
}

int PiServer::_unlink_cb(const char *fpath, const struct stat *, int, struct FTW *)
{
    int rv = remove(fpath);

    if (rv)
        perror(fpath);

    return rv;
}

void PiServer::deleteUser(const std::string &dn, const string &name)
{
    string homedir = string(PISERVER_HOMEROOT)+"/"+name;

    if (!isUsernameValid(name))
        throw runtime_error("Username not well-formed");

    _connectToLDAP();
    _ldapDelete(dn);
    /* Recursively delete contents of home directory if it exists */
    if (::access(homedir.c_str(), F_OK) != -1)
    {
        if (::nftw(homedir.c_str(), _unlink_cb, 128, FTW_DEPTH | FTW_PHYS) != 0)
            throw runtime_error("Error deleting home directory");
    }
}

void PiServer::updateUser(const std::string &dn, std::multimap<std::string,std::string> &attr)
{
    struct crypt_data cd;

    _connectToLDAP();

    auto it = attr.find("password");
    if (it != attr.end())
    {
        string password = it->second;
        attr.insert(make_pair("userPassword", string("{CRYPT}")+string(crypt_r(password.c_str(), "$5$", &cd))));
        attr.erase(it);
    }
    _ldapModify(dn, attr);
}

bool PiServer::isUsernameValid(const std::string &name)
{
    if (name.empty())
        return false;

    /* POSIX.1-2008: http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap03.html#tag_03_431
     * does not allow starting with dash. Also do not allow starting with period, as it creates hidden files */
    if (name[0] == '-' || name[0] == '.')
        return false;

    for (char c : name)
    {
        /* To be portable on every file system only alphanumeric characters . _ - are allowed */
        if (!isalnum(c) && c != '.' && c != '_' && c != '-')
            return false;
    }

    return true;
}

bool PiServer::isUsernameAvailable(const std::string &name)
{
    bool avail;
    int rc;
    LDAPMessage  *result = NULL;
    string filter = "(&(objectClass=posixAccount)(cn="+_ldapEscape(name)+"))";
    string ldapDN = _ldapDN();

    _connectToLDAP();
    rc = ldap_search_ext_s(_ldap, ldapDN.c_str(), LDAP_SCOPE_SUBTREE,
                      filter.c_str(), NULL, 0, NULL, NULL, LDAP_NO_LIMIT, LDAP_NO_LIMIT, &result);
    if (rc != LDAP_SUCCESS)
    {
        if (result)
            ldap_msgfree(result);

        throw runtime_error("Error searching LDAP server for users: "+string(ldap_err2string(rc)));
    }

    avail = ldap_count_entries(_ldap, result) == 0;

    if (result)
        ldap_msgfree(result);

    return avail;
}

bool PiServer::isUsernameAvailableLocally(const std::string &name)
{
    return getpwnam(name.c_str()) == NULL;
}

std::map<std::string,User> PiServer::searchUsernames(const std::string &query)
{
    std::map<string,User> users;
    LDAPMessage  *result = NULL, *entry = NULL;
    int rc;
    struct berval **values;
    string ldapDN = _ldapDN();
    string uidfield = _uidField();
    string filter;
    filter = getLdapFilter(getSetting("ldapGroup"));
    const char *attrfilter[] = { uidfield.c_str(), "description", "authtimestamp", NULL};

    if (!query.empty())
        filter = "(&"+filter+"("+uidfield+"=*"+_ldapEscape(query)+"*))";

    _connectToLDAP();

    rc = ldap_search_ext_s(_ldap, ldapDN.c_str(), LDAP_SCOPE_SUBTREE,
                      filter.c_str(), (char **) attrfilter, 0, NULL, NULL, LDAP_NO_LIMIT, LDAP_NO_LIMIT, &result);
    if (rc != LDAP_SUCCESS)
    {
        if (result)
            ldap_msgfree(result);

        throw runtime_error("Error searching LDAP server for users: "+string(ldap_err2string(rc)));
    }

    for (entry = ldap_first_entry(_ldap, result); entry != NULL; entry = ldap_next_entry(_ldap, entry))
    {
        string dn, name, description, lastLogin;

        char *dn_cstr = ldap_get_dn(_ldap, entry);
        dn = dn_cstr;
        ldap_memfree(dn_cstr);

        values = ldap_get_values_len(_ldap, entry, uidfield.c_str() );
        if (values)
        {
            if (values[0])
                name = string(values[0]->bv_val, values[0]->bv_len);
            ldap_value_free_len(values);
        }
        values = ldap_get_values_len(_ldap, entry, "description");
        if (values)
        {
            if (values[0])
                description = string(values[0]->bv_val, values[0]->bv_len);
            ldap_value_free_len(values);
        }
        values = ldap_get_values_len(_ldap, entry, "authtimestamp");
        if (values)
        {
            if (values[0])
                lastLogin = string(values[0]->bv_val, values[0]->bv_len);
            ldap_value_free_len(values);
        }

        users.insert( make_pair(name, User(dn, name, description, lastLogin)) );
    }

    if (result)
        ldap_msgfree(result);

    return users;
}

/* Host management functions */
void PiServer::addHost(Host *h)
{
    _hosts[h->mac()] = h;
    updateHost(h);
}

void PiServer::addHosts(std::set<std::string> macs, Distribution *d, const std::string &description)
{
    for (string mac : macs)
    {
        Host *h = new Host(mac, description, d);
        _hosts[mac] = h;
        updateHost(h, false);
    }

    _saveHosts();
}

void PiServer::deleteHost(Host *h)
{
    _hosts.erase(h->mac());

    /* Remove tftproot symlink */
    string link = h->tftpPath();
    if (::unlink(link.c_str()) == -1) { }

    _saveHosts();
    delete h;
}

Host *PiServer::getHost(const std::string &mac)
{
    return _hosts.at(mac);
}

std::map<std::string,Host *> *PiServer::getHosts()
{
    return &_hosts;
}

void PiServer::updateHost(Host *h, bool saveConfigs)
{
    /* Set tftproot symlink from host's MAC address to distribution's boot folder */
    string link = h->tftpPath();
    if (::unlink(link.c_str()) == -1) { }

    if (h->distro())
    {
        string bootfolder = h->distro()->distroPath()+"/boot";
        if (::symlink(bootfolder.c_str(), link.c_str()) == -1)
        {
                cerr << "Error creating tftproot symlink " << link
                     << " to " << bootfolder << ": " << strerror(errno) << endl;
        }
    }

    if (saveConfigs)
        _saveHosts();
}

void PiServer::_saveHosts()
{
    json j = json::array();
    ofstream o(PISERVER_HOSTSFILE);

    for (auto &kv : _hosts)
    {
        Host *h = kv.second;
        json jh;
        string distroname;

        if (h->distro())
            distroname = h->distro()->name();

        jh["mac"] = h->mac();
        jh["description"] = h->description();
        jh["distro"] = distroname;
        j.push_back(jh);
    }

    o << std::setw(4) << j << std::endl;

    regenDnsmasqConf();
}

void PiServer::_saveDistributions()
{
    json j = json::array();
    ofstream o(PISERVER_DISTROFILE);

    for (auto &kv : _distro)
    {
        Distribution *d = kv.second;
        json jd;

        jd["name"] = d->name();
        jd["version"] = d->version();
        jd["path"] = d->distroPath();
        j.push_back(jd);
    }

    o << std::setw(4) << j << std::endl;
}

void PiServer::regenDnsmasqConf(bool restartDnsmasqIfChanged)
{
    //ofstream fs(PISERVER_DNSMASQCONFFILE, ios_base::out | ios_base::trunc);
    stringstream fs;
    fs << "# This is an auto-generated file. DO NOT EDIT" << endl << endl
       << "bind-dynamic" << endl
       << "log-dhcp" << endl
       << "enable-tftp" << endl
       << "tftp-root="+string(PISERVER_TFTPROOT) << endl
       << "tftp-unique-root=mac" << endl
       << "local-service" << endl
       << "host-record=piserver,"+currentIP() << endl;
    if ( getSetting("standaloneDhcpServer", 0) )
    {
        fs << "dhcp-range=tag:piserver,"+getSetting("startIP")+","+getSetting("endIP")+","+getSetting("netmask") << endl;
        string gw = getSetting("gateway");
        if (!gw.empty())
            fs << "dhcp-option=tag:piserver,option:router,"+gw << endl;
    }
    else
    {
        fs << "dhcp-range=tag:piserver,"+currentIP()+",proxy" << endl;
    }
    fs << "pxe-service=tag:piserver,0,\"Raspberry Pi Boot\"" << endl;

    for (auto &kv : _hosts)
    {
        if (kv.second->distro())
        {
            fs << "dhcp-host="+kv.second->mac()+",set:piserver" << endl;
        }
    }

    /* Only save and restart if something actually changed */
    if (restartDnsmasqIfChanged && _fileGetContents(PISERVER_DNSMASQCONFFILE) != fs.str())
    {
        _filePutContents(PISERVER_DNSMASQCONFFILE, fs.str());
        _restartService("dnsmasq");
    }
}

void PiServer::addToExports(const std::string &line)
{
    string exports = _fileGetContents("/etc/exports");
    if (exports.find(line) == string::npos)
    {
        if (!exports.empty() && exports.back() != '\n')
            exports += "\n";
        exports += line+"\n";
        _filePutContents("/etc/exports", exports);
        _restartService("nfs-kernel-server");
    }
}

void PiServer::_patchDistributions()
{
    regex nfsrootRegex("(nfsroot=[^ ]* ?)");
    regex rootRegex("(root=[^ ]* ?)");
    regex ipRegex("(ip=[^ ]* ?)");

    for (auto &kv : _distro)
    {
        string cmdlinefile = kv.second->distroPath()+"/boot/cmdline.txt";
        string resolvfile = kv.second->distroPath()+"/etc/resolv.conf";

        if (::access(cmdlinefile.c_str(), R_OK) != -1)
        {
            string cmdlinetxt = _fileGetContents(cmdlinefile);
            string origCmdlinetxt = cmdlinetxt;
            string newOptions = " ip=dhcp root=/dev/nfs nfsroot="+currentIP()+":"+kv.second->distroPath()+",v3";

            /* Remove existing parameters from cmdline.txt */
            cmdlinetxt = regex_replace(cmdlinetxt, nfsrootRegex, "");
            cmdlinetxt = regex_replace(cmdlinetxt, rootRegex, "");
            cmdlinetxt = regex_replace(cmdlinetxt, ipRegex, "");

            /* rtrim string */
            size_t endpos = cmdlinetxt.find_last_not_of(" \n\r\t");
            if(endpos != string::npos)
                cmdlinetxt = cmdlinetxt.substr(0, endpos+1);

            /* Add new ones, and save to disk if the result is different than what was in the file before */
            cmdlinetxt += newOptions;
            if (cmdlinetxt != origCmdlinetxt)
                _filePutContents(cmdlinefile, cmdlinetxt);
        }

        string newResolv = "# Auto-genererated by PiServer - Do not change\n"
                "nameserver "+currentIP()+"\n";
        if ( _fileGetContents(resolvfile) != newResolv )
        {
            /* Unlink to get rid of symlink if any */
            if (::unlink(resolvfile.c_str()) == -1) { }
            _filePutContents(resolvfile, newResolv);
        }
    }
}

void PiServer::updateIP()
{
    if (currentIP().empty())
        return;

    _patchDistributions();
    regenDnsmasqConf();
}

/* OS management functions */
std::map<std::string,Distribution *> *PiServer::getDistributions()
{
    return &_distro;
}

void PiServer::deleteDistribution(const std::string &name)
{
    Distribution *d = _distro.at(name);
    int inuseby = 0;

    for (auto &kv : _hosts)
    {
        if (kv.second->distro() == d)
            inuseby++;
    }

    if (inuseby > 0)
        throw runtime_error(_("Cannot delete this OS, as it is in use by ")+to_string(inuseby)+_(" hosts"));

    if (::nftw(d->distroPath().c_str(), _unlink_cb, 128, FTW_DEPTH | FTW_PHYS) != 0)
        throw runtime_error("Error deleting OS files from disk"); /* Should not happen */

    _distro.erase(name);
    _saveDistributions();
}

void PiServer::addDistribution(Distribution *distro)
{
    string name = distro->name();

    if ( _distro.count(name) && _distro.at(name)->version() != distro->version() )
    {
        /* Concerns an upgrade of an existing OS. Swap contents of old and new distro objects */
        Distribution *oldDistro = _distro.at(name);
        string oldVersion = oldDistro->version();
        name = string(_("Old"))+" "+oldVersion+" "+name;
        distro->setName(name);
        oldDistro->setVersion( distro->version() );
        distro->setVersion( oldVersion );

        for (auto &kv : _hosts)
        {
            if (kv.second->distro() == oldDistro)
            {
                updateHost(kv.second, false);
            }
        }
    }

    _distro[name] = distro;
    _saveDistributions();
    _patchDistributions();
}

std::string PiServer::currentIP(const std::string &iface)
{
    string result;

    try
    {
        if (iface.length())
            result = getInterfaces().at(iface);
        else
            result = getInterfaces().at(getSetting("interface"));
    }
    catch (out_of_range) { }

    return result;
}

std::map<std::string,std::string> PiServer::getInterfaces()
{
    map<string,string> res;
    struct ifaddrs *ifaddr, *ifa;
    char host[NI_MAXHOST];

    if (getifaddrs(&ifaddr) == 0)
    {
        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET)
                continue;

            if (getnameinfo(ifa->ifa_addr,sizeof(struct sockaddr_in),host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST) == 0)
            {
                res[ifa->ifa_name] = host;
            }
        }

        freeifaddrs(ifaddr);
    }

    return res;
}

void PiServer::_restartService(const char *name)
{
    GError *error = NULL;
    const gchar *cmd[] = {"/bin/systemctl", "restart", name, NULL};

    if (!g_spawn_async(NULL, (gchar **) cmd, NULL, G_SPAWN_DEFAULT, NULL, NULL, NULL, &error))
    {
        g_warning("Error trying to restart service %s: %s", name, error->message);
    }
}

/* Prevent that chroot'ed applications set filesystem capabilities (as they are not supported by nfs) */
void PiServer::_dropFilesystemCapabilities(gpointer)
{
    if (!CAP_IS_SUPPORTED(CAP_SETFCAP))
        return;

    cap_t caps = cap_get_proc();
    if (!caps)
        return;

    /* Need to grant CAP_SETPCAP to self, before being able to drop bounds */
    const cap_value_t setpcap_arr[1] = { CAP_SETPCAP };
    cap_set_flag(caps, CAP_EFFECTIVE, 1, setpcap_arr, CAP_SET);
    cap_set_proc(caps);

    cap_drop_bound(CAP_SETFCAP);

    cap_set_flag(caps, CAP_EFFECTIVE, 1, setpcap_arr, CAP_CLEAR);
    cap_set_proc(caps);
    cap_free(caps);
}

void PiServer::startChrootTerminal(const std::string &distribution)
{
    GError *error = NULL;
    string distroPath = _distro.at(distribution)->distroPath();
    string cmdline1 = string (PISERVER_DATADIR "/scripts/chroot_image.sh");
    string cmdline2 = distroPath;
    const gchar *cmd[] = {"/usr/bin/x-terminal-emulator", "-e", cmdline1.c_str(), cmdline2.c_str(), NULL};

    if (!g_spawn_async(NULL, (gchar **) cmd, NULL, G_SPAWN_DEFAULT, &PiServer::_dropFilesystemCapabilities, NULL, NULL, &error))
    {
        g_warning("Error trying to start terminal: %s", error->message);
    }
}

bool PiServer::hasArmCpu()
{
    struct utsname u;

    if (uname(&u) == 0)
    {
        return (strncmp(u.machine, "arm", 3) == 0);
    }

    return false;
}

void PiServer::_connectToLDAP()
{
    if (!_ldap)
    {
        struct berval cred;

        string ldapbind = getSetting("ldapUser", PISERVER_LDAP_USER);
        string ldappass = getSetting("ldapPassword");
        string ldapuri  = getSetting("ldapUri", PISERVER_LDAP_URL);

        int version = LDAP_VERSION3;
        int rc = ldap_initialize(&_ldap, ldapuri.c_str() );

        if (rc != LDAP_SUCCESS)
        {
            throw runtime_error("Error initializing LDAP connecting: "+string(ldap_err2string(rc)));
        }

        ldap_set_option(_ldap, LDAP_OPT_PROTOCOL_VERSION, &version);
        ldap_set_option(_ldap, LDAP_OPT_REFERRALS, LDAP_OPT_OFF);

        cred.bv_val = (char *) ldappass.c_str();
        cred.bv_len = ldappass.length();
        rc = ldap_sasl_bind_s(_ldap, ldapbind.c_str(), LDAP_SASL_SIMPLE, &cred, NULL, NULL, NULL);

        if (rc != LDAP_SUCCESS)
        {
            ldap_unbind_ext(_ldap, NULL, NULL);
            _ldap = NULL;
            throw runtime_error("Error binding to LDAP server: "+string(ldap_err2string(rc)));
        }
    }
}

std::string PiServer::_ldapEscape(const std::string &input)
{
    string output;

    for (int i=0; i<input.length(); i++)
    {
        switch (input[i])
        {
        case '*':
            output += "\\2A";
            break;
        case '(':
            output += "\\28";
            break;
        case ')':
            output += "\\29";
            break;
        case '\\':
            output += "\\5C";
            break;
        case '\0':
            output += "\\00";
            break;
        default:
            output += input[i];
        }
    }

    return output;
}

int PiServer::_getHighestUID(const char *objectClass /*= "posixAccount"*/, const char *attrname /*= "uidNumber"*/)
{
    set<int> uids;
    LDAPMessage  *result = NULL, *entry = NULL;
    int rc;
    struct berval **values;
    string ldapDN = _ldapDN();
    string filter = string("(objectClass=")+objectClass+")";
    const char *attrfilter[] = {attrname, NULL};

    _connectToLDAP();

    rc = ldap_search_ext_s(_ldap, ldapDN.c_str(), LDAP_SCOPE_SUBTREE,
                      filter.c_str(), (char **) attrfilter, 0, NULL, NULL, LDAP_NO_LIMIT, LDAP_NO_LIMIT, &result);
    if (rc != LDAP_SUCCESS)
    {
        if (result)
            ldap_msgfree(result);

        throw runtime_error("Error searching LDAP server for highest uid: "+string(ldap_err2string(rc)));
    }

    for (entry = ldap_first_entry(_ldap, result); entry != NULL; entry = ldap_next_entry(_ldap, entry))
    {
        values = ldap_get_values_len(_ldap, entry, attrname);
        if (values)
        {
            for (int i = 0; values[i]; i++)
            {
                string uidStr = string(values[i]->bv_val, values[i]->bv_len);
                uids.insert(stoi(uidStr));
            }

            ldap_value_free_len(values);
        }
    }

    if (result)
        ldap_msgfree(result);

    if (uids.empty())
        return 0;
    else
        return *uids.rbegin();
}

void PiServer::_ldapAdd(const std::string &dn, const multimap<std::string,std::string> &attrmap, bool modify)
{
    _connectToLDAP();
    int count = attrmap.size();
    LDAPMod *attrs   = new LDAPMod[count];
    LDAPMod **pattrs  = new LDAPMod*[count+1];
    struct berval *bers = new struct berval[count];
    struct berval **pbers = new struct berval*[count*2];
    int iber = 0, ipber, iattr, rc;

    /* Create ber structure for each value */
    for (const auto &kv : attrmap)
    {
        bers[iber].bv_val = (char *) kv.second.c_str();
        bers[iber].bv_len = kv.second.size();
        iber++;
    }

    iattr = 0; iber = 0; ipber = 0;
    for (auto iter = attrmap.begin(); iter != attrmap.end(); iattr++)
    {
        const string &key = iter->first;
        attrs[iattr].mod_type = (char *) key.c_str();

        pattrs[iattr] = &attrs[iattr];
        if (iter->second.empty())
        {
            attrs[iattr].mod_op = LDAP_MOD_DELETE;
            attrs[iattr].mod_vals.modv_strvals = NULL;
            iter++;
        }
        else
        {
            attrs[iattr].mod_op = LDAP_MOD_REPLACE | LDAP_MOD_BVALUES;
            attrs[iattr].mod_vals.modv_bvals = &pbers[ipber];

            /* There can be one or more values with same key. In all
               cases they will be after each other as multimap is always sorted */
            while (iter != attrmap.end() && iter->first == key)
            {
                pbers[ipber++] = &bers[iber++];
                iter++;
            }
            pbers[ipber++] = NULL;
        }
    }
    pattrs[iattr] = NULL;

    if (modify)
        rc = ldap_modify_ext_s(_ldap, dn.c_str(), pattrs, NULL, NULL);
    else
        rc = ldap_add_ext_s(_ldap, dn.c_str(), pattrs, NULL, NULL);

    delete [] attrs;
    delete [] bers;
    delete [] pbers;
    delete [] pattrs;

    if (rc != LDAP_SUCCESS)
        throw runtime_error("Error adding object to LDAP: "+string(ldap_err2string(rc)));
}

void PiServer::_ldapModify(const std::string &dn, const std::multimap<std::string,std::string> &attr)
{
   _ldapAdd(dn, attr, true);
}

void PiServer::_ldapDelete(const std::string &dn)
{
    _connectToLDAP();

    int rc = ldap_delete_ext_s(_ldap, dn.c_str(), NULL, NULL);
    if (rc != LDAP_SUCCESS)
        throw runtime_error("Error deleting object from LDAP: "+string(ldap_err2string(rc)));
}

std::string PiServer::_fileGetContents(const std::string &filename)
{
    ifstream i(filename, ios_base::in | ios_base::binary);
    stringstream buffer;
    buffer << i.rdbuf();
    return buffer.str();
}

void PiServer::_filePutContents(const std::string &filename, const std::string &contents)
{
    ofstream o(filename, ios_base::out | ios_base::binary | ios_base::trunc);
    o.write(contents.c_str(), contents.size());
}

void PiServer::setSetting(const std::string &key, const std::string &value)
{
    _settings[key] = value;
}

void PiServer::setSetting(const std::string &key, int value)
{
    _settings[key] = value;
}

std::string PiServer::getSetting(const std::string &key, const std::string &defaultValue)
{
    try
    {
        return _settings.at(key).get<string>();
    }
    catch (exception &e)
    {
        return defaultValue;
    }
}

int PiServer::getSetting(const std::string &key, int defaultValue)
{
    try
    {
        return _settings.at(key).get<int>();
    }
    catch (exception &e)
    {
        return defaultValue;
    }
}

void PiServer::unsetSetting(const std::string &key)
{
    _settings.erase(key);
}

void PiServer::saveSettings()
{
    if (::access(PISERVER_SETTINGSFILE, F_OK) == -1)
    {
        /* File doesn't exists yet. Create it empty first as
           ofstream doesn't support setting file permissions */
        int fd = ::open(PISERVER_SETTINGSFILE, O_CREAT, 0600);
        if (fd != -1)
            close(fd);
    }

    std::ofstream o(PISERVER_SETTINGSFILE);
    o << std::setw(4) << _settings << std::endl;
}

double PiServer::availableDiskSpace(const std::string &path)
{
    double bytesfree = 0;
    struct statvfs buf;

    if (statvfs(path.c_str(), &buf)) {
            return -1;
    }
    if (buf.f_frsize) {
            bytesfree = (((double)buf.f_bavail) * ((double)buf.f_frsize));
    } else {
            bytesfree = (((double)buf.f_bavail) * ((double)buf.f_bsize));
    }

    return bytesfree;
}

bool PiServer::_activeDirectory()
{
    return getSetting("ldapServerType") == "ad";
}

bool PiServer::externalServer()
{
    return !getSetting("ldapServerType").empty();
}

std::string PiServer::_ldapDN()
{
    return getSetting("ldapDN", PISERVER_LDAP_DN);
}

std::string PiServer::_uidField()
{
    return _activeDirectory() ? "sAMAccountName" : "uid";
}

/* Try out LDAP login details, and return domain sid if AD */
std::string PiServer::getDomainSidFromLdap(const std::string &server, const std::string &servertype,
                                       const std::string &basedn, const std::string &bindUser, const std::string &bindPass)
{
    string sid;
    struct berval cred, **values;
    LDAPMessage  *result = NULL, *entry = NULL;
    int version = LDAP_VERSION3;
    int rc = ldap_initialize(&_ldap, server.c_str() );

    if (rc != LDAP_SUCCESS)
    {
        throw runtime_error("Error initializing LDAP connecting: "+string(ldap_err2string(rc)));
    }

    try
    {
        ldap_set_option(_ldap, LDAP_OPT_PROTOCOL_VERSION, &version);
        ldap_set_option(_ldap, LDAP_OPT_REFERRALS, LDAP_OPT_OFF);
        cred.bv_val = (char *) bindPass.c_str();
        cred.bv_len = bindPass.length();
        rc = ldap_sasl_bind_s(_ldap, bindUser.c_str(), LDAP_SASL_SIMPLE, &cred, NULL, NULL, NULL);

        if (rc != LDAP_SUCCESS)
        {
            throw runtime_error(Glib::ustring::compose("Error logging in to LDAP server '%1' with user '%2': %3", server, bindUser, ldap_err2string(rc)));
        }

        /* Try to fetch information about domain */
        rc = ldap_search_ext_s(_ldap, basedn.c_str(), LDAP_SCOPE_BASE,
                          NULL, NULL, 0, NULL, NULL, LDAP_NO_LIMIT, LDAP_NO_LIMIT, &result);
        if (rc != LDAP_SUCCESS)
        {
            if (result)
                ldap_msgfree(result);

            throw runtime_error(Glib::ustring::compose("Error searching LDAP server for DN '%1': %2", basedn, ldap_err2string(rc)));
        }

        entry = ldap_first_entry(_ldap, result);
        if (!entry)
        {
            throw runtime_error(Glib::ustring::compose("DN '%1' does not exist on the server. Verify domain entered.", basedn));
        }

        values = ldap_get_values_len(_ldap, entry, "objectSid");
        if (values)
        {
            unsigned char *b = (unsigned char *) values[0]->bv_val;
            size_t len = values[0]->bv_len;

            if (len > 7)
            {
                /* Convert binary objectSid to string format */

                int rev = b[0];
                int subauthCount = b[1];
                uint32_t identAuth = GUINT_FROM_BE( *(uint32_t *)(b+4) );

                if (len != subauthCount*4+8)
                    throw runtime_error("Error parsing objectSid");

                sid = "S-"+to_string(b[0])+"-"+to_string(identAuth);

                for (int i=0; i<subauthCount; i++)
                {
                    sid += "-"+to_string(GUINT_FROM_LE( *(uint32_t *)(b+i*4+8) ));
                }
            }
            ldap_value_free_len(values);
        }

        if (servertype == "ad" && sid.empty())
        {
            throw runtime_error(Glib::ustring::compose("DN '%1' does not have objectSid attribute. Verify if LDAP server type selected is correct.", basedn));
        }
    }
    catch (...)
    {
        /* Clean-up before passing through exception */
        ldap_unbind_ext(_ldap, NULL, NULL);
        _ldap = NULL;
        throw;
    }

    ldap_unbind_ext(_ldap, NULL, NULL);
    _ldap = NULL;

    return sid;
}

std::set<std::string> PiServer::getPotentialBaseDNs()
{
    std::set<string> containers;
    LDAPMessage  *result = NULL, *entry = NULL;
    int rc;
    string ldapDN = _ldapDN();
    string filter = "(|(objectClass=organizationalUnit)(objectClass=container)(objectClass=domainDNS)(objectClass=dcObject))";
    const char *attrfilter[] = {NULL};

    _connectToLDAP();
    rc = ldap_search_ext_s(_ldap, ldapDN.c_str(), LDAP_SCOPE_SUBTREE,
                      filter.c_str(), (char **) attrfilter, 0, NULL, NULL, LDAP_NO_LIMIT, LDAP_NO_LIMIT, &result);
    if (rc != LDAP_SUCCESS)
    {
        if (result)
            ldap_msgfree(result);

        throw runtime_error("Error searching LDAP server: "+string(ldap_err2string(rc)));
    }

    for (entry = ldap_first_entry(_ldap, result); entry != NULL; entry = ldap_next_entry(_ldap, entry))
    {
        string dn;

        char *dn_cstr = ldap_get_dn(_ldap, entry);
        dn = dn_cstr;
        ldap_memfree(dn_cstr);
        containers.insert(dn);
    }

    if (result)
        ldap_msgfree(result);

    return containers;
}

std::set<std::string> PiServer::getLdapGroups()
{
    std::set<string> groups;
    LDAPMessage  *result = NULL, *entry = NULL;
    int rc;
    string ldapDN = _ldapDN();
    string filter = "(|(objectClass=group)(objectClass=groupOfNames)(objectClass=groupOfUniqueNames))";
    const char *attrfilter[] = {NULL};

    _connectToLDAP();
    rc = ldap_search_ext_s(_ldap, ldapDN.c_str(), LDAP_SCOPE_SUBTREE,
                      filter.c_str(), (char **) attrfilter, 0, NULL, NULL, LDAP_NO_LIMIT, LDAP_NO_LIMIT, &result);
    if (rc != LDAP_SUCCESS)
    {
        if (result)
            ldap_msgfree(result);

        throw runtime_error("Error searching LDAP server: "+string(ldap_err2string(rc)));
    }

    for (entry = ldap_first_entry(_ldap, result); entry != NULL; entry = ldap_next_entry(_ldap, entry))
    {
        string dn;

        char *dn_cstr = ldap_get_dn(_ldap, entry);
        dn = dn_cstr;
        ldap_memfree(dn_cstr);
        groups.insert(dn);
    }

    if (result)
        ldap_msgfree(result);

    return groups;
}

std::string PiServer::getLdapFilter(const std::string &forGroup)
{
    string filter;

    if (forGroup.empty())
    {
        if (_activeDirectory()) /* Hide hidden and disabled users */
            filter = "(&(objectCategory=person)(objectClass=user)"
                     "(!(UserAccountControl:1.2.840.113556.1.4.803:=2)))";
        else
            filter = "(objectClass=posixAccount)";
    }
    else
    {
        if (_activeDirectory())
            filter = "(&(objectCategory=person)(objectClass=user)"
                     "(memberof:1.2.840.113556.1.4.1941:="+_ldapEscape(forGroup)+")"
                     "(!(UserAccountControl:1.2.840.113556.1.4.803:=2)))";
        else
            filter = "(&(objectClass=posixAccount)(memberof="+_ldapEscape(forGroup)+"))";
    }

    return filter;
}
