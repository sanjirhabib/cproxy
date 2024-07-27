#pragma once
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "cstr.h"
void* ssl_init(string cert, string privkey, string fullchain);
void ssl_close(void* ssl);
void ssl_free(void* ctx);
int ssl_handshake(void* ssl);
void* ssl_start(void* ctx,int socket);
