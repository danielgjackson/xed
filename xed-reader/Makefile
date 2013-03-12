CC = gcc
CFLAGS = -I./include
DEPS = include/xed/xed.h
LIBS =
#LIBS = -lm -ldl -lpthread
OBJ = src/xed_decode.o src/xed.o

all: xed_decode

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

xed_decode: $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

clean:
	-rm *.o xed_decode
