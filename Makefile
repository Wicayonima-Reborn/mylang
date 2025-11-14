# ============================
# Cross-Platform Makefile
# Works on Windows (MinGW) + Linux
# ============================

# OS detection
ifeq ($(OS),Windows_NT)
    ASM_FORMAT = win64
    OBJ_EXT = obj
    EXE_EXT = .exe
    RM = del /f
    RUN_PREFIX = ./
else
    ASM_FORMAT = elf64
    OBJ_EXT = o
    EXE_EXT =
    RM = rm -f
    RUN_PREFIX = ./
endif

CC = gcc
CFLAGS = -std=c99 -O2 -Iinclude

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)

all: mycc

# Build compiler
mycc: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o mycc

# Build the runtime only (needed for linking generated asm)
runtime.o: src/runtime.c
	$(CC) -c src/runtime.c -o runtime.o

# Clean
clean:
	$(RM) *.o *.obj *.exe *.asm *.o *.obj test$(EXE_EXT) mycc$(EXE_EXT)

# Example: compile test.my automatically
example: mycc runtime.o
	./mycc examples/test.my -o test
	nasm -f $(ASM_FORMAT) test.asm -o test.$(OBJ_EXT)
	$(CC) test.$(OBJ_EXT) runtime.o -o test$(EXE_EXT)

run-example: example
	$(RUN_PREFIX)test$(EXE_EXT)

# Just run if already built
run:
	$(RUN_PREFIX)test$(EXE_EXT)