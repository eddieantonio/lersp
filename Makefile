CPPFLAGS = -DGC_DEBUG=1

BIN = lersp

all: $(BIN)

clean:
	$(RM) $(BIN)

.PHONY: all clean
