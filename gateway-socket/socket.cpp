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
#include <net/if_packet.h>
#include <net/if_arp.h>
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
	prevFlag = 0;


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

	prevFlag = request.ifr_flags;


	request.ifr_flags = IFF_UP | IFF_BROADCAST | IFF_POINTOPOINT | IFF_NOTRAILERS | IFF_NOARP | IFF_PROMISC;
	if(ioctl(sock, SIOCSIFFLAGS, &request) < 0)
	{
		perror ("cannot set dev");
		close(sock);
		exit(1);
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
	eth.sll_family = AF_PACKET;
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

		struct ifreq request;
		memset(&request, 0, sizeof(request));

		strncpy(request.ifr_name, devName, IFNAMSIZ);
		printf("recover prev setting of %s\n", request.ifr_name);
		request.ifr_flags = prevFlag;
		ioctl(sock, SIOCSIFFLAGS, &request);


		close(sock);
		sock = -1;
	}
}

int Device::readPacket(void* buffer, int length)
{
	while(true)
	{
		struct sockaddr_ll eth;
		memset(&eth, 0, sizeof(eth));
		socklen_t len = sizeof(eth);
		int readByte = recvfrom(sock, buffer, length, MSG_DONTWAIT | MSG_TRUNC, (struct sockaddr*)&eth, &len);
		if(readByte < 0)
			return readByte;
		if(len > sizeof(eth))
			continue;
		if(eth.sll_ifindex != this->devID)
			continue;

		return readByte;
	}
	return -1;
}

int Device::writePacket(const void* buffer, int length)
{
	struct sockaddr_ll eth;
	memset(&eth, 0, sizeof(eth));
	socklen_t len = sizeof(eth);

	eth.sll_family = AF_PACKET;
	eth.sll_protocol = htons(ARPHRD_ETHER);
	eth.sll_ifindex = devID;
	eth.sll_hatype = ARPHRD_ETHER;
	eth.sll_pkttype = PACKET_OUTGOING;
	eth.sll_halen = ETH_ALEN;
	memcpy(eth.sll_addr, buffer, ETH_ALEN);

	return sendto(sock, buffer, length, 0, (struct sockaddr*)&eth, len);
}
