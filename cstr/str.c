#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include "var.h"
#include "str.h"

string s_dequote(string in,char* qchars){
	if(in.len<2) return in;
	if(strchr(qchars,in.str[0])) return sub(in,1,End-1);
	return in;
}
string s_unescape_ex(string in,char* find, char* replace){
	int extra=0;
	for(int i=0; i<in.len-1; i++){
		if(in.str[i]!='\\') continue;
		extra++;
		i++;
	}
	if(!extra) return in;
	string ret=s_new(NULL,in.len-extra);
	int offset=0;
	for(int i=0; i<in.len; i++){
		char c=in.str[i];
		if(i>=in.len-1 || c!='\\'){
			ret.str[offset++]=c;
			continue;
		}
		i++;
		char* found=strchr(replace,c);
		ret.str[offset++]=*found ? find[found-replace] : c;
	}
	_free(&in);
	return ret;
}
string s_unescape(string in){
	return s_unescape_ex(in,"\0\n\r\t\\\033","0nrt\\e");
}
string s_escape(string in){
	return s_escape_ex(in,"\0\n\r\t\\\033","0nrt\\e");
}
string s_escape_ex(string in,char* find,char* replace){
	int extra=0;
	for(int i=0; i<in.len ; i++)
		if(strchr(find,in.str[i]))
			extra++;
	if(!extra) return in;
	string ret=s_new(NULL,in.len+extra);
	int offset=0;
	for(int i=0; i<in.len ; i++){
		char c=in.str[i];
		char* found=strchr(find,c);
		if(found){
			ret.str[offset++]='\\';
			ret.str[offset++]=replace[found-find];
		}
		else
			ret.str[offset++]=c;
	}
	_free(&in);
	return ret;
}
int s_chr(string in,char has){
	for(int i=0; i<in.len; i++)
		if(in.str[i]==has) return i+1;
	return 0;
}
char* s_has(string in,char* has){
	int sublen=strlen(has);
	for(int i=0; i<=in.len-sublen; i++)
		if(c_eq(in.str+i,has,sublen))
			return in.str+i;
	return NULL;
}
//string replace(string in,char* find,char* replace){
//	string ret={0};
//	pair sub=cut(in,find);
//	while(sub.head.len!=in.len){
//		ret=cat_all(ret,sub.head,c_(replace));
//		in=sub.tail;
//		sub=cut(in,find);
//	}
//	ret=cat(ret,sub.head);
//	return ret;
//}

int is_char(char c,int alpha,int num,char* extra){
	if(alpha && (c>='a' && c<='z' || c>='A' && c<='Z')) return 1;
	if(num && c>='0' && c<='9') return 1;
	if(extra && strchr(extra,c)) return 1;
	return 0;
}
int is_alpha_char(int c){
	return is_char(c,1,0,NULL);
}
int is_word_char(int c){
	return is_char(c,1,1,"_");
}
int is_numeric_char(int c){
	return is_char(c,0,1,"-+.");
}
int is_word(string word,char* list){
	char* str=list;
	char* end=list+strlen(list)-word.len;
	while(str<=end){
		if(memcmp(str,word.str,word.len)==0 && (str==end || *(str+word.len)==' ')) return 1;
		while(str<end && *str!=' ') str++;
		str++;
	}
	return 0;
}
string s_join(string in,char* terminator,string add){
	if(!add.len) return in;
	if(!in.len) return add;
	return cat(cat_c(in,terminator),add);
}
string format(char* fmt,...){
	va_list args;
	va_start(args,fmt);
	string ret=valist_s(0,fmt,args);
	va_end(args);
	return ret;
}
string valist_s(int readonly,char* fmt,va_list args){
	string format=c_(fmt);
	string subs=NullStr;
	pair temp=cut(format,'%');
	string ret=temp.head;
	for(string subs=s_next(temp.tail,'%',NullStr); subs.ptr; subs=s_next(temp.tail,'%',subs)){
		int flen=0;
		string nums=subs;
		nums.len=0;
		while(subs.len && strchr("-.*0123456789",subs.str[0])){
			nums.len++;
			subs.str++;
			subs.len--;
		}
		struct s_align {
			int neg;
			int lenparam;
			int width;
			int zero;
			int deci;
		} align={0};
		if(nums.len && nums.str[0]=='-'){
			align.neg=1;
			nums.str++;
			nums.len--;
		}
		if(nums.len && nums.str[0]=='.'){
			nums.str++;
			nums.len--;
		}
		if(nums.len && nums.str[0]=='*'){
			align.lenparam=1;
			nums.str++;
			nums.len--;
		}
		if(nums.len && nums.str[0]=='0'){
			align.zero=1;
			nums.str++;
			nums.len--;
		}
		if(nums.len){
			pair temp=cut(nums,'.');
			align.width=_i(temp.head);
			align.deci=_i(temp.tail);
		}

		if(!subs.len){
			ret=cat(ret,(string){.str=subs.str-1,.len=subs.len+1});
		}
		else if(subs.str[0]=='s'){
			if(align.lenparam){
				int len=va_arg(args,int);
				char* by=va_arg(args,char*);
				if(len>=0 && !c_len(by)){
					memset(grow(&ret,len),' ',len);
				}
				else if(!len){
				}
				else assert(0);
				flen=1;
			}
			else{
				ret=cat(ret,c_(va_arg(args,char*)));
			}
		}
		else if(subs.str[0]=='p'){
			void* ptr=va_arg(args,void*);
			char buff[sizeof(void*)*2+1];
			long long a=(long long)ptr;
			buff[sizeof(void*)*2]=0;
			for(int i=0; i<sizeof(void*)*2; i++){
				int val=(a & (((long long)0xF)<<(i*4)))>>(i*4);
				char c=val<10 ? '0'+val : 'a'+(val-10);
				buff[sizeof(void*)*2-i-1]=c;
			}
			char* head=buff;
			while(head[1] && *head=='0') head++;
			ret=cat_c(ret,"0x");
			ret=cat_c(ret,head);
		}
		else if(subs.str[0]=='v'){
			var param=va_arg(args,var);
			datasize1(param);
			if(readonly) param.readonly=1;
			if(!param.len){}
			else if(param.datasize==1) ret=cat(ret,param);
			else if(param.len==IsInt){
				ret=cat(ret,i_s(param.i));
			}
			else assert(0);
		}
		else if(subs.str[0]=='d'){
			int i=va_arg(args,int);
			string num=i_s(i);
			int spaces=align.width-num.len;
			if(spaces>0 && !align.neg) memset(grow(&ret,spaces),' ',spaces);
			ret=cat(ret,num);
			if(spaces>0 && align.neg) memset(grow(&ret,spaces),' ',spaces);
		}
		ret=cat(ret,sub(subs,1+flen,End));
	}
	return ret;
}
range s_range(string in){
	return (range){
		.from=0,
		.len=in.len
	};
}
string s_rest(string in,char sep,string r){
	r.len=in.len-(r.str-in.str)-1;
	return r;
}
string from_rest(char* from, string in){
	return (string){
		.len=in.len-(from-in.str),
		.str=from,
	};
}
string c_repeat(char* in, int times){
	int len=strlen(in);
	string ret=s_new(NULL,len*times);
	for(int i=0; i<times; i++){
		memcpy(ret.str+i*times,in,len);
	}
	return ret;
}
string s_lower(string in){
	if(in.readonly) in=_dup(in);
	for(int i=0; i<in.len; i++){
		if(in.str[i]>='A' && in.str[i]<='Z') in.str[i]+=('a'-'A');
	}
	return in;
}
string s_splice(string in, int from, int len,string by){
	//return splice(in,offset,len,replace,NULL);
	if(!in.len) return by;

	range r=len_range(in.len,from,len);
	from=r.from;
	len=r.len;


	if(in.readonly) in=_dup(in);
	int add=by.len ? by.len-len : -len;
	if(add>0){
		in=resize(in,in.len+add);
		memmove(in.str+from+len+add,in.str+from+len,in.len-from-len-add);
	}
	else if(add<0){
		memmove(in.str+from+len+add,in.str+from+len,in.len-from-len);
		in=resize(in,in.len+add);
	}
	if(by.len){
		memcpy(in.str+from,by.str,by.len);
		_free(&by);
	}
	return in;
}
int s_int(string in){
	int ret=atoil(in);
	_free(&in);
	return ret;
}
string s_del(string in, int offset, int len){
	return s_splice(in,offset,len,NullStr);
}
void s_catchar(string* in,char c){
	*(char*)grow(in,1)=c;
}
string c_nullterm(char* in){
	return cl_(in,strlen(in)+1);
}
string s_nullterm(string in){
	if(in.len && !in.str[in.len-1]) return in;
	*(char*)grow(&in,1)='\0';
	return in;
}
int char_count(string in,char c){
	int ret=0;
	for(int i=0; i<in.len; i++) if(in.str[i]==c) ret++;
	return ret;
}
string s_insert(string in,int offset,string by){
	offset=between(0,offset,in.len);
	if(!by.len) return in;
	int len=in.len;
	in=resize(in,in.len+by.len);
	memmove(in.str+offset+by.len,in.str+offset,len-offset);
	memcpy(in.str+offset,by.str,by.len);
	return in;
}
string trim_ex(string in, char* chars){
	char* ptr=in.ptr;
	char* end=in.ptr+in.len;
	while(ptr<end && strchr(chars,*ptr)) ptr++;
	while(end>ptr && strchr(chars,*(end-1))) end--;
	return cl_(ptr,end-ptr);
}
string trim(string in){
	return trim_ex(in," \t\n\r");
}
