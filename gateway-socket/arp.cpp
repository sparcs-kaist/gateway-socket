/*
 * arp.cpp
 *
 *  Created on: 2014. 1. 13.
 *      Author: leeopop
 */

#include "arp.hh"
#include <arpa/inet.h>

ARP::ARP(Packet* packet)
{
	this->packet = packet;
	this->offset = 0;
}

ARP::ARP(Packet* packet, int offset)
{
	this->packet = packet;
	this->offset = offset;
}


void ARP::setHardware(uint16_t htype)
{
	uint16_t write = htons(htype);
	packet->writeByteArray(offset, offset+2, &write);
}

uint16_t ARP::getHardware()
{
	uint16_t temp = 0;
	packet->readByteArray(offset, offset+2, &temp);
	return ntohs(temp);
}
