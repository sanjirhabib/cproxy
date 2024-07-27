#include <assert.h>
#include <errno.h>

#include "map.h"
#include "log.h"

#define lib_error(x) _lib_error(x,__FILE__,__LINE__)
#define lib_warn(x) _lib_warn(x,__FILE__,__LINE__)
#define app_warn(x) _app_warn(x,__FILE__,__LINE__)
#define error(x) _error(x,__LINE__,__FILE__,__FUNCTION__)
#define xx() _xx(__FILE__,__LINE__,__FUNCTION__)

int msg_timer;
vector msg_queue;

/*header
struct s_log {
	vector lines;
	vector types;
	int is_error;
};
extern struct s_log logs;

end*/

struct s_log logs={0};

vector ring_add(vector in,vector add,int size){
	datasize1(in);
	datasize1(add);
	if(in.len+add.len<size) return cat(in,add);
	if(add.len>=size) return sub(add,add.len-size,size);
	if(in.len<size) in=resize(in,size);
	assert(in.datasize==add.datasize);
	memmove(in.str+add.len*add.datasize,in.str,(in.len-add.len)*in.datasize);
	memcpy(in.str,add.str,add.len*add.datasize);
	_free(&add);
	return in;
}

int is_error(){
	return logs.is_error;
}
int log_error(char* format,...){
	va_list args;
	va_start(args,format);
	string ret=valist_s(1,format,args);
	va_end(args);
	log_add(ret,Error);
	return -1;
}
int log_add(string in,int type){
	fprintf(stderr,"ERROR: %.*s\n",ls(in));
	vector add=vec_own(s_vec(ro(in),'\n'));
	logs.lines=ring_add(logs.lines,add,200);
	vector types=vec_new(int,add.len);
	each(types,i,int* v) v[i]=type;
	logs.types=ring_add(logs.types,types,200);
	_free(&in);
	if(type==Error) logs.is_error=1;
	return 0;
}
int log_os(char* msg,...){
	va_list args;
	va_start(args,msg);
	string ret=valist_s(1,msg,args);
	va_end(args);
	ret=format("%v: %s", ret, strerror(errno));
	log_add(ret,Error);
	return -1;
}
var dump(var in){
	_dump(in,0);
	return in;
}
void _dump(var in,int level){
	datasize1(in);
	if(in.datasize>1){
		if(in.datasize!=sizeof(var)){
			msg("%*s[ len: %d, datasize: %d, readonly: %d ]",level*4,"",in.len,in.datasize,in.readonly);
			return;
		}
		msg("%*s[",level*4,"");
		for(int i=0; i<in.len; i++){
			_dump(get(in,i),level+1);
		}
		msg("%*s]",level*4,"");
	}
	else if(in.len>0){
		string temp=s_escape(ro(in));
		msg("%*s\"%v\"",level*4,"",temp);
		_free(&temp);
	}
	else if(!in.len){
		msg("%*s<blank>",level*4,"");
	}
}
void dx(var in){
	dump(in);
	fflush(stdout);
	raise(SIGINT);
}
int _error(char* msg,int line, char* file,const char* func){
	printf("error: %s in %s():%d in %s\n",msg,func,line,file);
	exit(-1);
	return 0;
}
int app_error(char* msg,...){
	fprintf(stderr,"ERROR: ");
	va_list args;
	va_start(args, msg);
	vfprintf(stderr,msg,args);
	va_end(args);
	fprintf(stderr,"\n");
	return -1;
}
int _app_warn(char* msg,char* file,int line){
	printf("WARNING: %s in %s line %d\n",msg,file,line);
	exit(-1);
}
int _lib_error(char* msg,char* file,int line){
	printf("ERROR: %s in %s line %d\n",msg,file,line);
	raise(SIGINT);
	exit(-1);
}
int _lib_warn(char* msg,char* file,int line){
	printf("WARNING: %s in %s line %d\n",msg,file,line);
	return -1;
}
void vec_print(vector in){
	vec_print_ex(in,p_print);
}
void vec_print_ex(vector in,void* callback){
	datasize1(in);
	printf("[\n");
	for(int i=0; i<in.len; i++){
		printf("\t");
		((void(*)(void* data))callback)(in.str+in.datasize*i);
		if(i<in.len-1) printf(",\n");
	}
	printf("\n]\n");
}
void map_dump(map in){
	if(!in.keys.len){
		msg("{NullMap}");
		return;
	}
	for(int j=0; j<in.vals.len/in.keys.len; j++){
		msg("{");
		for(int i=0; i<in.keys.len; i++){
			fprintf(stderr,"\t%.*s: ",ls(in.keys.var[i]));
			dump(in.vals.var[j*in.keys.len+i]);
		}
		msg("}");
	}
}
void println(var in){
	print(in);
	printf("\n");
}
void pi_print(int* in){
	printf("%d",*in);
}
void p_print(var* in){
	if(in) print(*in);
}
void print(var in){
	datasize1(in);
	if(in.len>0){
		printf("%.*s",ls(in));
	}
	else if(in.datasize>1){
		vec_print(*(vector*)in.ptr);
	}
	else{
		switch(in.len){
			case IsInt : printf("%d",in.i); break;
			case IsDouble : printf("%f",in.f); break;
			case IsPtr : printf("%p",in.ptr); break;
			case IsNull : printf("NULL"); break;
			//case IsMap : map_print(*(map*)in.ptr); break;
		}
	}
}
void map_print(map in){
	map_print_ex(in,p_print);
}
void map_print_ex(map in,void* callback){
	printf("Map:{\n");
	for(int i=0; i<in.keys.len; i++){
		p_print(getp(in.keys,i));
		printf(": ");
		((void(*)(void*))callback)(getp(in.vals,i));
		if(i<in.keys.len-1) printf(",\n");
	}
	printf("\n}\n");
}
void log_print(){
	if(!logs.is_error) return;
	each(logs.lines,i,string* s){
		msg("%v",s[i]);
	}
	logs.is_error=0;
}
void log_init(){
	logs.lines=NullVec;
	logs.types=vec_new(int,0);
}
void log_close(){
	log_print();
	vec_free(&logs.lines);
	_free(&logs.types);
}
void out(char* format,...){
	va_list args;
	va_start(args, format);
	string temp=valist_s(1,format,args);
	printf("%.*s\n",ls(temp));
	vfree(temp);
	va_end(args);
}
void msg(char* format,...){
	va_list args;
	va_start(args, format);
	string temp=valist_s(1,format,args);
	printf("%.*s\n",ls(temp));
	vfree(temp);
	va_end(args);
}
void _xx(const char* file,int line,const char* func){
	printf("--%s():%d in %s\n",func,line,file);
	fflush(stdout);
}
