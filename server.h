#ifndef __SERVER_H__
#define __SERVER_H__

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "c_hashmap/hashmap.h"
#include "crc32/crc32.h"
#include "server_configs.h"
#include "packet.h"
#include "mysql_connector.h"
#include "random_pool.h"
#include "list/gnu_list.h"
#include "timer.h"
#include "server_configs.h"
#include "messages.h"

struct user;

struct timeout {
	int alive_time;
};

struct client {
	struct epoll_event event;
	struct cbtimer timer;
	char name[20], ipaddr[20];
	int fd;
	struct sockaddr addr;
	struct timeout timeout;
	struct user *usr;
	struct message_looper looper;

	int buffer_offset;
	char buffer[SERVER_MAX_PACKETS];
	char respond[SERVER_MAX_PACKETS];
};

struct server {
	int serverfd, epollfd, stop_server;
	struct epoll_event event, eventlist[MAX_EVENTS];
	struct sockaddr_in serveraddr;
	struct mysql_config *mysql_config;
	struct random_pool random;
	int dump;

	map_t clients_map;
	map_t methods_map;
	map_t users_map;
	map_t tokens_map;

	struct list_head timers_list;
};

#endif /*__SERVER_H__*/

