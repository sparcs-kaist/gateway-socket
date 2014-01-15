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

int DHCP::getMessageType()
{
	return packet->readByte(offset + 242);
}

int DHCP::writeResponse(Packet* packet, int offset, bool discover, uint16_t MTU, uint32_t transaction_id, struct in_addr client_in_addr, ether_addr client_ether_addr, struct in_addr server_identifier, struct in_addr subnet_mask, struct in_addr router, std::vector<struct in_addr> dns_vector, uint32_t lease_time)
{
	int current_offset = offset;
	packet->writeByte(current_offset++, 2); // Opcode
	packet->writeByte(current_offset++, 1); // Hardware Type
	packet->writeByte(current_offset++, 6); // Hardware Address Length
	packet->writeByte(current_offset++, 0); // Hops

	packet->writeByteArray(current_offset, current_offset+4, &transaction_id); // Transaction ID
	current_offset+=4;

	for(int i=current_offset; i<current_offset + 8; i++)
		packet->writeByte(i, 0); // Number of seconds + Flags + Client IP Address
	current_offset+=8;


	packet->writeByteArray(current_offset, current_offset+4, &client_in_addr); // Your IP Address
	current_offset+=4;

	for(int i=current_offset; i<current_offset+4; i++)
		packet->writeByte(i, 0); // Server IP Address + Gateway IP Address
	current_offset+=8;



	packet->writeByteArray(current_offset, current_offset+6, &client_ether_addr); // Client MAC Address
	current_offset += 6;
	for(int k=current_offset; k < current_offset + 10; k++)
		packet->writeByte(k, 0); // Client MAC Address
	current_offset += 10;

	for(int i=current_offset; i<current_offset + 64 + 128; i++)
		packet->writeByte(i, 0); // Server host name + Boot file name
	current_offset += 64+128;


	packet->writeByte(current_offset++, 99);
	packet->writeByte(current_offset++, 130);
	packet->writeByte(current_offset++, 83);
	packet->writeByte(current_offset++, 99); // Magic Number


	packet->writeByte(current_offset++, 53);
	packet->writeByte(current_offset++, 1);
	if(discover)
		packet->writeByte(current_offset++, 2); // DHCP Message Type (OFFER)
	else
		packet->writeByte(current_offset++, 5); // DHCP Message Type (ACK)

	packet->writeByte(current_offset++, 54);
	packet->writeByte(current_offset++, 4);

	packet->writeByteArray(current_offset, current_offset+4, &server_identifier); // DHCP Server Identifier
	current_offset += 4;

	packet->writeByte(current_offset++, 51);
	packet->writeByte(current_offset++, 4);
	packet->writeByteArray(current_offset, current_offset+4, &lease_time); // IP Address Lease Time
	current_offset+=4;

	packet->writeByte(current_offset++, 1);
	packet->writeByte(current_offset++, 4);
	packet->writeByteArray(current_offset, current_offset+4, &subnet_mask); // Subnet Mask
	current_offset+=4;

	packet->writeByte(current_offset++, 3);
	packet->writeByte(current_offset++, 4);
	packet->writeByteArray(current_offset, current_offset+4, &router); // Router
	current_offset+=4;

	packet->writeByte(current_offset++, 26);
	packet->writeByte(current_offset++, 2);

	packet->writeByteArray(current_offset, current_offset+2, &MTU); // MTU
	current_offset+=2;

	packet->writeByte(current_offset++, 6);
	packet->writeByte(current_offset++, dns_vector.size() * 4); //prepare DNS

	for(std::vector<in_addr>::iterator i = dns_vector.begin(); i != dns_vector.end(); ++i)
	{
		struct in_addr dns = *i;
		packet->writeByteArray(current_offset, current_offset+4, &dns);
		current_offset+=4;
	} // Domain Name Server
	packet->writeByte(current_offset++, 255);
	return current_offset - offset;
}
