CC=gcc
CFLAGS= -fsanitize=address -lcrypto
#      debug  address sanitizer   linking libcrypto

all: clean build

default: build

build: server.c client.c transport.c security.c
	${CC} -o server server.c transport.c security.c ${CFLAGS}
	${CC} -o client client.c transport.c security.c ${CFLAGS}

clean:
	rm -rf server client

zip: clean
	rm -f project2.zip
	mkdir -p project
	cp consts.h security.h transport.h server.c client.c security.c transport.c Makefile project
	zip project2.zip project/*