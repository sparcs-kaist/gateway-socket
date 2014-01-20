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
#include <syslog.h>

using namespace std;
using namespace sql;

static void terminate_database(int fd, short what, void *arg)
{
	syslog(LOG_INFO, "Terminating database...");
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
	selectAllUser = 0;
	selectUserIPfromMAC = 0;
	updateAccessTime = 0;
	selectUnusedDynamicIP = 0;
	selectUsingDynamicIP = 0;

	deleteDynamic = 0;
	insertDynamic = 0;

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
		selectAllUser = conn->prepareStatement("SELECT * FROM `user` WHERE `ip` IS NOT NULL");
		selectUserIPfromMAC = conn->prepareStatement("SELECT `ip` FROM `user` WHERE `mac` = ?");
		updateAccessTime = conn->prepareStatement("UPDATE `user` SET `accessed` = ? WHERE `mac` = ?");

		selectUnusedDynamicIP = conn->prepareStatement("SELECT `S`.`ip` FROM `static_ip` AS `S` WHERE `S`.isStatic = FALSE AND (SELECT COUNT(`ip`) FROM `user` AS `U` WHERE `U`.`ip` = `S`.`ip`) = 0");
		selectUsingDynamicIP = conn->prepareStatement("SELECT `S`.`ip` FROM `static_ip` AS `S`, `user` AS `U` WHERE (`S`.isStatic = FALSE) AND (`S`.`ip` = `U`.`ip`) AND (`U`.`accessed` < ?) ORDER BY `U`.`accessed` ASC");

		deleteDynamic = conn->prepareStatement("UPDATE `user` SET `ip` = NULL WHERE `ip` = ?");
		insertDynamic = conn->prepareStatement("UPDATE `user` SET `ip` = ? WHERE `mac` = ?");
	}
	catch(sql::SQLException &e)
	{
		syslog(LOG_ERR, "SQL error on creating database(%d): %s", e.getErrorCode(), e.getSQLStateCStr());
		exit(1);
	}

	termEventFD = eventfd(0,0);
	if(termEventFD < 0)
	{
		syslog(LOG_ERR, "cannot create term event");
		exit(1);
	}

	createDHCPRequestFD = eventfd(0,0);
	if(createDHCPRequestFD < 0)
	{
		syslog(LOG_ERR, "cannot create dhcp event.");
		exit(1);
	}

	updateTimeFD = eventfd(0,0);
	if(updateTimeFD < 0)
	{
		syslog(LOG_ERR, "cannot update time event.");
		exit(1);
	}

	evbase = event_base_new();
	if(evbase == 0)
		exit(1);
	pthread_mutex_init(&createDHCPRequestLock, 0);
	pthread_mutex_init(&updateTimeLock, 0);

	struct event *term_event = event_new(evbase, termEventFD,
			EV_READ, terminate_database, evbase);
	event_add(term_event, NULL);

	struct event *dhcp_event = event_new(evbase, createDHCPRequestFD,
			EV_READ | EV_PERSIST, Database::create_dhcp, this);
	event_add(dhcp_event, 0);

	struct event *update_event = event_new(evbase, updateTimeFD,
			EV_READ | EV_PERSIST, Database::update_time, this);
	event_add(update_event, 0);
}

void Database::update_time(int fd, short what, void *arg)
{
	Database* database = (Database*)arg;

	vector<struct update_time> temp;
	pthread_mutex_lock(&database->updateTimeLock);
	eventfd_t v;
	eventfd_read(fd, &v);

	for(eventfd_t count = 0; count < v; count++)
	{
		temp.push_back(database->updateTimeQueue.front());
		database->updateTimeQueue.pop();
	}
	pthread_mutex_unlock(&database->updateTimeLock);

	char mac_buf[32];
	for( vector<struct update_time>::iterator iter = temp.begin();
			iter != temp.end();
			iter++
	)
	{
		struct update_time request = *iter;

		Ethernet::printMAC(request.mac, mac_buf, sizeof(mac_buf));

		int ret = 0;
		try
		{
			database->updateAccessTime->clearParameters();
			database->updateAccessTime->setUInt64(1, request.UTC);
			database->updateAccessTime->setString(2, mac_buf);
			ret = database->updateAccessTime->executeUpdate();
		}
		catch(sql::SQLException &e)
		{
			syslog(LOG_ERR, "SQL error on updating time(%d): %s", e.getErrorCode(), e.getSQLStateCStr());
		}
		if(ret == 0)
		{
			syslog(LOG_INFO, "User removed from the database, MAC(%s)", mac_buf);
			request.gateway->delUserInfo(request.ip);
		}
	}
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
		syslog(LOG_INFO, "DHCP received: MAC(%s), ID(%X)", mac_buf, ntohl(request.transID));
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
				ip_addr.s_addr = 0;
				bool found = false;
				if(ip_str.length() == 0)
				{
					//dynamic IP
					ResultSet* emptyIP = database->selectUnusedDynamicIP->executeQuery();
					if(emptyIP->first())
					{
						found = true;
						SQLString ip_str = emptyIP->getString("ip");
						ip_addr.s_addr = inet_addr(ip_str.c_str());
						syslog(LOG_INFO, "DHCP for dynamic MAC(%s), IP(%s)", mac_buf, ip_str.c_str());
					}
					else
					{
						//release used IP
						struct timeval temp_time;
						uint64_t current_time = 0;
						gettimeofday(&temp_time, 0);
						current_time = (temp_time.tv_sec * 1000) + (temp_time.tv_usec / 1000);
						current_time -= 1000* DHCP_TIMEOUT;

						database->selectUsingDynamicIP->clearParameters();
						database->selectUserIPfromMAC->setUInt64(1, current_time);
						ResultSet* usingIP = database->selectUsingDynamicIP->executeQuery();

						if(usingIP->first())
						{
							found = true;
							SQLString usingIPStr = usingIP->getString("ip");
							database->deleteDynamic->clearParameters();
							database->deleteDynamic->setString(1, usingIPStr);
							ip_addr.s_addr = inet_addr(usingIPStr->c_str());
						}
						else
						{
							syslog(LOG_INFO, "No available IP for DHCP request MAC(%s), IP(%s)", mac_buf, ip_str.c_str());
						}

						usingIP->close();
						delete usingIP;
					}
					emptyIP->close();
					delete emptyIP;
				}
				else
				{
					ip_addr.s_addr = inet_addr(ip_str.c_str());
					found = true;
				}

				if(found)
				{
					database->insertDynamic->clearParameters();
					database->insertDynamic->setString(1, ip_str);
					database->insertDynamic->setString(2, mac_buf);
					database->insertDynamic->executeUpdate();
					database->selectMACwithIP->clearParameters();
					database->selectMACwithIP->setString(1, ip_str);

					ResultSet* result2 = database->selectMACwithIP->executeQuery();
					if(result2->first())
					{
						//SQLString mac_str = result2->getString("mac");


						struct ether_addr gateway_mac = Ethernet::readMAC("78:19:f7:58:30:01");
						Packet* packet = new Packet(MY_PACKET_LEN);
						packet->setLength(MY_PACKET_LEN);

						Packet dhcp(MY_PACKET_LEN);
						dhcp.setLength(MY_PACKET_LEN - UDP::totalHeaderLen);
						int dhcpLen = DHCP::writeResponse(&dhcp, 0, request.isDiscover,
								htons(request.mtu),
								request.transID,
								ip_addr, request.mac,
								database->gatewayIP,
								database->subnetMask,
								database->gatewayIP,
								database->dnsList, DHCP_TIMEOUT);
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
						userInfo.timeout = database->timeout;
						userInfo.user_mac = request.mac;

						if(!request.isDiscover)
						{
							syslog(LOG_INFO, "DHCP accepted: MAC(%s), IP(%s)", mac_buf, ip_str.c_str());
							request.gateway->addUserInfo(userInfo);
						}
						request.gateway->sendPacketRequest(packet);
					}
					result2->close();
					delete result2;
				}
			}

			result->close();
			delete result;
		}
		catch(sql::SQLException &e)
		{
			syslog(LOG_ERR, "SQL error on creating dhcp(%d): %s", e.getErrorCode(), e.getSQLStateCStr());
			exit(1);
		}
	}
}

Database::~Database()
{
	delete selectMACwithIP;
	delete selectAllIP;
	delete selectAllUser;
	delete selectUserIPfromMAC;
	delete updateAccessTime;

	delete insertDynamic;
	delete deleteDynamic;

	delete selectUnusedDynamicIP;
	delete selectUsingDynamicIP;

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

vector< struct userInfo > Database::getAllUser(uint64_t timeout)
{
	vector< struct userInfo > ret;
	ResultSet * result = selectAllUser->executeQuery();

	while(result->next())
	{
		SQLString ip_str = result->getString("ip");
		SQLString mac_str = result->getString("mac");

		struct userInfo info;

		info.ip.s_addr = inet_addr(ip_str.c_str());
		info.user_mac = Ethernet::readMAC(mac_str.c_str());
		info.last_access = result->getInt64("accessed");
		info.timeout = timeout;

		ret.push_back(info);
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

void Database::updateTime(const struct update_time &request)
{
	pthread_mutex_lock(&this->updateTimeLock);

	updateTimeQueue.push(request);

	eventfd_t v = 1;
	eventfd_write(this->updateTimeFD, v);

	pthread_mutex_unlock(&this->updateTimeLock);
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
