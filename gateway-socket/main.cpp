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
#include "database.hh"
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
static Database *db;

static void exit_handle(int num)
{
	printf("Exiting...\n");
	fflush(0);

	gateway->terminate();
	db->terminate();
}

static void* serve(void* arg)
{
	Gateway* gateway = (Gateway*)arg;

	gateway->serve();

	return 0;
}

static void* serve_db(void* arg)
{
	Database* database = (Database*)arg;

	database->serve();

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
	int neccessary = 7;
	char in_dev[IFNAMSIZ];
	char out_dev[IFNAMSIZ];
	signal(SIGINT, exit_handle);
	char db_user[255];
	char db_pass[255];
	char db_name[255];
	strncpy(db_name, "gateway", sizeof(db_name));

	static int long_opt;
	int option_index;

	struct in_addr gatewayIP;
	struct in_addr subnetMask;
	vector<struct in_addr> dnsList;

	static struct option long_options[] =
	{
			{"inside", required_argument,        0, 'i'},
			{"outside", required_argument,       0, 'o'},
			{"db-user",   required_argument,       0, 'u'},
			{"db-pass",   required_argument,       0, 'p'},
			{"db-name",   required_argument,       0, 'n'},
			{"gateway",   required_argument,       &long_opt, 1},
			{"subnet",   required_argument,       &long_opt, 2},
			{"dns",   required_argument,       &long_opt, 3},
			{0, 0, 0, 0}
	};


	while (1)
	{
		long_opt = 0;
		int c = getopt_long (argc, argv, "u:p:i:o:n:",
				long_options, &option_index);

		/* Detect the end of the options. */
		if (c == -1)
			break;

		if(long_opt)
		{
			switch(long_opt)
			{
			case 1:
			{
				neccessary--;
				gatewayIP.s_addr = inet_addr(optarg);
				break;
			}
			case 2:
			{
				neccessary--;
				subnetMask.s_addr = inet_addr(optarg);
				break;
			}
			case 3:
			{
				if(dnsList.size() == 0)
					neccessary--;
				struct in_addr temp;
				temp.s_addr = inet_addr(optarg);
				dnsList.push_back(temp);
			}
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
			case 'u':
			{
				neccessary--;
				strncpy(db_user, optarg, sizeof(db_user));
				break;
			}
			case 'p':
			{
				neccessary--;
				strncpy(db_pass, optarg, sizeof(db_pass));
				break;
			}
			case 'n':
			{
				strncpy(db_name, optarg, sizeof(db_name));
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

	db = new Database("localhost", db_user, db_pass, db_name, gatewayIP, subnetMask, dnsList);
	gateway = new Gateway(in_dev, out_dev, db);


	vector< pair<struct in_addr, struct ether_addr> > allIP = db->getAllStaticIP();
	for(vector< pair<struct in_addr, struct ether_addr> >::iterator iter = allIP.begin(); iter != allIP.end(); iter++)
	{
		struct in_addr in_addr = iter->first;
		struct ether_addr mac_addr = iter->second;

		gateway->addStaticIP(in_addr, mac_addr);
	}

	pthread_t main_thread;
	pthread_t db_thread;

	pthread_create(&db_thread, 0, serve_db, db);
	pthread_create(&main_thread, 0, serve, gateway);

	pthread_join(main_thread, 0);
	pthread_join(db_thread, 0);

	delete gateway;
	delete db;
	return 0;
}
