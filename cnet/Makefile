CFLAGS=-g -lm -fPIC -fvisibility=hidden -I../cstr -L../cstr -lcstr -lssl -lcrypto -Iinclude -O2 #-fsanitize=address

libcnet.a: lib/ssl.o lib/connection.o lib/poll.o lib/config.o lib/server.o lib/dns.o lib/http.o
	ar rcs $@ $^


clean:
	rm -f lib/*.o
	rm -f include/*.h

lib/%.o: %.c
	cheader $< include
	gcc -c -o $@ $< $(CFLAGS)

