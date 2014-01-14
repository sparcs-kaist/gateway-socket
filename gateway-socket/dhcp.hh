/*
 * dhcp.hh
 *
 * Created on : 2014. 1. 15.
 *      Author : aon
 */

#ifndef DHCP_HH_
#define DHCP_HH_

#include "packet.hh"
#include <net/ethernet.h>

class DHCP
{
    private:
        Packet* packet;
        int offset;
    public:
        DHCP(Packet* packet);
        DHCP(Packet* packet, int offset);
        char getOpcode();
        uint32_t getTransactionID();
        struct ether_addr getClientMAC();
};

#endif
