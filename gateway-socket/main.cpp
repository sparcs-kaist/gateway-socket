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

static Gateway* gateway;

static void exit_handle(int num)
{
	printf("Exiting...\n");
	fflush(0);

	gateway->terminate();
	gateway = 0;
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
	gateway = new Gateway("eth1", "eth2");
	pthread_t main_thread;

	gateway->serve();
	pthread_create(&main_thread, 0, serve, gateway);
	pthread_join(main_thread, 0);

	delete gateway;
	return 0;
}
