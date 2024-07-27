#include "var.h"
#include "str.h"
#include "vector.h"

#define vec_all(ret, ...) _vec_all(ret, ##__VA_ARGS__,VarEnd)
#define vec_new(x,y) _vec_new(sizeof(x),y)
#define vec_append(ret, ...) _vec_append(ret, ##__VA_ARGS__,VarEnd)
#define NullVec (var){.datasize=sizeof(var)}

/*header
typedef var vector;
end*/

var get(vector in, int index){
	if(!in.len) return null;
	assert(in.datasize==sizeof(var));
	return index<0||index>=in.len ? null : ro(in.var[index]);
}
var vec_val(vector in, int index){
	return p_(getp(in,index));
}
void* getp(vector in,int index){
	if(index<0||index>=in.len) return NULL;
	assert(in.datasize);
	return in.str+in.datasize*index;
}
vector vvec_free(vector in){
	vec_free(&in);
	return NullVec;
}
void vec_free(vector* in){
	vec_free_ex(in,_free);
}
void vec_free_ex(vector* in,void* data_free){
	if(in->readonly) return;
	if(data_free){
		void(*callback)(void*)=data_free;
		for(int i=0; i<in->len; i++){
			callback(in->str+in->datasize*i);
		}
	}
	_free(in);
}
var vfree(var in){
	_free(&in);
	return (var){ .datasize=in.datasize };
}
void* vec_last(vector in){
	return getp(in,in.len-1);
}
void* vec_first(vector in){
	return getp(in,0);
}
vector vec_del(vector* in,int idx){
	return vec_del_ex(in,idx,1,_free);
}
vector vec_disown(vector in){
	if(in.readonly) in=_dup(in);
	for(int i=0; i<in.len; i++){
		in.var[i].readonly=1;
	}
	return in;
}
vector vec_own(vector in){
	for(int i=0; i<in.len; i++){
		if(in.var[i].readonly) in.var[i]=_dup(in.var[i]);
	}
	return in;
}
vector vec_del_ex(vector* in,int idx,int len,void* callback){
	range r=len_range(in->len,idx,len);
	if(!r.len) return *in;
	if(!r.from && r.len==in->len){
		vec_free_ex(in,callback);
		return *in;
	}
	assert(in->datasize);
	int datasize=in->datasize;	
	if(callback){
		for(int i=r.from; i<r.from+r.len; i++){
			((void(*)(void*))callback)((void*)(in->str+datasize*i));
		}
	}
	if(r.from+r.len<in->len){
		memmove(
			in->str+datasize*r.from,
			in->str+datasize*(r.from+r.len),
			datasize*(in->len-(r.from+r.len))
		);
	}
	*in=resize(*in,in->len-r.len);
	return *in;
}
vector vec_splice(vector in,int from,int len,vector by,void* callback){
	if(!in.len) return by;

	range r=len_range(in.len,from,len);
	from=r.from;
	len=r.len;


	if(in.readonly) in=_dup(in);
	assert(in.datasize);
	if(callback){
		for(int i=from; i<from+len; i++){
			((void(*)(void*))callback)(getp(in,i));
		}
	}
	int add=by.len ? by.len-len : -len;
	if(add>0){
		in=resize(in,in.len+add);
		memmove(getp(in,from+len+add),getp(in,from+len),in.datasize*(in.len-from-len-add));
	}
	else if(add<0){
		memmove(getp(in,from+len+add),getp(in,from+len),in.datasize*(in.len-from-len));
		in=resize(in,in.len+add);
	}
	if(by.len){
		assert(in.datasize==by.datasize);
		memcpy(getp(in,from),by.str,by.len*in.datasize);
		_free(&by);
	}
	return in;
}
vector _vec_new(int datasize,int len){
	vector ret=(vector){.datasize=datasize};
	if(len) ret=resize(ret,len);
	return ret;
}
vector vec_set(vector in,int idx,var val){
	if(idx<0||idx>=in.len) return in;
	_free(&in.var[idx]);
	in.var[idx]=val;
	return in;
}
vector a_vec(void* in,int datasize,int len){
	return (vector){
		.str=in,
		.datasize=datasize,
		.len=len,
		.readonly=1
	};
}
vector vec_dup(vector in){
	vector ret=_dup(in);
	for(int i=0; i<ret.len; i++){
		ret.var[i]=_dup(ret.var[i]);
	}
	return ret;
}
vector s_vec(string in,char by){
	return s_vec_ex(in,by,0);
}
vector s_vec_s(string in,char* by,int limit){
	vector ret=NullVec;
	string r=NullStr;
	while((r=s_next_s(in,by,r)).ptr){
		add(ret)=r;
		if(limit && ret.len==limit-1){
			add(ret)=s_rest_s(in,by,r);
			break;
		}
	}
	return ret;
}
string s_rest_s(string in,char* sep,string r){
	r.len=in.len-(r.str-in.str)-strlen(sep);
	return r;
}
vector s_vec_ex(string in,char by,int limit){
	vector ret=NullVec;
	string r=NullStr;
	while((r=s_next(in,by,r)).ptr){
		add(ret)=r;
		if(limit && ret.len==limit-1){
			add(ret)=s_rest(in,by,r);
			break;
		}
	}
	return ret;
}
string vec_s(vector in,char* sep){
	if(!in.len) return NullVec;
	string sep1=c_(sep);
	int len=((in.len-1)*sep1.len)+vec_strlen(in);
	pair ret=buff_new(len);
	for(int i=0; i<in.len ; i++){
		if(i) buff_add(&ret,sep1);
		buff_add(&ret,ro(in.var[i]));
	}
	vec_free(&in);
	return ret.head;
}
int vec_strlen(vector in){
	int ret=0;
	for(int i=0; i<in.len; i++) ret+=_c(in.var[i],NULL);
	return ret;
}
vector _vec_append(string ret, ...){
	va_list args;
	va_start(args,ret);
	while(1){
		var val=va_arg(args,var);
		if(val.len==End) break;
		add(ret)=val;
	}
	va_end(args);
	return ret;
}
vector _vec_all(string in,...){
	vector ret=NullVec;
	va_list args;
	va_start(args,in);
	var val=in;
	while(1){
		if(val.len==End) break;
		add(ret)=val;
		val=va_arg(args,var);
	}
	va_end(args);
	return ret;
}
vector vec_escape(const vector in){
	vector ret=NullVec;
	ret=resize(ret,in.len);
	for(int i=0; i<in.len; i++){
		ret.var[i]=s_escape(ro(in.var[i]));
	}
	return ret;
}
