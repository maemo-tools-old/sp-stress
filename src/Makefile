TARGETS = cpuload ioload memload

all: $(TARGETS)

clean:
	$(RM) *.o *~

distclean: clean
	$(RM) $(TARGETS)

cpuload: cpuload.c
	gcc $(CFLAGS) -o $@ $^

ioload: ioload.c
	gcc $(CFLAGS) -lrt -o $@ $^

memload: memload.c
	gcc $(CFLAGS) -o $@ $^
