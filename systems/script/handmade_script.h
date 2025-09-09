/*
 * Handmade Script - Complete scripting language and VM
 * 
 * Stack-based virtual machine with JIT compilation
 * Zero external dependencies, all handmade
 * 
 * PERFORMANCE TARGETS:
 * - 1M function calls/second
 * - <100 bytes per object overhead
 * - JIT speedup: 10x for numeric code
 * - GC pause: <1ms
 * - Script reload: <10ms
 */

#ifndef HANDMADE_SCRIPT_H
#define HANDMADE_SCRIPT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Forward declarations
typedef struct Script_VM Script_VM;
typedef struct Script_Value Script_Value;
typedef struct Script_Function Script_Function;
typedef struct Script_Table Script_Table;
typedef struct Script_String Script_String;
typedef struct Script_Coroutine Script_Coroutine;

// Native function signature
typedef Script_Value (*Script_Native_Fn)(Script_VM* vm, int argc, Script_Value* argv);

// Value types - tagged union for dynamic typing
typedef enum {
    SCRIPT_NIL,
    SCRIPT_BOOLEAN,
    SCRIPT_NUMBER,
    SCRIPT_STRING,
    SCRIPT_FUNCTION,
    SCRIPT_NATIVE,
    SCRIPT_TABLE,
    SCRIPT_COROUTINE,
    SCRIPT_USERDATA
} Script_Value_Type;

// Value structure - 16 bytes for efficient packing
typedef struct Script_Value {
    Script_Value_Type type;
    union {
        bool boolean;
        double number;
        Script_String* string;
        Script_Function* function;
        Script_Native_Fn native;
        Script_Table* table;
        Script_Coroutine* coroutine;
        void* userdata;
    } as;
} Script_Value;

// String with hash for fast comparison
typedef struct Script_String {
    uint32_t hash;
    uint32_t length;
    uint32_t ref_count;
    char data[]; // Flexible array member
} Script_String;

// Table (hash map) for objects
typedef struct Script_Table_Entry {
    Script_String* key;
    Script_Value value;
    struct Script_Table_Entry* next;
} Script_Table_Entry;

typedef struct Script_Table {
    uint32_t ref_count;
    uint32_t size;
    uint32_t capacity;
    Script_Table_Entry** buckets;
    struct Script_Table* metatable; // For metamethods
} Script_Table;

// Bytecode instructions - 32-bit for alignment
typedef enum {
    // Stack operations
    OP_PUSH_NIL,
    OP_PUSH_TRUE,
    OP_PUSH_FALSE,
    OP_PUSH_NUMBER,   // arg: constant index
    OP_PUSH_STRING,   // arg: constant index
    OP_POP,
    OP_DUP,
    OP_SWAP,
    
    // Variables
    OP_GET_LOCAL,     // arg: stack offset
    OP_SET_LOCAL,     // arg: stack offset
    OP_GET_GLOBAL,    // arg: string constant
    OP_SET_GLOBAL,    // arg: string constant
    OP_GET_UPVAL,     // arg: upvalue index
    OP_SET_UPVAL,     // arg: upvalue index
    
    // Table operations
    OP_NEW_TABLE,     // arg: initial capacity
    OP_GET_FIELD,     // TOS[-1] = table, TOS[0] = key
    OP_SET_FIELD,     // TOS[-2] = table, TOS[-1] = key, TOS[0] = value
    
    // Arithmetic
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_NEG,
    OP_POW,
    
    // Comparison
    OP_EQ,
    OP_NEQ,
    OP_LT,
    OP_LE,
    OP_GT,
    OP_GE,
    
    // Logical
    OP_AND,
    OP_OR,
    OP_NOT,
    
    // Control flow
    OP_JMP,           // arg: offset
    OP_JMP_IF_FALSE,  // arg: offset
    OP_JMP_IF_TRUE,   // arg: offset
    OP_LOOP,          // arg: negative offset
    
    // Functions
    OP_CALL,          // arg: argc
    OP_RETURN,        // arg: return value count
    OP_CLOSURE,       // arg: function constant
    OP_CLOSE_UPVAL,
    
    // Coroutines
    OP_YIELD,         // arg: value count
    OP_RESUME,        // arg: value count
    
    // Debug
    OP_PRINT,
    OP_ASSERT,
    OP_BREAKPOINT,
    
    OP_COUNT
} Script_Opcode;

// Bytecode instruction - 32-bit aligned
typedef struct {
    uint8_t opcode;
    uint8_t arg_a;
    uint16_t arg_b;
} Script_Instruction;

// Upvalue for closures
typedef struct Script_Upvalue {
    Script_Value* location;
    Script_Value closed;
    struct Script_Upvalue* next;
} Script_Upvalue;

// Function object
typedef struct Script_Function {
    uint32_t ref_count;
    uint32_t arity;
    uint32_t upvalue_count;
    uint32_t instruction_count;
    uint32_t constant_count;
    uint32_t local_count;
    Script_Instruction* code;
    Script_Value* constants;
    Script_String* name;
    Script_String* source_file;
    uint32_t* line_info; // Line number for each instruction
    
    // JIT info
    void* jit_code;
    uint32_t execution_count;
    uint32_t optimization_level;
} Script_Function;

// Call frame
typedef struct Script_Frame {
    Script_Function* function;
    Script_Instruction* ip;
    Script_Value* stack_base;
    Script_Upvalue** upvalues;
} Script_Frame;

// Coroutine state
typedef enum {
    COROUTINE_SUSPENDED,
    COROUTINE_RUNNING,
    COROUTINE_DEAD
} Script_Coroutine_State;

// Coroutine object
typedef struct Script_Coroutine {
    uint32_t ref_count;
    Script_Coroutine_State state;
    Script_Frame* frames;
    uint32_t frame_count;
    uint32_t frame_capacity;
    Script_Value* stack;
    uint32_t stack_size;
    uint32_t stack_capacity;
} Script_Coroutine;

// Memory allocator interface
typedef struct {
    void* (*alloc)(size_t size);
    void* (*realloc)(void* ptr, size_t old_size, size_t new_size);
    void (*free)(void* ptr, size_t size);
    void* userdata;
} Script_Allocator;

// GC statistics
typedef struct {
    uint64_t bytes_allocated;
    uint64_t bytes_freed;
    uint64_t gc_runs;
    uint64_t gc_time_ms;
    uint32_t live_objects;
    uint32_t dead_objects;
} Script_GC_Stats;

// VM configuration
typedef struct {
    uint32_t stack_size;        // Default: 8192
    uint32_t frame_stack_size;   // Default: 256
    uint32_t gc_threshold;       // Bytes before GC trigger
    uint32_t jit_threshold;      // Executions before JIT
    bool enable_jit;
    bool enable_debug;
    bool enable_profiling;
    Script_Allocator allocator;
} Script_Config;

// Main VM structure
typedef struct Script_VM {
    // Configuration
    Script_Config config;
    
    // Execution state
    Script_Value* stack;
    Script_Value* stack_top;
    uint32_t stack_capacity;
    
    Script_Frame* frames;
    Script_Frame* frame_top;
    uint32_t frame_capacity;
    
    // Global environment
    Script_Table* globals;
    
    // String interning
    Script_Table* strings;
    
    // GC state
    Script_GC_Stats gc_stats;
    uint64_t next_gc;
    Script_Value* gray_stack;
    uint32_t gray_count;
    uint32_t gray_capacity;
    
    // Open upvalues
    Script_Upvalue* open_upvalues;
    
    // Active coroutines
    Script_Coroutine* current_coroutine;
    
    // Error handling
    char error_message[256];
    Script_String* last_error;
    
    // JIT compiler state
    void* jit_state;
    
    // Debug hooks
    void (*debug_hook)(Script_VM* vm, Script_Frame* frame);
    void* debug_userdata;
    
    // Profiling
    uint64_t* instruction_counts;
    uint64_t* instruction_cycles;
} Script_VM;

// Compilation result
typedef struct {
    Script_Function* function;
    char* error_message;
    uint32_t error_line;
    uint32_t error_column;
} Script_Compile_Result;

// API Functions

// VM lifecycle
Script_VM* script_vm_create(Script_Config* config);
void script_vm_destroy(Script_VM* vm);
void script_vm_reset(Script_VM* vm);

// Compilation
Script_Compile_Result script_compile(Script_VM* vm, const char* source, const char* name);
Script_Compile_Result script_compile_file(Script_VM* vm, const char* filename);
void script_free_compile_result(Script_VM* vm, Script_Compile_Result* result);

// Execution
bool script_call(Script_VM* vm, Script_Value function, int argc, Script_Value* argv, Script_Value* result);
bool script_run(Script_VM* vm, Script_Function* function);
bool script_eval(Script_VM* vm, const char* source, Script_Value* result);

// Stack manipulation
void script_push(Script_VM* vm, Script_Value value);
Script_Value script_pop(Script_VM* vm);
Script_Value script_peek(Script_VM* vm, int offset);
void script_set_top(Script_VM* vm, int index);
int script_get_top(Script_VM* vm);

// Value creation
Script_Value script_nil(void);
Script_Value script_bool(bool value);
Script_Value script_number(double value);
Script_Value script_string(Script_VM* vm, const char* str, size_t length);
Script_Value script_native(Script_Native_Fn fn);
Script_Value script_table(Script_VM* vm, uint32_t capacity);
Script_Value script_userdata(void* data);

// Value inspection
bool script_is_nil(Script_Value value);
bool script_is_bool(Script_Value value);
bool script_is_number(Script_Value value);
bool script_is_string(Script_Value value);
bool script_is_function(Script_Value value);
bool script_is_table(Script_Value value);
bool script_is_truthy(Script_Value value);

// Value conversion
bool script_to_bool(Script_Value value);
double script_to_number(Script_Value value);
const char* script_to_string(Script_VM* vm, Script_Value value);

// Table operations
void script_table_set(Script_VM* vm, Script_Table* table, const char* key, Script_Value value);
Script_Value script_table_get(Script_VM* vm, Script_Table* table, const char* key);
bool script_table_has(Script_VM* vm, Script_Table* table, const char* key);
void script_table_remove(Script_VM* vm, Script_Table* table, const char* key);
uint32_t script_table_size(Script_Table* table);

// Global environment
void script_set_global(Script_VM* vm, const char* name, Script_Value value);
Script_Value script_get_global(Script_VM* vm, const char* name);
bool script_has_global(Script_VM* vm, const char* name);

// Native function binding
void script_bind_function(Script_VM* vm, const char* name, Script_Native_Fn fn);
void script_bind_table(Script_VM* vm, const char* name, Script_Table* table);

// Coroutines
Script_Coroutine* script_coroutine_create(Script_VM* vm, Script_Function* function);
bool script_coroutine_resume(Script_VM* vm, Script_Coroutine* coro, int argc, Script_Value* argv, Script_Value* result);
bool script_coroutine_yield(Script_VM* vm, int argc, Script_Value* argv);
Script_Coroutine_State script_coroutine_status(Script_Coroutine* coro);

// Memory management
void script_gc_run(Script_VM* vm);
void script_gc_pause(Script_VM* vm);
void script_gc_resume(Script_VM* vm);
Script_GC_Stats script_gc_stats(Script_VM* vm);

// Debug interface
void script_set_debug_hook(Script_VM* vm, void (*hook)(Script_VM*, Script_Frame*), void* userdata);
void script_print_stack(Script_VM* vm);
void script_print_value(Script_Value value);
const char* script_get_error(Script_VM* vm);

// JIT control
void script_jit_enable(Script_VM* vm, bool enable);
void script_jit_compile(Script_VM* vm, Script_Function* function);
void script_jit_reset(Script_VM* vm);

// Hot reload support
bool script_save_state(Script_VM* vm, void* buffer, size_t* size);
bool script_load_state(Script_VM* vm, const void* buffer, size_t size);

// Performance counters
uint64_t script_get_instruction_count(Script_VM* vm, Script_Opcode op);
uint64_t script_get_instruction_cycles(Script_VM* vm, Script_Opcode op);
void script_reset_profiling(Script_VM* vm);

#endif // HANDMADE_SCRIPT_H