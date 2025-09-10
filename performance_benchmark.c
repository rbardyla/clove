/*
 * Performance benchmark for minimal engine
 * Tests rendering performance without graphics context
 */

#define _POSIX_C_SOURCE 199309L
#include "handmade_platform.h"
#include "handmade_renderer.h" 
#include "handmade_gui.h"
#include <stdio.h>
#include <time.h>
#include <math.h>

#define TEST_ITERATIONS 10000
#define TARGET_FPS 60.0f
#define TARGET_FRAME_TIME (1.0f / TARGET_FPS)

static double get_time_seconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void benchmark_math_operations() {
    printf("=== MATH OPERATIONS BENCHMARK ===\n");
    
    double start = get_time_seconds();
    
    // Test vector operations
    v2 vectors[1000];
    for (int i = 0; i < 1000; i++) {
        vectors[i] = V2((float)i * 0.1f, (float)i * 0.2f);
    }
    
    // Test color operations 
    Color colors[1000];
    for (int i = 0; i < 1000; i++) {
        float t = (float)i / 1000.0f;
        colors[i] = COLOR(sinf(t), cosf(t), t, 1.0f);
    }
    
    // Test sprite setup operations
    Sprite sprites[100];
    for (int i = 0; i < 100; i++) {
        sprites[i].position = vectors[i];
        sprites[i].size = V2(1.0f, 1.0f);
        sprites[i].rotation = (float)i * 0.1f;
        sprites[i].color = colors[i];
        sprites[i].texture_offset = V2(0.0f, 0.0f);
        sprites[i].texture_scale = V2(1.0f, 1.0f);
    }
    
    double end = get_time_seconds();
    double math_time = end - start;
    
    printf("Math operations time: %.4f ms\n", math_time * 1000.0);
    printf("Math operations per second: %.0f\n", TEST_ITERATIONS / math_time);
}

static void benchmark_string_operations() {
    printf("\n=== STRING OPERATIONS BENCHMARK ===\n");
    
    double start = get_time_seconds();
    
    char buffers[100][256];
    for (int i = 0; i < 100; i++) {
        snprintf(buffers[i], 256, "Debug text %d: %.2f, %.2f", 
                i, (float)i * 0.5f, (float)i * 0.3f);
    }
    
    double end = get_time_seconds();
    double string_time = end - start;
    
    printf("String operations time: %.4f ms\n", string_time * 1000.0);
    printf("Strings per second: %.0f\n", 100.0 / string_time);
}

static void benchmark_camera_operations() {
    printf("\n=== CAMERA OPERATIONS BENCHMARK ===\n");
    
    double start = get_time_seconds();
    
    Camera2D cameras[1000];
    for (int i = 0; i < 1000; i++) {
        Camera2DInit(&cameras[i], 16.0f / 9.0f);
        cameras[i].position.x = (float)i * 0.1f;
        cameras[i].position.y = (float)i * 0.05f;
        cameras[i].zoom = 1.0f + (float)i * 0.001f;
        cameras[i].rotation = (float)i * 0.01f;
    }
    
    double end = get_time_seconds();
    double camera_time = end - start;
    
    printf("Camera operations time: %.4f ms\n", camera_time * 1000.0);
    printf("Camera operations per second: %.0f\n", 1000.0 / camera_time);
}

static void benchmark_gui_data_structures() {
    printf("\n=== GUI DATA STRUCTURES BENCHMARK ===\n");
    
    double start = get_time_seconds();
    
    HandmadeGUI guis[100];
    for (int i = 0; i < 100; i++) {
        guis[i].initialized = false;
        guis[i].mouse_position = V2((float)i, (float)i * 0.5f);
        guis[i].hot_id = (u64)i;
        guis[i].active_id = (u64)i + 1000;
        guis[i].cursor = V2((float)i * 10.0f, (float)i * 5.0f);
        guis[i].line_height = 20.0f;
        guis[i].text_color = COLOR(1.0f, 1.0f, 1.0f, 1.0f);
        guis[i].button_color = COLOR(0.3f, 0.3f, 0.3f, 1.0f);
        guis[i].widgets_drawn = (u32)i;
    }
    
    double end = get_time_seconds();
    double gui_time = end - start;
    
    printf("GUI data structure operations time: %.4f ms\n", gui_time * 1000.0);
    printf("GUI operations per second: %.0f\n", 100.0 / gui_time);
}

static void estimate_frame_performance() {
    printf("\n=== FRAME PERFORMANCE ESTIMATION ===\n");
    
    double start = get_time_seconds();
    
    // Simulate typical frame operations
    for (int frame = 0; frame < 1000; frame++) {
        // Simulate game state updates
        f32 dt = 1.0f / 60.0f;
        f32 time = (f32)frame * dt;
        
        // Simulate typical objects in a frame
        for (int obj = 0; obj < 50; obj++) {
            v2 pos = V2(sinf(time + obj), cosf(time + obj));
            Color color = COLOR(sinf(time + obj * 0.1f), 
                               cosf(time + obj * 0.2f), 
                               sinf(time + obj * 0.3f), 1.0f);
            
            // Simulate sprite setup (what renderer would do)
            Sprite sprite = {
                .position = pos,
                .size = V2(0.1f, 0.1f),
                .rotation = time + obj,
                .color = color,
                .texture_offset = V2(0.0f, 0.0f),
                .texture_scale = V2(1.0f, 1.0f)
            };
        }
        
        // Simulate GUI operations
        char debug_text[256];
        snprintf(debug_text, sizeof(debug_text), "Frame %d: %.2f", frame, time);
    }
    
    double end = get_time_seconds();
    double total_time = end - start;
    double frame_time = total_time / 1000.0;
    double estimated_fps = 1.0 / frame_time;
    
    printf("Simulated 1000 frames in: %.4f seconds\n", total_time);
    printf("Average frame time: %.4f ms\n", frame_time * 1000.0);
    printf("Estimated FPS (without rendering): %.1f\n", estimated_fps);
    printf("Target frame time: %.4f ms\n", TARGET_FRAME_TIME * 1000.0);
    
    if (frame_time < TARGET_FRAME_TIME) {
        printf("✓ PERFORMANCE: CPU overhead can support 60+ FPS\n");
        printf("  Frame time budget remaining: %.4f ms\n", 
               (TARGET_FRAME_TIME - frame_time) * 1000.0);
    } else {
        printf("✗ WARNING: CPU overhead may limit 60+ FPS\n");
        printf("  Overhead exceeds budget by: %.4f ms\n", 
               (frame_time - TARGET_FRAME_TIME) * 1000.0);
    }
}

int main() {
    printf("=== HANDMADE ENGINE PERFORMANCE BENCHMARK ===\n");
    printf("Testing CPU-side performance without GPU rendering\n");
    printf("Target: 60+ FPS (16.67ms frame budget)\n\n");
    
    benchmark_math_operations();
    benchmark_string_operations();
    benchmark_camera_operations();
    benchmark_gui_data_structures();
    estimate_frame_performance();
    
    printf("\n=== BENCHMARK COMPLETE ===\n");
    printf("Note: This measures CPU overhead only. GPU rendering would add additional cost.\n");
    printf("For real 60+ FPS validation, run the actual engine with graphics.\n");
    
    return 0;
}