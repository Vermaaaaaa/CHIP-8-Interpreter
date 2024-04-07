# Compiler flags
CFLAGS = -std=c17 -Wall -Wextra -Werror -Wno-format

# Library flags
LDFLAGS = -L D:\SDL2-2.28.4\lib\x64
LDLIBS = -l SDL2

# Target and its dependencies
all: test.c
	gcc $(CFLAGS) test.c -o test $(LDFLAGS) $(LDLIBS)
