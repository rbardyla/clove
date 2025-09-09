/*
 * NEURAL JIT COMPILER - X86-64 CODE GENERATION DEMO
 * ==================================================
 * 
 * Demonstrates actual machine code generation for simple operations.
 * Generates real x86-64 instructions that can be executed.
 * 
 * Author: Handmade Neural Engine
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <time.h>
#include <math.h>

/* ============================================================================
 * X86-64 INSTRUCTION ENCODING
 * ============================================================================
 * Direct machine code emission - no assembler needed
 */

typedef struct {
    uint8_t* code;
    size_t size;
    size_t capacity;
} CodeBuffer;

static void emit_byte(CodeBuffer* buf, uint8_t byte) {
    if (buf->size < buf->capacity) {
        buf->code[buf->size++] = byte;
    }
}

static void emit_bytes(CodeBuffer* buf, const uint8_t* bytes, size_t count) {
    for (size_t i = 0; i < count; i++) {
        emit_byte(buf, bytes[i]);
    }
}

static void emit_u32(CodeBuffer* buf, uint32_t value) {
    emit_byte(buf, value & 0xFF);
    emit_byte(buf, (value >> 8) & 0xFF);
    emit_byte(buf, (value >> 16) & 0xFF);
    emit_byte(buf, (value >> 24) & 0xFF);
}

/* ============================================================================
 * SIMPLE CODE GENERATION EXAMPLES
 * ============================================================================
 */

/* Generate a simple function that returns a constant
 * int return_42(void) { return 42; }
 */
static void generate_return_constant(CodeBuffer* buf, int value) {
    printf("Generating: return %d\n", value);
    
    /* mov eax, value */
    emit_byte(buf, 0xB8);
    emit_u32(buf, value);
    
    /* ret */
    emit_byte(buf, 0xC3);
    
    printf("  Generated %zu bytes of machine code\n", buf->size);
}

/* Generate a function that adds two integers
 * int add(int a, int b) { return a + b; }
 * Parameters in: EDI, ESI (System V AMD64 ABI, using 32-bit registers)
 */
static void generate_add_function(CodeBuffer* buf) {
    printf("Generating: add(a, b)\n");
    
    /* mov eax, edi  ; Move first parameter to EAX */
    emit_bytes(buf, (uint8_t[]){0x89, 0xF8}, 2);
    
    /* add eax, esi  ; Add second parameter */
    emit_bytes(buf, (uint8_t[]){0x01, 0xF0}, 2);
    
    /* ret */
    emit_byte(buf, 0xC3);
    
    printf("  Generated %zu bytes of machine code\n", buf->size);
}

/* Generate a simple loop that sums an array
 * int sum_array(int* arr, int count)
 * Parameters: RDI = array pointer, RSI = count
 */
static void generate_sum_array(CodeBuffer* buf) {
    printf("Generating: sum_array(arr, count)\n");
    
    /* xor eax, eax  ; sum = 0 */
    emit_bytes(buf, (uint8_t[]){0x31, 0xC0}, 2);
    
    /* xor ecx, ecx  ; i = 0 */
    emit_bytes(buf, (uint8_t[]){0x31, 0xC9}, 2);
    
    /* test esi, esi  ; if count == 0 */
    emit_bytes(buf, (uint8_t[]){0x85, 0xF6}, 2);
    
    /* jz .done  ; jump to end if count is 0 */
    emit_bytes(buf, (uint8_t[]){0x74, 0x0A}, 2);  /* 10 bytes ahead */
    
    /* .loop: */
    /* add eax, [rdi + rcx*4]  ; sum += arr[i] */
    emit_bytes(buf, (uint8_t[]){0x03, 0x04, 0x8F}, 3);
    
    /* inc ecx  ; i++ */
    emit_bytes(buf, (uint8_t[]){0xFF, 0xC1}, 2);
    
    /* cmp ecx, esi  ; i < count? */
    emit_bytes(buf, (uint8_t[]){0x39, 0xF1}, 2);
    
    /* jl .loop  ; jump back if less */
    emit_bytes(buf, (uint8_t[]){0x7C, 0xF5}, 2);  /* -11 bytes back */
    
    /* .done: */
    /* ret */
    emit_byte(buf, 0xC3);
    
    printf("  Generated %zu bytes of machine code\n", buf->size);
}

/* Generate optimized array sum using loop unrolling
 * Processes 4 elements at a time for better performance
 */
static void generate_sum_array_unrolled(CodeBuffer* buf) {
    printf("Generating: sum_array_unrolled(arr, count)\n");
    
    /* xor eax, eax  ; sum = 0 */
    emit_bytes(buf, (uint8_t[]){0x31, 0xC0}, 2);
    
    /* xor ecx, ecx  ; i = 0 */
    emit_bytes(buf, (uint8_t[]){0x31, 0xC9}, 2);
    
    /* mov edx, esi  ; edx = count */
    emit_bytes(buf, (uint8_t[]){0x89, 0xF2}, 2);
    
    /* shr edx, 2  ; edx = count / 4 */
    emit_bytes(buf, (uint8_t[]){0xC1, 0xEA, 0x02}, 3);
    
    /* jz .remainder  ; skip if no groups of 4 */
    emit_bytes(buf, (uint8_t[]){0x74, 0x18}, 2);
    
    /* .unrolled_loop: */
    /* add eax, [rdi]      ; sum += arr[i+0] */
    emit_bytes(buf, (uint8_t[]){0x03, 0x07}, 2);
    
    /* add eax, [rdi + 4]  ; sum += arr[i+1] */
    emit_bytes(buf, (uint8_t[]){0x03, 0x47, 0x04}, 3);
    
    /* add eax, [rdi + 8]  ; sum += arr[i+2] */
    emit_bytes(buf, (uint8_t[]){0x03, 0x47, 0x08}, 3);
    
    /* add eax, [rdi + 12] ; sum += arr[i+3] */
    emit_bytes(buf, (uint8_t[]){0x03, 0x47, 0x0C}, 3);
    
    /* add rdi, 16  ; advance pointer by 4 elements */
    emit_bytes(buf, (uint8_t[]){0x48, 0x83, 0xC7, 0x10}, 4);
    
    /* add ecx, 4  ; i += 4 */
    emit_bytes(buf, (uint8_t[]){0x83, 0xC1, 0x04}, 3);
    
    /* dec edx  ; groups-- */
    emit_bytes(buf, (uint8_t[]){0xFF, 0xCA}, 2);
    
    /* jnz .unrolled_loop  ; continue if more groups */
    emit_bytes(buf, (uint8_t[]){0x75, 0xE6}, 2);
    
    /* .remainder: */
    /* cmp ecx, esi  ; i < count? */
    emit_bytes(buf, (uint8_t[]){0x39, 0xF1}, 2);
    
    /* jge .done  ; exit if done */
    emit_bytes(buf, (uint8_t[]){0x7D, 0x0B}, 2);
    
    /* .remainder_loop: */
    /* add eax, [rdi]  ; sum += *arr */
    emit_bytes(buf, (uint8_t[]){0x03, 0x07}, 2);
    
    /* add rdi, 4  ; arr++ */
    emit_bytes(buf, (uint8_t[]){0x48, 0x83, 0xC7, 0x04}, 4);
    
    /* inc ecx  ; i++ */
    emit_bytes(buf, (uint8_t[]){0xFF, 0xC1}, 2);
    
    /* cmp ecx, esi  ; i < count? */
    emit_bytes(buf, (uint8_t[]){0x39, 0xF1}, 2);
    
    /* jl .remainder_loop */
    emit_bytes(buf, (uint8_t[]){0x7C, 0xF3}, 2);
    
    /* .done: */
    /* ret */
    emit_byte(buf, 0xC3);
    
    printf("  Generated %zu bytes of machine code (with 4x unrolling)\n", buf->size);
}

/* ============================================================================
 * JIT EXECUTION FRAMEWORK
 * ============================================================================
 */

static void* allocate_executable_memory(size_t size) {
    void* mem = mmap(NULL, size,
                    PROT_READ | PROT_WRITE | PROT_EXEC,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        return NULL;
    }
    return mem;
}

static void free_executable_memory(void* mem, size_t size) {
    munmap(mem, size);
}

/* Execute generated code and measure performance */
static void test_generated_code(void) {
    printf("\n=== Testing Generated Code ===\n\n");
    
    /* Allocate executable memory */
    size_t code_size = 4096;
    uint8_t* exec_mem = allocate_executable_memory(code_size);
    if (!exec_mem) {
        printf("Failed to allocate executable memory\n");
        return;
    }
    
    /* Test 1: Return constant */
    {
        CodeBuffer buf = { .code = exec_mem, .size = 0, .capacity = code_size };
        generate_return_constant(&buf, 42);
        
        /* Cast to function pointer and execute */
        typedef int (*func_t)(void);
        func_t func = (func_t)exec_mem;
        int result = func();
        
        printf("  Result: %d (expected 42)\n\n", result);
    }
    
    /* Test 2: Add function */
    {
        CodeBuffer buf = { .code = exec_mem, .size = 0, .capacity = code_size };
        generate_add_function(&buf);
        
        typedef int (*func_t)(int, int);
        func_t func = (func_t)exec_mem;
        int result = func(10, 32);
        
        printf("  Result: 10 + 32 = %d (expected 42)\n\n", result);
    }
    
    /* Test 3: Array sum */
    {
        int array[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        int count = 10;
        
        CodeBuffer buf = { .code = exec_mem, .size = 0, .capacity = code_size };
        generate_sum_array(&buf);
        
        typedef int (*func_t)(int*, int);
        func_t func = (func_t)exec_mem;
        int result = func(array, count);
        
        printf("  Result: sum([1..10]) = %d (expected 55)\n\n", result);
    }
    
    /* Test 4: Compare naive vs unrolled array sum */
    {
        const int SIZE = 10000;
        int* large_array = malloc(SIZE * sizeof(int));
        for (int i = 0; i < SIZE; i++) {
            large_array[i] = i + 1;
        }
        
        /* Generate naive version */
        CodeBuffer buf1 = { .code = exec_mem, .size = 0, .capacity = code_size };
        generate_sum_array(&buf1);
        typedef int (*func_t)(int*, int);
        func_t naive_func = (func_t)exec_mem;
        
        /* Generate unrolled version at different location */
        uint8_t* exec_mem2 = allocate_executable_memory(code_size);
        CodeBuffer buf2 = { .code = exec_mem2, .size = 0, .capacity = code_size };
        generate_sum_array_unrolled(&buf2);
        func_t unrolled_func = (func_t)exec_mem2;
        
        /* Benchmark both */
        const int ITERATIONS = 100000;
        
        clock_t start = clock();
        int sum1 = 0;
        for (int i = 0; i < ITERATIONS; i++) {
            sum1 = naive_func(large_array, SIZE);
        }
        double naive_time = (double)(clock() - start) / CLOCKS_PER_SEC;
        
        start = clock();
        int sum2 = 0;
        for (int i = 0; i < ITERATIONS; i++) {
            sum2 = unrolled_func(large_array, SIZE);
        }
        double unrolled_time = (double)(clock() - start) / CLOCKS_PER_SEC;
        
        printf("Performance comparison (sum of %d elements, %d iterations):\n", 
               SIZE, ITERATIONS);
        printf("  Naive:    %.3f seconds (sum=%d)\n", naive_time, sum1);
        printf("  Unrolled: %.3f seconds (sum=%d)\n", unrolled_time, sum2);
        printf("  Speedup:  %.2fx\n\n", naive_time / unrolled_time);
        
        free(large_array);
        free_executable_memory(exec_mem2, code_size);
    }
    
    free_executable_memory(exec_mem, code_size);
}

/* Disassemble generated code (for debugging) */
static void dump_code(const uint8_t* code, size_t size, const char* name) {
    printf("Assembly for %s (%zu bytes):\n", name, size);
    printf("  Hex dump: ");
    for (size_t i = 0; i < size && i < 32; i++) {
        printf("%02X ", code[i]);
        if ((i + 1) % 16 == 0) printf("\n           ");
    }
    if (size > 32) printf("...");
    printf("\n\n");
}

/* Demonstrate the compilation process */
static void demo_compilation_process(void) {
    printf("=== JIT Compilation Process ===\n\n");
    
    printf("Step 1: Allocate executable memory\n");
    printf("  Using mmap with PROT_EXEC flag\n\n");
    
    printf("Step 2: Emit machine code bytes\n");
    uint8_t code_buffer[256];
    CodeBuffer buf = { .code = code_buffer, .size = 0, .capacity = 256 };
    
    printf("  Example: generating 'return 42'\n");
    printf("    mov eax, 42  => B8 2A 00 00 00\n");
    printf("    ret          => C3\n");
    generate_return_constant(&buf, 42);
    dump_code(code_buffer, buf.size, "return_42");
    
    printf("Step 3: Execute generated code\n");
    printf("  Cast memory to function pointer\n");
    printf("  Call like normal C function\n\n");
    
    printf("Step 4: Profile and optimize\n");
    printf("  Track execution count and cycles\n");
    printf("  Recompile hot paths with better optimization\n\n");
}

/* Main demonstration */
int main(void) {
    printf("==========================================\n");
    printf(" X86-64 CODE GENERATION DEMONSTRATION\n");
    printf(" Direct Machine Code Emission\n");
    printf("==========================================\n\n");
    
    /* Show the compilation process */
    demo_compilation_process();
    
    /* Test generated code */
    test_generated_code();
    
    /* Summary */
    printf("=== Summary ===\n\n");
    printf("This demonstration showed:\n");
    printf("1. Direct emission of x86-64 machine code\n");
    printf("2. No external assembler or compiler needed\n");
    printf("3. Generated code executes like native functions\n");
    printf("4. Loop unrolling provides measurable speedup\n\n");
    
    printf("Real-world JIT applications:\n");
    printf("- Neural network kernels (GEMM, convolutions)\n");
    printf("- Custom activation functions\n");
    printf("- Fused operations to reduce memory traffic\n");
    printf("- CPU-specific optimizations (AVX2, FMA)\n\n");
    
    printf("Key advantages:\n");
    printf("- Zero dependencies\n");
    printf("- Complete control over generated code\n");
    printf("- Can adapt to runtime conditions\n");
    printf("- Optimal performance for hot paths\n\n");
    
    printf("This is handmade performance:\n");
    printf("Every instruction deliberate, every byte understood.\n");
    
    return 0;
}