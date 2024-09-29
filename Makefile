all: 
	gcc main.c -Wall -I./include -L./lib -l:libraylib.a -lm -o game