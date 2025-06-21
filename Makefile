all: run

build:
	gcc -Wall -Wextra -ggdb -o wav wav.c

run: build
	./wav sounds/cosmic.wav
