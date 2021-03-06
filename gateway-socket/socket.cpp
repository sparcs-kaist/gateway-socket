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
#include "gateway.hh"
#include <syslog.h>

Device::Device(const char* devName)
{
	sock = -1;
	devID = -1;
	prevFlag = 0;


	sock = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if(sock < 0)
	{
		perror("cannot open packet socket");
		syslog(LOG_ERR, "%s: cannot open packet socket", devName);
		exit(1);
	}


	struct ifreq request;
	memset(&request, 0, sizeof(request));


	strncpy(request.ifr_name, devName, IFNAMSIZ);
	strncpy(this->devName, devName, IFNAMSIZ);

	if(ioctl(sock, SIOCGIFFLAGS, &request) < 0)
	{
		perror("cannot get dev info");
		syslog(LOG_ERR, "%s: cannot get dev info", devName);
		close(sock);
		exit(1);
	}

	prevFlag = request.ifr_flags;


	request.ifr_flags = IFF_UP | IFF_BROADCAST | IFF_NOARP | IFF_NOTRAILERS | IFF_PROMISC;
	if(ioctl(sock, SIOCSIFFLAGS, &request) < 0)
	{
		perror("cannot set dev");
		syslog(LOG_ERR, "%s: cannot set dev flag", devName);
		close(sock);
		exit(1);
	}

	request.ifr_mtu = MY_MTU;

	if (ioctl(sock, SIOCSIFMTU, &request) < 0)
	{
		perror("cannot set mtu");
		syslog(LOG_ERR, "%s: cannot set mtu", devName);
		close(sock);
		exit(1);
	}

	if (ioctl(sock, SIOCGIFMTU, &request) < 0)
	{
		perror("cannot get mtu");
		syslog(LOG_ERR, "%s: cannot get mtu", devName);
		close(sock);
		exit(1);
	}
	syslog(LOG_INFO, "MTU %d\n", request.ifr_ifindex);

	if (ioctl(sock, SIOCGIFINDEX, &request) < 0)
	{
		perror("cannot get dev index");
		syslog(LOG_ERR, "%s: cannot get dev idx", devName);
		close(sock);
		exit(1);
	}
	devID = request.ifr_ifindex;
	syslog(LOG_INFO, "DEV %d\n", request.ifr_ifindex);

	int val = 0;
	socklen_t len = sizeof(val);
	getsockopt(sock, SOL_SOCKET, SO_SNDBUF, &val, &len);
	syslog(LOG_INFO, "%s: send buf %d\n", devName, val);
	val = 0;
	len = sizeof(val);
	getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &val, &len);
	syslog(LOG_INFO, "%s: recv buf %d\n", devName, val);

	struct sockaddr_ll eth;
	memset(&eth, 0, sizeof(eth));

	//For bind only sll_protocol and sll_ifindex are used.
	eth.sll_family = AF_PACKET;
	eth.sll_ifindex = devID;

	eth.sll_protocol = htons(ETH_P_ALL);
	eth.sll_ifindex = devID;
	eth.sll_hatype = ARPHRD_ETHER;
	eth.sll_halen = ETH_ALEN;
	if(bind(sock, (struct sockaddr*)&eth, sizeof(eth)) < 0)
	{
		perror("cannot bind dev");
		syslog(LOG_ERR, "%s: cannot bind dev", devName);
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
		syslog(LOG_INFO, "recover prev setting of %s\n", request.ifr_name);
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
		eth.sll_family = AF_PACKET;
		eth.sll_protocol = htons(ETH_P_ALL);
		eth.sll_ifindex = devID;
		eth.sll_hatype = ARPHRD_ETHER;
		eth.sll_pkttype = PACKET_OTHERHOST;
		eth.sll_halen = ETH_ALEN;

		socklen_t len = sizeof(eth);
		int readByte = recvfrom(sock, buffer, length, MSG_DONTWAIT | MSG_TRUNC, (struct sockaddr*)&eth, &len);
		if(readByte < 0)
			return readByte;
		if(eth.sll_hatype != ARPHRD_ETHER)
			continue;
		if(len > sizeof(eth))
			continue;
		if(eth.sll_ifindex != this->devID)
			continue;
		if(eth.sll_pkttype == PACKET_OUTGOING)
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
	eth.sll_protocol = htons(ETH_P_802_3);
	eth.sll_ifindex = devID;
	eth.sll_hatype = ARPHRD_ETHER;
	eth.sll_pkttype = PACKET_OTHERHOST;
	eth.sll_halen = ETH_ALEN;
	memset(eth.sll_addr, 0, sizeof(eth.sll_addr));
	memcpy(eth.sll_addr, buffer, ETH_ALEN);

	return sendto(sock, buffer, length, 0, (struct sockaddr*)&eth, len);
}
