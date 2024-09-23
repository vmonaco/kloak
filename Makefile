#!/usr/bin/make -f

TARGETARCH=$(shell gcc -dumpmachine)

# https://best.openssf.org/Compiler-Hardening-Guides/Compiler-Options-Hardening-Guide-for-C-and-C++.html
#
# Omitted the following flags:
# -D_GLIBCXX_ASSERTIONS  # application is not written in C++
# -fstrict-flex-arrays=3 # not supported in Debian Bookworm's GCC version (12)
# -fPIC -shared          # not a shared library
# -fexceptions           # not multithreaded
# -fhardened             # not supported in Debian Bookworm's GCC version (12)
#
# Added the following flags:
# -fsanitize=address,undefined # enable ASan/UBSan
CFLAGS = -O2 -Wall -Wformat -Wformat=2 -Wconversion -Wimplicit-fallthrough \
  -Werror=format-security -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 \
	-fstack-clash-protection \
	-fstack-protector-strong -Wl,-z,nodlopen -Wl,-z,noexecstack -Wl,-z,relro \
	-Wl,-z,now -Wl,--as-needed -Wl,--no-copy-dt-needed-entries -Wtrampolines \
	-Wbidi-chars=any -fPIE -pie -Werror=implicit \
	-Werror=incompatible-pointer-types -Werror=int-conversion \
	-fno-delete-null-pointer-checks -fno-strict-overflow -fno-strict-aliasing \
	-fsanitize=address,undefined

ifeq ($(TARGETARCH), x86_64-linux-gnu)
	CFLAGS += -fcf-protection=full # only supported on x86_64
endif
ifeq ($(TARGETARCH), aarch64-linux-gnu)
  CFLAGS += -mbranch-protection=standard # only supported on aarch64
endif

all : kloak eventcap

kloak : src/main.c src/keycodes.c src/keycodes.h
	gcc src/main.c src/keycodes.c -o kloak -lm $(shell pkg-config --cflags --libs libevdev) $(shell pkg-config --cflags --libs libsodium) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)

eventcap : src/eventcap.c
	gcc src/eventcap.c -o eventcap $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)

clean :
	rm -f kloak eventcap
