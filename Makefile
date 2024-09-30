all: compile

compile:
	gcc main.c -Wall -I./include -L./lib -l:libraylib.a -lm -o game

compile-debug:
	gcc main.c -g -Wall -I./include -L./lib -l:libraylib.a -lm -o game

check: compile-debug vg

vg:
	valgrind --track-origins=yes --leak-check=full --show-leak-kinds=definite ./game