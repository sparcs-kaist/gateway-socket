/*
 * protocol.hh
 *
 *  Created on: 2014. 1. 12.
 *      Author: leeopop
 */

#ifndef PROTOCOL_HH_
#define PROTOCOL_HH_

#include <net/ethernet.h>
#include "packet.hh"

class Ethernet
{
private:
	Packet* packet;
public:
	Ethernet(Packet* packet);

	struct ether_addr getSource();
	struct ether_addr getDestination();

	void setSource(struct ether_addr);
	void setDestination(struct ether_addr);

	u_int16_t getProtocol();
	void setProtocol(u_int16_t protocol_host);

	static char* printMAC(struct ether_addr, char* buf, int len);

	static const struct ether_addr BROADCAST_ADDR;
};


#endif /* PROTOCOL_HH_ */
