#pragma once
#include "poll.h"
#include "config.h"
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
int usage(string name);
int cmdline_run(int argc,char** argv,string name,void* build,void* free);
int server_run(map config,void* build,void* free);
int cb_sigterm(int* stop,int mask);
int worker_exit(int pid);
vector worker_fork(int total);
