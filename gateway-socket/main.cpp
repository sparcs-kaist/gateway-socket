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
#include <arpa/inet.h>
#include <iostream>
#include <signal.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <syslog.h>

#define MYSQL_RETRY_COUNT 30
#define MYSQL_FINAL_WAIT 5

using namespace std;

static Gateway* gateway = 0;
static Database *db = 0;

static void exit_handle(int num)
{
	syslog(LOG_INFO, "Exiting...\n");
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
			"--gateway [gateway IP]\n"
			"--subnet [subnet IP]\n"
			"--daemon -d daemonize\n"
			"--dns [dns IP] [--dns another]\n"
			"--timeout -t [timeout(milli)]\n"
			"--help\n");
	exit(1);
}

#define DAEMON_NAME "gateway"

int main(int argc, char** argv)
{
	static bool daemonize = false;
	pid_t pid, sid;

	int neccessary = 7;
	int timeout = 10000;
	char in_dev[IFNAMSIZ];
	char out_dev[IFNAMSIZ];
	signal(SIGINT, exit_handle);
	char db_user[255];
	char db_pass[255];
	char db_name[255];
	strncpy(db_name, "gateway", sizeof(db_name));

	static int long_opt;
	int option_index;

	struct in_addr gatewayIP = {0};
	struct in_addr subnetMask = {0};
	vector<struct in_addr> dnsList;

	static struct option long_options[] =
	{
			{"inside", required_argument,        0, 'i'},
			{"outside", required_argument,       0, 'o'},
			{"db-user",   required_argument,       0, 'u'},
			{"db-pass",   required_argument,       0, 'p'},
			{"db-name",   required_argument,       0, 'n'},
			{"timeout",   required_argument,       0, 't'},
			{"daemon",   no_argument,       0, 'd'},
			{"gateway",   required_argument,       &long_opt, 1},
			{"subnet",   required_argument,       &long_opt, 2},
			{"dns",   required_argument,       &long_opt, 3},
			{0, 0, 0, 0}
	};


	while (1)
	{
		long_opt = 0;
		int c = getopt_long (argc, argv, "u:p:i:o:n:t:d",
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
			case 'd':
			{
				daemonize = true;
				break;
			}
			case 't':
			{
				timeout = atoi(optarg);
				if(timeout < 0)
					timeout = 10000;
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

	syslog(LOG_INFO, "%s daemon starting up", DAEMON_NAME);

	setlogmask(LOG_UPTO(LOG_INFO));
	openlog(DAEMON_NAME, LOG_CONS | LOG_PERROR, LOG_USER);

	if(daemonize)
	{
		printf("starting the daemonizing process\n");

		pid = fork();
		if (pid < 0)
		{
			exit(1);
		}

		if (pid > 0)
		{
			printf("daemon pid: %d\n", pid);
			exit(0);
		}

		umask(0);

	}

	if(daemonize)
	{
		sid = setsid();
		if (sid < 0)
		{
			exit(1);
		}

		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
	}


	{
		int fail = 0;
		for(fail = 0; fail < MYSQL_RETRY_COUNT; fail++)
		{
			FILE* mysql_pid = fopen("/var/run/mysqld/mysqld.pid","r");
			if(mysql_pid == 0)
			{
				int count = fscanf(mysql_pid, "%d", &pid);
				if(count != 1 && pid <= 0)
				{
					syslog(LOG_INFO, "mysqld at pid %d is found", pid);
					fclose(mysql_pid);
					break;
				}
			}
			syslog(LOG_WARNING, "cannot find mysqld (try %d/%d)", fail+1, MYSQL_RETRY_COUNT);
			sleep(1);
			if(mysql_pid != 0)
				fclose(mysql_pid);
		}
		if(fail >= MYSQL_RETRY_COUNT)
		{
			syslog(LOG_ERR, "cannot find mysqld on /var/run/mysqld/mysqld.pid");
			exit(1);
		}
		else
		{
			syslog(LOG_INFO, "wait %d more seconds to complete the job", MYSQL_FINAL_WAIT);
			sleep(MYSQL_FINAL_WAIT);
		}
	}

	db = new Database("localhost", db_user, db_pass, db_name, gatewayIP, subnetMask, dnsList, timeout);
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

	syslog(LOG_INFO, "%s daemon exiting", DAEMON_NAME);
	return 0;
}
