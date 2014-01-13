/*
 * main.cpp
 *
 *  Created on: 2014. 1. 12.
 *      Author: leeopop
 */

#include "socket.hh"
#include "packet.hh"
#include "gateway.hh"
#include "ethernet.hh"
#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>
#include <signal.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <getopt.h>
#include <string.h>
#include <stdio.h>

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

static void help(const char* name)
{
	printf("Usage : %s options\n", name);
	printf("--inside -i [inside device name]\n"
			"--outside -o [outside device name]\n"
			"--static-ip -s [config file for static IP]\n"
			"--help\n");
	exit(1);
}

int main(int argc, char** argv)
{
	int neccessary = 2;
	char in_dev[IFNAMSIZ];
	char out_dev[IFNAMSIZ];
	char static_ip_filename[256];
	static_ip_filename[0] = 0;
	signal(SIGINT, exit_handle);

	static int long_opt;
	int option_index;
	static struct option long_options[] =
	{
			{"inside", required_argument,        0, 'i'},
			{"outside", required_argument,       0, 'o'},
			{"static-ip",   required_argument,       0, 's'},
			{0, 0, 0, 0}
	};


	while (1)
	{
		long_opt = 0;
		int c = getopt_long (argc, argv, "s:i:o:",
				long_options, &option_index);

		/* Detect the end of the options. */
		if (c == -1)
			break;

		if(long_opt)
		{
			switch(long_opt)
			{
			}
		}
		else
		{
			switch (c)
			{
			case 0:
				break;

			case 'i':
			{
				neccessary--;
				strncpy(in_dev, optarg, sizeof(in_dev));
				break;
			}
			case 'o':
			{
				neccessary--;
				strncpy(out_dev, optarg, sizeof(in_dev));
				break;
			}
			case 's':
			{
				strncpy(static_ip_filename, optarg, sizeof(static_ip_filename));
				break;
			}
			default:
				help(argv[0]);
			}
		}
	}

	if(neccessary)
	{
		printf("Missing options (%d)\n", neccessary);
		exit(1);
	}

	gateway = new Gateway(in_dev, out_dev);

	if(strlen(static_ip_filename))
	{
		char ip_str[32];
		char mac_str[32];
		FILE* ip_file = fopen(static_ip_filename, "r");
		while(2 == fscanf(ip_file, "%30s %30s", ip_str, mac_str))
		{
			struct in_addr in_addr;
			if(!inet_aton(ip_str, &in_addr))
			{
				printf("Bad ip adress %s\n", ip_str);
				continue;
			}
			struct ether_addr mac_addr = Ethernet::readMAC(mac_str);
			gateway->addStaticIP(in_addr, mac_addr);
		}
		fclose(ip_file);
	}

	pthread_t main_thread;

	pthread_create(&main_thread, 0, serve, gateway);

	pthread_join(main_thread, 0);

	delete gateway;
	return 0;
}
