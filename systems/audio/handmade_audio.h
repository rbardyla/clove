/*
 * Handmade Audio System
 * Zero-dependency, low-latency audio engine with ALSA backend
 * 
 * PERFORMANCE TARGETS:
 * - <10ms latency
 * - 100+ simultaneous sounds  
 * - <5% CPU usage
 * - Zero allocations during playback
 */

#ifndef HANDMADE_AUDIO_H
#define HANDMADE_AUDIO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Configuration constants */
#define AUDIO_SAMPLE_RATE 48000
#define AUDIO_CHANNELS 2
#define AUDIO_BITS_PER_SAMPLE 16
#define AUDIO_BUFFER_FRAMES 512  /* ~10.6ms latency at 48kHz */
#define AUDIO_MAX_VOICES 128
#define AUDIO_MAX_EFFECTS 8
#define AUDIO_RING_BUFFER_SIZE (AUDIO_BUFFER_FRAMES * 4)

/* Fixed-point math for performance */
#define AUDIO_FIXED_SHIFT 16
#define AUDIO_FIXED_ONE (1 << AUDIO_FIXED_SHIFT)
#define AUDIO_FLOAT_TO_FIXED(x) ((int32_t)((x) * AUDIO_FIXED_ONE))
#define AUDIO_FIXED_TO_FLOAT(x) ((float)(x) / AUDIO_FIXED_ONE)

/* Sound flags */
#define AUDIO_FLAG_LOOP      0x01
#define AUDIO_FLAG_STREAMING 0x02
#define AUDIO_FLAG_3D        0x04
#define AUDIO_FLAG_PAUSED    0x08

/* Effect types */
typedef enum {
    AUDIO_EFFECT_NONE = 0,
    AUDIO_EFFECT_REVERB,
    AUDIO_EFFECT_LOWPASS,
    AUDIO_EFFECT_HIGHPASS,
    AUDIO_EFFECT_ECHO,
    AUDIO_EFFECT_COMPRESSOR,
    AUDIO_EFFECT_DISTORTION,
    AUDIO_EFFECT_CHORUS,
    AUDIO_EFFECT_FLANGER
} audio_effect_type;

/* Sound priority for voice stealing */
typedef enum {
    AUDIO_PRIORITY_LOW = 0,
    AUDIO_PRIORITY_NORMAL = 1,
    AUDIO_PRIORITY_HIGH = 2,
    AUDIO_PRIORITY_CRITICAL = 3
} audio_priority;

/* 3D vector for spatial audio */
typedef struct {
    float x, y, z;
} audio_vec3;

/* Audio format descriptor */
typedef struct {
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    uint32_t frame_count;
} audio_format;

/* Sound buffer - holds audio data */
typedef struct {
    int16_t *samples;
    uint32_t frame_count;
    uint32_t channels;
    uint32_t sample_rate;
    uint32_t size_bytes;
    bool is_loaded;
} audio_sound_buffer;

/* Playing voice instance */
typedef struct {
    uint32_t sound_id;
    uint32_t position;      /* Current playback position in frames */
    float volume;           /* 0.0 to 1.0 */
    float pan;              /* -1.0 (left) to 1.0 (right) */
    float pitch;            /* 1.0 = normal speed */
    uint32_t flags;
    audio_priority priority;
    
    /* 3D audio properties */
    audio_vec3 position_3d;
    audio_vec3 velocity;
    float min_distance;
    float max_distance;
    
    /* Effect sends */
    float effect_send[AUDIO_MAX_EFFECTS];
    
    /* Internal state */
    float phase_accumulator;  /* For resampling */
    bool active;
    uint32_t generation;      /* For handle validation */
} audio_voice;

/* DSP effect instance */
typedef struct {
    audio_effect_type type;
    bool enabled;
    float mix;  /* Dry/wet mix 0.0 to 1.0 */
    
    union {
        struct {  /* Reverb parameters */
            float room_size;
            float damping;
            float wet_level;
            float dry_level;
            float width;
        } reverb;
        
        struct {  /* Filter parameters */
            float cutoff;
            float resonance;
        } filter;
        
        struct {  /* Echo parameters */
            float delay_ms;
            float feedback;
            float mix;
        } echo;
        
        struct {  /* Compressor parameters */
            float threshold;
            float ratio;
            float attack_ms;
            float release_ms;
        } compressor;
    } params;
    
    /* Internal DSP state - implementation specific */
    void *state;
} audio_effect;

/* Music layer for dynamic music */
typedef struct {
    uint32_t sound_id;
    float volume;
    float fade_speed;
    bool is_active;
    uint32_t sync_point;  /* Frame position for syncing layers */
} audio_music_layer;

/* Main audio system state */
typedef struct {
    /* ALSA handles */
    void *pcm_handle;
    void *hw_params;
    
    /* Ring buffer for lock-free audio */
    int16_t *ring_buffer;
    volatile uint32_t write_pos;
    volatile uint32_t read_pos;
    
    /* Sound storage */
    audio_sound_buffer *sounds;
    uint32_t max_sounds;
    uint32_t sound_count;
    
    /* Voice pool */
    audio_voice voices[AUDIO_MAX_VOICES];
    uint32_t voice_generation;
    
    /* Effects rack */
    audio_effect effects[AUDIO_MAX_EFFECTS];
    
    /* Music system */
    audio_music_layer music_layers[8];
    float music_intensity;  /* 0.0 to 1.0 for dynamic music */
    
    /* Listener for 3D audio */
    audio_vec3 listener_position;
    audio_vec3 listener_forward;
    audio_vec3 listener_up;
    audio_vec3 listener_velocity;
    
    /* Master controls */
    float master_volume;
    float sound_volume;
    float music_volume;
    
    /* Threading */
    void *audio_thread;
    volatile bool running;
    
    /* Performance counters */
    uint64_t frames_processed;
    uint64_t underruns;
    float cpu_usage;
    uint32_t active_voices;
    
    /* Memory pools */
    void *memory_pool;
    size_t memory_size;
    size_t memory_used;
} audio_system;

/* Handle type for sounds and voices */
typedef uint32_t audio_handle;
#define AUDIO_INVALID_HANDLE 0

/* Core system functions */
bool audio_init(audio_system *audio, size_t memory_size);
void audio_shutdown(audio_system *audio);
void audio_update(audio_system *audio, float dt);

/* Sound loading/management */
audio_handle audio_load_wav(audio_system *audio, const char *path);
audio_handle audio_load_wav_from_memory(audio_system *audio, const void *data, size_t size);
void audio_unload_sound(audio_system *audio, audio_handle sound);

/* Playback control */
audio_handle audio_play_sound(audio_system *audio, audio_handle sound, float volume, float pan);
audio_handle audio_play_sound_3d(audio_system *audio, audio_handle sound, audio_vec3 pos, float volume);
void audio_stop_sound(audio_system *audio, audio_handle voice);
void audio_pause_sound(audio_system *audio, audio_handle voice, bool pause);

/* Voice control */
void audio_set_voice_volume(audio_system *audio, audio_handle voice, float volume);
void audio_set_voice_pan(audio_system *audio, audio_handle voice, float pan);
void audio_set_voice_pitch(audio_system *audio, audio_handle voice, float pitch);
void audio_set_voice_position_3d(audio_system *audio, audio_handle voice, audio_vec3 pos);
void audio_set_voice_velocity(audio_system *audio, audio_handle voice, audio_vec3 vel);

/* Effects control */
void audio_enable_effect(audio_system *audio, uint32_t slot, audio_effect_type type);
void audio_disable_effect(audio_system *audio, uint32_t slot);
void audio_set_reverb_params(audio_system *audio, uint32_t slot, float room_size, float damping);
void audio_set_filter_params(audio_system *audio, uint32_t slot, float cutoff, float resonance);
void audio_set_echo_params(audio_system *audio, uint32_t slot, float delay_ms, float feedback);

/* Music system */
void audio_play_music_layer(audio_system *audio, uint32_t layer, audio_handle sound, float volume);
void audio_stop_music_layer(audio_system *audio, uint32_t layer);
void audio_set_music_intensity(audio_system *audio, float intensity);
void audio_crossfade_music(audio_system *audio, uint32_t from_layer, uint32_t to_layer, float time);

/* 3D listener control */
void audio_set_listener_position(audio_system *audio, audio_vec3 pos);
void audio_set_listener_orientation(audio_system *audio, audio_vec3 forward, audio_vec3 up);
void audio_set_listener_velocity(audio_system *audio, audio_vec3 vel);

/* Master volume control */
void audio_set_master_volume(audio_system *audio, float volume);
void audio_set_sound_volume(audio_system *audio, float volume);
void audio_set_music_volume(audio_system *audio, float volume);

/* Performance monitoring */
float audio_get_cpu_usage(audio_system *audio);
uint32_t audio_get_active_voices(audio_system *audio);
uint64_t audio_get_underrun_count(audio_system *audio);

/* Utility functions */
float audio_db_to_linear(float db);
float audio_linear_to_db(float linear);
audio_vec3 audio_vec3_normalize(audio_vec3 v);
float audio_vec3_distance(audio_vec3 a, audio_vec3 b);

#endif /* HANDMADE_AUDIO_H */