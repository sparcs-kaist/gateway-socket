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


void ARP::setHeader(struct arphdr header)
{
	header.ar_hrd = htons(header.ar_hrd);
	header.ar_pro = htons(header.ar_pro);
	header.ar_op = htons(header.ar_op);
	packet->writeByteArray(offset, offset+sizeof(header), &header);
}

struct arphdr ARP::getHeader()
{
	struct arphdr header = {0,};
	packet->readByteArray(offset, offset+sizeof(header), &header);

	header.ar_hrd = ntohs(header.ar_hrd);
	header.ar_pro = ntohs(header.ar_pro);
	header.ar_op = ntohs(header.ar_op);
	return header;
}


struct ether_addr ARP::getSourceMAC()
{
	struct ether_addr ret;
	int from = offset + sizeof(struct arphdr);
	int len = sizeof(struct ether_addr);
	packet->readByteArray(from, from+len, &ret);
	return ret;
}

struct ether_addr ARP::getDestinationMAC()
{
	struct ether_addr ret;
	int from = offset + sizeof(struct arphdr) + sizeof(struct in_addr) + sizeof(struct ether_addr);
	int len = sizeof(struct ether_addr);
	packet->readByteArray(from, from+len, &ret);
	return ret;
}
void ARP::setSourceMAC(struct ether_addr sourceMAC)
{
	int from = offset + sizeof(struct arphdr);
	int len = sizeof(struct ether_addr);
	packet->writeByteArray(from, from+len, &sourceMAC);
}
void ARP::setDestinationMAC(struct ether_addr destinationMAC)
{
	int from = offset + sizeof(struct arphdr) + sizeof(struct in_addr) + sizeof(struct ether_addr);
	int len = sizeof(struct ether_addr);
	packet->writeByteArray(from, from+len, &destinationMAC);
}

struct in_addr ARP::getSourceIP()
{
	struct in_addr ret;
	int from = offset + sizeof(struct arphdr) + sizeof(struct ether_addr);
	int len = sizeof(struct in_addr);
	packet->readByteArray(from, from+len, &ret);
	return ret;
}
struct in_addr ARP::getDestinationIP()
{
	struct in_addr ret;
	int from = offset + sizeof(struct arphdr) + sizeof(struct in_addr) + sizeof(struct ether_addr) + sizeof(struct ether_addr);
	int len = sizeof(struct in_addr);
	packet->readByteArray(from, from+len, &ret);
	return ret;
}
void ARP::setSourceIP(struct in_addr ip)
{
	int from = offset + sizeof(struct arphdr) + sizeof(struct ether_addr);
	int len = sizeof(struct in_addr);
	packet->writeByteArray(from, from+len, &ip);
}
void ARP::setDestinationIP(struct in_addr ip)
{
	int from = offset + sizeof(struct arphdr) + sizeof(struct in_addr) + sizeof(struct ether_addr) + sizeof(struct ether_addr);
	int len = sizeof(struct in_addr);
	packet->writeByteArray(from, from+len, &ip);
}
