
DESTDIR ?= 
PREFIX  ?= /usr
COMPRESS_MAN ?= yes
STRIP_BINARY ?= yes
EXAMPLES ?= yes

CSECFLAGS ?= -fstack-protector-all -Wall --param ssp-buffer-size=4 -D_FORTIFY_SOURCE=2 -fstack-check -DPARANOID -std=gnu99
CFLAGS ?= -pipe -O2
CFLAGS += $(CSECFLAGS)
DEBUGCFLAGS ?= -pipe -Wall -Werror -ggdb3 -export-dynamic -Wno-error=unused-variable -O0 -pipe $(CSECFLAGS)

CARCHFLAGS ?= -march=native

LIBS := -lm
LDSECFLAGS ?= -Xlinker -zrelro
LDFLAGS += $(LDSECFLAGS) -pthread -flto -L. -lfitter_C
INC := $(INC)

INSTDIR = $(DESTDIR)$(PREFIX)

objs=\
error.o\
malloc.o\
pthreadex.o\
analyzer_root.o\
main.o\
binary.o\
libfitter_C.so


binary=voltlogger_analyzer

.PHONY: doc

all: $(objs)
	$(CC) $(CARCHFLAGS) $(CFLAGS) $(LDFLAGS) $(objs) $(LIBS) -o $(binary)

%.o: %.c
	$(CC) $(CARCHFLAGS) $(CFLAGS) $(INC) $< -c -o $@

debug:
	$(CC) $(CARCHFLAGS) -D_DEBUG_SUPPORT $(DEBUGCFLAGS) $(INC) $(LDFLAGS) *.c $(LIBS) -o $(binary)

libfitter_C.so:
	echo '.L libfitter.C+' | root -l

clean:
	rm -f $(binary) $(objs)

distclean: clean

doc:
	doxygen .doxygen

install:
	install -d "$(INSTDIR)/bin" "$(INSTDIR)/share/man/man1"
ifeq ($(STRIP_BINARY),yes)
	strip --strip-unneeded -R .comment -R .GCC.command.line -R .note.gnu.gold-version $(binary)
endif
	install -m 755 $(binary) "$(INSTDIR)"/bin/
	install -m 644 man/man1/voltlogger_analyzer.1 "$(INSTDIR)"/share/man/man1/
ifeq ($(COMPRESS_MAN),yes)
	rm -f "$(INSTDIR)"/share/man/man1/voltlogger_analyzer.1.gz
	gzip "$(INSTDIR)"/share/man/man1/voltlogger_analyzer.1
endif

deinstall:
	rm -f "$(INSTDIR)"/bin/$(binary)

