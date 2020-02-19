/**
 * DHCP client class
 * Serves two purposes:
 *
 * - Double check no other DHCP servers are active, if user wants to run standalone mode
 * - Query DHCP server to autodetect domain name in use. Makes setting up active directory authentication easier
 */

#include "dhcpclient.h"
#include "dhcp.h"
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

DhcpClient::DhcpClient()
    : _s(-1), _timeoutInSeconds(3)
{
}

DhcpClient::~DhcpClient()
{
    if (_timer.connected())
    {
        _timer.disconnect();
    }
    if (_s != -1)
    {
        _conn.disconnect();
        close(_s);
    }
}

void DhcpClient::sendDhcpDiscover(const std::string &networkInterface)
{
    if (_s == -1)
    {
        struct sockaddr_in a = { 0 };
        int flag = 1;

        a.sin_family = AF_INET;
        a.sin_port = htons(68);
        a.sin_addr.s_addr = INADDR_ANY;

        _s = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        if (_s < 0)
            throw std::runtime_error("Error creating socket");

        setsockopt(_s, SOL_SOCKET, SO_REUSEADDR, (char *) &flag, sizeof(flag));
        setsockopt(_s, SOL_SOCKET, SO_BROADCAST, (char *) &flag, sizeof(flag));
        setsockopt(_s, SOL_SOCKET, SO_BINDTODEVICE, networkInterface.c_str(), networkInterface.size());

        if (bind(_s, (struct sockaddr *) &a, sizeof(a)) < 0)
        {
            close(_s);
            throw std::runtime_error("Error binding to UDP port 68");
        }

        _conn = Glib::signal_io().connect(sigc::mem_fun(this, &DhcpClient::on_packet), _s, Glib::IO_IN);
    }

    struct dhcp_packet discover = { 0 };
    int i = 0;
    discover.op    = 1;
    discover.htype = 1;
    discover.hlen  = 6;
    discover.secs  = htons(60);
    discover.flags = htons(0x8000); /* Broadcast */
    discover.chaddr[0] = 0x31; /* Dummy MAC */
    discover.chaddr[1] = 0x32;
    discover.chaddr[2] = 0x33;
    discover.chaddr[3] = 0x34;
    discover.chaddr[4] = 0x35;
    discover.chaddr[5] = 0x36;
    discover.cookie = DHCP_COOKIE;
    /* DHCP options */
    discover.data[i++] = 53; /* Message type */
    discover.data[i++] = 1;
    discover.data[i++] = 1;  /* Discover */
    discover.data[i++] = 61; /* Client identifier */
    discover.data[i++] = 7;
    discover.data[i++] = 1;
    for (int j=0; j<6; j++)
    {
        discover.data[i++] = discover.chaddr[j];
    }
    discover.data[i++] = 55; /* Parameter request list */
    discover.data[i++] = 1;
    discover.data[i++] = 15; /* We would like to know the domain */
    discover.data[i++] = 0xFF;

    size_t len = sizeof(discover) - sizeof(discover.data) + i;
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(67);
    addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);

    if (sendto(_s, &discover, len, 0, (struct sockaddr *) &addr, sizeof(addr)) == -1)
    {
        throw std::runtime_error("Error sending DHCPDISCOVER packet");
    }

    _haveOffer = false;
    _timer = Glib::signal_timeout().connect_seconds( sigc::mem_fun(this, &DhcpClient::on_timeout), _timeoutInSeconds);
}

bool DhcpClient::on_packet(Glib::IOCondition)
{
    struct sockaddr_storage addr = { 0 };
    socklen_t addrlen = sizeof(addr);
    char ipstr[INET_ADDRSTRLEN] = { 0 };
    struct dhcp_packet dhcp = { 0 };
    int len = recvfrom(_s, &dhcp, sizeof(dhcp), 0, (struct sockaddr *) &addr, &addrlen);

    if ( len > 0
         && dhcp.op == 2
         && dhcp.cookie == DHCP_COOKIE
         && addr.ss_family == AF_INET
        )
    {
        _haveOffer = true;
        struct sockaddr_in *addr4 = (struct sockaddr_in *) &addr;
        inet_ntop(AF_INET, &addr4->sin_addr, ipstr, sizeof(ipstr));
        _signal_dhcpOffer.emit(ipstr);

        /* Parse options looking for domain */
        int optlen = len - (sizeof(dhcp) - sizeof(dhcp.data));
        if (optlen > 0)
            _parseDhcpOptions(dhcp.data, optlen);
    }

    return true;
}

bool DhcpClient::on_timeout()
{
    _conn.disconnect();
    close(_s);
    _s = -1;

    if (!_haveOffer)
        _signal_timeout.emit();

    return false;
}

/* Parse DHCP options to look for our domain name */
void DhcpClient::_parseDhcpOptions(const unsigned char *buf, int len)
{
    int i = 0;

    while ((i+1) < len)
    {
        int opt = buf[i++];

        if (opt == 0xFF) /* End of options */
            break;
        if (opt == 0) /* Padding */
            continue;

        int optlen = buf[i++];
        if (i+optlen >= len)
        {
            break; /* Option beyond length of packet. TODO: parse overload space as well */
        }

        if (opt == 15 && optlen) /* Domain */
        {
            std::string domain((char *) &buf[i], optlen);
            if (domain.back() == '\0')
                domain.pop_back();

            if (!domain.empty())
            {
                _signal_dhcpOfferDomain.emit(domain);
            }
        }

        i += optlen;
    }
}
