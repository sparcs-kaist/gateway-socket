/*
 * dhcp.hh
 *
 * Created on : 2014. 1. 15.
 *      Author : aon
 */

#ifndef DHCP_HH_
#define DHCP_HH_

#include "packet.hh"
#include <netinet/in.h>
#include <net/ethernet.h>
#include <vector.h>

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
        int writeResponse(Packet* packet, uint32_t transaction_id, in_addr client_in_addr, ether_addr client_ether_addr, in_addr server_identifier, in_addr subnet_mask, in_addr router, vector<in_addr> dns_vector);
};

#endif
