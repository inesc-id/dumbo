ROOT = ../..

include $(ROOT)/Makefile.common

BINS = bank

.PHONY:	all clean

all:	$(BINS)

%.o:	%.c
	$(CC)  $(CFLAGS) $(DEFINES) -c -o $@ $<

$(BINS):	%:	%.o $(TMLIB)
	$(CC) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(BINS) *.o
