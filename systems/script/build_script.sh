#!/bin/bash

# Handmade Script Build Script
# 
# Builds the complete scripting language and VM
# No external dependencies except system libraries

set -e  # Exit on error

echo "=== Building Handmade Script ==="
echo

# Compiler flags
CC=gcc
CFLAGS="-O3 -march=native -Wall -Wextra -Wno-unused-parameter"
CFLAGS_DEBUG="-g -O0 -DDEBUG -fsanitize=address"
LDFLAGS="-lm -lpthread"

# Build mode (release by default)
BUILD_MODE=${1:-release}

if [ "$BUILD_MODE" = "debug" ]; then
    echo "Building in DEBUG mode..."
    CFLAGS=$CFLAGS_DEBUG
else
    echo "Building in RELEASE mode..."
fi

# Create build directory
mkdir -p build

# Build the scripting system as a library
echo "Compiling script parser..."
$CC $CFLAGS -c script_parser.c -o build/script_parser.o

echo "Compiling script compiler..."
$CC $CFLAGS -c script_compiler.c -o build/script_compiler.o

echo "Compiling script VM..."
$CC $CFLAGS -c script_vm.c -o build/script_vm.o

echo "Compiling script JIT..."
$CC $CFLAGS -c script_jit.c -o build/script_jit.o

echo "Compiling standard library..."
$CC $CFLAGS -c script_stdlib.c -o build/script_stdlib.o

echo "Compiling engine integration..."
$CC $CFLAGS -c script_integration.c -o build/script_integration.o

# Create static library
echo "Creating static library..."
ar rcs build/libhandmade_script.a \
    build/script_parser.o \
    build/script_compiler.o \
    build/script_vm.o \
    build/script_jit.o \
    build/script_stdlib.o \
    build/script_integration.o

# Build the demo/REPL executable
echo "Building script demo..."
$CC $CFLAGS script_demo.c build/libhandmade_script.a $LDFLAGS -o build/script_demo

# Build standalone interpreter
echo "Building standalone interpreter..."
cat > build/script_main.c << 'EOF'
#include "../handmade_script.h"
#include <stdio.h>

void script_register_stdlib(Script_VM* vm);
void script_integrate_engine(Script_VM* vm);

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <script.hs>\n", argv[0]);
        return 1;
    }
    
    Script_Config config = {
        .stack_size = 8192,
        .enable_jit = true
    };
    
    Script_VM* vm = script_vm_create(&config);
    script_integrate_engine(vm);
    
    Script_Compile_Result result = script_compile_file(vm, argv[1]);
    if (result.error_message) {
        printf("Error: %s (line %d)\n", result.error_message, result.error_line);
        return 1;
    }
    
    if (!script_run(vm, result.function)) {
        printf("Runtime error: %s\n", script_get_error(vm));
        return 1;
    }
    
    script_vm_destroy(vm);
    return 0;
}
EOF

$CC $CFLAGS build/script_main.c build/libhandmade_script.a $LDFLAGS -o build/handmade_script

# Create example scripts
echo "Creating example scripts..."

cat > build/hello.hs << 'EOF'
// Hello World in Handmade Script
print("Hello, World!")

let name = "Handmade Script"
print("Welcome to " + name + "!")

fn greet(person) {
    print("Hello, " + person + "!")
}

greet("Developer")
EOF

cat > build/game_example.hs << 'EOF'
// Game logic example

let player = {
    x: 100,
    y: 100,
    health: 100,
    speed: 5
}

let enemies = []

fn spawn_enemy(x, y) {
    let enemy = {
        x: x,
        y: y,
        health: 50,
        damage: 10
    }
    table.insert(enemies, "enemy" + table.size(enemies), enemy)
    return enemy
}

fn update_player(dt) {
    if input.is_key_pressed("W") {
        player.y = player.y - player.speed * dt
    }
    if input.is_key_pressed("S") {
        player.y = player.y + player.speed * dt
    }
    if input.is_key_pressed("A") {
        player.x = player.x - player.speed * dt
    }
    if input.is_key_pressed("D") {
        player.x = player.x + player.speed * dt
    }
}

fn game_loop(dt) {
    update_player(dt)
    
    // Update enemies
    for let i = 0; i < table.size(enemies); i = i + 1 {
        let enemy = enemies["enemy" + i]
        // Simple AI: move towards player
        let dx = player.x - enemy.x
        let dy = player.y - enemy.y
        let dist = math.sqrt(dx * dx + dy * dy)
        
        if dist > 0 {
            enemy.x = enemy.x + (dx / dist) * 2 * dt
            enemy.y = enemy.y + (dy / dist) * 2 * dt
        }
    }
}

// Spawn some enemies
spawn_enemy(200, 200)
spawn_enemy(300, 100)
spawn_enemy(150, 300)

print("Game initialized with " + table.size(enemies) + " enemies")
print("Player at (" + player.x + ", " + player.y + ")")

// Run one frame
game_loop(0.016)
EOF

cat > build/benchmark.hs << 'EOF'
// Performance benchmark script

print("Running performance benchmarks...")

// Test 1: Function calls
fn empty_function() {
    return 0
}

let start = sys.clock()
for let i = 0; i < 1000000; i = i + 1 {
    empty_function()
}
let elapsed = sys.clock() - start
print("1M function calls: " + elapsed + " seconds")
print("Calls per second: " + (1000000 / elapsed))

// Test 2: Math operations
start = sys.clock()
let sum = 0
for let i = 0; i < 1000000; i = i + 1 {
    sum = sum + math.sin(i) * math.cos(i)
}
elapsed = sys.clock() - start
print("1M math operations: " + elapsed + " seconds")

// Test 3: Table operations
start = sys.clock()
let table = {}
for let i = 0; i < 10000; i = i + 1 {
    table["key" + i] = i * 2
}
elapsed = sys.clock() - start
print("10K table insertions: " + elapsed + " seconds")

// Test 4: String operations
start = sys.clock()
let str = ""
for let i = 0; i < 1000; i = i + 1 {
    str = str + "x"
}
elapsed = sys.clock() - start
print("1K string concatenations: " + elapsed + " seconds")

print("Benchmarks complete!")
EOF

# Run tests if requested
if [ "$2" = "test" ]; then
    echo
    echo "=== Running Tests ==="
    
    echo "Testing hello world..."
    ./build/handmade_script build/hello.hs
    
    echo
    echo "Testing game example..."
    ./build/handmade_script build/game_example.hs
    
    echo
    echo "Running benchmarks..."
    ./build/script_demo --bench
fi

echo
echo "=== Build Complete ==="
echo
echo "Outputs:"
echo "  Library:     build/libhandmade_script.a"
echo "  Interpreter: build/handmade_script"
echo "  REPL/Demo:   build/script_demo"
echo
echo "Example scripts in build/"
echo
echo "Usage:"
echo "  ./build/script_demo                # Interactive REPL"
echo "  ./build/script_demo --bench        # Run benchmarks"
echo "  ./build/script_demo --demo         # Run demo scripts"
echo "  ./build/handmade_script <file.hs>  # Run script file"
echo

# Make executables executable
chmod +x build/handmade_script
chmod +x build/script_demo