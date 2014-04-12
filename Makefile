CFLAGS = -g
CPPFLAGS = -DGC_DEBUG=1

BIN = lersp


all: $(BIN)

lersp.o: lersp.c lersp.h

clean:
	$(RM) $(BIN) $(BIN).o

.PHONY: all clean
