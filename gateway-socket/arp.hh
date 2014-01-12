/*
 * arp.hh
 *
 *  Created on: 2014. 1. 13.
 *      Author: leeopop
 */

#ifndef ARP_HH_
#define ARP_HH_


#include <net/if_arp.h>
#include "packet.hh"

class ARP
{
private:
	Packet* packet;
	int offset;

public:
	ARP(Packet* packet);
	ARP(Packet* packet, int offset);
};


#endif /* ARP_HH_ */
