# Handmade Editor Makefile

.PHONY: all debug release clean run watch hot-reload install-deps

all: debug

debug:
	@./build.sh debug

release:
	@./build.sh release

profile:
	@./build.sh profile

clean:
	@rm -rf build/*
	@echo "✓ Build directory cleaned"

run: debug
	@./build/editor

# Install missing dependencies (optional - for watch mode)
install-deps:
	@echo "Installing optional dependencies for watch mode..."
	@echo "Run: sudo apt install inotify-tools"
	@echo "This enables automatic rebuilds when files change."

# Watch mode - rebuild on file changes
# Note: Requires inotify-tools package
watch:
	@if command -v inotifywait >/dev/null 2>&1; then \
		./build.sh debug no watch; \
	else \
		echo "Error: inotifywait not found."; \
		echo "Watch mode requires inotify-tools package."; \
		echo "Install with: sudo apt install inotify-tools"; \
		echo ""; \
		echo "Alternatively, you can manually rebuild with:"; \
		echo "  make debug"; \
		exit 1; \
	fi

# Hot reload build
hot-reload:
	@./build.sh debug yes
	@echo "✓ Hot reload build complete"
	@echo "Run the editor with hot reload support"

# Hot reload with watch
hot-watch:
	@if command -v inotifywait >/dev/null 2>&1; then \
		./build.sh debug yes watch; \
	else \
		echo "Error: inotifywait not found."; \
		echo "Watch mode requires inotify-tools package."; \
		echo "Install with: sudo apt install inotify-tools"; \
		exit 1; \
	fi

help:
	@echo "Handmade Editor Build System"
	@echo ""
	@echo "Usage:"
	@echo "  make         - Build debug version"
	@echo "  make debug   - Build debug version"
	@echo "  make release - Build release version"
	@echo "  make profile - Build with profiling"
	@echo "  make run     - Build and run"
	@echo "  make clean   - Clean build directory"
	@echo ""
	@echo "Advanced:"
	@echo "  make watch      - Auto-rebuild on file changes (requires inotify-tools)"
	@echo "  make hot-reload - Build with hot reload support"
	@echo "  make hot-watch  - Hot reload with auto-rebuild"
	@echo ""
	@echo "To install optional dependencies:"
	@echo "  make install-deps"
	@echo ""
	@echo "Build times: <1 second for full rebuild!"