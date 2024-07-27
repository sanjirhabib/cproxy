#include <wordexp.h>

#include "log.h"
#include "file.h"


int is_file(string filename){
	struct stat st={0};
	string temp=filename_os(filename);
	stat(temp.str, &st);
	_free(&temp);
	return S_ISREG(st.st_mode) || S_ISLNK(st.st_mode);
}
int is_dir(string filename){
	struct stat st={0};
	string temp=filename_os(filename);
	stat(temp.str, &st);
	_free(&temp);
	return S_ISDIR(st.st_mode);
}
int file_size(string filename){
	struct stat st={0};
	string temp=filename_os(filename);
	stat(temp.str, &st);
	_free(&temp);
	return st.st_size;
}
string path_cat(string path1, string path2){
	if(!path1.len) return path2;
	if(!path2.len) return path1;
	if(path2.str[0]=='.') path2=cut(path2,'.').tail;
	if(path2.str[0]=='/') path2=cut(path2,'/').tail;
	return cat(path1,path2);
}
int fp_write(var fp,string out){
	string parts=out;
	while(parts.len>0){
		int ret=fwrite(parts.str,1,parts.len,fp.ptr);
		if(!ret){
			log_os("Writing to file pointer failed");
			return 0;
		}
		parts=sub(parts,ret,parts.len);
	}
	vfree(out);
	return 1;
}
int s_save(string in,string filename){
	var fp=file_fp(filename,"w");
	if(!fp.ptr) return 0;
	int ret=fp_write(fp,in);
	if(!ret){
		log_os("failed to save into file %v",filename);
		fp_close(fp);
		return 0;
	}
	fp_close(fp);
	return 1;
}
var file_fp(string filename,char* rw){
	if(!filename.len || eq(filename,"-")) return ptr_(stdout);
	string name=filename_os(filename);
	if(!name.len) return null;
	FILE* fp=fopen(name.str,rw);
	if(!fp){
		log_os("failed to open file %v",sub(name,0,End-1));
		vfree(name);
		return null;
	}
	vfree(name);
	return ptr_(fp);
}
void fp_close(var fp){
	if(!fp.ptr || fp.ptr==stdout||fp.ptr==stderr) return;
	fclose(fp.ptr);
}
string file_read(string filename,int from, int len){
	var fp=file_fp(filename,"r");
	if(!fp.ptr) return null;
	fseek(fp.ptr,0,SEEK_END);
	size_t size=ftell(fp.ptr);
	fseek(fp.ptr,0,SEEK_SET);
	range r=len_range(size,from,len);
	string ret=s_new(NULL,r.len);
	size_t read=fread(ret.str,1,r.len,fp.ptr);
	fp_close(fp);
	if(read!=r.len) _free(&ret);
	return ret;
}
string file_s(string filename){
	return file_read(filename,0,End);
}
string filename_os(string filename){
	if(!filename.len) return filename;
	filename=s_nullterm(filename);
    wordexp_t exp_result;
    int err=wordexp(filename.str, &exp_result, 0);
	if(err){
		log_os("filename expansion for %v failed",sub(filename,0,End-1));
		return NullStr;
	}
	string ret=_dup(cl_(exp_result.we_wordv[0],strlen(exp_result.we_wordv[0])+1));
	wordfree(&exp_result);
	_free(&filename);
	return ret;
}
