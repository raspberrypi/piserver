#ifndef CONFIG_H
#define CONFIG_H

#ifndef PISERVER_DATADIR
#define PISERVER_DATADIR  "/var/lib/piserver"
#endif

#define PACKAGE            "piserver"

#define PISERVER_HOSTSFILE  PISERVER_DATADIR "/hosts.json"
#define PISERVER_DISTROFILE PISERVER_DATADIR "/installed_distros.json"
#define PISERVER_SETTINGSFILE PISERVER_DATADIR "/settings.json"
#define PISERVER_TFTPROOT   PISERVER_DATADIR "/tftproot"
#define PISERVER_DISTROROOT PISERVER_DATADIR "/os"
#define PISERVER_SHAREDROOT PISERVER_DISTROROOT "/shared"
#define PISERVER_HOMEROOT   "/home"
#define PISERVER_DNSMASQCONFFILE "/etc/dnsmasq.d/piserver"
#define PISERVER_REPO_URL  "http://downloads.raspberrypi.org/piserver.json"
#define PISERVER_REPO_CACHEFILE PISERVER_DATADIR "/avail_distros_cache.json"
#define PISERVER_POSTINSTALLSCRIPT PISERVER_DATADIR "/scripts/postinstall.sh"
#define PISERVER_CONVERTSCRIPT PISERVER_DATADIR "/scripts/convert_folder.sh"

#define OUI_FILTER         "^(b8:27:eb|dc:a6:32|e4:5f:01|d8:3a:dd):"
#define DHCPANALYZER_FILTER_STRING "PXEClient"

#define PISERVER_LDAP_URL  "ldapi:///"
#define PISERVER_DOMAIN    "raspberrypi.local"
#define PISERVER_LDAP_DN   "dc=raspberrypi,dc=local"

#define PISERVER_LDAP_USER "cn=admin,dc=raspberrypi,dc=local"

#define PISERVER_LDAP_TIMEOUT  10
#define PISERVER_MIN_UID   10000
#define PISERVER_GID       100
#define PISERVER_SHELL     "/bin/bash"

#endif // CONFIG_H
