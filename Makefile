CC?=cc
OBJS=regopen.o
PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
MANDIR=$(PREFIX)/share/man

all: regopen

regopen: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c $<

install: regopen regopen.1
	mkdir -p $(DESTDIR)$(BINDIR) $(DESTDIR)$(MANDIR)/man1
	install -m755 regopen $(DESTDIR)$(BINDIR)/
	install -m644 regopen.1 $(DESTDIR)$(MANDIR)/man1/

clean:
	rm -f regopen $(OBJS)
