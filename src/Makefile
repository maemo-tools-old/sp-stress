TARGETS = cpuload ioload memload

all: $(TARGETS)

clean:
	$(RM) *.o *~

distclean: clean
	$(RM) $(TARGETS)

cpuload: cpuload.c
	gcc $(CFLAGS) -s -o $@ $^

ioload: ioload.c
	gcc $(CFLAGS) -s -lrt -o $@ $^

memload: memload.c
	gcc $(CFLAGS) -s -o $@ $^
