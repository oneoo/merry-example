SYS := $(shell gcc -dumpmachine)
CC = gcc
OPTIMIZATION = -O3

CFLAGS = -lm -ldl -lpthread $(HARDMODE)
ifeq (, $(findstring linux, $(SYS)))
CFLAGS = -liconv -lm -ldl -lpthread $(HARDMODE)
endif

DEBUG = -g -ggdb -Wall

ifndef $(PREFIX)
PREFIX = /usr/local
endif

INCLUDES=-I/usr/local/include

all: example

example : main.o
	$(CC)  objs/*.o -o $@ $(CFLAGS) $(DEBUG)

main.o:
	[ -d objs ] || mkdir objs;
	cd objs && $(CC) -c ../merry/common/*.c $(DEBUG) $(INCLUDES) $(HARDMODE);
	cd objs && $(CC) -c ../merry/se/*.c $(DEBUG) $(INCLUDES) $(HARDMODE);
	cd objs && $(CC) -c ../merry/*.c $(DEBUG) $(INCLUDES) $(HARDMODE);
	cd objs && $(CC) -c ../*.c $(DEBUG) $(INCLUDES) $(HARDMODE);

.PHONY : clean zip install noopt hardmode

clean:
	rm -rf objs
	rm -rf example
	rm -rf *.o

install:all
	$(MAKE) DEBUG=$(OPTIMIZATION);
	strip example;
	[ -d $(PREFIX) ] || mkdir $(PREFIX);
	! [ -f $(PREFIX)/example ] || mv $(PREFIX)/example $(PREFIX)/example.old
	cp example $(PREFIX)/

noopt:
	$(MAKE) OPTIMIZATION=""

hardmode:
	$(MAKE) HARDMODE="-std=c99 -pedantic -Wall"