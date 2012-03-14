TARGETS = cpuload memload swpload

all: $(TARGETS)

cpuload: cpuload.c
memload: memload.c
swpload: swpload.c

clean:
	$(RM) *.o *~

distclean: clean
	$(RM) $(TARGETS)
