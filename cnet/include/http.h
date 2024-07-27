#pragma once
#include <dlfcn.h>
#include "server.h"
typedef enum {
	NONE,
	GET,
	POST,
	HEAD,
	PUT,
	DELETE,
	PATCH,
	CONNECT
} Method;
typedef struct {
	map config;
	map pages;
	map mime;
	string sharedir;
	string pagedir;
	string pagesraw;
} httpcache;
typedef struct {
	httpcache cache;
	void* ssl;
	int http;
	int https;
	int fcgi;
	map reqests;
	Poll* poll;
	int total;
	int ok;
	int error;
	int stop;
} HTTPServer;
typedef enum {
	Connected,
	ReceivedPartial,
	ReceivedHeader,
	ResponsePartial,
	ResponseFull,
	KeepAlive
} HTTPState;
extern char* HTTPState_c[];
typedef struct {
	Connection client;
	Connection server;
	HTTPServer* httpd;
	double time;
	HTTPState state;
	int keepalive;
} session;
typedef struct {
	int(*error)();
	int(*success)();
} HTTPHandler;
typedef struct {
	map header;
	string body;
	string error;
	int id;
} message;
void* http_new(map config, Poll* poll);
void http_free(HTTPServer* server);
int cb_httptimeout(session* in,int mask);
int http_timeout(session* in,int sec);
int httpsub_free(session* in,char* error);
int cb_httpaccept(HTTPServer* in, int mask);
int http_isready(string in);
int cb_sslhandshake(session* sub, int mask);
int cb_sslconnect(HTTPServer* in,int mask);
int http_handle(session* sub);
int cb_incoming(session* sub,int mask);
map appheader_m(string appheader);
string quick_response(char* result,string body);
pair c_pair(char* status,char* body);
int send_response(string head, string body,session* sub);
int request_response(session* sub, message mess);
int dir_handler(session* sub, message mess);
int page_handler(session* sub,message mess);
int cb_writerest(session* sub,int mask);
string cut_header(string* in);
map header_m(string in);
string conn_readbody(Connection* conn, map header, int msec);
map conn_readheader(Connection* conn, int msec);
int fetch_get(Connection* conn);
int fetch_connect(Connection* conn);
int fetch(int(*handler)(Connection*));
Method method_id(string code);
int http_run(map config);
