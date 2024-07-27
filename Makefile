CFLAGS=-g -lm -fPIC -fvisibility=hidden -Icstr -Lcstr -lcstr -Icnet -Lcnet -lcnet -lssl -lcrypto -Iinclude -O2 #-fsanitize=address



cproxy: lib/proxy.o cstr/libcstr.a cnet/libcnet.a lib/proxy_main.o 
	gcc -o $@ $^ $(CFLAGS)

all:
	make -C cstr clean
	make -C cnet clean
	make -C . clean
	make -C cstr
	make -C cnet
	make -C .

cnet/libcnet.a: cnet
	make -C cnet

cstr/libcstr.a: cstr
	make -C cstr


run:
	cproxy server.conf

clean:
	rm -f lib/*.o
	rm -f include/*.h

proxyoff:
	gsettings set org.gnome.system.proxy mode none
proxyon:
	gsettings reset org.gnome.system.proxy.http host
	gsettings reset org.gnome.system.proxy.http port
	gsettings reset org.gnome.system.proxy.https host
	gsettings reset org.gnome.system.proxy.https port
	gsettings set org.gnome.system.proxy.socks host example.com
	gsettings set org.gnome.system.proxy.socks port 2180
	gsettings set org.gnome.system.proxy mode manual


lib/%.o: %.c
	./cheader $< include
	gcc -c -o $@ $< $(CFLAGS)

install:
	cp cproxy /usr/local/bin/
	cp cproxy.service /etc/systemd/system/
	cp cproxy.conf /etc/
	systemctl enable cproxy
	systemctl restart cproxy
	systemctl status cproxy

uninstall:
	systemctl stop cproxy
	systemctl disable --now cproxy
	rm /etc/systemd/system/cproxy
	systemctl daemon-reload
	echo cproxy service removed
	rm -f /usr/local/bin/cproxy
	rm -f /etc/cproxy.conf

reinstall:
	cp cproxy.service /etc/systemd/system/
	cp cproxy /usr/local/bin/
	cp cproxy.conf /etc/
	systemctl daemon-reload
	systemctl restart cproxy
	systemctl status cproxy

