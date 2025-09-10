# Handmade Engine Foundation

## What This Is

This is the **minimal foundation** for the Handmade Engine. It provides:

1. **Platform Layer Integration** - Uses the existing `handmade_platform_linux.c` 
2. **Window Creation** - Opens a window with OpenGL context
3. **Memory Arena Setup** - Initializes permanent and frame memory arenas
4. **Basic Main Loop** - Handles input, updates, and rendering
5. **Foundation for Growth** - Clean base to build systems on top of

## Files Created

- **`/home/thebackhand/Projects/handmade-engine/main.c`** - Minimal game logic with Game* functions
- **`/home/thebackhand/Projects/handmade-engine/build_minimal.sh`** - Build script for the foundation
- **`/home/thebackhand/Projects/handmade-engine/test_foundation.c`** - Component tests
- **`/home/thebackhand/Projects/handmade-engine/main_testable.c`** - Version with stubbed GL for testing

## How to Build and Run

### Build the Foundation
```bash
./build_minimal.sh
```

### Run the Foundation  
```bash
./handmade_foundation
```

**Controls:**
- ESC - Quit
- SPACE - Print debug message
- You should see an animated background color and a triangle with "HI" in pixels

### Run Tests
```bash
gcc -std=c99 -Wall -Wextra -g test_foundation.c main_testable.c -lm -o test_foundation
./test_foundation
```

## What It Does

### Visual Output
- **Animated background** - Color shifts over time using sine waves
- **Colored triangle** - Red/Green/Blue vertices 
- **Pixel text** - Spells "HI" using individual GL points

### Architecture 
- **Zero External Dependencies** - Only uses platform layer + OpenGL
- **Memory Arenas** - No malloc/free, uses arena allocators from platform
- **Game Functions** - Implements the Game* interface expected by platform layer
- **Clean Separation** - Platform handles window/input, Game* handles logic

## Next Steps

This foundation gives you a **working base** to build on:

1. **Add Systems** - Renderer, physics, audio, etc. can be added incrementally
2. **Expand Game Logic** - Replace simple triangle with actual game content
3. **Add Hot Reload** - Platform already supports it, just need to configure
4. **Integrate Existing Systems** - The foundation can load other systems as needed

## Key Achievement

✅ **Working Window + OpenGL Context + Main Loop**  
✅ **No Compilation Errors**  
✅ **No Missing Dependencies**  
✅ **Clean Platform Integration**  
✅ **Testable Components**  
✅ **Ready for Development**

The foundation **works** and gives you a solid base to build the full engine on top of, without any of the compilation issues that were blocking progress before.