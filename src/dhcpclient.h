#ifndef DHCPCLIENT_H
#define DHCPCLIENT_H

/**
 * DHCP client class
 * Serves two purposes:
 *
 * - Double check no other DHCP servers are active, if user wants to run standalone mode
 * - Query DHCP server to autodetect domain name in use. Makes setting up active directory authentication easier
 */

#include <glibmm.h>
#include <set>
#include <string>

class DhcpClient
{
public:
    DhcpClient();
    virtual ~DhcpClient();
    void sendDhcpDiscover(const std::string &networkInterface);

    /* signals */
    inline sigc::signal<void,std::string /*server ip*/> signal_dhcpOffer()
    {
        return _signal_dhcpOffer;
    }

    inline sigc::signal<void,std::string /*domain*/> signal_dhcpOfferDomain()
    {
        return _signal_dhcpOfferDomain;
    }


    inline sigc::signal<void> signal_timeout()
    {
        return _signal_timeout;
    }

protected:
    int _s, _timeoutInSeconds;
    bool _haveOffer;
    sigc::connection _conn;
    sigc::signal<void,std::string> _signal_dhcpOffer;
    sigc::signal<void,std::string> _signal_dhcpOfferDomain;
    sigc::signal<void> _signal_timeout;
    sigc::connection _timer;

    void _parseDhcpOptions(const unsigned char *buf, int len);

    /* Slots */
    bool on_packet(Glib::IOCondition cond);
    bool on_timeout();
};

#endif // DHCPCLIENT_H
