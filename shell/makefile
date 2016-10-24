CC=gcc
CFLAGS=-g -Wall -I.
DEPS = builtin.h
OBJ = tokenizer.o builtin.o shell.o

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

shell: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f shell *.o
