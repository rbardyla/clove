/*
 * Handmade Script Demo - Interactive REPL and benchmarks
 * 
 * Tests:
 * - Function calls: Target 1M/second
 * - GC pause: Target <1ms
 * - JIT speedup: Target 10x
 * - Hot-reload: Target <10ms
 */

#include "handmade_script.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

// External functions
void script_integrate_engine(Script_VM* vm);
void script_hotreload_update(Script_VM* vm, const char* filename);

// Performance timing
static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

// Sample scripts for testing
static const char* fibonacci_script = 
    "fn fib(n) {\n"
    "    if n <= 1 {\n"
    "        return n\n"
    "    }\n"
    "    return fib(n - 1) + fib(n - 2)\n"
    "}\n"
    "\n"
    "let result = fib(20)\n"
    "print(\"Fibonacci(20) = \" + result)\n";

static const char* game_logic_script = 
    "// Enemy AI behavior\n"
    "let enemies = {}\n"
    "\n"
    "fn spawn_enemy(x, y) {\n"
    "    let enemy = {\n"
    "        x: x,\n"
    "        y: y,\n"
    "        health: 100,\n"
    "        speed: 5,\n"
    "        state: \"patrol\"\n"
    "    }\n"
    "    return enemy\n"
    "}\n"
    "\n"
    "fn update_enemy(enemy, player, dt) {\n"
    "    // Calculate distance to player\n"
    "    let dx = player.x - enemy.x\n"
    "    let dy = player.y - enemy.y\n"
    "    let distance = math.sqrt(dx * dx + dy * dy)\n"
    "    \n"
    "    if distance < 100 {\n"
    "        enemy.state = \"chase\"\n"
    "        // Move towards player\n"
    "        enemy.x = enemy.x + (dx / distance) * enemy.speed * dt\n"
    "        enemy.y = enemy.y + (dy / distance) * enemy.speed * dt\n"
    "    } else {\n"
    "        enemy.state = \"patrol\"\n"
    "        // Random patrol movement\n"
    "        enemy.x = enemy.x + math.random(-1, 1) * enemy.speed * dt\n"
    "        enemy.y = enemy.y + math.random(-1, 1) * enemy.speed * dt\n"
    "    }\n"
    "}\n"
    "\n"
    "// Test enemy AI\n"
    "let enemy = spawn_enemy(100, 100)\n"
    "let player = { x: 150, y: 150 }\n"
    "update_enemy(enemy, player, 0.016)\n"
    "print(\"Enemy state: \" + enemy.state)\n";

static const char* benchmark_script = 
    "// Performance benchmark\n"
    "fn benchmark_math() {\n"
    "    let sum = 0\n"
    "    for let i = 0; i < 1000000; i = i + 1 {\n"
    "        sum = sum + math.sin(i) * math.cos(i)\n"
    "    }\n"
    "    return sum\n"
    "}\n"
    "\n"
    "fn benchmark_function_calls() {\n"
    "    fn add(a, b) { return a + b }\n"
    "    let sum = 0\n"
    "    for let i = 0; i < 1000000; i = i + 1 {\n"
    "        sum = add(sum, 1)\n"
    "    }\n"
    "    return sum\n"
    "}\n"
    "\n"
    "fn benchmark_table_ops() {\n"
    "    let t = {}\n"
    "    for let i = 0; i < 10000; i = i + 1 {\n"
    "        t[\"key\" + i] = i * 2\n"
    "    }\n"
    "    return table.size(t)\n"
    "}\n";

// Run benchmarks
static void run_benchmarks(Script_VM* vm) {
    printf("\n=== Performance Benchmarks ===\n\n");
    
    // Test 1: Function call performance
    printf("1. Function Call Performance:\n");
    const char* call_test = 
        "fn test() { return 42 }\n"
        "let start = sys.clock()\n"
        "for let i = 0; i < 1000000; i = i + 1 { test() }\n"
        "let elapsed = sys.clock() - start\n"
        "print(\"1M function calls: \" + elapsed + \" seconds\")\n"
        "print(\"Calls/second: \" + (1000000 / elapsed))\n";
    
    Script_Value result;
    script_eval(vm, call_test, &result);
    
    // Test 2: Math operations (JIT speedup test)
    printf("\n2. Math Operations (JIT speedup):\n");
    
    // First run without JIT
    script_jit_enable(vm, false);
    double start = get_time_ms();
    script_eval(vm, 
        "let sum = 0\n"
        "for let i = 0; i < 100000; i = i + 1 {\n"
        "    sum = sum + i * 2.5 - i / 3.7\n"
        "}\n", &result);
    double no_jit_time = get_time_ms() - start;
    printf("Without JIT: %.2f ms\n", no_jit_time);
    
    // Second run with JIT
    script_jit_enable(vm, true);
    script_vm_reset(vm);
    start = get_time_ms();
    script_eval(vm, 
        "let sum = 0\n"
        "for let i = 0; i < 100000; i = i + 1 {\n"
        "    sum = sum + i * 2.5 - i / 3.7\n"
        "}\n", &result);
    double jit_time = get_time_ms() - start;
    printf("With JIT: %.2f ms\n", jit_time);
    printf("JIT Speedup: %.1fx\n", no_jit_time / jit_time);
    
    // Test 3: GC pause time
    printf("\n3. Garbage Collection:\n");
    const char* gc_test = 
        "// Create lots of temporary objects\n"
        "for let i = 0; i < 10000; i = i + 1 {\n"
        "    let obj = { x: i, y: i * 2, data: \"test\" + i }\n"
        "}\n"
        "sys.gc()\n"
        "let stats = sys.memory()\n"
        "print(\"GC runs: \" + stats.gc_runs)\n"
        "print(\"Live objects: \" + stats.live_objects)\n"
        "print(\"Allocated: \" + stats.allocated + \" bytes\")\n";
    
    script_eval(vm, gc_test, &result);
    
    Script_GC_Stats stats = script_gc_stats(vm);
    printf("Average GC pause: %.2f ms\n", 
           stats.gc_runs > 0 ? (double)stats.gc_time_ms / stats.gc_runs : 0);
    
    // Test 4: Memory overhead
    printf("\n4. Memory Overhead:\n");
    size_t object_size = sizeof(Script_Value) + sizeof(Script_Table) + 
                        sizeof(Script_Table_Entry) * 4;
    printf("Object overhead: %zu bytes\n", object_size);
    printf("Target: <100 bytes [%s]\n", object_size < 100 ? "PASS" : "FAIL");
    
    // Test 5: Hot-reload time
    printf("\n5. Hot-reload Performance:\n");
    
    // Write test script to file
    FILE* f = fopen("test_reload.script", "w");
    fprintf(f, "let test_var = 42\n");
    fclose(f);
    
    start = get_time_ms();
    script_hotreload_update(vm, "test_reload.script");
    double reload_time = get_time_ms() - start;
    
    printf("Hot-reload time: %.2f ms\n", reload_time);
    printf("Target: <10ms [%s]\n", reload_time < 10 ? "PASS" : "FAIL");
    
    unlink("test_reload.script");
    
    printf("\n=== Benchmark Complete ===\n\n");
}

// REPL implementation
static void run_repl(Script_VM* vm) {
    printf("Handmade Script REPL v1.0\n");
    printf("Type 'help' for commands, 'quit' to exit\n\n");
    
    char line[1024];
    char multiline[8192] = {0};
    bool in_multiline = false;
    
    while (1) {
        if (in_multiline) {
            printf("... ");
        } else {
            printf("> ");
        }
        
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }
        
        // Remove newline
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') {
            line[len-1] = '\0';
            len--;
        }
        
        // Check for commands
        if (!in_multiline) {
            if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) {
                break;
            }
            
            if (strcmp(line, "help") == 0) {
                printf("Commands:\n");
                printf("  help      - Show this help\n");
                printf("  quit      - Exit REPL\n");
                printf("  clear     - Clear screen\n");
                printf("  bench     - Run benchmarks\n");
                printf("  demo      - Run demo scripts\n");
                printf("  stack     - Show stack\n");
                printf("  gc        - Run garbage collection\n");
                printf("  prof      - Show profiling data\n");
                printf("  {{        - Start multiline input\n");
                printf("  }}        - End multiline input\n");
                printf("\nExamples:\n");
                printf("  let x = 42\n");
                printf("  fn add(a, b) { return a + b }\n");
                printf("  print(add(10, 20))\n");
                printf("  let player = { x: 100, y: 200, health: 100 }\n");
                continue;
            }
            
            if (strcmp(line, "clear") == 0) {
                system("clear");
                continue;
            }
            
            if (strcmp(line, "bench") == 0) {
                run_benchmarks(vm);
                continue;
            }
            
            if (strcmp(line, "demo") == 0) {
                printf("\nRunning Fibonacci demo...\n");
                script_eval(vm, fibonacci_script, NULL);
                
                printf("\nRunning Game Logic demo...\n");
                script_eval(vm, game_logic_script, NULL);
                continue;
            }
            
            if (strcmp(line, "stack") == 0) {
                script_print_stack(vm);
                continue;
            }
            
            if (strcmp(line, "gc") == 0) {
                script_gc_run(vm);
                Script_GC_Stats stats = script_gc_stats(vm);
                printf("GC complete. Freed %llu bytes\n", 
                       stats.bytes_freed);
                continue;
            }
            
            if (strcmp(line, "prof") == 0) {
                if (vm->config.enable_profiling) {
                    printf("\nInstruction Profile:\n");
                    for (int i = 0; i < OP_COUNT; i++) {
                        uint64_t count = script_get_instruction_count(vm, i);
                        if (count > 0) {
                            uint64_t cycles = script_get_instruction_cycles(vm, i);
                            printf("  Op %d: %llu calls, %llu cycles (avg %.1f)\n",
                                   i, count, cycles, (double)cycles / count);
                        }
                    }
                } else {
                    printf("Profiling disabled. Enable with config.enable_profiling\n");
                }
                continue;
            }
            
            if (strcmp(line, "{") == 0) {
                in_multiline = true;
                multiline[0] = '\0';
                continue;
            }
        }
        
        // Handle multiline input
        if (in_multiline) {
            if (strcmp(line, "}") == 0) {
                in_multiline = false;
                // Execute multiline code
                Script_Value result;
                if (script_eval(vm, multiline, &result)) {
                    if (!script_is_nil(result)) {
                        printf("= ");
                        script_print_value(result);
                        printf("\n");
                    }
                } else {
                    printf("Error: %s\n", script_get_error(vm));
                }
                continue;
            } else {
                strcat(multiline, line);
                strcat(multiline, "\n");
                continue;
            }
        }
        
        // Execute single line
        Script_Value result;
        if (script_eval(vm, line, &result)) {
            if (!script_is_nil(result)) {
                printf("= ");
                script_print_value(result);
                printf("\n");
            }
        } else {
            printf("Error: %s\n", script_get_error(vm));
        }
    }
    
    printf("\nGoodbye!\n");
}

int main(int argc, char** argv) {
    // Configure VM
    Script_Config config = {
        .stack_size = 8192,
        .frame_stack_size = 256,
        .gc_threshold = 1024 * 1024,
        .jit_threshold = 100,
        .enable_jit = true,
        .enable_debug = false,
        .enable_profiling = true
    };
    
    // Create VM
    Script_VM* vm = script_vm_create(&config);
    
    // Integrate engine bindings
    script_integrate_engine(vm);
    
    // Check command line arguments
    if (argc > 1) {
        if (strcmp(argv[1], "--bench") == 0) {
            run_benchmarks(vm);
        } else if (strcmp(argv[1], "--demo") == 0) {
            printf("Running demo scripts...\n\n");
            script_eval(vm, fibonacci_script, NULL);
            printf("\n");
            script_eval(vm, game_logic_script, NULL);
        } else {
            // Execute file
            Script_Compile_Result result = script_compile_file(vm, argv[1]);
            if (result.error_message) {
                printf("Compile error: %s (line %d)\n", 
                       result.error_message, result.error_line);
            } else {
                if (!script_run(vm, result.function)) {
                    printf("Runtime error: %s\n", script_get_error(vm));
                }
            }
            script_free_compile_result(vm, &result);
        }
    } else {
        // Interactive REPL
        run_repl(vm);
    }
    
    // Clean up
    script_vm_destroy(vm);
    
    return 0;
}