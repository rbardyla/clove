/*
 * Test program to verify audio integration
 * Generates and plays test sounds to confirm audio system is working
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <stdint.h>

typedef uint32_t u32;
typedef int16_t i16;
typedef float f32;

#include "systems/audio/handmade_audio.h"

// Generate a simple sine wave beep
static audio_handle GenerateTestSound(audio_system* audio, float frequency, float duration) {
    u32 sample_rate = AUDIO_SAMPLE_RATE;
    u32 frame_count = (u32)(duration * sample_rate);
    u32 size = frame_count * 2 * sizeof(int16_t); // Stereo
    
    int16_t* samples = (int16_t*)malloc(size);
    if (!samples) return AUDIO_INVALID_HANDLE;
    
    // Generate sine wave
    for (u32 i = 0; i < frame_count; i++) {
        float t = (float)i / sample_rate;
        float sample = sinf(2.0f * 3.14159f * frequency * t);
        
        // Apply envelope
        float envelope = 1.0f - (t / duration);
        sample *= envelope * 16384.0f;
        
        int16_t sample_int = (int16_t)sample;
        samples[i * 2] = sample_int;     // Left
        samples[i * 2 + 1] = sample_int; // Right
    }
    
    audio_handle handle = audio_load_wav_from_memory(audio, samples, size);
    free(samples);
    return handle;
}

int main() {
    printf("=== Audio Integration Test ===\n");
    
    // Initialize audio system
    audio_system audio = {0};
    size_t audio_memory = 8 * 1024 * 1024; // 8MB
    
    if (!audio_init(&audio, audio_memory)) {
        printf("ERROR: Failed to initialize audio system\n");
        printf("Make sure ALSA is installed and configured\n");
        return 1;
    }
    
    printf("Audio system initialized successfully\n");
    
    // Set volumes
    audio_set_master_volume(&audio, 0.8f);
    audio_set_sound_volume(&audio, 1.0f);
    
    // Generate test sounds
    printf("Generating test sounds...\n");
    audio_handle beep_low = GenerateTestSound(&audio, 200.0f, 0.2f);
    audio_handle beep_mid = GenerateTestSound(&audio, 400.0f, 0.15f);
    audio_handle beep_high = GenerateTestSound(&audio, 800.0f, 0.1f);
    
    if (beep_low == AUDIO_INVALID_HANDLE || 
        beep_mid == AUDIO_INVALID_HANDLE || 
        beep_high == AUDIO_INVALID_HANDLE) {
        printf("ERROR: Failed to generate test sounds\n");
        audio_shutdown(&audio);
        return 1;
    }
    
    printf("Test sounds generated\n");
    printf("\nPlaying test sequence...\n");
    
    // Play test sequence
    printf("Playing LOW beep (200Hz)...\n");
    audio_play_sound(&audio, beep_low, 0.7f, 0.0f);
    audio_update(&audio, 0.016f);
    usleep(300000);
    
    printf("Playing MID beep (400Hz)...\n");
    audio_play_sound(&audio, beep_mid, 0.7f, 0.0f);
    audio_update(&audio, 0.016f);
    usleep(300000);
    
    printf("Playing HIGH beep (800Hz)...\n");
    audio_play_sound(&audio, beep_high, 0.7f, 0.0f);
    audio_update(&audio, 0.016f);
    usleep(300000);
    
    // Play stereo pan test
    printf("\nPlaying stereo pan test...\n");
    printf("LEFT channel...\n");
    audio_play_sound(&audio, beep_mid, 0.5f, -1.0f);
    audio_update(&audio, 0.016f);
    usleep(300000);
    
    printf("CENTER...\n");
    audio_play_sound(&audio, beep_mid, 0.5f, 0.0f);
    audio_update(&audio, 0.016f);
    usleep(300000);
    
    printf("RIGHT channel...\n");
    audio_play_sound(&audio, beep_mid, 0.5f, 1.0f);
    audio_update(&audio, 0.016f);
    usleep(300000);
    
    // Check performance
    printf("\nAudio Statistics:\n");
    printf("  Active voices: %u\n", audio_get_active_voices(&audio));
    printf("  CPU usage: %.1f%%\n", audio_get_cpu_usage(&audio) * 100.0f);
    printf("  Underruns: %lu\n", audio_get_underrun_count(&audio));
    
    // Cleanup
    printf("\nShutting down audio system...\n");
    audio_shutdown(&audio);
    
    printf("\n=== Test Complete ===\n");
    printf("If you heard the beeps, audio integration is working!\n");
    
    return 0;
}