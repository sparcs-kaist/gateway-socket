/*
 * gateway.cpp
 *
 *  Created on: 2014. 1. 11.
 *      Author: leeopop
 */

#include "gateway.hh"
#include "packet.hh"
#include "ethernet.hh"

#include <sys/eventfd.h>
#include <event.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <memory.h>
#include <queue>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace std;
using namespace boost;

typedef unordered_map<uint32_t,struct ether_addr> StaticIPMap;

static void terminate_gateway(int fd, short what, void *arg)
{
	printf("recv termination event.\n");
	fflush(0);
	eventfd_t v;
	eventfd_read(fd, &v);
	struct event_base* evbase = (struct event_base*) arg;
	event_base_loopexit(evbase, NULL);
}

void Gateway::add_static_ip(int fd, short what, void *arg)
{
	Gateway* gateway = (Gateway*)arg;
	pthread_mutex_lock(&gateway->lock);
	eventfd_t v;
	eventfd_read(fd, &v);
	pair<struct in_addr, struct ether_addr> data = gateway->staticIPAddRequest.front();
	gateway->staticIPAddRequest.pop();

	if(gateway->staticIPMap.size() > 0)
		gateway->staticIPMap.erase(data.first.s_addr);
	gateway->staticIPMap.insert(pair<uint32_t, struct ether_addr>(data.first.s_addr, data.second));

	char ip_str[32];
	char mac_str[32];
	inet_ntop(AF_INET, &data.first, ip_str, sizeof(ip_str));
	Ethernet::printMAC(data.second, mac_str, sizeof(mac_str));
	printf("Inserted new static IP (%s) for MAC (%s)\n", ip_str, mac_str);
	pthread_mutex_unlock(&gateway->lock);
}

void Gateway::del_static_ip(int fd, short what, void *arg)
{
	Gateway* gateway = (Gateway*)arg;
	pthread_mutex_lock(&gateway->lock);
	eventfd_t v;
	eventfd_read(fd, &v);
	struct in_addr key = gateway->staticIPDelRequest.front();
	gateway->staticIPDelRequest.pop();

	int result = gateway->staticIPMap.erase(key.s_addr);
	char ip_str[32];
	inet_ntop(AF_INET, &key, ip_str, sizeof(ip_str));
	if(result)
		printf("Removed static IP (%s)\n", ip_str);
	else
		printf("Cannot find static IP (%s) for removal\n", ip_str);
	pthread_mutex_unlock(&gateway->lock);
}

Gateway::Gateway(const char* inDev, const char* outDev)
{

	this->inDev = new Device(inDev);
	this->outDev = new Device(outDev);

	pthread_mutex_init(&this->lock, NULL);

	termEventFD = eventfd(0,0);
	if(termEventFD < 0)
	{
		printf("cannot create term event\n");
		exit(1);
	}
	addStaticIPEventFD = eventfd(0,0);
	if(addStaticIPEventFD < 0)
	{
		printf("cannot create add event\n");
		exit(1);
	}
	delStaticIPEventFD = eventfd(0,0);
	if(delStaticIPEventFD < 0)
	{
		printf("cannot create del event\n");
		exit(1);
	}


	evbase = event_base_new();
	if(evbase == 0)
		exit(1);

	struct event *term_event = event_new(evbase, termEventFD,
			EV_READ, terminate_gateway, evbase);
	event_add(term_event, NULL);

	struct event *add_static_event = event_new(evbase, addStaticIPEventFD,
			EV_READ | EV_PERSIST, Gateway::add_static_ip, this);
	event_add(add_static_event, NULL);
	struct event *del_static_event = event_new(evbase, delStaticIPEventFD,
			EV_READ | EV_PERSIST, Gateway::del_static_ip, this);
	event_add(del_static_event, NULL);
}

Gateway::~Gateway()
{
	pthread_mutex_destroy(&this->lock);
	delete inDev;
	delete outDev;
	close(termEventFD);
	close(addStaticIPEventFD);
	close(delStaticIPEventFD);
}

void Gateway::serve(void)
{
	Packet inPacket(MTU);
	Packet outPacket(MTU);

	const unsigned char broadcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};


	while(!event_base_got_exit(evbase))
	{
		int inBurst, outBurst;
		for(inBurst = 0; inBurst < IO_BURST; inBurst++)
		{
			int readLen = inDev->readPacket(inPacket.memory,MTU);
			if(readLen == -1)
				break;
			if(readLen > MTU)
				continue;

			struct ether_header header;
			if(readLen < (int)sizeof(header))
				continue;

			Packet* packet = &inPacket;
			Ethernet ethernet(packet);
			if(ethernet.getProtocol() == ETHERTYPE_ARP)
			{
				ARP arp(packet, ethernet.getNextOffset());
				struct arphdr arp_header = arp.getHeader();

				if (! (
					(arp_header.ar_hrd == ARPHRD_ETHER) &&
					(arp_header.ar_pro == ETHERTYPE_IP) &&
					(arp_header.ar_op == ARPOP_InREQUEST || ARPOP_InREPLY) &&
					(arp_header.ar_pln == sizeof(struct in_addr)) &&
					(arp_header.ar_hln == sizeof(struct ether_addr))
					) )
				{
					//filter out
					continue;
				}

				if(arp_header.ar_op == ARPOP_REQUEST)
				{
					struct in_addr destIP = arp.getDestinationIP();
					StaticIPMap::const_iterator static_iter
					= this->staticIPMap.find(destIP.s_addr);

					if(static_iter != staticIPMap.end())
					{
						continue;
					}
				}

				else if(arp_header.ar_op == ARPOP_REPLY)
				{
					struct in_addr srcIP = arp.getSourceIP();
					StaticIPMap::const_iterator static_iter
					= this->staticIPMap.find(srcIP.s_addr);

					if(static_iter != staticIPMap.end())
					{
						arp.setSourceMAC(static_iter->second);
						ethernet.setSource(static_iter->second);
					}
				}
			}
			else if(ethernet.getProtocol() == ETHERTYPE_IP)
			{

			}


			outDev->writePacket(inPacket.memory, inPacket.getLength());
		}

		for(outBurst = 0; outBurst < IO_BURST; outBurst++)
		{
			int readLen = outDev->readPacket(outPacket.memory,MTU);
			if(readLen == -1)
				break;
			if(readLen > MTU)
				continue;

			Packet* packet = &inPacket;
			Ethernet ethernet(packet);
			if(ethernet.getProtocol() == ETHERTYPE_ARP)
			{
				ARP arp(packet, ethernet.getNextOffset());
				struct arphdr arp_header = arp.getHeader();

				if (! (
						(arp_header.ar_hrd == ARPHRD_ETHER) &&
						(arp_header.ar_pro == ETHERTYPE_IP) &&
						(arp_header.ar_op == ARPOP_InREQUEST || ARPOP_InREPLY) &&
						(arp_header.ar_pln == sizeof(struct in_addr)) &&
						(arp_header.ar_hln == sizeof(struct ether_addr))
				) )
				{
					//filter out
					continue;
				}

				if(arp_header.ar_op == ARPOP_REPLY)
				{
					struct in_addr srcIP = arp.getSourceIP();
					StaticIPMap::const_iterator static_iter
					= this->staticIPMap.find(srcIP.s_addr);

					if(static_iter != staticIPMap.end())
					{
						//someone is using our IP
						continue;
					}
				}
			}
			else if(ethernet.getProtocol() == ETHERTYPE_IP)
			{

			}

			inDev->writePacket(outPacket.memory, outPacket.getLength());
		}


		if((inBurst == 0) && (outBurst == 0))
			usleep(1);
		event_base_loop(evbase, EVLOOP_NONBLOCK);
	}
}

void Gateway::terminate()
{
	eventfd_t v = 1;
	eventfd_write(this->termEventFD, v);
}

void Gateway::addStaticIP(struct in_addr ip, struct ether_addr mac)
{
	pthread_mutex_lock(&this->lock);
	this->staticIPAddRequest.push(pair<struct in_addr, struct ether_addr>(ip, mac));
	eventfd_t v = 1;
	eventfd_write(this->addStaticIPEventFD, v);
	pthread_mutex_unlock(&this->lock);
}

void Gateway::delStaticIP(struct in_addr ip)
{
	pthread_mutex_lock(&this->lock);
	this->staticIPDelRequest.push(ip);
	eventfd_t v = 1;
	eventfd_write(this->addStaticIPEventFD, v);
	pthread_mutex_unlock(&this->lock);
}
