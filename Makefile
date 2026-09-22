SRC := $(wildcard src/*.c)

all:
	gcc $(SRC) -Isrc -o cfs `pkg-config fuse --cflags --libs`

clean:
	rm -f cfs
