CC = gcc
SRC = ./src/*.c
BUILD = ./build/*.o
INCLUDE = ./include/
BIN = bin/utf
UTF = build/utf.o
CFLAGS = -Wall -Werror -g -pedantic -Wextra 
REQ = $(SRC) ./include/*.h

all: dirs $(BIN)

debug: all

dirs: 
	mkdir -p bin
	mkdir -p build

$(BIN): $(UTF)
	$(CC) $(CFLAGS) -I $(INCLUDE) $^ -o $(BIN)

$(UTF): $(REQ)
	$(CC) $(CFLAGS) -I $(INCLUDE) -c $(SRC) -o $@

.PHONY: clean

clean:
	rm -rf bin build

