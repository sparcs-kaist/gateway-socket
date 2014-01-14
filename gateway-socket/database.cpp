/*
 * database.cpp
 *
 *  Created on: 2014. 1. 15.
 *      Author: leeopop
 */


#include "database.hh"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <stdio.h>

Database::Database(const char* host, const char* userName, const char* passwd, const char* dbName)
{
	try
	{
		driver = get_driver_instance();
		conn = driver->connect(host, userName, passwd);

		conn->setSchema(dbName);

		selectMACwithIP = conn->prepareStatement("SELECT `mac` FROM 'static_ip' WHERE `ip` = ?");
		selectAllIP = conn->prepareStatement("SELECT DISTINCT(`ip`) FROM 'static_ip'");
		selectUserIPfromMAC = conn->prepareStatement("SELECT `ip` FROM 'user' WHERE `mac` = ?");
		updateAccessTime = conn->prepareStatement("UPDATE `user` SET 'accessed' = NOW() WHERE `mac` = ?");
	}
	catch(sql::SQLException &e)
	{
		printf("SQL error on creating database(%d): %s\n", e.getErrorCode(), e.getSQLStateCStr());
		exit(1);
	}
}

