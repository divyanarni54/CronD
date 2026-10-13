CC = gcc

CFLAGS = -Wall -Wextra -std=c11 -Iinclude

CROND = crond
CRONDCTL = crondctl

CROND_SRC = src/crond.c \
            src/parser.c \
            src/scheduler.c \
            src/command.c \
            src/control.c

CROND_OBJ = $(CROND_SRC:.c=.o)

all: $(CROND) $(CRONDCTL)

$(CROND): $(CROND_OBJ)
	$(CC) $(CROND_OBJ) -o $(CROND)

$(CRONDCTL): src/crondctl.c
	$(CC) $(CFLAGS) src/crondctl.c -o $(CRONDCTL)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(CROND_OBJ) $(CROND) $(CRONDCTL)

run: $(CROND)
	./$(CROND)

.PHONY: all clean run
