/*
 * udp.hh
 *
 * Created on 2014.01.15
 *	Author : overmania
 */

#ifndef UDP_HH_
#define UDP_HH_

#include "packet.hh"
#include <netinet/udp.h>
#include <netinet/ether.h>
#include <netinet/ip.h>

struct pseudoheader
{
	struct in_addr src;
	struct in_addr dst;
	uint8_t zero;
	uint8_t proto;
	uint16_t tot_len;
};

class UDP
{
private:
	Packet *packet;
	int offset;
public:
	UDP(Packet *packet, int offset);

	uint16_t getSource();
	uint16_t getDestination();

	void setSource(uint16_t );
	void setDestination(uint16_t );

	int getNextOffset();

	static int makePacket(Packet *packet, struct ether_addr ether_src, struct ether_addr ether_dst,
	                       struct in_addr ip_src, struct in_addr ip_dst, uint16_t p_src, 
	                       uint16_t p_dst, const void *data, size_t data_len);

	const static int totalHeaderLen = sizeof(struct ether_header)+
            sizeof(struct ip)+sizeof(struct udphdr);
};

#endif /* UDP_HH_ */
