TARGETS = cpuload memload swpload

all: $(TARGETS)

clean:
	$(RM) *.o *~

distclean: clean
	$(RM) $(TARGETS)

cpuload: cpuload.c
	gcc $(CFLAGS) -o $@ $^

memload: memload.c
	gcc $(CFLAGS) -o $@ $^

swpload: swpload.c
	gcc $(CFLAGS) -o $@ $^
