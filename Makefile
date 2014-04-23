CFLAGS = -g
CPPFLAGS =

BIN = lersp

all: $(BIN)

lersp.o: lersp.c lersp.h

clean:
	$(RM) $(BIN) $(BIN).o

.PHONY: all clean
