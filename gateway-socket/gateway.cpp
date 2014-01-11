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
	Device eth2("eth2");
	char buffer[64];

	for(int k=0; k<1000; k++)
	{
		int ret = eth2.readPacket(buffer, sizeof(buffer));
		printf("packet recved %d\n", ret);
		if(ret == -1)
			usleep(10);
	}
	return 0;
}
