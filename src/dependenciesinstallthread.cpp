#include "dependenciesinstallthread.h"
#include "config.h"
#include <chrono>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

using namespace std;

DependenciesInstallThread::DependenciesInstallThread(PiServer *ps)
    : _ps(ps), _thread(NULL)
{
}

DependenciesInstallThread::~DependenciesInstallThread()
{
    if (_thread)
    {
        _thread->join();
        delete _thread;
    }
}

void DependenciesInstallThread::start()
{
    _thread = new thread(&DependenciesInstallThread::run, this);
}

void DependenciesInstallThread::run()
{
    string ldapUri = _ps->getSetting("ldapUri", PISERVER_LDAP_URL);
    string ldapDN = _ps->getSetting("ldapDN", PISERVER_LDAP_DN);
    string ldapServerType = _ps->getSetting("ldapServerType");
    string ldapUser = _ps->getSetting("ldapUser");
    string ldapPassword = _ps->getSetting("ldapPassword");
    string ldapExtraConfig = _ps->getSetting("ldapExtraConfig");
    string pkglist = "dnsmasq openssh-server nfs-kernel-server libnss-ldapd libpam-ldapd ldap-utils gnutls-bin libarchive-tools ntp";
    bool installSlapd = ldapServerType.empty();
    bool nslcdAlreadyExists = ::access("/etc/nslcd.conf", F_OK) != -1;
    bool slapdAlreadyExists = ::access("/etc/ldap/slapd.d", F_OK) != -1;

    try
    {
        _execCheckResult("apt-get update -q");

        if (installSlapd)
        {
            if (slapdAlreadyExists)
            {
                cerr << "Force removal of existing slapd" << endl;
                _execCheckResult("apt-get -q -y remove --purge slapd");
            }

            _preseed({
                {"slapd	slapd/password1	password", ldapPassword},
                {"slapd	slapd/internal/adminpw password", ldapPassword},
                {"slapd	slapd/internal/generated_adminpw password", ldapPassword},
                {"slapd	slapd/password2	password", ldapPassword},
                {"slapd	slapd/allow_ldap_v2	boolean", "false"},
                {"slapd	slapd/password_mismatch", "note"},
                {"slapd	slapd/invalid_config boolean", "true"},
                {"slapd	shared/organization	string", PISERVER_DOMAIN},
                {"slapd	slapd/upgrade_slapcat_failure", "error"},
                {"slapd	slapd/no_configuration boolean", "false"},
                {"slapd	slapd/move_old_database	boolean", "true"},
                {"slapd	slapd/dump_database_destdir	string", "/var/backups/slapd-VERSION"},
                {"slapd	slapd/purge_database boolean", "false"},
                {"slapd	slapd/domain string", PISERVER_DOMAIN},
                {"slapd	slapd/backend select", "MDB"},
                {"slapd	slapd/dump_database	select", "when needed"}
            });
            pkglist += " slapd";
        }

        /* Install pam_mkhomedir to create home on first login */
        _execCheckResult("cp " PISERVER_DATADIR "/scripts/mkhomedir-piserver /usr/share/pam-configs");
        _execCheckResult("pam-auth-update --package");

        map<string,string> nslcdPreseed = {
            {"nslcd nslcd/ldap-uris string", ldapUri},
            {"nslcd nslcd/ldap-base string", ldapDN},
            {"libnss-ldapd libnss-ldapd/nsswitch multiselect", "group, passwd, shadow"},
            {"nslcd nslcd/ldap-auth-type select", "none"}
        };
        if (!ldapServerType.empty())
        {
            if (!ldapUser.empty())
            {
                nslcdPreseed["nslcd nslcd/ldap-auth-type select"] = "simple";
                nslcdPreseed.insert(make_pair("nslcd nslcd/ldap-binddn string", ldapUser));
                nslcdPreseed.insert(make_pair("nslcd nslcd/ldap-bindpw string", ldapPassword));
            }
        }
        _preseed(nslcdPreseed);

        ::setenv("DEBIAN_FRONTEND", "noninteractive", 1);
        _execCheckResult("apt-get -q -y install "+pkglist);

        if (nslcdAlreadyExists)
        {
            cerr << "Force reconfiguration of existing nslcd" << endl;
            ::unlink("/etc/nslcd.conf");
            _execCheckResult("dpkg-reconfigure nslcd");
        }
        ::unsetenv("DEBIAN_FRONTEND");

        if (installSlapd)
        {
            // Using gnutls's certtool instead of OpenSSL, because it allows setting start date to 1970
            _execCheckResult("certtool --generate-privkey --ecc --outfile /etc/ldap/piserver.key");
            ofstream os("/etc/ldap/piserver.tpl");
            os << "cn = \"piserver\"" << endl
               << "tls_www_server" << endl
               << "activation_date=\"1970-01-01 00:00:00 UTC\"" << endl
               << "expiration_days=\"-1\"" << endl;
            os.close();
            _execCheckResult("certtool --generate-self-signed --load-privkey /etc/ldap/piserver.key --template=/etc/ldap/piserver.tpl --outfile=/etc/ssl/certs/piserver.pem");
            _execCheckResult("chown openldap /etc/ldap/piserver.key");
            if (::unlink("/etc/ldap/piserver.tpl") != 0) { }

            const char *ldif =
                    "dn: cn=config\n"
                    "changetype: modify\n"
                    "replace: olcTLSCertificateFile\n"
                    "olcTLSCertificateFile: /etc/ssl/certs/piserver.pem\n"
                    "-\n"
                    "replace: olcTLSCACertificateFile\n"
                    "olcTLSCACertificateFile: /etc/ssl/certs/piserver.pem\n"
                    "-\n"
                    "replace: olcTLSCertificateKeyFile\n"
                    "olcTLSCertificateKeyFile: /etc/ldap/piserver.key\n";
            FILE *f = popen("ldapmodify -Y EXTERNAL -H ldapi:///", "w");
            fwrite(ldif, strlen(ldif), 1, f);
            pclose(f);

            ldif =
                    "dn: cn=module{0},cn=config\n"
                    "changetype: modify\n"
                    "add: olcModuleload\n"
                    "olcModuleLoad: lastbind.la\n"
                    "\n"
                    "dn: olcOverlay=lastbind,olcDatabase={1}mdb,cn=config\n"
                    "changetype: add\n"
                    "objectClass: olcLastBindConfig\n"
                    "objectClass: olcOverlayConfig\n"
                    "objectClass: top\n"
                    "olcOverlay: lastbind\n"
                    "olcLastBindPrecision: 60\n";
            f = popen("ldapmodify -Y EXTERNAL -H ldapi:///", "w");
            fwrite(ldif, strlen(ldif), 1, f);
            pclose(f);

            // Test that LDAP server works. Generates exception if not
            _ps->isUsernameAvailable("test");
        }

        if (!ldapExtraConfig.empty())
        {
            ofstream o("/etc/nslcd.conf", ios_base::out | ios_base::binary | ios_base::app);
            o.write(ldapExtraConfig.c_str(), ldapExtraConfig.size());
            o.close();
            _execCheckResult("systemctl restart nslcd");
        }

        _execCheckResult("systemctl start piserver_ssh");
        _execCheckResult("systemctl enable piserver_ssh");
        _execCheckResult("systemctl enable rpcbind");
        _ps->addToExports(PISERVER_DISTROROOT " *(ro,no_subtree_check,no_root_squash,fsid=1055)");
        _ps->regenDnsmasqConf();

        // On Debian Stretch it seems necessary to restart nscd before logging in with LDAP works
        if ( ::system("systemctl restart nscd") != 0 ) { }

        ::sync();
        //std::this_thread::sleep_for(chrono::seconds(1));
        _signalSucess.emit();
    }
    catch (exception &e)
    {
        _error = e.what();
        _signalFailure.emit();
    }
}

void DependenciesInstallThread::_execCheckResult(const std::string &cmd)
{
    if (::system(cmd.c_str()) != 0)
    {
        throw runtime_error("Error executing '"+cmd+"'");
    }
}

void DependenciesInstallThread::_preseed(const std::map<std::string,std::string> &values)
{
    FILE *f = popen("/usr/bin/debconf-set-selections", "w");
    if (f)
    {
        for (auto kv : values)
        {
            string s = kv.first+" "+kv.second+"\n";
            fwrite(s.c_str(), s.size(), 1, f);
        }

        pclose(f);
    }
}

Glib::Dispatcher &DependenciesInstallThread::signalSuccess()
{
    return _signalSucess;
}

Glib::Dispatcher &DependenciesInstallThread::signalFailure()
{
    return _signalFailure;
}

std::string DependenciesInstallThread::error()
{
    return _error;
}
