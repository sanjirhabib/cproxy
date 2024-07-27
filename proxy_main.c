#include "cnet.h"
#include "proxy.h"

#define proxy_version "2024-07-27"

int main(int argc,char** argv){
	msg("cproxy, Sanjir Habib (c) 2024. MIT License. Version : %s",proxy_version);
	return cmdline_run(argc,argv,c_("cproxy"),proxy_new,proxy_free);
}
