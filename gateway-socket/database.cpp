/*
 * database.cpp
 *
 *  Created on: 2014. 1. 15.
 *      Author: leeopop
 */


#include "database.hh"
#include "ethernet.hh"
#include "gateway.hh"
#include "dhcp.hh"
#include "udp.hh"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <stdio.h>
#include <vector>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/eventfd.h>

using namespace std;
using namespace sql;

static void terminate_database(int fd, short what, void *arg)
{
	printf("Terminating database...\n");
	fflush(0);
	eventfd_t v;
	eventfd_read(fd, &v);
	struct event_base* evbase = (struct event_base*) arg;
	event_base_loopexit(evbase, NULL);
}

Database::Database(const char* host, const char* userName, const char* passwd, const char* dbName,
		struct in_addr gatewayIP,
		struct in_addr subnetMask,
		const std::vector<struct in_addr> &dnsList,
		uint32_t timeout)
{
	this->timeout = timeout;
	driver = 0;
	conn = 0;
	selectMACwithIP = 0;
	selectAllIP = 0;
	selectUserIPfromMAC = 0;
	updateAccessTime = 0;

	this->gatewayIP = gatewayIP;
	this->subnetMask = subnetMask;
	this->dnsList = dnsList;
	try
	{
		driver = get_driver_instance();
		conn = driver->connect(host, userName, passwd);

		conn->setSchema(dbName);

		selectMACwithIP = conn->prepareStatement("SELECT `mac` FROM `static_ip` WHERE `ip` = ?");
		selectAllIP = conn->prepareStatement("SELECT * FROM `static_ip`");
		selectUserIPfromMAC = conn->prepareStatement("SELECT `ip` FROM `user` WHERE `mac` = ?");
		updateAccessTime = conn->prepareStatement("UPDATE `user` SET `accessed` = NOW() WHERE `mac` = ?");
	}
	catch(sql::SQLException &e)
	{
		printf("SQL error on creating database(%d): %s\n", e.getErrorCode(), e.getSQLStateCStr());
		exit(1);
	}

	termEventFD = eventfd(0,0);
	if(termEventFD < 0)
	{
		printf("cannot create term event\n");
		exit(1);
	}

	createDHCPRequestFD = eventfd(0,0);
	if(createDHCPRequestFD < 0)
	{
		printf("cannot create dhcp event.\n");
		exit(1);
	}

	evbase = event_base_new();
	if(evbase == 0)
		exit(1);
	pthread_mutex_init(&createDHCPRequestLock, 0);

	struct event *term_event = event_new(evbase, termEventFD,
			EV_READ, terminate_database, evbase);
	event_add(term_event, NULL);

	struct event *dhcp_event = event_new(evbase, createDHCPRequestFD,
			EV_READ | EV_PERSIST, Database::create_dhcp, this);
	event_add(dhcp_event, 0);
}

void Database::create_dhcp(int fd, short what, void *arg)
{
	Database* database = (Database*)arg;

	vector<struct dhcp_request> temp;

	pthread_mutex_lock(&database->createDHCPRequestLock);
	eventfd_t v;
	eventfd_read(fd, &v);

	for(eventfd_t count = 0; count < v; count++)
	{
		temp.push_back(database->createDHCPRequestQueue.front());
		database->createDHCPRequestQueue.pop();
	}
	pthread_mutex_unlock(&database->createDHCPRequestLock);

	char mac_buf[32];
	for( vector<struct dhcp_request>::iterator iter = temp.begin();
			iter != temp.end();
			iter++
			)
	{
		struct dhcp_request request = *iter;
		Ethernet::printMAC(request.mac, mac_buf, sizeof(mac_buf));
		printf("DHCP received: MAC(%s), ID(%X)\n", mac_buf, ntohl(request.transID));
		try
		{
			database->selectUserIPfromMAC->clearParameters();
			Ethernet::printMAC(request.mac, mac_buf, sizeof(mac_buf));
			database->selectUserIPfromMAC->setString(1, mac_buf);

			ResultSet* result = database->selectUserIPfromMAC->executeQuery();

			if(result->first())
			{
				SQLString ip_str = result->getString("ip");

				struct in_addr ip_addr;
				ip_addr.s_addr = inet_addr(ip_str.c_str());

				database->selectMACwithIP->clearParameters();
				database->selectMACwithIP->setString(1, ip_str);

				ResultSet* result2 = database->selectMACwithIP->executeQuery();
				if(result2->first())
				{
					SQLString mac_str = result2->getString("mac");


					struct ether_addr gateway_mac = Ethernet::readMAC(mac_str.c_str());
					Packet* packet = new Packet(MTU);
					packet->setLength(MTU);

					Packet dhcp(MTU);
					dhcp.setLength(MTU - UDP::totalHeaderLen);
					int dhcpLen = DHCP::writeResponse(&dhcp, 0, request.isDiscover,
							htons(request.mtu),
							request.transID,
							ip_addr, request.mac,
							database->gatewayIP,
							database->subnetMask,
							database->gatewayIP,
							database->dnsList, 14400);
					dhcp.setLength(dhcpLen);

					int udpLen = UDP::makePacket(packet, gateway_mac, request.mac,
							database->gatewayIP,
							ip_addr,
							67,
							68, dhcp.inMemory, dhcp.getLength());
					packet->setLength(udpLen);

					struct userInfo userInfo;
					userInfo.ip = ip_addr;
					userInfo.last_access = 0;
					userInfo.timeout = htonl(database->timeout);
					userInfo.user_mac = request.mac;

					if(!request.isDiscover)
					{
						printf("DHCP accepted: MAC(%s), IP(%s)\n", mac_buf, ip_str.c_str());
						request.gateway->addUserInfo(userInfo);
					}
					request.gateway->sendPacketRequest(packet);
				}
				result2->close();
				delete result2;
			}

			result->close();
			delete result;
		}
		catch(sql::SQLException &e)
		{
			printf("SQL error on creating dhcp(%d): %s\n", e.getErrorCode(), e.getSQLStateCStr());
			exit(1);
		}
	}
}

Database::~Database()
{
	delete selectMACwithIP;
	delete selectAllIP;
	delete selectUserIPfromMAC;
	delete updateAccessTime;

	close(createDHCPRequestFD);
	pthread_mutex_destroy(&this->createDHCPRequestLock);
	conn->close();
	delete conn;
}

vector< pair<struct in_addr, struct ether_addr> > Database::getAllStaticIP()
{
	vector< pair<struct in_addr, struct ether_addr> > ret;
	ResultSet * result = selectAllIP->executeQuery();

	while(result->next())
	{
		SQLString ip_str = result->getString("ip");
		SQLString mac_str = result->getString("mac");

		struct in_addr addr;
		addr.s_addr = inet_addr(ip_str.c_str());

		struct ether_addr mac_addr = Ethernet::readMAC(mac_str.c_str());


		ret.push_back( pair<struct in_addr, struct ether_addr> (addr, mac_addr));
	}

	result->close();
	delete result;
	return ret;
}

void Database::createDHCP(const struct dhcp_request &request)
{
	pthread_mutex_lock(&this->createDHCPRequestLock);

	createDHCPRequestQueue.push(request);

	eventfd_t v = 1;
	eventfd_write(this->createDHCPRequestFD, v);

	pthread_mutex_unlock(&this->createDHCPRequestLock);
}

void Database::serve()
{
	while(!event_base_got_exit(evbase))
		event_base_loop(evbase, 0);
}

void Database::terminate()
{
	eventfd_t v = 1;
	eventfd_write(this->termEventFD, v);
}
