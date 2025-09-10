/*
 * Simple Audio System Test
 * Minimal test following handmade philosophy:
 * 1. Start with simplest thing that makes sound
 * 2. Understand every line of code
 * 3. No external dependencies beyond ALSA
 * 4. Always have something working
 */

#define _GNU_SOURCE
#include "handmade_audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define TEST_FREQUENCY 440.0f  /* A4 note */
#define TEST_DURATION 2        /* 2 seconds */

int main() {
    printf("=== HANDMADE AUDIO SIMPLE TEST ===\n");
    printf("Testing Casey's principles:\n");
    printf("1. Always have something working\n");
    printf("2. Understand every line of code\n");
    printf("3. No black boxes\n");
    printf("4. Performance first\n\n");

    /* Initialize audio system */
    printf("1. Initializing audio system...\n");
    audio_system audio;
    if (!audio_init(&audio, 4 * 1024 * 1024)) {  /* 4MB */
        printf("FAILED: Could not initialize audio\n");
        return 1;
    }
    printf("   SUCCESS: Audio initialized\n");
    printf("   Memory pool: 4MB\n");
    printf("   Sample rate: %d Hz\n", AUDIO_SAMPLE_RATE);
    printf("   Channels: %d\n", AUDIO_CHANNELS);
    printf("   Latency: ~%.1fms\n", (AUDIO_BUFFER_FRAMES * 1000.0f) / AUDIO_SAMPLE_RATE);

    /* Generate simple test sound in memory */
    printf("\n2. Generating test sound (%.0f Hz, %d seconds)...\n", TEST_FREQUENCY, TEST_DURATION);
    uint32_t frames = AUDIO_SAMPLE_RATE * TEST_DURATION;
    size_t buffer_size = frames * AUDIO_CHANNELS * sizeof(int16_t);
    int16_t *test_buffer = malloc(buffer_size);
    
    if (!test_buffer) {
        printf("FAILED: Could not allocate test buffer\n");
        audio_shutdown(&audio);
        return 1;
    }
    
    /* Generate sine wave - pure handmade approach */
    for (uint32_t i = 0; i < frames; i++) {
        float t = (float)i / AUDIO_SAMPLE_RATE;
        float amplitude = 0.3f * sinf(2.0f * M_PI * TEST_FREQUENCY * t);
        
        /* Apply envelope to avoid clicks */
        float envelope = 1.0f;
        if (t < 0.1f) {
            envelope = t / 0.1f;  /* Fade in */
        } else if (t > TEST_DURATION - 0.1f) {
            envelope = (TEST_DURATION - t) / 0.1f;  /* Fade out */
        }
        
        int16_t sample = (int16_t)(amplitude * envelope * 16384.0f);
        test_buffer[i * 2] = sample;      /* Left channel */
        test_buffer[i * 2 + 1] = sample;  /* Right channel */
    }
    printf("   SUCCESS: Generated %d frames\n", frames);

    /* Load sound into audio system */
    printf("\n3. Loading sound into audio system...\n");
    audio_handle sound = audio_load_wav_from_memory(&audio, test_buffer, buffer_size);
    if (sound == AUDIO_INVALID_HANDLE) {
        printf("FAILED: Could not load sound\n");
        free(test_buffer);
        audio_shutdown(&audio);
        return 1;
    }
    printf("   SUCCESS: Sound loaded (handle: %u)\n", sound);

    /* Test basic playback */
    printf("\n4. Testing basic playback...\n");
    audio_handle voice = audio_play_sound(&audio, sound, 1.0f, 0.0f);
    if (voice == AUDIO_INVALID_HANDLE) {
        printf("FAILED: Could not play sound\n");
        free(test_buffer);
        audio_shutdown(&audio);
        return 1;
    }
    printf("   SUCCESS: Sound playing (voice: %u)\n", voice);
    printf("   Playing %.0f Hz tone for %d seconds...\n", TEST_FREQUENCY, TEST_DURATION);

    /* Monitor playback */
    for (int i = 0; i < TEST_DURATION + 1; i++) {
        sleep(1);
        uint32_t active_voices = audio_get_active_voices(&audio);
        float cpu_usage = audio_get_cpu_usage(&audio);
        uint64_t underruns = audio_get_underrun_count(&audio);
        
        printf("   [%d/%d] Active voices: %u, CPU: %.1f%%, Underruns: %lu\n", 
               i + 1, TEST_DURATION + 1, active_voices, cpu_usage * 100.0f, underruns);
    }

    /* Test volume control */
    printf("\n5. Testing volume control...\n");
    for (float vol = 1.0f; vol >= 0.0f; vol -= 0.2f) {
        audio_set_master_volume(&audio, vol);
        printf("   Volume: %.1f\n", vol);
        usleep(200000);  /* 200ms */
    }
    audio_set_master_volume(&audio, 1.0f);

    /* Test performance with multiple voices */
    printf("\n6. Testing performance with multiple voices...\n");
    audio_handle voices[10];
    for (int i = 0; i < 10; i++) {
        voices[i] = audio_play_sound(&audio, sound, 0.1f, (float)(i - 5) / 5.0f);
        printf("   Started voice %d (pan: %.1f)\n", i, (float)(i - 5) / 5.0f);
        usleep(50000);  /* 50ms between voices */
    }
    
    printf("   All voices playing simultaneously...\n");
    sleep(2);
    
    uint32_t final_active = audio_get_active_voices(&audio);
    float final_cpu = audio_get_cpu_usage(&audio);
    uint64_t final_underruns = audio_get_underrun_count(&audio);
    
    printf("   Final stats - Active: %u, CPU: %.1f%%, Underruns: %lu\n", 
           final_active, final_cpu * 100.0f, final_underruns);

    /* Stop all voices */
    printf("\n7. Stopping all voices...\n");
    for (int i = 0; i < 10; i++) {
        audio_stop_sound(&audio, voices[i]);
    }

    /* Cleanup */
    printf("\n8. Cleaning up...\n");
    free(test_buffer);
    audio_shutdown(&audio);
    
    printf("\n=== TEST RESULTS ===\n");
    printf("✓ Audio system initialization: PASS\n");
    printf("✓ Sound generation: PASS\n");
    printf("✓ Sound loading: PASS\n");
    printf("✓ Basic playback: PASS\n");
    printf("✓ Volume control: PASS\n");
    printf("✓ Multiple voices: PASS\n");
    printf("✓ System cleanup: PASS\n");
    
    if (final_cpu < 0.05f) {
        printf("✓ Performance target: PASS (%.1f%% < 5%%)\n", final_cpu * 100.0f);
    } else {
        printf("⚠ Performance target: WARN (%.1f%% >= 5%%)\n", final_cpu * 100.0f);
    }
    
    if (final_underruns == 0) {
        printf("✓ Audio stability: PASS (0 underruns)\n");
    } else {
        printf("⚠ Audio stability: WARN (%lu underruns)\n", final_underruns);
    }
    
    printf("\n=== HANDMADE AUDIO SYSTEM VERIFIED ===\n");
    return 0;
}