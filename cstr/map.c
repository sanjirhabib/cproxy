#include "var.h"
#include "str.h"
#include "vector.h"
#include "map.h"

#define map_new(ret, ...) _map_new(ret, ##__VA_ARGS__,NULL)
#define map_each(x,y,z) z=x.vals.ptr; for(int y=0; y<x.keys.len; y++)
#define map_add(x,y) *(var*)map_add_ex(&x,c_(y),_free)
#define map_addv(x,y) *(var*)map_add_ex(&x,y,_free)
#define map_add_type(a,b,c,d) *(a*)map_add_ex(&b,c,d)
#define map_get_type(x,y,z) (*(x*)map_getp(y,z))
/*header
typedef struct {
	vector keys;
	vector vals;
	vector index;
} map;
typedef struct {
	int head;
	int tail;
} mapindex;
end*/

#define NullMap (map){.index.datasize=sizeof(mapindex),.keys.datasize=sizeof(var),.vals.datasize=sizeof(var)}

map vec_map(vector keys,vector vals){
	return (map){
		.keys=keys,
		.vals=vals,
		.index=keys_index(keys),
	};
}
map map_new_ex(int datasize){
	map ret={0};
	ret.keys=NullVec;
	ret.vals=_vec_new(datasize,0);
	ret.index=vec_new(mapindex,0);
	return ret;
}
int keys_idx(vector keys, vector index, string key){
	if(!keys.len) return End;
	int slot=hash(key) & index.len-1;
	slot=((mapindex*)index.str)[slot].head-1;
	while(1){
		if(slot<0) return End;
		if(eq_s(key,((var*)keys.str)[slot])) return slot;
		slot=((mapindex*)index.str)[slot].tail-1;
	}
	//never.
}
int map_c_i(map in,char* key){
	return p_i(map_p_c(in,key));
}
var map_c(map in,char* key){
	return p_(map_p_c(in,key));
}
string map_iget(map in,int key){
	return p_(map_igetp(in,key));
}
void* map_p_c(map in,const char* key){
	return getp(in.vals,keys_idx(in.keys,in.index,c_(key)));
}
void* map_igetp(map in,int key){
	return getp(in.vals,keys_idx(in.keys,in.index,i_(key)));
}
string map_val(map in,int idx){
	return vec_val(in.vals,idx);
}
string map_key(map in,int idx){
	return vec_val(in.keys,idx);
}
void* map_getp(map in,var key){
	return getp(in.vals,keys_idx(in.keys,in.index,key));
}
string map_getc(map in,char* key){
	return p_(map_getp(in,c_(key)));
}
string map_get(map in,var key){
	return p_(map_getp(in,key));
}
map* map_delc(map* in, char* key){
	return map_del_ex(in,c_(key),_free);
}
map* map_del(map* in, var key){
	return map_del_ex(in,key,_free);
}
map* map_del_idx(map* in, int slot,void* callback){
	if(slot<0||slot>=in->keys.len) return in;
	vec_del_ex(&in->vals,slot,1,callback);
	vec_del_ex(&in->keys,slot,1,_free);
	int newlen=pow2(in->keys.len);
	if(newlen!=in->index.len){
		in->index=resize(in->index,newlen);
		return map_reindex(in,0);
	}
	mapindex* idx=(void*)in->index.str;
	int slotnext=idx[slot].tail;
	for(int i=0; i<in->index.len; i++){
		if(idx[i].head==slot+1) idx[i].head=slotnext;
		if(i>=slot && i<in->keys.len) idx[i].tail=idx[i+1].tail;
		if(idx[i].tail==slot+1) idx[i].tail=slotnext;
		if(idx[i].head>slot+1) idx[i].head--;
		if(idx[i].tail>slot+1) idx[i].tail--;
	}
	idx[in->keys.len].tail=0;
	return in;
}
map* map_del_ex(map* in, var key,void* callback){
	return map_del_idx(in,keys_idx(in->keys,in->index,key),callback);
}
void* map_add_ex(map* in, var key, void* callback){
	int slot=keys_idx(in->keys,in->index,key);
	if(slot!=End){ //key exists
		if(callback) ((void(*)(void*))callback)(getp(in->vals,slot));
		_free(&key);
		return in->vals.str+in->vals.datasize*slot;
	}
	if(in->index.len<in->keys.len+1){
		in->index=resize(in->index,pow2(in->keys.len+1));
		map_reindex(in,0);
	}
	add(in->keys)=key;
	map_reindex(in,in->keys.len-1);
	return grow(&in->vals,1);
}
vector keys_index(vector keys){
	vector ret=vec_new(mapindex,pow2(keys.len));
	mapindex* idx=(mapindex*)ret.str;
	for(int i=0; i<keys.len; i++){
		int slot=hash(keys.var[i]) & ret.len-1;
		if(idx[slot].head) idx[i].tail=idx[slot].head;
		idx[slot].head=i+1;
	}
	return ret;
}
map* map_reindex(map* in,int from){
	mapindex* idx=(mapindex*)in->index.str;
	if(!from) memset(idx,0,in->index.datasize*in->index.len);
	for(int i=from; i<in->keys.len; i++){
		var key=((var*)in->keys.str)[i];
		int slot=hash(key) & in->index.len-1;
		if(idx[slot].head) idx[i].tail=idx[slot].head;
		idx[slot].head=i+1;
	}
	return in;
}
void* map_last(map in){
	return vec_last(in.vals);
}
void* map_first(map in){
	return vec_first(in.keys);
}
void* map_lastkey(map in){
	return vec_last(in.keys);
}
void* map_firstkey(map in){
	return vec_first(in.keys);
}
map map_ro(map in){
	in.vals.readonly=1;
	in.keys.readonly=1;
	in.index.readonly=1;
	return in;
}
void map_free(map* in){
	map_free_ex(in,_free);
}
void map_free_ex(map* in,void* callback){
	vec_free_ex(&in->vals,callback);
	vec_free(&in->keys);
	_free(&in->index);
}
void* map_p_i(map m,int i){
	return m.vals.str+m.vals.datasize*i;
}
map _map_new(char* in,...){
	map ret=NullMap;
	va_list args;
	va_start(args,in);
	char* name=in;
	while(1){
		if(!name) break;
		var val=va_arg(args,var);
		map_add(ret,name)=val;
		name=va_arg(args,char*);
	}
	va_end(args);
	return ret;
}
var map_var(map in){
	return (var){.ptr=to_heap(&in,sizeof(in)),.len=IsMap};
}
int rows_count(map in){
	return in.vals.len/in.keys.len;
}
map rows_row(map in, int idx){
	if(idx<0||idx>=rows_count(in)) return (map){0};
	in.vals.str=in.vals.str+in.vals.datasize*in.keys.len*idx;
	in.vals.len=in.keys.len;
	in.vals.readonly=1;
	in.keys.readonly=1;
	in.index.readonly=1;
	return in;
}
string rows_s(const map in){
	vector lines=NullVec;
	lines=resize(lines,in.vals.len/in.keys.len+1);
	lines.var[0]=vec_s(vec_escape(in.keys),"\t");
	for(int i=0; i<in.vals.len/in.keys.len; i++){
		lines.var[i+1]=vec_s(vec_escape(rows_row(in,i).vals),"\t");
	}
	return vec_s(lines,"\n");
}
string map_s(map in,char* sepkey,char* sepval){
	if(!in.keys.len) return NullStr;
	string sep1=c_(sepkey);
	string sep2=c_(sepval);
	int len=in.keys.len*(sep1.len+sep2.len)+vec_strlen(in.keys)+vec_strlen(in.vals);
	pair ret=buff_new(len);
	for(int i=0; i<in.keys.len; i++){
		buff_add(&ret,ro(in.keys.var[i]));
		buff_add(&ret,sep1);
		buff_add(&ret,ro(in.vals.var[i]));
		buff_add(&ret,sep2);
	}
	map_free(&in);
	return ret.head;
}
map s_rows(string in){
	vector lines=s_vec(trim(in),'\n');
	if(!lines.len) return NullMap;
	vector keys=s_vec(lines.var[0],'\t');
	if(!keys.len) return NullMap;
	vector index=keys_index(keys);
	vector vals=NullVec;
	for(int i=1; i<lines.len; i++){
		vector temp=s_vec(lines.var[i],'\t');
		temp=resize(temp,keys.len);
		vals=cat(vals,temp);
	}
	_free(&lines);
	return (map){
		.keys=keys,
		.vals=vals,
		.index=index,
	};
}
string _json(var in){
	datasize1(in);
	string ret={0};
	if(!in.ptr && !in.len) return c_("NULL");
	if(in.len>0 && !in.datasize) //string
		return s_escape(in); // todo: only for '"'

	if(in.len==IsMap){
		map m=*(map*)in.ptr;
		ret=cat(ret,c_("{"));
		for(int i=0; i<m.keys.len; i++){
			if(i) ret=cat(ret,c_(", "));
			ret=cat(ret, _json(map_key(m,i)));
			ret=cat(ret,c_(": "));
			ret=cat(ret, _json(vec_val(m.vals,i)));
		}
		ret=cat(ret,c_("}"));
		return ret;
	}
	if(in.len>0 && in.datasize){
		ret=cat(ret,c_("["));
		for(int i=0; i<in.len; i++){
			if(i) ret=cat(ret,c_(", "));
			ret=cat(ret, _json(vec_val(in,i)));
		}
		ret=cat(ret,c_("]"));
		return ret;
	}
	return _s(in);
}
int hash(var v){
	if(v.len>0) return cl_hash(v.ptr,v.len);
	int ret=0x1d4592f8+(int)v.ll+(v.ll>>32);
	return ret ? ret : 0xc533c700;
}
int cl_hash(char *str, int len){
	int ret = 0x1d4592f8;
	int i=0;
	while(i<len) ret=((ret<<5)+ret)+tolower(str[i++]);
	return ret ? ret : 0xc533c700; //never return 0
};
int pow2(int i){
	if(!i) return 0;
	i--;
	int ret=2;
	while(i>>=1) ret<<=1;
	return ret;
};
map s_map(string in,char* sepkey,char* sepval){
	vector temp=s_vec_s(trim(in),sepval,0);	
	map ret=NullMap;
	for(int i=0; i<temp.len; i++){
		pair pair=cut_s(temp.var[i],sepkey);
		map_addv(ret,s_lower(trim(pair.head)))=trim(pair.tail);
	}
	vec_free_ex(&temp,NULL);
	return ret;
}
