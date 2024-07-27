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
//#include <openssl/ssl.h>


#include "cstr.h"
#include "ssl.h"
#include "connection.h"

#define sys_error() _sys_error(__FILE__,__LINE__)


/*header
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
end*/

int _sys_error(char* file,int line){
	printf("ERROR: %s (%d) in %s line %d\n",strerror(errno),errno,file,line);
	return -1;
}
int bind6(int port){
	struct sockaddr_in6 address={0};
	address.sin6_family = AF_INET6;
	address.sin6_addr = in6addr_any;
	address.sin6_port = htons(port);

	int sock = socket(AF_INET6, SOCK_STREAM, 0);
	if(sock<0) return -1;
	int ret=setsockopt(sock,SOL_SOCKET,SO_REUSEADDR|SO_REUSEPORT,&(int){1},sizeof(int));
	if(ret) return -1;

	ret=bind(sock,(struct sockaddr *)&address,sizeof(address));
	if(ret<0) return -1;
	return sock;
}
int bind4(int port){
    struct sockaddr_in address={0};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if(sock<0) return -1;
	int ret=setsockopt(sock,SOL_SOCKET,SO_REUSEADDR|SO_REUSEPORT,&(int){1},sizeof(int));
	if(ret) return -1;

	ret=bind(sock,(struct sockaddr *)&address,sizeof(address));
	if(ret<0) return -1;
	return sock;
}

IPNo s_ip(string ip){
	ip=s_nullterm(ip);
	IPNo ret={0};
	int err=0;
	if(s_chr(ip,'.')){
		err=inet_pton(AF_INET,ip.str,&ret.ip4);
		ret.ip3=0xFFFF0000;
	}
	else{
		err=inet_pton(AF_INET6,ip.str,&ret);
	}
	_free(&ip);
	return err<=0 ? (IPNo){0} : *(IPNo*)&ret;
}
int is_ip4(IPNo ip){
	return ip.ip3==0xFFFF0000;
}
string ip_s(IPNo ip){
	if(is_ip4(ip)){
		char ret[INET_ADDRSTRLEN]={0};
		return inet_ntop(AF_INET,&ip.ip4,ret,INET_ADDRSTRLEN) ? c_copy(ret) : (string){0};
	}
	char ret[INET6_ADDRSTRLEN]={0};
	return inet_ntop(AF_INET6,&ip,ret,INET6_ADDRSTRLEN) ? c_copy(ret) : (string){0};
}

int sock_new(int port){
	return socket(AF_INET, SOCK_STREAM, 0);
}
int sock6_new(int port){
	return socket(AF_INET6, SOCK_STREAM, 0);
}

int listen_port(int port){
	if(!port) return 0;
	int sock=bind6(port);
	if(sock==-1){
		msg("bind with ipv6 failed");
		sock=bind4(port);
	}
	if(sock==-1) return -1;
	signal(SIGPIPE, SIG_IGN); //you will get errno=EPIPE now.
	int ret=listen(sock,256); //2nd number is back log queue size
	if(ret<0) return -1;
	return sock;
}
int sock_nonblock(int conn){
	int ret=fcntl(conn,F_GETFL,0);
	if(ret==-1) return ret;
	ret=fcntl(conn,F_SETFL,ret|O_NONBLOCK);
	if(ret) return -1;
}
void sock_free(int sock){
	if(!sock) return;
	shutdown(sock,SHUT_RDWR);
	close(sock);
}

int msleep(int msec){
	usleep(msec*1000);
}
int conn_write(Connection* conn){
	int ret=0;
	if(!conn->can_write) return ret;
	if(!conn->write.len) return ret;

	while(conn->write.len){
		int total=conn->ssl ? SSL_write(conn->ssl,conn->write.str,conn->write.len) : write(conn->socket,conn->write.str,conn->write.len);
		int myerr=errno;

		if(total<=0){
			if(!(conn->type & UDP)){
				conn->can_write=1;
				if(myerr!=EAGAIN && errno!=EINPROGRESS){
					msg("sock write error");
					sys_error();
					conn->is_error=1;
				}
			}
			return ret;
		}
		ret+=total;
		conn->write=chop(conn->write,total);
	}
	return ret;
}
int conn_peer(Connection* conn){
	struct sockaddr_in6 ret={0};
	socklen_t len=sizeof(ret);
	int err=getpeername(conn->socket,&ret,&len);
	if(err) return 0;
	conn->ip=*(IPNo*)&ret.sin6_addr.s6_addr;
	conn->port=ret.sin6_port;
	return 1;
}
int conn_read(Connection* conn){
	int ret=0;
	if(!conn->can_read) return ret;
	if(conn->read.len>=32*1024) return ret;
	for(;;){
		string buff={0};
		buff=s_new(NULL,1024*8);
		int len=conn->ssl ? SSL_read(conn->ssl,buff.str,buff.len) : read(conn->socket, buff.str,buff.len);
		if(len<=0){
			if(!(conn->type & UDP)){
				conn->can_read=0;
				if(errno!=EAGAIN && errno!=EINPROGRESS){
					msg("sock %d read error",conn->socket);
					sys_error();
					conn->is_error=1;
				}
			}
			_free(&buff);
			return ret;
		}
		ret+=len;
		buff.len=len;
		conn->read=cat(conn->read,buff);
		if(conn->read.len>=32*1024) break;
	}
	return ret;
}
int conn_readonce(Connection* conn){
	if(!conn->can_read) return 0;
	string buff=s_new(NULL,1024*8);
	int len=conn->ssl ? SSL_read(conn->ssl,buff.str,buff.len) : read(conn->socket, buff.str,buff.len);
	if(len<=0){
		conn->can_read=0;
		if(errno!=EAGAIN && errno!=EINPROGRESS){
			msg("sock %d read error",conn->socket);
			sys_error();
			conn->is_error=1;
		}
		_free(&buff);
		return 0;
	}
	buff.len=len;
	conn->read=cat(conn->read,buff);
	return len;
}

int connect4(Connection* conn){
	int sock_fd = socket(AF_INET, conn->type & UDP ? SOCK_DGRAM : SOCK_STREAM, 0);
	if(sock_fd<0) return -1;
	if(conn->type & ASYNC) sock_nonblock(sock_fd);
	struct sockaddr_in servaddr={
		.sin_family = AF_INET,
		.sin_port = htons(conn->port),
		.sin_addr = *(struct in_addr*)&conn->ip.ip4
	};
	int ret=connect(sock_fd, &servaddr, sizeof(servaddr));
	if(ret<0 && errno!=EINPROGRESS) return -1;
	return sock_fd;
}
int connect6(Connection* conn){
	int sock_fd = socket(AF_INET6, conn->type & UDP ? SOCK_DGRAM : SOCK_STREAM, 0);
	if(sock_fd<0) return -1;
	if(conn->type & ASYNC) sock_nonblock(sock_fd);
	struct sockaddr_in6 servaddr={
		.sin6_family = AF_INET6,
		.sin6_port = htons(conn->port),
		.sin6_addr = *(struct in6_addr*)&conn->ip
	};
	int ret=connect(sock_fd, &servaddr, sizeof(servaddr));
	if(ret<0 && errno!=EINPROGRESS) return -1;
	return sock_fd;	
}
int conn_connect(Connection* conn){
	conn->is_error=1;
	int ret=connect6(conn);
	if(ret==-1 && is_ip4(conn->ip)){
		msg("ipv6 connection failed, trying ipv4");
		ret=connect4(conn);
	}
	if(ret==-1){
		sys_error();
		string temp=ip_s(conn->ip);
		msg("ip %v",temp);
		_free(&temp);
		return -1;
	}
	conn->socket=ret;
	conn->can_read=1;
	conn->can_write=1;
	conn->is_error=0;
	conn_peer(conn);
	return 0;
}
Connection connection_new(IPNo ip,int port,ConnectionType type){
	Connection conn={
		.ip=ip,
		.port=port,
		.type=type
	};
	conn_connect(&conn);
	return conn;
}
void conn_free(Connection* conn){
	if(!conn->socket) return;
	if(conn->ssl) ssl_close(conn->ssl);
	//msg("closing socket %d",conn->socket);
	shutdown(conn->socket,SHUT_RDWR);
	close(conn->socket);
	//conn->socket=0;
	_free(&conn->read);
	_free(&conn->write);
	if(conn->host.readonly){
		msg("ERROR: conn.host.str is readonly");
	}
	_free(&conn->host);
}
int unix_listen(string file){
	struct sockaddr_un address={0};
	address.sun_family = AF_UNIX;
	memcpy(address.sun_path,file.str, file.len);
	address.sun_path[file.len]=0;
	int fd=socket(AF_UNIX, SOCK_STREAM, 0);
	if(fd<0) return sys_error();
	int ret=bind(fd, (void *)&address, SUN_LEN(&address));
	if(ret<0) return sys_error();
	ret=listen(fd,256);
	if(ret<0) return sys_error();
	return fd;
}
int unix_connect(string file){
	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if(fd<0) return sys_error();
	struct sockaddr_un address={0};
	address.sun_family = AF_UNIX;
	memcpy(address.sun_path,file.str, file.len);
	address.sun_path[file.len]=0;
	int ret=connect(fd,(void*)&address, SUN_LEN(&address));
	if(ret<0) return sys_error();
	return fd;
}
