CFLAGS=-O2 -g -lm -fPIC -fvisibility=hidden -lssl -lcrypto -Iinclude #-fsanitize=address

libcstr.a: lib/var.o lib/str.o lib/vector.o lib/map.o lib/log.o lib/file.o
	ar rcs $@ $^

clean:
	rm -f lib/*.o
	rm -f include/*.h

lib/%.o: %.c
	cheader $< include
	gcc -c -o $@ $< $(CFLAGS)

testmap: testmap.c lib/var.o
	gcc -o $@ testmap.c lib/var.o $(CFLAGS)


benchmark: lib/benchmark.o lib/var.o
	gcc -o $@ $^ $(CFLAGS) -O2 -lm

