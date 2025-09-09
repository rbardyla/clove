# Quick rebuild for hot reload testing
.PHONY: all game platform clean watch

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -march=native -O3 -ffast-math
GAME_FLAGS = -fPIC -shared -fvisibility=hidden
PLATFORM_LIBS = -lX11 -lGL -ldl -lm -lpthread

all: platform game.so

platform: platform_main.c handmade_hotreload.o
	$(CC) $(CFLAGS) $^ -o $@ $(PLATFORM_LIBS)

handmade_hotreload.o: handmade_hotreload.c handmade_hotreload.h
	$(CC) $(CFLAGS) -c $< -o $@

game.so: game_main.c handmade_hotreload.h
	@echo "[HOT RELOAD] Rebuilding game..."
	@$(CC) $(CFLAGS) $(GAME_FLAGS) $< -o $@ -lm
	@echo "[HOT RELOAD] Game rebuilt - will auto-reload!"

# Quick game rebuild (most common during development)
game: game.so

# Watch for changes and auto-rebuild
watch:
	@echo "Watching for changes... (Ctrl+C to stop)"
	@while true; do \
		inotifywait -q -e modify game_main.c; \
		make -s game; \
	done

clean:
	rm -f platform game.so handmade_hotreload.o

run: all
	./platform

# Performance build with profiling
profile: CFLAGS += -g -pg
profile: all

# Debug build with symbols
debug: CFLAGS = -Wall -Wextra -std=c99 -g -O0 -DDEBUG
debug: all
