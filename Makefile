all : kloak eventcap

kloak : src/main.c src/keycodes.c src/keycodes.h
	gcc src/main.c src/keycodes.c src/keycodes.h -o kloak -lm -lpthread

eventcap : src/eventcap.c
	gcc src/eventcap.c -o eventcap

clean :
	rm -rf kloak eventcap src/main.o src/eventcap.o
