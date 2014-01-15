/*
 * database.hh
 *
 *  Created on: 2014. 1. 15.
 *      Author: leeopop
 */

#ifndef DATABASE_HH_
#define DATABASE_HH_

#include <cppconn/driver.h>
#include <cppconn/statement.h>
#include <vector>
#include <netinet/ether.h>
#include <event.h>
#include <pthread.h>
#include <queue>
#include <stdint.h>

class Gateway;

struct dhcp_request
{
	struct ether_addr mac;
	uint32_t transID;
	Gateway* gateway;
};

class Database
{
private:
	sql::Driver* driver;
	sql::Connection* conn;

	sql::PreparedStatement* selectMACwithIP;
	sql::PreparedStatement* selectAllIP;
	sql::PreparedStatement* selectUserIPfromMAC;
	sql::PreparedStatement* updateAccessTime;

	struct event_base* evbase;

	int termEventFD;
	int createDHCPRequestFD;
	std::queue<struct dhcp_request> createDHCPRequestQueue;
	pthread_mutex_t createDHCPRequestLock;

	struct in_addr gatewayIP;
	struct in_addr subnetMask;
	std::vector<struct in_addr> dnsList;

	static void create_dhcp(int fd, short what, void *arg);

public:
	Database(const char* host, const char* userName, const char* passwd, const char* dbName,
			struct in_addr gatewayIP,
			struct in_addr subnetMask,
			const std::vector<struct in_addr> &dnsList);
	~Database();

	std::vector< std::pair<struct in_addr, struct ether_addr> > getAllStaticIP();

	void createDHCP(const struct dhcp_request &request);

	void terminate();

	void serve();
};


#endif /* DATABASE_HH_ */
