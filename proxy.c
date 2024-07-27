#include "proxy.h"
#include "cnet.h"


/*header

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

end*/
int cb_server(Proxysub* data,int mask){
	data->updated=time_sec();
	if(mask&END) data->server.is_error=1;
	if(mask&IN) data->server.can_read=1;
	if(mask&OUT) data->server.can_write=1;
	return passthrough(data);
}
int cb_client(Proxysub* data,int mask){
	data->updated=time_sec();
	if(mask&END) data->client.is_error=1;
	if(mask&IN) data->client.can_read=1;
	if(mask&OUT) data->client.can_write=1;
	return passthrough(data);
}
int close_error(Proxysub* sub, char* msg){
	printf("ERROR: %s\n",msg);
	sub->client.write=c_("HTTP/1.1 501 Not Implemented\r\n\r\n");
	conn_write(&sub->client);
	sub->proxy->error++;
	sub->proxy->ok--;
	proxysub_free(sub);
	return 0;
}
void proxysub_free(Proxysub* data){
	if(data->client.write.len||data->server.write.len){
		log_error("closing with client:%d and server:%d bytes unsent",data->client.write.len, data->server.write.len);
	}
	poll_del(data->proxy->poll,data->server.socket);
	poll_del(data->proxy->poll,data->client.socket);
	dns_cancel(data->dnsreq);
	msg("      %5d (%2d-server%s) %3d %v", data->server.read.len, data->server.socket,data->server.is_error ? " ERR" : "", data->server.write.len,data->server.host);
	msg("      %5d (%2d-client%s) %3d", data->client.read.len,data->client.socket, data->client.is_error ? " ERR" : "", data->client.write.len);
	conn_free(&data->client);
	conn_free(&data->server);
	data->proxy->ok++;
	data->proxy->total--;
	free(data);
}
int passthrough(Proxysub* data){
	poll_set_timeout(data->proxy->poll,data->client.socket,data->proxy->timeout);
	for(;;){
		int total_in=0;
		int total_out=0;
		if(data->server.can_write && data->server.write.len){
			int sent=conn_write(&data->server);
			string ip=ip_s(data->server.ip);
			msg("            (%2d) %-5d %5d %v %v",data->server.socket,data->server.write.len,sent,data->server.host,ip);
			_free(&ip);
			total_out+=sent;
		}
		if(data->client.can_read && data->server.write.len<64*1024){
			total_in+=conn_read(&data->client);
			if(data->client.read.len){
				data->server.write=cat(data->server.write,data->client.read);
				data->client.read=(string){0};
			}
		}
		if(data->client.can_write && data->client.write.len){
			int sent=conn_write(&data->client);
			msg("%5d %5d (%2d)             %v",sent,data->client.write.len,data->client.socket,data->server.host);
			total_out+=sent;
		}
		if(data->server.can_read && data->client.write.len<64*1024){
			total_in+=conn_read(&data->server);
			if(data->server.read.len){
				data->client.write=cat(data->client.write,data->server.read);
				data->server.read=(string){0};
			}
		}
		if(total_in+total_out==0) break;
	}
	if(data->client.is_error && data->server.is_error){
		proxysub_free(data);
		return 0;
	}
	if(data->client.is_error && !data->server.write.len){
		proxysub_free(data);
		return 0;
	}
	if(data->server.is_error && !data->client.write.len){
		proxysub_free(data);
		return 0;
	}
	return 0;
}
int dns_resolved(Proxysub* data, IPNo ip){
	data->dnsreq=NULL;
	data->updated=time_sec();
	if(ip_isnull(ip)) return close_error(data,"dns failed");

	data->server.ip=ip;
	data->server.type=ASYNC|TCP;
	conn_connect(&data->server);
	if(data->server.socket<0)
		return close_error(data,"server: 2nd server connection failed");


	poll_add(data->proxy->poll,data->server.socket,IN|OUT|EDGE|END,cb_server,data,"proxy/server");
	if(data->client.read.len){
		data->server.can_write=1;
		data->server.write=data->client.read;
		data->client.read=(string){0};
		conn_write(&data->server);
	}
	else{
		data->client.write=c_("HTTP/1.1 200 OK\r\n\r\n");
		data->client.can_write=1;
		conn_write(&data->client);
	}
	poll_update(data->proxy->poll,data->client.socket,cb_client,0);
	return 0;
}
int cb_clientfirstin(Proxysub* sub,int mask){
	sub->updated=time_sec();
	if(mask&END){
		close_error(sub,"client end before receiving anything");
		return 0;
	}
	if(mask&OUT) sub->client.can_write=1;
	if(!(mask&IN)) return 0;
	sub->client.can_read=1;

	conn_read(&sub->client);
	if(sub->server.port){
		log_error("more client data after header");
		return 0;
	}

	if(!s_has(sub->client.read,"\r\n\r\n")){
		if(sub->client.read.len>8*1024)
			return close_error(sub,"request header not found after reading 8kb");
		return 0;
	}
	string fullheader=own(&sub->client.read);
	string header=cut_header(&sub->client.read);	
	map req=header_m(header);

	if(!req.keys.len){
		_free(&fullheader);
		map_free(&req);
		_free(&header);
		return close_error(sub,"parsing header failed");
	}

	Method method=method_id(map_c(req,"method"));
	sub->server.host=map_c(req,"host");
	if(!sub->server.host.len || method==NONE){
		_free(&fullheader);
		map_free(&req);
		_free(&header);
		return close_error(sub,"host or method is wrong");
	}
	map_free(&req);
	pair hostport=cut(sub->server.host,':');
	sub->server.port=hostport.tail.len ? atoi(hostport.tail.str) : 80;
	sub->server.host=s_copy(hostport.head);
	string name=ro(sub->server.host);
	_free(&header);

	if(method!=CONNECT){
		_free(&sub->client.read);
		sub->client.read=fullheader;
	}	
	else{
		_free(&fullheader);
	}
	if(sub->proxy->sites.len){
		if(!site_allowed(sub->proxy->sites,name)){
			msg("- %v",name);
			proxysub_free(sub);
			return 0;
		}
		else msg("+ %v",name);
	}

	int attrs=0;

	sub->dnsreq=dns_resolve(sub->proxy->dns,name,dns_resolved,sub,sub->proxy->ipv6);
	return 1;
}
int ends_s(string in, string by){
	return in.len>=by.len && memcmp(in.str+in.len-by.len,by.str,by.len)==0;
}
int site_allowed(vector list, string name){
	for(int i=0; i<list.len; i++){
		if(ends_s(name,list.var[i])) return 1;
	}
	return 0;
}
int cb_idletimeout(void* data,int mask){
	log_error("idle timeout");
	proxysub_free(data);
	return 0;
}
int cb_newclient(Proxy* proxy,int mask){
	proxy->updated=time_sec();
	if(mask&END){
		msg("http ended");
		proxy->stop=1;
		return -1;
	}
	Proxysub sub={
		.client.socket=accept(proxy->http,NULL,NULL),
		.created=time_sec(),
		.updated=time_sec(),
		.proxy=proxy
	};
	//msg("incoming socket %d concurrent %d",sub.client.socket,proxy->total+1);
	if(sub.client.socket<0){
		return 0;
	}
	conn_peer(&sub.client);
	if(ip_isnull(sub.client.ip)){
		app_error("sock_ip() failed to get client ip");
	}
	//out("%v connected",ip_s(sub.client.ip));
	if(!ipmask_match(proxy->permit,sub.client.ip)){
		out("%v ip not permitted",ip_s(sub.client.ip));
		close(sub.client.socket);
		return 0;
	}
	
	proxy->total++;
	sock_nonblock(sub.client.socket);
	poll_add(proxy->poll,sub.client.socket,IN|OUT|EDGE|END,cb_clientfirstin,to_heap(&sub,sizeof(sub)),"proxy/client");
	poll_add_timeout(proxy->poll,sub.client.socket,cb_idletimeout);
	poll_set_timeout(proxy->poll,sub.client.socket,proxy->timeout);
}
int ipmask_match(vector nets,IPNo ip){
	//disallow all ip v6
	//if(!ip.ip4 || ip.ip3!=0xFFFF0000) return 0;

	for(int i=0; i<nets.len; i++){
		IPMask* mask=getp(nets,i);
		if((ip.ip4 & mask->mask) == (mask->ip.ip4 & mask->mask)) return 1;
		if(!mask->mask && memcmp(&ip,&mask->ip,sizeof(IPNo))==0) return 1;
	}
	return 0;
}
vector s_ipmaskv(string in){
	vector ret=vec_new(IPMask,0);
	vector vals=s_vec(in,',');
	for(int i=0; i<vals.len; i++){
		IPMask v=s_ipmask(trim(vec_val(vals,i)));
		if(!v.ip.ip4) continue;
		add_type(IPMask,ret)=v;
	}
	vec_free(&vals);
	return ret;
}
IPMask s_ipmask(string in){
	pair sp=cut(in,'/');
	IPMask ret={
		.ip=s_ip(sp.head),
		.mask=((unsigned int)-1)>>(32-(sp.tail.str ? atoi(sp.tail.str) : 0))
	};
	_free(&in);
	return ret;
}
int is_yes(string in){
	if(!in.len || eq(s_lower(in),"yes") || eq(in,"1")) return 1;
	return 0;
}
void* proxy_new(map config,Poll* poll){
	string sitesraw=file_s(map_c(config,"sitelist"));
	Proxy proxy={
		.sitesraw = sitesraw,
		.sites    = s_vec(sitesraw,'\n'),
		.http     = listen_port(map_c_i(config,"http")),
		.socks    = listen_port(map_c_i(config,"socks")),
		.admin    = listen_port(map_c_i(config,"admin")),
		.permit   = s_ipmaskv(map_c(config,"permit")),
		.timeout  = max(10,map_c_i(config,"timeout")),
		.created  = time_sec(),
		.updated  = time_sec(),
		.ipv6     = is_yes(map_c(config,"ipv6")) ? 0 : IP4,
		.dns      = dns_new(config,poll),
		.poll     = poll,
		.config   = config
	};
	if(!proxy.http && !proxy.socks){
		log_error("neither http nor socks server started");
		return NULL;
	}
	if(!proxy.dns){
		log_error("finding nameserver from /etc/resolv.conf failed");
		return NULL;
	}
	void* ret=to_heap(&proxy,sizeof(proxy));
	if(proxy.http){
		poll_add(proxy.poll,proxy.http,IN|END,cb_newclient,ret,"listen/http/proxy");
		msg("http proxy: listening port %d",map_c_i(config,"http"));
	}
	if(proxy.socks){
		msg("socks proxy: listening port %d fd %d",map_c_i(config,"socks"),proxy.socks);
		poll_add(proxy.poll,proxy.socks,IN|END,cbsks_accept,ret,"listen/socks");

	}
	if(proxy.sites.len) msg("proxying : %d sites",proxy.sites.len);
	return ret;
}
void proxy_free(Proxy* proxy){
	poll_del(proxy->poll,proxy->http);
	poll_del(proxy->poll,proxy->socks);
	sock_free(proxy->http);
	sock_free(proxy->socks);
	dns_free(proxy->dns);
	vec_free(&proxy->sites);
	vec_free_ex(&proxy->permit,NULL);
	_free(&proxy->sitesraw);
	free(proxy);
}
int cb_proxystop(Proxy* in,int mask){
	log_error("stop signal");
	in->stop=1;
	return 1;
}
int proxy_run(map config){
	server_run(config,proxy_new,proxy_free);
}

enum SKSCommand {
	SKS_CONNECT=1
};
typedef enum {
	SKS_IP4=1,
	SKS_DOMAIN=3,
	SKS_IP6=4
} SKSIP;
struct s4_req {
	char v;
	char command;
	char x;
	char type;
};

int cbsks_accept(Proxy* proxy,int mask){
	proxy->updated=time_sec();
	if(mask&END){
		proxy->stop=1;
		return -1;
	}
	Proxysub psub={
		.client.socket=accept(proxy->socks,NULL,NULL),
		.created=time_sec(),
		.updated=time_sec(),
		.proxy=proxy
	};
	if(psub.client.socket<0){
		log_error("bad connection listening sock %d accepted sock_fd %d",proxy->socks,psub.client.socket);
		return 0;
	}
	conn_peer(&psub.client);
	if(!ipmask_match(proxy->permit,psub.client.ip)){
		out("%v ip not permitted",ip_s(psub.client.ip));
		close(psub.client.socket);
		return 0;
	}
	proxy->total++;
	sock_nonblock(psub.client.socket);
	poll_add(proxy->poll,psub.client.socket,IN|END,cbsks_auth,to_heap(&psub,sizeof(psub)),"socks/client");
	poll_add_timeout(proxy->poll,psub.client.socket,cb_idletimeout);
	poll_set_timeout(proxy->poll,psub.client.socket,proxy->timeout);
	return 0;
}

// https://www.rfc-editor.org/rfc/rfc1928

int cbsks_auth(Proxysub* in,int mask){
	if(mask&END){
		proxysub_free(in);
		return 0;
	}
	if(!(mask&IN)) return 0;
	in->client.can_read=1;
	in->client.can_write=1;
	conn_read(&in->client);
	if(!in->client.read.len) return 0; //continue;
	if(in->client.read.len<3) return sks_baddata(in,"too short auth req");
	char version=in->client.read.str[0];
	if(version!=4 && version!=5) return sks_baddata(in,"malformed auth req");
	if(in->client.read.len!=(unsigned char)in->client.read.str[1]+2) return sks_baddata(in,"auth method list is broken");
	_free(&in->client.read);
	if(!map_c(in->proxy->config,"password").len){
		char res[2]={5,0};
		in->client.write=cl_(res,sizeof(res));
		conn_write(&in->client);
		poll_update(in->proxy->poll, in->client.socket, cb_sksreceive,IN|END);
	}
	else{
		char res[2]={5,2};
		in->client.write=cl_(res,sizeof(res));
		conn_write(&in->client);
		poll_update(in->proxy->poll, in->client.socket, cb_varifypass,IN|END);
	}
	return 0;
}

// https://www.rfc-editor.org/rfc/rfc1929

int cb_varifypass(Proxysub* in,int mask){
	if(mask&END){
		proxysub_free(in);
		return 0;
	}
	if(!(mask&IN)) return 0;
	in->client.can_read=1;
	in->client.can_write=1;
	conn_read(&in->client);
	string s=ro(in->client.read);
	if(s.len<3) return sks_baddata(in,"user/pass not provided");
	if(s.str[0]!=1) return sks_baddata(in,"user/pass type is not 1");
	int len=(unsigned char)s.str[1];
	s=sub(s,2,s.len);
	string username=sub(s,0,len);
	s=sub(s,len,s.len);
	len=(unsigned char)s.str[0];
	s=sub(s,1,s.len);
	string password=s;
	if(len!=password.len) sks_baddata(in,"password len is bad");
	pair userpass=cut(map_c(in->proxy->config,"password"),':');
	if(!eq_s(username,userpass.head)){
		char res[2]={1,1};
		in->client.write=cl_(res,sizeof(res));
		conn_write(&in->client);
		log_error("user error [%v]",username);
		proxysub_free(in);
		return 0;
	}
	if(!s_caseeq(password,userpass.tail)){
		char res[2]={1,1};
		in->client.write=cl_(res,sizeof(res));
		conn_write(&in->client);
		log_error("pass error [%v]",password);
		proxysub_free(in);
		return 0;
	}
	_free(&in->client.read);
	char res[2]={1,0};
	in->client.write=cl_(res,sizeof(res));
	conn_write(&in->client);
	poll_update(in->proxy->poll, in->client.socket, cb_sksreceive,IN|END);
}
int sks_baddata(Proxysub* psub,char* s){
	log_error("baddata socks: %s",s);
	proxysub_free(psub);
	return 0;
}
int cb_sksreceive(Proxysub* in,int mask){
	if(mask&END){
		proxysub_free(in);
		return 0;
	}
	if(!(mask&IN)) return 0;
	in->client.can_read=1;
	in->client.can_write=1;
	conn_read(&in->client);
	string r=ro(in->client.read);
	if(r.len<sizeof(struct s4_req)) return sks_baddata(in,"request header too short");
	struct s4_req* req=r.ptr;
	r=sub(r,sizeof(struct s4_req),r.len);

	if(req->command!=SKS_CONNECT){
		return error("socks : given command unsupported");
	}
	if(req->type==SKS_IP4){
		if(r.len<6) return sks_baddata(in,"malformed IP4 addressed");
		IPNo ip={0};
		int port=ntohs(*(unsigned short*)(r.str+4));
		ip.ip4=*(int*)r.ptr;
		ip.ip3=0xFFFF0000;
		in->server.host=ip_s(ip);
		in->server.port=port;
		_free(&in->client.read);
		return sks_connect(in,ip);
	}
	if(req->type==SKS_IP6){
		if(r.len<18) return sks_baddata(in,"malformed IP6 addressed");
		IPNo ip={0};
		int port=ntohs(*(unsigned short*)(r.str+16));
		ip=*(IPNo*)r.ptr;
		in->server.host=ip_s(ip);
		in->server.port=port;
		_free(&in->client.read);
		return sks_connect(in,ip);
	}
	else if(req->type==SKS_DOMAIN){
		if(r.len<1) return sks_baddata(in,"domian name too short");
		int len=(unsigned char)r.str[0];
		if(!len || r.len<1+len+2) return sks_baddata(in,"malformed domain name");
		in->server.host=s_copy(sub(r,1,len));
		in->server.port=ntohs(*(unsigned short*)(r.str+1+len));
		_free(&in->client.read);
		dns_resolve(in->proxy->dns,in->server.host,sks_connect,in,in->proxy->ipv6);
		return 0;
	}
	else sks_baddata(in,"ip address type invalid");
}
int sks_connect(Proxysub* in, IPNo ip){
	if(ip_isnull(ip)){
		msg("%v",in->server.host);
		return sks_baddata(in,"DNS resolve error");
	}
	in->server.ip=ip;
	in->server.type=ASYNC;
	int ret=conn_connect(&in->server);
	if(ret==-1)
		return sks_baddata(in,"connecting with server failed");
	
	poll_add(in->proxy->poll,in->server.socket,IN|OUT|EDGE|END,sks_tryip4,in,"socks/server");
	return 0;
}
int sks_tryip4(Proxysub* in,int mask){
	if(!(mask&END) || in->proxy->stop) return sks_sendok(in,mask);
	if(in->server.ip.ip3==0xFFFF0000 || s_has(in->server.host,":")){
		return sks_baddata(in,"server connection failed to start even at IP4 host");
	}
	dnscache_del(in->proxy->dns,ro(in->server.host));
	string host=own(&in->server.host);
	int port=in->server.port;
	poll_del(in->proxy->poll,in->server.socket);
	conn_free(&in->server);
	in->server=(Connection){
		.host=host,
		.port=port
	};
	dns_resolve(in->proxy->dns,in->server.host,sks_connect,in,IP4);
	return 0;
}
int sks_sendok(Proxysub* in,int mask){
	if(mask&END){
		return sks_baddata(in,"server2 connection failed to start");
	}
	string res={0};
	if(in->server.ip.ip3==0xFFFF0000){
		res=s_new(NULL,10);
		res.str[0]=5;
		res.str[3]=SKS_IP4;
		memcpy(res.ptr+4,&in->server.ip,4);
		memcpy(res.ptr+8,&in->server.port,2);
	}
	else{
		res=s_new(NULL,22);
		res.str[0]=5;
		res.str[3]=SKS_IP6;
		memcpy(res.ptr+4,&in->server.ip,sizeof(IPNo));
		memcpy(res.ptr+20,&in->server.port,2);
	}
	in->client.write=res;
	poll_update(in->proxy->poll,in->client.socket,cb_client,IN|OUT|EDGE|END);
	poll_update(in->proxy->poll,in->server.socket,cb_server,IN|OUT|EDGE|END);
	conn_write(&in->client);
	poll_set_timeout(in->proxy->poll,in->client.socket,in->proxy->timeout);
	return 0;
}
