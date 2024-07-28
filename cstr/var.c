#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <memory.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>
#include <assert.h>
#include <stddef.h>
#include <sys/stat.h>

#include "var.h"


#define End (-1)
#define Error (-1)
#define VarEnd (var){.len=End}
#define ls(x) (x).len>0 ? (int)(x).len : 0,(x).str
#define cat_all(ret, ...) _cat_all(ret, ##__VA_ARGS__,VarEnd)
#define min(x,y) (x<y?x:y)
#define max(x,y) (x>y?x:y)
#define between(x,y,z) ((y)<(x) ? (x) : ((y)>(z) ? (z) : (y))) 
#define null (var){0}
#define NullStr (var){.datasize=1}
#define each(x,y,z) z=x.ptr; for(int y=0; y<x.len; y++)
#define add(x) *(var*)grow(&x,1)
#define add_type(x,y) *(x*)grow(&y,1)
#define get_type(a,b,c) *(a*)getp(b,c)
#define datasize1(x) x.len>=0 && !x.datasize && (x.datasize=1)


/*header
typedef struct s_range {
	int from;
	int len;
} range;
typedef enum {
	IsNull=0,
	IsPtr=-2,
	IsInt=-4,
	IsMap=-7,
	IsDouble=-8
} VarType;
typedef struct s_int2 {
	int x;
	int y;
} int2;
typedef struct s_var {
	union {
		int i;
		int2 xy;
		double f;
		void* ptr;
		char* str;
		int* no;
		long long ll;
		struct s_var* var;
	};
	long long len : 47;
	unsigned long long readonly : 1;
	unsigned short datasize;
} var;

typedef var string;

typedef struct s_pair {
	string head;
	string tail;
} pair;
end*/

void pair_free(pair* pair){
	_free(&pair->head);
	_free(&pair->tail);
}
int c_len(char* in){
	return in ? strlen(in) : 0;
}
int s_ends(string in,char* by){
	return eq(sub(in,in.len-c_len(by),End),by);
}
int start_s(string in,string subs){
	return in.len>=subs.len && eq_s(sub(in,0,subs.len),subs);
}
int s_start(string in,char* sub){
	return start_s(in,c_(sub));
}
var p_(var* in){
	return in ? ro(*in) : null;
}
int s_out(string in){
	for(int i=0; i<in.len; i++){
		char c=in.str[i];
		switch(c){
			case '\t': printf("%*s",4,""); break;
			default: putchar(c); break;
		}
	}
	_free(&in);
	return 0;
}
var ptr_(void* in){
	return (var){
		.ptr=in,
		.len=IsPtr,
	};
}
int atoil(var in){
	if(in.len>32||in.len<=0) return 0;
	char temp[33]={0};
	memcpy(temp,in.str,in.len);
	_free(&in);
	return atoi(temp);
}
int _i(var in){
	return p_i(&in);
}
int p_i(var* in){
	if(!in||!in->len) return 0;	
	if(in->len>0) return atoil(*in);
	if(in->len==IsInt) return in->i;
	if(in->len==IsDouble) return (int)(in->f);
	return 0;
}
string i_s(long long in){
	char ret[21]={0};
	int at=sizeof(ret)-2;
	long long temp=in<0 ? in*-1 : in;
	do{ ret[at--]='0'+temp%10; } while ((temp/=10));
	if(in<0) ret[at--]='-';
	return c_copy(ret+at+1);
}
void _free(var* in){
	if(in->readonly||in->len<=0) return;
	mem_resize(in->ptr,0,0);
	in->len=0;
	in->ptr=NULL;
}
var i_(int val){
	return (var){ .i=val, .len=IsInt };
}
string ptr_s(void* in){
	return in ? *(string*)in : NullStr;
}
var ro(var in){
	in.readonly=1;
	return in;
}
var own(var* in){
	if(!in) return null;
	var ret=*in;
	in->readonly=1;
	return ret;
}
var move(var* in){
	var ret=*in;
	*in=null;
	return ret;
}
string vown(string in){
	if(in.len && in.readonly) return _dup(in);
	return in;
}
int c_eq(char* in1,char* in2,int len){
	for(int i=0; i<len; i++)
		if(tolower(in1[i])!=tolower(in2[i])) return 0;
	return 1;
}
int eq_s(var in1, var in2){
	if(in1.len<=0) return in1.ptr==in2.ptr;
	if(in1.len!=in2.len) return 0;
	return c_eq(in1.str,in2.str,in1.len);
}
string c_dup(char* in){
	return s_new(in,in ? strlen(in) : 0);
}
string s_new(void* str,int len){
	string ret=NullStr;
	ret=resize(ret,len);
	if(str && len) memcpy(ret.str,str,len);
	return ret;
}
void* mem_resize(void* in,int len,int oldlen){
	if(!len){
		if(in) free(in);
		return NULL;
	}
	if(!in) return memset(malloc(len),0,len);
	if(len<=oldlen) return in;
	void* ret=realloc(in,len);
	memset((char*)ret+oldlen,0,len-oldlen);
	return ret;
}
void* grow(string* in,int len){
	*in=resize(*in,in->len+len);
	return in->str+(in->len-len)*in->datasize;
}
string resize(string in,int len){
	datasize1(in);
	if(!len){
		_free(&in);
		return (string){.datasize=in.datasize};
	}
	if(in.len==len) return in;
	if(len<in.len){
		in.len=len;
		return in;
	}
	void* ret=in.readonly
		? memcpy(mem_resize(NULL,len*in.datasize,0),in.str,(int)(min(len,in.len)*in.datasize))
		: mem_resize(in.str,len*in.datasize,in.len*in.datasize);
	return (string){
		.ptr=ret,
		.len=len,
		.datasize=in.datasize,
	};
}
string chop(string in,int len){
	if(len>=in.len){
		_free(&in);
		return (string){0};
	}
	datasize1(in);
	memmove(in.str,in.str+len*in.datasize,(in.len-len)*in.datasize);
	in.len-=len;
	return in;
}
string cat(string str1,string str2){
	if(!str2.len) return str1;
	if(!str1.len) return str2;

	datasize1(str1);
	datasize1(str2);
	assert(str2.datasize==str2.datasize);

	if(str1.str+str1.len*str1.datasize==str2.str){ //when string was previously split, ptrs are side by side
		str1.len+=str2.len;
		return str1;
	}
	void* ptr=grow(&str1,str2.len);
	memcpy(ptr,str2.str,str2.len*str1.datasize);
	_free(&str2);
	return str1;
}
void* to_heap(void* in,size_t len){
	void* ret=mem_resize(NULL,len,0);
	memcpy(ret,in,len);
	return ret;
}
void pair_sub(pair* in,int start,int len){
	if(!len) len=in->tail.len-start;
	in->tail.str+=start;
	in->tail.len-=len;
}
string s_c(string in){
	string ret=s_new(NULL,in.len+1);
	memcpy(ret.str,in.str,in.len);
	_free(&in);
	return ret;
}
string c_copy(char* in){
	return s_new(in,strlen(in));
}
string s_copy(string in){
	return s_new(in.str,in.len);
}
string head(string in, int len){
	return sub(in,0,len);
}
string tail(string in, int len){
	return sub(in,len,in.len-len);
}
string sub(string in, int from, int len){
	if(!in.len) return in;
	datasize1(in);
	range r=len_range(in.len,from,len);
	return r.len ? (string){
		.str=in.str+r.from*in.datasize,
		.len=r.len,
		.datasize=in.datasize,
		.readonly=1,
	} : NullStr;
}
int s_caseeq(string str1,string str2){
	if(str1.len!=str2.len) return 0;	
	for(int i=0; i<str1.len; i++)
		if(str1.str[i]!=str2.str[i]) return 0;
	return 1;
}
int eq(string str1,char* str2){
	if(!str2) return 0;
	int len=strlen(str2);
	if(len!=str1.len) return 0;
	return c_eq(str2,str1.str,len);
}
pair cut_any(string in,char* any){
	pair ret={.head=in};
	int i=0;
	while(i<in.len && !strchr(any,in.str[i])) i++;
	ret.head.len=i;
	while(i<in.len && strchr(any,in.str[i])) i++;
	ret.tail=sub(in,i,in.len);
	return ret;
}
string s_next_s(string in,char* sep,string r){
	if(!in.len) return NullStr;
	int len=strlen(sep);
	if(!r.str){
		r.str=in.str;
		r.readonly=1;
	}
	else
		r.str+=r.len+len;
	int rest=in.str+in.len-r.str;

	if(!rest){
		r.len=0;
		return r;
	}
	if(rest<0){
		return NullStr;
	}

	char* end=in.ptr+in.len-1;
	char* ptr=r.str;
	while(ptr<=end-len && memcmp(ptr,sep,len)!=0) ptr++;

	if(ptr>end-len){ //whole string matched.
		r.len=in.len-(r.str-in.str);
		return r;
	}

	r.len=ptr-r.str;
	return r;
}
pair cut_s(string in,char* by){
	string s=s_next_s(in,by,NullStr);
	return (pair){
		.head=s,
		.tail=sub(in,s.len+1,in.len),
	};
}
pair cut(string in,char by){
	string s=s_next(in,by,NullStr);
	return (pair){
		.head=s,
		.tail=sub(in,s.len+1,in.len),
	};
}
string cutp_s(string* in,char* by){
	pair ret=cut_s(*in,by);
	*in=ret.tail;
	return ret.head;
}
string cutp(string* in,char by){
	pair ret=cut(*in,by);
	*in=ret.tail;
	return ret.head;
}

void buff_add(pair* in,var add){
	if(!in->tail.len) return;
	int len=_c(add,in->tail.str);
	if(len>in->tail.len) die("mem corrupted");
	in->tail.str+=len;
	in->tail.len-=len;
	_free(&add);
}
pair buff_new(int len){
	string s=s_new(NULL,len);
	return (pair){.head=s,.tail=s};
}
string _s(var in){
	if(in.len>=0) return in;
	string ret=s_new(NULL,_c(in,NULL));
	_c(in,ret.str);
	return ret;
}
string cl_(const void* in,int len){
	return in && len ? (string){
		.str=(void*)in,
		.len=len,
		.datasize=1,
		.readonly=1,
	} : NullStr;
}
string c_(const char* in){
	return cl_(in,in ? strlen(in) : 0);
}
int _c(var in,char* out){
	if(in.len>0){
		if(out) memcpy(out,in.str,in.len);
		return in.len;
	}
	switch(in.len){
		case IsPtr : if(out) memcpy(out,"<ptr>",6); return 5;
		case End : if(out) memcpy(out,"<end>",6); return 5;
		case IsInt :;
			int len=snprintf(0,0,"%d",in.i);
			if(!out) return len;
			char buff[32];
			snprintf(buff,sizeof(buff),"%d",in.i);
			memcpy(out,buff,len);
			return len;
		case IsDouble :
			len=snprintf(0,0,"%g",in.f);
			if(!out) return len;
			snprintf(buff,sizeof(buff),"%g",in.f);
			memcpy(out,buff,len);
			return len;
		default : return 0;
	}
}
string cat_c(string in,char* add){
	return cat(in,c_(add));
}
int _len(var in){
	return _c(in,NULL);
}
string _cat_all(string in,...){
	va_list args;
	va_list args2;
	va_start(args,in);
	va_copy(args2,args);
	int len=_len(in);
	while(1){
		var sub=va_arg(args,var);
		if(sub.len==End) break;
		len+=_len(sub);
	}
	pair ret=buff_new(len);
	buff_add(&ret,in);
	while(1){
		var sub=va_arg(args2,var);
		if(sub.len==End) break;
		buff_add(&ret,sub);
	}
	va_end(args2);
	va_end(args);
	return ret.head;
}
var _dup(var in){
	if(!in.len) return in;
	datasize1(in);
	var ret=in;
	ret.readonly=0;
	ret.datasize=ret.datasize;
	ret.ptr=memcpy(mem_resize(NULL,ret.len*ret.datasize,0),in.str,ret.len*ret.datasize);
	return ret;
}
void die(char* msg){
	fprintf(stderr,"%s",msg);
	exit(-1);
}
range len_range(int full,int from,int len){
	if(from<0) from=0;
	if(from>full) from=full;
	if(len<0) len=(full-from)-(End-len);
	if(len<0) len=0;
	if(from+len>full) len=full-from;
	return (range){
		.from=from,
		.len=len
	};
}
string s_next(string in,char sep,string r){
	if(!in.len) return NullStr;
	if(!r.str){
		r.str=in.str;
		r.readonly=1;
	}
	else
		r.str+=r.len+1;
	int rest=in.str+in.len-r.str;
	if(!rest){
		r.len=0;
		return r;
	}
	if(rest<0){
		return NullStr;
	}

	char* end=in.ptr+in.len-1;
	char* ptr=r.str;
	while(ptr<=end && *ptr!=sep) ptr++;

	if(ptr>end){ //whole string matched.
		r.len=in.len-(r.str-in.str);
		return r;
	}

	r.len=ptr-r.str;
	return r;
}
