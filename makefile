CFLAGS =-std=c17 -Wall -Wextra -Werror

all:
	gcc main.c -o $(CFLAGS) 'sdl2-config --cflags --libs'