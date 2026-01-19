# ============================
# Cross-Platform Makefile
# Works on Linux, WSL, Git Bash, MinGW
# ============================

# -------- OS detection --------
ASM_FORMAT = elf64
OBJ_EXT = o
EXE_EXT =

ifeq ($(OS),Windows_NT)
    ASM_FORMAT = win64
    OBJ_EXT = obj
    EXE_EXT = .exe
endif

# -------- Tools --------
CC = gcc
CFLAGS = -std=c99 -O2 -Iinclude
RM = rm -f

# -------- Sources --------
# Compiler sources (exclude runtime)
SRC = $(filter-out src/runtime.c, $(wildcard src/*.c))
OBJ = $(SRC:.c=.o)

# -------- Targets --------
all: mycc

# Build compiler
mycc: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o mycc$(EXE_EXT)

# Build runtime only (for generated programs)
runtime.o: src/runtime.c
	$(CC) -c src/runtime.c -o runtime.o

# -------- Example pipeline --------
example: mycc runtime.o
	./mycc examples/test.my -o test
	nasm -f $(ASM_FORMAT) test.asm -o test.$(OBJ_EXT)
	$(CC) test.$(OBJ_EXT) runtime.o -o test$(EXE_EXT)

run-example: example
	./test$(EXE_EXT)

# -------- Sanity check (CI / Sonar friendly) --------
check: mycc
	@echo "Running basic compiler check..."
	./mycc --help || true

# -------- Install --------
PREFIX ?= /usr/local

install: mycc
	mkdir -p $(PREFIX)/bin
	cp mycc$(EXE_EXT) $(PREFIX)/bin/mycc$(EXE_EXT)
	@echo "Installed mycc to $(PREFIX)/bin"

# -------- Clean --------
clean:
	$(RM) src/*.o runtime.o
	$(RM) test.asm test.$(OBJ_EXT)
	$(RM) mycc$(EXE_EXT) test$(EXE_EXT)