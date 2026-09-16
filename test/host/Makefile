# Host build of the rule tests. No Pebble SDK involved.
CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined
SRC      = test_game.c ../../src/c/game.c ../../src/c/levels.c

test_game: $(SRC) ../../src/c/game.h ../../src/c/levels.h
	$(CC) $(CFLAGS) -o $@ $(SRC)

.PHONY: run clean
run: test_game
	./test_game

clean:
	rm -rf test_game test_game.dSYM
