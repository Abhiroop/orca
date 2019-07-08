OBJECTS = acp.o comb_list.o dictionary.o edge.o edgelist.o fifo.o \
          grammar.o nodelist.o parser.o symbol_list.o wordlist.o

AOUT    = acp

CC      = gcc

CFLAGS  = -c -O2

LD      = gcc

LDFLAGS =

LIBS    = 

default: $(OBJECTS) const.h types.h
	$(LD) $(LDFLAGS) -o $(AOUT) $(OBJECTS) $(LIBS)

acp.o: 
	$(CC) $(CFLAGS) acp.c

comb_list.o: 
	$(CC) $(CFLAGS) comb_list.c

dictionary.o:
	$(CC) $(CFLAGS) dictionary.c

edge.o: 
	$(CC) $(CFLAGS) edge.c

edgelist.o:
	$(CC) $(CFLAGS) edgelist.c

fifo.o:
	$(CC) $(CFLAGS) fifo.c

grammar.o: 
	$(CC) $(CFLAGS) grammar.c

nodelist.o:
	$(CC) $(CFLAGS) nodelist.c

parser.o:
	$(CC) $(CFLAGS) parser.c

symbol_list.o:
	$(CC) $(CFLAGS) symbol_list.c

wordlist.o:
	$(CC) $(CFLAGS) wordlist.c

clean:
	/bin/rm -f *.o $(AOUT)

lint:
	lint -hbaxc *.c *.h




