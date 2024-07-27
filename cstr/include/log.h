#pragma once
#include <assert.h>
#include <errno.h>
#include "map.h"
#define lib_error(x) _lib_error(x,__FILE__,__LINE__)
#define lib_warn(x) _lib_warn(x,__FILE__,__LINE__)
#define app_warn(x) _app_warn(x,__FILE__,__LINE__)
#define error(x) _error(x,__LINE__,__FILE__,__FUNCTION__)
#define xx() _xx(__FILE__,__LINE__,__FUNCTION__)
struct s_log {
	vector lines;
	vector types;
	int is_error;
};
extern struct s_log logs;

vector ring_add(vector in,vector add,int size);
int is_error();
int log_error(char* format,...);
int log_add(string in,int type);
int log_os(char* msg,...);
var dump(var in);
void _dump(var in,int level);
void dx(var in);
int _error(char* msg,int line, char* file,const char* func);
int app_error(char* msg,...);
int _app_warn(char* msg,char* file,int line);
int _lib_error(char* msg,char* file,int line);
int _lib_warn(char* msg,char* file,int line);
void vec_print(vector in);
void vec_print_ex(vector in,void* callback);
void map_dump(map in);
void println(var in);
void pi_print(int* in);
void p_print(var* in);
void print(var in);
void map_print(map in);
void map_print_ex(map in,void* callback);
void log_print();
void log_init();
void log_close();
void out(char* format,...);
void msg(char* format,...);
void _xx(const char* file,int line,const char* func);
