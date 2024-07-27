#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>


#include "cstr.h"


// https://wiki.openssl.org/index.php/Simple_TLS_Server

void* ssl_init(string cert, string privkey, string fullchain){
	fullchain=s_nullterm(fullchain);
	cert=s_nullterm(cert);
	privkey=s_nullterm(privkey);
	if(!cert.len||!privkey.len||!fullchain.len){
		msg("cert file not provided");
		return NULL;
	}
	void* ctx = SSL_CTX_new(TLS_server_method());
	if(!ctx){
		ERR_print_errors_fp(stdout);
		error("ssl ctx failed");
	}
	if (SSL_CTX_use_certificate_chain_file(ctx, fullchain.str) <= 0) {
		ERR_print_errors_fp(stdout);
		error("ssl fullchain failed");
	}
	if (SSL_CTX_use_certificate_file(ctx, cert.str, SSL_FILETYPE_PEM) <= 0) {
		ERR_print_errors_fp(stdout);
		error("ssl cert failed");
	}
	if (SSL_CTX_use_PrivateKey_file(ctx, privkey.str, SSL_FILETYPE_PEM) <= 0 ) {
		ERR_print_errors_fp(stdout);
		error("ssl privkey failed");
	}
	_free(&fullchain);
	_free(&cert);
	_free(&privkey);
	return ctx;
}
void ssl_close(void* ssl){
	if(!ssl) return;
	SSL_shutdown(ssl);
}
void ssl_free(void* ctx){
	if(!ctx) return;
	SSL_CTX_free(ctx);
}
int ssl_handshake(void* ssl){
	int ret=SSL_accept(ssl);	
	if(ret<=0){
		int err=SSL_get_error(ssl,ret);
		if(err==SSL_ERROR_WANT_WRITE||err==SSL_ERROR_WANT_READ){
			//msg("try again now pending...");
			return 0;
		}
		else{
			msg("start ssl failed ret %d err %d",ret,err);
			ERR_print_errors_fp(stdout);
			return -1;
		}
	}
	//msg("ssl success");
	return 1;
}
void* ssl_start(void* ctx,int socket){
	SSL* ssl=SSL_new(ctx);
	if(!ssl) return NULL;
	SSL_set_fd(ssl,socket);
	return ssl;
}
