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
#include <vector>

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
	int getMessageType();

	static int writeResponse(Packet* packet, int offset, bool discover, uint32_t transaction_id, struct in_addr client, ether_addr client_ether_addr, struct in_addr server_identifier, struct in_addr subnet_mask, struct in_addr router, std::vector<struct in_addr> dns_vector, int lease_time);
};

#endif
