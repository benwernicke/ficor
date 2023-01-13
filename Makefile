CC := gcc
DEBUG_FLAGS    := -Wall -pedantic -g -fsanitize=leak -fsanitize=undefined -fsanitize=address
RELEASE_FLAGS  := -march=native -mtune=native -O3 -flto

ficor.out := main.o flag.o

SRC := $(wildcard *.c)
OBJ := ${SRC:c=o}
TARGETS := ficor.out

.PHONY: clean all release debug install uninstall

all: debug

clean:
	rm *.out *.o

debug: CFLAGS := ${DEBUG_FLAGS}
debug: ${TARGETS}

release: CFLAGS := ${RELEASE_FLAGS}
release: ${TARGETS}

${TARGETS}: ${OBJ}
	${CC} ${CFLAGS} ${$@} -o $@
	
%.o: %.c
	${CC} ${CFLAGS} $< -c -o $@

intall:
	cp ficor.out /usr/local/bin/ficor

uinstall:
	rm /usr/local/ficor
