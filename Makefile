# ══════════════════════════════════════════════════════════════════════
#  Nolqu Language Runtime — Makefile
#  v1.0.0-rc1 — Mixed C / C++ build
#
#  ARCHITECTURE:
#    Runtime core (VM, GC, memory, values, objects)  → compiled as C11
#    Tooling layer (lexer, parser, compiler, REPL, CLI) → compiled as C++17
#
#  PLATFORMS:
#    Linux:           make             (gcc + g++)
#    macOS:           make CC=clang CXX=clang++
#    Windows (MinGW): make             (gcc + g++ from MinGW-w64)
#    Termux (Android):make             (gcc + g++ from pkg install clang)
#
#  USAGE:
#    make           — release build  → ./nq
#    make debug     — debug + ASan   → ./nq-debug
#    make test      — run test suite
#    make install   — install to /usr/local/bin
#    make clean     — remove build artefacts
# ══════════════════════════════════════════════════════════════════════

# ── Toolchain ─────────────────────────────────────────────────────────
CC      = gcc
CXX     = g++
TARGET  = nq
SRCDIR  = src
INCDIR  = include
OBJDIR  = build

# ── Compiler flags ────────────────────────────────────────────────────
#
# Shared flags (applied to both C and C++ compilation):
COMMON_FLAGS = \
    -Wall -Wextra \
    -Wno-unused-parameter \
    -I$(SRCDIR) -I$(INCDIR)

# C runtime core — strict C11 + pedantic
CFLAGS = $(COMMON_FLAGS) \
    -std=c11 \
    -Wpedantic

# C++ tooling layer — C++17, no VLAs warning
CXXFLAGS = $(COMMON_FLAGS) \
    -std=c++17

LDFLAGS = -lm

# ── Release flags ──────────────────────────────────────────────────────
# O3 + march=native: full scalar optimization + use all CPU instructions.
# flto: link-time optimization — inlines across translation units (big win
#       for the hot VM dispatch loop which calls helpers defined in vm.c).
CFLAGS_RELEASE   = $(CFLAGS)   -O3 -march=native -flto -DNDEBUG
CXXFLAGS_RELEASE = $(CXXFLAGS) -O3 -march=native -flto -DNDEBUG

# ── Debug flags ────────────────────────────────────────────────────────
# AddressSanitizer + UndefinedBehaviourSanitizer (Linux / macOS only).
# On Windows / Termux omit -fsanitize if not supported.
CFLAGS_DEBUG   = $(CFLAGS)   -g3 -O0 -DDEBUG -DNQ_DEBUG_TRACE \
                 -fsanitize=address,undefined
CXXFLAGS_DEBUG = $(CXXFLAGS) -g3 -O0 -DDEBUG -DNQ_DEBUG_TRACE \
                 -fsanitize=address,undefined

# ── Source file lists ──────────────────────────────────────────────────
#
# C runtime core — performance-sensitive, stays as C11
C_SOURCES = \
    $(SRCDIR)/memory.c   \
    $(SRCDIR)/value.c    \
    $(SRCDIR)/chunk.c    \
    $(SRCDIR)/object.c   \
    $(SRCDIR)/table.c    \
    $(SRCDIR)/gc.c       \
    $(SRCDIR)/vm.c       \
    $(SRCDIR)/jit.c

# C++ tooling layer — lexer, parser, compiler, REPL, CLI, transpiler
CXX_SOURCES = \
    $(SRCDIR)/lexer.cpp    \
    $(SRCDIR)/ast.cpp      \
    $(SRCDIR)/parser.cpp   \
    $(SRCDIR)/compiler.cpp \
    $(SRCDIR)/codegen.cpp  \
    $(SRCDIR)/nq_embed.cpp \
    $(SRCDIR)/repl.cpp     \
    $(SRCDIR)/main.cpp

# ── Object file derivation ─────────────────────────────────────────────
C_OBJS_REL   = $(patsubst $(SRCDIR)/%.c,   $(OBJDIR)/release/%.o, $(C_SOURCES))
CXX_OBJS_REL = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/release/%.o, $(CXX_SOURCES))
ALL_OBJS_REL = $(C_OBJS_REL) $(CXX_OBJS_REL)

C_OBJS_DBG   = $(patsubst $(SRCDIR)/%.c,   $(OBJDIR)/debug/%.o, $(C_SOURCES))
CXX_OBJS_DBG = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/debug/%.o, $(CXX_SOURCES))
ALL_OBJS_DBG = $(C_OBJS_DBG) $(CXX_OBJS_DBG)

# ── Phony targets ──────────────────────────────────────────────────────
.PHONY: all release debug clean install uninstall test check

all: release

# ── Release build ──────────────────────────────────────────────────────
release: $(OBJDIR)/release $(TARGET)

$(TARGET): $(ALL_OBJS_REL)
	@echo "  LD    $(TARGET)"
	@$(CXX) $(CXXFLAGS_RELEASE) -o $@ $^ $(LDFLAGS)
	@echo ""
	@echo "  ✓ Build successful: ./$(TARGET)"
	@echo "    Run:   ./$(TARGET) program.nq"
	@echo "    REPL:  ./$(TARGET) repl"
	@echo "    Check: ./$(TARGET) check program.nq"

# C runtime core — compiled with gcc / clang as C11
$(OBJDIR)/release/%.o: $(SRCDIR)/%.c
	@echo "  CC    $<"
	@$(CC) $(CFLAGS_RELEASE) -c -o $@ $<

# C++ tooling layer — compiled with g++ / clang++ as C++17
$(OBJDIR)/release/%.o: $(SRCDIR)/%.cpp
	@echo "  CXX   $<"
	@$(CXX) $(CXXFLAGS_RELEASE) -c -o $@ $<

# ── Debug build ────────────────────────────────────────────────────────
debug: $(OBJDIR)/debug $(TARGET)-debug

$(TARGET)-debug: $(ALL_OBJS_DBG)
	@echo "  LD    $(TARGET)-debug"
	@$(CXX) $(CXXFLAGS_DEBUG) -o $@ $^ $(LDFLAGS)
	@echo "  ✓ Debug build: ./$(TARGET)-debug (ASan + UBSan)"

$(OBJDIR)/debug/%.o: $(SRCDIR)/%.c
	@echo "  CC    $< [debug]"
	@$(CC) $(CFLAGS_DEBUG) -c -o $@ $<

$(OBJDIR)/debug/%.o: $(SRCDIR)/%.cpp
	@echo "  CXX   $< [debug]"
	@$(CXX) $(CXXFLAGS_DEBUG) -c -o $@ $<

# ── Directory creation ─────────────────────────────────────────────────
$(OBJDIR)/release:
	@mkdir -p $@

$(OBJDIR)/debug:
	@mkdir -p $@

# ── Install ────────────────────────────────────────────────────────────
INSTALL_DIR = /usr/local/bin

install: release
	@echo "  Installing nq → $(INSTALL_DIR)/$(TARGET)"
	@cp $(TARGET) $(INSTALL_DIR)/$(TARGET)
	@chmod 755 $(INSTALL_DIR)/$(TARGET)
	@echo "  ✓ Installed"

uninstall:
	@rm -f $(INSTALL_DIR)/$(TARGET)
	@echo "  ✓ Uninstalled nq"

# ── Test suite ─────────────────────────────────────────────────────────
NQ = ./$(TARGET)

test: release
	@echo ""
	@echo "  ── Nolqu $(shell ./$(TARGET) version 2>/dev/null | head -1) Test Suite ──"
	@echo ""
	@$(NQ) examples/hello.nq        > /dev/null && echo "  ✓ hello"
	@$(NQ) examples/fibonacci.nq    > /dev/null && echo "  ✓ fibonacci"
	@$(NQ) examples/functions.nq    > /dev/null && echo "  ✓ functions"
	@$(NQ) examples/counter.nq      > /dev/null && echo "  ✓ counter"
	@$(NQ) examples/arrays.nq       > /dev/null && echo "  ✓ arrays"
	@$(NQ) examples/stdlib.nq       > /dev/null && echo "  ✓ stdlib"
	@$(NQ) examples/import_demo.nq  > /dev/null && echo "  ✓ import"
	@$(NQ) examples/files.nq        > /dev/null && echo "  ✓ file I/O"
	@$(NQ) examples/tests/jit_test.nq > /dev/null && echo "  ✓ jit controls"
	@echo ""
	@echo "  ── Static analysis ──"
	@$(NQ) check examples/hello.nq       > /dev/null
	@$(NQ) check examples/fibonacci.nq   > /dev/null
	@$(NQ) check examples/functions.nq   > /dev/null
	@echo "  ✓ nq check (3 files)"
	@echo ""
	@echo "  ── All tests passed ──"
	@echo ""

# ── Clean ──────────────────────────────────────────────────────────────
clean:
	@rm -rf $(OBJDIR) $(TARGET) $(TARGET)-debug
	@echo "  ✓ Clean"

# ── Windows / Termux compatibility notes ──────────────────────────────
#
# Windows (MinGW-w64):
#   Install: https://www.mingw-w64.org/ or via MSYS2
#   Build:   mingw32-make  (same Makefile, no changes needed)
#   Note:    -fsanitize flags not available; use 'make debug' only on Linux/macOS
#             Add  CFLAGS_DEBUG=$(CFLAGS) -g3 -O0  to override on Windows
#
# Termux (Android):
#   Install: pkg install clang
#   Build:   make CC=clang CXX=clang++
#   Note:    /usr/local/bin does not exist; use:
#             make install INSTALL_DIR=$PREFIX/bin
#
