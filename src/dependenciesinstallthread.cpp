#include "dependenciesinstallthread.h"
#include "config.h"
#include <chrono>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

using namespace std;

DependenciesInstallThread::DependenciesInstallThread(PiServer *ps, const std::string &ldapPassword)
    : _ps(ps), _ldapPassword(ldapPassword), _thread(NULL)
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
    map<string,string> preseedValues = {
        {"slapd	slapd/password1	password", _ldapPassword},
        {"slapd	slapd/internal/adminpw password", _ldapPassword},
        {"slapd	slapd/internal/generated_adminpw password", _ldapPassword},
        {"slapd	slapd/password2	password", _ldapPassword},
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
        {"slapd	slapd/dump_database	select", "when needed"},
        {"nslcd nslcd/ldap-uris string", PISERVER_LDAP_URL},
        {"nslcd nslcd/ldap-base string", PISERVER_LDAP_DN},
        {"libnss-ldapd libnss-ldapd/nsswitch multiselect", "group, passwd, shadow"}
    };

    bool nslcdAlreadyExists = ::access("/etc/nslcd.conf", F_OK) != -1;
    bool slapdAlreadyExists = ::access("/etc/ldap/slapd.d", F_OK) != -1;

    try
    {
        _execCheckResult("apt-get update -q");
        _preseed(preseedValues);

        ::setenv("DEBIAN_FRONTEND", "noninteractive", 1);
        _execCheckResult("apt-get -q -y install dnsmasq openssh-server nfs-kernel-server slapd libnss-ldapd libpam-ldapd ldap-utils gnutls-bin");

        if (slapdAlreadyExists)
        {
            cerr << "Force reconfiguration of existing slapd" << endl;
            _execCheckResult("dpkg-reconfigure slapd");
        }

        if (nslcdAlreadyExists)
        {
            cerr << "Force reconfiguration of existing nslcd" << endl;
            _execCheckResult("dpkg-reconfigure nslcd");
        }
        ::unsetenv("DEBIAN_FRONTEND");

        // Using gnutls's certtool instead of OpenSSL, because it allows setting start date to 1970
        _execCheckResult("certtool --generate-privkey --ecc --outfile /etc/ldap/piserver.key");
        ofstream os("/etc/ldap/piserver.tpl");
        os << "cn = \"piserver\"" << endl
           << "tls_www_server" << endl
           << "activation_date=\"1970-01-01 00:00:00 UTC\"" << endl
           << "expiration_days=\"7300\"" << endl;
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

        // Test that LDAP server works. Generates exception if not
        _ps->isUsernameAvailable("test");

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
