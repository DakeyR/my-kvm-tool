CC=clang
CFLAGS=-Wall -Wextra -g -std=c99

SRC=main.c \
		kvm.c \
		options.c \
		serial.c

OBJS=$(SRC:.c=.o)

all: ${OBJS}
	${CC} ${CLAGS} -o mykvm ${OBJS}

clean:
	rm -rf ${OBJS}
	rm -rf mykvm
