/*
 * gateway.hh
 *
 *  Created on: 2014. 1. 12.
 *      Author: leeopop
 */

#ifndef GATEWAY_HH_
#define GATEWAY_HH_

#include "socket.hh"
#include "arp.hh"
#include <sys/time.h>
#include <pthread.h>
#include <queue>
#include <unordered_map>

#define MTU (ETH_FRAME_LEN*2)
#define IO_BURST 128

class Gateway
{
private:
	Device *inDev;
	Device *outDev;
	struct event_base* evbase;

	int termEventFD;
	int addStaticIPEventFD;
	int delStaticIPEventFD;


	pthread_mutex_t lock;
	std::queue<std::pair<struct in_addr, struct ether_addr>> staticIPAddRequest;
	std::queue<struct in_addr> staticIPDelRequest;
	std::unordered_map<uint32_t, struct ether_addr> staticIPMap;

	static void add_static_ip(int fd, short what, void *arg);
	static void del_static_ip(int fd, short what, void *arg);

public:
	Gateway(const char* inDev, const char* outDev);
	~Gateway();

	void serve(void);

	void terminate();

	void addStaticIP(struct in_addr ip, struct ether_addr mac);
	void delStaticIP(struct in_addr ip);
};


#endif /* GATEWAY_HH_ */
