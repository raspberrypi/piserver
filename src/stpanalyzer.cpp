/**
 * Class to detect if the Spanning-Tree-Protocol
 * is enabled on the Ethernet switch (which can
 * interfere with network booting)
 *
 * Detection only works when Piserver is installed
 * bare-metal on a server directly connected to
 * the network (not under with Vmware, Virtualbox, etc.)
 */

#include "stpanalyzer.h"
#include <cstdint>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/if.h>

struct stp_packet {
    /* Ethernet header */
    struct ethhdr eth;
    /* 802.2 LLC header */
    uint8_t dsap, ssap, control;
    /* STP fields */
    uint16_t protocol;
    uint8_t  version;
    uint8_t  msgtype;
    uint8_t  flags;
    uint16_t rootpriority;
    unsigned char rootmac[6];
    uint32_t rootpathcost;
    uint16_t bridgepriority;
    unsigned char bridgemac[6];
    uint16_t portid;
    uint16_t msgage;
    uint16_t maxage;
    uint16_t hellotime;
    uint16_t forwarddelay;
} __attribute__((__packed__));

#define LSAP_BDPU  0x42
#define MULTICAST_MAC_BDPU   {0x1, 0x80, 0xC2, 0, 0, 0}
#define MULTICAST_MAC_BDPUPV {0x1, 0, 0x0C, 0xCC, 0xCC, 0xCD}

StpAnalyzer::StpAnalyzer(int onlyReportIfForwardDelayIsAbove)
    : _s(-1), _minForwardDelay(onlyReportIfForwardDelayIsAbove)
{
}

StpAnalyzer::~StpAnalyzer()
{
    stopListening();
}

void StpAnalyzer::startListening(const std::string &ifname)
{
    int iface;

    _s = socket(AF_PACKET,SOCK_RAW,htons(ETH_P_802_2));
    if (_s < 0)
    {
        //throw std::runtime_error("Error creating STP listen socket");
        return;
    }

    iface = if_nametoindex(ifname.c_str());
    if (!iface) {
        //throw std::runtime_error("Error obtaining interface id");
        return;
    }

    struct packet_mreq mreq = { iface, PACKET_MR_MULTICAST, ETH_ALEN, MULTICAST_MAC_BDPU };
    setsockopt(_s, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    struct packet_mreq mreq2 = { iface, PACKET_MR_MULTICAST, ETH_ALEN, MULTICAST_MAC_BDPUPV };
    setsockopt(_s, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mreq2, sizeof(mreq2));

    _conn = Glib::signal_io().connect(sigc::mem_fun(this, &StpAnalyzer::on_packet), _s, Glib::IO_IN);
}

void StpAnalyzer::stopListening()
{
    if (_s != -1)
    {
        _conn.disconnect();
        close(_s);
    }
}

bool StpAnalyzer::on_packet(Glib::IOCondition)
{
    struct stp_packet packet = { 0 };
    char strbuf[64];

    int len = recvfrom(_s, &packet, sizeof(packet), 0, NULL, 0);

    if (len == sizeof(packet)
            && packet.dsap == LSAP_BDPU
            && packet.ssap == LSAP_BDPU
            && packet.protocol == 0)
    {
        /* It is a STP packet */
        int forwardDelay = GINT_FROM_LE(packet.forwarddelay);
        if (forwardDelay > _minForwardDelay)
        {
            snprintf(strbuf, sizeof(strbuf), "MAC: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x forward delay: %d",
                     packet.eth.h_source[0], packet.eth.h_source[1], packet.eth.h_source[2],
                     packet.eth.h_source[3], packet.eth.h_source[4], packet.eth.h_source[5], forwardDelay);
            std::string extrainfo = strbuf;
            _signal_detected.emit(extrainfo);
        }

        /* Only analyze first packet */
        stopListening();
    }

    return true;
}
