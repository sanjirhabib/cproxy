#define _GNU_SOURCE
#include <sys/wait.h>
#include <sys/epoll.h>
#include <time.h>
#include <fcntl.h>
#include <sys/signalfd.h>

#include "cstr.h"
#include "connection.h"
#include "poll.h"

/*header
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
end*/

int poll_run(Poll* poll){
	struct epoll_event wakeevent[20]={0};
	double wait=time_sec();
	int total=epoll_pwait(poll->poll,wakeevent,sizeof(wakeevent)/sizeof(struct epoll_event),1000,&poll->signals);
	poll->load.free+=time_sec()-wait;
	poll->load.work+=wait-poll->load.time;
	if(poll->load.work+poll->load.free>10){
		poll->load.load=poll->load.work/(poll->load.work+poll->load.free);
		poll->load.work=0;
		poll->load.free=0;
	}
	poll->load.time=time_sec();
	
	int ret=0;
	for(int i=0; i<total; i++){
		if(poll->shutdown) break;
		int id=wakeevent[i].data.u32;
		if(!id) continue;
		Pollitem* item=map_igetp(poll->items,id);
		if(!item){
			msg("ERROR! awake #%d with invalid id %d on event r:%d w:%d c:%d",i,id,wakeevent[i].events & IN,wakeevent[i].events & OUT,wakeevent[i].events & END);
			epoll_ctl(poll->poll, EPOLL_CTL_DEL, id, NULL);
			continue;
		}
		if(!(wakeevent[i].events & item->event.events)){
			msg("poll: wake on wrong event, sleeping again, socket %d",item->fd);
			continue;
		}
		if(item->callback){
			ret=((int(*)(void*,int))(item->callback))(item->data,wakeevent[i].events);
		}
	}
	int sec=time_sec();
	if(poll->timeout<=sec) return 0;
	poll->timeout=sec+1;
	for(;;){
		if(poll->shutdown) break;
		int found=0;
		Pollitem* item=poll->items.vals.ptr;
		for(int i=0; i<poll->items.keys.len; i++){
			if(!item[i].timeout||item[i].timeout>sec) continue;

			item[i].timeout=0;

			if(!item[i].timeout_callback)
				app_error("timeout without a callback");				
			
			//msg("timer i %d timeout %d callback %p repeat %d",i,item[i].timeout,item[i].timeout_callback,item[i].timeout_repeat);
			if(item[i].timeout_repeat) item[i].timeout=item[i].timeout_repeat+sec;
			((int(*)(void*,int))item[i].timeout_callback)(item[i].data,END);
			found=1;
			break;
		}
		if(!found) break;
	}
	return ret;
}
void poll_free(Poll* poll){
	poll->shutdown=1;
	while(poll->items.keys.len){
		Pollitem* item=poll->items.vals.ptr;
		int last=poll->items.keys.len-1;
		((int(*)(void*,int))item[last].callback)(item[last].data,END);
		if(poll->items.keys.len>last){
			msg("connections closed. remaining %d items",poll->items.keys.len);
			for(int i=0; i<poll->items.keys.len; i++){
				printf("%s ",item[i].name);
			}
			printf("\n");
			break;
		}
	}
	map_free_ex(&poll->items,pollitem_free_ptr);
}

Poll poll_new(char* name){
	sigset_t signals={0};
	sigemptyset(&signals);
	return (Poll){
		.name=name,
		.signals=signals,
		.load.time=time_sec(),
		.poll=epoll_create1(0),
		.items=map_new_ex(sizeof(Pollitem))
	};
}
void pollitem_free_ptr(Pollitem* in){
	int ret=epoll_ctl(in->poll, EPOLL_CTL_DEL, in->fd, NULL);
	if(ret){
		msg("epoll del fd %d failed",in->fd);
		sys_error();
	}
}
void poll_update(Poll* poll,int fd,void* callback,int mask){
	Pollitem* item=map_igetp(poll->items,fd);
	if(!item) return;
	if(mask){
		item->event.events=mask;
		epoll_ctl(poll->poll, EPOLL_CTL_MOD, fd, &item->event);
	}
	item->callback=callback;
//	item->data=data;
}
void poll_del(Poll* poll,int fd){
	if(!fd) return;
	Pollitem* item=map_igetp(poll->items,fd);
	if(!item){
		msg("poll del failed for fd %d",fd);
		return;
	}
	map_del_ex(&poll->items,i_(fd),pollitem_free_ptr);
}
void poll_del_signal(Poll* poll,int signo){
	Pollitem* items=map_p_i(poll->items,0);
	for(int i=0; i<poll->items.keys.len; i++){
		if(items[i].signo!=signo) continue;
		signal(signo, SIG_DFL);
		sigdelset(&poll->signals,signo);
		int fd=poll->items.keys.var[i].i;
		poll_del(poll,fd);
		close(fd);
		return;
	}
	msg("signal del failed");
}
void poll_add_signal(Poll* poll,int signo,void* callback,void* data){
	signal(signo, SIG_IGN);
	sigaddset(&poll->signals,signo);
	sigset_t mask={0};
	sigemptyset(&mask);
	sigaddset(&mask, signo);
	int fd=signalfd(-1, &mask, 0);
	Pollitem* item=poll_add(poll,fd,IN,callback,data,"signal");
	item->signo=signo;
}
int poll_set_repeat(Poll* poll, int fd, int timeout){
	Pollitem* item=map_igetp(poll->items,fd);
	if(!item) return 0;
	item->timeout_repeat=timeout;
	return 0;
}
int poll_add_timeout(Poll* poll, int fd, void* callback){
	Pollitem* item=map_igetp(poll->items,fd);
	if(!item) return 0;
	item->timeout_callback=callback;
	return 0;
}
int poll_set_timeout(Poll* poll, int fd, int timeout){
	Pollitem* item=map_igetp(poll->items,fd);
	if(!item) return 0;
	item->timeout=time_sec()+timeout;
	return 0;
}
Pollitem* poll_add(Poll* poll,int fd,Eventmask mask,void* callback,void* data,char* name){
	if(!fd) return NULL;
	Pollitem item={
		.fd=fd,
		.name=name,
		.sec=time_sec(),
		.callback=callback,
		.data=data,
		.poll=poll->poll,
		.event.events=mask,
		.event.data.u32=fd
	};
	int idx=keys_idx(poll->items.keys,poll->items.index,i_(fd));
	if(idx>=0){
		msg("ERROR re adding fd %d while previous exists, closing old one",fd);
		Pollitem* item=poll->items.vals.ptr;
		((int(*)(void*,int))item[idx].callback)(item[idx].data,END);
	}
	Pollitem* pitem=map_add_ex(&poll->items,i_(fd),NULL);
	*pitem=item;
	epoll_ctl(poll->poll, EPOLL_CTL_ADD, fd, &pitem->event);
	return pitem;
}

static struct timespec starttime={0};

void time_init(){
	clock_gettime(CLOCK_MONOTONIC,&starttime);
}

double time_sec(){
	if(!starttime.tv_sec) time_init();
	struct timespec now={0};
	clock_gettime(CLOCK_MONOTONIC,&now);
	return (now.tv_sec - starttime.tv_sec) + (now.tv_nsec - starttime.tv_nsec) / 1000000000.0;
}
