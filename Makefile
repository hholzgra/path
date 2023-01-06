#
# Makefile for path program.
#

CFLAGS = -O3 -Wall -Wmissing-prototypes

path: path.o
	cc -o path path.o

clean:
	rm -f path path.o
