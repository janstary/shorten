CFLAGS	= -Wall -pedantic

PREFIX	= ${DESTDIR}/${HOME}
BINDIR	= ${PREFIX}/bin/
MANDIR	= ${PREFIX}/man/man1/

BIN 	= shorten
MAN 	= shorten.1
OBJS	=		\
	array.o		\
	exit.o		\
	fixio.o		\
	lpc.o		\
	poly.o		\
	riffwave.o	\
	ualaw.o		\
	shorten.o	\
	vario.o

all: ${BIN}

${BIN}: ${OBJS} bitshift.h
	${CC} ${CFLAGS} -o ${BIN} ${OBJS}

bitshift.h: mkbshift
	./mkbshift

mkbshift: mkbshift.c exit.o array.o ualaw.o
	$(CC) $(CFLAGS) -o mkbshift mkbshift.c exit.o array.o ualaw.o -lm

%.o: %.c shorten.h
	${CC} ${CFLAGS} -c $<

lint: ${MAN}
	mandoc -Tlint ${MAN}

test: shorten
	./shntest

install: ${BIN} ${MAN}
	install -m 755 -d ${BINDIR}
	install -m 755 -d ${MANDIR}
	install -m 755 ${BIN} ${BINDIR}
	install -m 664 ${MAN} ${MANDIR}

uninstall:
	rm -f ${BINDIR}/${BIN}
	rm -f ${MANDIR}/${MAN}

clean:
	rm -f -- ${BIN} ${OBJS} mkbshift *.core *~
