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
	close(termEventFD);
	close(addUserEventFD);
	close(delUserEventFD);
}

void Gateway::serve(void)
{
	Packet inPacket(MTU);
	Packet outPacket(MTU);


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

			printf("in read %d\n", readLen);

			inPacket.setLength(readLen);
			inPacket.readByteArray(0, sizeof(header), &header);

			int written = outDev->writePacket(inPacket.memory, inPacket.getLength());
			if(written > 0) printf("in written %d\n", written);
		}


		if(inBurst == 0 && outBurst == 0);
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
