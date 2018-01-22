#ifndef ACTIVEDISCOVERY_H
#define ACTIVEDISCOVERY_H

/**
 * Class to auto-detect Active Directory servers
 */

#include <glibmm.h>
#include <string>
#include "dhcpclient.h"

class ActiveDiscovery
{
public:
    ActiveDiscovery();
    virtual ~ActiveDiscovery();
    void startDiscovery(const std::string &networkInterface);

    /* signals */
    inline sigc::signal<void,std::string /* serverUri */, std::string /* baseDN */> signal_discovered()
    {
        return _signal_discovered;
    }

    inline sigc::signal<void> signal_discoveryFailed()
    {
        return _signal_discoveryFailed;
    }

protected:
    int _timeoutInSeconds;
    std::string _domain;
    DhcpClient _dhcp;
    sigc::signal<void,std::string,std::string> _signal_discovered;
    sigc::signal<void> _signal_discoveryFailed;
    sigc::connection _timer;

    void _discoveryFailed();

    /* slots */
    void onDhcpOfferWithDomain(const std::string &domain);
    bool on_timeout();
};

#endif // ACTIVEDISCOVERY_H
