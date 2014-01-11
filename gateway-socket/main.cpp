/*
 * main.cpp
 *
 *  Created on: 2014. 1. 12.
 *      Author: leeopop
 */




#include "socket.hh"
#include "packet.hh"
#include "gateway.hh"
#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>
#include <signal.h>
#include <pthread.h>
#include <sys/eventfd.h>

using namespace std;

static int exit_event = -1;

static void exit_handle(int num)
{
	printf("Exiting...(%d)\n", exit_event);
	fflush(0);
	eventfd_t v = 1;
	eventfd_write(exit_event, v);
}

static void* serve(void* arg)
{
	Gateway* gateway = (Gateway*)arg;

	gateway->serve();

	return 0;
}

int main()
{
	signal(SIGINT, exit_handle);
	Gateway* gateway = new Gateway("eth1", "eth2");
	exit_event = gateway->getTermFD();
	pthread_t main_thread;

	pthread_create(&main_thread, 0, serve, gateway);
	pthread_join(main_thread, 0);

	delete gateway;
	return 0;
}
