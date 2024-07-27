#pragma once
#define _GNU_SOURCE
#include <sys/wait.h>
#include <sys/epoll.h>
#include <time.h>
#include <fcntl.h>
#include <sys/signalfd.h>
#include "cstr.h"
#include "connection.h"
typedef struct {
	char* name;
	void* data;
	void* callback;
	struct epoll_event event;
	int fd;
	int poll;
	int signo;
	int timeout;
	void* timeout_callback;
	int timeout_repeat;
	double sec;
} Pollitem;
typedef struct {
	double time;
	double work;
	double free;
	double load;
} Load;
typedef struct {
	char* name;
	map items;
	int poll;
	sigset_t signals;
	struct epoll_event wakeevent;
	Load load;
	int timeout;
	int shutdown;
} Poll;
typedef enum {
	IN=EPOLLIN,
	OUT=EPOLLOUT,
	EDGE=EPOLLET,
	END=EPOLLRDHUP|EPOLLHUP
} Eventmask;
int poll_run(Poll* poll);
void poll_free(Poll* poll);
Poll poll_new(char* name);
void pollitem_free_ptr(Pollitem* in);
void poll_update(Poll* poll,int fd,void* callback,int mask);
void poll_del(Poll* poll,int fd);
void poll_del_signal(Poll* poll,int signo);
void poll_add_signal(Poll* poll,int signo,void* callback,void* data);
int poll_set_repeat(Poll* poll, int fd, int timeout);
int poll_add_timeout(Poll* poll, int fd, void* callback);
int poll_set_timeout(Poll* poll, int fd, int timeout);
Pollitem* poll_add(Poll* poll,int fd,Eventmask mask,void* callback,void* data,char* name);
void time_init();
double time_sec();
