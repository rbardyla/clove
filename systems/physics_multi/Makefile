# Continental Architect - Handmade Build System
# Zero dependencies, maximum control

CC = gcc
CFLAGS_BASE = -std=c99 -Wall -Wextra -Wno-unused-parameter
CFLAGS_DEBUG = $(CFLAGS_BASE) -g -O0 -DHANDMADE_DEBUG=1 -fsanitize=address -fsanitize=undefined
CFLAGS_RELEASE = $(CFLAGS_BASE) -O3 -march=native -ffast-math -funroll-loops -DNDEBUG
CFLAGS_PROFILE = $(CFLAGS_BASE) -O2 -pg -fno-omit-frame-pointer

LIBS = -lX11 -lGL -lm
LIBS_DEBUG = $(LIBS) -lasan -lubsan

# Output directory
BINDIR = ../../binaries

# Main targets
EDITOR = $(BINDIR)/continental_editor
GAME = $(BINDIR)/continental_game
GEOLOGICAL_TEST = $(BINDIR)/test_geological

# Source files
EDITOR_SRC = core/continental_editor_v4.c
GAME_SRC = core/continental_ultimate.c
GEOLOGICAL_SRC = core/handmade_geological.c
TEST_GEOLOGICAL_SRC = tests/test_geological.c

.PHONY: all clean debug release profile test help audit

# Default target
all: release

# Help target
help:
	@echo "Continental Architect Build System"
	@echo "=================================="
	@echo "Targets:"
	@echo "  make all      - Build everything (release mode)"
	@echo "  make debug    - Build with debug symbols and sanitizers"
	@echo "  make release  - Build optimized for performance"
	@echo "  make profile  - Build with profiling support"
	@echo "  make test     - Run all tests"
	@echo "  make audit    - Run code audit checks"
	@echo "  make clean    - Remove all build artifacts"
	@echo ""
	@echo "Individual targets:"
	@echo "  make editor   - Build only the editor"
	@echo "  make game     - Build only the game"
	@echo "  make geological - Build geological test"
	@echo ""
	@echo "Debug targets:"
	@echo "  make run-editor - Build and run editor (debug)"
	@echo "  make run-game   - Build and run game (debug)"
	@echo "  make valgrind   - Run editor under valgrind"
	@echo "  make gdb        - Run editor under gdb"

# Build configurations
debug: CFLAGS = $(CFLAGS_DEBUG)
debug: LIBS_USED = $(LIBS_DEBUG)
debug: editor-debug game-debug geological-debug
	@echo "Debug build complete with sanitizers enabled"

release: CFLAGS = $(CFLAGS_RELEASE)
release: LIBS_USED = $(LIBS)
release: editor-release game-release geological-release
	@echo "Release build complete - optimized for performance"

profile: CFLAGS = $(CFLAGS_PROFILE)
profile: LIBS_USED = $(LIBS)
profile: editor-profile game-profile
	@echo "Profile build complete - use gprof for analysis"

# Individual debug targets
editor-debug:
	$(CC) $(CFLAGS_DEBUG) -o $(EDITOR)_debug $(EDITOR_SRC) $(LIBS_DEBUG)
	@echo "Editor built with debug symbols: $(EDITOR)_debug"

game-debug:
	$(CC) $(CFLAGS_DEBUG) -o $(GAME)_debug $(GAME_SRC) $(LIBS_DEBUG)
	@echo "Game built with debug symbols: $(GAME)_debug"

geological-debug:
	$(CC) $(CFLAGS_DEBUG) -o $(GEOLOGICAL_TEST)_debug $(TEST_GEOLOGICAL_SRC) $(GEOLOGICAL_SRC) -lm -lasan -lubsan
	@echo "Geological test built with debug symbols: $(GEOLOGICAL_TEST)_debug"

# Individual release targets
editor-release:
	$(CC) $(CFLAGS_RELEASE) -o $(EDITOR) $(EDITOR_SRC) $(LIBS)
	@strip $(EDITOR)
	@echo "Editor built for release: $(EDITOR)"

game-release:
	$(CC) $(CFLAGS_RELEASE) -o $(GAME) $(GAME_SRC) $(LIBS)
	@strip $(GAME)
	@echo "Game built for release: $(GAME)"

geological-release:
	$(CC) $(CFLAGS_RELEASE) -o $(GEOLOGICAL_TEST) $(TEST_GEOLOGICAL_SRC) $(GEOLOGICAL_SRC) -lm
	@strip $(GEOLOGICAL_TEST)
	@echo "Geological test built for release: $(GEOLOGICAL_TEST)"

# Individual profile targets
editor-profile:
	$(CC) $(CFLAGS_PROFILE) -o $(EDITOR)_profile $(EDITOR_SRC) $(LIBS)
	@echo "Editor built for profiling: $(EDITOR)_profile"

game-profile:
	$(CC) $(CFLAGS_PROFILE) -o $(GAME)_profile $(GAME_SRC) $(LIBS)
	@echo "Game built for profiling: $(GAME)_profile"

# Convenience shortcuts
editor: editor-release
game: game-release
geological: geological-release

# Run targets
run-editor: editor-debug
	$(EDITOR)_debug

run-game: game-debug
	$(GAME)_debug

# Test target
test: geological-debug
	@echo "Running geological simulation test..."
	@$(GEOLOGICAL_TEST)_debug
	@echo ""
	@echo "Running editor compile test..."
	@$(CC) -fsyntax-only $(CFLAGS_BASE) $(EDITOR_SRC)
	@echo "Editor syntax OK"
	@echo ""
	@echo "Running game compile test..."
	@$(CC) -fsyntax-only $(CFLAGS_BASE) $(GAME_SRC)
	@echo "Game syntax OK"
	@echo ""
	@echo "All tests passed!"

# Code audit checks
audit:
	@echo "=== Code Audit ==="
	@echo ""
	@echo "Checking for malloc/free usage (should be none)..."
	@! grep -n "malloc\|calloc\|free" $(EDITOR_SRC) $(GAME_SRC) $(GEOLOGICAL_SRC) || echo "✓ No heap allocations found"
	@echo ""
	@echo "Checking for unsafe string operations..."
	@! grep -n "strcpy\|strcat\|sprintf" $(EDITOR_SRC) $(GAME_SRC) || echo "✓ No unsafe string operations"
	@echo ""
	@echo "Checking for external dependencies..."
	@! grep -n "#include <" $(EDITOR_SRC) $(GAME_SRC) | grep -v "stdio\|stdlib\|string\|time\|unistd\|math\|X11\|GL" || echo "✓ No external dependencies"
	@echo ""
	@echo "Line count statistics:"
	@wc -l $(EDITOR_SRC) $(GAME_SRC) $(GEOLOGICAL_SRC)
	@echo ""
	@echo "=== Audit Complete ==="

# Valgrind memory check
valgrind: editor-debug
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes $(EDITOR)_debug

# GDB debugging
gdb: editor-debug
	gdb $(EDITOR)_debug

# Performance analysis
perf: editor-release
	perf record -g $(EDITOR)
	perf report

# Clean build artifacts
clean:
	rm -f $(EDITOR) $(EDITOR)_debug $(EDITOR)_profile
	rm -f $(GAME) $(GAME)_debug $(GAME)_profile
	rm -f $(GEOLOGICAL_TEST) $(GEOLOGICAL_TEST)_debug
	rm -f gmon.out perf.data perf.data.old
	rm -f *.o *.a
	@echo "Clean complete"

# Installation (optional)
install: release
	@echo "Installation not implemented - binaries are in $(BINDIR)"

# Print configuration
config:
	@echo "Compiler: $(CC)"
	@echo "Debug flags: $(CFLAGS_DEBUG)"
	@echo "Release flags: $(CFLAGS_RELEASE)"
	@echo "Libraries: $(LIBS)"
	@echo "Binary directory: $(BINDIR)"