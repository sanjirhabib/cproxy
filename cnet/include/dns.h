#pragma once
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "cstr.h"
#include "poll.h"
#include "connection.h"
typedef struct header {
	unsigned short id;
	unsigned char rd : 1;
	unsigned char tc : 1;
	unsigned char aa : 1;
	unsigned char opcode : 4;
	unsigned char qr : 1;
	unsigned char rcode : 4;
	unsigned char z : 3;	
	unsigned char ra : 1;
	unsigned short qdcount;
	unsigned short ancount;
	unsigned short nscount;
	unsigned short arcount;
} HEADER;

typedef struct q_flags {
	unsigned short qtype;
	unsigned short qclass;
} Q_FLAGS;

typedef struct rr_flags {
	unsigned short type;
	unsigned short class;
	unsigned int ttl;
} RR_FLAGS;

typedef struct {
	IPNo ip;
	double time;
} DNSCache;
typedef struct {
	IPNo nameserver;
	map cache;
	Poll* poll;
	int stop;
	map config;
	vector polldata;
} dns_resolver;
typedef struct {
	Connection conn;
	void* data;
	void* callback;
	int type;
	int cancel;
	string name;
	dns_resolver* resolver;
} DNSPolldata;

typedef enum {
	DNS_ALL=0,
	DNS_A=1,
	DNS_NS=2,
	DNS_CNAME=5,
	DNS_SOA=6,
	DNS_PTR=12,
	DNS_MX=15,
	DNS_TXT=16,
	DNS_SIG=24,
	DNS_AAAA=28,
	DNS_SRV=33,
	DNS_NAPTR=35,
	DNS_OPT=41,
	DNS_TKEY=249,
	DNS_TSIG=250,
	DNS_MAILB=253,
	DNS_ANY=255
} DNSRecordtype;

void dnscache_del(dns_resolver* res,string host);
void dnscache_add(map* cache,string host,IPNo ip);
IPNo dnscache_get(map* cache,string host);
int dns_callback(DNSPolldata* in,IPNo ip);
void dns_cancel(DNSPolldata* data);
int dns_timeout(DNSPolldata* data, int mask);
int dns_received(DNSPolldata* data,int mask);
void dns_free(dns_resolver* in);
void* dns_new(map config,Poll* poll);
IPNo find_nameserver(string server);
void* dns_resolve(dns_resolver* resolver, string name,void* callback,void* data,int type);
IPNo dns_resolve_sync(dns_resolver* resolver,string name);
string dns_query_packet(string host,ConnectionType type);
Connection dns_query(dns_resolver* resolver,string host,ConnectionType type);
vector dnsresponse_v(string res);
int ip_isnull(IPNo in);
IPNo dnsresponse_ip(string res);
string host_binary(string host);
string binary_host(string in,string all) ;
