CC 						= gcc
DIR_RAYLIB 				= external/raylib/src
INCLUDE 				= -I$(DIR_RAYLIB)
STATIC_LIBS 			= $(DIR_RAYLIB)/libraylib.a
CFLAGS					= -std=c99 -Wextra -pedantic -ggdb
# By default we suppose we are working on Linux
PLATFORM_OS 			?= LINUX
# OS env variable in Windows is equal to Windows_NT
ifeq ($(OS),Windows_NT)
	PLATFORM_OS = WINDOWS
	LDFLAGS		= -lopengl32 -lgdi32 -lwinmm
	GAME_NAME	= blockwave.exe
	RUN_CMD 	= .\$(GAME_NAME)
else
# extract the OS name from uname command
	UNAMEOS = $(shell uname)
	ifeq ($(UNAMEOS),Linux)
		PLATFORM_OS = LINUX
		LDFLAGS		=-lm
		GAME_NAME	= blockwave
		RUN_CMD 	= ./$(GAME_NAME)
	endif
	ifeq ($(UNAMEOS),Darwin)
# I haven't actually tried this on OSX
		PLATFORM_OS = OSX
		LDFLAGS		=-lm
		GAME_NAME	= blockwave
		RUN_CMD 	= ./$(GAME_NAME)
	endif
endif

run: compile
	$(RUN_CMD)

compile: raylib
	$(CC) main.c $(CFLAGS) $(INCLUDE) $(STATIC_LIBS) $(LDFLAGS) -o $(GAME_NAME)

raylib:
	$(MAKE) -C $(DIR_RAYLIB) PLATFORM=PLATFORM_DESKTOP

valgrind:
	valgrind --track-origins=yes --leak-check=full --show-leak-kinds=definite $(GAME_NAME)
