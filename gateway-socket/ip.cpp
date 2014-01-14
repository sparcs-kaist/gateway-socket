/*
 * ip.cpp
 *
 *  Created on: 2014. 1. 14.
 *      Author: leeopop
 */

#include "ip.hh"

IP::IP(Packet* packet, int offset)
{
	this->packet = packet;
	this->offset = offset;
}

struct in_addr IP::getSource()
{
	struct in_addr ret;
	packet->readByteArray(offset+12, offset+16, &ret);
	return ret;
}
struct in_addr IP::getDestination()
{
	struct in_addr ret;
	packet->readByteArray(offset+16, offset+20, &ret);
	return ret;
}

void IP::setSource(struct in_addr addr)
{
	packet->writeByteArray(offset+12,offset+16, &addr);
}
void IP::setDestination(struct in_addr addr)
{
	packet->writeByteArray(offset+16,offset+20, &addr);
}

uint8_t IP::getProtocol()
{
	return packet->readByte(offset+9);
}

void IP::setProtocol(uint8_t proto)
{
	packet->writeByte(offset+9, proto);
}

int IP::getNextOffset()
{
	unsigned char hlen = packet->readByte(offset+0);
	hlen &= 0x0F;
	if(hlen < 5)
		hlen = 5;
	return hlen * 4;
}
