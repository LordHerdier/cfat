all:
	gcc myfs.c -o myfs `pkg-config fuse --cflags --libs`

clean:
	rm -f myfs

restore:
	cp empty.cfs.bak empty.cfs
	gcc myfs.c -o myfs `pkg-config fuse --cflags --libs`
	clear
	./myfs -f empty.cfs -m mnt -v
