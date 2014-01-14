/*
 * dhcp.cpp
 *
 * Created on: 2014. 1. 15.
 *      Author : aon
 *
 */

#include "dhcp.hh"
#include "packet.hh"

DHCP::DHCP(Packet* packet)
{
	this->packet = packet;
	this->offset = 0;
}

DHCP::DHCP(Packet* packet, int offset)
{
	this->packet = packet;
	this->offset = offset;
}

char DHCP::getOpcode()
{
	return packet->readByte(offset);
}

uint32_t DHCP::getTransactionID()
{
	uint32_t ret;
	packet->readByteArray(offset+4, offset+8, &ret);
	return ret;
}

struct ether_addr DHCP::getClientMAC()
{
	struct ether_addr ret;
	packet->readByteArray(offset+28, offset+34, &ret);
	return ret;
}

int DHCP::writeResponse(Packet* packet, int offset, uint32_t transaction_id, struct in_addr client_in_addr, struct in_addr, ether_addr client_ether_addr, struct in_addr server_identifier, struct in_addr subnet_mask, struct in_addr router, std::vector<struct in_addr> dns_vector, int lease_time = 7200)
{
	packet->writeByte(offset, 2); // Opcode
	packet->writeByte(offset+1, 1); // Hardware Type
	packet->writeByte(offset+2, 6); // Hardware Address Length
	packet->writeByte(offset+3, 0); // Hops
	packet->writeByteArray(offset+4, offset+8, &transaction_id); // Transaction ID
	for(int i=8; i<16; i++)
		packet->writeByte(offset+i, 0); // Number of seconds + Flags + Client IP Address
	packet->writeByteArray(offset+16, offset+20, &client_in_addr); // Your IP Address
	for(int i=20; i<28; i++)
		packet->writeByte(offset+i, 0); // Server IP Address + Gateway IP Address
	packet->writeByteArray(offset+28, offset+34, &client_ether_addr); // Client MAC Address
	for(int i=34; i<42; i++)
		packet->writeByte(offset+i, 0); // Server host name + Boot file name
	packet->writeByte(offset+42, 99);
	packet->writeByte(offset+43, 130);
	packet->writeByte(offset+44, 83);
	packet->writeByte(offset+45, 99); // Magic Number
	packet->writeByte(offset+46, 53);
	packet->writeByte(offset+47, 1);
	packet->writeByte(offset+48, 5); // DHCP Message Type
	packet->writeByte(offset+49, 54);
	packet->writeByte(offset+50, 4);
	packet->writeByteArray(offset+51, offset+55, &server_identifier); // DHCP Server Identifier
	packet->writeByte(offset+55, 51);
	packet->writeByte(offset+56, 4);
	packet->writeByteArray(offset+57, offset+61, &lease_time); // IP Address Lease Time
	packet->writeByte(offset+61, 1);
	packet->writeByte(offset+62, 4);
	packet->writeByteArray(offset+63, offset+67, &subnet_mask); // Subnet Mask
	packet->writeByte(offset+67, 3);
	packet->writeByte(offset+68, 4);
	packet->writeByteArray(offset+69, offset+73, &router); // Router
	int tmp = 73;
	for(std::vector<in_addr>::iterator i = dns_vector.begin(); i != dns_vector.end(); ++i)
	{
		struct in_addr dns = *i;
		packet->writeByteArray(offset+tmp, offset+tmp+4, &dns);
		tmp +=4;
	} // Domain Name Server
	packet->writeByte(offset+tmp, 255);
	tmp++;
	return tmp;
}
