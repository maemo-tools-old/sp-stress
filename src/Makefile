TARGETS = cpuload ioload memload

all: $(TARGETS)

clean:
	$(RM) *.o *~

distclean: clean
	$(RM) $(TARGETS)

cpuload: cpuload.c
	gcc -Wall -O2 -s -o $@ $^

ioload: stress_io.c
	gcc -W -Wall -s -O2 -lrt -o $@ $^

memload: bubble.c
	gcc -W -Wall -s -O2 -o $@ $^
