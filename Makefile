CFLAGS=-g -lm -fPIC -fvisibility=hidden -Icstr -Lcstr -lcstr -Icnet -Lcnet -lcnet -lssl -lcrypto -Iinclude -O2 #-fsanitize=address



cproxy: lib/proxy.o proxy_main.c cstr/libcstr.a cnet/libcnet.a lib/proxy_main.o 
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
	sudo cp cproxy.service /etc/systemd/system/
	sudo systemctl enable cproxy
	sudo systemctl restart cproxy
	sudo systemctl status cproxy

uninstall:
	sudo systemctl stop cproxy
	sudo systemctl disable --now cproxy
	sudo rm /etc/systemd/system/cproxy
	sudo systemctl daemon-reload
	echo cproxy service removed

reinstall:
	sudo cp cproxy.service /etc/systemd/system/
	sudo systemctl daemon-reload
	sudo systemctl restart cproxy
	sudo systemctl status cproxy

