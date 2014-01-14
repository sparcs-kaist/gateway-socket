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
	this->inMemory = (unsigned char*)malloc(capacity);
	if(inMemory == 0)
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
	memcpy(inMemory, data, length);
	return length;
}

Packet::~Packet()
{
	free(inMemory);
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

	unsigned char target = inMemory[byteIdx];
	unsigned char probe = 1 << (7-bitIdx);

	return (target & probe) > 0;
}


void Packet::readByteArray(int from, int to, void* buffer)
{
	if(from < 0 || to > this->writtenByte)
		return;
	memcpy(buffer, inMemory+from, to-from);
}
void Packet::writeByteArray(int from, int to, const void* buffer)
{
	if(from < 0 || to > this->writtenByte)
		return;
	memcpy(inMemory+from, buffer, to-from);
}

int Packet::readByte(int index)
{
	if(index < 0 || index >= this->writtenByte)
		return -1;
	return inMemory[index];
}
void Packet::writeByte(int index, uint8_t data)
{
	if(index < 0 || index >= this->writtenByte)
		return;
	inMemory[index] = data;
}

void Packet::setLength(int length)
{
	writtenByte = length;
}

int Packet::getLength()
{
	return writtenByte;
}

int Packet::getCapacity()
{
	return capacity;
}
