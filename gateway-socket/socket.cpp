/*
 * socket.cpp
 *
 *  Created on: 2014. 1. 11.
 *      Author: leeopop
 */

#include "socket.hh"
#include <unistd.h>
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <error.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <string.h>
#include <memory.h>
#include <stdlib.h>

Device::Device(const char* devName)
{
	sock = -1;
	devID = -1;
	alreadyPromisc = true;


	sock = socket(PF_PACKET, SOCK_PACKET, htons(ETH_P_ALL));
	if(sock < 0)
	{
		perror("cannot open packet socket");
		exit(1);
	}


	struct ifreq request;
	memset(&request, 0, sizeof(request));


	strncpy(request.ifr_name, devName, IFNAMSIZ);
	strncpy(this->devName, devName, IFNAMSIZ);

	if(ioctl(sock, SIOCGIFFLAGS, &request) < 0)
	{
		perror ("cannot get dev info");
		close(sock);
		exit(1);
	}

	if((alreadyPromisc = ((request.ifr_flags & IFF_PROMISC) > 0)))
	{
		printf("Already in promiscuous mode (%s).\n", devName);
	}
	else
	{
		request.ifr_flags |= IFF_PROMISC;
		if(ioctl(sock, SIOCSIFFLAGS, &request) < 0)
		{
			perror ("cannot set dev");
			close(sock);
			exit(1);
		}
	}


	if (ioctl(sock, SIOCGIFINDEX, &request) < 0)
	{
		perror ("cannot get dev index");
		close(sock);
		exit(1);
	}
}

Device::~Device()
{
	if(sock > 0)
	{
		if(!alreadyPromisc)
		{
			struct ifreq request;
			memset(&request, 0, sizeof(request));

			strncpy(request.ifr_name, devName, IFNAMSIZ);
			if(ioctl(sock, SIOCGIFFLAGS, &request) == 0)
			{
				request.ifr_flags &= ~IFF_PROMISC;
				ioctl(sock, SIOCSIFFLAGS, &request);
			}
		}

		close(sock);
		sock = -1;
	}
}


