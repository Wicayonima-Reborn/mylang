# ============================
# Cross-Platform Makefile
# Safe for PowerShell, CMD, Git Bash, WSL, Linux
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
RM = rm -rf

# -------- Directories --------
SRCDIR   = src
BUILDDIR = build
OBJDIR   = $(BUILDDIR)/obj
BINDIR   = $(BUILDDIR)/bin
ASMDIR   = $(BUILDDIR)/asm

# -------- Sources --------
SRC = $(filter-out $(SRCDIR)/runtime.c, $(wildcard $(SRCDIR)/*.c))
OBJ = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.$(OBJ_EXT),$(SRC))
RUNTIME_OBJ = $(OBJDIR)/runtime.$(OBJ_EXT)

# -------- Targets --------
all: mycc

# -------- Directory rules --------
$(OBJDIR):
ifeq ($(OS),Windows_NT)
	@if not exist $(OBJDIR) mkdir $(OBJDIR)
else
	@mkdir -p $(OBJDIR)
endif

$(BINDIR):
ifeq ($(OS),Windows_NT)
	@if not exist $(BINDIR) mkdir $(BINDIR)
else
	@mkdir -p $(BINDIR)
endif

$(ASMDIR):
ifeq ($(OS),Windows_NT)
	@if not exist $(ASMDIR) mkdir $(ASMDIR)
else
	@mkdir -p $(ASMDIR)
endif

# -------- Build compiler --------
mycc: $(OBJ) | $(BINDIR)
	$(CC) $(CFLAGS) $(OBJ) -o $(BINDIR)/mycc$(EXE_EXT)

# -------- Object rules --------
$(OBJDIR)/%.$(OBJ_EXT): $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(RUNTIME_OBJ): $(SRCDIR)/runtime.c | $(OBJDIR)
	$(CC) -c $< -o $@

# -------- Example pipeline --------
example: mycc $(RUNTIME_OBJ) | $(ASMDIR)
	$(BINDIR)/mycc$(EXE_EXT) examples/test.my -o $(ASMDIR)/test
	nasm -f $(ASM_FORMAT) $(ASMDIR)/test.asm -o $(OBJDIR)/test.$(OBJ_EXT)
	$(CC) $(OBJDIR)/test.$(OBJ_EXT) $(RUNTIME_OBJ) -o $(BINDIR)/test$(EXE_EXT)

run-example: example
	$(BINDIR)/test$(EXE_EXT)

# -------- Sanity check --------
check: mycc
	$(BINDIR)/mycc$(EXE_EXT) --help || true

# -------- Clean --------
clean:
	$(RM) $(BUILDDIR)