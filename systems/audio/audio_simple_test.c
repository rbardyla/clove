/*
 * Simple Audio Test
 * Minimal test to verify audio system works
 */

#include "handmade_audio.h"
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <string.h>

int main() {
    printf("Simple Audio Test\n");
    printf("=================\n\n");
    
    /* Initialize audio with 4MB */
    printf("Initializing audio system...\n");
    audio_system audio;
    if (!audio_init(&audio, 4 * 1024 * 1024)) {
        fprintf(stderr, "Failed to initialize audio\n");
        return 1;
    }
    
    printf("Audio initialized successfully!\n");
    printf("  Sample rate: %d Hz\n", AUDIO_SAMPLE_RATE);
    printf("  Channels: %d\n", AUDIO_CHANNELS);
    printf("  Latency: ~%d ms\n\n", (AUDIO_BUFFER_FRAMES * 1000) / AUDIO_SAMPLE_RATE);
    
    /* Generate a simple test tone */
    printf("Generating 440Hz test tone...\n");
    int16_t *tone = malloc(AUDIO_SAMPLE_RATE * 2 * sizeof(int16_t));
    for (int i = 0; i < AUDIO_SAMPLE_RATE; i++) {
        float sample = sinf(2.0f * M_PI * 440.0f * i / AUDIO_SAMPLE_RATE) * 10000.0f;
        tone[i * 2] = (int16_t)sample;      /* Left */
        tone[i * 2 + 1] = (int16_t)sample;  /* Right */
    }
    
    /* Load the tone */
    audio_handle sound = audio_load_wav_from_memory(&audio, tone, AUDIO_SAMPLE_RATE * 2 * sizeof(int16_t));
    if (sound == AUDIO_INVALID_HANDLE) {
        fprintf(stderr, "Failed to load sound\n");
        audio_shutdown(&audio);
        free(tone);
        return 1;
    }
    
    printf("Sound loaded (handle: %d)\n\n", sound);
    
    /* Play the tone */
    printf("Playing test tone for 2 seconds...\n");
    audio_handle voice = audio_play_sound(&audio, sound, 0.5f, 0.0f);
    if (voice == AUDIO_INVALID_HANDLE) {
        fprintf(stderr, "Failed to play sound\n");
    } else {
        printf("Sound playing (voice: %08x)\n", voice);
    }
    
    /* Let it play for 2 seconds */
    for (int i = 0; i < 20; i++) {
        usleep(100000);  /* 100ms */
        printf("\rCPU: %.1f%%, Voices: %d, Underruns: %lu    ",
               audio_get_cpu_usage(&audio) * 100.0f,
               audio_get_active_voices(&audio),
               audio_get_underrun_count(&audio));
        fflush(stdout);
    }
    printf("\n\n");
    
    /* Test volume control */
    printf("Testing volume control...\n");
    for (float vol = 1.0f; vol >= 0.0f; vol -= 0.2f) {
        audio_set_master_volume(&audio, vol);
        printf("  Volume: %.0f%%\n", vol * 100.0f);
        usleep(200000);
    }
    
    /* Test panning */
    printf("\nTesting panning...\n");
    audio_set_master_volume(&audio, 0.5f);
    voice = audio_play_sound(&audio, sound, 0.5f, -1.0f);
    printf("  Left channel\n");
    usleep(500000);
    
    audio_stop_sound(&audio, voice);
    voice = audio_play_sound(&audio, sound, 0.5f, 1.0f);
    printf("  Right channel\n");
    usleep(500000);
    
    audio_stop_sound(&audio, voice);
    voice = audio_play_sound(&audio, sound, 0.5f, 0.0f);
    printf("  Center\n");
    usleep(500000);
    
    /* Test effects */
    printf("\nTesting reverb effect...\n");
    audio_enable_effect(&audio, 0, AUDIO_EFFECT_REVERB);
    audio_set_reverb_params(&audio, 0, 0.9f, 0.5f);
    audio_play_sound(&audio, sound, 0.5f, 0.0f);
    usleep(2000000);
    
    /* Final stats */
    printf("\nFinal Statistics:\n");
    printf("  Frames processed: %lu\n", audio.frames_processed);
    printf("  Underruns: %lu\n", audio_get_underrun_count(&audio));
    printf("  CPU usage: %.1f%%\n", audio_get_cpu_usage(&audio) * 100.0f);
    printf("  Memory used: %.1f KB\n", audio.memory_used / 1024.0f);
    
    /* Shutdown */
    printf("\nShutting down...\n");
    audio_shutdown(&audio);
    free(tone);
    
    printf("Test completed successfully!\n");
    return 0;
}