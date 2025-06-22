SRC=./src/wav.c

all: run

build: $(SRC)
	gcc -Wall -Wextra -ggdb -o wav $(SRC) -lasound

run: build
	./wav sounds/cosmic.wav

clean: build
	rm wav
