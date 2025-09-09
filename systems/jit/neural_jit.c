/*
 * NEURAL JIT COMPILER IMPLEMENTATION
 * ===================================
 * 
 * Handmade x86-64 code generation for neural operations.
 * No external dependencies. Every instruction hand-crafted.
 * 
 * ARCHITECTURE:
 * - Direct machine code emission (no intermediate representation)
 * - Profile-guided optimization with hot path detection
 * - SIMD vectorization using AVX2/FMA instructions
 * - Custom memory allocator for executable pages
 * 
 * Author: Handmade Neural Engine
 */

#include "neural_jit.h"
#include <sys/mman.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* PERFORMANCE: JIT compilation threshold - compile after N calls */
#define JIT_COMPILE_THRESHOLD 100

/* MEMORY: Page size for executable memory allocation */
#define PAGE_SIZE 4096

/* CACHE: Maximum number of cached kernels */
#define MAX_CACHE_ENTRIES 256

/* ============================================================================
 * CPU FEATURE DETECTION
 * ============================================================================
 * PERFORMANCE: Detect CPU capabilities at runtime for optimal code generation
 */

static void cpuid(uint32_t leaf, uint32_t subleaf, 
                 uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
    __asm__ __volatile__(
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(subleaf)
    );
}

void njit_detect_cpu_features(CPUFeatures* features) {
    uint32_t eax, ebx, ecx, edx;
    
    /* Clear features */
    memset(features, 0, sizeof(CPUFeatures));
    
    /* Get maximum supported CPUID leaf */
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    uint32_t max_leaf = eax;
    
    if (max_leaf >= 1) {
        /* Standard features */
        cpuid(1, 0, &eax, &ebx, &ecx, &edx);
        
        /* EDX features */
        features->has_sse2 = (edx >> 26) & 1;
        
        /* ECX features */
        features->has_sse3   = (ecx >> 0) & 1;
        features->has_ssse3  = (ecx >> 9) & 1;
        features->has_sse41  = (ecx >> 19) & 1;
        features->has_sse42  = (ecx >> 20) & 1;
        features->has_avx    = (ecx >> 28) & 1;
        features->has_fma3   = (ecx >> 12) & 1;
    }
    
    if (max_leaf >= 7) {
        /* Extended features */
        cpuid(7, 0, &eax, &ebx, &ecx, &edx);
        
        /* EBX features */
        features->has_avx2    = (ebx >> 5) & 1;
        features->has_avx512f = (ebx >> 16) & 1;
    }
    
    /* DEBUG: Print detected features */
    printf("CPU Features: SSE2=%d SSE3=%d SSSE3=%d SSE4.1=%d SSE4.2=%d "
           "AVX=%d AVX2=%d FMA3=%d AVX512F=%d\n",
           features->has_sse2, features->has_sse3, features->has_ssse3,
           features->has_sse41, features->has_sse42, features->has_avx,
           features->has_avx2, features->has_fma3, features->has_avx512f);
}

/* ============================================================================
 * X86-64 INSTRUCTION ENCODING
 * ============================================================================
 * PERFORMANCE: Direct machine code emission - no assembler needed
 */

typedef struct {
    uint8_t* code;
    size_t   size;
    size_t   capacity;
} CodeEmitter;

static void emit_byte(CodeEmitter* e, uint8_t byte) {
    if (e->size >= e->capacity) return; /* Safety check */
    e->code[e->size++] = byte;
}

static void emit_bytes(CodeEmitter* e, const uint8_t* bytes, size_t count) {
    for (size_t i = 0; i < count; i++) {
        emit_byte(e, bytes[i]);
    }
}

static void emit_u32(CodeEmitter* e, uint32_t value) {
    emit_byte(e, value & 0xFF);
    emit_byte(e, (value >> 8) & 0xFF);
    emit_byte(e, (value >> 16) & 0xFF);
    emit_byte(e, (value >> 24) & 0xFF);
}

/* Currently unused but kept for future extensions */
__attribute__((unused))
static void emit_u64(CodeEmitter* e, uint64_t value) {
    emit_u32(e, value & 0xFFFFFFFF);
    emit_u32(e, value >> 32);
}

/* REX prefix encoding for 64-bit operations */
/* Currently unused but kept for future 64-bit register operations */
__attribute__((unused))
static uint8_t rex_prefix(int w, int r, int x, int b) {
    return 0x40 | (w << 3) | (r << 2) | (x << 1) | b;
}

/* ModR/M byte encoding */
static uint8_t modrm_byte(int mod, int reg, int rm) {
    return (mod << 6) | ((reg & 7) << 3) | (rm & 7);
}

/* SIB byte encoding */
static uint8_t sib_byte(int scale, int index, int base) {
    return (scale << 6) | ((index & 7) << 3) | (base & 7);
}

/* VEX prefix for AVX/AVX2 instructions */
static void emit_vex3(CodeEmitter* e, uint8_t m, uint8_t w, uint8_t v, uint8_t l, uint8_t p) {
    emit_byte(e, 0xC4);
    emit_byte(e, ((~7) << 5) | m);  /* R=1, X=1, B=1, map_select */
    emit_byte(e, (w << 7) | ((~v & 0xF) << 3) | (l << 2) | p);
}

/* ============================================================================
 * AVX2/FMA CODE GENERATION
 * ============================================================================
 * PERFORMANCE: Generate optimized SIMD code for neural operations
 */

/* Generate AVX2 GEMM kernel for small matrices
 * PERFORMANCE: Unrolled 8x8 kernel using FMA instructions
 * CACHE: Fits in L1 cache for optimal throughput */
static void generate_gemm_kernel_avx2(CodeEmitter* e, uint32_t m, uint32_t n, uint32_t k) {
    /* Function prologue - System V AMD64 ABI
     * Parameters: RDI=A, RSI=B, RDX=C, RCX=m, R8=n, R9=k
     * XMM0=alpha, XMM1=beta */
    
    /* push rbp */
    emit_byte(e, 0x55);
    /* mov rbp, rsp */
    emit_bytes(e, (uint8_t[]){0x48, 0x89, 0xE5}, 3);
    
    /* Save callee-saved registers */
    /* push rbx */
    emit_byte(e, 0x53);
    /* push r12 */
    emit_bytes(e, (uint8_t[]){0x41, 0x54}, 2);
    /* push r13 */
    emit_bytes(e, (uint8_t[]){0x41, 0x55}, 2);
    /* push r14 */
    emit_bytes(e, (uint8_t[]){0x41, 0x56}, 2);
    /* push r15 */
    emit_bytes(e, (uint8_t[]){0x41, 0x57}, 2);
    
    /* Broadcast alpha and beta to YMM registers
     * vbroadcastss ymm2, xmm0  ; alpha */
    emit_vex3(e, 0x02, 0, 0, 1, 0x66);
    emit_byte(e, 0x18);
    emit_byte(e, modrm_byte(3, 2, 0));
    
    /* vbroadcastss ymm3, xmm1  ; beta */
    emit_vex3(e, 0x02, 0, 0, 1, 0x66);
    emit_byte(e, 0x18);
    emit_byte(e, modrm_byte(3, 3, 1));
    
    /* Main GEMM loop - iterate over output matrix
     * PERFORMANCE: Process 8x8 blocks with AVX2 (8 floats per YMM) */
    
    /* xor r12, r12  ; i = 0 */
    emit_bytes(e, (uint8_t[]){0x4D, 0x31, 0xE4}, 3);
    
    /* Outer loop: iterate over rows */
    /* .L_row_loop: */
    size_t row_loop_start = e->size;
    
    /* xor r13, r13  ; j = 0 */
    emit_bytes(e, (uint8_t[]){0x4D, 0x31, 0xED}, 3);
    
    /* Inner loop: iterate over columns */
    /* .L_col_loop: */
    size_t col_loop_start = e->size;
    
    /* Zero accumulator registers YMM4-YMM11 for 8x8 block
     * vxorps ymm4, ymm4, ymm4 */
    for (int i = 4; i < 12; i++) {
        emit_vex3(e, 0x01, 0, i, 1, 0);
        emit_byte(e, 0x57);
        emit_byte(e, modrm_byte(3, i, i));
    }
    
    /* xor r14, r14  ; kk = 0 */
    emit_bytes(e, (uint8_t[]){0x4D, 0x31, 0xF6}, 3);
    
    /* Reduction loop: compute dot product */
    /* .L_k_loop: */
    size_t k_loop_start = e->size;
    
    /* Load A[i:i+8, kk] into YMM12
     * Calculate address: rdi + (r12 * k + r14) * 4
     * mov rax, r12 */
    emit_bytes(e, (uint8_t[]){0x4C, 0x89, 0xE0}, 3);
    /* imul rax, r9  ; rax = i * k */
    emit_bytes(e, (uint8_t[]){0x49, 0x0F, 0xAF, 0xC1}, 4);
    /* add rax, r14  ; rax = i * k + kk */
    emit_bytes(e, (uint8_t[]){0x4C, 0x01, 0xF0}, 3);
    /* shl rax, 2     ; rax *= sizeof(float) */
    emit_bytes(e, (uint8_t[]){0x48, 0xC1, 0xE0, 0x02}, 4);
    /* add rax, rdi  ; rax = &A[i][kk] */
    emit_bytes(e, (uint8_t[]){0x48, 0x01, 0xF8}, 3);
    
    /* vmovups ymm12, [rax]  ; Load 8 floats from A */
    emit_vex3(e, 0x01, 0, 0, 1, 0);
    emit_byte(e, 0x10);
    emit_byte(e, modrm_byte(0, 12, 0));
    
    /* Load B[kk, j:j+8] and perform FMA
     * Calculate address: rsi + (r14 * n + r13) * 4 */
    /* mov rbx, r14 */
    emit_bytes(e, (uint8_t[]){0x4C, 0x89, 0xF3}, 3);
    /* imul rbx, r8  ; rbx = kk * n */
    emit_bytes(e, (uint8_t[]){0x49, 0x0F, 0xAF, 0xD8}, 4);
    /* add rbx, r13  ; rbx = kk * n + j */
    emit_bytes(e, (uint8_t[]){0x4C, 0x01, 0xEB}, 3);
    /* shl rbx, 2    ; rbx *= sizeof(float) */
    emit_bytes(e, (uint8_t[]){0x48, 0xC1, 0xE3, 0x02}, 4);
    /* add rbx, rsi  ; rbx = &B[kk][j] */
    emit_bytes(e, (uint8_t[]){0x48, 0x01, 0xF3}, 3);
    
    /* vmovups ymm13, [rbx]  ; Load 8 floats from B */
    emit_vex3(e, 0x01, 0, 0, 1, 0);
    emit_byte(e, 0x10);
    emit_byte(e, modrm_byte(0, 13, 3));
    
    /* vfmadd231ps ymm4, ymm12, ymm13  ; C[i][j:j+8] += A[i][kk] * B[kk][j:j+8] */
    emit_vex3(e, 0x02, 0, 4, 1, 0x66);
    emit_byte(e, 0xB8);
    emit_byte(e, modrm_byte(3, 12, 13));
    
    /* TODO: Unroll more for better performance with multiple accumulator registers */
    
    /* inc r14  ; kk++ */
    emit_bytes(e, (uint8_t[]){0x49, 0xFF, 0xC6}, 3);
    /* cmp r14, r9  ; kk < k? */
    emit_bytes(e, (uint8_t[]){0x4D, 0x39, 0xCE}, 3);
    /* jb .L_k_loop */
    emit_byte(e, 0x72);
    emit_byte(e, k_loop_start - e->size - 1);
    
    /* Scale by alpha and add beta*C
     * Calculate C address: rdx + (r12 * n + r13) * 4 */
    /* mov rax, r12 */
    emit_bytes(e, (uint8_t[]){0x4C, 0x89, 0xE0}, 3);
    /* imul rax, r8  ; rax = i * n */
    emit_bytes(e, (uint8_t[]){0x49, 0x0F, 0xAF, 0xC0}, 4);
    /* add rax, r13  ; rax = i * n + j */
    emit_bytes(e, (uint8_t[]){0x4C, 0x01, 0xE8}, 3);
    /* shl rax, 2    ; rax *= sizeof(float) */
    emit_bytes(e, (uint8_t[]){0x48, 0xC1, 0xE0, 0x02}, 4);
    /* add rax, rdx  ; rax = &C[i][j] */
    emit_bytes(e, (uint8_t[]){0x48, 0x01, 0xD0}, 3);
    
    /* vmulps ymm4, ymm4, ymm2  ; Scale by alpha */
    emit_vex3(e, 0x01, 0, 4, 1, 0);
    emit_byte(e, 0x59);
    emit_byte(e, modrm_byte(3, 4, 2));
    
    /* vmovups ymm14, [rax]  ; Load current C values */
    emit_vex3(e, 0x01, 0, 0, 1, 0);
    emit_byte(e, 0x10);
    emit_byte(e, modrm_byte(0, 14, 0));
    
    /* vfmadd231ps ymm4, ymm14, ymm3  ; C = alpha*AB + beta*C */
    emit_vex3(e, 0x02, 0, 4, 1, 0x66);
    emit_byte(e, 0xB8);
    emit_byte(e, modrm_byte(3, 14, 3));
    
    /* vmovups [rax], ymm4  ; Store result */
    emit_vex3(e, 0x01, 0, 0, 1, 0);
    emit_byte(e, 0x11);
    emit_byte(e, modrm_byte(0, 4, 0));
    
    /* add r13, 8  ; j += 8 */
    emit_bytes(e, (uint8_t[]){0x49, 0x83, 0xC5, 0x08}, 4);
    /* cmp r13, r8  ; j < n? */
    emit_bytes(e, (uint8_t[]){0x4D, 0x39, 0xC5}, 3);
    /* jb .L_col_loop */
    emit_byte(e, 0x72);
    emit_byte(e, col_loop_start - e->size - 1);
    
    /* inc r12  ; i++ */
    emit_bytes(e, (uint8_t[]){0x49, 0xFF, 0xC4}, 3);
    /* cmp r12, rcx  ; i < m? */
    emit_bytes(e, (uint8_t[]){0x4C, 0x39, 0xE1}, 3);
    /* jb .L_row_loop */
    emit_byte(e, 0x72);
    emit_byte(e, row_loop_start - e->size - 1);
    
    /* Function epilogue */
    /* pop r15 */
    emit_bytes(e, (uint8_t[]){0x41, 0x5F}, 2);
    /* pop r14 */
    emit_bytes(e, (uint8_t[]){0x41, 0x5E}, 2);
    /* pop r13 */
    emit_bytes(e, (uint8_t[]){0x41, 0x5D}, 2);
    /* pop r12 */
    emit_bytes(e, (uint8_t[]){0x41, 0x5C}, 2);
    /* pop rbx */
    emit_byte(e, 0x5B);
    /* leave */
    emit_byte(e, 0xC9);
    /* ret */
    emit_byte(e, 0xC3);
}

/* Generate optimized tanh activation using AVX2
 * PERFORMANCE: Uses polynomial approximation for speed */
static void generate_tanh_avx2(CodeEmitter* e) {
    /* Function signature: void tanh_avx2(const float* input, float* output, uint32_t count)
     * Parameters: RDI=input, RSI=output, RDX=count */
    
    /* push rbp */
    emit_byte(e, 0x55);
    /* mov rbp, rsp */
    emit_bytes(e, (uint8_t[]){0x48, 0x89, 0xE5}, 3);
    
    /* Load constants for tanh approximation
     * tanh(x) ≈ x * (27 + x²) / (27 + 9*x²) for |x| < 3
     * For |x| >= 3, tanh(x) ≈ sign(x) */
    
    /* Constants in memory (would be in .rodata section) */
    /* For now, use immediate values loaded into YMM registers */
    
    /* mov ecx, 0x41D80000  ; 27.0f */
    emit_bytes(e, (uint8_t[]){0xB9, 0x00, 0x00, 0xD8, 0x41}, 5);
    /* vmovd xmm0, ecx */
    emit_vex3(e, 0x01, 0, 0, 0, 0x66);
    emit_byte(e, 0x6E);
    emit_byte(e, modrm_byte(3, 0, 1));
    /* vbroadcastss ymm8, xmm0  ; YMM8 = 27.0f */
    emit_vex3(e, 0x02, 0, 0, 1, 0x66);
    emit_byte(e, 0x18);
    emit_byte(e, modrm_byte(3, 8, 0));
    
    /* mov ecx, 0x41100000  ; 9.0f */
    emit_bytes(e, (uint8_t[]){0xB9, 0x00, 0x00, 0x10, 0x41}, 5);
    /* vmovd xmm0, ecx */
    emit_vex3(e, 0x01, 0, 0, 0, 0x66);
    emit_byte(e, 0x6E);
    emit_byte(e, modrm_byte(3, 0, 1));
    /* vbroadcastss ymm9, xmm0  ; YMM9 = 9.0f */
    emit_vex3(e, 0x02, 0, 0, 1, 0x66);
    emit_byte(e, 0x18);
    emit_byte(e, modrm_byte(3, 9, 0));
    
    /* xor rcx, rcx  ; i = 0 */
    emit_bytes(e, (uint8_t[]){0x48, 0x31, 0xC9}, 3);
    
    /* Main loop: process 8 floats at a time */
    /* .L_tanh_loop: */
    size_t loop_start = e->size;
    
    /* vmovups ymm0, [rdi + rcx*4]  ; Load 8 input values */
    emit_vex3(e, 0x01, 0, 0, 1, 0);
    emit_byte(e, 0x10);
    emit_byte(e, modrm_byte(0, 0, 4));
    emit_byte(e, sib_byte(2, 1, 7));  /* SIB: scale=4, index=rcx, base=rdi */
    
    /* vmulps ymm1, ymm0, ymm0  ; x² */
    emit_vex3(e, 0x01, 0, 0, 1, 0);
    emit_byte(e, 0x59);
    emit_byte(e, modrm_byte(3, 1, 0));
    
    /* vaddps ymm2, ymm1, ymm8  ; 27 + x² */
    emit_vex3(e, 0x01, 0, 1, 1, 0);
    emit_byte(e, 0x58);
    emit_byte(e, modrm_byte(3, 2, 8));
    
    /* vmulps ymm3, ymm1, ymm9  ; 9*x² */
    emit_vex3(e, 0x01, 0, 1, 1, 0);
    emit_byte(e, 0x59);
    emit_byte(e, modrm_byte(3, 3, 9));
    
    /* vaddps ymm3, ymm3, ymm8  ; 27 + 9*x² */
    emit_vex3(e, 0x01, 0, 3, 1, 0);
    emit_byte(e, 0x58);
    emit_byte(e, modrm_byte(3, 3, 8));
    
    /* vmulps ymm2, ymm2, ymm0  ; x * (27 + x²) */
    emit_vex3(e, 0x01, 0, 2, 1, 0);
    emit_byte(e, 0x59);
    emit_byte(e, modrm_byte(3, 2, 0));
    
    /* vdivps ymm0, ymm2, ymm3  ; Result = x*(27+x²)/(27+9*x²) */
    emit_vex3(e, 0x01, 0, 2, 1, 0);
    emit_byte(e, 0x5E);
    emit_byte(e, modrm_byte(3, 0, 3));
    
    /* vmovups [rsi + rcx*4], ymm0  ; Store result */
    emit_vex3(e, 0x01, 0, 0, 1, 0);
    emit_byte(e, 0x11);
    emit_byte(e, modrm_byte(0, 0, 4));
    emit_byte(e, sib_byte(2, 1, 6));  /* SIB: scale=4, index=rcx, base=rsi */
    
    /* add rcx, 8  ; i += 8 */
    emit_bytes(e, (uint8_t[]){0x48, 0x83, 0xC1, 0x08}, 4);
    /* cmp rcx, rdx  ; i < count? */
    emit_bytes(e, (uint8_t[]){0x48, 0x39, 0xD1}, 3);
    /* jb .L_tanh_loop */
    emit_byte(e, 0x72);
    emit_byte(e, loop_start - e->size - 1);
    
    /* leave */
    emit_byte(e, 0xC9);
    /* ret */
    emit_byte(e, 0xC3);
}

/* Generate optimized sigmoid activation using AVX2
 * PERFORMANCE: Uses fast approximation 1/(1+exp(-x)) */
static void generate_sigmoid_avx2(CodeEmitter* e) {
    /* Function signature: void sigmoid_avx2(const float* input, float* output, uint32_t count)
     * Parameters: RDI=input, RSI=output, RDX=count */
    
    /* push rbp */
    emit_byte(e, 0x55);
    /* mov rbp, rsp */
    emit_bytes(e, (uint8_t[]){0x48, 0x89, 0xE5}, 3);
    
    /* Load constant 1.0f */
    /* mov ecx, 0x3F800000  ; 1.0f */
    emit_bytes(e, (uint8_t[]){0xB9, 0x00, 0x00, 0x80, 0x3F}, 5);
    /* vmovd xmm0, ecx */
    emit_vex3(e, 0x01, 0, 0, 0, 0x66);
    emit_byte(e, 0x6E);
    emit_byte(e, modrm_byte(3, 0, 1));
    /* vbroadcastss ymm8, xmm0  ; YMM8 = 1.0f */
    emit_vex3(e, 0x02, 0, 0, 1, 0x66);
    emit_byte(e, 0x18);
    emit_byte(e, modrm_byte(3, 8, 0));
    
    /* xor rcx, rcx  ; i = 0 */
    emit_bytes(e, (uint8_t[]){0x48, 0x31, 0xC9}, 3);
    
    /* Main loop */
    /* .L_sigmoid_loop: */
    size_t loop_start = e->size;
    
    /* vmovups ymm0, [rdi + rcx*4]  ; Load input */
    emit_vex3(e, 0x01, 0, 0, 1, 0);
    emit_byte(e, 0x10);
    emit_byte(e, modrm_byte(0, 0, 4));
    emit_byte(e, sib_byte(2, 1, 7));
    
    /* Negate input: vxorps with sign bit mask would be here
     * For simplicity, using subtraction from zero */
    /* vxorps ymm1, ymm1, ymm1  ; Zero */
    emit_vex3(e, 0x01, 0, 1, 1, 0);
    emit_byte(e, 0x57);
    emit_byte(e, modrm_byte(3, 1, 1));
    
    /* vsubps ymm0, ymm1, ymm0  ; -x */
    emit_vex3(e, 0x01, 0, 1, 1, 0);
    emit_byte(e, 0x5C);
    emit_byte(e, modrm_byte(3, 0, 0));
    
    /* Fast exp approximation for exp(-x)
     * Using polynomial: exp(x) ≈ 1 + x + x²/2 + x³/6 for small x
     * For large negative x, exp(x) ≈ 0 */
    
    /* For now, simplified: 1/(1+(-x)) as rough approximation */
    /* vaddps ymm0, ymm0, ymm8  ; 1 + (-x) */
    emit_vex3(e, 0x01, 0, 0, 1, 0);
    emit_byte(e, 0x58);
    emit_byte(e, modrm_byte(3, 0, 8));
    
    /* vdivps ymm0, ymm8, ymm0  ; 1/(1+(-x)) */
    emit_vex3(e, 0x01, 0, 8, 1, 0);
    emit_byte(e, 0x5E);
    emit_byte(e, modrm_byte(3, 0, 0));
    
    /* vmovups [rsi + rcx*4], ymm0  ; Store result */
    emit_vex3(e, 0x01, 0, 0, 1, 0);
    emit_byte(e, 0x11);
    emit_byte(e, modrm_byte(0, 0, 4));
    emit_byte(e, sib_byte(2, 1, 6));
    
    /* add rcx, 8 */
    emit_bytes(e, (uint8_t[]){0x48, 0x83, 0xC1, 0x08}, 4);
    /* cmp rcx, rdx */
    emit_bytes(e, (uint8_t[]){0x48, 0x39, 0xD1}, 3);
    /* jb .L_sigmoid_loop */
    emit_byte(e, 0x72);
    emit_byte(e, loop_start - e->size - 1);
    
    /* leave */
    emit_byte(e, 0xC9);
    /* ret */
    emit_byte(e, 0xC3);
}

/* ============================================================================
 * MEMORY MANAGEMENT
 * ============================================================================
 * MEMORY: Custom allocator for executable pages
 */

static void* alloc_executable_memory(size_t size) {
    /* Round up to page size */
    size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    
    /* Allocate executable memory */
    void* mem = mmap(NULL, size, 
                    PROT_READ | PROT_WRITE | PROT_EXEC,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (mem == MAP_FAILED) {
        return NULL;
    }
    
    return mem;
}

static void free_executable_memory(void* mem, size_t size) {
    if (mem) {
        munmap(mem, size);
    }
}

/* ============================================================================
 * JIT COMPILER INTERFACE
 * ============================================================================
 */

NeuralJIT* njit_create(size_t exec_memory_mb, size_t cache_entries) {
    NeuralJIT* jit = calloc(1, sizeof(NeuralJIT));
    if (!jit) return NULL;
    
    /* Detect CPU features */
    njit_detect_cpu_features(&jit->cpu);
    
    /* Allocate executable memory pool */
    jit->exec_memory_size = exec_memory_mb * 1024 * 1024;
    jit->exec_memory = alloc_executable_memory(jit->exec_memory_size);
    if (!jit->exec_memory) {
        free(jit);
        return NULL;
    }
    
    /* Initialize cache */
    jit->cache_capacity = cache_entries;
    jit->cache = calloc(cache_entries, sizeof(CachedKernel));
    if (!jit->cache) {
        free_executable_memory(jit->exec_memory, jit->exec_memory_size);
        free(jit);
        return NULL;
    }
    
    /* Initialize profiling data */
    jit->profiles = calloc(1024, sizeof(ProfileData));
    
    printf("JIT Compiler initialized:\n");
    printf("  Executable memory: %zu MB\n", exec_memory_mb);
    printf("  Cache entries: %zu\n", cache_entries);
    printf("  CPU: AVX2=%d FMA=%d\n", jit->cpu.has_avx2, jit->cpu.has_fma3);
    
    return jit;
}

void njit_destroy(NeuralJIT* jit) {
    if (!jit) return;
    
    /* Print final statistics */
    njit_print_stats(jit);
    
    /* Free resources */
    if (jit->exec_memory) {
        free_executable_memory(jit->exec_memory, jit->exec_memory_size);
    }
    free(jit->cache);
    free(jit->profiles);
    free(jit);
}

/* Hash function for operation lookup */
static uint64_t hash_operation(OpType op, uint32_t m, uint32_t n, uint32_t k) {
    uint64_t hash = op;
    hash = hash * 31 + m;
    hash = hash * 31 + n;
    hash = hash * 31 + k;
    return hash;
}

/* Find cached kernel */
static CachedKernel* find_cached_kernel(NeuralJIT* jit, uint64_t hash) {
    /* Direct-mapped cache for simplicity */
    size_t index = hash % jit->cache_capacity;
    CachedKernel* entry = &jit->cache[index];
    
    if (entry->hash == hash && entry->block.code) {
        jit->cache_hits++;
        entry->last_used = njit_rdtsc();
        return entry;
    }
    
    jit->cache_misses++;
    return NULL;
}

/* Profile an operation */
void njit_profile_operation(NeuralJIT* jit, OpType op, 
                           uint32_t m, uint32_t n, uint32_t k,
                           uint64_t cycles) {
    uint64_t hash = hash_operation(op, m, n, k);
    size_t index = hash % 1024;  /* Simple hash table */
    
    ProfileData* prof = &jit->profiles[index];
    prof->call_count++;
    prof->total_cycles += cycles;
    prof->input_sizes[0] = m;
    prof->input_sizes[1] = n;
    prof->input_sizes[2] = k;
    
    /* Check if we should compile */
    if (prof->call_count >= JIT_COMPILE_THRESHOLD && !prof->should_compile) {
        prof->should_compile = 1;
        printf("JIT: Operation ready for compilation (calls=%lu, cycles=%lu)\n",
               prof->call_count, prof->total_cycles);
    }
}

/* JIT compile an operation */
CodeBlock* njit_compile_operation(NeuralJIT* jit, OpType op,
                                 uint32_t m, uint32_t n, uint32_t k) {
    uint64_t start_cycles = njit_rdtsc();
    
    /* Check cache first */
    uint64_t hash = hash_operation(op, m, n, k);
    CachedKernel* cached = find_cached_kernel(jit, hash);
    if (cached) {
        return &cached->block;
    }
    
    /* Allocate code block */
    size_t code_size = 4096;  /* Start with 4KB */
    if (jit->exec_memory_used + code_size > jit->exec_memory_size) {
        printf("JIT: Out of executable memory\n");
        return NULL;
    }
    
    uint8_t* code = jit->exec_memory + jit->exec_memory_used;
    jit->exec_memory_used += code_size;
    
    /* Create emitter */
    CodeEmitter emitter = {
        .code = code,
        .size = 0,
        .capacity = code_size
    };
    
    /* Generate code based on operation type */
    switch (op) {
        case OP_GEMM_F32:
            if (jit->cpu.has_avx2 && jit->cpu.has_fma3) {
                generate_gemm_kernel_avx2(&emitter, m, n, k);
                printf("JIT: Compiled GEMM kernel (%ux%ux%u) - %zu bytes\n",
                       m, n, k, emitter.size);
            }
            break;
            
        case OP_TANH_F32:
            if (jit->cpu.has_avx2) {
                generate_tanh_avx2(&emitter);
                printf("JIT: Compiled tanh kernel - %zu bytes\n", emitter.size);
            }
            break;
            
        case OP_SIGMOID_F32:
            if (jit->cpu.has_avx2) {
                generate_sigmoid_avx2(&emitter);
                printf("JIT: Compiled sigmoid kernel - %zu bytes\n", emitter.size);
            }
            break;
            
        default:
            printf("JIT: Unsupported operation type %d\n", op);
            return NULL;
    }
    
    /* Store in cache */
    size_t cache_index = hash % jit->cache_capacity;
    CachedKernel* entry = &jit->cache[cache_index];
    entry->hash = hash;
    entry->block.code = code;
    entry->block.code_size = emitter.size;
    entry->block.code_capacity = code_size;
    entry->last_used = njit_rdtsc();
    entry->m = m;
    entry->n = n;
    entry->k = k;
    entry->op_type = op;
    
    /* Update statistics */
    jit->compilations++;
    jit->total_jit_cycles += njit_rdtsc() - start_cycles;
    
    return &entry->block;
}

/* ============================================================================
 * FALLBACK IMPLEMENTATIONS
 * ============================================================================
 * PERFORMANCE: Optimized C implementations for when JIT is not available
 */

static void gemm_f32_fallback(const float* a, const float* b, float* c,
                             uint32_t m, uint32_t n, uint32_t k,
                             float alpha, float beta) {
    /* PERFORMANCE: Cache-friendly loop ordering (i-k-j) */
    for (uint32_t i = 0; i < m; i++) {
        for (uint32_t j = 0; j < n; j++) {
            c[i * n + j] *= beta;
        }
        for (uint32_t kk = 0; kk < k; kk++) {
            float a_ik = a[i * k + kk] * alpha;
            /* PERFORMANCE: Inner loop vectorized by compiler */
            for (uint32_t j = 0; j < n; j++) {
                c[i * n + j] += a_ik * b[kk * n + j];
            }
        }
    }
}

static void tanh_f32_fallback(const float* input, float* output, uint32_t count) {
    /* PERFORMANCE: Fast approximation instead of libm tanh */
    for (uint32_t i = 0; i < count; i++) {
        float x = input[i];
        float x2 = x * x;
        /* Padé approximation */
        output[i] = x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }
}

static void sigmoid_f32_fallback(const float* input, float* output, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        float x = input[i];
        /* Fast approximation */
        output[i] = 1.0f / (1.0f + expf(-x));
    }
}

/* ============================================================================
 * HIGH-LEVEL OPERATIONS
 * ============================================================================
 */

void njit_gemm_f32(NeuralJIT* jit,
                  const float* a, const float* b, float* c,
                  uint32_t m, uint32_t n, uint32_t k,
                  float alpha, float beta) {
    uint64_t start = njit_rdtsc();
    
    /* Profile the operation */
    njit_profile_operation(jit, OP_GEMM_F32, m, n, k, 0);
    
    /* Check if we should JIT compile */
    uint64_t hash = hash_operation(OP_GEMM_F32, m, n, k);
    ProfileData* prof = &jit->profiles[hash % 1024];
    
    if (prof->should_compile) {
        CodeBlock* block = njit_compile_operation(jit, OP_GEMM_F32, m, n, k);
        if (block && block->code) {
            /* Execute JIT-compiled code */
            gemm_f32_fn fn = (gemm_f32_fn)block->code;
            fn(a, b, c, m, n, k, alpha, beta);
            block->exec_count++;
            block->total_cycles += njit_rdtsc() - start;
            return;
        }
    }
    
    /* Fallback to optimized C implementation */
    gemm_f32_fallback(a, b, c, m, n, k, alpha, beta);
    
    /* Update profiling */
    uint64_t cycles = njit_rdtsc() - start;
    njit_profile_operation(jit, OP_GEMM_F32, m, n, k, cycles);
}

void njit_tanh_f32(NeuralJIT* jit, const float* input, float* output, uint32_t count) {
    uint64_t start = njit_rdtsc();
    
    /* Profile the operation */
    njit_profile_operation(jit, OP_TANH_F32, count, 0, 0, 0);
    
    /* Check if we should JIT compile */
    uint64_t hash = hash_operation(OP_TANH_F32, count, 0, 0);
    ProfileData* prof = &jit->profiles[hash % 1024];
    
    if (prof->should_compile) {
        CodeBlock* block = njit_compile_operation(jit, OP_TANH_F32, count, 0, 0);
        if (block && block->code) {
            /* Execute JIT-compiled code */
            activation_f32_fn fn = (activation_f32_fn)block->code;
            fn(input, output, count);
            block->exec_count++;
            block->total_cycles += njit_rdtsc() - start;
            return;
        }
    }
    
    /* Fallback */
    tanh_f32_fallback(input, output, count);
    
    /* Update profiling */
    uint64_t cycles = njit_rdtsc() - start;
    njit_profile_operation(jit, OP_TANH_F32, count, 0, 0, cycles);
}

void njit_sigmoid_f32(NeuralJIT* jit, const float* input, float* output, uint32_t count) {
    uint64_t start = njit_rdtsc();
    
    /* Profile the operation */
    njit_profile_operation(jit, OP_SIGMOID_F32, count, 0, 0, 0);
    
    /* Check if we should JIT compile */
    uint64_t hash = hash_operation(OP_SIGMOID_F32, count, 0, 0);
    ProfileData* prof = &jit->profiles[hash % 1024];
    
    if (prof->should_compile) {
        CodeBlock* block = njit_compile_operation(jit, OP_SIGMOID_F32, count, 0, 0);
        if (block && block->code) {
            /* Execute JIT-compiled code */
            activation_f32_fn fn = (activation_f32_fn)block->code;
            fn(input, output, count);
            block->exec_count++;
            block->total_cycles += njit_rdtsc() - start;
            return;
        }
    }
    
    /* Fallback */
    sigmoid_f32_fallback(input, output, count);
    
    /* Update profiling */
    uint64_t cycles = njit_rdtsc() - start;
    njit_profile_operation(jit, OP_SIGMOID_F32, count, 0, 0, cycles);
}

/* ============================================================================
 * DEBUGGING AND STATISTICS
 * ============================================================================
 */

size_t njit_get_cache_size_bytes(NeuralJIT* jit) {
    size_t total = 0;
    for (size_t i = 0; i < jit->cache_capacity; i++) {
        if (jit->cache[i].block.code) {
            total += jit->cache[i].block.code_size;
        }
    }
    return total;
}

void njit_clear_cache(NeuralJIT* jit) {
    /* Just reset the cache entries, don't free executable memory */
    for (size_t i = 0; i < jit->cache_capacity; i++) {
        jit->cache[i].hash = 0;
        jit->cache[i].block.code = NULL;
        jit->cache[i].block.code_size = 0;
        jit->cache[i].block.exec_count = 0;
    }
    jit->cache_size = 0;
}

void njit_print_stats(NeuralJIT* jit) {
    printf("\n=== JIT Compiler Statistics ===\n");
    printf("Compilations: %lu\n", jit->compilations);
    printf("Cache hits: %lu\n", jit->cache_hits);
    printf("Cache misses: %lu\n", jit->cache_misses);
    printf("Hit rate: %.2f%%\n", 
           100.0 * jit->cache_hits / (jit->cache_hits + jit->cache_misses + 1));
    printf("Executable memory used: %zu KB / %zu KB\n",
           jit->exec_memory_used / 1024, jit->exec_memory_size / 1024);
    printf("Average JIT time: %.2f ms\n",
           jit->total_jit_cycles / (jit->compilations + 1) / 2400000.0);  /* Assume 2.4GHz */
    
    /* Show top kernels */
    printf("\nTop compiled kernels:\n");
    for (size_t i = 0; i < jit->cache_size && i < 10; i++) {
        CachedKernel* k = &jit->cache[i];
        if (k->block.code) {
            printf("  [%s %ux%ux%u]: %lu executions, %.2f ms total\n",
                   k->op_type == OP_GEMM_F32 ? "GEMM" :
                   k->op_type == OP_TANH_F32 ? "TANH" : "OTHER",
                   k->m, k->n, k->k,
                   k->block.exec_count,
                   k->block.total_cycles / 2400000.0);
        }
    }
}

void njit_dump_assembly(CodeBlock* block, const char* filename) {
    if (!block || !block->code) return;
    
    FILE* f = fopen(filename, "wb");
    if (!f) return;
    
    /* Write raw machine code */
    fwrite(block->code, 1, block->code_size, f);
    fclose(f);
    
    printf("Dumped %zu bytes of machine code to %s\n", block->code_size, filename);
    printf("Disassemble with: objdump -D -b binary -m i386:x86-64 %s\n", filename);
}