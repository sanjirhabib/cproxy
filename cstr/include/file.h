#pragma once
#include <wordexp.h>
#include "log.h"
int is_file(string filename);
int is_dir(string filename);
int file_size(string filename);
string path_cat(string path1, string path2);
int fp_write(var fp,string out);
int s_save(string in,string filename);
var file_fp(string filename,char* rw);
void fp_close(var fp);
string file_read(string filename,int from, int len);
string file_s(string filename);
string filename_os(string filename);
