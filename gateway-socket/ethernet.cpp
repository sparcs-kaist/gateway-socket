/*
 * protocol.cpp
 *
 *  Created on: 2014. 1. 12.
 *      Author: leeopop
 */

#include "ethernet.hh"
#include <stdio.h>
#include <arpa/inet.h>
#include <memory.h>

Ethernet::Ethernet(Packet* packet)
{
	this->packet = packet;
	this->offset = 0;
}

Ethernet::Ethernet(Packet* packet, int offset)
{
	this->packet = packet;
	this->offset = offset;
}

struct ether_addr Ethernet::getSource()
{
	struct ether_addr ret;
	packet->readByteArray(offset + ETH_ALEN, offset + ETH_ALEN+ETH_ALEN, &ret);
	return ret;
}
struct ether_addr Ethernet::getDestination()
{
	struct ether_addr ret;
	packet->readByteArray(offset, offset + ETH_ALEN, &ret);
	return ret;
}

char* Ethernet::printMAC(struct ether_addr addr, char* buf, int len)
{
	snprintf(buf, len, "%02X:%02X:%02X:%02X:%02X:%02X",
			addr.ether_addr_octet[0], addr.ether_addr_octet[1], addr.ether_addr_octet[2],
			addr.ether_addr_octet[3], addr.ether_addr_octet[4], addr.ether_addr_octet[5]);
	return buf;
}

static int hex_to_byte(unsigned char hex)
{
	if(hex >= 'a' && hex <= 'f')
		return hex + 10 - 'a';
	if(hex >= 'A' && hex <= 'F')
		return hex + 10 - 'A';
	if(hex >= '0' && hex <= '9')
		return hex - '0';
	return -1;
}

struct ether_addr Ethernet::readMAC(const char* buf)
{
	struct ether_addr temp;
	memset(&temp, 0, sizeof(temp));
	unsigned char* start = (unsigned char*)&temp;
	unsigned char* end = start + sizeof(temp);
	bool go_next = false;
	while(start < end)
	{
		if(!go_next)
		{
			int ret = hex_to_byte(*(buf++));
			if(ret == -1)
			{
				continue;
			}
			unsigned char current = (unsigned char)ret;
			current = current << 4;
			*start |= current;
			go_next = true;
		}
		else
		{
			int ret = hex_to_byte(*(buf++));
			if(ret == -1)
			{
				continue;
			}
			unsigned char current = (unsigned char)ret;
			*start |= current;
			go_next = false;
			start++;
		}
	}
	return temp;
}

void Ethernet::setSource(struct ether_addr addr)
{
	packet->writeByteArray(offset + ETH_ALEN, offset + ETH_ALEN+ETH_ALEN, &addr);
}
void Ethernet::setDestination(struct ether_addr addr)
{
	packet->writeByteArray(offset, offset + ETH_ALEN, &addr);
}

u_int16_t Ethernet::getProtocol()
{
	uint16_t proto = 0;
	packet->readByteArray(offset + ETH_ALEN+ETH_ALEN, offset + ETH_ALEN+ETH_ALEN+4, &proto);
	proto = ntohs(proto);
	return proto;
}
void Ethernet::setProtocol(u_int16_t protocol_host)
{
	uint16_t proto = htons(protocol_host);
	packet->writeByteArray(offset + ETH_ALEN+ETH_ALEN, offset + ETH_ALEN+ETH_ALEN+4, &proto);
}

int Ethernet::getNextOffset()
{
	return offset + sizeof(struct ether_header);
}
