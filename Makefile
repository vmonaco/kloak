#!/usr/bin/make -f

all : kloak eventcap

kloak : src/main.c src/keycodes.c src/keycodes.h
	gcc -Wstack-protector -fstack-protector-strong -fstack-clash-protection -pie -fPIE -D_FORTIFY_SOURCE=2 -O -Wl,-z,relro,-z,now -Wl,-z,noexecstack -Wformat -Wformat-security  src/main.c src/keycodes.c -o kloak -lm $(shell pkg-config --cflags --libs libevdev) $(shell pkg-config --cflags --libs libsodium) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)

eventcap : src/eventcap.c
	gcc src/eventcap.c -o eventcap $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)

clean :
	rm -f kloak eventcap
