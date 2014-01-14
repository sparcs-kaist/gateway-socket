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

class Database
{
private:
	sql::Driver* driver;
	sql::Connection* conn;

	sql::PreparedStatement* selectMACwithIP;
	sql::PreparedStatement* selectAllIP;
	sql::PreparedStatement* selectUserIPfromMAC;
	sql::PreparedStatement* updateAccessTime;

public:
	Database(const char* host, const char* userName, const char* passwd, const char* dbName);
	~Database();

	std::vector< std::pair<struct in_addr, struct ether_addr> > getAllStaticIP();
};


#endif /* DATABASE_HH_ */
