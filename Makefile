.POSIX:

PROG= untangle
SRCS= main.c
OBJS= $(SRCS:.c=.o)

CC= cc
CFLAGS= -std=c99 -Wall -Wextra -pedantic
CPPFLAGS= -D_POSIX_C_SOURCE=200809L
LDFLAGS=

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o $(PROG) $(OBJS)

.c.o:
	$(CC) $(CPPFLAGS) $(CLFAGS) -c $<

clean:
	rm -v $(PROG) $(OBJS)

run: $(PROG)
	./$(PROG)

.PHONY: all clean run
