# Serial port over Telnet
# Lubomir Rintel <lkundrak@v3.sk>
# License: GPL

VERSION = 1.0

PREFIX = /usr/local

OBJS = nets.o netsctl.o common.o
BINS = nets netsctl
MAN1 = nets.1 netsctl.1

all: $(BINS) $(MAN1)

$(OBJS): common.h
$(BINS): common.o

%.1: %.pod
	pod2man --center 'User Commands' --section 1 --release $(VERSION) $< >$@

%.8: %.pod
	pod2man --center 'System management --section 8 commands' --release $(VERSION) $< >$@

%.html: %.pod
	pod2html $< >$@

clean:
	rm -f *.o $(OBJS) $(BINS) $(MAN1)

install: $(BINS)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -p $(BINS) $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(PREFIX)/share/man/man1
	install -p -m644 $(MAN1) $(DESTDIR)$(PREFIX)/share/man/man1
