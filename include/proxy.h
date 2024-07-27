#pragma once
#include "cnet.h"

typedef struct {
	IPNo ip;
	int mask;
} IPMask;

typedef struct {
	int http;
	int socks;
	int admin;
	vector sites;
	string sitesraw;
	Poll* poll;
	void* dns;
	int total;
	int ok;
	int error;
	vector permit;
	int timeout;
	int stop;
	int ipv6;
	map config;
	double created;
	double updated;
} Proxy;

typedef struct {
	Connection client;
	Connection server;
	Proxy* proxy;
	void* dnsreq;
	double created;
	double updated;
} Proxysub;

int cb_server(Proxysub* data,int mask);
int cb_client(Proxysub* data,int mask);
int close_error(Proxysub* sub, char* msg);
void proxysub_free(Proxysub* data);
int passthrough(Proxysub* data);
int dns_resolved(Proxysub* data, IPNo ip);
int cb_clientfirstin(Proxysub* sub,int mask);
int ends_s(string in, string by);
int site_allowed(vector list, string name);
int cb_idletimeout(void* data,int mask);
int cb_newclient(Proxy* proxy,int mask);
int ipmask_match(vector nets,IPNo ip);
vector s_ipmaskv(string in);
IPMask s_ipmask(string in);
int is_yes(string in);
void* proxy_new(map config,Poll* poll);
void proxy_free(Proxy* proxy);
int cb_proxystop(Proxy* in,int mask);
int proxy_run(map config);
int cbsks_accept(Proxy* proxy,int mask);
int cbsks_auth(Proxysub* in,int mask);
int cb_varifypass(Proxysub* in,int mask);
int sks_baddata(Proxysub* psub,char* s);
int cb_sksreceive(Proxysub* in,int mask);
int sks_connect(Proxysub* in, IPNo ip);
int sks_tryip4(Proxysub* in,int mask);
int sks_sendok(Proxysub* in,int mask);
