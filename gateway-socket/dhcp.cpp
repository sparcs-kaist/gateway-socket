/*
 * dhcp.cpp
 *
 * Created on: 2014. 1. 15.
 *      Author : aon
 *
 */

#include "dhcp.hh"


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

char getOpcode()
{
    return packet->readByte(offset);
}

uint32_t getTransactionID()
{
    uint32_t ret;
    packet->readByteArray(offset+4, offset+8, &ret);
    return ret;
}

struct ether_addr getClientMAC()
{
    struct ether_addr ret;
    packet->readByteArray(offset+28, offset+32, &ret);
    return ret;
}

int writeResponse(Packet* packet, uint32_t transaction_id, in_addr client_in_addr, ether_addr client_ether_addr, in_addr server_identifier, in_addr subnet_mask, in_addr router, vector<in_addr> dns_vector);
{
    const int lease_time = 7200;
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
    packet->writeByteArray(offset+28, offset+32, &client_ether_addr); // Client MAC Address
    for(int i=32; i<40; i++)
        packet->writeByte(offset+i, 0); // Server host name + Boot file name
    packet->writeByte(offset+40, 99);
    packet->writeByte(offset+41, 130);
    packet->writeByte(offset+42, 83);
    packet->writeByte(offset+43, 99); // Magic Number
    packet->writeByte(offset+44, 53);
    packet->writeByte(offset+45, 1);
    packet->writeByte(offset+46, 5); // DHCP Message Type
    packet->writeByte(offset+47, 54);
    packet->writeByte(offset+48, 4);
    packet->writeByteArray(offset+49, offset+53, &server_identifier); // DHCP Server Identifier
    packet->writeByte(offset+53, 51);
    packet->writeByte(offset+54, 4);
    packet->writeByteArray(offset+55, offset+59, &lease_time); // IP Address Lease Time
    packet->writeByte(offset+59, 1);
    packet->writeByte(offset+60, 4);
    packet->writeByteArray(offset+61, offset+65, &subnet_mask); // Subnet Mask
    packet->writeByte(offset+65, 3);
    packet->writeByte(offset+66, 4);
    packet->writeByteArray(offset+67, offset+71, &router); // Router
    int tmp = 71;
    for(vector<in_addr>::iterator i = dns_vector.begin(); i != dns_vector.end(); ++i)
    {
        struct in_addr dns = *i;
        packet->writeByteArray(offset+tmp, offset+tmp+4, &dns);
        tmp +=4;
    } // Domain Name Server
    packet->writeByte(offset+tmp, 255);
}
