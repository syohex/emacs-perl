EMACS_ROOT ?= ../..
UNAME_S=$(shell uname -s)
ifeq ($(UNAME_S),Darwin)
	EMACS ?= /Applications/Emacs.app/Contents/MacOS/Emacs
else
	EMACS ?= emacs
endif

CC      = $(shell perl -MConfig -wl -e 'print $$Config{cc}')
LD      = $(shell perl -MConfig -wl -e 'print $$Config{ld}')
CPPFLAGS = -I$(EMACS_ROOT)/src $(shell perl -MExtUtils::Embed -e ccopts)
CFLAGS = -std=gnu99 -ggdb3 -Wall -fPIC $(CPPFLAGS)
PERL_LIBS = $(shell perl -MExtUtils::Embed -e ldopts)

.PHONY : test

all: perl-core.so

perl-core.so: perl-core.o
	$(LD) -shared $(LDFLAGS) -o $@ $^ $(PERL_LIBS)

perl-core.o: perl-core.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	-rm -f perl-core.so perl-core.o

test:
	$(EMACS) -Q -batch -L . $(LOADPATH) \
		-l test/test.el \
		-f ert-run-tests-batch-and-exit
