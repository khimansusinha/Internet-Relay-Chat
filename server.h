#ifndef _SERVER_H
#define _SERVER_H

#include<iostream>
#include<cstdlib>
#include<pthread.h>
#include<mutex>
#include<condition_variable>
#include <list>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/unistd.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <unordered_set>
#include <unordered_map>
#include <string.h>
#include <string>
#include <vector>
#include <iterator>
#include <cstring>

using namespace std;

#define MAXLINE 1024

extern int server_port;
extern char nick[1024];
extern char pass[1024];
extern char servers[1024];
extern char sock_addr[1024];

#define MAX_CLIENTS 100
#define MAX_CLIENT_INFO_LEN 1024

typedef struct handler {
	int sockfd;
	string msg;
	int flag;

}handler_t;

typedef struct client_info {
	char nickname[MAX_CLIENT_INFO_LEN];
	char username[MAX_CLIENT_INFO_LEN];
	char hostname[MAX_CLIENT_INFO_LEN];
	char realname[MAX_CLIENT_INFO_LEN];
	char usermode[2];
	char userunused[2];
	bool nickusercmd[2];

}client_info_t;

extern string g_server_hostname;
extern int g_stop;

extern list<handler> gworklist;
extern pthread_mutex_t m;
extern pthread_cond_t cv;

extern pthread_rwlock_t map_rw_lock;
extern unordered_map<string, int> g_nickname_map; //set to keep all the nick name during nick data request after accept()

extern pthread_rwlock_t map_clinfo_rw_lock;
extern unordered_map<int, client_info_t> g_ws_clinfo_map; //worker socket fd and its corresponsing client info map during accept()

extern pthread_rwlock_t channel_map_rw_lock;
extern unordered_map<string, vector<string> > channel_nkname_map;

extern pthread_rwlock_t channel_topic_rw_lock;
extern unordered_map<string, string> channel_topic_map;
 
#endif
