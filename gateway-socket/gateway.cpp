/*
 * gateway.cpp
 *
 *  Created on: 2014. 1. 11.
 *      Author: leeopop
 */

#include "gateway.hh"
#include "packet.hh"
#include "ethernet.hh"
#include "arp.hh"
#include "ip.hh"
#include "udp.hh"
#include "dhcp.hh"
#include "database.hh"

#include <sys/eventfd.h>
#include <event.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <net/ethernet.h>
#include <net/if_arp.h>
#include <memory.h>
#include <queue>
#include <arpa/inet.h>
#include <netinet/ip_icmp.h>

using namespace std;
using namespace boost;

typedef unordered_map<uint32_t,struct ether_addr> StaticIPMap;
typedef unordered_map<uint32_t,struct userInfo*> UserMap;

static void terminate_gateway(int fd, short what, void *arg)
{
	printf("Terminating gateway...\n");
	fflush(0);
	eventfd_t v;
	eventfd_read(fd, &v);
	struct event_base* evbase = (struct event_base*) arg;
	event_base_loopexit(evbase, NULL);
}

void Gateway::add_static_ip(int fd, short what, void *arg)
{
	Gateway* gateway = (Gateway*)arg;
	pthread_mutex_lock(&gateway->addStaticIPLock);
	eventfd_t v;
	eventfd_read(fd, &v);
	for(eventfd_t count = 0; count < v; count++)
	{
		pair<struct in_addr, struct ether_addr> data = gateway->staticIPAddRequest.front();
		gateway->staticIPAddRequest.pop();

		if(gateway->staticIPMap.size() > 0)
			gateway->staticIPMap.erase(data.first.s_addr);
		gateway->staticIPMap.insert(pair<uint32_t, struct ether_addr>(data.first.s_addr, data.second));

		char ip_str[32];
		char mac_str[32];
		inet_ntop(AF_INET, &data.first, ip_str, sizeof(ip_str));
		Ethernet::printMAC(data.second, mac_str, sizeof(mac_str));
		printf("Inserted new static IP (%s) for MAC (%s)\n", ip_str, mac_str);
	}
	pthread_mutex_unlock(&gateway->addStaticIPLock);
}

void Gateway::del_static_ip(int fd, short what, void *arg)
{
	Gateway* gateway = (Gateway*)arg;
	pthread_mutex_lock(&gateway->delStaticIPLock);
	eventfd_t v;
	eventfd_read(fd, &v);
	for(eventfd_t count = 0; count < v; count++)
	{
		struct in_addr key = gateway->staticIPDelRequest.front();
		gateway->staticIPDelRequest.pop();

		int result = gateway->staticIPMap.erase(key.s_addr);
		char ip_str[32];
		inet_ntop(AF_INET, &key, ip_str, sizeof(ip_str));
		if(result)
			printf("Removed static IP (%s)\n", ip_str);
		else
			printf("Cannot find static IP (%s) for removal\n", ip_str);
	}
	pthread_mutex_unlock(&gateway->delStaticIPLock);
}

void Gateway::add_user(int fd, short what, void *arg)
{
	char ip_str[32];
	char mac_str[32];
	Gateway* gateway = (Gateway*)arg;
	pthread_mutex_lock(&gateway->addUserLock);
	eventfd_t v;
	eventfd_read(fd, &v);

	for(eventfd_t count = 0; count < v; count++)
	{
		struct userInfo* info = gateway->userAddRequest.front();
		gateway->userAddRequest.pop();

		UserMap::iterator iter = gateway->userMap.find(info->ip.s_addr);
		if(iter != gateway->userMap.end())
		{
			inet_ntop(AF_INET, &iter->second->ip, ip_str, sizeof(ip_str));
			Ethernet::printMAC(iter->second->user_mac, mac_str, sizeof(mac_str));
			printf("Delete existing user IP(%s), MAC(%s)\n", ip_str, mac_str);
			free(iter->second);
			gateway->userMap.erase(iter);
		}


		inet_ntop(AF_INET, &info->ip, ip_str, sizeof(ip_str));
		Ethernet::printMAC(info->user_mac, mac_str, sizeof(mac_str));
		printf("Added user IP(%s), MAC(%s)\n", ip_str, mac_str);

		gateway->userMap.insert(pair<uint32_t, struct userInfo*>(info->ip.s_addr, info));

	}

	pthread_mutex_unlock(&gateway->addUserLock);
}
void Gateway::del_user(int fd, short what, void *arg)
{
	Gateway* gateway = (Gateway*)arg;
	pthread_mutex_lock(&gateway->delUserLock);
	eventfd_t v;
	eventfd_read(fd, &v);

	for(eventfd_t count = 0; count < v; count++)
	{
		struct in_addr ip = gateway->userDelRequest.front();
		gateway->userDelRequest.pop();

		boost::unordered_map<uint32_t, struct userInfo*>::iterator iter = gateway->userMap.find(ip.s_addr);
		if(iter != gateway->userMap.end())
		{
			struct userInfo* info = iter->second;
			char ip_str[32];
			char mac_str[32];
			inet_ntop(AF_INET, &ip, ip_str, sizeof(ip_str));

			Ethernet::printMAC(info->user_mac, mac_str, sizeof(mac_str));
			printf("Removed user IP(%s), MAC(%s)\n", ip_str, mac_str);
			free(info);

			gateway->userMap.erase(iter);
		}
	}

	pthread_mutex_unlock(&gateway->delUserLock);
}

void Gateway::send_packet(int fd, short what, void *arg)
{
	Gateway* gateway = (Gateway*)arg;
	pthread_mutex_lock(&gateway->sendPacketLock);
	eventfd_t v;
	eventfd_read(fd, &v);
	for(eventfd_t count = 0; count < v; count++)
	{
		Packet* packet = gateway->sendPacketRequestQueue.front();
		gateway->sendPacketRequestQueue.pop();

		gateway->inDev->writePacket(packet->inMemory, packet->getLength());
		delete packet;
	}
	pthread_mutex_unlock(&gateway->sendPacketLock);
}

Gateway::Gateway(const char* inDev, const char* outDev, Database* db)
{

	this->inDev = new Device(inDev);
	this->outDev = new Device(outDev);
	this->db = db;

	pthread_mutex_init(&this->addStaticIPLock, NULL);
	pthread_mutex_init(&this->delStaticIPLock, NULL);
	pthread_mutex_init(&this->addUserLock, NULL);
	pthread_mutex_init(&this->delUserLock, NULL);
	pthread_mutex_init(&this->sendPacketLock, NULL);

	termEventFD = eventfd(0,0);
	if(termEventFD < 0)
	{
		printf("cannot create term event\n");
		exit(1);
	}
	addStaticIPEventFD = eventfd(0,0);
	if(addStaticIPEventFD < 0)
	{
		printf("cannot create add event\n");
		exit(1);
	}
	delStaticIPEventFD = eventfd(0,0);
	if(delStaticIPEventFD < 0)
	{
		printf("cannot create del event\n");
		exit(1);
	}
	addUserEventFD = eventfd(0,0);
	if(addUserEventFD < 0)
	{
		printf("cannot create add user event\n");
		exit(1);
	}

	delUserEventFD = eventfd(0,0);
	if(delUserEventFD < 0)
	{
		printf("cannot create del user event\n");
		exit(1);
	}

	sendPacketFD = eventfd(0,0);
	if(sendPacketFD < 0)
	{
		printf("cannot create del user event\n");
		exit(1);
	}


	evbase = event_base_new();
	if(evbase == 0)
		exit(1);

	struct event *term_event = event_new(evbase, termEventFD,
			EV_READ, terminate_gateway, evbase);
	event_add(term_event, NULL);

	struct event *add_static_event = event_new(evbase, addStaticIPEventFD,
			EV_READ | EV_PERSIST, Gateway::add_static_ip, this);
	event_add(add_static_event, NULL);
	struct event *del_static_event = event_new(evbase, delStaticIPEventFD,
			EV_READ | EV_PERSIST, Gateway::del_static_ip, this);
	event_add(del_static_event, NULL);

	struct event *add_user_event = event_new(evbase, addUserEventFD,
			EV_READ | EV_PERSIST, Gateway::add_user, this);
	event_add(add_user_event, NULL);

	struct event *del_user_event = event_new(evbase, delUserEventFD,
			EV_READ | EV_PERSIST, Gateway::del_user, this);
	event_add(del_user_event, NULL);

	struct event *send_packet_event = event_new(evbase, sendPacketFD,
			EV_READ | EV_PERSIST, Gateway::send_packet, this);
	event_add(send_packet_event, NULL);
}

Gateway::~Gateway()
{
	pthread_mutex_destroy(&this->addStaticIPLock);
	pthread_mutex_destroy(&this->delStaticIPLock);
	pthread_mutex_destroy(&this->addUserLock);
	pthread_mutex_destroy(&this->delUserLock);
	pthread_mutex_destroy(&this->sendPacketLock);
	delete inDev;
	delete outDev;
	close(termEventFD);
	close(addStaticIPEventFD);
	close(delStaticIPEventFD);
	close(addUserEventFD);
	close(delUserEventFD);
	close(sendPacketFD);
}

static uint16_t one_compl(const void* start, const int len)
{
	const uint16_t *pointer = (const uint16_t*)start;
	const void* end = (const char*)start + len;
	uint32_t sum = 0;
	while((const void *) pointer < end)
	{
		sum += ntohs(*pointer);
		pointer++;
	}
	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	return htons(~((uint16_t) sum));
}

struct icmp_too_big
{
	struct icmphdr header;
	struct ip ip;
	char data[8];
}__attribute__ ((__packed__));

void Gateway::icmp_mtu(Device* dev, Packet* packet, uint16_t MTU)
{
	int current_offset = 0;
	if(packet->getLength() < (int)(sizeof(struct ether_header) + sizeof(struct ip) + 8))
	{
		printf("plen %d\n", packet->getLength());
		return;
	}


	Ethernet r_ethernet(packet);
	if(r_ethernet.getProtocol() != ETHERTYPE_IP)
	{
		printf("proto %d\n", r_ethernet.getProtocol());
		return;
	}

	IP r_ip(packet, sizeof(struct ether_header));
	int ip_hlen = r_ip.getNextOffset() - sizeof(struct ether_header);
	if(ip_hlen != 20)
	{
		printf("hlen %d\n", ip_hlen);
		return;
	}

	Packet s_packet(sizeof(struct ether_header) + sizeof(struct icmp_too_big) + sizeof(struct ip));
	s_packet.setLength(sizeof(struct ether_header) + sizeof(struct icmp_too_big) + sizeof(struct ip));
	struct icmp_too_big response;
	memset(&response, 0, sizeof(response));
	response.header.type = ICMP_DEST_UNREACH;
	response.header.code = ICMP_FRAG_NEEDED;
	response.header.un.frag.mtu = htons(MTU);

	packet->readByteArray(sizeof(struct ether_header),sizeof(struct ether_header) + sizeof(struct ip)+8, &response.ip);
	response.header.checksum = one_compl(&response, sizeof(response));

	Ethernet s_ethernet(&s_packet);
	s_ethernet.setSource(r_ethernet.getDestination());
	s_ethernet.setDestination(r_ethernet.getSource());
	s_ethernet.setProtocol(ETHERTYPE_IP);



	/* START OF IP */
	struct ip s_ip_hdr;
	s_ip_hdr.ip_hl = 5;
	s_ip_hdr.ip_v = 4;
	s_ip_hdr.ip_tos = 0;
	s_ip_hdr.ip_len = htons(sizeof(struct ip)+sizeof(struct icmp_too_big));
	s_ip_hdr.ip_id = 0;
	s_ip_hdr.ip_off = htons(IP_DF);
	s_ip_hdr.ip_ttl = 64;
	s_ip_hdr.ip_p = IPPROTO_ICMP;
	s_ip_hdr.ip_sum = 0;
	s_ip_hdr.ip_src = r_ip.getDestination();//inet_addr("143.248.48.1");
	s_ip_hdr.ip_dst = r_ip.getSource();

	s_ip_hdr.ip_sum = one_compl(&s_ip_hdr, sizeof(s_ip_hdr));

	current_offset = sizeof(struct ether_header);

	s_packet.writeByteArray(current_offset, current_offset + sizeof(struct ip), &s_ip_hdr);
	current_offset += sizeof(struct ip);

	s_packet.writeByteArray(current_offset, current_offset + sizeof(struct icmp_too_big), &response);
	current_offset += sizeof(struct icmp_too_big);

	dev->writePacket(s_packet.inMemory, s_packet.getLength());
	printf("ICMP sent.\n");
	/* END OF IP */
}

void Gateway::serve(void)
{
	const unsigned char ETHER_BROADCAST[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,};
	const struct in_addr IPv4_BROADCAST = { 0xFFFFFFFF };
	const struct in_addr IPv4_NONE = { 0x00000000 };
	Packet inPacket(MY_PACKET_LEN);
	Packet outPacket(MY_PACKET_LEN);
	uint64_t current_time = 0;
	struct timeval temp_time;

	while(!event_base_got_exit(evbase))
	{
		gettimeofday(&temp_time, 0);
		current_time = (temp_time.tv_sec * 1000) + (temp_time.tv_usec / 1000);
		int inBurst, outBurst;
		for(inBurst = 0; inBurst < IO_BURST; inBurst++)
		{
			struct userInfo* user = 0;
			int readLen = inDev->readPacket(inPacket.inMemory,MY_PACKET_LEN);
			if(readLen == -1)
				break;
			if(readLen > MY_PACKET_LEN)
			{
				inPacket.setLength(MY_PACKET_LEN);
				printf("Too long packet(%d) received from inside, send ICMP\n", readLen);
				icmp_mtu(inDev, &inPacket, MY_MTU);
				continue;
			}

			if(readLen < (int)sizeof(struct ether_header))
			{
				//printf("short %d\n", __LINE__);//#DEBUG
				continue;
			}

			inPacket.setLength(readLen);

			Packet* packet = &inPacket;
			Ethernet ethernet(packet);
			if(ethernet.getProtocol() == ETHERTYPE_ARP)
			{
				ARP arp(packet, ethernet.getNextOffset());
				struct arphdr arp_header = arp.getHeader();

				if (! (
					(arp_header.ar_hrd == ARPHRD_ETHER) &
					(arp_header.ar_pro == ETHERTYPE_IP) &
					((arp_header.ar_op == ARPOP_REQUEST) || (arp_header.ar_op == ARPOP_REPLY)) &
					(arp_header.ar_pln == sizeof(struct in_addr)) &
					(arp_header.ar_hln == sizeof(struct ether_addr))
					) )
				{
					//filter out
					//printf("filltered %d\n", __LINE__);//#DEBUG
					continue;
				}

				
				struct in_addr destIP = arp.getDestinationIP();
				struct in_addr srcIP = arp.getSourceIP();

				StaticIPMap::iterator destIter
				= this->staticIPMap.find(destIP.s_addr);

				StaticIPMap::iterator srcIter
				= this->staticIPMap.find(srcIP.s_addr);

				UserMap::const_iterator userIter
				= this->userMap.find(srcIP.s_addr);

				if(destIter != staticIPMap.end())
				{
					//printf("drop %d\n", __LINE__);////#DEBUG
					continue; //we don't have to send it outside
				}

				if(userIter != userMap.end())
				{
					//change mac address
					struct ether_addr source_mac = ethernet.getSource();
					if(memcmp(&userIter->second->user_mac, &source_mac, sizeof(struct ether_addr)) != 0)
					{
						//printf("drop %d\n", __LINE__);//#DEBUG
						continue; //unauthorized user is using ip
					}
					user = userIter->second;
					arp.setSourceMAC(srcIter->second);
					ethernet.setSource(srcIter->second);
				}
				else if(srcIter != staticIPMap.end())
				{
					//printf("drop %d\n", __LINE__);//#DEBUG
					continue; //unauthorized user is using our ip
				}
			}
			else if(ethernet.getProtocol() == ETHERTYPE_IP)
			{
				IP ip(packet, ethernet.getNextOffset());
				if(readLen < (ethernet.getNextOffset() + ip.getNextOffset()))
				{
					//printf("drop %d\n", __LINE__);//#DEBUG
					continue; //too short ip packet
				}

				struct in_addr destIP = ip.getDestination();
				struct in_addr srcIP = ip.getSource();

				if(ip.getProtocol() == IPPROTO_UDP)
				{
					if( (memcmp(&IPv4_NONE, &srcIP, sizeof(struct in_addr)) == 0)
							&& (memcmp(&IPv4_BROADCAST, &destIP, sizeof(struct in_addr)) == 0) )
					{
						UDP udp(packet, ip.getNextOffset());
						if((udp.getSource() == 68) && (udp.getDestination() == 67))
						{
							DHCP dhcp(packet, udp.getNextOffset());
							if(dhcp.getOpcode() == 1)
							{
								int messageType = dhcp.getMessageType();

								struct dhcp_request request;
								if(messageType == 1)
									request.isDiscover = true;
								else if(messageType == 3)
									request.isDiscover = false;
								else
								{
									//printf("drop %d\n", __LINE__);//#DEBUG
									continue;
								}
								request.gateway = this;
								request.mtu = MY_MTU;
								request.mac = dhcp.getClientMAC();
								request.transID = dhcp.getTransactionID();

								struct ether_addr source_mac = ethernet.getSource();
								if(memcmp(&request.mac, &source_mac, ETH_ALEN) != 0)
								{
									//printf("drop %d\n", __LINE__);//#DEBUG
									continue; //bad mac address
								}

								db->createDHCP(request);
							}
							//printf("drop %d\n", __LINE__);//#DEBUG
							continue;
						}
					}
				}

				StaticIPMap::const_iterator destIter
				= this->staticIPMap.find(destIP.s_addr);

				StaticIPMap::const_iterator srcIter
				= this->staticIPMap.find(srcIP.s_addr);

				UserMap::const_iterator userIter
				= this->userMap.find(srcIP.s_addr);

				if(destIter != staticIPMap.end())
				{
					//printf("drop %d\n", __LINE__);//#DEBUG
					continue; //we don't have to send it outside
				}

				if(userIter != userMap.end())
				{
					//change mac address
					struct ether_addr source_mac = ethernet.getSource();
					if(memcmp(&userIter->second->user_mac, &source_mac, sizeof(struct ether_addr)) != 0)
					{
						//printf("drop %d\n", __LINE__);//#DEBUG
						continue; //unauthorized user is using ip
					}
					user = userIter->second;
					ethernet.setSource(srcIter->second);
				}
				else if(srcIter != staticIPMap.end())
				{
					//printf("unauthorized user is using our ip %d\n", __LINE__);//#DEBUG
					continue;
				}
			}
			if(user)
			{
				if((user->last_access + user->timeout) < current_time)
				{
					user->last_access = current_time;
					struct update_time request;
					request.mac = user->user_mac;
					request.ip = user->ip;
					request.UTC = current_time;
					request.gateway = this;
					db->updateTime(request);
				}
			}
			outDev->writePacket(inPacket.inMemory, inPacket.getLength());
		}

		for(outBurst = 0; outBurst < IO_BURST; outBurst++)
		{
			int readLen = outDev->readPacket(outPacket.inMemory,MY_PACKET_LEN);
			if(readLen == -1)
				break;
			if(readLen > MY_PACKET_LEN)
			{
				outPacket.setLength(MY_PACKET_LEN);
				printf("Too long packet(%d) received from outside, send ICMP\n", readLen);
				icmp_mtu(outDev, &outPacket, MY_MTU);
				continue;
			}
			outPacket.setLength(readLen);

			Packet* packet = &outPacket;
			Ethernet ethernet(packet);
			if(ethernet.getProtocol() == ETHERTYPE_ARP)
			{
				ARP arp(packet, ethernet.getNextOffset());
				struct arphdr arp_header = arp.getHeader();

				if (! (
						(arp_header.ar_hrd == ARPHRD_ETHER) &
						(arp_header.ar_pro == ETHERTYPE_IP) &
						((arp_header.ar_op == ARPOP_REQUEST) || (arp_header.ar_op == ARPOP_REPLY)) &
						(arp_header.ar_pln == sizeof(struct in_addr)) &
						(arp_header.ar_hln == sizeof(struct ether_addr))
				) )
				{
					//filter out
					//printf("drop %d\n", __LINE__);//#DEBUG
					continue;
				}

				struct in_addr destIP = arp.getDestinationIP();
				struct in_addr srcIP = arp.getSourceIP();

				StaticIPMap::const_iterator destIter
				= this->staticIPMap.find(destIP.s_addr);

				StaticIPMap::const_iterator srcIter
				= this->staticIPMap.find(srcIP.s_addr);

				UserMap::iterator userIter
				= this->userMap.find(destIP.s_addr);

				if(srcIter != staticIPMap.end())
				{
					//printf("drop %d\n", __LINE__);//#DEBUG
					continue; //we don't have to bring it inside
					//someone is using our ip outside
				}

				if(destIter != staticIPMap.end())
				{
					if(userIter != userMap.end())
					{
						//change mac address
						struct ether_addr dest_mac = ethernet.getDestination();

						if((memcmp(&destIter->second, &dest_mac, sizeof(struct ether_addr)) != 0)
								&&
								(memcmp(ETHER_BROADCAST, &dest_mac, sizeof(struct ether_addr)) != 0))
						{
							//printf("drop %d\n", __LINE__);//#DEBUG
							continue; //not broad, not my mac
						}
						arp.setDestinationMAC(userIter->second->user_mac);
						ethernet.setDestination(userIter->second->user_mac);
					}
				}
				else if(userIter != userMap.end())
				{
					//printf("we don't have static IP anymore\n");//#DEBUG
					free(userIter->second);
					userMap.erase(userIter);
					//TOOD free memory afterwards
				}
			}
			else if(ethernet.getProtocol() == ETHERTYPE_IP)
			{
				IP ip(packet, ethernet.getNextOffset());
				if(readLen < (ethernet.getNextOffset() + ip.getNextOffset()))
				{
					//printf("drop %d\n", __LINE__);//#DEBUG
					continue; //too short ip packet
				}

				struct in_addr destIP = ip.getDestination();
				struct in_addr srcIP = ip.getSource();

				StaticIPMap::const_iterator destIter
				= this->staticIPMap.find(destIP.s_addr);

				StaticIPMap::const_iterator srcIter
				= this->staticIPMap.find(srcIP.s_addr);

				UserMap::const_iterator userIter
				= this->userMap.find(destIP.s_addr);

				if(srcIter != staticIPMap.end())
				{
					//printf("drop %d\n", __LINE__);//#DEBUG
					continue; //we don't have to bring it inside
					//someone is using our ip outside
				}

				if(destIter != staticIPMap.end())
				{
					if(userIter == userMap.end())
					{
						//printf("drop %d\n", __LINE__);//#DEBUG
						continue; //no such user
					}
					//change mac address
					struct ether_addr dest_mac = ethernet.getDestination();

					if((memcmp(&destIter->second, &dest_mac, sizeof(struct ether_addr)) != 0)
							) //no broadcast for specific IP is allowed
							//&&
							//(memcmp(BROADCAST, &dest_mac, sizeof(struct ether_addr)) != 0))
					{
						//printf("no broadcast for specific IP is allowed\n");
						//printf("drop %d\n", __LINE__);//#DEBUG
						continue; //not broad, not my mac
					}
					ethernet.setDestination(userIter->second->user_mac);
				}
				else if(userIter != userMap.end())
				{
					//we don't have static IP anymore
					free(userIter->second);
					userMap.erase(userIter);
					//TOOD free memory afterwards
				}

				if(ip.getProtocol() == IPPROTO_UDP)
				{
					if( (memcmp(&IPv4_NONE, &srcIP, sizeof(struct in_addr)) == 0)
							&& (memcmp(&IPv4_BROADCAST, &destIP, sizeof(struct in_addr)) == 0) )
					{
						UDP udp(packet, ip.getNextOffset());
						if(((udp.getSource() == 68) & (udp.getDestination() == 67))
								| ((udp.getSource() == 67) & (udp.getDestination() == 68)))
						{
							//printf("drop outer DHCP %d\n", __LINE__);//#DEBUG
							continue;
						}
					}
				}
			}

			inDev->writePacket(outPacket.inMemory, outPacket.getLength());
		}


		if((inBurst == 0) && (outBurst == 0))
			usleep(0);
		event_base_loop(evbase, EVLOOP_NONBLOCK);
	}
}

void Gateway::terminate()
{
	eventfd_t v = 1;
	eventfd_write(this->termEventFD, v);
}

void Gateway::addStaticIP(struct in_addr ip, struct ether_addr mac)
{
	pthread_mutex_lock(&this->addStaticIPLock);
	this->staticIPAddRequest.push(pair<struct in_addr, struct ether_addr>(ip, mac));
	eventfd_t v = 1;
	eventfd_write(this->addStaticIPEventFD, v);
	pthread_mutex_unlock(&this->addStaticIPLock);
}

void Gateway::delStaticIP(struct in_addr ip)
{
	pthread_mutex_lock(&this->delStaticIPLock);
	this->staticIPDelRequest.push(ip);
	eventfd_t v = 1;
	eventfd_write(this->delStaticIPEventFD, v);
	pthread_mutex_unlock(&this->delStaticIPLock);
}

void Gateway::addUserInfo(struct userInfo info)
{
	pthread_mutex_lock(&this->addUserLock);
	struct userInfo* copy = (struct userInfo*)malloc(sizeof(struct userInfo));
	memcpy(copy, &info, sizeof(struct userInfo));
	this->userAddRequest.push(copy);
	eventfd_t v = 1;
	eventfd_write(this->addUserEventFD, v);
	pthread_mutex_unlock(&this->addUserLock);
}

void Gateway::delUserInfo(struct in_addr ip)
{
	pthread_mutex_lock(&this->delUserLock);
	this->userDelRequest.push(ip);
	eventfd_t v = 1;
	eventfd_write(this->delUserEventFD, v);
	pthread_mutex_unlock(&this->delUserLock);
}

void Gateway::sendPacketRequest(Packet* packet)
{
	pthread_mutex_lock(&this->sendPacketLock);
	this->sendPacketRequestQueue.push(packet);
	eventfd_t v = 1;
	eventfd_write(this->sendPacketFD, v);
	pthread_mutex_unlock(&this->sendPacketLock);
}
