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


