#pragma once
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
void pair_free(pair* pair);
int c_len(char* in);
int s_ends(string in,char* by);
int start_s(string in,string subs);
int s_start(string in,char* sub);
var p_(var* in);
int s_out(string in);
var ptr_(void* in);
int atoil(var in);
int _i(var in);
int p_i(var* in);
string i_s(long long in);
void _free(var* in);
var i_(int val);
string ptr_s(void* in);
var ro(var in);
var own(var* in);
var move(var* in);
string vown(string in);
int c_eq(char* in1,char* in2,int len);
int eq_s(var in1, var in2);
string c_dup(char* in);
string s_new(void* str,int len);
void* mem_resize(void* in,int len,int oldlen);
void* grow(string* in,int len);
string resize(string in,int len);
string chop(string in,int len);
string cat(string str1,string str2);
void* to_heap(void* in,size_t len);
void pair_sub(pair* in,int start,int len);
string s_c(string in);
string c_copy(char* in);
string s_copy(string in);
string head(string in, int len);
string tail(string in, int len);
string sub(string in, int from, int len);
int s_caseeq(string str1,string str2);
int eq(string str1,char* str2);
pair cut_any(string in,char* any);
string s_next_s(string in,char* sep,string r);
pair cut_s(string in,char* by);
pair cut(string in,char by);
string cutp_s(string* in,char* by);
string cutp(string* in,char by);
void buff_add(pair* in,var add);
pair buff_new(int len);
string _s(var in);
string cl_(const void* in,int len);
string c_(const char* in);
int _c(var in,char* out);
string cat_c(string in,char* add);
int _len(var in);
string _cat_all(string in,...);
var _dup(var in);
void die(char* msg);
range len_range(int full,int from,int len);
string s_next(string in,char sep,string r);
