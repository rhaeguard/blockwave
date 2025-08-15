all: compile

compile:
	gcc main.c -std=c99 -Wall -I./include -L./lib -l:libraylib.a -lm -o game

compile-debug:
	gcc main.c -std=c99 -g -Wall -I./include -L./lib -l:libraylib.a -lm -o game

check: compile-debug vg

vg:
	valgrind --track-origins=yes --leak-check=full --show-leak-kinds=definite ./game

windows:
	gcc main.c -std=c99 -Wall -I./include -L./lib -l:raylib.dll -lm -o game