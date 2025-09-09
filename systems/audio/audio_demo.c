/*
 * Handmade Audio System Demo
 * Interactive demonstration of all audio features
 * 
 * Controls:
 * 1-5: Play different sound effects
 * Q/W/E/R: Control music layers
 * A/S/D/F: Move 3D sound source
 * Z/X: Adjust reverb
 * C/V: Adjust filter cutoff
 * Space: Play test tone
 * ESC: Exit
 */

#include "handmade_audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>
#include <string.h>

/* Demo configuration */
#define DEMO_MEMORY_SIZE (32 * 1024 * 1024)  /* 32MB for audio */
#define DEMO_UPDATE_RATE 60  /* Hz */

/* Test sound generation */
static void generate_test_tone(int16_t *buffer, uint32_t frames, float frequency) {
    for (uint32_t i = 0; i < frames; i++) {
        float t = (float)i / AUDIO_SAMPLE_RATE;
        float sample = sinf(2.0f * M_PI * frequency * t) * 16000.0f;
        buffer[i * 2] = (int16_t)sample;      /* Left */
        buffer[i * 2 + 1] = (int16_t)sample;   /* Right */
    }
}

/* Generate a simple drum sound */
static void generate_drum(int16_t *buffer, uint32_t frames) {
    for (uint32_t i = 0; i < frames; i++) {
        float t = (float)i / AUDIO_SAMPLE_RATE;
        float envelope = expf(-t * 35.0f);  /* Fast decay */
        
        /* Mix of sine wave and noise */
        float sine = sinf(2.0f * M_PI * 60.0f * t);  /* 60Hz base frequency */
        float noise = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        float sample = (sine * 0.7f + noise * 0.3f) * envelope * 20000.0f;
        
        buffer[i * 2] = (int16_t)sample;
        buffer[i * 2 + 1] = (int16_t)sample;
    }
}

/* Generate white noise */
static void generate_noise(int16_t *buffer, uint32_t frames, float amplitude) {
    for (uint32_t i = 0; i < frames; i++) {
        float sample = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        sample *= amplitude * 16000.0f;
        buffer[i * 2] = (int16_t)sample;
        buffer[i * 2 + 1] = (int16_t)sample;
    }
}

/* Generate a sweep sound */
static void generate_sweep(int16_t *buffer, uint32_t frames, float start_freq, float end_freq) {
    for (uint32_t i = 0; i < frames; i++) {
        float t = (float)i / frames;
        float frequency = start_freq + (end_freq - start_freq) * t;
        float phase = 2.0f * M_PI * frequency * t;
        float sample = sinf(phase) * 16000.0f;
        
        buffer[i * 2] = (int16_t)sample;
        buffer[i * 2 + 1] = (int16_t)sample;
    }
}

/* Generate music loop */
static void generate_music_loop(int16_t *buffer, uint32_t frames, int pattern) {
    /* Simple algorithmic music generation */
    int notes[] = {220, 247, 262, 294, 330, 349, 392, 440};  /* A minor scale */
    int pattern_length = 16;
    uint32_t samples_per_note = frames / pattern_length;
    
    for (uint32_t i = 0; i < frames; i++) {
        int note_index = (i / samples_per_note) % pattern_length;
        int note = notes[(note_index + pattern * 2) % 8];
        
        float t = (float)(i % samples_per_note) / AUDIO_SAMPLE_RATE;
        float envelope = 1.0f - t * 2.0f;  /* Simple decay */
        if (envelope < 0) envelope = 0;
        
        float sample = sinf(2.0f * M_PI * note * t) * envelope * 10000.0f;
        
        /* Add some harmonics */
        sample += sinf(4.0f * M_PI * note * t) * envelope * 3000.0f;
        sample += sinf(6.0f * M_PI * note * t) * envelope * 1000.0f;
        
        buffer[i * 2] = (int16_t)sample;
        buffer[i * 2 + 1] = (int16_t)sample;
    }
}

/* Setup terminal for non-blocking input */
static void setup_terminal() {
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);
    tty.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &tty);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
}

/* Restore terminal */
static void restore_terminal() {
    struct termios tty;
    tcgetattr(STDIN_FILENO, &tty);
    tty.c_lflag |= ICANON | ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

/* Clear screen and move cursor to top */
static void clear_screen() {
    printf("\033[2J\033[H");
}

/* Draw performance stats */
static void draw_stats(audio_system *audio, audio_vec3 sound_pos, float reverb_mix, float filter_cutoff) {
    clear_screen();
    
    printf("=== HANDMADE AUDIO SYSTEM DEMO ===\n\n");
    
    printf("Performance:\n");
    printf("  CPU Usage: %.1f%%\n", audio_get_cpu_usage(audio) * 100.0f);
    printf("  Active Voices: %d / %d\n", audio_get_active_voices(audio), AUDIO_MAX_VOICES);
    printf("  Underruns: %lu\n", audio_get_underrun_count(audio));
    printf("  Memory Used: %.1f MB\n\n", audio->memory_used / (1024.0f * 1024.0f));
    
    printf("3D Sound Position: (%.1f, %.1f, %.1f)\n", sound_pos.x, sound_pos.y, sound_pos.z);
    printf("Listener Position: (%.1f, %.1f, %.1f)\n\n", 
           audio->listener_position.x, audio->listener_position.y, audio->listener_position.z);
    
    printf("Effects:\n");
    printf("  Reverb Mix: %.0f%%\n", reverb_mix * 100.0f);
    printf("  Filter Cutoff: %.0f Hz\n\n", filter_cutoff);
    
    printf("Controls:\n");
    printf("  1-5: Play sound effects\n");
    printf("  Q/W/E/R: Toggle music layers\n");
    printf("  A/S/D/F: Move 3D sound (left/back/forward/right)\n");
    printf("  Arrow keys: Move listener\n");
    printf("  Z/X: Adjust reverb mix\n");
    printf("  C/V: Adjust filter cutoff\n");
    printf("  B/N: Adjust master volume\n");
    printf("  Space: Play test tone\n");
    printf("  P: Performance test (100 sounds)\n");
    printf("  ESC: Exit\n\n");
    
    printf("Press keys to interact...\n");
    
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    printf("Initializing Handmade Audio System...\n");
    
    /* Initialize audio system */
    audio_system audio;
    if (!audio_init(&audio, DEMO_MEMORY_SIZE)) {
        fprintf(stderr, "Failed to initialize audio system\n");
        return 1;
    }
    
    /* Generate test sounds */
    printf("Generating test sounds...\n");
    
    /* Sound 1: Test tone */
    int16_t *tone_buffer = malloc(AUDIO_SAMPLE_RATE * 2 * sizeof(int16_t));
    generate_test_tone(tone_buffer, AUDIO_SAMPLE_RATE / 2, 440.0f);
    audio_handle sound_tone = audio_load_wav_from_memory(&audio, tone_buffer, 
                                                         AUDIO_SAMPLE_RATE * sizeof(int16_t));
    
    /* Sound 2: Drum */
    int16_t *drum_buffer = malloc(AUDIO_SAMPLE_RATE * 2 * sizeof(int16_t));
    generate_drum(drum_buffer, AUDIO_SAMPLE_RATE / 4);
    audio_handle sound_drum = audio_load_wav_from_memory(&audio, drum_buffer,
                                                         AUDIO_SAMPLE_RATE / 2 * sizeof(int16_t));
    
    /* Sound 3: White noise */
    int16_t *noise_buffer = malloc(AUDIO_SAMPLE_RATE * 2 * sizeof(int16_t));
    generate_noise(noise_buffer, AUDIO_SAMPLE_RATE / 2, 0.3f);
    audio_handle sound_noise = audio_load_wav_from_memory(&audio, noise_buffer,
                                                          AUDIO_SAMPLE_RATE * sizeof(int16_t));
    
    /* Sound 4: Sweep */
    int16_t *sweep_buffer = malloc(AUDIO_SAMPLE_RATE * 2 * sizeof(int16_t));
    generate_sweep(sweep_buffer, AUDIO_SAMPLE_RATE, 100.0f, 2000.0f);
    audio_handle sound_sweep = audio_load_wav_from_memory(&audio, sweep_buffer,
                                                          AUDIO_SAMPLE_RATE * 2 * sizeof(int16_t));
    
    /* Music loops */
    int16_t *music_buffers[4];
    audio_handle music_sounds[4];
    for (int i = 0; i < 4; i++) {
        music_buffers[i] = malloc(AUDIO_SAMPLE_RATE * 4 * 2 * sizeof(int16_t));  /* 4 second loops */
        generate_music_loop(music_buffers[i], AUDIO_SAMPLE_RATE * 4, i);
        music_sounds[i] = audio_load_wav_from_memory(&audio, music_buffers[i],
                                                     AUDIO_SAMPLE_RATE * 8 * sizeof(int16_t));
    }
    
    /* Setup effects */
    audio_enable_effect(&audio, 0, AUDIO_EFFECT_REVERB);
    audio_set_reverb_params(&audio, 0, 0.8f, 0.5f);
    
    audio_enable_effect(&audio, 1, AUDIO_EFFECT_LOWPASS);
    audio_set_filter_params(&audio, 1, 5000.0f, 1.0f);
    
    audio_enable_effect(&audio, 2, AUDIO_EFFECT_COMPRESSOR);
    
    /* State variables */
    audio_vec3 sound_3d_pos = {5.0f, 0.0f, 0.0f};
    audio_handle voice_3d = AUDIO_INVALID_HANDLE;
    bool music_layers[4] = {false, false, false, false};
    float reverb_mix = 0.3f;
    float filter_cutoff = 5000.0f;
    
    /* Setup terminal for input */
    setup_terminal();
    
    /* Main loop */
    bool running = true;
    clock_t last_update = clock();
    
    while (running) {
        /* Handle input */
        char key;
        if (read(STDIN_FILENO, &key, 1) == 1) {
            switch (key) {
                case '1':
                    audio_play_sound(&audio, sound_tone, 0.7f, 0.0f);
                    break;
                    
                case '2':
                    audio_play_sound(&audio, sound_drum, 1.0f, -0.5f);
                    break;
                    
                case '3':
                    audio_play_sound(&audio, sound_noise, 0.5f, 0.5f);
                    break;
                    
                case '4':
                    audio_play_sound(&audio, sound_sweep, 0.6f, 0.0f);
                    break;
                    
                case '5':
                    /* Play 3D sound */
                    if (voice_3d != AUDIO_INVALID_HANDLE) {
                        audio_stop_sound(&audio, voice_3d);
                    }
                    voice_3d = audio_play_sound_3d(&audio, sound_tone, sound_3d_pos, 1.0f);
                    break;
                    
                case 'q':
                case 'Q':
                    music_layers[0] = !music_layers[0];
                    if (music_layers[0]) {
                        audio_play_music_layer(&audio, 0, music_sounds[0], 0.5f);
                    } else {
                        audio_stop_music_layer(&audio, 0);
                    }
                    break;
                    
                case 'w':
                case 'W':
                    music_layers[1] = !music_layers[1];
                    if (music_layers[1]) {
                        audio_play_music_layer(&audio, 1, music_sounds[1], 0.5f);
                    } else {
                        audio_stop_music_layer(&audio, 1);
                    }
                    break;
                    
                case 'a':
                case 'A':
                    sound_3d_pos.x -= 1.0f;
                    if (voice_3d != AUDIO_INVALID_HANDLE) {
                        audio_set_voice_position_3d(&audio, voice_3d, sound_3d_pos);
                    }
                    break;
                    
                case 'd':
                case 'D':
                    sound_3d_pos.x += 1.0f;
                    if (voice_3d != AUDIO_INVALID_HANDLE) {
                        audio_set_voice_position_3d(&audio, voice_3d, sound_3d_pos);
                    }
                    break;
                    
                case 's':
                case 'S':
                    sound_3d_pos.z -= 1.0f;
                    if (voice_3d != AUDIO_INVALID_HANDLE) {
                        audio_set_voice_position_3d(&audio, voice_3d, sound_3d_pos);
                    }
                    break;
                    
                case 'f':
                case 'F':
                    sound_3d_pos.z += 1.0f;
                    if (voice_3d != AUDIO_INVALID_HANDLE) {
                        audio_set_voice_position_3d(&audio, voice_3d, sound_3d_pos);
                    }
                    break;
                    
                case 'z':
                case 'Z':
                    reverb_mix = fmaxf(0.0f, reverb_mix - 0.1f);
                    audio.effects[0].mix = reverb_mix;
                    break;
                    
                case 'x':
                case 'X':
                    reverb_mix = fminf(1.0f, reverb_mix + 0.1f);
                    audio.effects[0].mix = reverb_mix;
                    break;
                    
                case 'c':
                case 'C':
                    filter_cutoff = fmaxf(100.0f, filter_cutoff - 500.0f);
                    audio_set_filter_params(&audio, 1, filter_cutoff, 2.0f);
                    break;
                    
                case 'v':
                case 'V':
                    filter_cutoff = fminf(10000.0f, filter_cutoff + 500.0f);
                    audio_set_filter_params(&audio, 1, filter_cutoff, 2.0f);
                    break;
                    
                case 'b':
                case 'B':
                    audio_set_master_volume(&audio, audio.master_volume - 0.1f);
                    break;
                    
                case 'n':
                case 'N':
                    audio_set_master_volume(&audio, audio.master_volume + 0.1f);
                    break;
                    
                case ' ':
                    /* Quick test tone */
                    audio_play_sound(&audio, sound_tone, 0.5f, 0.0f);
                    break;
                    
                case 'p':
                case 'P':
                    /* Performance test - play many sounds */
                    printf("\nPerformance test: Playing 100 sounds...\n");
                    for (int i = 0; i < 100; i++) {
                        float pan = ((float)i / 100.0f) * 2.0f - 1.0f;
                        audio_play_sound(&audio, sound_drum, 0.3f, pan);
                        usleep(10000);  /* 10ms between sounds */
                    }
                    break;
                    
                case 27:  /* ESC */
                    running = false;
                    break;
            }
        }
        
        /* Update display at 10Hz */
        clock_t now = clock();
        float dt = (float)(now - last_update) / CLOCKS_PER_SEC;
        if (dt > 0.1f) {
            draw_stats(&audio, sound_3d_pos, reverb_mix, filter_cutoff);
            last_update = now;
        }
        
        /* Small delay to prevent busy waiting */
        usleep(1000);
    }
    
    /* Cleanup */
    restore_terminal();
    clear_screen();
    
    printf("Shutting down audio system...\n");
    audio_shutdown(&audio);
    
    /* Free buffers */
    free(tone_buffer);
    free(drum_buffer);
    free(noise_buffer);
    free(sweep_buffer);
    for (int i = 0; i < 4; i++) {
        free(music_buffers[i]);
    }
    
    printf("Audio demo completed successfully!\n");
    printf("Peak performance: %.1f%% CPU, %d voices\n", 
           audio.cpu_usage * 100.0f, audio.active_voices);
    
    return 0;
}