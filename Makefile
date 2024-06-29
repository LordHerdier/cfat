all:
	gcc cfs.c -o cfs `pkg-config fuse --cflags --libs`

clean:
	rm -f cfs
