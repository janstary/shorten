.SUFFIS: .c .o
BOBJS = mkbshift.o
COBJS = exit.o array.o Sulawalaw.o
SOBJS = shorten.o fixio.o vario.o poly.o lpc.o riffwave.o
CC    = cc
CFLAGS= -O

prefix = /usr/local
exec_prefix = $(prefix)/bin
man_prefix = $(prefix)/man/man$(manext)
manext = 1

.c:	; $(CC) $(CFLAGS) -o $* $*.c
.c.o:	; $(CC) $(CFLAGS) -c $*.c

all: shorten shorten.1

install: shorten
	cp shorten $(exec_prefix)
	cp shorten.1 $(man_prefix)

install-debian: shorten
	install -s shorten $(DESTDIR)/usr/bin

shorten: bitshift.h $(COBJS) $(SOBJS)
	$(CC) $(CFLAGS) -o shorten $(COBJS) $(SOBJS) -lm

bitshift.h: mkbshift
	./mkbshift

mkbshift: $(COBJS) $(BOBJS)
	$(CC) $(CFLAGS) -o mkbshift $(COBJS) $(BOBJS) -lm

test: shorten
	./shorten -x mvs_wav.shn tmp.wav
	if [ `wc -lc tmp.wav | sed 's/ //g'` != "17032640tmp.wav" ]; then exit 1; fi
	shntest

clean:
	rm -f $(COBJS) $(BOBJS) $(SOBJS) shorten mkbshift tmp.* *~
