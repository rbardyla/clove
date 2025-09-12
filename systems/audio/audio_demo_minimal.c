/*
 * Handmade Audio Demo - Minimal Arena-Only Version
 * ZERO malloc/free - 100% handmade compliant
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include "../../handmade_platform.h"
#include "handmade_audio.h"

#define DEMO_MEMORY_SIZE (8 * 1024 * 1024)  /* 8MB arena */

/* Generate simple tone in arena */
audio_handle audio_generate_tone(audio_system *audio, MemoryArena *arena, 
                                 float frequency, float duration_seconds);

int main(void) {
    printf("=== Handmade Audio Demo (Minimal) ===\n");
    printf("100%% Arena Allocation - Zero malloc/free\n\n");
    
    /* Create memory arena - stack allocated backing */
    static unsigned char memory_backing[DEMO_MEMORY_SIZE] __attribute__((aligned(64)));
    MemoryArena arena = {
        .base = memory_backing,
        .size = DEMO_MEMORY_SIZE,
        .used = 0
    };
    
    /* Initialize audio system */
    audio_system audio;
    if (!audio_init(&audio, &arena)) {
        fprintf(stderr, "Failed to initialize audio system\n");
        return 1;
    }
    
    printf("Audio system initialized with %.1f MB arena\n", 
           DEMO_MEMORY_SIZE / (1024.0 * 1024.0));
    
    /* Generate test tones using arena allocation */
    printf("Generating test tones...\n");
    
    audio_handle tone_440 = audio_generate_tone(&audio, &arena, 440.0f, 0.5f);
    audio_handle tone_880 = audio_generate_tone(&audio, &arena, 880.0f, 0.5f);
    audio_handle tone_220 = audio_generate_tone(&audio, &arena, 220.0f, 1.0f);
    
    if (tone_440 == AUDIO_INVALID_HANDLE || 
        tone_880 == AUDIO_INVALID_HANDLE || 
        tone_220 == AUDIO_INVALID_HANDLE) {
        fprintf(stderr, "Failed to generate test tones\n");
        audio_shutdown(&audio);
        return 1;
    }
    
    printf("Arena usage: %.1f KB / %.1f MB\n", 
           arena.used / 1024.0, arena.size / (1024.0 * 1024.0));
    
    /* Play test sequence */
    printf("\nPlaying test sequence...\n");
    printf("Commands: q=quit, 1-3=play tone, s=stop all, v=volume\n\n");
    
    char input;
    bool running = true;
    
    while (running) {
        printf("> ");
        fflush(stdout);
        
        input = getchar();
        if (input == '\n') continue;
        
        /* Clear input buffer */
        while (getchar() != '\n');
        
        switch (input) {
            case 'q':
                running = false;
                break;
                
            case '1':
                printf("Playing 440 Hz tone\n");
                audio_play_sound(&audio, tone_440, 0.5f, 0.0f);
                break;
                
            case '2':
                printf("Playing 880 Hz tone\n");
                audio_play_sound(&audio, tone_880, 0.5f, 0.0f);
                break;
                
            case '3':
                printf("Playing 220 Hz tone\n");
                audio_play_sound(&audio, tone_220, 0.5f, 0.0f);
                break;
                
            case 's':
                printf("Stopping all sounds\n");
                for (int i = 0; i < AUDIO_MAX_VOICES; i++) {
                    audio_stop_sound(&audio, (i << 16) | 1);
                }
                break;
                
            case 'v':
                printf("Enter volume (0.0-1.0): ");
                float volume;
                if (scanf("%f", &volume) == 1) {
                    audio_set_master_volume(&audio, volume);
                    printf("Master volume set to %.2f\n", volume);
                }
                while (getchar() != '\n');
                break;
                
            case 'i':
                printf("Active voices: %u\n", audio_get_active_voices(&audio));
                printf("Underruns: %lu\n", audio_get_underrun_count(&audio));
                printf("Arena usage: %.1f KB / %.1f MB\n", 
                       arena.used / 1024.0, arena.size / (1024.0 * 1024.0));
                break;
                
            default:
                printf("Unknown command: %c\n", input);
                break;
        }
    }
    
    printf("\nShutting down...\n");
    audio_shutdown(&audio);
    
    printf("Demo complete - 100%% handmade compliant!\n");
    return 0;
}