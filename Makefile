CC=clang
CFLAGS=-I.
DEPS = mc.h
OBJ = main.o mc.o 

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

main: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)
