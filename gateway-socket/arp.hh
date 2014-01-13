/*
 * arp.hh
 *
 *  Created on: 2014. 1. 13.
 *      Author: leeopop
 */

#ifndef ARP_HH_
#define ARP_HH_


#include <net/if_arp.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include "packet.hh"

class ARP
{
private:
	Packet* packet;
	int offset;

public:
	ARP(Packet* packet);
	ARP(Packet* packet, int offset);

	struct arphdr getHeader();
	void setHeader(struct arphdr);

	struct ether_addr getSourceMAC();
	struct ether_addr getDestinationMAC();
	void setSourceMAC(struct ether_addr);
	void setDestinationMAC(struct ether_addr);

	struct in_addr getSourceIP();
	struct in_addr getDestinationIP();
	void setSourceIP(struct in_addr);
	void setDestinationIP(struct in_addr);
};


#endif /* ARP_HH_ */
