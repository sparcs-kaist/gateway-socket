/*
 * protocol.cpp
 *
 *  Created on: 2014. 1. 12.
 *      Author: leeopop
 */

#include "protocol.hh"
#include <stdio.h>
#include <arpa/inet.h>

const ether_addr Ethernet::BROADCAST_ADDR = { {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF} };

Ethernet::Ethernet(Packet* packet)
{
	this->packet = packet;
}

struct ether_addr Ethernet::getSource()
{
	struct ether_addr ret;
	packet->readByteArray(ETH_ALEN, ETH_ALEN+ETH_ALEN, &ret);
	return ret;
}
struct ether_addr Ethernet::getDestination()
{
	struct ether_addr ret;
	packet->readByteArray(0, ETH_ALEN, &ret);
	return ret;
}

char* Ethernet::printMAC(struct ether_addr addr, char* buf, int len)
{
	snprintf(buf, len, "%02X:%02X:%02X:%02X:%02X:%02X",
			addr.ether_addr_octet[0], addr.ether_addr_octet[1], addr.ether_addr_octet[2],
			addr.ether_addr_octet[3], addr.ether_addr_octet[4], addr.ether_addr_octet[5]);
	return buf;
}

void Ethernet::setSource(struct ether_addr addr)
{
	packet->writeByteArray(ETH_ALEN, ETH_ALEN+ETH_ALEN, &addr);
}
void Ethernet::setDestination(struct ether_addr addr)
{
	packet->writeByteArray(0, ETH_ALEN, &addr);
}

u_int16_t Ethernet::getProtocol()
{
	uint16_t proto = 0;
	packet->readByteArray(ETH_ALEN+ETH_ALEN, ETH_ALEN+ETH_ALEN+2, &proto);
	proto = ntohs(proto);
	return proto;
}
void Ethernet::setProtocol(u_int16_t protocol_host)
{
	uint16_t proto = htons(protocol_host);
	packet->writeByteArray(ETH_ALEN+ETH_ALEN, ETH_ALEN+ETH_ALEN+2, &proto);
}

