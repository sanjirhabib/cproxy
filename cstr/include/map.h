#pragma once
#include "var.h"
#include "str.h"
#include "vector.h"
#define map_new(ret, ...) _map_new(ret, ##__VA_ARGS__,NULL)
#define map_each(x,y,z) z=x.vals.ptr; for(int y=0; y<x.keys.len; y++)
#define map_add(x,y) *(var*)map_add_ex(&x,c_(y),_free)
#define map_addv(x,y) *(var*)map_add_ex(&x,y,_free)
#define map_add_type(a,b,c,d) *(a*)map_add_ex(&b,c,d)
#define map_get_type(x,y,z) (*(x*)map_getp(y,z))
typedef struct {
	vector keys;
	vector vals;
	vector index;
} map;
typedef struct {
	int head;
	int tail;
} mapindex;
#define NullMap (map){.index.datasize=sizeof(mapindex),.keys.datasize=sizeof(var),.vals.datasize=sizeof(var)}
map vec_map(vector keys,vector vals);
map map_new_ex(int datasize);
int keys_idx(vector keys, vector index, string key);
int map_c_i(map in,char* key);
var map_c(map in,char* key);
string map_iget(map in,int key);
void* map_p_c(map in,const char* key);
void* map_igetp(map in,int key);
string map_val(map in,int idx);
string map_key(map in,int idx);
void* map_getp(map in,var key);
string map_getc(map in,char* key);
string map_get(map in,var key);
map* map_delc(map* in, char* key);
map* map_del(map* in, var key);
map* map_del_idx(map* in, int slot,void* callback);
map* map_del_ex(map* in, var key,void* callback);
void* map_add_ex(map* in, var key, void* callback);
vector keys_index(vector keys);
map* map_reindex(map* in,int from);
void* map_last(map in);
void* map_first(map in);
void* map_lastkey(map in);
void* map_firstkey(map in);
map map_ro(map in);
void map_free(map* in);
void map_free_ex(map* in,void* callback);
void* map_p_i(map m,int i);
map _map_new(char* in,...);
var map_var(map in);
int rows_count(map in);
map rows_row(map in, int idx);
string rows_s(const map in);
string map_s(map in,char* sepkey,char* sepval);
map s_rows(string in);
string _json(var in);
int hash(var v);
int cl_hash(char *str, int len);
int pow2(int i);
map s_map(string in,char* sepkey,char* sepval);
