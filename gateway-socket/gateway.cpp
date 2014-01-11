/*
 * gateway.cpp
 *
 *  Created on: 2014. 1. 11.
 *      Author: leeopop
 */

#include "gateway.hh"
#include "packet.hh"

#include <sys/eventfd.h>
#include <event.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <memory.h>

static void terminate_gateway(int fd, short what, void *arg)
{
	printf("recv termination event.\n");
	fflush(0);
	eventfd_t v;
	eventfd_read(fd, &v);
	struct event_base* evbase = (struct event_base*) arg;
	event_base_loopexit(evbase, NULL);
}

Gateway::Gateway(const char* inDev, const char* outDev)
{
	this->inDev = new Device(inDev);
	this->outDev = new Device(outDev);


	termEventFD = eventfd(0,0);
	if(termEventFD < 0)
	{
		printf("cannot create term event\n");
		exit(1);
	}
	addUserEventFD = eventfd(0,0);
	if(addUserEventFD < 0)
	{
		printf("cannot create add event\n");
		exit(1);
	}
	delUserEventFD = eventfd(0,0);
	if(delUserEventFD < 0)
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
}

Gateway::~Gateway()
{
	delete inDev;
	delete outDev;
	close(termEventFD);
	close(addUserEventFD);
	close(delUserEventFD);
}

void Gateway::serve(void)
{
	Packet inPacket(MTU);
	Packet outPacket(MTU);

	const unsigned char broadcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
	const unsigned char me[6] = {0x90,0x2B,0x34,0x58,0xE4,0x15};


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

			inPacket.setLength(readLen);
			inPacket.readByteArray(0, sizeof(header), &header);


			int written = outDev->writePacket(inPacket.memory, inPacket.getLength());

			printf("gateway: %02X:%02X:%02X:%02X:%02X:%02X (in) => %02X:%02X:%02X:%02X:%02X:%02X (out) : %d bytes\n",
								header.ether_shost[0], header.ether_shost[1], header.ether_shost[2],
								header.ether_shost[3], header.ether_shost[4], header.ether_shost[5],
								header.ether_dhost[0], header.ether_dhost[1], header.ether_dhost[2],
								header.ether_dhost[3], header.ether_dhost[4], header.ether_dhost[5],
								written);

		}

		for(outBurst = 0; outBurst < IO_BURST; outBurst++)
		{
			int readLen = outDev->readPacket(outPacket.memory,MTU);
			if(readLen == -1)
				break;
			if(readLen > MTU)
				continue;

			struct ether_header header;
			if(readLen < (int)sizeof(header))
				continue;

			outPacket.setLength(readLen);
			outPacket.readByteArray(0, sizeof(header), &header);
			if(memcmp(header.ether_shost, me, 6) == 0)
				continue;

			int written = inDev->writePacket(outPacket.memory, outPacket.getLength());

			printf("gateway: %02X:%02X:%02X:%02X:%02X:%02X (out) => %02X:%02X:%02X:%02X:%02X:%02X (in) : %d bytes\n",
					header.ether_shost[0], header.ether_shost[1], header.ether_shost[2],
					header.ether_shost[3], header.ether_shost[4], header.ether_shost[5],
					header.ether_dhost[0], header.ether_dhost[1], header.ether_dhost[2],
					header.ether_dhost[3], header.ether_dhost[4], header.ether_dhost[5],
					written);
		}


		if((inBurst == 0) && (outBurst == 0))
			usleep(1);
		event_base_loop(evbase, EVLOOP_NONBLOCK);
	}
}

int Gateway::getTermFD()
{
	return termEventFD;
}
int Gateway::getAddFD()
{
	return addUserEventFD;
}
int Gateway::getDelFD()
{
	return delUserEventFD;
}
