#ifndef DHCP_H
#define DHCP_H

#include <cstdint>

struct dhcp_packet {
      uint8_t op, htype, hlen, hops;
      uint32_t xid;
      uint16_t secs, flags;
      uint32_t ciaddr, yiaddr, siaddr, giaddr;
      unsigned char chaddr[16], legacy[192];
      uint32_t cookie;
      unsigned char data[500];
} __attribute__((__packed__));

#define DHCP_COOKIE  htonl(0x63825363)

#endif // DHCP_H
