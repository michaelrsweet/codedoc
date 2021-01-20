#
# Makefile for codedoc, a code documentation utility.
#
#     https://www.msweet.org/codedoc
#
# Copyright © 2003-2021 by Michael R Sweet.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

VERSION	=	3.7
prefix	=	$(DESTDIR)/usr/local
includedir =	$(prefix)/include
bindir	=	$(prefix)/bin
libdir	=	$(prefix)/lib
mandir	=	$(prefix)/share/man

CC	=	gcc
CFLAGS	=	$(OPTIM) -Wall '-DVERSION="$(VERSION)"' $(OPTIONS)
LDFLAGS	=	$(OPTIM)
LIBS	=	-lmxml -lz -lm
OPTIM	=	-Os -g
OPTIONS	=
#OPTIONS	=	-DDEBUG=1

OBJS	=	codedoc.o mmd.o zipc.o
TARGETS	=	codedoc

.SUFFIXES:	.c .o
.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

all:		$(TARGETS)

clean:
	rm -f $(TARGETS) $(OBJS)

install:	$(TARGETS)
	mkdir -p $(bindir)
	cp codedoc $(bindir)
	mkdir -p $(mandir)/man1
	cp codedoc.1 $(mandir)/man1

cppcheck:
	cppcheck --template=gcc --addon=cert.py --suppress=cert-MSC24-C --suppress=cert-EXP05-C --suppress=cert-API01-C --suppress=cert-INT31-c --suppress=cert-STR05-C $(OBJS:.o=.c) 2>cppcheck.log
	@test -s cppcheck.log && (echo ""; echo "Errors detected:"; echo ""; cat cppcheck.log; exit 1) || exit 0

sanitizer:
	$(MAKE) clean
	$(MAKE) OPTIM="-g -fsanitize=address" all

TESTOPTIONS	=	\
			--author "Michael R Sweet" \
			--body testfiles/body.md \
			--copyright "Copyright © 2003-2019 by Michael R Sweet" \
			--coverimage codedoc-256.png \
			--docversion $(VERSION) \
			--title "Test Documentation" \
			--footer DOCUMENTATION.md

test:		codedoc
	rm -f test.xml
	./codedoc $(TESTOPTIONS) test.xml testfiles/*.cxx >test.html
	./codedoc $(TESTOPTIONS) --man test test.xml >test.man
	./codedoc $(TESTOPTIONS) --epub test.epub test.xml

codedoc:	$(OBJS)
	$(CC) $(LDFLAGS) -o codedoc $(OBJS) $(LIBS)

$(OBJS):	Makefile
codedoc.o:	mmd.h zipc.h
mmd.o:		mmd.h
zipc.o:		zipc.h

codedoc.html:	codedoc DOCUMENTATION.md
	./codedoc --body DOCUMENTATION.md >codedoc.html
