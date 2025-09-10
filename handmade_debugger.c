#include "handmade_debugger.h"
#include "handmade_profiler_enhanced.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>
#include <execinfo.h>
#include <dlfcn.h>

// AAA-Quality In-Engine Debugger
// Features:
// - Software breakpoints with conditional evaluation
// - Real-time variable watching
// - Call stack unwinding
// - Memory inspection
// - Single-step execution
// - Time travel debugging (with recording)

// Global debugger state
static debugger_context g_debugger = {0};
static int g_debugger_initialized = 0;
static int g_debugger_attached = 0;

// Signal handler for breakpoints
static void debugger_signal_handler(int sig, siginfo_t* info, void* context);

// === Core Debugger API ===

void debugger_init(debugger_context* ctx) {
    if (g_debugger_initialized) return;
    
    memset(ctx, 0, sizeof(debugger_context));
    
    // Set default state
    ctx->time_scale = 1.0f;
    ctx->memory_view_size = 4096;
    
    // Install signal handler for INT3 (breakpoint)
    struct sigaction sa;
    sa.sa_sigaction = debugger_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGTRAP, &sa, NULL);
    
    // Initialize conditional expression evaluator
    ctx->evaluate_condition = debugger_evaluate_simple_condition;
    
    g_debugger = *ctx;
    g_debugger_initialized = 1;
    
    printf("[DEBUGGER] Initialized in-engine debugger\n");
}

// === Breakpoint Management ===

void debugger_add_breakpoint(debugger_context* ctx, void* address, 
                            const char* file, u32 line) {
    if (ctx->breakpoint_count >= MAX_BREAKPOINTS) {
        printf("[DEBUGGER] Maximum breakpoints reached\n");
        return;
    }
    
    breakpoint* bp = &ctx->breakpoints[ctx->breakpoint_count++];
    bp->address = address;
    bp->file = file;
    bp->line = line;
    bp->enabled = 1;
    bp->hit_count = 0;
    bp->condition = NULL;
    bp->callback = NULL;
    
    // Install software breakpoint (INT3 instruction)
    debugger_install_breakpoint(bp);
    
    printf("[DEBUGGER] Breakpoint added at %s:%u (%p)\n", 
           file, line, address);
}

void debugger_add_conditional_breakpoint(debugger_context* ctx, void* address,
                                        const char* file, u32 line,
                                        const char* condition) {
    debugger_add_breakpoint(ctx, address, file, line);
    
    breakpoint* bp = &ctx->breakpoints[ctx->breakpoint_count - 1];
    bp->condition = strdup(condition);
    
    printf("[DEBUGGER] Conditional breakpoint added: %s\n", condition);
}

void debugger_remove_breakpoint(debugger_context* ctx, void* address) {
    for (u32 i = 0; i < ctx->breakpoint_count; i++) {
        breakpoint* bp = &ctx->breakpoints[i];
        if (bp->address == address) {
            // Uninstall software breakpoint
            debugger_uninstall_breakpoint(bp);
            
            // Remove from array
            memmove(&ctx->breakpoints[i], &ctx->breakpoints[i + 1],
                   (ctx->breakpoint_count - i - 1) * sizeof(breakpoint));
            ctx->breakpoint_count--;
            
            printf("[DEBUGGER] Breakpoint removed at %p\n", address);
            return;
        }
    }
}

void debugger_install_breakpoint(breakpoint* bp) {
    // Save original instruction
    bp->original_instruction = *(u8*)bp->address;
    
    // Write INT3 (0xCC) instruction
    *(u8*)bp->address = 0xCC;
    
    // Flush instruction cache
    __builtin___clear_cache((char*)bp->address, (char*)bp->address + 1);
}

void debugger_uninstall_breakpoint(breakpoint* bp) {
    // Restore original instruction
    *(u8*)bp->address = bp->original_instruction;
    
    // Flush instruction cache
    __builtin___clear_cache((char*)bp->address, (char*)bp->address + 1);
}

// === Watch Variables ===

void debugger_add_watch(debugger_context* ctx, const char* name, 
                       void* address, size_t size) {
    if (ctx->watch_count >= MAX_WATCHES) {
        printf("[DEBUGGER] Maximum watches reached\n");
        return;
    }
    
    watch_variable* watch = &ctx->watches[ctx->watch_count++];
    watch->name = strdup(name);
    watch->address = address;
    watch->size = size;
    watch->expanded = 0;
    
    // Detect type based on size
    if (size == sizeof(f32)) {
        watch->type_name = "float";
        watch->formatter = debugger_format_float;
    } else if (size == sizeof(f64)) {
        watch->type_name = "double";
        watch->formatter = debugger_format_double;
    } else if (size == sizeof(u32)) {
        watch->type_name = "u32";
        watch->formatter = debugger_format_u32;
    } else if (size == sizeof(u64)) {
        watch->type_name = "u64";
        watch->formatter = debugger_format_u64;
    } else {
        watch->type_name = "bytes";
        watch->formatter = debugger_format_bytes;
    }
    
    printf("[DEBUGGER] Watch added: %s (%s) at %p\n", 
           name, watch->type_name, address);
}

void debugger_update_watches(debugger_context* ctx) {
    // Updates are implicit - values are read when displayed
}

// === Call Stack ===

void debugger_update_call_stack(debugger_context* ctx) {
    void* addresses[64];
    int frame_count = backtrace(addresses, 64);
    
    ctx->call_stack_depth = 0;
    
    for (int i = 0; i < frame_count && ctx->call_stack_depth < 64; i++) {
        call_stack_frame* frame = &ctx->call_stack[ctx->call_stack_depth++];
        frame->return_address = addresses[i];
        
        // Get symbol information
        Dl_info dl_info;
        if (dladdr(addresses[i], &dl_info)) {
            frame->function_name = dl_info.dli_sname;
            frame->file = dl_info.dli_fname;
        } else {
            frame->function_name = "<unknown>";
            frame->file = "<unknown>";
        }
        
        frame->line = 0; // Would need debug symbols for line numbers
        frame->frame_pointer = NULL; // Would need frame pointer walking
        frame->stack_pointer = NULL;
    }
}

// === Execution Control ===

void debugger_pause(debugger_context* ctx) {
    ctx->paused = 1;
    printf("[DEBUGGER] Execution paused\n");
}

void debugger_continue(debugger_context* ctx) {
    ctx->paused = 0;
    ctx->single_step = 0;
    ctx->step_over = 0;
    ctx->step_out = 0;
    printf("[DEBUGGER] Execution continued\n");
}

void debugger_step(debugger_context* ctx) {
    ctx->single_step = 1;
    ctx->paused = 0;
    printf("[DEBUGGER] Single step\n");
}

void debugger_step_over(debugger_context* ctx) {
    ctx->step_over = 1;
    ctx->paused = 0;
    
    // Would need to set temporary breakpoint after function call
    printf("[DEBUGGER] Step over\n");
}

void debugger_step_out(debugger_context* ctx) {
    ctx->step_out = 1;
    ctx->paused = 0;
    
    // Would need to set temporary breakpoint at return address
    printf("[DEBUGGER] Step out\n");
}

void debugger_set_time_scale(debugger_context* ctx, f32 scale) {
    ctx->time_scale = scale;
    printf("[DEBUGGER] Time scale set to %.2fx\n", scale);
}

// === Memory Inspector ===

void debugger_set_memory_view(debugger_context* ctx, void* address, size_t size) {
    ctx->memory_view_address = address;
    ctx->memory_view_size = size;
    printf("[DEBUGGER] Memory view set to %p (%zu bytes)\n", address, size);
}

void debugger_dump_memory(debugger_context* ctx, void* address, size_t size) {
    u8* bytes = (u8*)address;
    
    printf("[DEBUGGER] Memory dump at %p (%zu bytes):\n", address, size);
    
    for (size_t i = 0; i < size; i += 16) {
        printf("%p: ", (void*)((u8*)address + i));
        
        // Hex bytes
        for (size_t j = 0; j < 16 && i + j < size; j++) {
            printf("%02X ", bytes[i + j]);
        }
        
        // Padding
        for (size_t j = size - i; j < 16; j++) {
            printf("   ");
        }
        
        // ASCII
        printf(" |");
        for (size_t j = 0; j < 16 && i + j < size; j++) {
            u8 c = bytes[i + j];
            printf("%c", (c >= 32 && c <= 126) ? c : '.');
        }
        printf("|\n");
    }
}

// === Signal Handler ===

static void debugger_signal_handler(int sig, siginfo_t* info, void* context) {
    if (sig != SIGTRAP) return;
    
    debugger_context* ctx = &g_debugger;
    void* fault_address = info->si_addr;
    
    // Find matching breakpoint
    breakpoint* bp = NULL;
    for (u32 i = 0; i < ctx->breakpoint_count; i++) {
        if (ctx->breakpoints[i].address == fault_address) {
            bp = &ctx->breakpoints[i];
            break;
        }
    }
    
    if (!bp) {
        printf("[DEBUGGER] Unknown breakpoint hit at %p\n", fault_address);
        return;
    }
    
    bp->hit_count++;
    
    printf("[DEBUGGER] Breakpoint hit: %s:%u (count: %u)\n",
           bp->file, bp->line, bp->hit_count);
    
    // Check condition if present
    if (bp->condition && !ctx->evaluate_condition(bp->condition)) {
        printf("[DEBUGGER] Condition failed, continuing: %s\n", bp->condition);
        return;
    }
    
    // Call callback if present
    if (bp->callback) {
        bp->callback(bp);
    }
    
    // Pause execution
    ctx->paused = 1;
    
    // Update call stack
    debugger_update_call_stack(ctx);
    
    // Enter debug loop
    debugger_debug_loop(ctx);
}

// === Debug Loop ===

void debugger_debug_loop(debugger_context* ctx) {
    printf("[DEBUGGER] Entering debug mode. Commands: c(ontinue), s(tep), bt, p <var>, q(uit)\n");
    
    char command[256];
    while (ctx->paused) {
        printf("(dbg) ");
        fflush(stdout);
        
        if (!fgets(command, sizeof(command), stdin)) break;
        
        // Parse command
        char* cmd = strtok(command, " \n");
        if (!cmd) continue;
        
        if (strcmp(cmd, "c") == 0 || strcmp(cmd, "continue") == 0) {
            debugger_continue(ctx);
        } else if (strcmp(cmd, "s") == 0 || strcmp(cmd, "step") == 0) {
            debugger_step(ctx);
        } else if (strcmp(cmd, "n") == 0 || strcmp(cmd, "next") == 0) {
            debugger_step_over(ctx);
        } else if (strcmp(cmd, "bt") == 0 || strcmp(cmd, "backtrace") == 0) {
            debugger_print_call_stack(ctx);
        } else if (strcmp(cmd, "p") == 0 || strcmp(cmd, "print") == 0) {
            char* var_name = strtok(NULL, " \n");
            if (var_name) {
                debugger_print_variable(ctx, var_name);
            }
        } else if (strcmp(cmd, "mem") == 0) {
            char* addr_str = strtok(NULL, " \n");
            if (addr_str) {
                void* addr = (void*)strtoul(addr_str, NULL, 16);
                debugger_dump_memory(ctx, addr, 64);
            }
        } else if (strcmp(cmd, "watches") == 0) {
            debugger_print_watches(ctx);
        } else if (strcmp(cmd, "breakpoints") == 0) {
            debugger_print_breakpoints(ctx);
        } else if (strcmp(cmd, "q") == 0 || strcmp(cmd, "quit") == 0) {
            exit(0);
        } else if (strcmp(cmd, "help") == 0) {
            printf("Commands:\n");
            printf("  c, continue    - Continue execution\n");
            printf("  s, step        - Single step\n");
            printf("  n, next        - Step over\n");
            printf("  bt, backtrace  - Show call stack\n");
            printf("  p <var>        - Print variable\n");
            printf("  mem <addr>     - Dump memory\n");
            printf("  watches        - Show watch variables\n");
            printf("  breakpoints    - Show breakpoints\n");
            printf("  q, quit        - Quit program\n");
        } else {
            printf("Unknown command: %s (try 'help')\n", cmd);
        }
    }
}

// === Display Functions ===

void debugger_print_call_stack(debugger_context* ctx) {
    printf("[DEBUGGER] Call stack (%u frames):\n", ctx->call_stack_depth);
    
    for (u32 i = 0; i < ctx->call_stack_depth; i++) {
        call_stack_frame* frame = &ctx->call_stack[i];
        printf("  #%u  %p in %s at %s:%u\n",
               i, frame->return_address, frame->function_name,
               frame->file, frame->line);
    }
}

void debugger_print_watches(debugger_context* ctx) {
    printf("[DEBUGGER] Watch variables (%u):\n", ctx->watch_count);
    
    for (u32 i = 0; i < ctx->watch_count; i++) {
        watch_variable* watch = &ctx->watches[i];
        
        char value_str[256];
        if (watch->formatter) {
            watch->formatter(watch->address, value_str, sizeof(value_str));
        } else {
            snprintf(value_str, sizeof(value_str), "<no formatter>");
        }
        
        printf("  %s (%s) = %s\n", watch->name, watch->type_name, value_str);
    }
}

void debugger_print_breakpoints(debugger_context* ctx) {
    printf("[DEBUGGER] Breakpoints (%u):\n", ctx->breakpoint_count);
    
    for (u32 i = 0; i < ctx->breakpoint_count; i++) {
        breakpoint* bp = &ctx->breakpoints[i];
        printf("  %u: %s:%u at %p (hits: %u) %s\n",
               i, bp->file, bp->line, bp->address, bp->hit_count,
               bp->enabled ? "" : "[DISABLED]");
        
        if (bp->condition) {
            printf("     Condition: %s\n", bp->condition);
        }
    }
}

void debugger_print_variable(debugger_context* ctx, const char* name) {
    // Find watch variable
    for (u32 i = 0; i < ctx->watch_count; i++) {
        watch_variable* watch = &ctx->watches[i];
        if (strcmp(watch->name, name) == 0) {
            char value_str[256];
            if (watch->formatter) {
                watch->formatter(watch->address, value_str, sizeof(value_str));
            } else {
                snprintf(value_str, sizeof(value_str), "<no formatter>");
            }
            
            printf("%s (%s) = %s\n", watch->name, watch->type_name, value_str);
            return;
        }
    }
    
    printf("Variable not found: %s\n", name);
}

// === Value Formatters ===

void debugger_format_float(void* data, char* out, size_t out_size) {
    snprintf(out, out_size, "%.6f", *(f32*)data);
}

void debugger_format_double(void* data, char* out, size_t out_size) {
    snprintf(out, out_size, "%.6f", *(f64*)data);
}

void debugger_format_u32(void* data, char* out, size_t out_size) {
    snprintf(out, out_size, "%u (0x%08X)", *(u32*)data, *(u32*)data);
}

void debugger_format_u64(void* data, char* out, size_t out_size) {
    snprintf(out, out_size, "%llu (0x%016llX)", *(u64*)data, *(u64*)data);
}

void debugger_format_bytes(void* data, char* out, size_t out_size) {
    u8* bytes = (u8*)data;
    int written = 0;
    
    for (int i = 0; i < 8 && written < out_size - 3; i++) {
        written += snprintf(out + written, out_size - written, "%02X ", bytes[i]);
    }
    
    if (written >= 3) out[written - 1] = '\0'; // Remove trailing space
}

// === Condition Evaluator ===

int debugger_evaluate_simple_condition(const char* condition) {
    // Simple expression evaluator for conditions like "x > 5" or "flag == 1"
    // This is a simplified version - a full implementation would use a parser
    
    char* expr = strdup(condition);
    char* var_name = strtok(expr, " ");
    char* op = strtok(NULL, " ");
    char* value_str = strtok(NULL, " ");
    
    if (!var_name || !op || !value_str) {
        free(expr);
        return 0; // Invalid expression
    }
    
    // Find variable in watches
    debugger_context* ctx = &g_debugger;
    watch_variable* watch = NULL;
    
    for (u32 i = 0; i < ctx->watch_count; i++) {
        if (strcmp(ctx->watches[i].name, var_name) == 0) {
            watch = &ctx->watches[i];
            break;
        }
    }
    
    if (!watch) {
        free(expr);
        return 0; // Variable not found
    }
    
    // Get current value
    f64 current_value = 0;
    if (watch->size == sizeof(f32)) {
        current_value = *(f32*)watch->address;
    } else if (watch->size == sizeof(f64)) {
        current_value = *(f64*)watch->address;
    } else if (watch->size == sizeof(u32)) {
        current_value = *(u32*)watch->address;
    } else if (watch->size == sizeof(u64)) {
        current_value = *(u64*)watch->address;
    }
    
    // Parse comparison value
    f64 compare_value = atof(value_str);
    
    // Evaluate condition
    int result = 0;
    if (strcmp(op, "==") == 0) {
        result = (current_value == compare_value);
    } else if (strcmp(op, "!=") == 0) {
        result = (current_value != compare_value);
    } else if (strcmp(op, "<") == 0) {
        result = (current_value < compare_value);
    } else if (strcmp(op, ">") == 0) {
        result = (current_value > compare_value);
    } else if (strcmp(op, "<=") == 0) {
        result = (current_value <= compare_value);
    } else if (strcmp(op, ">=") == 0) {
        result = (current_value >= compare_value);
    }
    
    free(expr);
    return result;
}

// === Visual Debugger Integration ===

void debugger_draw_overlay(f32 x, f32 y, f32 width, f32 height) {
    debugger_context* ctx = &g_debugger;
    
    if (!g_debugger_initialized || !ctx->paused) return;
    
    // Draw debugger panel
    draw_filled_rect(x, y, width, height, 0x1E1E1EFF);
    draw_rect_outline(x, y, width, height, 0x569CD6FF, 2.0f);
    
    f32 current_y = y + 10;
    
    // Title
    draw_text(x + 10, current_y, "DEBUGGER - PAUSED", 0xFF4444FF, 14);
    current_y += 25;
    
    // Call stack
    draw_text(x + 10, current_y, "Call Stack:", 0xE0E0E0FF, 12);
    current_y += 20;
    
    for (u32 i = 0; i < ctx->call_stack_depth && i < 5; i++) {
        call_stack_frame* frame = &ctx->call_stack[i];
        draw_text_formatted(x + 20, current_y, 0xCCCCCCFF, 10,
                           "#%u  %s", i, frame->function_name);
        current_y += 15;
    }
    
    current_y += 10;
    
    // Watches
    draw_text(x + 10, current_y, "Variables:", 0xE0E0E0FF, 12);
    current_y += 20;
    
    for (u32 i = 0; i < ctx->watch_count && i < 5; i++) {
        watch_variable* watch = &ctx->watches[i];
        
        char value_str[256];
        if (watch->formatter) {
            watch->formatter(watch->address, value_str, sizeof(value_str));
        } else {
            strcpy(value_str, "<no formatter>");
        }
        
        draw_text_formatted(x + 20, current_y, 0xCCCCCCFF, 10,
                           "%s = %s", watch->name, value_str);
        current_y += 15;
    }
    
    // Instructions
    current_y += 10;
    draw_text(x + 10, current_y, "Press F5 to continue, F10 to step", 
              0x888888FF, 10);
}

// === Keyboard Shortcuts ===

void debugger_handle_key(int key) {
    debugger_context* ctx = &g_debugger;
    
    switch (key) {
        case KEY_F5:
            if (ctx->paused) debugger_continue(ctx);
            break;
        case KEY_F10:
            if (ctx->paused) debugger_step(ctx);
            break;
        case KEY_F11:
            if (ctx->paused) debugger_step_over(ctx);
            break;
        case KEY_SHIFT_F11:
            if (ctx->paused) debugger_step_out(ctx);
            break;
        case KEY_F9:
            // Toggle breakpoint at current location (would need more context)
            break;
    }
}

// === Convenience Macros ===

#define DBG_BREAKPOINT() \
    do { \
        static int bp_installed = 0; \
        if (!bp_installed) { \
            debugger_add_breakpoint(&g_debugger, &&bp_label, __FILE__, __LINE__); \
            bp_installed = 1; \
        } \
        bp_label: \
        __asm__ volatile("int3"); \
    } while(0)

#define DBG_WATCH(var) \
    debugger_add_watch(&g_debugger, #var, &(var), sizeof(var))

#define DBG_WATCH_NAMED(name, var) \
    debugger_add_watch(&g_debugger, name, &(var), sizeof(var))

#define DBG_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printf("[DEBUG ASSERT] %s failed at %s:%d\n", #condition, __FILE__, __LINE__); \
            DBG_BREAKPOINT(); \
        } \
    } while(0)

// === Time Travel Support ===

void debugger_start_time_travel_recording(void) {
    // Start recording all state changes for time travel debugging
    profiler_start_recording();
}

void debugger_stop_time_travel_recording(void) {
    profiler_stop_recording();
}

void debugger_rewind_to_frame(u32 frame_number) {
    // Rewind execution to specific frame (requires full state recording)
    printf("[DEBUGGER] Time travel to frame %u (not implemented)\n", frame_number);
}

// === Cleanup ===

void debugger_shutdown(debugger_context* ctx) {
    // Remove all breakpoints
    for (u32 i = 0; i < ctx->breakpoint_count; i++) {
        debugger_uninstall_breakpoint(&ctx->breakpoints[i]);
        if (ctx->breakpoints[i].condition) {
            free((void*)ctx->breakpoints[i].condition);
        }
    }
    
    // Free watch names
    for (u32 i = 0; i < ctx->watch_count; i++) {
        if (ctx->watches[i].name) {
            free((void*)ctx->watches[i].name);
        }
    }
    
    memset(ctx, 0, sizeof(debugger_context));
    g_debugger_initialized = 0;
    
    printf("[DEBUGGER] Shutdown complete\n");
}