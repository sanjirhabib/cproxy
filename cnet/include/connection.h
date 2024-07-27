#pragma once
#define _GNU_SOURCE
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <arpa/inet.h>
#include <errno.h>
#include "cstr.h"
#include "ssl.h"
#define sys_error() _sys_error(__FILE__,__LINE__)
typedef struct {
	int ip1;
	int ip2;
	int ip3;
	int ip4;
} IPNo;
typedef struct {
	int socket;
	void* ssl;
	string read;
	string write;
	int can_read;
	int can_write;
	int is_error;
	IPNo ip;
	int port;
	int type;
	string host;
} Connection;
typedef enum {
	TCP=1,
	UDP=1<<1,
	IP4=1<<2,
	IP6=1<<3,
	ASYNC=1<<4,
	SYNC=1<<5,
	LISTEN=1<<6,
	CONN=1<<7
} ConnectionType;
int _sys_error(char* file,int line);
int bind6(int port);
int bind4(int port);
IPNo s_ip(string ip);
int is_ip4(IPNo ip);
string ip_s(IPNo ip);
int sock_new(int port);
int sock6_new(int port);
int listen_port(int port);
int sock_nonblock(int conn);
void sock_free(int sock);
int msleep(int msec);
int conn_write(Connection* conn);
int conn_peer(Connection* conn);
int conn_read(Connection* conn);
int conn_readonce(Connection* conn);
int connect4(Connection* conn);
int connect6(Connection* conn);
int conn_connect(Connection* conn);
Connection connection_new(IPNo ip,int port,ConnectionType type);
void conn_free(Connection* conn);
int unix_listen(string file);
int unix_connect(string file);
