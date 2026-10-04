# Compiler and Flags
CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE -D_DARWIN_C_SOURCE
LDFLAGS ?= 

# Installation Directories
PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
MANDIR  ?= $(PREFIX)/share/man/man1

# Targets
.PHONY: all clean install uninstall test

all: hmv

hmv: hmv.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

test: hmv
	@echo "Running hmv test suite..."
	@bash ./test_hmv.sh

install: hmv
	@echo "Installing hmv to $(BINDIR)..."
	@install -d $(BINDIR)
	@install -m 755 hmv $(BINDIR)/hmv
	@echo "Generating and installing manual page to $(MANDIR)..."
	@install -d $(MANDIR)
	@./hmv -m > $(MANDIR)/hmv.1
	@chmod 644 $(MANDIR)/hmv.1
	@echo "Installation complete."

uninstall:
	@echo "Removing hmv from $(BINDIR)..."
	@rm -f $(BINDIR)/hmv
	@echo "Removing manual page from $(MANDIR)..."
	@rm -f $(MANDIR)/hmv.1
	@echo "Uninstall complete."

clean:
	@echo "Cleaning build artifacts..."
	@rm -f hmv hmv.1
