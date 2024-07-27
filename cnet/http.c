#include <dlfcn.h>

//#include "connection.h"
#include "server.h"
#include "http.h"


/*header
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
end*/
char* HTTPState_c[]={ //
	"Connected",
	"ReceivedPartial",
	"ReceivedHeader",
	"ResponsePartial",
	"ResponseFull",
	"KeepAlive"
};
void* http_new(map config, Poll* poll){
	string pagesraw=file_s(map_c(config,"pages"));
	HTTPServer server={
		.http           = listen_port(map_c_i(config,"http")),
		.https          = listen_port(map_c_i(config,"https")),
		.ssl            = ssl_init(
			map_get(config,c_("cert")),
			map_get(config,c_("privkey")),
			map_get(config,c_("fullchain"))
		),
		.cache={
			.sharedir   = map_get(config,c_("sharedir")),
			.pagedir    = map_get(config,c_("pagedir")),
			.config     = config,
			.pages      = s_map(trim(pagesraw),"\t","\n"),
			.pagesraw   = pagesraw
		},
		.poll=poll
	};

	if(!server.http && !server.https){
		msg("no port indicated for http for https");
		return NULL;
	}

	HTTPServer* ret=to_heap(&server,sizeof(server));

	if(server.http){
		msg("http: listening port %d",p_i(map_getp(config,c_("http"))));
		sock_nonblock(server.http);
		poll_add(poll,server.http,IN|END,cb_httpaccept,ret,"listen/http");
	}
	if(server.https){
		msg("https: listening port %d",p_i(map_getp(config,c_("https"))));
		sock_nonblock(server.https);
		poll_add(poll,server.https,IN|END,cb_sslconnect,ret,"listen/https");
	}
	msg("serving %d pages",server.cache.pages.keys.len);
	return ret;
}
void http_free(HTTPServer* server){
	sock_free(server->http);
	sock_free(server->https);
	sock_free(server->fcgi);
	ssl_free(server->ssl);

	map_free(&server->cache.pages);
	map_free(&server->cache.mime);

	_free( &server->cache.sharedir);
	_free( &server->cache.pagesraw);

	free(server);
}
int cb_httptimeout(session* in,int mask){
	msg("timeout %d",in->client.socket);
	if(!(mask&IN)) return 0;
	in->keepalive=0;
	if(in->state==KeepAlive) return httpsub_free(in,NULL);
	var mess=format("timeout as %v",c_(HTTPState_c[in->state]));
	httpsub_free(in,mess.str);
	_free(&mess);
}
int http_timeout(session* in,int sec){
	return poll_set_timeout(in->httpd->poll,in->client.socket,sec);
}
int httpsub_free(session* in,char* error){
	if(in->keepalive && !in->client.is_error){
		msg("keepalive mode");
		in->state=KeepAlive;
		in->httpd->ok++;
		_free(&in->client.read);
		_free(&in->client.write);
		in->client.can_read=1;
		in->client.can_write=1;
//		msg("can %d %d error %d",in->client.can_read,in->client.can_write,in->client.is_error);
		poll_update(in->httpd->poll,in->client.socket,cb_incoming,0);
		http_timeout(in,10);
		return 0;
	}
	in->httpd->total--;
	if(error){
		msg("client %d %s",in->client.socket,error);
		in->httpd->error++;
	}
	else if(in->client.write.len){
		msg("client %d incomplete write %d bytes",in->client.socket,in->client.write.len);
		in->httpd->error++;
	}
	else in->httpd->ok++;
	poll_del(in->httpd->poll,in->client.socket);
	conn_free(&in->client);
	free(in);
	return 0;
}
int cb_httpaccept(HTTPServer* in, int mask){
	if(mask&END){
		msg("listener ended");

		poll_del( in->poll,in->http);
		sock_free(in->http);

		in->stop=1;
		return -1;
	}
	session sub={
		.time=time_sec(),
		.client.socket=accept(in->http,NULL,NULL),
		.httpd=in
	};
	sock_nonblock(sub.client.socket);
	in->total++;

//Habib: 2024 partial implementation commented.
//	call_chain chain={
//		.is_ready=http_isready,
//		.read_header=http_readheader
//	};


	poll_add(in->poll,sub.client.socket,IN|OUT|EDGE|END,cb_incoming,to_heap(&sub,sizeof(sub)),"http/client");
	poll_add_timeout(in->poll,sub.client.socket,cb_httptimeout);
	poll_set_timeout(in->poll,sub.client.socket,5);
}
int http_isready(string in){
	return s_has(in,"\r\n\r\n")!=NULL;
}
int cb_sslhandshake(session* sub, int mask){
	if(mask&END){
		httpsub_free(sub,"ssl connection dropped");
		return 0;
	}
	if(!(mask&(IN|OUT))) return 0;
	int ret=ssl_handshake(sub->client.ssl);
	if(ret==-1){
		httpsub_free(sub,"ssl handshake failed");
		return 0;
	}
	if(ret){ //success.
		poll_update(sub->httpd->poll,sub->client.socket,cb_incoming,IN|OUT|EDGE|END);
		http_timeout(sub,5);
		return 0;
	}
	//continue
	return 0;
}
int cb_sslconnect(HTTPServer* in,int mask){
	if(mask&END){
		msg("listener ended");
		poll_del(in->poll,in->https);
		sock_free(in->https);
		in->stop=1;
		return -1;
	}
	session sub={
		.time=time_sec(),
		.client.socket=accept(in->https,NULL,NULL),
		.httpd=in
	};
	sock_nonblock(sub.client.socket);
	sub.client.ssl=ssl_start(in->ssl,sub.client.socket);
	if(!sub.client.ssl){
		msg("start ssl failed");
		conn_free(&sub.client);
		return 0;
	}
	in->total++;
	poll_add(in->poll,sub.client.socket,IN|OUT|END,cb_sslhandshake,to_heap(&sub,sizeof(sub)),"https/client");
	poll_add_timeout(in->poll,sub.client.socket,cb_httptimeout);
	poll_set_timeout(in->poll,sub.client.socket,5);
	return 0;
}
int http_handle(session* sub){
	string header=cutp_s(&sub->client.read,"\r\n\r\n");
	message mess={
		.header=header_m(header),
		.body=move(&sub->client.read)
	};
	sub->keepalive=eq_s(map_c(mess.header,"connection"),c_("keep-alive"));
	return request_response(sub,mess);
}
int cb_incoming(session* sub,int mask){
	if(mask&END){
		sub->client.is_error=1;
		return httpsub_free(sub,"connection dropped");
	}
	if(mask&OUT) sub->client.can_write=1;
	if(mask&IN) sub->client.can_read=1;
	if(!sub->client.can_read) return 0;
	int ret=conn_read(&sub->client);
	if(!ret) return 0;
	if(!http_isready(sub->client.read)) return 0;
	return http_handle(sub);
}
map appheader_m(string appheader){
	map ret=NullMap;
	map_add(ret,"content-type")=c_("text/html");
	map_add(ret,"status")=i_(200);
	map app=s_map(appheader,":","\n");
	for(int i=0; i<app.keys.len; i++){
		map_addv(ret,own(app.keys.var+i))=own(app.vals.var+i);
	}
	map_free(&app);
	return ret;
}
string quick_response(char* result,string body){
	return format(
		"HTTP/1.1 %v\r\n"
		"Content-Type: text/html\r\n"
		"Content-Length: %v\r\n"
		"Connection: Closed\r\n"
		"\r\n%v",c_(result),i_(body.len),body);
}
pair c_pair(char* status,char* body){
	return (pair){
		.head=c_(status),
		.tail=c_(body)
	};
}
int send_response(string head, string body,session* sub){
	map resheader=appheader_m(head);
	string status=map_c(resheader,"status");
	map_delc(&resheader,"status");
	string response=format(
		"HTTP/1.1 %v\r\n"
		"connection: %v\r\n"
		"content-length: %v\r\n"
		"%v\r\n"
		"%v",
		status,
		c_(sub->keepalive ? "keep-alive" : "close"),
		i_(body.len),
		map_s(resheader,": ","\r\n"),
		body
	);
	sub->client.write=response;
	conn_write(&sub->client);
	if(sub->client.write.len){
		sub->state=ResponsePartial;
		http_timeout(sub,10); //10 secs for writing.
		poll_update(sub->httpd->poll,sub->client.socket,cb_writerest,0);
	}
	else{
		sub->state=ResponseFull;
		httpsub_free(sub,NULL);
	}
	return 1;
}
int request_response(session* sub, message mess){
	int ret=0;
	//if(!ret) ret=fcgi_handler(sub,mess);
	if(!ret) ret=page_handler(sub,mess);
	if(!ret) ret=dir_handler(sub,mess);
	if(!ret) ret=send_response(c_("status:404"),c_("This file doesn't exist on our server."),sub);
	//out("%v %v %v %v",map_c(mess.header,"remote_addr"),map_c(mess.header,"request_method"),map_c(mess.header,"request_uri"),i_(ret.tail.len));
	return ret;
}
int dir_handler(session* sub, message mess){
	if(!eq(map_c(mess.header,"method"),"GET")) return 0;
	//if(mess.header["method"]!="GET") return 0;1234567890;
	string sharedir=sub->httpd->cache.sharedir;
	if(!sharedir.len) return 0;
	string path=map_c(mess.header,"path");
	if(s_has(path,"../")) return 0;
	string file=cat(ro(sharedir),path);
	string ret=file_s(file);
	if(!ret.len) return 0;
	//todo: mime type.
	return send_response((string){0},ret,sub);
}
int page_handler(session* sub,message mess){
	httpcache cache=sub->httpd->cache;
	if(!cache.pagedir.len) return 0;

	string path=map_c(mess.header,"path");
	string name=map_get(cache.pages,path);
	if(!name.len) return 0;
	name=s_nullterm(path_cat(ro(cache.pagedir),name));

	void* lib=dlopen(name.str, RTLD_LAZY); //|RTLD_GLOBAL
	if(!lib){
		sys_error();
		msg("dlopen(%v) failed",name);
		_free(&name);
		return send_response(c_("status:500"),c_("Internal Server Error"),sub);
	}
	_free(&name);
	void* func=dlsym(lib, "page");
	if(!func){
		msg("no page() function in lib %v",name);
		return send_response(c_("status:500"),c_("Internal Server Error"),sub);
	}
	pair ret=((pair(*)(void*))func)(&mess);
	dlclose(lib);
	return send_response(ret.head,ret.tail,sub);
}
int cb_writerest(session* sub,int mask){
	if(mask&END) return httpsub_free(sub,NULL);
	if(!(mask&OUT)) return 0;
	sub->client.can_write=1;
	int ret=conn_write(&sub->client);
	if(!sub->client.write.len) return httpsub_free(sub,NULL);
	if(ret) http_timeout(sub,10); //10 more seconds to live.
	return 0;
}
string cut_header(string* in){
	if(!s_has(*in,"\r\n\r\n")) return (string){0};
	pair s2=cut_s(*in,"\r\n\r\n");
	string ret=*in;
	*in=s_copy(s2.tail);
	return resize(ret,s2.head.len);
}
map header_m(string in){
	pair reqhead=cut_s(in,"\r\n");
	map ret=s_map(reqhead.tail,":","\r\n");
	vector http=s_vec_ex(reqhead.head,' ',3);
	if(http.len!=3){
		msg("wrong header");
		return (map){0};
	}
	map_add(ret,"method")=http.var[0];
	map_add(ret,"path")=http.var[1];
	map_add(ret,"http")=http.var[2];
	vec_free_ex(&http,NULL);
	return ret;
}
string conn_readbody(Connection* conn, map header, int msec){
	int size=map_c_i(header,"content-length");
	if(!size) return (string){0};
	while(!conn->is_error && msec>0){
		conn->can_read=1;
		conn_read(conn);
		if(conn->read.len>=size) return conn->read;
		msec-=200;
		msleep(200);
	}
	string ret=conn->read;
	conn->read=(string){0};
	return ret;
}
map conn_readheader(Connection* conn, int msec){
	while(!conn->is_error && msec>0){
		conn->can_read=1;
		conn_read(conn);
		string header=cut_header(&conn->read);
		if(header.len)
			return header_m(conn->read);
		msec-=200;
		msleep(200);
	}
	return (map){0};
}
int fetch_get(Connection* conn){
	msg("client: fetching GET");
	string send1=c_("GET http://c.local/var.c HTTP/1.1\r\nHost: 127.0.0.1:80\r\nUser-Agent: curl/7.82.0\r\nAccept: */*\r\n\r\n");
	conn->write=send1;
	conn_write(conn);
	map header=conn_readheader(conn,1000);
	string body=conn_readbody(conn,header,1000);
	map_free(&header);
	msg("client: fetched %d bytes",body.len);
	return 0;
}
int fetch_connect(Connection* conn){
	msg("client: fetching CONNECT");
	string send1=c_("CONNECT c.local HTTP/1.1\r\nHost: 127.0.0.1:80\r\nUser-Agent: curl/7.82.0\r\nProxy-Connection: Keep-Alive\r\n\r\n");
	string send2=c_("GET http://c.local/var.c HTTP/1.1\r\nHost: 127.0.0.1:80\r\nUser-Agent: curl/7.82.0\r\nAccept: */*\r\n\r\n");
	conn->write=send1;
	conn_write(conn);
	map header=conn_readheader(conn,1000);
	if(!header.keys.len) return 0;
	if(map_c_i(header,"code")!=200) return 0;
	msg("client: 200 ok");
	conn->write=send2;
	conn_write(conn);
	map_free(&header);
	header=conn_readheader(conn,1000);
	string body=conn_readbody(conn,header,1000);
	map_free(&header);
	msg("client: fetched %d bytes",body.len);
	return 0;
}
int fetch(int(*handler)(Connection*)){
	msg("client: started (connect)");
	msleep(200);
	Connection conn=connection_new(s_ip(c_("127.0.0.1")),8070,TCP);
	if(conn.is_error){
		msg("client: connecting with server failed");
		return 0;
	}
	handler(&conn);
	conn_free(&conn);
	return 0;
}
Method method_id(string code){
	Method ret=NONE;
	if(eq(code,"GET")) ret=GET;
	else if(eq(code,"POST")) ret=POST;
	else if(eq(code,"HEAD")) ret=HEAD;
	else if(eq(code,"PUT")) ret=PUT;
	else if(eq(code,"DELETE")) ret=DELETE;
	else if(eq(code,"PATCH")) ret=PATCH;
	else if(eq(code,"CONNECT")) ret=CONNECT;
	return ret;	
}
int http_run(map config){
	server_run(config,http_new,http_free);
}
