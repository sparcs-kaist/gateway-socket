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
#include <sys/socket.h>

Device::Device(const char* devName)
{
	sock = -1;
	devID = -1;
	alreadyPromisc = true;


	sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
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
		if(1)
		{
			request.ifr_flags &= ~IFF_PROMISC;
			if(ioctl(sock, SIOCSIFFLAGS, &request) < 0)
			{
				perror ("cannot set dev");
				close(sock);
				exit(1);
			}
		}
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
	devID = request.ifr_ifindex;

	struct sockaddr_ll eth;
	memset(&eth, 0, sizeof(eth));

	//For bind only sll_protocol and sll_ifindex are used.
	printf("dev %d\n", devID);
	eth.sll_family = AF_PACKET;
	eth.sll_protocol = htons(ETH_P_ALL);
	eth.sll_ifindex = devID;
	if(bind(sock, (struct sockaddr*)&eth, sizeof(eth)) < 0)
	{
		perror ("cannot bind dev");
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

int Device::readPacket(void* buffer, int length)
{
	return recv(sock, buffer, length, MSG_DONTWAIT | MSG_TRUNC);
}

int Device::writePacket(const void* buffer, int length)
{
	return send(sock, buffer, length, MSG_DONTWAIT);
}
