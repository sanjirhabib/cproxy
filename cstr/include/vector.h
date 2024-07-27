#pragma once
#include "var.h"
#include "str.h"
#define vec_all(ret, ...) _vec_all(ret, ##__VA_ARGS__,VarEnd)
#define vec_new(x,y) _vec_new(sizeof(x),y)
#define vec_append(ret, ...) _vec_append(ret, ##__VA_ARGS__,VarEnd)
#define NullVec (var){.datasize=sizeof(var)}
typedef var vector;
var get(vector in, int index);
var vec_val(vector in, int index);
void* getp(vector in,int index);
vector vvec_free(vector in);
void vec_free(vector* in);
void vec_free_ex(vector* in,void* data_free);
var vfree(var in);
void* vec_last(vector in);
void* vec_first(vector in);
vector vec_del(vector* in,int idx);
vector vec_disown(vector in);
vector vec_own(vector in);
vector vec_del_ex(vector* in,int idx,int len,void* callback);
vector vec_splice(vector in,int from,int len,vector by,void* callback);
vector _vec_new(int datasize,int len);
vector vec_set(vector in,int idx,var val);
vector a_vec(void* in,int datasize,int len);
vector vec_dup(vector in);
vector s_vec(string in,char by);
vector s_vec_s(string in,char* by,int limit);
string s_rest_s(string in,char* sep,string r);
vector s_vec_ex(string in,char by,int limit);
string vec_s(vector in,char* sep);
int vec_strlen(vector in);
vector _vec_append(string ret, ...);
vector _vec_all(string in,...);
vector vec_escape(const vector in);
