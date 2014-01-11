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
#include <queue>

class Packet
{
private:
	int writtenByte;
	int capacity;
	unsigned char* memory;


	void setLength(int length);
public:


	Packet(int capacity);
	~Packet();

	bool readBit(int index);
	void writeBit(int index, bool flag);

	int readByte(int index);
	void writeByte(int index, uint8_t data);

	void readByteArray(int from, int to, void* buffer);
	void writeByteArray(int from, int to, const void* buffer);


	int getLength();
	int getCapacity();
	int setData(void* data, int length);

	friend class Gateway;
};

#endif /* PACKET_HH_ */
