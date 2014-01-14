/*
 * ip.hh
 *
 *  Created on: 2014. 1. 14.
 *      Author: leeopop
 */

#ifndef IP_HH_
#define IP_HH_

#include "packet.hh"
#include <netinet/in.h>

class IP
{
private:
	Packet* packet;
	int offset;
public:
	IP(Packet* packet, int offset);

	struct in_addr getSource();
	struct in_addr getDestination();

	void setSource(struct in_addr);
	void setDestination(struct in_addr);

	uint8_t getProtocol();
	void setProtocol(uint8_t);
	int getNextOffset();
};

#endif /* IP_HH_ */
