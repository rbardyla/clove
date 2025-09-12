/*
 * Handmade Audio System - Minimal Arena-Only Implementation
 * ZERO malloc/free - Everything uses arena allocation
 * 
 * This is a simplified version that ensures 100% handmade compliance
 */

#define _GNU_SOURCE
#include "../../handmade_platform.h"
#include "handmade_audio.h"
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>

/* Fix timespec redefinition by including ALSA after time headers */
#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>

/* ALSA configuration */
#define ALSA_DEVICE "default"
#define ALSA_PERIOD_SIZE 256
#define ALSA_PERIODS 4

/* Memory alignment for SIMD */
#define AUDIO_ALIGN 32
#define ALIGN_UP(x, align) (((x) + (align) - 1) & ~((align) - 1))

/* Forward declarations */
static void *audio_thread_proc(void *data);
static void audio_mix_voices(audio_system *audio, int16_t *output, uint32_t frames);
static inline int16_t audio_clamp_sample(float sample);

/* Arena allocation helper */
static void *audio_arena_alloc(MemoryArena *arena, size_t size) {
    size = ALIGN_UP(size, AUDIO_ALIGN);
    if (arena->used + size > arena->size) {
        fprintf(stderr, "Audio: Arena out of memory! Used: %zu, Requested: %zu, Total: %zu\n",
                arena->used, size, arena->size);
        return NULL;
    }
    void *ptr = arena->base + arena->used;
    arena->used += size;
    memset(ptr, 0, size);  /* Zero initialize */
    return ptr;
}

/* Initialize audio system - PURE ARENA ALLOCATION */
bool audio_init(audio_system *audio, MemoryArena *arena) {
    memset(audio, 0, sizeof(audio_system));
    
    /* Store arena reference */
    audio->memory_pool = arena->base;
    audio->memory_size = arena->size;
    audio->memory_used = arena->used;
    
    /* Allocate ring buffer from arena */
    size_t ring_buffer_size = AUDIO_RING_BUFFER_SIZE * AUDIO_CHANNELS * sizeof(int16_t);
    audio->ring_buffer = (int16_t*)audio_arena_alloc(arena, ring_buffer_size);
    if (!audio->ring_buffer) {
        return false;
    }
    
    /* Allocate sound storage from arena */
    audio->max_sounds = 64;  /* Reduced for simplicity */
    audio->sounds = (audio_sound_buffer*)audio_arena_alloc(arena, 
        audio->max_sounds * sizeof(audio_sound_buffer));
    if (!audio->sounds) {
        return false;
    }
    
    /* Initialize voices */
    for (int i = 0; i < AUDIO_MAX_VOICES; i++) {
        audio->voices[i].active = false;
        audio->voices[i].generation = 0;
    }
    
    /* Set default volumes */
    audio->master_volume = 1.0f;
    audio->sound_volume = 1.0f;
    audio->music_volume = 1.0f;
    
    /* Set default listener */
    audio->listener_forward = (audio_vec3){0, 0, -1};
    audio->listener_up = (audio_vec3){0, 1, 0};
    
    /* Initialize ALSA */
    snd_pcm_t *pcm;
    int err;
    
    if ((err = snd_pcm_open(&pcm, ALSA_DEVICE, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        fprintf(stderr, "Audio: Cannot open ALSA device: %s\n", snd_strerror(err));
        return false;
    }
    audio->pcm_handle = pcm;
    
    /* Configure ALSA using stack allocation */
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_hw_params_alloca(&hw_params);  /* Stack allocation */
    
    snd_pcm_hw_params_any(pcm, hw_params);
    snd_pcm_hw_params_set_access(pcm, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm, hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(pcm, hw_params, AUDIO_CHANNELS);
    
    unsigned int rate = AUDIO_SAMPLE_RATE;
    snd_pcm_hw_params_set_rate_near(pcm, hw_params, &rate, 0);
    
    snd_pcm_uframes_t period_size = ALSA_PERIOD_SIZE;
    snd_pcm_hw_params_set_period_size_near(pcm, hw_params, &period_size, 0);
    
    unsigned int periods = ALSA_PERIODS;
    snd_pcm_hw_params_set_periods_near(pcm, hw_params, &periods, 0);
    
    if ((err = snd_pcm_hw_params(pcm, hw_params)) < 0) {
        fprintf(stderr, "Audio: Cannot set ALSA parameters: %s\n", snd_strerror(err));
        snd_pcm_close(pcm);
        return false;
    }
    
    snd_pcm_prepare(pcm);
    
    /* Start audio thread */
    audio->running = true;
    pthread_t thread;
    if (pthread_create(&thread, NULL, audio_thread_proc, audio) != 0) {
        fprintf(stderr, "Audio: Failed to create audio thread\n");
        snd_pcm_close(pcm);
        return false;
    }
    audio->audio_thread = (void*)thread;
    
    printf("Audio: Initialized (Arena: %.1f MB, Rate: %d Hz)\n",
           arena->size / (1024.0 * 1024.0), AUDIO_SAMPLE_RATE);
    
    return true;
}

/* Shutdown audio system */
void audio_shutdown(audio_system *audio) {
    if (!audio->pcm_handle) return;
    
    audio->running = false;
    pthread_join((pthread_t)audio->audio_thread, NULL);
    
    snd_pcm_drain((snd_pcm_t*)audio->pcm_handle);
    snd_pcm_close((snd_pcm_t*)audio->pcm_handle);
    
    printf("Audio: Shutdown complete\n");
}

/* Audio thread - ZERO HEAP ALLOCATION */
static void *audio_thread_proc(void *data) {
    audio_system *audio = (audio_system*)data;
    snd_pcm_t *pcm = (snd_pcm_t*)audio->pcm_handle;
    
    /* Stack-allocated mixing buffer */
    int16_t buffer[ALSA_PERIOD_SIZE * AUDIO_CHANNELS] __attribute__((aligned(32)));
    
    while (audio->running) {
        /* Mix audio */
        audio_mix_voices(audio, buffer, ALSA_PERIOD_SIZE);
        
        /* Write to ALSA */
        int err = snd_pcm_writei(pcm, buffer, ALSA_PERIOD_SIZE);
        if (err < 0) {
            if (err == -EPIPE) {
                audio->underruns++;
                snd_pcm_prepare(pcm);
            }
        }
        
        audio->frames_processed += ALSA_PERIOD_SIZE;
    }
    
    return NULL;
}

/* Simplified mixing - no DSP, just basic mixing */
static void audio_mix_voices(audio_system *audio, int16_t *output, uint32_t frames) {
    /* Clear output buffer */
    memset(output, 0, frames * AUDIO_CHANNELS * sizeof(int16_t));
    
    /* Stack-allocated float buffer for mixing */
    float mix_buffer[ALSA_PERIOD_SIZE * AUDIO_CHANNELS] __attribute__((aligned(32)));
    memset(mix_buffer, 0, sizeof(mix_buffer));
    
    uint32_t active_count = 0;
    
    /* Mix each active voice */
    for (int v = 0; v < AUDIO_MAX_VOICES; v++) {
        audio_voice *voice = &audio->voices[v];
        if (!voice->active) continue;
        if (voice->flags & AUDIO_FLAG_PAUSED) continue;
        
        active_count++;
        
        audio_sound_buffer *sound = &audio->sounds[voice->sound_id];
        if (!sound->is_loaded || !sound->samples) {
            voice->active = false;
            continue;
        }
        
        float volume = voice->volume * audio->master_volume * audio->sound_volume;
        float left_gain = volume * (1.0f - voice->pan) * 0.5f + 0.5f;
        float right_gain = volume * (1.0f + voice->pan) * 0.5f + 0.5f;
        
        for (uint32_t i = 0; i < frames; i++) {
            if (voice->position >= sound->frame_count) {
                if (voice->flags & AUDIO_FLAG_LOOP) {
                    voice->position = 0;
                } else {
                    voice->active = false;
                    break;
                }
            }
            
            if (sound->channels == 2) {
                float left = sound->samples[voice->position * 2];
                float right = sound->samples[voice->position * 2 + 1];
                mix_buffer[i * 2] += left * left_gain;
                mix_buffer[i * 2 + 1] += right * right_gain;
            } else {
                float sample = sound->samples[voice->position];
                mix_buffer[i * 2] += sample * left_gain;
                mix_buffer[i * 2 + 1] += sample * right_gain;
            }
            
            voice->position++;
        }
    }
    
    audio->active_voices = active_count;
    
    /* Convert float to int16 */
    for (uint32_t i = 0; i < frames * AUDIO_CHANNELS; i++) {
        output[i] = audio_clamp_sample(mix_buffer[i] * 32767.0f);
    }
}

/* Clamp sample */
static inline int16_t audio_clamp_sample(float sample) {
    if (sample > 32767.0f) return 32767;
    if (sample < -32768.0f) return -32768;
    return (int16_t)sample;
}

/* Generate simple test tone - ARENA ALLOCATED */
audio_handle audio_generate_tone(audio_system *audio, MemoryArena *arena, 
                                 float frequency, float duration_seconds) {
    if (audio->sound_count >= audio->max_sounds) {
        return AUDIO_INVALID_HANDLE;
    }
    
    uint32_t frame_count = (uint32_t)(AUDIO_SAMPLE_RATE * duration_seconds);
    size_t sample_size = frame_count * sizeof(int16_t);
    
    /* Allocate from arena */
    int16_t *samples = (int16_t*)audio_arena_alloc(arena, sample_size);
    if (!samples) {
        return AUDIO_INVALID_HANDLE;
    }
    
    /* Generate sine wave */
    float phase_inc = (2.0f * 3.14159265f * frequency) / AUDIO_SAMPLE_RATE;
    for (uint32_t i = 0; i < frame_count; i++) {
        samples[i] = (int16_t)(sinf(i * phase_inc) * 16383.0f);
    }
    
    /* Store sound */
    uint32_t sound_id = audio->sound_count++;
    audio_sound_buffer *sound = &audio->sounds[sound_id];
    sound->samples = samples;
    sound->frame_count = frame_count;
    sound->channels = 1;
    sound->sample_rate = AUDIO_SAMPLE_RATE;
    sound->size_bytes = sample_size;
    sound->is_loaded = true;
    
    return sound_id + 1;
}

/* Play sound */
audio_handle audio_play_sound(audio_system *audio, audio_handle sound_handle, 
                              float volume, float pan) {
    if (sound_handle == AUDIO_INVALID_HANDLE || sound_handle > audio->sound_count) {
        return AUDIO_INVALID_HANDLE;
    }
    
    uint32_t sound_id = sound_handle - 1;
    
    /* Find free voice */
    for (int i = 0; i < AUDIO_MAX_VOICES; i++) {
        audio_voice *voice = &audio->voices[i];
        if (!voice->active) {
            voice->sound_id = sound_id;
            voice->position = 0;
            voice->volume = volume;
            voice->pan = pan;
            voice->pitch = 1.0f;
            voice->flags = 0;
            voice->priority = AUDIO_PRIORITY_NORMAL;
            voice->phase_accumulator = 0;
            voice->generation++;
            voice->active = true;
            
            return (i << 16) | voice->generation;
        }
    }
    
    return AUDIO_INVALID_HANDLE;
}

/* Stop sound */
void audio_stop_sound(audio_system *audio, audio_handle voice_handle) {
    if (voice_handle == AUDIO_INVALID_HANDLE) return;
    
    uint32_t index = voice_handle >> 16;
    uint32_t generation = voice_handle & 0xFFFF;
    
    if (index < AUDIO_MAX_VOICES && audio->voices[index].generation == generation) {
        audio->voices[index].active = false;
    }
}

/* Set master volume */
void audio_set_master_volume(audio_system *audio, float volume) {
    audio->master_volume = fmaxf(0.0f, fminf(1.0f, volume));
}

/* Simple stubs for unused complex features */
audio_handle audio_play_sound_3d(audio_system *audio, audio_handle sound_handle, 
                                 audio_vec3 pos, float volume) {
    (void)pos;  /* Not used in minimal version */
    return audio_play_sound(audio, sound_handle, volume, 0);
}

void audio_set_listener_position(audio_system *audio, audio_vec3 pos) {
    audio->listener_position = pos;
}

void audio_set_listener_orientation(audio_system *audio, audio_vec3 forward, audio_vec3 up) {
    audio->listener_forward = forward;
    audio->listener_up = up;
}

void audio_set_voice_volume(audio_system *audio, audio_handle voice_handle, float volume) {
    if (voice_handle == AUDIO_INVALID_HANDLE) return;
    uint32_t index = voice_handle >> 16;
    uint32_t generation = voice_handle & 0xFFFF;
    if (index < AUDIO_MAX_VOICES && audio->voices[index].generation == generation) {
        audio->voices[index].volume = volume;
    }
}

void audio_set_voice_pitch(audio_system *audio, audio_handle voice_handle, float pitch) {
    if (voice_handle == AUDIO_INVALID_HANDLE) return;
    uint32_t index = voice_handle >> 16;
    uint32_t generation = voice_handle & 0xFFFF;
    if (index < AUDIO_MAX_VOICES && audio->voices[index].generation == generation) {
        audio->voices[index].pitch = pitch;
    }
}

void audio_set_voice_position_3d(audio_system *audio, audio_handle voice_handle, audio_vec3 pos) {
    (void)audio; (void)voice_handle; (void)pos;
    /* Simplified - no 3D audio in minimal version */
}

void audio_pause_sound(audio_system *audio, audio_handle voice_handle, bool pause) {
    if (voice_handle == AUDIO_INVALID_HANDLE) return;
    uint32_t index = voice_handle >> 16;
    uint32_t generation = voice_handle & 0xFFFF;
    if (index < AUDIO_MAX_VOICES && audio->voices[index].generation == generation) {
        if (pause) {
            audio->voices[index].flags |= AUDIO_FLAG_PAUSED;
        } else {
            audio->voices[index].flags &= ~AUDIO_FLAG_PAUSED;
        }
    }
}

/* Utility functions */
audio_vec3 audio_vec3_normalize(audio_vec3 v) {
    float len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len > 0.0001f) {
        v.x /= len;
        v.y /= len;
        v.z /= len;
    }
    return v;
}

float audio_vec3_distance(audio_vec3 a, audio_vec3 b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

/* Minimal stubs for unused features */
audio_handle audio_load_wav(audio_system *audio, const char *path) {
    (void)audio; (void)path;
    /* Not implemented in minimal version - use audio_generate_tone instead */
    return AUDIO_INVALID_HANDLE;
}

audio_handle audio_load_wav_from_memory(audio_system *audio, const void *data, size_t size) {
    (void)audio; (void)data; (void)size;
    /* Not implemented in minimal version */
    return AUDIO_INVALID_HANDLE;
}

void audio_unload_sound(audio_system *audio, audio_handle sound) {
    (void)audio; (void)sound;
    /* Arena allocated - no unload needed */
}

void audio_update(audio_system *audio, float dt) {
    (void)audio; (void)dt;
    /* Minimal version - no update needed */
}

float audio_get_cpu_usage(audio_system *audio) {
    (void)audio;
    return 0.0f;  /* Not tracked in minimal version */
}

uint32_t audio_get_active_voices(audio_system *audio) {
    return audio->active_voices;
}

uint64_t audio_get_underrun_count(audio_system *audio) {
    return audio->underruns;
}

void audio_set_sound_volume(audio_system *audio, float volume) {
    audio->sound_volume = fmaxf(0.0f, fminf(1.0f, volume));
}

void audio_set_music_volume(audio_system *audio, float volume) {
    audio->music_volume = fmaxf(0.0f, fminf(1.0f, volume));
}

/* Music layer stubs */
void audio_play_music_layer(audio_system *audio, uint32_t layer, audio_handle sound, float volume) {
    (void)audio; (void)layer; (void)sound; (void)volume;
}
void audio_stop_music_layer(audio_system *audio, uint32_t layer) {
    (void)audio; (void)layer;
}
void audio_set_music_intensity(audio_system *audio, float intensity) {
    (void)audio; (void)intensity;
}
void audio_crossfade_music(audio_system *audio, uint32_t from_layer, uint32_t to_layer, float time) {
    (void)audio; (void)from_layer; (void)to_layer; (void)time;
}

/* Additional stubs */
void audio_set_voice_pan(audio_system *audio, audio_handle voice_handle, float pan) {
    (void)audio; (void)voice_handle; (void)pan;
}
void audio_set_voice_velocity(audio_system *audio, audio_handle voice_handle, audio_vec3 vel) {
    (void)audio; (void)voice_handle; (void)vel;
}
void audio_set_listener_velocity(audio_system *audio, audio_vec3 vel) {
    (void)audio; (void)vel;
}
float audio_db_to_linear(float db) { return powf(10.0f, db / 20.0f); }
float audio_linear_to_db(float linear) { return 20.0f * log10f(linear); }

/* DSP stub - no effects in minimal version */
void audio_process_effects(audio_system *audio, float *buffer, uint32_t frames) {
    (void)audio; (void)buffer; (void)frames;
    /* No effects processing in minimal version */
}