/*
 * packet.cpp
 *
 *  Created on: 2014. 1. 12.
 *      Author: leeopop
 */

#include "packet.hh"
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>

Packet::Packet(int capacity)
{
	this->writtenByte = 0;
	this->capacity = capacity;
	this->memory = (unsigned char*)malloc(capacity);
	if(memory == 0)
	{
		printf("No memory\n");
		exit(1);
	}
}

int Packet::setData(void* data, int length)
{
	if(length > capacity)
		return -1;

	writtenByte = length;
	memcpy(memory, data, length);
	return length;
}

Packet::~Packet()
{
	free(memory);
}

bool Packet::readBit(int index)
{
	if(index < 0 || index >= (capacity << 3))
	{
		printf("Out of bound\n");
		exit(1);
	}

	unsigned int byteIdx = index >> 3;
	unsigned int bitIdx = index & 7;

	unsigned char target = memory[byteIdx];
	unsigned char probe = 1 << (7-bitIdx);

	return (target & probe) > 0;
}


void Packet::readByteArray(int from, int to, void* buffer)
{
	memcpy(buffer, memory+from, to-from);
}
void Packet::writeByteArray(int from, int to, const void* buffer)
{
	memcpy(memory+from, buffer, to-from);
}

int Packet::readByte(int index)
{
	return memory[index];
}
void Packet::writeByte(int index, uint8_t data)
{
	memory[index] = data;
}

