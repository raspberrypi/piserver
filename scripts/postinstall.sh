#!/bin/sh

set -e

#
# Copy certain settings such as keyboard layout and timezone from the host system
#

if [ -d "$DISTROROOT/etc" ]; then
    cp -a /etc/timezone "$DISTROROOT/etc"
fi
if [ -d "$DISTROROOT/etc/default" ]; then
    cp -a /etc/default/keyboard "$DISTROROOT/etc/default"
fi
if [ -d "$DISTROROOT/etc/console-setup" ]; then
    cp -a /etc/console-setup/cached* "$DISTROROOT/etc/console-setup"
fi

#
# Copy LDAP SSL certificate, and enable verification
#

if [ -d "$DISTROROOT/etc/ssl/certs" ] && [ -f "/etc/ssl/certs/piserver.pem" ]; then
    cp -a /etc/ssl/certs/piserver.pem "$DISTROROOT/etc/ssl/certs"
    if [ -f "$DISTROROOT/etc/nslcd.conf" ]; then
        sed -i "s/#ssl off/ssl start_tls/g" "$DISTROROOT/etc/nslcd.conf"
        sed -i "s/#tls_reqcert never/tls_reqcert demand/g" "$DISTROROOT/etc/nslcd.conf"
        sed -i "s#/etc/ssl/certs/ca-certificates.crt#/etc/ssl/certs/piserver.pem#g" "$DISTROROOT/etc/nslcd.conf"
    fi
fi
