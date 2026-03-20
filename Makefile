CC 						= gcc
DIR_RAYLIB 				= external/raylib/src
INCLUDE 				= -I$(DIR_RAYLIB)
STATIC_LIBS 			= $(DIR_RAYLIB)/libraylib.a
CFLAGS					= -std=c99 -Wextra -pedantic -ggdb
PLATFORM 				?= WINDOWS
ifeq ($(PLATFORM), WINDOWS)
    LDFLAGS		= -lopengl32 -lgdi32 -lwinmm
	GAME_NAME	= blockwave.exe
	RUN_CMD 	= .\$(GAME_NAME)
else
    LDFLAGS=-lm
	GAME_NAME	= blockwave
	RUN_CMD 	= ./$(GAME_NAME)
endif

run: compile
	$(RUN_CMD)

compile: raylib
	$(CC) main.c $(CFLAGS) $(INCLUDE) $(STATIC_LIBS) $(LDFLAGS) -o $(GAME_NAME)

raylib:
	$(MAKE) -C $(DIR_RAYLIB) PLATFORM=PLATFORM_DESKTOP

valgrind:
	valgrind --track-origins=yes --leak-check=full --show-leak-kinds=definite $(GAME_NAME)
