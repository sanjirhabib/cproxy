#pragma once
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include "var.h"
string s_dequote(string in,char* qchars);
string s_unescape_ex(string in,char* find, char* replace);
string s_unescape(string in);
string s_escape(string in);
string s_escape_ex(string in,char* find,char* replace);
int s_chr(string in,char has);
char* s_has(string in,char* has);
int is_char(char c,int alpha,int num,char* extra);
int is_alpha_char(int c);
int is_word_char(int c);
int is_numeric_char(int c);
int is_word(string word,char* list);
string s_join(string in,char* terminator,string add);
string format(char* fmt,...);
string valist_s(int readonly,char* fmt,va_list args);
range s_range(string in);
string s_rest(string in,char sep,string r);
string from_rest(char* from, string in);
string c_repeat(char* in, int times);
string s_lower(string in);
string s_splice(string in, int from, int len,string by);
int s_int(string in);
string s_del(string in, int offset, int len);
void s_catchar(string* in,char c);
string c_nullterm(char* in);
string s_nullterm(string in);
int char_count(string in,char c);
string s_insert(string in,int offset,string by);
string trim_ex(string in, char* chars);
string trim(string in);
