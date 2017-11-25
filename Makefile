
SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)
HDRS := $(wildcard *.h)

CFLAGS := -g `pkg-config fuse --cflags`
LDLIBS := `pkg-config fuse --libs` -lbsd

nufs: directory.c nufs.c storage.c 
	gcc $(CFLAGS) -o nufs $^ $(LDLIBS)

test-code: test.c directory.c storage.c
	gcc $(CFLAGS) -o test $^ $(LDLIBS)

clean: unmount
	rm -f nufs *.o test.log
	rmdir mnt || true
	rm -f data.nufs

mount: nufs
	mkdir -p mnt || true
	./nufs -s -f mnt data.nufs

unmount:
	fusermount -u mnt || true

test: nufs
	perl test.pl

gdb: nufs
	mkdir -p mnt || true
	gdb --args ./nufs -f mnt data.nufs

.PHONY: clean mount unmount gdb
