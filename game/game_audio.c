// game_audio.c - Procedural audio generation for Crystal Dungeons
// Generates 8-bit style sound effects and music in real-time

#include "crystal_dungeons.h"
#include "../systems/audio/handmade_audio.h"
#include <math.h>
#include <string.h>

// ============================================================================
// AUDIO CONFIGURATION
// ============================================================================

#define SAMPLE_RATE 48000
#define CHANNELS 2
#define BITS_PER_SAMPLE 16
#define AUDIO_BUFFER_SIZE 4096

// Musical notes (frequencies in Hz)
#define NOTE_C3  130.81f
#define NOTE_D3  146.83f
#define NOTE_E3  164.81f
#define NOTE_F3  174.61f
#define NOTE_G3  196.00f
#define NOTE_A3  220.00f
#define NOTE_B3  246.94f
#define NOTE_C4  261.63f
#define NOTE_D4  293.66f
#define NOTE_E4  329.63f
#define NOTE_F4  349.23f
#define NOTE_G4  392.00f
#define NOTE_A4  440.00f
#define NOTE_B4  493.88f
#define NOTE_C5  523.25f

// ============================================================================
// WAVEFORM GENERATORS
// ============================================================================

typedef enum {
    WAVE_SINE,
    WAVE_SQUARE,
    WAVE_TRIANGLE,
    WAVE_SAWTOOTH,
    WAVE_NOISE,
} waveform_type;

static f32 generate_waveform(waveform_type type, f32 phase) {
    switch (type) {
        case WAVE_SINE:
            return sinf(phase * 2.0f * M_PI);
            
        case WAVE_SQUARE:
            return (phase < 0.5f) ? 1.0f : -1.0f;
            
        case WAVE_TRIANGLE:
            if (phase < 0.25f) {
                return phase * 4.0f;
            } else if (phase < 0.75f) {
                return 2.0f - (phase * 4.0f);
            } else {
                return (phase * 4.0f) - 4.0f;
            }
            
        case WAVE_SAWTOOTH:
            return 2.0f * phase - 1.0f;
            
        case WAVE_NOISE:
            return ((f32)rand() / (f32)RAND_MAX) * 2.0f - 1.0f;
            
        default:
            return 0.0f;
    }
}

// ============================================================================
// ENVELOPE GENERATOR (ADSR)
// ============================================================================

typedef struct envelope {
    f32 attack;     // Time to reach peak (seconds)
    f32 decay;      // Time to decay to sustain (seconds)
    f32 sustain;    // Sustain level (0-1)
    f32 release;    // Time to fade out (seconds)
} envelope;

static f32 apply_envelope(envelope* env, f32 time, f32 note_duration, bool released) {
    if (!released && time < note_duration) {
        // Attack phase
        if (time < env->attack) {
            return time / env->attack;
        }
        // Decay phase
        else if (time < env->attack + env->decay) {
            f32 decay_time = time - env->attack;
            f32 decay_progress = decay_time / env->decay;
            return 1.0f - (decay_progress * (1.0f - env->sustain));
        }
        // Sustain phase
        else {
            return env->sustain;
        }
    }
    // Release phase
    else {
        f32 release_time = time - note_duration;
        if (release_time < env->release) {
            return env->sustain * (1.0f - (release_time / env->release));
        }
        return 0.0f;
    }
}

// ============================================================================
// SOUND EFFECT GENERATORS
// ============================================================================

typedef struct sfx_generator {
    waveform_type wave;
    f32 frequency;
    f32 frequency_slide;
    envelope env;
    f32 duration;
    f32 volume;
    
    // Modulation
    f32 vibrato_depth;
    f32 vibrato_speed;
    f32 pitch_bend;
    
    // Effects
    f32 distortion;
    f32 low_pass_cutoff;
    f32 echo_delay;
    f32 echo_feedback;
} sfx_generator;

// Predefined sound effects
static sfx_generator sfx_sword_swing = {
    .wave = WAVE_NOISE,
    .frequency = 200.0f,
    .frequency_slide = -150.0f,
    .env = { .attack = 0.01f, .decay = 0.05f, .sustain = 0.2f, .release = 0.1f },
    .duration = 0.2f,
    .volume = 0.7f,
};

static sfx_generator sfx_enemy_hit = {
    .wave = WAVE_SQUARE,
    .frequency = 150.0f,
    .frequency_slide = -50.0f,
    .env = { .attack = 0.01f, .decay = 0.02f, .sustain = 0.0f, .release = 0.1f },
    .duration = 0.15f,
    .volume = 0.8f,
    .distortion = 0.3f,
};

static sfx_generator sfx_player_hurt = {
    .wave = WAVE_SAWTOOTH,
    .frequency = 100.0f,
    .frequency_slide = -30.0f,
    .env = { .attack = 0.01f, .decay = 0.1f, .sustain = 0.3f, .release = 0.2f },
    .duration = 0.3f,
    .volume = 0.9f,
};

static sfx_generator sfx_item_pickup = {
    .wave = WAVE_SINE,
    .frequency = NOTE_C4,
    .frequency_slide = NOTE_C5 - NOTE_C4,
    .env = { .attack = 0.01f, .decay = 0.05f, .sustain = 0.5f, .release = 0.2f },
    .duration = 0.3f,
    .volume = 0.6f,
    .vibrato_depth = 0.1f,
    .vibrato_speed = 10.0f,
};

static sfx_generator sfx_door_open = {
    .wave = WAVE_TRIANGLE,
    .frequency = 80.0f,
    .frequency_slide = 20.0f,
    .env = { .attack = 0.1f, .decay = 0.2f, .sustain = 0.4f, .release = 0.3f },
    .duration = 0.5f,
    .volume = 0.5f,
};

static sfx_generator sfx_explosion = {
    .wave = WAVE_NOISE,
    .frequency = 50.0f,
    .frequency_slide = -30.0f,
    .env = { .attack = 0.01f, .decay = 0.2f, .sustain = 0.3f, .release = 0.5f },
    .duration = 0.8f,
    .volume = 1.0f,
    .distortion = 0.5f,
    .low_pass_cutoff = 500.0f,
};

static sfx_generator sfx_magic = {
    .wave = WAVE_SINE,
    .frequency = NOTE_E4,
    .frequency_slide = 100.0f,
    .env = { .attack = 0.05f, .decay = 0.1f, .sustain = 0.6f, .release = 0.3f },
    .duration = 0.4f,
    .volume = 0.5f,
    .vibrato_depth = 0.2f,
    .vibrato_speed = 15.0f,
    .echo_delay = 0.1f,
    .echo_feedback = 0.3f,
};

// Generate sound effect samples
static void generate_sfx(sfx_generator* sfx, f32* buffer, u32 sample_count) {
    f32 phase = 0.0f;
    f32 time = 0.0f;
    f32 time_step = 1.0f / SAMPLE_RATE;
    
    // Echo buffer
    static f32 echo_buffer[SAMPLE_RATE];  // 1 second max echo
    static u32 echo_write_pos = 0;
    
    for (u32 i = 0; i < sample_count; i++) {
        // Calculate current frequency with slide
        f32 freq_progress = time / sfx->duration;
        f32 current_freq = sfx->frequency + (sfx->frequency_slide * freq_progress);
        
        // Apply vibrato
        if (sfx->vibrato_depth > 0) {
            current_freq += sinf(time * sfx->vibrato_speed * 2.0f * M_PI) * 
                           sfx->vibrato_depth * current_freq;
        }
        
        // Generate base waveform
        f32 sample = generate_waveform(sfx->wave, phase);
        
        // Apply envelope
        f32 envelope_value = apply_envelope(&sfx->env, time, sfx->duration, false);
        sample *= envelope_value;
        
        // Apply distortion
        if (sfx->distortion > 0) {
            sample = tanhf(sample * (1.0f + sfx->distortion * 4.0f));
        }
        
        // Simple low-pass filter
        if (sfx->low_pass_cutoff > 0) {
            static f32 prev_sample = 0.0f;
            f32 rc = 1.0f / (2.0f * M_PI * sfx->low_pass_cutoff);
            f32 alpha = time_step / (rc + time_step);
            sample = prev_sample + alpha * (sample - prev_sample);
            prev_sample = sample;
        }
        
        // Apply echo
        if (sfx->echo_delay > 0 && sfx->echo_feedback > 0) {
            u32 echo_delay_samples = (u32)(sfx->echo_delay * SAMPLE_RATE);
            u32 echo_read_pos = (echo_write_pos - echo_delay_samples + SAMPLE_RATE) % SAMPLE_RATE;
            
            f32 echo_sample = echo_buffer[echo_read_pos];
            sample += echo_sample * sfx->echo_feedback;
            
            echo_buffer[echo_write_pos] = sample;
            echo_write_pos = (echo_write_pos + 1) % SAMPLE_RATE;
        }
        
        // Apply volume and store
        buffer[i] = sample * sfx->volume;
        
        // Update phase and time
        phase += current_freq / SAMPLE_RATE;
        if (phase >= 1.0f) phase -= 1.0f;
        time += time_step;
    }
}

// ============================================================================
// MUSIC SEQUENCER
// ============================================================================

typedef struct music_note {
    f32 frequency;
    f32 duration;
    f32 volume;
} music_note;

typedef struct music_track {
    music_note* notes;
    u32 note_count;
    u32 current_note;
    f32 note_timer;
    waveform_type wave;
    envelope env;
    f32 phase;
} music_track;

// Dungeon theme - mysterious and adventurous
static music_note dungeon_melody[] = {
    // Bar 1
    { NOTE_E3, 0.25f, 0.7f },
    { NOTE_G3, 0.25f, 0.7f },
    { NOTE_B3, 0.25f, 0.7f },
    { NOTE_E4, 0.25f, 0.7f },
    
    // Bar 2
    { NOTE_D4, 0.25f, 0.7f },
    { NOTE_B3, 0.25f, 0.7f },
    { NOTE_G3, 0.25f, 0.7f },
    { NOTE_E3, 0.25f, 0.7f },
    
    // Bar 3
    { NOTE_F3, 0.25f, 0.7f },
    { NOTE_A3, 0.25f, 0.7f },
    { NOTE_C4, 0.25f, 0.7f },
    { NOTE_F4, 0.25f, 0.7f },
    
    // Bar 4
    { NOTE_E4, 0.25f, 0.7f },
    { NOTE_C4, 0.25f, 0.7f },
    { NOTE_A3, 0.25f, 0.7f },
    { NOTE_F3, 0.25f, 0.7f },
};

static music_note dungeon_bass[] = {
    // Bar 1
    { NOTE_E3, 1.0f, 0.5f },
    
    // Bar 2
    { NOTE_E3, 1.0f, 0.5f },
    
    // Bar 3
    { NOTE_F3, 1.0f, 0.5f },
    
    // Bar 4
    { NOTE_F3, 1.0f, 0.5f },
};

// Battle theme - intense and fast-paced
static music_note battle_melody[] = {
    // Fast arpeggios
    { NOTE_C4, 0.125f, 0.8f },
    { NOTE_E4, 0.125f, 0.8f },
    { NOTE_G4, 0.125f, 0.8f },
    { NOTE_C5, 0.125f, 0.8f },
    
    { NOTE_B4, 0.125f, 0.8f },
    { NOTE_G4, 0.125f, 0.8f },
    { NOTE_E4, 0.125f, 0.8f },
    { NOTE_B3, 0.125f, 0.8f },
    
    { NOTE_A3, 0.125f, 0.8f },
    { NOTE_C4, 0.125f, 0.8f },
    { NOTE_E4, 0.125f, 0.8f },
    { NOTE_A4, 0.125f, 0.8f },
    
    { NOTE_G4, 0.125f, 0.8f },
    { NOTE_E4, 0.125f, 0.8f },
    { NOTE_C4, 0.125f, 0.8f },
    { NOTE_G3, 0.125f, 0.8f },
};

// ============================================================================
// AUDIO MIXER
// ============================================================================

typedef struct audio_channel {
    f32* buffer;
    u32 buffer_size;
    u32 play_position;
    f32 volume;
    bool is_playing;
    bool loop;
} audio_channel;

typedef struct game_audio_system {
    // Audio channels
    audio_channel sfx_channels[8];
    audio_channel music_channel;
    
    // Music tracks
    music_track melody_track;
    music_track bass_track;
    music_track drum_track;
    
    // Master volume
    f32 master_volume;
    f32 sfx_volume;
    f32 music_volume;
    
    // Current music
    enum {
        MUSIC_NONE,
        MUSIC_DUNGEON,
        MUSIC_BATTLE,
        MUSIC_BOSS,
        MUSIC_VICTORY,
    } current_music;
    
    // Audio buffer
    f32* mix_buffer;
    u32 mix_buffer_size;
    
} game_audio_system;

static game_audio_system* g_audio = NULL;

// ============================================================================
// PUBLIC INTERFACE
// ============================================================================

void game_audio_init(void) {
    if (g_audio) return;
    
    g_audio = (game_audio_system*)calloc(1, sizeof(game_audio_system));
    
    // Initialize volumes
    g_audio->master_volume = 0.8f;
    g_audio->sfx_volume = 0.7f;
    g_audio->music_volume = 0.5f;
    
    // Allocate mix buffer
    g_audio->mix_buffer_size = AUDIO_BUFFER_SIZE;
    g_audio->mix_buffer = (f32*)calloc(g_audio->mix_buffer_size, sizeof(f32));
    
    // Initialize music tracks
    g_audio->melody_track.notes = dungeon_melody;
    g_audio->melody_track.note_count = sizeof(dungeon_melody) / sizeof(music_note);
    g_audio->melody_track.wave = WAVE_SQUARE;
    g_audio->melody_track.env = (envelope){ 0.01f, 0.05f, 0.7f, 0.05f };
    
    g_audio->bass_track.notes = dungeon_bass;
    g_audio->bass_track.note_count = sizeof(dungeon_bass) / sizeof(music_note);
    g_audio->bass_track.wave = WAVE_TRIANGLE;
    g_audio->bass_track.env = (envelope){ 0.01f, 0.1f, 0.8f, 0.1f };
    
    // Allocate SFX buffers
    for (int i = 0; i < 8; i++) {
        g_audio->sfx_channels[i].buffer = (f32*)calloc(SAMPLE_RATE, sizeof(f32));
        g_audio->sfx_channels[i].buffer_size = SAMPLE_RATE;
    }
}

void game_audio_shutdown(void) {
    if (!g_audio) return;
    
    // Free SFX buffers
    for (int i = 0; i < 8; i++) {
        if (g_audio->sfx_channels[i].buffer) {
            free(g_audio->sfx_channels[i].buffer);
        }
    }
    
    // Free mix buffer
    if (g_audio->mix_buffer) {
        free(g_audio->mix_buffer);
    }
    
    free(g_audio);
    g_audio = NULL;
}

// Play a sound effect
void game_audio_play_sfx(sfx_generator* sfx) {
    if (!g_audio) return;
    
    // Find a free channel
    audio_channel* channel = NULL;
    for (int i = 0; i < 8; i++) {
        if (!g_audio->sfx_channels[i].is_playing) {
            channel = &g_audio->sfx_channels[i];
            break;
        }
    }
    
    if (!channel) return;  // No free channels
    
    // Generate the sound effect
    u32 sample_count = (u32)(sfx->duration * SAMPLE_RATE);
    if (sample_count > channel->buffer_size) {
        sample_count = channel->buffer_size;
    }
    
    generate_sfx(sfx, channel->buffer, sample_count);
    
    // Start playback
    channel->play_position = 0;
    channel->volume = g_audio->sfx_volume;
    channel->is_playing = true;
    channel->loop = false;
}

// Play specific sound effects
void game_audio_sword_swing(void) {
    game_audio_play_sfx(&sfx_sword_swing);
}

void game_audio_enemy_hit(void) {
    game_audio_play_sfx(&sfx_enemy_hit);
}

void game_audio_player_hurt(void) {
    game_audio_play_sfx(&sfx_player_hurt);
}

void game_audio_item_pickup(void) {
    game_audio_play_sfx(&sfx_item_pickup);
}

void game_audio_door_open(void) {
    game_audio_play_sfx(&sfx_door_open);
}

void game_audio_explosion(void) {
    game_audio_play_sfx(&sfx_explosion);
}

void game_audio_magic(void) {
    game_audio_play_sfx(&sfx_magic);
}

// Music control
void game_audio_play_music(int music_id) {
    if (!g_audio) return;
    
    g_audio->current_music = music_id;
    
    // Reset track positions
    g_audio->melody_track.current_note = 0;
    g_audio->melody_track.note_timer = 0;
    g_audio->bass_track.current_note = 0;
    g_audio->bass_track.note_timer = 0;
    
    // Switch to appropriate music
    switch (music_id) {
        case MUSIC_BATTLE:
            g_audio->melody_track.notes = battle_melody;
            g_audio->melody_track.note_count = sizeof(battle_melody) / sizeof(music_note);
            break;
            
        default:
            g_audio->melody_track.notes = dungeon_melody;
            g_audio->melody_track.note_count = sizeof(dungeon_melody) / sizeof(music_note);
            break;
    }
}

void game_audio_stop_music(void) {
    if (!g_audio) return;
    g_audio->current_music = MUSIC_NONE;
}

// Generate music samples for a track
static void generate_music_track(music_track* track, f32* buffer, u32 sample_count) {
    f32 time_step = 1.0f / SAMPLE_RATE;
    
    for (u32 i = 0; i < sample_count; i++) {
        if (track->current_note >= track->note_count) {
            track->current_note = 0;  // Loop
        }
        
        music_note* note = &track->notes[track->current_note];
        
        // Generate waveform
        f32 sample = generate_waveform(track->wave, track->phase);
        
        // Apply envelope
        f32 env_value = apply_envelope(&track->env, track->note_timer, note->duration, false);
        sample *= env_value * note->volume;
        
        buffer[i] += sample;
        
        // Update phase
        track->phase += note->frequency / SAMPLE_RATE;
        if (track->phase >= 1.0f) track->phase -= 1.0f;
        
        // Update note timer
        track->note_timer += time_step;
        if (track->note_timer >= note->duration) {
            track->note_timer = 0;
            track->current_note++;
            track->phase = 0;  // Reset phase for new note
        }
    }
}

// Mix all audio and generate output
void game_audio_update(f32* output_buffer, u32 sample_count) {
    if (!g_audio) return;
    
    // Clear mix buffer
    memset(output_buffer, 0, sample_count * sizeof(f32));
    
    // Mix SFX channels
    for (int ch = 0; ch < 8; ch++) {
        audio_channel* channel = &g_audio->sfx_channels[ch];
        if (!channel->is_playing) continue;
        
        for (u32 i = 0; i < sample_count; i++) {
            if (channel->play_position >= channel->buffer_size) {
                if (channel->loop) {
                    channel->play_position = 0;
                } else {
                    channel->is_playing = false;
                    break;
                }
            }
            
            output_buffer[i] += channel->buffer[channel->play_position++] * 
                               channel->volume * g_audio->sfx_volume;
        }
    }
    
    // Generate and mix music
    if (g_audio->current_music != MUSIC_NONE) {
        // Temporary buffer for music
        f32* music_buffer = (f32*)calloc(sample_count, sizeof(f32));
        
        // Generate tracks
        generate_music_track(&g_audio->melody_track, music_buffer, sample_count);
        generate_music_track(&g_audio->bass_track, music_buffer, sample_count);
        
        // Mix music into output
        for (u32 i = 0; i < sample_count; i++) {
            output_buffer[i] += music_buffer[i] * g_audio->music_volume;
        }
        
        free(music_buffer);
    }
    
    // Apply master volume and clipping
    for (u32 i = 0; i < sample_count; i++) {
        output_buffer[i] *= g_audio->master_volume;
        
        // Soft clipping
        if (output_buffer[i] > 1.0f) {
            output_buffer[i] = tanhf(output_buffer[i]);
        } else if (output_buffer[i] < -1.0f) {
            output_buffer[i] = tanhf(output_buffer[i]);
        }
    }
}

// Set volume levels
void game_audio_set_master_volume(f32 volume) {
    if (g_audio) {
        g_audio->master_volume = fmaxf(0.0f, fminf(1.0f, volume));
    }
}

void game_audio_set_sfx_volume(f32 volume) {
    if (g_audio) {
        g_audio->sfx_volume = fmaxf(0.0f, fminf(1.0f, volume));
    }
}

void game_audio_set_music_volume(f32 volume) {
    if (g_audio) {
        g_audio->music_volume = fmaxf(0.0f, fminf(1.0f, volume));
    }
}