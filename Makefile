CC=gcc
RM=rm -f
LDFLAGS=-g -Wall -Wextra

SRCS=main.c
OBJS=$(subst .c,.o,$(SRCS))

all: radio

radio: $(OBJS)
	$(CC) $(LDFLAGS) -o radio $(OBJS)

main.o: main.c

clean:
	$(RM) $(OBJS)

dist-clean: clean
	$(RM) radio
