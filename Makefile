CC := gcc
DEBUG_FLAGS    := -Wall -pedantic -g -fsanitize=leak -fsanitize=undefined -fsanitize=address
RELEASE_FLAGS  := -march=native -mtune=native -O3 -flto

ficor.out := main.o flag.o

SRC := $(wildcard *.c)
OBJ := ${SRC:c=o}
TARGETS := ficor.out


all: debug

clean:
	rm *.out *.o

install:
	cp -f ficor.out /usr/local/bin/ficor

debug: CFLAGS := ${DEBUG_FLAGS}
debug: ${TARGETS}

release: CFLAGS := ${RELEASE_FLAGS}
release: ${TARGETS}

${TARGETS}: ${OBJ}
	${CC} ${CFLAGS} ${$@} -o $@
	
%.o: %.c
	${CC} ${CFLAGS} $< -c -o $@


uninstall:
	rm -f /usr/local/ficor

.PHONY: clean all release debug install
