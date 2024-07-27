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
#include "dns.h"


/*header
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

end*/

void dnscache_del(dns_resolver* res,string host){
	map_del_ex(&res->cache,host,NULL);
}
void dnscache_add(map* cache,string host,IPNo ip){
	double now=time_sec();
	for(int i=0; i<cache->vals.len; i++){
		DNSCache* v=getp(cache->vals,i);
		if(v->time<now-120){
			map_del_idx(cache,i,NULL);
			i--;
			continue;
		}
		else break;
	}
	DNSCache row={
		.ip=ip,
		.time=time_sec()
	};
	*(DNSCache*)map_add_ex(cache,host,NULL)=row;
}
IPNo dnscache_get(map* cache,string host){
	DNSCache* ret=map_getp(*cache,host);
	if(!ret) return (IPNo){0};
	if(ret->time<time_sec()-120){
		map_del_ex(cache,host,NULL);
		return (IPNo){0};
	}
	return ret->ip;
}

int dns_callback(DNSPolldata* in,IPNo ip){
	DNSPolldata data=*in;
	free(in);
	poll_del(data.resolver->poll,data.conn.socket);
	conn_free(&data.conn);
	for(int i=0; i<data.resolver->polldata.len; i++){
		if(data.resolver->polldata.var[i].ptr==in){
			vec_del_ex(&data.resolver->polldata,i,1,NULL);
			break;
		}
	}
	if(data.cancel) return 0;
	return ((int(*)(void*,IPNo))(data.callback))(data.data,ip);
}
void dns_cancel(DNSPolldata* data){
	if(data) data->cancel=1;
}
int dns_timeout(DNSPolldata* data, int mask){
	msg("DNS timeout");
	dns_callback(data,(IPNo){0});
}
int dns_received(DNSPolldata* data,int mask){
	if(mask & END){
		msg("dns call shutdown");
		free(data);
		return 0;
	}
	if(!(mask & IN)) return 0;
	data->conn.can_read=1;
	conn_read(&data->conn);
	IPNo ip=dnsresponse_ip(data->conn.read);
	if(ip_isnull(ip) && !(data->type & IP4)){ //ip6 failed, try ip4 only
		conn_read(&data->conn);
		_free(&data->conn.write);
		_free(&data->conn.read);
		data->type=data->type | IP4;
		data->type=data->type & ~IP6;
		data->conn.write=dns_query_packet(ro(data->name),0);
		int ret=conn_write(&data->conn);
		return 0;
	}
	dnscache_add(&data->resolver->cache,s_copy(data->name),ip);
	dns_callback(data,ip);
}
void dns_free(dns_resolver* in){
	map_free_ex(&in->cache,NULL);
	free(in);
}
void* dns_new(map config,Poll* poll){
	IPNo nameserver=find_nameserver(map_c(config,"dns"));
	if(ip_isnull(nameserver)) return NULL;
	dns_resolver ret={
		.nameserver=nameserver,
		.cache=map_new_ex(sizeof(DNSCache)),
		.config=config,
		.poll=poll,
		.polldata=NullVec
		
	};
	return to_heap(&ret,sizeof(ret));
}
IPNo find_nameserver(string server){
	if(server.len) return s_ip(server);
	string file=file_s(c_nullterm("/etc/resolv.conf"));
	vector lines=s_vec(file,'\n');
	IPNo ret={0};
	for(int i=0; i<lines.len; i++){
		if(s_start(lines.var[i],"nameserver")){
			ret=s_ip(cut(lines.var[i],' ').tail);
			break;
		}
	}
	vec_free(&lines);
	_free(&file);
	return ret;
}
void* dns_resolve(dns_resolver* resolver, string name,void* callback,void* data,int type){
	IPNo ip=dnscache_get(&resolver->cache, name);
	if(!ip_isnull(ip)){
		((int(*)(void*,IPNo))(callback))(data,ip);
		return NULL;
	}

	DNSPolldata polldata={
		.conn=dns_query(resolver,ro(name),ASYNC|type),
		.data=data,
		.name=name,
		.type=type,
		.resolver=resolver,
		.callback=callback
	};
	DNSPolldata* pdata=to_heap(&polldata,sizeof(polldata));
	add(resolver->polldata)=ptr_(pdata);
	poll_add(resolver->poll, polldata.conn.socket, IN|END, dns_received, pdata,"dns/call");
	conn_write(&pdata->conn);
	poll_add_timeout(resolver->poll,polldata.conn.socket,dns_timeout);
	poll_set_timeout(resolver->poll,polldata.conn.socket,8);
	return pdata;
}
IPNo dns_resolve_sync(dns_resolver* resolver,string name){
	Connection conn=dns_query(resolver,ro(name),0);
	conn_write(&conn);
	conn_readonce(&conn);
	IPNo ret=dnsresponse_ip(conn.read);
	if(ip_isnull(ret)){
		conn.write=dns_query_packet(ro(name),0);
		conn_write(&conn);
		_free(&conn.read);
		conn_readonce(&conn);
		ret=dnsresponse_ip(conn.read);
	}
	_free(&name);
	conn_free(&conn);
	return ret;
}
string dns_query_packet(string host,ConnectionType type){
	HEADER header={
		.rd = 1,
		.qdcount = htons(1),
	};
	string hostname=host_binary(host);
	Q_FLAGS qflags={
		.qtype=htons(type & IP4 ? DNS_A : DNS_AAAA),
		.qclass=htons(1)
	};
	return cat_all(cl_(&header,sizeof(header)),hostname,cl_(&qflags,sizeof(qflags)));
}
Connection dns_query(dns_resolver* resolver,string host,ConnectionType type){
	Connection conn=connection_new(resolver->nameserver,53,UDP|type);
	conn.write=dns_query_packet(host,type);
	return conn;
}
vector dnsresponse_v(string res){
	string main=res;
	vector ret=NullVec;
	if(!main.len) return ret;
	HEADER header = *(HEADER*)res.str;
	header.ancount=ntohs(header.ancount);
	if(!header.ancount) return ret;
	res=tail(res,sizeof(HEADER));
	string name=c_(res.str);
	res=tail(res,sizeof(Q_FLAGS)+name.len+1);

	for(int i = 0; i < header.ancount; i++){
		int len=ntohs(*(unsigned short*)(res.str+10))+12;
		add(ret)=head(res,len);
		res=tail(res,len);
	}
	return ret;
}
int ip_isnull(IPNo in){
	return !in.ip1 && !in.ip2 && !in.ip3 && !in.ip4;
}
IPNo dnsresponse_ip(string res){
	IPNo ip={0};
	vector ret=dnsresponse_v(res);
	if(!ret.len) return ip;
	for(int i=0; i<ret.len; i++){
		string val=ret.var[i];
		RR_FLAGS rrflags=*(RR_FLAGS*)(val.str+2);
		rrflags.type=ntohs(rrflags.type);
		string data=tail(val,12);
		if(rrflags.type==DNS_A){
			ip.ip4=*(int*)(data.str);
			ip.ip3=0xFFFF0000;
		}
		else if(rrflags.type==DNS_AAAA){
			ip=*(IPNo*)(data.str);
		}
	}
	vec_free(&ret);
	return ip;
}

// www.apple.com into 3www5apple3com0
string host_binary(string host){
	string ret={0};
	vector parts=s_vec(host,'.');
	string* rows=parts.ptr;
	for(int i=0; i<parts.len; i++){
		add_type(char,ret)=rows[i].len;
		ret=cat(ret,rows[i]);
	}
	add_type(char,ret)=0;
	vec_free_ex(&parts,NULL);
	_free(&host);
	return ret;
}

string binary_host(string in,string all) {
	string ret={0};
	int i=0;
	for(;;){
		if(!in.str[i]) break;
		if(in.str[i]==0xC0){
			i=in.str[i+1];
			in=all;
			continue;
		}
		string ssub=sub(in,i+1,in.str[i]);
		if(ret.len) add_type(char,ret)='.';
		ret=cat(ret,ssub);
		i+=in.str[i]+1;
	}
	return ret;
}
