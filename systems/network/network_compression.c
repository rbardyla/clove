/*
 * Network Delta Compression
 * Bit-level packing and delta encoding for minimal bandwidth
 * 
 * PERFORMANCE: All compression inline, no allocations
 * CACHE: Sequential access patterns for compression
 * OPTIMIZATION: SIMD for batch operations where applicable
 */

#include "handmade_network.h"
#include <string.h>
#include <math.h>

#ifdef __AVX2__
#include <immintrin.h>
#endif

// Bit writer for packing data
typedef struct {
    uint8_t* buffer;
    uint32_t capacity;
    uint32_t bit_position;
} bit_writer_t;

// Bit reader for unpacking
typedef struct {
    const uint8_t* buffer;
    uint32_t size;
    uint32_t bit_position;
} bit_reader_t;

// Initialize bit writer
static void bit_writer_init(bit_writer_t* writer, uint8_t* buffer, uint32_t capacity) {
    writer->buffer = buffer;
    writer->capacity = capacity;
    writer->bit_position = 0;
    memset(buffer, 0, capacity);
}

// Write bits to stream
// PERFORMANCE: Branchless bit manipulation
static void bit_writer_write(bit_writer_t* writer, uint32_t value, uint32_t bits) {
    if (bits == 0 || writer->bit_position + bits > writer->capacity * 8) {
        return;
    }
    
    uint32_t byte_pos = writer->bit_position >> 3;
    uint32_t bit_offset = writer->bit_position & 7;
    
    // OPTIMIZATION: Handle aligned byte writes
    if (bit_offset == 0 && bits >= 8) {
        // Skip 32-bit optimization for now to avoid shift issues
        /*while (bits >= 32 && byte_pos + 4 <= writer->capacity) {
            *(uint32_t*)(writer->buffer + byte_pos) = value;
            value >>= 32;
            bits -= 32;
            byte_pos += 4;
            writer->bit_position += 32;
        }*/
        
        while (bits >= 8 && byte_pos < writer->capacity) {
            writer->buffer[byte_pos++] = (uint8_t)value;
            value >>= 8;
            bits -= 8;
            writer->bit_position += 8;
        }
    }
    
    // Write remaining bits
    while (bits > 0 && byte_pos < writer->capacity) {
        uint32_t bits_to_write = 8 - bit_offset;
        if (bits_to_write > bits) bits_to_write = bits;
        
        uint32_t mask = (1 << bits_to_write) - 1;
        writer->buffer[byte_pos] &= ~(mask << bit_offset);
        writer->buffer[byte_pos] |= ((value & mask) << bit_offset);
        
        value >>= bits_to_write;
        bits -= bits_to_write;
        writer->bit_position += bits_to_write;
        
        bit_offset = 0;
        byte_pos++;
    }
}

// Initialize bit reader
static void bit_reader_init(bit_reader_t* reader, const uint8_t* buffer, uint32_t size) {
    reader->buffer = buffer;
    reader->size = size;
    reader->bit_position = 0;
}

// Read bits from stream
static uint32_t bit_reader_read(bit_reader_t* reader, uint32_t bits) {
    if (bits == 0 || bits > 32 || reader->bit_position + bits > reader->size * 8) {
        return 0;
    }
    
    uint32_t result = 0;
    uint32_t byte_pos = reader->bit_position >> 3;
    uint32_t bit_offset = reader->bit_position & 7;
    uint32_t bits_read = 0;
    
    // OPTIMIZATION: Handle aligned reads
    if (bit_offset == 0 && bits >= 8) {
        if (bits >= 32 && byte_pos + 4 <= reader->size) {
            result = *(uint32_t*)(reader->buffer + byte_pos);
            reader->bit_position += 32;
            return result;
        }
        
        while (bits >= 8 && byte_pos < reader->size) {
            result |= ((uint32_t)reader->buffer[byte_pos++] << bits_read);
            bits_read += 8;
            bits -= 8;
            reader->bit_position += 8;
        }
    }
    
    // Read remaining bits
    while (bits > 0 && byte_pos < reader->size) {
        uint32_t bits_to_read = 8 - bit_offset;
        if (bits_to_read > bits) bits_to_read = bits;
        
        uint32_t mask = (1 << bits_to_read) - 1;
        uint32_t value = (reader->buffer[byte_pos] >> bit_offset) & mask;
        result |= (value << bits_read);
        
        bits_read += bits_to_read;
        bits -= bits_to_read;
        reader->bit_position += bits_to_read;
        
        bit_offset = 0;
        byte_pos++;
    }
    
    return result;
}

// Quantize float to fixed point
// PERFORMANCE: Avoid floating point in hot path
static uint32_t quantize_float(float value, float min, float max, uint32_t bits) {
    float normalized = (value - min) / (max - min);
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;
    
    uint32_t max_value = (1 << bits) - 1;
    return (uint32_t)(normalized * max_value + 0.5f);
}

// Dequantize fixed point to float
static float dequantize_float(uint32_t value, float min, float max, uint32_t bits) {
    uint32_t max_value = (1 << bits) - 1;
    float normalized = (float)value / max_value;
    return min + normalized * (max - min);
}

// Compress position (3 floats -> packed bits)
// OPTIMIZATION: Common case for world coordinates
uint32_t net_compress_position(float x, float y, float z, uint8_t* buffer) {
    bit_writer_t writer;
    bit_writer_init(&writer, buffer, 16);  // Max 16 bytes
    
    // Quantize to 16 bits per component (48 bits total)
    // Assuming world bounds of -1000 to 1000 units
    const float MIN_POS = -1000.0f;
    const float MAX_POS = 1000.0f;
    const uint32_t POS_BITS = 16;
    
    uint32_t qx = quantize_float(x, MIN_POS, MAX_POS, POS_BITS);
    uint32_t qy = quantize_float(y, MIN_POS, MAX_POS, POS_BITS);
    uint32_t qz = quantize_float(z, MIN_POS, MAX_POS, POS_BITS);
    
    bit_writer_write(&writer, qx, POS_BITS);
    bit_writer_write(&writer, qy, POS_BITS);
    bit_writer_write(&writer, qz, POS_BITS);
    
    return (writer.bit_position + 7) / 8;  // Return bytes used
}

// Decompress position
void net_decompress_position(const uint8_t* buffer, float* x, float* y, float* z) {
    bit_reader_t reader;
    bit_reader_init(&reader, buffer, 16);
    
    const float MIN_POS = -1000.0f;
    const float MAX_POS = 1000.0f;
    const uint32_t POS_BITS = 16;
    
    uint32_t qx = bit_reader_read(&reader, POS_BITS);
    uint32_t qy = bit_reader_read(&reader, POS_BITS);
    uint32_t qz = bit_reader_read(&reader, POS_BITS);
    
    *x = dequantize_float(qx, MIN_POS, MAX_POS, POS_BITS);
    *y = dequantize_float(qy, MIN_POS, MAX_POS, POS_BITS);
    *z = dequantize_float(qz, MIN_POS, MAX_POS, POS_BITS);
}

// Compress rotation (yaw, pitch -> 16 bits)
uint16_t net_compress_rotation(float yaw, float pitch) {
    // Normalize angles to 0-360 range
    while (yaw < 0) yaw += 360.0f;
    while (yaw >= 360.0f) yaw -= 360.0f;
    
    // Clamp pitch to -90 to 90
    if (pitch < -90.0f) pitch = -90.0f;
    if (pitch > 90.0f) pitch = 90.0f;
    
    // Pack yaw in 9 bits (0-511, ~0.7 degree precision)
    // Pack pitch in 7 bits (0-127, ~1.4 degree precision)
    uint16_t qyaw = (uint16_t)(yaw * 511.0f / 360.0f);
    uint16_t qpitch = (uint16_t)((pitch + 90.0f) * 127.0f / 180.0f);
    
    return (qyaw << 7) | qpitch;
}

// Decompress rotation
void net_decompress_rotation(uint16_t compressed, float* yaw, float* pitch) {
    uint16_t qyaw = compressed >> 7;
    uint16_t qpitch = compressed & 0x7F;
    
    *yaw = (float)qyaw * 360.0f / 511.0f;
    *pitch = (float)qpitch * 180.0f / 127.0f - 90.0f;
}

// Delta encode integer array
// PERFORMANCE: SIMD optimized for large arrays
__attribute__((unused))
static uint32_t delta_encode_integers(const int32_t* values, uint32_t count,
                                      uint8_t* output, uint32_t max_output) {
    if (count == 0) return 0;
    
    bit_writer_t writer;
    bit_writer_init(&writer, output, max_output);
    
    // Write first value uncompressed
    bit_writer_write(&writer, *(uint32_t*)&values[0], 32);
    
#ifdef __AVX2__
    // SIMD delta encoding for bulk of data
    if (count >= 8) {
        for (uint32_t i = 1; i < count - 7; i += 8) {
            __m256i curr = _mm256_loadu_si256((__m256i*)&values[i]);
            __m256i prev = _mm256_loadu_si256((__m256i*)&values[i - 1]);
            __m256i delta = _mm256_sub_epi32(curr, prev);
            
            // Store deltas
            int32_t deltas[8];
            _mm256_storeu_si256((__m256i*)deltas, delta);
            
            for (int j = 0; j < 8; j++) {
                // Zigzag encode for better compression of small negatives
                uint32_t encoded = (deltas[j] << 1) ^ (deltas[j] >> 31);
                
                // Variable length encoding
                if (encoded < 128) {
                    bit_writer_write(&writer, encoded, 8);
                } else if (encoded < 16384) {
                    bit_writer_write(&writer, 0x80 | (encoded & 0x7F), 8);
                    bit_writer_write(&writer, encoded >> 7, 8);
                } else {
                    bit_writer_write(&writer, 0xFF, 8);
                    bit_writer_write(&writer, encoded, 32);
                }
            }
        }
    }
#endif
    
    // Handle remaining values
    for (uint32_t i = 1; i < count; i++) {
        int32_t delta = values[i] - values[i - 1];
        
        // Zigzag encode
        uint32_t encoded = (delta << 1) ^ (delta >> 31);
        
        // Variable length encoding
        if (encoded < 128) {
            bit_writer_write(&writer, encoded, 8);
        } else if (encoded < 16384) {
            bit_writer_write(&writer, 0x80 | (encoded & 0x7F), 8);
            bit_writer_write(&writer, encoded >> 7, 8);
        } else {
            bit_writer_write(&writer, 0xFF, 8);
            bit_writer_write(&writer, encoded, 32);
        }
    }
    
    return (writer.bit_position + 7) / 8;
}

// Delta decode integer array
__attribute__((unused))
static uint32_t delta_decode_integers(const uint8_t* input, uint32_t input_size,
                                      int32_t* values, uint32_t max_count) {
    bit_reader_t reader;
    bit_reader_init(&reader, input, input_size);
    
    uint32_t count = 0;
    
    // Read first value
    if (count < max_count) {
        values[count++] = (int32_t)bit_reader_read(&reader, 32);
    }
    
    // Read deltas
    while (count < max_count && reader.bit_position < reader.size * 8) {
        uint8_t first_byte = bit_reader_read(&reader, 8);
        uint32_t encoded;
        
        if (first_byte < 128) {
            encoded = first_byte;
        } else if (first_byte == 0xFF) {
            encoded = bit_reader_read(&reader, 32);
        } else {
            uint8_t second_byte = bit_reader_read(&reader, 8);
            encoded = ((first_byte & 0x7F) | (second_byte << 7));
        }
        
        // Zigzag decode
        int32_t delta = (encoded >> 1) ^ (-(encoded & 1));
        values[count] = values[count - 1] + delta;
        count++;
    }
    
    return count;
}

// Run-length encode byte array
// OPTIMIZATION: Fast path for runs of zeros (common in sparse data)
static uint32_t rle_encode(const uint8_t* input, uint32_t input_size,
                          uint8_t* output, uint32_t max_output) {
    uint32_t out_pos = 0;
    uint32_t in_pos = 0;
    
    while (in_pos < input_size && out_pos + 2 < max_output) {
        uint8_t value = input[in_pos];
        uint32_t run_length = 1;
        
        // Count run length (max 255)
        while (in_pos + run_length < input_size &&
               run_length < 255 &&
               input[in_pos + run_length] == value) {
            run_length++;
        }
        
        // OPTIMIZATION: Special case for single bytes
        if (run_length == 1 && value < 128) {
            output[out_pos++] = value;
        } else {
            output[out_pos++] = 0x80 | (run_length - 1);
            output[out_pos++] = value;
        }
        
        in_pos += run_length;
    }
    
    return out_pos;
}

// Run-length decode
static uint32_t rle_decode(const uint8_t* input, uint32_t input_size,
                          uint8_t* output, uint32_t max_output) {
    uint32_t out_pos = 0;
    uint32_t in_pos = 0;
    
    while (in_pos < input_size && out_pos < max_output) {
        uint8_t control = input[in_pos++];
        
        if (control < 128) {
            // Single byte
            output[out_pos++] = control;
        } else {
            // Run of bytes
            uint32_t run_length = (control & 0x7F) + 1;
            uint8_t value = input[in_pos++];
            
            // OPTIMIZATION: Use memset for long runs
            if (run_length > 16) {
                memset(output + out_pos, value, run_length);
                out_pos += run_length;
            } else {
                for (uint32_t i = 0; i < run_length && out_pos < max_output; i++) {
                    output[out_pos++] = value;
                }
            }
        }
    }
    
    return out_pos;
}

// Huffman tree node for compression
typedef struct {
    uint32_t frequency;
    uint16_t symbol;
    uint16_t left;
    uint16_t right;
} huffman_node_t;

// Simple Huffman encoding for common patterns
// CACHE: Keep frequently used codes in L1
// Note: Using hex instead of binary literals for C99 compatibility
__attribute__((unused))
static const struct {
    uint8_t pattern[4];
    uint8_t length;
    uint8_t code;
    uint8_t code_bits;
} common_patterns[] = {
    {{0, 0, 0, 0}, 4, 0x0, 1},        // Four zeros (very common)
    {{0, 0, 0, 0}, 2, 0x2, 2},        // Two zeros  
    {{0xFF, 0xFF, 0xFF, 0xFF}, 4, 0x6, 3},  // Four ones
    {{0, 0, 0, 1}, 4, 0xE, 4},        // Three zeros, one byte
    {{1, 0, 0, 0}, 4, 0xF, 4},        // One byte, three zeros
};

// Compress game snapshot with delta encoding
uint32_t compress_snapshot(const game_snapshot_t* current,
                          const game_snapshot_t* previous,
                          uint8_t* output, uint32_t max_output) {
    bit_writer_t writer;
    bit_writer_init(&writer, output, max_output);
    
    // Write tick and timestamp
    bit_writer_write(&writer, current->tick, 32);
    bit_writer_write(&writer, current->timestamp, 64);
    bit_writer_write(&writer, current->checksum, 32);
    
    // Delta encode player positions
    for (uint32_t i = 0; i < NET_MAX_PLAYERS; i++) {
        // Check if player changed
        bool changed = false;
        if (previous) {
            float dx = current->players[i].x - previous->players[i].x;
            float dy = current->players[i].y - previous->players[i].y;
            float dz = current->players[i].z - previous->players[i].z;
            
            changed = (fabsf(dx) > 0.01f || fabsf(dy) > 0.01f || fabsf(dz) > 0.01f);
        } else {
            changed = true;  // No previous, send everything
        }
        
        bit_writer_write(&writer, changed ? 1 : 0, 1);
        
        if (changed) {
            // Send delta or absolute position
            if (previous) {
                // Delta position (smaller range, higher precision)
                float dx = current->players[i].x - previous->players[i].x;
                float dy = current->players[i].y - previous->players[i].y;
                float dz = current->players[i].z - previous->players[i].z;
                
                // Quantize deltas to 12 bits each (-10 to 10 units)
                uint32_t qdx = quantize_float(dx, -10.0f, 10.0f, 12);
                uint32_t qdy = quantize_float(dy, -10.0f, 10.0f, 12);
                uint32_t qdz = quantize_float(dz, -10.0f, 10.0f, 12);
                
                bit_writer_write(&writer, qdx, 12);
                bit_writer_write(&writer, qdy, 12);
                bit_writer_write(&writer, qdz, 12);
            } else {
                // Absolute position
                uint8_t pos_buffer[16];
                uint32_t pos_size = net_compress_position(
                    current->players[i].x,
                    current->players[i].y,
                    current->players[i].z,
                    pos_buffer
                );
                
                for (uint32_t j = 0; j < pos_size; j++) {
                    bit_writer_write(&writer, pos_buffer[j], 8);
                }
            }
            
            // Velocity (usually small, 8 bits each)
            uint32_t qvx = quantize_float(current->players[i].vx, -50.0f, 50.0f, 8);
            uint32_t qvy = quantize_float(current->players[i].vy, -50.0f, 50.0f, 8);
            uint32_t qvz = quantize_float(current->players[i].vz, -50.0f, 50.0f, 8);
            
            bit_writer_write(&writer, qvx, 8);
            bit_writer_write(&writer, qvy, 8);
            bit_writer_write(&writer, qvz, 8);
            
            // Rotation
            uint16_t rot = net_compress_rotation(
                current->players[i].yaw,
                current->players[i].pitch
            );
            bit_writer_write(&writer, rot, 16);
            
            // State and health
            bit_writer_write(&writer, current->players[i].state, 16);
            bit_writer_write(&writer, current->players[i].health, 8);
        }
    }
    
    // Compress entities with RLE
    uint8_t rle_buffer[8192];
    uint32_t rle_size = rle_encode(
        current->compressed_entities,
        current->entity_count,
        rle_buffer,
        sizeof(rle_buffer)
    );
    
    bit_writer_write(&writer, rle_size, 16);
    for (uint32_t i = 0; i < rle_size; i++) {
        bit_writer_write(&writer, rle_buffer[i], 8);
    }
    
    return (writer.bit_position + 7) / 8;
}

// Decompress game snapshot
uint32_t decompress_snapshot(const uint8_t* input, uint32_t input_size,
                            const game_snapshot_t* previous,
                            game_snapshot_t* output) {
    bit_reader_t reader;
    bit_reader_init(&reader, input, input_size);
    
    // Read header
    output->tick = bit_reader_read(&reader, 32);
    output->timestamp = bit_reader_read(&reader, 64);
    output->checksum = bit_reader_read(&reader, 32);
    
    // Copy from previous as baseline
    if (previous) {
        memcpy(output->players, previous->players, sizeof(output->players));
    }
    
    // Decode player deltas
    for (uint32_t i = 0; i < NET_MAX_PLAYERS; i++) {
        bool changed = bit_reader_read(&reader, 1);
        
        if (changed) {
            if (previous) {
                // Read delta position
                uint32_t qdx = bit_reader_read(&reader, 12);
                uint32_t qdy = bit_reader_read(&reader, 12);
                uint32_t qdz = bit_reader_read(&reader, 12);
                
                float dx = dequantize_float(qdx, -10.0f, 10.0f, 12);
                float dy = dequantize_float(qdy, -10.0f, 10.0f, 12);
                float dz = dequantize_float(qdz, -10.0f, 10.0f, 12);
                
                output->players[i].x = previous->players[i].x + dx;
                output->players[i].y = previous->players[i].y + dy;
                output->players[i].z = previous->players[i].z + dz;
            } else {
                // Read absolute position
                uint8_t pos_buffer[16];
                for (uint32_t j = 0; j < 6; j++) {  // 48 bits = 6 bytes
                    pos_buffer[j] = bit_reader_read(&reader, 8);
                }
                
                net_decompress_position(
                    pos_buffer,
                    &output->players[i].x,
                    &output->players[i].y,
                    &output->players[i].z
                );
            }
            
            // Velocity
            uint32_t qvx = bit_reader_read(&reader, 8);
            uint32_t qvy = bit_reader_read(&reader, 8);
            uint32_t qvz = bit_reader_read(&reader, 8);
            
            output->players[i].vx = dequantize_float(qvx, -50.0f, 50.0f, 8);
            output->players[i].vy = dequantize_float(qvy, -50.0f, 50.0f, 8);
            output->players[i].vz = dequantize_float(qvz, -50.0f, 50.0f, 8);
            
            // Rotation
            uint16_t rot = bit_reader_read(&reader, 16);
            net_decompress_rotation(
                rot,
                &output->players[i].yaw,
                &output->players[i].pitch
            );
            
            // State and health
            output->players[i].state = bit_reader_read(&reader, 16);
            output->players[i].health = bit_reader_read(&reader, 8);
        }
    }
    
    // Decompress entities
    uint32_t rle_size = bit_reader_read(&reader, 16);
    uint8_t rle_buffer[8192];
    
    for (uint32_t i = 0; i < rle_size; i++) {
        rle_buffer[i] = bit_reader_read(&reader, 8);
    }
    
    output->entity_count = rle_decode(
        rle_buffer,
        rle_size,
        output->compressed_entities,
        sizeof(output->compressed_entities)
    );
    
    return (reader.bit_position + 7) / 8;
}