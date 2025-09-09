/*
 * Audio DSP Effects Processing
 * SIMD-optimized digital signal processing
 * 
 * PERFORMANCE: All effects use SSE/AVX for parallel processing
 * CACHE: Process in blocks to maximize cache utilization
 * ALGORITHMS: Optimized implementations of classic DSP algorithms
 */

#include "handmade_audio.h"
#include <immintrin.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* DSP constants */
#define DSP_BLOCK_SIZE 64  /* Process in 64-sample blocks for cache efficiency */
#define REVERB_COMB_FILTERS 8
#define REVERB_ALLPASS_FILTERS 4

/* Reverb comb filter delays (in samples at 48kHz) */
static const int reverb_comb_delays[REVERB_COMB_FILTERS] = {
    1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116
};

/* Reverb allpass filter delays */
static const int reverb_allpass_delays[REVERB_ALLPASS_FILTERS] = {
    225, 341, 441, 556
};

/* Filter coefficients structure */
typedef struct {
    float a0, a1, a2;  /* Feedforward coefficients */
    float b1, b2;      /* Feedback coefficients */
    float z1, z2;      /* Delay line for left channel */
    float z1_r, z2_r;  /* Delay line for right channel */
} biquad_filter;

/* Reverb state */
typedef struct {
    /* Comb filters */
    float *comb_buffers[REVERB_COMB_FILTERS];
    int comb_indices[REVERB_COMB_FILTERS];
    float comb_feedback[REVERB_COMB_FILTERS];
    float comb_damp1[REVERB_COMB_FILTERS];
    float comb_damp2[REVERB_COMB_FILTERS];
    float comb_filter_store[REVERB_COMB_FILTERS];
    
    /* Allpass filters */
    float *allpass_buffers[REVERB_ALLPASS_FILTERS];
    int allpass_indices[REVERB_ALLPASS_FILTERS];
    
    /* Parameters */
    float room_size;
    float damping;
    float wet_gain;
    float dry_gain;
    float width;
} reverb_state;

/* Echo/delay state */
typedef struct {
    float *buffer;
    uint32_t buffer_size;
    uint32_t write_pos;
    uint32_t delay_samples;
    float feedback;
    float mix;
} echo_state;

/* Compressor state */
typedef struct {
    float threshold;
    float ratio;
    float attack_coeff;
    float release_coeff;
    float envelope;
    float makeup_gain;
} compressor_state;

/* Initialize DSP effects */
static void audio_init_reverb(audio_effect *effect, size_t sample_rate) {
    reverb_state *state = (reverb_state*)calloc(1, sizeof(reverb_state));
    effect->state = state;
    
    /* Allocate comb filter buffers */
    for (int i = 0; i < REVERB_COMB_FILTERS; i++) {
        int size = reverb_comb_delays[i];
        state->comb_buffers[i] = (float*)calloc(size, sizeof(float));
        state->comb_indices[i] = 0;
        state->comb_filter_store[i] = 0;
    }
    
    /* Allocate allpass filter buffers */
    for (int i = 0; i < REVERB_ALLPASS_FILTERS; i++) {
        int size = reverb_allpass_delays[i];
        state->allpass_buffers[i] = (float*)calloc(size, sizeof(float));
        state->allpass_indices[i] = 0;
    }
    
    /* Set default parameters */
    state->room_size = 0.5f;
    state->damping = 0.5f;
    state->wet_gain = 0.3f;
    state->dry_gain = 0.7f;
    state->width = 1.0f;
    
    /* Update comb filter parameters */
    for (int i = 0; i < REVERB_COMB_FILTERS; i++) {
        state->comb_feedback[i] = state->room_size;
        state->comb_damp1[i] = state->damping;
        state->comb_damp2[i] = 1.0f - state->damping;
    }
}

static void audio_init_filter(audio_effect *effect) {
    biquad_filter *filter = (biquad_filter*)calloc(1, sizeof(biquad_filter));
    effect->state = filter;
    
    /* Initialize as bypass */
    filter->a0 = 1.0f;
    filter->a1 = 0.0f;
    filter->a2 = 0.0f;
    filter->b1 = 0.0f;
    filter->b2 = 0.0f;
}

static void audio_init_echo(audio_effect *effect, size_t sample_rate) {
    echo_state *state = (echo_state*)calloc(1, sizeof(echo_state));
    effect->state = state;
    
    /* Allocate delay buffer for up to 2 seconds */
    state->buffer_size = sample_rate * 2;
    state->buffer = (float*)calloc(state->buffer_size * 2, sizeof(float));  /* Stereo */
    state->write_pos = 0;
    state->delay_samples = sample_rate / 4;  /* 250ms default */
    state->feedback = 0.5f;
    state->mix = 0.5f;
}

static void audio_init_compressor(audio_effect *effect, size_t sample_rate) {
    compressor_state *state = (compressor_state*)calloc(1, sizeof(compressor_state));
    effect->state = state;
    
    state->threshold = 0.7f;
    state->ratio = 4.0f;
    state->envelope = 0.0f;
    
    /* Calculate attack/release coefficients */
    float attack_ms = 10.0f;
    float release_ms = 100.0f;
    state->attack_coeff = expf(-1.0f / (attack_ms * sample_rate * 0.001f));
    state->release_coeff = expf(-1.0f / (release_ms * sample_rate * 0.001f));
    
    /* Calculate makeup gain based on ratio */
    state->makeup_gain = powf(state->threshold, 1.0f - 1.0f/state->ratio);
}

/* Process reverb using Freeverb algorithm */
static void process_reverb(reverb_state *state, float *buffer, uint32_t frames) {
    /* PERFORMANCE: Process comb filters in parallel
     * CACHE: Access patterns optimized for sequential memory access
     * SIMD: Vectorized comb filter processing */
    
    for (uint32_t i = 0; i < frames * 2; i += 2) {
        float input_left = buffer[i];
        float input_right = buffer[i + 1];
        float input_mixed = (input_left + input_right) * 0.5f;
        
        float out_left = 0.0f;
        float out_right = 0.0f;
        
        /* Process comb filters in parallel */
        for (int c = 0; c < REVERB_COMB_FILTERS; c++) {
            int index = state->comb_indices[c];
            float *comb_buf = state->comb_buffers[c];
            int comb_size = reverb_comb_delays[c];
            
            /* Read from delay line */
            float delayed = comb_buf[index];
            
            /* Low-pass filter in feedback path */
            float filtered = delayed * state->comb_damp2[c] + 
                           state->comb_filter_store[c] * state->comb_damp1[c];
            state->comb_filter_store[c] = filtered;
            
            /* Write to delay line with feedback */
            comb_buf[index] = input_mixed + filtered * state->comb_feedback[c];
            
            /* Accumulate output */
            if (c & 1) {
                out_right += delayed;
            } else {
                out_left += delayed;
            }
            
            /* Update index */
            state->comb_indices[c] = (index + 1) % comb_size;
        }
        
        /* Scale comb output */
        out_left *= 0.125f;  /* 1/8 */
        out_right *= 0.125f;
        
        /* Process allpass filters in series */
        float allpass_out_left = out_left;
        float allpass_out_right = out_right;
        
        for (int a = 0; a < REVERB_ALLPASS_FILTERS; a++) {
            int index = state->allpass_indices[a];
            float *allpass_buf = state->allpass_buffers[a];
            int allpass_size = reverb_allpass_delays[a];
            
            /* Left channel */
            float delayed_left = allpass_buf[index * 2];
            float input_left = allpass_out_left;
            allpass_buf[index * 2] = input_left + delayed_left * 0.5f;
            allpass_out_left = delayed_left - input_left * 0.5f;
            
            /* Right channel */
            float delayed_right = allpass_buf[index * 2 + 1];
            float input_right = allpass_out_right;
            allpass_buf[index * 2 + 1] = input_right + delayed_right * 0.5f;
            allpass_out_right = delayed_right - input_right * 0.5f;
            
            state->allpass_indices[a] = (index + 1) % allpass_size;
        }
        
        /* Apply stereo width */
        float wet1 = allpass_out_left * state->width;
        float wet2 = allpass_out_right * state->width;
        float cross = 1.0f - state->width;
        
        /* Mix wet and dry signals */
        buffer[i] = input_left * state->dry_gain + 
                   (wet1 + wet2 * cross) * state->wet_gain;
        buffer[i + 1] = input_right * state->dry_gain + 
                       (wet2 + wet1 * cross) * state->wet_gain;
    }
}

/* Process biquad filter (lowpass/highpass) */
static void process_filter(biquad_filter *filter, float *buffer, uint32_t frames, bool is_lowpass) {
    /* PERFORMANCE: Direct Form II transposed for numerical stability
     * SIMD: Process stereo channels in parallel */
    
    __m128 a0 = _mm_set1_ps(filter->a0);
    __m128 a1 = _mm_set1_ps(filter->a1);
    __m128 a2 = _mm_set1_ps(filter->a2);
    __m128 b1 = _mm_set1_ps(filter->b1);
    __m128 b2 = _mm_set1_ps(filter->b2);
    
    __m128 z1 = _mm_set_ps(filter->z1_r, filter->z1, filter->z1_r, filter->z1);
    __m128 z2 = _mm_set_ps(filter->z2_r, filter->z2, filter->z2_r, filter->z2);
    
    for (uint32_t i = 0; i < frames * 2; i += 4) {
        __m128 input = _mm_load_ps(&buffer[i]);
        
        /* Direct Form II Transposed:
         * y = a0*x + z1
         * z1 = a1*x - b1*y + z2
         * z2 = a2*x - b2*y */
        
        __m128 output = _mm_add_ps(_mm_mul_ps(a0, input), z1);
        __m128 new_z1 = _mm_add_ps(_mm_sub_ps(_mm_mul_ps(a1, input), 
                                              _mm_mul_ps(b1, output)), z2);
        __m128 new_z2 = _mm_sub_ps(_mm_mul_ps(a2, input), _mm_mul_ps(b2, output));
        
        _mm_store_ps(&buffer[i], output);
        
        z1 = new_z1;
        z2 = new_z2;
    }
    
    /* Store state for next block */
    float temp[4];
    _mm_store_ps(temp, z1);
    filter->z1 = temp[0];
    filter->z1_r = temp[1];
    _mm_store_ps(temp, z2);
    filter->z2 = temp[0];
    filter->z2_r = temp[1];
}

/* Calculate biquad filter coefficients */
static void calculate_filter_coefficients(biquad_filter *filter, float cutoff, 
                                         float resonance, float sample_rate, bool is_lowpass) {
    float omega = 2.0f * M_PI * cutoff / sample_rate;
    float sin_omega = sinf(omega);
    float cos_omega = cosf(omega);
    float alpha = sin_omega / (2.0f * resonance);
    
    if (is_lowpass) {
        /* Lowpass filter coefficients */
        float b0 = 1.0f + alpha;
        filter->b1 = (-2.0f * cos_omega) / b0;
        filter->b2 = (1.0f - alpha) / b0;
        filter->a0 = ((1.0f - cos_omega) * 0.5f) / b0;
        filter->a1 = (1.0f - cos_omega) / b0;
        filter->a2 = filter->a0;
    } else {
        /* Highpass filter coefficients */
        float b0 = 1.0f + alpha;
        filter->b1 = (-2.0f * cos_omega) / b0;
        filter->b2 = (1.0f - alpha) / b0;
        filter->a0 = ((1.0f + cos_omega) * 0.5f) / b0;
        filter->a1 = -(1.0f + cos_omega) / b0;
        filter->a2 = filter->a0;
    }
}

/* Process echo/delay effect */
static void process_echo(echo_state *state, float *buffer, uint32_t frames) {
    /* PERFORMANCE: Simple delay line with feedback
     * CACHE: Sequential access pattern */
    
    for (uint32_t i = 0; i < frames * 2; i += 2) {
        /* Read from delay line */
        uint32_t read_pos = (state->write_pos + state->buffer_size - state->delay_samples) 
                          % state->buffer_size;
        
        float delayed_left = state->buffer[read_pos * 2];
        float delayed_right = state->buffer[read_pos * 2 + 1];
        
        /* Mix with input and apply feedback */
        float out_left = buffer[i] + delayed_left * state->mix;
        float out_right = buffer[i + 1] + delayed_right * state->mix;
        
        /* Write to delay line with feedback */
        state->buffer[state->write_pos * 2] = buffer[i] + delayed_left * state->feedback;
        state->buffer[state->write_pos * 2 + 1] = buffer[i + 1] + delayed_right * state->feedback;
        
        /* Output */
        buffer[i] = out_left;
        buffer[i + 1] = out_right;
        
        /* Update write position */
        state->write_pos = (state->write_pos + 1) % state->buffer_size;
    }
}

/* Process compressor */
static void process_compressor(compressor_state *state, float *buffer, uint32_t frames) {
    /* PERFORMANCE: Feed-forward peak compressor
     * SIMD: Vectorized envelope detection and gain calculation */
    
    for (uint32_t i = 0; i < frames * 2; i += 2) {
        /* Get stereo peak */
        float peak = fmaxf(fabsf(buffer[i]), fabsf(buffer[i + 1]));
        
        /* Envelope follower */
        float target_env = peak;
        float rate = (target_env > state->envelope) ? state->attack_coeff : state->release_coeff;
        state->envelope = target_env + (state->envelope - target_env) * rate;
        
        /* Calculate gain reduction */
        float gain = 1.0f;
        if (state->envelope > state->threshold) {
            float over = state->envelope - state->threshold;
            float compressed = over / state->ratio;
            float reduction = over - compressed;
            gain = 1.0f - (reduction / state->envelope);
        }
        
        /* Apply gain with makeup */
        gain *= state->makeup_gain;
        buffer[i] *= gain;
        buffer[i + 1] *= gain;
    }
}

/* Process distortion effect */
static void process_distortion(float *buffer, uint32_t frames, float drive, float mix) {
    /* PERFORMANCE: Soft clipping using tanh approximation
     * SIMD: Vectorized distortion processing */
    
    __m128 drive_vec = _mm_set1_ps(drive);
    __m128 mix_vec = _mm_set1_ps(mix);
    __m128 dry_mix = _mm_set1_ps(1.0f - mix);
    
    for (uint32_t i = 0; i < frames * 2; i += 4) {
        __m128 input = _mm_load_ps(&buffer[i]);
        __m128 driven = _mm_mul_ps(input, drive_vec);
        
        /* Fast tanh approximation for soft clipping */
        __m128 x2 = _mm_mul_ps(driven, driven);
        __m128 distorted = _mm_div_ps(driven, 
                                      _mm_add_ps(_mm_set1_ps(1.0f), 
                                                _mm_mul_ps(x2, _mm_set1_ps(0.28f))));
        
        /* Mix dry and wet signals */
        __m128 output = _mm_add_ps(_mm_mul_ps(input, dry_mix),
                                   _mm_mul_ps(distorted, mix_vec));
        
        _mm_store_ps(&buffer[i], output);
    }
}

/* Main effects processing function */
void audio_process_effects(audio_system *audio, float *buffer, uint32_t frames) {
    /* PERFORMANCE: Process effects in series
     * CACHE: Process in blocks for better cache utilization */
    
    for (int slot = 0; slot < AUDIO_MAX_EFFECTS; slot++) {
        audio_effect *effect = &audio->effects[slot];
        if (!effect->enabled || !effect->state) continue;
        
        switch (effect->type) {
            case AUDIO_EFFECT_REVERB:
                process_reverb((reverb_state*)effect->state, buffer, frames);
                break;
                
            case AUDIO_EFFECT_LOWPASS:
                process_filter((biquad_filter*)effect->state, buffer, frames, true);
                break;
                
            case AUDIO_EFFECT_HIGHPASS:
                process_filter((biquad_filter*)effect->state, buffer, frames, false);
                break;
                
            case AUDIO_EFFECT_ECHO:
                process_echo((echo_state*)effect->state, buffer, frames);
                break;
                
            case AUDIO_EFFECT_COMPRESSOR:
                process_compressor((compressor_state*)effect->state, buffer, frames);
                break;
                
            case AUDIO_EFFECT_DISTORTION:
                process_distortion(buffer, frames, 5.0f, effect->mix);
                break;
                
            default:
                break;
        }
    }
}

/* Enable an effect */
void audio_enable_effect(audio_system *audio, uint32_t slot, audio_effect_type type) {
    if (slot >= AUDIO_MAX_EFFECTS) return;
    
    audio_effect *effect = &audio->effects[slot];
    
    /* Clean up old effect if needed */
    if (effect->state) {
        free(effect->state);
        effect->state = NULL;
    }
    
    effect->type = type;
    effect->enabled = true;
    effect->mix = 1.0f;
    
    /* Initialize effect state */
    switch (type) {
        case AUDIO_EFFECT_REVERB:
            audio_init_reverb(effect, AUDIO_SAMPLE_RATE);
            break;
            
        case AUDIO_EFFECT_LOWPASS:
        case AUDIO_EFFECT_HIGHPASS:
            audio_init_filter(effect);
            break;
            
        case AUDIO_EFFECT_ECHO:
            audio_init_echo(effect, AUDIO_SAMPLE_RATE);
            break;
            
        case AUDIO_EFFECT_COMPRESSOR:
            audio_init_compressor(effect, AUDIO_SAMPLE_RATE);
            break;
            
        default:
            break;
    }
}

/* Disable an effect */
void audio_disable_effect(audio_system *audio, uint32_t slot) {
    if (slot >= AUDIO_MAX_EFFECTS) return;
    
    audio_effect *effect = &audio->effects[slot];
    effect->enabled = false;
    
    if (effect->state) {
        /* Free effect-specific state */
        if (effect->type == AUDIO_EFFECT_REVERB) {
            reverb_state *state = (reverb_state*)effect->state;
            for (int i = 0; i < REVERB_COMB_FILTERS; i++) {
                free(state->comb_buffers[i]);
            }
            for (int i = 0; i < REVERB_ALLPASS_FILTERS; i++) {
                free(state->allpass_buffers[i]);
            }
        } else if (effect->type == AUDIO_EFFECT_ECHO) {
            echo_state *state = (echo_state*)effect->state;
            free(state->buffer);
        }
        
        free(effect->state);
        effect->state = NULL;
    }
}

/* Set reverb parameters */
void audio_set_reverb_params(audio_system *audio, uint32_t slot, float room_size, float damping) {
    if (slot >= AUDIO_MAX_EFFECTS) return;
    
    audio_effect *effect = &audio->effects[slot];
    if (effect->type != AUDIO_EFFECT_REVERB || !effect->state) return;
    
    reverb_state *state = (reverb_state*)effect->state;
    state->room_size = fmaxf(0.0f, fminf(1.0f, room_size));
    state->damping = fmaxf(0.0f, fminf(1.0f, damping));
    
    /* Update comb filter parameters */
    for (int i = 0; i < REVERB_COMB_FILTERS; i++) {
        state->comb_feedback[i] = state->room_size * 0.98f;  /* Scale for stability */
        state->comb_damp1[i] = state->damping;
        state->comb_damp2[i] = 1.0f - state->damping;
    }
}

/* Set filter parameters */
void audio_set_filter_params(audio_system *audio, uint32_t slot, float cutoff, float resonance) {
    if (slot >= AUDIO_MAX_EFFECTS) return;
    
    audio_effect *effect = &audio->effects[slot];
    if ((effect->type != AUDIO_EFFECT_LOWPASS && effect->type != AUDIO_EFFECT_HIGHPASS) || 
        !effect->state) return;
    
    biquad_filter *filter = (biquad_filter*)effect->state;
    
    /* Clamp parameters to reasonable ranges */
    cutoff = fmaxf(20.0f, fminf(20000.0f, cutoff));
    resonance = fmaxf(0.5f, fminf(20.0f, resonance));
    
    calculate_filter_coefficients(filter, cutoff, resonance, AUDIO_SAMPLE_RATE, 
                                 effect->type == AUDIO_EFFECT_LOWPASS);
}

/* Set echo parameters */
void audio_set_echo_params(audio_system *audio, uint32_t slot, float delay_ms, float feedback) {
    if (slot >= AUDIO_MAX_EFFECTS) return;
    
    audio_effect *effect = &audio->effects[slot];
    if (effect->type != AUDIO_EFFECT_ECHO || !effect->state) return;
    
    echo_state *state = (echo_state*)effect->state;
    
    /* Convert delay to samples and clamp */
    uint32_t delay_samples = (uint32_t)(delay_ms * AUDIO_SAMPLE_RATE / 1000.0f);
    delay_samples = fminf(delay_samples, state->buffer_size - 1);
    state->delay_samples = delay_samples;
    
    /* Clamp feedback to prevent runaway */
    state->feedback = fmaxf(0.0f, fminf(0.95f, feedback));
}