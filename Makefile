CFLAGS = -I ./include
LIB    = ./lib/fmod/libfmodex64.so ./libggfonts.so
LFLAGS = $(LIB) -lrt -lX11 -lGL -pthread -lm #-lXrandr

all: parashoot

parashoot: parashoot.cpp ppm.cpp fmod.c log.cpp
	g++ $(CFLAGS) parashoot.cpp ppm.cpp log.cpp fmod.c -Wall -Wextra $(LFLAGS) -o parashoot

clean:
	rm -f parashoot
	rm -f *.o

