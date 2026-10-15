CC   = gcc
WARN = -Wall -Wextra -Wpedantic
SRC  = moscar.c

all: moscar

moscar: $(SRC)
	$(CC) $(WARN) -O2 $< -o $@

moscar_asan: $(SRC)
	$(CC) $(WARN) -g -fsanitize=address,undefined $< -o $@

test: moscar_asan
	BIN=./moscar_asan bash run_tests.sh

lint:
	$(CC) -O2 $(WARN) -Wmaybe-uninitialized -fanalyzer -c $(SRC) -o /dev/null

clean:
	rm -f moscar moscar_asan

.PHONY: all test lint clean


