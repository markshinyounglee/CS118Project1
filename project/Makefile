CC=gcc
CFLAGS= -g   -fsanitize=address  -lcrypto
#      debug  address sanitizer   linking libcrypto

all: clean build

default: build

build: server.c client.c
	${CC} ${CFLAGS} -o server server.c transport.c security.c
	${CC} ${CFLAGS} -o client client.c transport.c security.c

clean:
	rm -rf server client server.txt client.txt 

zip: clean
	rm project2.zip
	mkdir -p project
	cp consts.h security.h transport.h server.c client.c security.c transport.c Makefile project
	zip project2.zip project/*