/*
 * gateway.cpp
 *
 *  Created on: 2014. 1. 11.
 *      Author: leeopop
 */

#include "socket.hh"
#include "packet.hh"
#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>

using namespace std;

int main()
{
	Packet temp(1024);

	int data = htonl(43214343);
	temp.setData(&data, sizeof(int));

	for(int k=0; k<32; k++)
		cout<<temp.readBit(k);
	return 0;
}
