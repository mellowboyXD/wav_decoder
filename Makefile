SRC=./src/wav.c

all: run

build:
	gcc -Wall -Wextra -ggdb -o wav $(SRC)

run: build
	./wav sounds/cosmic.wav
