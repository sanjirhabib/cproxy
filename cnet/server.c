#include "poll.h"
#include "config.h"
#include "server.h"

/*header
typedef enum {
	//Error=-1,
	Waiting=0,
	Processing,
	Keepopen,
	Finished
} status;
typedef struct {
	Connection conn;
	int(*is_ready)(string in);
	void* read_header;
	void* process;
	void(*free)(void* data);
	void* data;
	status status;
} call_chain;
end*/

//int server_handle(int sock,Poll* poll,call_chain* chain){
//	if(!sock) return 0;
//	poll_add(poll,sock,IN|END,cb_accept,ret,"listen/http");
//}
//int cb_accept(server* in,int mask){
//	int socket=accept(in->socket,NULL,NULL);
//	sock_nonblock(socket);
//	poll_add(in->poll,sub.client.socket,IN|OUT|EDGE|END,cb_process,to_heap(&sub,sizeof(sub)),"http/client");
//}
//int cb_process(call_chain* chain, int mask){
//	if(mask & END){
//		chain->free(chain->data);
//		return 0;
//	}
//	if(mask & IN) chain->conn.can_read=1;
//	if(mask & OUT) chain->conn.can_write=1;
//
//	if(chain->status==Waiting && !chain->conn.can_write) return 0;
//
//	if(chain->conn.write.len){
//		int ret=conn_write(&chain->conn);
//		if(chain->conn.is_error){
//			chain->free(chain->data);
//			return 0;
//		}
//	}
//	int ret=conn_read(&chain->conn);
//	if(chain->conn.is_error){
//		chain->free(chain->data);
//		return 0;
//	}
//	if(!ret) return 0;
//	if(chain->status==Waiting){
//		if(!chain->is_ready(chain->conn.read)) return 0;
//		ret=chain->read_header(chain->conn.read,chain->data);
//		if(ret==-1){
//			chain->free(chain->data);
//			return 0;
//		}
//		chain->status=Processing;
//	}
//	if(chain->conn.read.len){
//		ret=chain->process_body(chain->conn.read,chain->data);
//
//		if(ret==-1){
//			chain->free(chain->data);
//			return 0;
//		}
//
//		else if(!ret)
//			chain->status=Finished;
//
//		else if(ret==1)
//			chain->status=Keepopen;
//		
//	}
//	if(chain->conn.write.len){
//		conn_write(&chain->conn);
//		if(chain->conn.is_error){
//			chain->free(chain->data);
//			return 0;
//		}
//	}
//	if(chain->status==Finished && !chain->conn.write.len){
//		chain->free(chain->data);
//		return 0;
//	}
//	if(chain->status==Keepopen && !chain->conn.write.len){
//		chain->status=Waiting;
//		_free(&chain->conn.read);
//		return 0;
//	}
//}
int usage(string name){
	out("Usage: %v <server.conf>",name);
	return 0;
}
int cmdline_run(int argc,char** argv,string name,void* build,void* free){
	if(argc<2) return usage(name);
	string file=file_s(c_(argv[1]));
	map config=config_m(file,name);
	if(!config.keys.len){
		msg("config file read failed");
		return -1;
	}
	map_print(config);
	server_run(config,build,free);
	map_free(&config);
	_free(&file);
	return 0;
}
int server_run(map config,void* build,void* free){
	worker_fork(map_c_i(config,"workers"));
	Poll poll=poll_new("server");
	void* server=((void*(*)(map,Poll*))build)(config,&poll);

	int stop=0;
	poll_add_signal(&poll,SIGTERM,cb_sigterm,&stop);
	poll_add_signal(&poll,SIGINT,cb_sigterm,&stop);

	while(!stop){
		poll_run(&poll);
	}

	msg("server: server freeing");

	poll_del_signal(&poll,SIGTERM);
	poll_del_signal(&poll,SIGINT);
	poll_free(&poll);
	((void(*)(void*))free)(server);
	return 0;
}
int cb_sigterm(int* stop,int mask){
	msg("stop signal");
	*stop=1;
	return 1;
}
int worker_exit(int pid){
	int isexit=0;
	if(pid && waitpid(pid,&isexit,WNOHANG)>0 && WIFEXITED(isexit)){
		msg("worker exit detected");
		return 1;
	}
	return 0;
}
vector worker_fork(int total){
	if(total>64) total=64;
	vector ret=vec_new(int,0);
	for(int i=0; i<total-1; i++){
		int pid=fork();
		if(!pid){
			msg("worker %d started",i);
			return (vector){0};
		}
		else{
			add_type(int,ret)=pid;
		}
	}
	return ret;
}
