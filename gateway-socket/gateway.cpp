/*
 * gateway.cpp
 *
 *  Created on: 2014. 1. 11.
 *      Author: leeopop
 */

#include "gateway.hh"

#include <sys/eventfd.h>
#include <event.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

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
	while(!event_base_got_exit(evbase))
	{
		sleep(1);
		printf("sleep\n");
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
