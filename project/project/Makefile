CC=g++
CFLAGS= 

all: clean build

default: build

build: server.cpp client.cpp 
	${CC} -o server server.cpp ${CFLAGS}
	${CC} -o client client.cpp ${CFLAGS}

clean:
	rm -rf server client *.bin *.out *.dSYM

zip: clean
	rm -f project.zip
	mkdir -p project
	cp ../README.md diagnostics.h server.cpp client.cpp Makefile project
	zip project1.zip project/*
