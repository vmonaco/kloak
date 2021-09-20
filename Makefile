#!/usr/bin/make -f

all : kloak eventcap

kloak : src/main.c src/keycodes.c src/keycodes.h
	gcc src/main.c src/keycodes.c -o kloak -lm -levdev -lsodium -I/usr/include/libevdev-1.0/ $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)

eventcap : src/eventcap.c
	gcc src/eventcap.c -o eventcap $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)

clean :
	rm -f kloak eventcap
