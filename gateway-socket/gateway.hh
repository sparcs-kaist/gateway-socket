/*
 * gateway.hh
 *
 *  Created on: 2014. 1. 12.
 *      Author: leeopop
 */

#ifndef GATEWAY_HH_
#define GATEWAY_HH_

#include "socket.hh"
#define MTU 1500
#define IO_BURST 32

class Gateway
{
private:
	Device *inDev;
	Device *outDev;
	struct event_base* evbase;

	int termEventFD;
	int addUserEventFD;
	int delUserEventFD;
public:
	Gateway(const char* inDev, const char* outDev);
	~Gateway();

	void serve(void);

	int getTermFD();
	int getAddFD();
	int getDelFD();
};


#endif /* GATEWAY_HH_ */
