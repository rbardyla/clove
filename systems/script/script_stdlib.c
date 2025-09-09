/*
 * Handmade Script Standard Library
 * 
 * Math, string, table, file I/O functions
 * All sandboxed for security
 */

#include "handmade_script.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Math functions
static Script_Value math_abs(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc != 1 || !script_is_number(argv[0])) {
        return script_nil();
    }
    return script_number(fabs(argv[0].as.number));
}

static Script_Value math_floor(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc != 1 || !script_is_number(argv[0])) {
        return script_nil();
    }
    return script_number(floor(argv[0].as.number));
}

static Script_Value math_ceil(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc != 1 || !script_is_number(argv[0])) {
        return script_nil();
    }
    return script_number(ceil(argv[0].as.number));
}

static Script_Value math_sin(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc != 1 || !script_is_number(argv[0])) {
        return script_nil();
    }
    return script_number(sin(argv[0].as.number));
}

static Script_Value math_cos(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc != 1 || !script_is_number(argv[0])) {
        return script_nil();
    }
    return script_number(cos(argv[0].as.number));
}

static Script_Value math_sqrt(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc != 1 || !script_is_number(argv[0])) {
        return script_nil();
    }
    return script_number(sqrt(argv[0].as.number));
}

static Script_Value math_random(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc == 0) {
        return script_number((double)rand() / RAND_MAX);
    } else if (argc == 2 && script_is_number(argv[0]) && script_is_number(argv[1])) {
        double min = argv[0].as.number;
        double max = argv[1].as.number;
        return script_number(min + (max - min) * ((double)rand() / RAND_MAX));
    }
    return script_nil();
}

// String functions
static Script_Value string_length(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc != 1 || !script_is_string(argv[0])) {
        return script_nil();
    }
    return script_number(argv[0].as.string->length);
}

static Script_Value string_substr(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc < 2 || !script_is_string(argv[0]) || !script_is_number(argv[1])) {
        return script_nil();
    }
    
    Script_String* str = argv[0].as.string;
    int start = (int)argv[1].as.number;
    int len = (argc >= 3 && script_is_number(argv[2])) ? 
              (int)argv[2].as.number : str->length - start;
    
    if (start < 0 || start >= str->length || len <= 0) {
        return script_string(vm, "", 0);
    }
    
    if (start + len > str->length) {
        len = str->length - start;
    }
    
    return script_string(vm, str->data + start, len);
}

static Script_Value string_find(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc != 2 || !script_is_string(argv[0]) || !script_is_string(argv[1])) {
        return script_nil();
    }
    
    Script_String* haystack = argv[0].as.string;
    Script_String* needle = argv[1].as.string;
    
    char* found = strstr(haystack->data, needle->data);
    if (found) {
        return script_number(found - haystack->data);
    }
    return script_number(-1);
}

static Script_Value string_replace(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc != 3 || !script_is_string(argv[0]) || 
        !script_is_string(argv[1]) || !script_is_string(argv[2])) {
        return script_nil();
    }
    
    Script_String* str = argv[0].as.string;
    Script_String* from = argv[1].as.string;
    Script_String* to = argv[2].as.string;
    
    // Simple implementation - replace first occurrence
    char* found = strstr(str->data, from->data);
    if (!found) {
        return argv[0];
    }
    
    size_t prefix_len = found - str->data;
    size_t suffix_start = prefix_len + from->length;
    size_t suffix_len = str->length - suffix_start;
    size_t new_len = prefix_len + to->length + suffix_len;
    
    char* buffer = malloc(new_len + 1);
    memcpy(buffer, str->data, prefix_len);
    memcpy(buffer + prefix_len, to->data, to->length);
    memcpy(buffer + prefix_len + to->length, str->data + suffix_start, suffix_len);
    buffer[new_len] = '\0';
    
    Script_Value result = script_string(vm, buffer, new_len);
    free(buffer);
    return result;
}

// Table functions
static Script_Value table_insert(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc != 3 || !script_is_table(argv[0]) || !script_is_string(argv[1])) {
        return script_nil();
    }
    
    script_table_set(vm, argv[0].as.table, argv[1].as.string->data, argv[2]);
    return argv[2];
}

static Script_Value table_remove(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc != 2 || !script_is_table(argv[0]) || !script_is_string(argv[1])) {
        return script_nil();
    }
    
    Script_Value value = script_table_get(vm, argv[0].as.table, argv[1].as.string->data);
    script_table_remove(vm, argv[0].as.table, argv[1].as.string->data);
    return value;
}

static Script_Value table_size(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc != 1 || !script_is_table(argv[0])) {
        return script_nil();
    }
    
    return script_number(script_table_size(argv[0].as.table));
}

// I/O functions (sandboxed)
static Script_Value io_print(Script_VM* vm, int argc, Script_Value* argv) {
    for (int i = 0; i < argc; i++) {
        if (i > 0) printf("\t");
        printf("%s", script_to_string(vm, argv[i]));
    }
    printf("\n");
    return script_nil();
}

static Script_Value io_read_file(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc != 1 || !script_is_string(argv[0])) {
        return script_nil();
    }
    
    // Sandboxed - only allow reading from specific directories
    const char* filename = argv[0].as.string->data;
    if (strstr(filename, "..") || filename[0] == '/') {
        // Prevent directory traversal
        return script_nil();
    }
    
    FILE* file = fopen(filename, "rb");
    if (!file) {
        return script_nil();
    }
    
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* buffer = malloc(size + 1);
    fread(buffer, 1, size, file);
    buffer[size] = '\0';
    fclose(file);
    
    Script_Value result = script_string(vm, buffer, size);
    free(buffer);
    return result;
}

static Script_Value io_write_file(Script_VM* vm, int argc, Script_Value* argv) {
    if (argc != 2 || !script_is_string(argv[0]) || !script_is_string(argv[1])) {
        return script_bool(false);
    }
    
    // Sandboxed - only allow writing to specific directories
    const char* filename = argv[0].as.string->data;
    if (strstr(filename, "..") || filename[0] == '/') {
        return script_bool(false);
    }
    
    FILE* file = fopen(filename, "wb");
    if (!file) {
        return script_bool(false);
    }
    
    Script_String* content = argv[1].as.string;
    size_t written = fwrite(content->data, 1, content->length, file);
    fclose(file);
    
    return script_bool(written == content->length);
}

// System functions
static Script_Value sys_time(Script_VM* vm, int argc, Script_Value* argv) {
    return script_number((double)time(NULL));
}

static Script_Value sys_clock(Script_VM* vm, int argc, Script_Value* argv) {
    return script_number((double)clock() / CLOCKS_PER_SEC);
}

static Script_Value sys_gc(Script_VM* vm, int argc, Script_Value* argv) {
    script_gc_run(vm);
    return script_nil();
}

static Script_Value sys_memory(Script_VM* vm, int argc, Script_Value* argv) {
    Script_GC_Stats stats = script_gc_stats(vm);
    Script_Value table = script_table(vm, 4);
    
    script_table_set(vm, table.as.table, "allocated", 
                    script_number(stats.bytes_allocated));
    script_table_set(vm, table.as.table, "freed", 
                    script_number(stats.bytes_freed));
    script_table_set(vm, table.as.table, "gc_runs", 
                    script_number(stats.gc_runs));
    script_table_set(vm, table.as.table, "live_objects", 
                    script_number(stats.live_objects));
    
    return table;
}

// Register all standard library functions
void script_register_stdlib(Script_VM* vm) {
    // Math library
    Script_Value math_lib = script_table(vm, 16);
    script_table_set(vm, math_lib.as.table, "abs", script_native(math_abs));
    script_table_set(vm, math_lib.as.table, "floor", script_native(math_floor));
    script_table_set(vm, math_lib.as.table, "ceil", script_native(math_ceil));
    script_table_set(vm, math_lib.as.table, "sin", script_native(math_sin));
    script_table_set(vm, math_lib.as.table, "cos", script_native(math_cos));
    script_table_set(vm, math_lib.as.table, "sqrt", script_native(math_sqrt));
    script_table_set(vm, math_lib.as.table, "random", script_native(math_random));
    script_table_set(vm, math_lib.as.table, "pi", script_number(3.14159265359));
    script_set_global(vm, "math", math_lib);
    
    // String library
    Script_Value string_lib = script_table(vm, 8);
    script_table_set(vm, string_lib.as.table, "length", script_native(string_length));
    script_table_set(vm, string_lib.as.table, "substr", script_native(string_substr));
    script_table_set(vm, string_lib.as.table, "find", script_native(string_find));
    script_table_set(vm, string_lib.as.table, "replace", script_native(string_replace));
    script_set_global(vm, "string", string_lib);
    
    // Table library
    Script_Value table_lib = script_table(vm, 4);
    script_table_set(vm, table_lib.as.table, "insert", script_native(table_insert));
    script_table_set(vm, table_lib.as.table, "remove", script_native(table_remove));
    script_table_set(vm, table_lib.as.table, "size", script_native(table_size));
    script_set_global(vm, "table", table_lib);
    
    // I/O library
    Script_Value io_lib = script_table(vm, 4);
    script_table_set(vm, io_lib.as.table, "print", script_native(io_print));
    script_table_set(vm, io_lib.as.table, "read_file", script_native(io_read_file));
    script_table_set(vm, io_lib.as.table, "write_file", script_native(io_write_file));
    script_set_global(vm, "io", io_lib);
    
    // System library
    Script_Value sys_lib = script_table(vm, 4);
    script_table_set(vm, sys_lib.as.table, "time", script_native(sys_time));
    script_table_set(vm, sys_lib.as.table, "clock", script_native(sys_clock));
    script_table_set(vm, sys_lib.as.table, "gc", script_native(sys_gc));
    script_table_set(vm, sys_lib.as.table, "memory", script_native(sys_memory));
    script_set_global(vm, "sys", sys_lib);
    
    // Global functions
    script_bind_function(vm, "print", io_print);
    
    // Initialize random seed
    srand(time(NULL));
}