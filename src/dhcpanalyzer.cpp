#include "dhcpanalyzer.h"
#include "piserver.h"
#include "dhcp.h"
#include <stdexcept>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <regex>

DhcpAnalyzer::DhcpAnalyzer()
    : _s(-1)
{
}

DhcpAnalyzer::~DhcpAnalyzer()
{
    if (_s != -1)
    {
        _conn.disconnect();
        close(_s);
    }
}

void DhcpAnalyzer::startListening()
{
    struct sockaddr_in a = { 0 };
    int flag = 1;

    a.sin_family = AF_INET;
    a.sin_port = htons(67);
    a.sin_addr.s_addr = INADDR_ANY;

    _s = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if (_s < 0)
        throw std::runtime_error("Error creating socket");

    setsockopt(_s, SOL_SOCKET, SO_REUSEADDR, (char *) &flag, sizeof(flag));
    setsockopt(_s, SOL_SOCKET, SO_BROADCAST, (char *) &flag, sizeof(flag));

    if (bind(_s, (struct sockaddr *) &a, sizeof(a)) < 0)
    {
        close(_s);
        throw std::runtime_error("Error binding to UDP port 67");
    }

    _conn = Glib::signal_io().connect(sigc::mem_fun(this, &DhcpAnalyzer::on_packet), _s, Glib::IO_IN);
}

bool DhcpAnalyzer::on_packet(Glib::IOCondition)
{
#ifdef OUI_FILTER
    static std::regex r(OUI_FILTER);
#endif
    struct dhcp_packet dhcp = { 0 };
    char macbuf[32];

    int len = recvfrom(_s, &dhcp, sizeof(dhcp), 0, NULL, 0);

    if ( len > 0
         && dhcp.op == 1
         && dhcp.cookie == DHCP_COOKIE
#ifdef DHCPANALYZER_FILTER_STRING
         && memmem(dhcp.data, sizeof(dhcp.data), DHCPANALYZER_FILTER_STRING, strlen(DHCPANALYZER_FILTER_STRING)) != NULL
#endif
        )
    {
        snprintf(macbuf, sizeof(macbuf), "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
                 dhcp.chaddr[0], dhcp.chaddr[1], dhcp.chaddr[2],
                 dhcp.chaddr[3], dhcp.chaddr[4], dhcp.chaddr[5]);
        std::string mac = macbuf;

#ifdef OUI_FILTER
        if (!regex_search(mac, r))
            return true;
#endif

        if (!_seen.count(mac))
        {
            _seen.insert(mac);
            _signal_macDetected.emit(mac);
        }
    }

    return true;
}
