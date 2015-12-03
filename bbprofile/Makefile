LDFLAGS = -lrt
CFLAGS = -static
TARGETS = bbprof_launch bbprofile

all: $(TARGETS)

bbprof_launch:
	gcc $(CFLAGS) -o $@ bbprof_launch.c $(LDFLAGS)

bbprofile:
	gcc $(CFLAGS) -c bbprofile.c $(LDFLAGS)

clean:
	rm -rf bbprof_launch bbprofile.o

# vim:ft=make
#
