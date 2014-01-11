/*
 * socket.hh
 *
 *  Created on: 2014. 1. 11.
 *      Author: leeopop
 */

#ifndef SOCKET_HH_
#define SOCKET_HH_

#include <net/if.h>

class Device
{
private:
	int sock;
	int devID;
	bool isRunning;

	bool alreadyPromisc;
	char devName[IFNAMSIZ];




public:
	Device(const char* devName);
	~Device();
};


#endif /* SOCKET_HH_ */
