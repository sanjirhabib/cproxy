#include "cnet.h"
#include "proxy.h"

#define proxy_version "2024-07-26"

int main(int argc,char** argv){
	msg("cproxy version : %s",proxy_version);
	return cmdline_run(argc,argv,c_("cproxy"),proxy_new,proxy_free);
}
