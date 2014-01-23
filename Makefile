CFLAGS=-O3 -g -std=c99 -Wall -Wextra

.PHONY: all
all: rpilogic rpldecode

rpilogic: LDFLAGS=-lbcm2835
rpilogic: rpilogic.o
	$(CC) $^ $(LDFLAGS) -o $@

.PHONY: clean
clean:
	rm -f *.o rpilogic rpldecode
