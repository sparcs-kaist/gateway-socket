#include "udp.hh"
#include "ethernet.hh"
#include "ip.hh"

UDP::UDP(Packet *packet, int offset)
{
	this->packet = packet;
	this->offset = offset;
}

uint16_t UDP::getSource()
{
	uint16_t ret;
	packet->readByteArray(offset+0, offset+2, &ret);
	return ret;
}

uint16_t UDP::getDestination()
{
	uint16_t ret;
	packet->readByteArray(offset+2, offset+4, &ret);
	return ret;
}

void UDP::setSource(uint16_t src)
{
	uint16_t tmp;
	tmp = htons(src);
	packet->writeByteArray(offset+0, offset+2, &tmp);
}

void UDP::setDestination(uint16_t dest)
{
	uint16_t tmp;
	tmp = htons(dest);
	packet->writeByteArray(offset+2, offset+4, &tmp);
}

int UDP::getNextOffset()
{
	return this->offset+sizeof(struct udphdr);
}

void UDP::makePacket(Packet *packet, struct ether_addr ether_src, struct ether_addr ether_dst,
	                       struct in_addr ip_src, struct in_addr ip_dst, uint16_t p_src,
	                       uint16_t p_dst, void *data, size_t data_len)
{
	packet = new Packet(sizeof(struct ether_header)+
	                    sizeof(struct ip)+sizeof(struct pseudoheader)+sizeof(struct udphdr)+data_len);
	int offset = 0;

	/* START OF ETHERNET */
	Ethernet *ethhdr = new Ethernet(packet, offset);
	ethhdr->setSource(ether_src);
	ethhdr->setDestination(ether_dst);
	ethhdr->setProtocol(ETHERTYPE_IP);
	offset = ethhdr->getNextOffset();
	/* END OF ETHERNET */

	/* START OF IP */
	IP *iphdr = new IP(packet, offset);
	struct ip thdr;
	thdr.ip_hl = 5;
	thdr.ip_v = 4;
	thdr.ip_tos = 0;
	thdr.ip_len = htons(sizeof(struct ip)+sizeof(struct udphdr)+data_len);
	thdr.ip_id = 0;
	thdr.ip_off = 0;
	thdr.ip_ttl = 64;
	thdr.ip_p = IPPROTO_UDP;
	thdr.ip_sum = 0;
	thdr.ip_src = ip_src;
	thdr.ip_dst = ip_dst;

	const uint16_t *pointer = (const uint16_t *) (&thdr);
	uint32_t sum = 0;
	while((void *) pointer < (void *) (&thdr+1))
	{
		sum += ntohs(*pointer);
		pointer++;
	}
	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	thdr.ip_sum = htons((uint16_t) sum);

	packet->writeByteArray(offset+0, offset+sizeof(struct ip), &thdr);
	offset = offset+sizeof(struct ip);
	/* END OF IP */

	/* START OF UDP & DATA */
	struct pseudoheader pheader;
	pheader.src = iphdr->getSource();
	pheader.dst = iphdr->getDestination();
	pheader.zero = 0;
	pheader.proto = IPPROTO_UDP;
	pheader.tot_len = htons(packet->getLength()-offset);

	UDP *udphdr = new UDP(packet, offset);
	udphdr->setSource(p_src);
	udphdr->setDestination(p_dst);
	packet->writeByte(offset+6, 0);
	packet->writeByte(offset+7, 0);
	offset = udphdr->getNextOffset();

	packet->writeByteArray(udphdr->getNextOffset()+0, udphdr->getNextOffset()+data_len, data);

	uint16_t temp = 0;
	sum = 0;
	pointer = (const uint16_t *) (&pheader);
	while((void *) pointer < (void *) (&thdr+1))
	{
		sum += ntohs(*pointer);
		pointer++;
	}
	int current = offset;
	while(current < packet->getLength())
	{
		if ((current+1) == packet->getLength())
			temp = packet->readByte(current);
		else
			packet->readByteArray(current, current+2, &temp);

		sum += ntohs(temp);
		current += 2;
	}
	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	temp = htons((uint16_t) sum);
	packet->writeByteArray(offset+6, offset+8, &temp);
	/* END OF UDP & DATA*/
}
