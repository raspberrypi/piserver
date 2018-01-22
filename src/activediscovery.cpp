/**
 * Class to auto-detect Active Directory servers
 */

#include "activediscovery.h"
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <iostream>

using namespace std;

ActiveDiscovery::ActiveDiscovery()
    : _timeoutInSeconds(3)
{
    _dhcp.signal_dhcpOfferDomain().connect( sigc::mem_fun(this, &ActiveDiscovery::onDhcpOfferWithDomain) );
}

ActiveDiscovery::~ActiveDiscovery()
{
    if (_timer.connected())
    {
        _timer.disconnect();
    }
}

void ActiveDiscovery::startDiscovery(const std::string &networkInterface)
{
    /* Step 1: try to obtain domain name from DHCP server */
    _domain.clear();
    _dhcp.sendDhcpDiscover(networkInterface);
    _timer = Glib::signal_timeout().connect_seconds( sigc::mem_fun(this, &ActiveDiscovery::on_timeout), _timeoutInSeconds);
}

/* Received a DHCPOFFER that includes our domain name */
void ActiveDiscovery::onDhcpOfferWithDomain(const std::string &domain)
{
    if (!_domain.empty()) /* Only handle first OFFER received */
        return;

    /* Step 2: try to obtain AD server address from DNS SRV records */
    std::string query = "_ldap._tcp."+domain;
    struct __res_state state = { 0 };
    ns_msg handle;
    unsigned char buf[1024];
    int len;
    _domain = domain;

    res_ninit(&state);
    len = res_nquery(&state, query.c_str(), C_IN, ns_t_srv, buf, sizeof(buf));
    if (len < 0 || ns_initparse(buf, len, &handle) != 0)
    {
        _discoveryFailed();
        return;
    }

    int rrcount = ns_msg_count(handle, ns_s_an);
    ns_rr rr;

    for (int i=0; i<rrcount; i++)
    {
        if (ns_parserr(&handle, ns_s_an, i, &rr) == 0 && ns_rr_type(rr) == ns_t_srv)
        {
            char fqdn[1024];
            if (dn_expand(ns_msg_base(handle), ns_msg_end(handle), ns_rr_rdata(rr) + 6, fqdn, sizeof(fqdn)) > 0)
            {
                string server = fqdn;

                if (server.find(".local") != string::npos)
                {
                    /* If hostname ends with .local use IP from DNS reply (additional section), instead of host name.
                       Some have .local configured as domain on their Windows DNS server, while Linux expects
                       .local domains to be mDNS (avahi), not regular DNS */
                    if (ns_parserr(&handle, ns_s_ar, i, &rr) == 0 && ns_rr_type(rr) == ns_t_a)
                    {
                        char ipstr[INET_ADDRSTRLEN] = {0};
                        inet_ntop(AF_INET, ns_rr_rdata(rr), ipstr, INET_ADDRSTRLEN);
                        server = ipstr;
                    }
                }

                if (_timer.connected())
                {
                    _timer.disconnect();
                }
                _signal_discovered.emit(server, _domain);
                return;
            }
        }
    }

    _discoveryFailed();
}

void ActiveDiscovery::_discoveryFailed()
{
    if (_timer.connected())
    {
        _timer.disconnect();
    }

    _signal_discoveryFailed.emit();
}

bool ActiveDiscovery::on_timeout()
{
    _signal_discoveryFailed.emit();
    return false;
}
