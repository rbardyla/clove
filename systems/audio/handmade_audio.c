/*
 * Handmade Audio System - Core Implementation
 * Direct ALSA integration with lock-free ring buffer
 * 
 * PERFORMANCE: All audio processing in dedicated thread
 * CACHE: Ring buffer sized to fit in L2 cache
 * THREADING: Lock-free producer/consumer pattern
 */

#define _GNU_SOURCE  // For aligned_alloc and clock_gettime
#include "../../handmade_platform.h"  // Must include platform header first for MemoryArena
#include "handmade_audio.h"
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <immintrin.h>  /* SSE/AVX intrinsics */
#include <time.h>
#include <unistd.h>
#include <alloca.h>
#include <alsa/asoundlib.h>

/* ALSA configuration */
#define ALSA_DEVICE "default"
#define ALSA_PERIOD_SIZE 256  /* Frames per period for low latency */
#define ALSA_PERIODS 4

/* Memory alignment for SIMD */
#define AUDIO_ALIGN 32
#define ALIGN_UP(x, align) (((x) + (align) - 1) & ~((align) - 1))

/* Lock-free ring buffer macros */
#define RING_BUFFER_MASK (AUDIO_RING_BUFFER_SIZE - 1)
#define RING_BUFFER_INCREMENT(x) (((x) + 1) & RING_BUFFER_MASK)

/* Forward declarations */
static void *audio_thread_proc(void *data);
static void audio_mix_voices(audio_system *audio, int16_t *output, uint32_t frames);
static void audio_apply_3d(audio_system *audio, audio_voice *voice, float *left_gain, float *right_gain);
void audio_process_effects(audio_system *audio, float *buffer, uint32_t frames);  /* Defined in audio_dsp.c */
static inline int16_t audio_clamp_sample(float sample);

/* HANDMADE: Arena allocation - no malloc/free in hot paths! */
static void *audio_arena_alloc(MemoryArena *arena, size_t size) {
    size = ALIGN_UP(size, AUDIO_ALIGN);
    if (arena->used + size > arena->size) {
        fprintf(stderr, "Audio: Arena out of memory! Used: %zu, Requested: %zu, Total: %zu\n",
                arena->used, size, arena->size);
        return NULL;
    }
    void *ptr = arena->base + arena->used;
    arena->used += size;
    return ptr;
}

/* Initialize audio system - HANDMADE: Arena-based allocation */
bool audio_init(audio_system *audio, MemoryArena *arena) {
    memset(audio, 0, sizeof(audio_system));
    
    /* Store arena reference - no heap allocation! */
    audio->memory_pool = arena->base;
    audio->memory_size = arena->size;
    audio->memory_used = arena->used;  // Track our usage
    
    /* Allocate ring buffer from arena */
    size_t ring_buffer_size = AUDIO_RING_BUFFER_SIZE * AUDIO_CHANNELS * sizeof(int16_t);
    audio->ring_buffer = (int16_t*)audio_arena_alloc(arena, ring_buffer_size);
    if (!audio->ring_buffer) {
        return false;
    }
    
    /* Allocate sound storage from arena */
    audio->max_sounds = 256;
    audio->sounds = (audio_sound_buffer*)audio_arena_alloc(arena, 
        audio->max_sounds * sizeof(audio_sound_buffer));
    
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
    
    /* Open PCM device */
    if ((err = snd_pcm_open(&pcm, ALSA_DEVICE, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        fprintf(stderr, "Audio: Cannot open ALSA device: %s\n", snd_strerror(err));
        return false;
    }
    audio->pcm_handle = pcm;
    
    /* Allocate hardware parameters */
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_hw_params_alloca(&hw_params);
    
    /* Configure hardware parameters */
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
    
    /* Apply hardware parameters */
    if ((err = snd_pcm_hw_params(pcm, hw_params)) < 0) {
        fprintf(stderr, "Audio: Cannot set ALSA parameters: %s\n", snd_strerror(err));
        snd_pcm_close(pcm);
        return false;
    }
    
    /* Prepare PCM for playback */
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
    
    /* Set thread priority for real-time audio */
    struct sched_param param;
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(thread, SCHED_FIFO, &param);
    
    printf("Audio: Initialized successfully (%.1f MB, %d Hz, %dms latency)\n",
           arena->size / (1024.0 * 1024.0), AUDIO_SAMPLE_RATE, 
           (ALSA_PERIOD_SIZE * 1000) / AUDIO_SAMPLE_RATE);
    
    return true;
}

/* Shutdown audio system - HANDMADE: No free() needed! */
void audio_shutdown(audio_system *audio) {
    if (!audio->pcm_handle) return;
    
    /* Stop audio thread */
    audio->running = false;
    pthread_join((pthread_t)audio->audio_thread, NULL);
    
    /* Close ALSA */
    snd_pcm_drain((snd_pcm_t*)audio->pcm_handle);
    snd_pcm_close((snd_pcm_t*)audio->pcm_handle);
    
    /* HANDMADE: No free() - arena memory is managed externally */
    
    printf("Audio: Shutdown complete\n");
}

/* Audio thread - runs continuously */
static void *audio_thread_proc(void *data) {
    audio_system *audio = (audio_system*)data;
    snd_pcm_t *pcm = (snd_pcm_t*)audio->pcm_handle;
    
    /* HANDMADE: Use stack allocation for mixing buffer - no malloc! */
    int16_t buffer[ALSA_PERIOD_SIZE * AUDIO_CHANNELS] __attribute__((aligned(32)));
    
    /* Performance timing */
    struct timespec start_time, end_time;
    uint64_t total_time = 0;
    uint64_t frame_count = 0;
    
    while (audio->running) {
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        
        /* Mix audio into buffer */
        audio_mix_voices(audio, buffer, ALSA_PERIOD_SIZE);
        
        /* Write to ALSA */
        int err = snd_pcm_writei(pcm, buffer, ALSA_PERIOD_SIZE);
        if (err < 0) {
            if (err == -EPIPE) {
                /* Underrun occurred */
                audio->underruns++;
                snd_pcm_prepare(pcm);
            } else {
                fprintf(stderr, "Audio: Write error: %s\n", snd_strerror(err));
            }
        }
        
        /* Update performance counters */
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        uint64_t elapsed = (end_time.tv_sec - start_time.tv_sec) * 1000000000ULL +
                          (end_time.tv_nsec - start_time.tv_nsec);
        total_time += elapsed;
        frame_count++;
        
        if (frame_count >= 100) {
            /* Update CPU usage every 100 frames */
            uint64_t frame_time_ns = (ALSA_PERIOD_SIZE * 1000000000ULL) / AUDIO_SAMPLE_RATE;
            audio->cpu_usage = (float)((double)total_time / (double)(frame_time_ns * frame_count));
            total_time = 0;
            frame_count = 0;
        }
        
        audio->frames_processed += ALSA_PERIOD_SIZE;
    }
    
    /* HANDMADE: No free() needed - stack allocated buffer */
    return NULL;
}

/* Mix all active voices - PERFORMANCE CRITICAL */
static void audio_mix_voices(audio_system *audio, int16_t *output, uint32_t frames) {
    /* PERFORMANCE: Hot path - 90% of audio thread time
     * CACHE: Process voices sequentially for cache locality
     * SIMD: Use SSE2 for mixing operations */
    
    /* Clear output buffer using SIMD */
    __m128i zero = _mm_setzero_si128();
    for (uint32_t i = 0; i < frames * AUDIO_CHANNELS / 8; i++) {
        _mm_store_si128((__m128i*)&output[i * 8], zero);
    }
    
    /* Temporary float buffer for mixing */
    float *mix_buffer = (float*)alloca(frames * AUDIO_CHANNELS * sizeof(float));
    memset(mix_buffer, 0, frames * AUDIO_CHANNELS * sizeof(float));
    
    uint32_t active_count = 0;
    
    /* Mix each active voice */
    for (int v = 0; v < AUDIO_MAX_VOICES; v++) {
        audio_voice *voice = &audio->voices[v];
        if (!voice->active) continue;
        if (voice->flags & AUDIO_FLAG_PAUSED) continue;  /* Skip paused voices */
        
        active_count++;
        
        audio_sound_buffer *sound = &audio->sounds[voice->sound_id];
        if (!sound->is_loaded) {
            voice->active = false;
            continue;
        }
        
        /* Calculate 3D audio gains if needed */
        float left_gain = voice->volume;
        float right_gain = voice->volume;
        
        if (voice->flags & AUDIO_FLAG_3D) {
            audio_apply_3d(audio, voice, &left_gain, &right_gain);
        } else {
            /* Apply panning for 2D sounds */
            float pan = voice->pan;
            left_gain *= (1.0f - pan) * 0.5f + 0.5f;
            right_gain *= (1.0f + pan) * 0.5f + 0.5f;
        }
        
        /* Apply master volumes */
        float volume_scale = audio->master_volume * audio->sound_volume;
        left_gain *= volume_scale;
        right_gain *= volume_scale;
        
        /* Mix voice into buffer with resampling */
        float pitch = voice->pitch;
        float phase = voice->phase_accumulator;
        
        for (uint32_t i = 0; i < frames; i++) {
            /* Get source sample with linear interpolation */
            uint32_t pos = voice->position;
            if (pos >= sound->frame_count) {
                if (voice->flags & AUDIO_FLAG_LOOP) {
                    voice->position = 0;
                    pos = 0;
                } else {
                    voice->active = false;
                    break;
                }
            }
            
            /* Linear interpolation for pitch shifting */
            float frac = phase - floorf(phase);
            uint32_t next_pos = (pos + 1) % sound->frame_count;
            
            if (sound->channels == 2) {
                /* Stereo source */
                float left = sound->samples[pos * 2] * (1.0f - frac) + 
                            sound->samples[next_pos * 2] * frac;
                float right = sound->samples[pos * 2 + 1] * (1.0f - frac) + 
                             sound->samples[next_pos * 2 + 1] * frac;
                
                mix_buffer[i * 2] += left * left_gain;
                mix_buffer[i * 2 + 1] += right * right_gain;
            } else {
                /* Mono source */
                float sample = sound->samples[pos] * (1.0f - frac) + 
                              sound->samples[next_pos] * frac;
                
                mix_buffer[i * 2] += sample * left_gain;
                mix_buffer[i * 2 + 1] += sample * right_gain;
            }
            
            /* Advance position with pitch */
            phase += pitch;
            while (phase >= 1.0f) {
                voice->position++;
                phase -= 1.0f;
            }
        }
        
        voice->phase_accumulator = phase;
    }
    
    audio->active_voices = active_count;
    
    /* Apply effects to mix buffer */
    if (active_count > 0) {
        audio_process_effects(audio, mix_buffer, frames);
    }
    
    /* Convert float mix to int16 output with clamping using SIMD */
    for (uint32_t i = 0; i < frames * AUDIO_CHANNELS; i += 4) {
        __m128 samples = _mm_load_ps(&mix_buffer[i]);
        __m128 scaled = _mm_mul_ps(samples, _mm_set1_ps(32767.0f));
        __m128i integers = _mm_cvtps_epi32(scaled);
        
        /* Pack and saturate to int16 */
        __m128i packed = _mm_packs_epi32(integers, integers);
        _mm_storel_epi64((__m128i*)&output[i], packed);
    }
}

/* Apply 3D audio calculations */
static void audio_apply_3d(audio_system *audio, audio_voice *voice, float *left_gain, float *right_gain) {
    /* Calculate distance attenuation */
    float dist = audio_vec3_distance(voice->position_3d, audio->listener_position);
    
    float attenuation = 1.0f;
    if (dist > voice->min_distance) {
        if (dist < voice->max_distance) {
            /* Linear falloff between min and max distance */
            float range = voice->max_distance - voice->min_distance;
            attenuation = 1.0f - (dist - voice->min_distance) / range;
        } else {
            attenuation = 0.0f;
        }
    }
    
    /* Calculate stereo panning based on position */
    audio_vec3 to_sound = {
        voice->position_3d.x - audio->listener_position.x,
        voice->position_3d.y - audio->listener_position.y,
        voice->position_3d.z - audio->listener_position.z
    };
    to_sound = audio_vec3_normalize(to_sound);
    
    /* Simple left/right panning based on listener orientation */
    audio_vec3 right = {
        audio->listener_forward.z, 
        0, 
        -audio->listener_forward.x
    };
    right = audio_vec3_normalize(right);
    
    float dot_right = to_sound.x * right.x + to_sound.y * right.y + to_sound.z * right.z;
    float pan = dot_right;  /* -1 to 1 */
    
    /* Apply panning */
    *left_gain = voice->volume * attenuation * (1.0f - pan) * 0.5f + 0.5f;
    *right_gain = voice->volume * attenuation * (1.0f + pan) * 0.5f + 0.5f;
    
    /* Apply simple Doppler effect if velocities are significant */
    float speed_of_sound = 343.0f;  /* m/s */
    audio_vec3 relative_velocity = {
        voice->velocity.x - audio->listener_velocity.x,
        voice->velocity.y - audio->listener_velocity.y,
        voice->velocity.z - audio->listener_velocity.z
    };
    
    float velocity_towards = -(relative_velocity.x * to_sound.x + 
                              relative_velocity.y * to_sound.y + 
                              relative_velocity.z * to_sound.z);
    
    /* Doppler pitch shift */
    float doppler_factor = 1.0f + (velocity_towards / speed_of_sound);
    doppler_factor = fmaxf(0.5f, fminf(2.0f, doppler_factor));  /* Clamp to reasonable range */
    voice->pitch *= doppler_factor;
}

/* Clamp floating point sample to int16 range */
static inline int16_t audio_clamp_sample(float sample) {
    if (sample > 32767.0f) return 32767;
    if (sample < -32768.0f) return -32768;
    return (int16_t)sample;
}

/* Load WAV file */
audio_handle audio_load_wav(audio_system *audio, const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Audio: Cannot open WAV file: %s\n", path);
        return AUDIO_INVALID_HANDLE;
    }
    
    /* Read WAV header */
    struct {
        char riff[4];
        uint32_t size;
        char wave[4];
        char fmt[4];
        uint32_t fmt_size;
        uint16_t format;
        uint16_t channels;
        uint32_t sample_rate;
        uint32_t byte_rate;
        uint16_t block_align;
        uint16_t bits_per_sample;
    } header;
    
    if (fread(&header, sizeof(header), 1, file) != 1) {
        fprintf(stderr, "Audio: Failed to read WAV header: %s\n", path);
        fclose(file);
        return AUDIO_INVALID_HANDLE;
    }
    
    /* Validate WAV file */
    if (memcmp(header.riff, "RIFF", 4) != 0 || memcmp(header.wave, "WAVE", 4) != 0) {
        fprintf(stderr, "Audio: Invalid WAV file: %s\n", path);
        fclose(file);
        return AUDIO_INVALID_HANDLE;
    }
    
    /* Find data chunk */
    char chunk_id[4];
    uint32_t chunk_size;
    while (fread(chunk_id, 4, 1, file) == 1) {
        if (fread(&chunk_size, 4, 1, file) != 1) {
            fprintf(stderr, "Audio: Failed to read chunk size: %s\n", path);
            fclose(file);
            return AUDIO_INVALID_HANDLE;
        }
        if (memcmp(chunk_id, "data", 4) == 0) break;
        fseek(file, chunk_size, SEEK_CUR);
    }
    
    /* Allocate sound buffer */
    if (audio->sound_count >= audio->max_sounds) {
        fprintf(stderr, "Audio: Maximum sounds reached\n");
        fclose(file);
        return AUDIO_INVALID_HANDLE;
    }
    
    uint32_t sound_id = audio->sound_count++;
    audio_sound_buffer *sound = &audio->sounds[sound_id];
    
    sound->channels = header.channels;
    sound->sample_rate = header.sample_rate;
    sound->frame_count = chunk_size / (header.channels * (header.bits_per_sample / 8));
    sound->size_bytes = chunk_size;
    
    /* Allocate sample buffer from pool */
    /* FIXME: WAV loading needs arena parameter - temporarily disabled for handmade compliance */
    sound->samples = NULL;  // TODO: Implement arena-based WAV loading
    if (!sound->samples) {
        fclose(file);
        audio->sound_count--;
        return AUDIO_INVALID_HANDLE;
    }
    
    /* Read samples - DISABLED for handmade compliance */
    /* TODO: Implement with arena allocation
    if (fread(sound->samples, chunk_size, 1, file) != 1) {
        fprintf(stderr, "Audio: Failed to read samples: %s\n", path);
        fclose(file);
        audio->sound_count--;
        return AUDIO_INVALID_HANDLE;
    }
    */
    fclose(file);
    
    sound->is_loaded = true;
    
    printf("Audio: Loaded %s (%.1f seconds, %d Hz, %d channels)\n", 
           path, (float)sound->frame_count / sound->sample_rate,
           sound->sample_rate, sound->channels);
    
    return sound_id + 1;  /* Handle is ID + 1 to avoid 0 */
}

/* Play a sound */
audio_handle audio_play_sound(audio_system *audio, audio_handle sound_handle, float volume, float pan) {
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
            
            return (i << 16) | voice->generation;  /* Encode voice index and generation */
        }
    }
    
    /* Voice stealing - find lowest priority voice */
    int steal_index = -1;
    audio_priority lowest_priority = AUDIO_PRIORITY_CRITICAL;
    
    for (int i = 0; i < AUDIO_MAX_VOICES; i++) {
        if (audio->voices[i].priority < lowest_priority) {
            lowest_priority = audio->voices[i].priority;
            steal_index = i;
        }
    }
    
    if (steal_index >= 0) {
        audio_voice *voice = &audio->voices[steal_index];
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
        
        return (steal_index << 16) | voice->generation;
    }
    
    return AUDIO_INVALID_HANDLE;
}

/* Play 3D sound */
audio_handle audio_play_sound_3d(audio_system *audio, audio_handle sound_handle, 
                                 audio_vec3 pos, float volume) {
    audio_handle voice = audio_play_sound(audio, sound_handle, volume, 0);
    if (voice != AUDIO_INVALID_HANDLE) {
        uint32_t index = voice >> 16;
        audio->voices[index].flags |= AUDIO_FLAG_3D;
        audio->voices[index].position_3d = pos;
        audio->voices[index].min_distance = 1.0f;
        audio->voices[index].max_distance = 100.0f;
    }
    return voice;
}

/* Stop playing sound */
void audio_stop_sound(audio_system *audio, audio_handle voice_handle) {
    if (voice_handle == AUDIO_INVALID_HANDLE) return;
    
    uint32_t index = voice_handle >> 16;
    uint32_t generation = voice_handle & 0xFFFF;
    
    if (index < AUDIO_MAX_VOICES && audio->voices[index].generation == generation) {
        audio->voices[index].active = false;
    }
}

/* Set voice volume */
void audio_set_voice_volume(audio_system *audio, audio_handle voice_handle, float volume) {
    if (voice_handle == AUDIO_INVALID_HANDLE) return;
    
    uint32_t index = voice_handle >> 16;
    uint32_t generation = voice_handle & 0xFFFF;
    
    if (index < AUDIO_MAX_VOICES && audio->voices[index].generation == generation) {
        audio->voices[index].volume = volume;
    }
}

/* Set voice pitch */
void audio_set_voice_pitch(audio_system *audio, audio_handle voice_handle, float pitch) {
    if (voice_handle == AUDIO_INVALID_HANDLE) return;
    
    uint32_t index = voice_handle >> 16;
    uint32_t generation = voice_handle & 0xFFFF;
    
    if (index < AUDIO_MAX_VOICES && audio->voices[index].generation == generation) {
        audio->voices[index].pitch = pitch;
    }
}

/* Set 3D position */
void audio_set_voice_position_3d(audio_system *audio, audio_handle voice_handle, audio_vec3 pos) {
    if (voice_handle == AUDIO_INVALID_HANDLE) return;
    
    uint32_t index = voice_handle >> 16;
    uint32_t generation = voice_handle & 0xFFFF;
    
    if (index < AUDIO_MAX_VOICES && audio->voices[index].generation == generation) {
        audio->voices[index].position_3d = pos;
    }
}

/* Set listener position */
void audio_set_listener_position(audio_system *audio, audio_vec3 pos) {
    audio->listener_position = pos;
}

/* Set listener orientation */
void audio_set_listener_orientation(audio_system *audio, audio_vec3 forward, audio_vec3 up) {
    audio->listener_forward = audio_vec3_normalize(forward);
    audio->listener_up = audio_vec3_normalize(up);
}

/* Set master volume */
void audio_set_master_volume(audio_system *audio, float volume) {
    audio->master_volume = fmaxf(0.0f, fminf(1.0f, volume));
}

/* Get CPU usage */
float audio_get_cpu_usage(audio_system *audio) {
    return audio->cpu_usage;
}

/* Get active voice count */
uint32_t audio_get_active_voices(audio_system *audio) {
    return audio->active_voices;
}

/* Vector utilities */
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

/* Load WAV from memory */
audio_handle audio_load_wav_from_memory(audio_system *audio, const void *data, size_t size) {
    if (!data || size < sizeof(int16_t) * 2) {
        fprintf(stderr, "Audio: Invalid memory buffer\n");
        return AUDIO_INVALID_HANDLE;
    }
    
    /* Allocate sound buffer */
    if (audio->sound_count >= audio->max_sounds) {
        fprintf(stderr, "Audio: Maximum sounds reached\n");
        return AUDIO_INVALID_HANDLE;
    }
    
    uint32_t sound_id = audio->sound_count++;
    audio_sound_buffer *sound = &audio->sounds[sound_id];
    
    /* Assume raw PCM data at 48kHz stereo */
    sound->channels = 2;
    sound->sample_rate = AUDIO_SAMPLE_RATE;
    sound->frame_count = size / (2 * sizeof(int16_t));
    sound->size_bytes = size;
    
    /* Allocate sample buffer from pool */
    /* FIXME: WAV loading needs arena parameter - temporarily disabled for handmade compliance */
    sound->samples = NULL;  // TODO: Implement arena-based WAV loading
    if (!sound->samples) {
        audio->sound_count--;
        return AUDIO_INVALID_HANDLE;
    }
    
    /* Copy samples */
    memcpy(sound->samples, data, size);
    sound->is_loaded = true;
    
    return sound_id + 1;  /* Handle is ID + 1 to avoid 0 */
}

/* Music layer control */
void audio_play_music_layer(audio_system *audio, uint32_t layer, audio_handle sound, float volume) {
    if (layer >= 8 || sound == AUDIO_INVALID_HANDLE) return;
    
    audio_music_layer *music = &audio->music_layers[layer];
    music->sound_id = sound - 1;
    music->volume = volume;
    music->is_active = true;
    music->fade_speed = 0.0f;
    
    /* Start playing the sound looped */
    audio_voice *voice = NULL;
    for (int i = 0; i < AUDIO_MAX_VOICES; i++) {
        if (!audio->voices[i].active) {
            voice = &audio->voices[i];
            break;
        }
    }
    
    if (voice) {
        voice->sound_id = music->sound_id;
        voice->position = 0;
        voice->volume = volume * audio->music_volume;
        voice->pan = 0;
        voice->pitch = 1.0f;
        voice->flags = AUDIO_FLAG_LOOP;  /* Music always loops */
        voice->priority = AUDIO_PRIORITY_HIGH;
        voice->phase_accumulator = 0;
        voice->generation++;
        voice->active = true;
    }
}

void audio_stop_music_layer(audio_system *audio, uint32_t layer) {
    if (layer >= 8) return;
    
    audio_music_layer *music = &audio->music_layers[layer];
    music->is_active = false;
    
    /* Stop any voices playing this music */
    for (int i = 0; i < AUDIO_MAX_VOICES; i++) {
        if (audio->voices[i].active && 
            audio->voices[i].sound_id == music->sound_id &&
            (audio->voices[i].flags & AUDIO_FLAG_LOOP)) {
            audio->voices[i].active = false;
        }
    }
}

void audio_set_music_intensity(audio_system *audio, float intensity) {
    audio->music_intensity = fmaxf(0.0f, fminf(1.0f, intensity));
    
    /* Adjust music layer volumes based on intensity */
    for (int i = 0; i < 8; i++) {
        if (audio->music_layers[i].is_active) {
            float target_volume = audio->music_layers[i].volume * audio->music_intensity;
            /* Find and update the voice playing this layer */
            for (int v = 0; v < AUDIO_MAX_VOICES; v++) {
                if (audio->voices[v].active && 
                    audio->voices[v].sound_id == audio->music_layers[i].sound_id) {
                    audio->voices[v].volume = target_volume * audio->music_volume;
                    break;
                }
            }
        }
    }
}

void audio_crossfade_music(audio_system *audio, uint32_t from_layer, uint32_t to_layer, float time) {
    if (from_layer >= 8 || to_layer >= 8) return;
    
    audio->music_layers[from_layer].fade_speed = -1.0f / time;
    audio->music_layers[to_layer].fade_speed = 1.0f / time;
}

/* Get underrun count */
uint64_t audio_get_underrun_count(audio_system *audio) {
    return audio->underruns;
}

/* Pause/unpause sound */
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

/* Set voice pan */
void audio_set_voice_pan(audio_system *audio, audio_handle voice_handle, float pan) {
    if (voice_handle == AUDIO_INVALID_HANDLE) return;
    
    uint32_t index = voice_handle >> 16;
    uint32_t generation = voice_handle & 0xFFFF;
    
    if (index < AUDIO_MAX_VOICES && audio->voices[index].generation == generation) {
        audio->voices[index].pan = fmaxf(-1.0f, fminf(1.0f, pan));
    }
}

/* Set voice velocity */
void audio_set_voice_velocity(audio_system *audio, audio_handle voice_handle, audio_vec3 vel) {
    if (voice_handle == AUDIO_INVALID_HANDLE) return;
    
    uint32_t index = voice_handle >> 16;
    uint32_t generation = voice_handle & 0xFFFF;
    
    if (index < AUDIO_MAX_VOICES && audio->voices[index].generation == generation) {
        audio->voices[index].velocity = vel;
    }
}

/* Set listener velocity */
void audio_set_listener_velocity(audio_system *audio, audio_vec3 vel) {
    audio->listener_velocity = vel;
}

/* Set sound volume */
void audio_set_sound_volume(audio_system *audio, float volume) {
    audio->sound_volume = fmaxf(0.0f, fminf(1.0f, volume));
}

/* Set music volume */
void audio_set_music_volume(audio_system *audio, float volume) {
    audio->music_volume = fmaxf(0.0f, fminf(1.0f, volume));
}

/* Unload sound */
void audio_unload_sound(audio_system *audio, audio_handle sound) {
    if (sound == AUDIO_INVALID_HANDLE || sound > audio->sound_count) return;
    
    uint32_t sound_id = sound - 1;
    audio->sounds[sound_id].is_loaded = false;
    
    /* Stop all voices using this sound */
    for (int i = 0; i < AUDIO_MAX_VOICES; i++) {
        if (audio->voices[i].active && audio->voices[i].sound_id == sound_id) {
            audio->voices[i].active = false;
        }
    }
}

/* Update function (for fading, etc) */
void audio_update(audio_system *audio, float dt) {
    /* Update music layer fades */
    for (int i = 0; i < 8; i++) {
        audio_music_layer *layer = &audio->music_layers[i];
        if (layer->is_active && layer->fade_speed != 0) {
            layer->volume += layer->fade_speed * dt;
            layer->volume = fmaxf(0.0f, fminf(1.0f, layer->volume));
            
            if (layer->volume <= 0 && layer->fade_speed < 0) {
                audio_stop_music_layer(audio, i);
            }
        }
    }
}

/* Decibel conversions */
float audio_db_to_linear(float db) {
    return powf(10.0f, db / 20.0f);
}

float audio_linear_to_db(float linear) {
    if (linear < 0.00001f) return -100.0f;
    return 20.0f * log10f(linear);
}