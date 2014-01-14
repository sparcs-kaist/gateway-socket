/*
 * database.cpp
 *
 *  Created on: 2014. 1. 15.
 *      Author: leeopop
 */


#include "database.hh"
#include "ethernet.hh"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <stdio.h>
#include <vector>
#include <arpa/inet.h>

using namespace std;
using namespace sql;

Database::Database(const char* host, const char* userName, const char* passwd, const char* dbName)
{
	driver = 0;
	conn = 0;
	selectMACwithIP = 0;
	selectAllIP = 0;
	selectUserIPfromMAC = 0;
	updateAccessTime = 0;
	try
	{
		driver = get_driver_instance();
		conn = driver->connect(host, userName, passwd);

		conn->setSchema(dbName);

		selectMACwithIP = conn->prepareStatement("SELECT `mac` FROM 'static_ip' WHERE `ip` = ?");
		selectAllIP = conn->prepareStatement("SELECT * FROM `static_ip`");
		selectUserIPfromMAC = conn->prepareStatement("SELECT `ip` FROM 'user' WHERE `mac` = ?");
		updateAccessTime = conn->prepareStatement("UPDATE `user` SET `accessed` = NOW() WHERE `mac` = ?");
	}
	catch(sql::SQLException &e)
	{
		printf("SQL error on creating database(%d): %s\n", e.getErrorCode(), e.getSQLStateCStr());
		exit(1);
	}
}

Database::~Database()
{
	conn->close();
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
	return ret;
}
