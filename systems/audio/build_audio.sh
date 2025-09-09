#!/bin/bash

# Handmade Audio System Build Script
# Compiles the audio system with ALSA and SIMD optimizations

set -e  # Exit on error

echo "====================================="
echo "Building Handmade Audio System"
echo "====================================="

# Compiler settings
CC=gcc
CFLAGS="-O3 -march=native -mtune=native -Wall -Wextra -std=c99"
CFLAGS="$CFLAGS -mavx2 -msse4.2 -mfma"  # SIMD instructions
CFLAGS="$CFLAGS -ffast-math -funroll-loops -fprefetch-loop-arrays"
CFLAGS="$CFLAGS -D_GNU_SOURCE"  # For Linux-specific features

# Debug build option
if [ "$1" == "debug" ]; then
    echo "Building in DEBUG mode..."
    CFLAGS="-g -O0 -DDEBUG -Wall -Wextra -std=c99 -march=native"
    CFLAGS="$CFLAGS -fsanitize=address -fsanitize=undefined"
fi

# Performance build option
if [ "$1" == "perf" ]; then
    echo "Building for PERFORMANCE analysis..."
    CFLAGS="$CFLAGS -g -fno-omit-frame-pointer"
fi

# Libraries
LIBS="-lasound -lpthread -lm -lrt"

# Source files
AUDIO_SOURCES="handmade_audio.c audio_dsp.c"

# Create build directory
mkdir -p build

echo ""
echo "Compiler flags: $CFLAGS"
echo "Libraries: $LIBS"
echo ""

# Build audio library as static archive
echo "Building audio library..."
$CC $CFLAGS -c handmade_audio.c -o build/handmade_audio.o
$CC $CFLAGS -c audio_dsp.c -o build/audio_dsp.o

# Create static library
ar rcs build/libhandmade_audio.a build/handmade_audio.o build/audio_dsp.o
echo "Created: build/libhandmade_audio.a"

# Build demo application
echo ""
echo "Building audio demo..."
$CC $CFLAGS audio_demo.c build/libhandmade_audio.a $LIBS -o build/audio_demo

# Build integration test
echo ""
echo "Building audio tests..."
cat > audio_test.c << 'EOF'
/*
 * Audio System Unit Tests
 * Validates core functionality and performance
 */

#include "handmade_audio.h"
#include <stdio.h>
#include <assert.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <unistd.h>

/* Test initialization */
static void test_initialization() {
    printf("Testing audio initialization...\n");
    
    audio_system audio;
    bool result = audio_init(&audio, 8 * 1024 * 1024);  /* 8MB */
    assert(result == true);
    assert(audio.pcm_handle != NULL);
    assert(audio.ring_buffer != NULL);
    assert(audio.master_volume == 1.0f);
    
    audio_shutdown(&audio);
    printf("  PASSED\n");
}

/* Test sound loading and playback */
static void test_sound_playback() {
    printf("Testing sound loading and playback...\n");
    
    audio_system audio;
    audio_init(&audio, 8 * 1024 * 1024);
    
    /* Generate test sound */
    int16_t test_sound[48000];
    for (int i = 0; i < 48000; i++) {
        test_sound[i] = (int16_t)(sinf(2.0f * M_PI * 440.0f * i / 48000.0f) * 16000);
    }
    
    /* Load sound */
    audio_handle sound = audio_load_wav_from_memory(&audio, test_sound, sizeof(test_sound));
    assert(sound != AUDIO_INVALID_HANDLE);
    
    /* Play sound */
    audio_handle voice = audio_play_sound(&audio, sound, 1.0f, 0.0f);
    assert(voice != AUDIO_INVALID_HANDLE);
    
    /* Verify voice is active */
    uint32_t active = audio_get_active_voices(&audio);
    assert(active > 0);
    
    /* Stop sound */
    audio_stop_sound(&audio, voice);
    
    audio_shutdown(&audio);
    printf("  PASSED\n");
}

/* Test 3D audio calculations */
static void test_3d_audio() {
    printf("Testing 3D audio...\n");
    
    audio_system audio;
    audio_init(&audio, 8 * 1024 * 1024);
    
    /* Set listener position */
    audio_vec3 listener_pos = {0, 0, 0};
    audio_set_listener_position(&audio, listener_pos);
    
    /* Test distance calculation */
    audio_vec3 sound_pos = {3, 4, 0};
    float distance = audio_vec3_distance(listener_pos, sound_pos);
    assert(fabs(distance - 5.0f) < 0.001f);  /* 3-4-5 triangle */
    
    /* Test normalization */
    audio_vec3 vec = {3, 4, 0};
    vec = audio_vec3_normalize(vec);
    float length = sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
    assert(fabs(length - 1.0f) < 0.001f);
    
    audio_shutdown(&audio);
    printf("  PASSED\n");
}

/* Test voice stealing */
static void test_voice_stealing() {
    printf("Testing voice stealing...\n");
    
    audio_system audio;
    audio_init(&audio, 16 * 1024 * 1024);
    
    /* Create dummy sound */
    int16_t dummy[1000];
    memset(dummy, 0, sizeof(dummy));
    audio_handle sound = audio_load_wav_from_memory(&audio, dummy, sizeof(dummy));
    
    /* Fill all voice slots */
    for (int i = 0; i < AUDIO_MAX_VOICES + 10; i++) {
        audio_handle voice = audio_play_sound(&audio, sound, 0.1f, 0.0f);
        /* Should always return valid handle due to voice stealing */
        assert(voice != AUDIO_INVALID_HANDLE);
    }
    
    /* Should have exactly MAX_VOICES active */
    assert(audio_get_active_voices(&audio) <= AUDIO_MAX_VOICES);
    
    audio_shutdown(&audio);
    printf("  PASSED\n");
}

/* Performance benchmark */
static void benchmark_mixing() {
    printf("Benchmarking audio mixing...\n");
    
    audio_system audio;
    audio_init(&audio, 32 * 1024 * 1024);
    
    /* Create test sounds */
    int16_t *sounds[10];
    audio_handle handles[10];
    
    for (int i = 0; i < 10; i++) {
        sounds[i] = malloc(48000 * 2 * sizeof(int16_t));
        for (int j = 0; j < 48000 * 2; j++) {
            sounds[i][j] = (int16_t)(sinf(2.0f * M_PI * (220 + i * 50) * j / 48000.0f) * 8000);
        }
        handles[i] = audio_load_wav_from_memory(&audio, sounds[i], 48000 * 2 * sizeof(int16_t));
    }
    
    /* Start playing many voices */
    printf("  Starting 100 voices...\n");
    for (int i = 0; i < 100; i++) {
        audio_play_sound(&audio, handles[i % 10], 0.3f, (float)(i % 20 - 10) / 10.0f);
    }
    
    /* Let it run for a bit */
    sleep(1);
    
    /* Check performance */
    float cpu_usage = audio_get_cpu_usage(&audio);
    uint32_t active = audio_get_active_voices(&audio);
    uint64_t underruns = audio_get_underrun_count(&audio);
    
    printf("  Active voices: %d\n", active);
    printf("  CPU usage: %.1f%%\n", cpu_usage * 100.0f);
    printf("  Underruns: %lu\n", underruns);
    
    /* Performance targets */
    assert(cpu_usage < 0.10f);  /* Less than 10% CPU */
    assert(underruns == 0);      /* No underruns */
    
    /* Cleanup */
    for (int i = 0; i < 10; i++) {
        free(sounds[i]);
    }
    
    audio_shutdown(&audio);
    printf("  PASSED\n");
}

/* Test decibel conversions */
static void test_db_conversion() {
    printf("Testing dB conversions...\n");
    
    /* Test dB to linear */
    float linear = audio_db_to_linear(-6.0f);
    assert(fabs(linear - 0.5012f) < 0.001f);  /* -6dB = ~0.5 */
    
    linear = audio_db_to_linear(0.0f);
    assert(fabs(linear - 1.0f) < 0.001f);  /* 0dB = 1.0 */
    
    /* Test linear to dB */
    float db = audio_linear_to_db(0.5f);
    assert(fabs(db - (-6.021f)) < 0.1f);  /* 0.5 = ~-6dB */
    
    db = audio_linear_to_db(1.0f);
    assert(fabs(db - 0.0f) < 0.001f);  /* 1.0 = 0dB */
    
    printf("  PASSED\n");
}

int main() {
    printf("\n=== AUDIO SYSTEM UNIT TESTS ===\n\n");
    
    test_initialization();
    test_sound_playback();
    test_3d_audio();
    test_voice_stealing();
    test_db_conversion();
    benchmark_mixing();
    
    printf("\n=== ALL TESTS PASSED ===\n\n");
    
    return 0;
}
EOF

$CC $CFLAGS audio_test.c build/libhandmade_audio.a $LIBS -o build/audio_test

# Check if ALSA is available
echo ""
echo "Checking ALSA configuration..."
if which aplay > /dev/null 2>&1; then
    echo "ALSA is installed"
    aplay -l | head -n 5 || true
else
    echo "WARNING: ALSA tools not found. Audio may not work."
fi

# Summary
echo ""
echo "====================================="
echo "Build Complete!"
echo "====================================="
echo ""
echo "Generated files:"
echo "  build/libhandmade_audio.a - Static library"
echo "  build/audio_demo - Interactive demo"
echo "  build/audio_test - Unit tests"
echo ""
echo "Library size: $(du -h build/libhandmade_audio.a | cut -f1)"
echo "Demo size: $(du -h build/audio_demo | cut -f1)"
echo ""
echo "To run the demo:"
echo "  ./build/audio_demo"
echo ""
echo "To run tests:"
echo "  ./build/audio_test"
echo ""
echo "To use in your project:"
echo "  #include \"handmade_audio.h\""
echo "  Link with: -L./build -lhandmade_audio -lasound -lpthread -lm"
echo ""

# Make executable
chmod +x build/audio_demo
chmod +x build/audio_test