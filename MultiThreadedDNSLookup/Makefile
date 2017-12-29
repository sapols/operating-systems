CC = gcc
CFLAGS = -c -g -Wall -Wextra
LFLAGS = -Wall -Wextra -pthread

all: multi-lookup

multi-lookup: multi-lookup.o sharedArray.o util.o
	$(CC) $(LFLAGS) $^ -o $@

sharedArray.o: sharedArray.c sharedArray.h
	$(CC) $(CFLAGS) $<

util.o: util.c util.h
	$(CC) $(CFLAGS) $<

multi-lookup.o: multi-lookup.c multi-lookup.h
	$(CC) $(CFLAGS) $<

clean:
	rm -f multi-lookup
	rm -f *.o
	rm -f *~
	rm -f results.txt
	rm -f serviced.txt
