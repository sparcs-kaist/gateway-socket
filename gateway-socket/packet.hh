/*
 * packet.hh
 *
 *  Created on: 2014. 1. 12.
 *      Author: leeopop
 */

#ifndef PACKET_HH_
#define PACKET_HH_

#include <stdint.h>
#include "socket.hh"

class PacketPool
{

};

class Packet
{
private:
	int writtenByte;
	int capacity;
	char* memory;


	void setLength(int length);
public:


	Packet(int capacity);
	~Packet();

	bool readBit(int index);
	void writeBit(int index, bool flag);

	int readByte(int index);
	int writeByte(int index, uint8_t data);

	int readByteArray(int from, int to, void* buffer);
	int writeByteArray(int from, int to, const void* buffer);


	int getLength();
	int getCapacity();
	int setData(void* data, int length);

	friend class Device;
};


#endif /* PACKET_HH_ */
