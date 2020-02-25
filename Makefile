CC ?= gcc

all: kaem

kaem:
	gcc -g $(CFLAGS) \
		-o bin/kaem \
		./functions/file_print.c \
		./functions/match.c \
		./functions/in_set.c \
		./functions/string.c \
		./functions/require.c \
		./functions/numerate_number.c \
		variable.c \
		kaem.c

clean:
	rm bin/*
