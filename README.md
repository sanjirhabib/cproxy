# cproxy
Socks 4 and 5 proxy server in C using epoll().


# Featrures
- Uses a custom DNS resolver so that DNS calls can be epolled too.
- No 3rd party dependance or library used.
- valgrind tested : no memory leak or out of bound memory touch.
- No GC. Manual memory management.

# Installation
```
git clone --recursive --depth=1 https://github.com/sanjirhabib/cproxy/
cd cproxy
make
```

Next edit cproxy.conf and change settings.


To run
```
cproxy cproxy.conf
```

To install it as a systemd service use the makefile

```
sudo make install
```
To uninstall

```
sudo make uninstall
```

This code base uses a bash script called cheader in root directory
to generate the c header files automatically. If that does't work on your system
keep the pre-generated header files in include/ and comment out calles to cheader
in all of the Makefiles

It uses two libraries called cstr and cnet which are included in this source.
