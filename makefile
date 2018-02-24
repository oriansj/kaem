## Copyright (C) 2016 Jeremiah Orians
## This file is part of stage0.
##
## stage0 is free software: you an redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## stage0 is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with stage0.  If not, see <http://www.gnu.org/licenses/>.

# Prevent rebuilding
VPATH = bin:test:test/results

all: kaem

CC=gcc
CFLAGS=-D_GNU_SOURCE -std=c99 -ggdb

kaem: kaem.c | bin
	$(CC) $(CFLAGS) kaem.c -o bin/kaem

# Clean up after ourselves
.PHONY: clean
clean:
	rm -rf bin/

# Directories
bin:
	mkdir -p bin

DESTDIR:=
PREFIX:=/usr/local
bindir:=$(DESTDIR)$(PREFIX)/bin
.PHONY: install
install: kaem
	mkdir -p $(bindir)
	cp $^ $(bindir)
