# Compiler and Flags
CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -D_DEFAULT_SOURCE -D_XOPEN_SOURCE=700

# Installation Directories
PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin

# Targets
.PHONY: all clean install uninstall test

all: hcal

hcal: hcal.c
	$(CC) $(CFLAGS) -o $@ $<

test: hcal
	@echo "Running hcal test suite..."
	@bash ./test_hcal.sh

install: hcal
	@echo "Installing hcal to $(BINDIR)..."
	@install -d $(BINDIR)
	@install -m 755 hcal $(BINDIR)/hcal
	@echo "Installation complete."

uninstall:
	@echo "Removing hcal from $(BINDIR)..."
	@rm -f $(BINDIR)/hcal
	@echo "Uninstall complete."

clean:
	@echo "Cleaning build artifacts..."
	@rm -f hcal
