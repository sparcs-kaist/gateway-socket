/*
 * ethernet.hh
 *
 *  Created on: 2014. 1. 12.
 *      Author: leeopop
 */

#ifndef ETHERNET_HH_
#define ETHERNET_HH_

#include <net/ethernet.h>
#include "packet.hh"

class Ethernet
{
private:
	Packet* packet;
	int offset;
public:
	Ethernet(Packet* packet);
	Ethernet(Packet* packet, int offset);

	struct ether_addr getSource();
	struct ether_addr getDestination();

	void setSource(struct ether_addr);
	void setDestination(struct ether_addr);

	u_int16_t getProtocol();
	void setProtocol(u_int16_t protocol_host);
	int getNextOffset();

	static char* printMAC(struct ether_addr, char* buf, int len);

	static const struct ether_addr BROADCAST_ADDR;
};


#endif /* ETHERNET_HH_ */
