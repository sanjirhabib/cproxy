#include "cstr.h"
#include "config.h"

map config_parts(string in){
	if(!in.len) return (map){0};
	vector parts=s_vec_s(in,"\n[",0);
	if(parts.var[0].str[0]=='[') parts.var[0]=tail(parts.var[0],1);
	map ret=NullMap;
	for(int i=0; i<parts.len; i++){
		pair pr=cut(parts.var[i],']');
		map_addv(ret,pr.head)=trim(pr.tail);
	}
	vec_free(&parts);
	return ret;
}
vector part_lines(string in){
	vector lines=s_vec(trim(in),'\n');
	vector ret=NullVec;
	for(int i=0; i<lines.len; i++){
		string line=lines.var[i];
		// cutoff comments and empty lines
		if(!trim(line).len) continue;
		if(strchr(";/#'\"",line.str[0])) continue;
		if(strchr(" \t",line.str[0])){
			if(ret.len) ret.var[ret.len-1]=cat(ret.var[ret.len-1],line);
			continue;
		}
		add(ret)=line;
	}
	vec_free(&lines);
	return ret;
}
map lines_m(vector lines){
	map ret=NullMap;
	for(int i=0; i<lines.len; i++){
		pair s2=cut(get(lines,i),'=');
		string val=trim(s2.tail);
		if(!val.len) continue;
		map_addv(ret,trim(s2.head))=val;
	}
	vec_free(&lines);
	return ret;
}
map config_m(string in,string part){
	map parts=config_parts(in);
	map ret=lines_m(part_lines(map_get(parts,part)));
	map_free(&parts);
	return ret;
}
