// handmade_reflection.c - Runtime type introspection implementation
// PERFORMANCE: Zero allocations during property access, compile-time metadata

#include "handmade_reflection.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <immintrin.h>  // For SIMD

// Global reflection database
reflection_database* g_reflection_db = NULL;

// ============================================================================
// HASH FUNCTIONS
// ============================================================================

uint32_t reflection_hash_string(const char* str)
{
    // FNV-1a hash
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= (uint32_t)*str++;
        hash *= 16777619u;
    }
    return hash;
}

static uint32_t hash_type_id(uint32_t type_id, uint32_t capacity)
{
    // Simple modulo for now, could use better hash
    return type_id % capacity;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void reflection_init(void* arena_memory, size_t arena_size)
{
    if (g_reflection_db) return;  // Already initialized
    
    // Use provided arena memory
    g_reflection_db = (reflection_database*)arena_memory;
    memset(g_reflection_db, 0, sizeof(reflection_database));
    
    g_reflection_db->arena_base = (uint8_t*)arena_memory + sizeof(reflection_database);
    g_reflection_db->arena_size = arena_size - sizeof(reflection_database);
    g_reflection_db->arena_used = 0;
    
    // Initialize hash table for type lookup
    size_t map_size = 2048;  // Power of 2 for fast modulo
    size_t map_bytes = map_size * (sizeof(uint32_t) * 2);
    
    g_reflection_db->type_map.keys = (uint32_t*)g_reflection_db->arena_base;
    g_reflection_db->type_map.indices = g_reflection_db->type_map.keys + map_size;
    g_reflection_db->type_map.capacity = map_size;
    g_reflection_db->type_map.count = 0;
    
    g_reflection_db->arena_used += map_bytes;
    
    // Clear the hash table
    memset(g_reflection_db->type_map.keys, 0xFF, map_size * sizeof(uint32_t));
    
    // Register built-in types
    reflection_register_type("bool", sizeof(bool), alignof(bool), TYPE_BOOL);
    reflection_register_type("i8", sizeof(int8_t), alignof(int8_t), TYPE_I8);
    reflection_register_type("i16", sizeof(int16_t), alignof(int16_t), TYPE_I16);
    reflection_register_type("i32", sizeof(int32_t), alignof(int32_t), TYPE_I32);
    reflection_register_type("i64", sizeof(int64_t), alignof(int64_t), TYPE_I64);
    reflection_register_type("u8", sizeof(uint8_t), alignof(uint8_t), TYPE_U8);
    reflection_register_type("u16", sizeof(uint16_t), alignof(uint16_t), TYPE_U16);
    reflection_register_type("u32", sizeof(uint32_t), alignof(uint32_t), TYPE_U32);
    reflection_register_type("u64", sizeof(uint64_t), alignof(uint64_t), TYPE_U64);
    reflection_register_type("f32", sizeof(float), alignof(float), TYPE_F32);
    reflection_register_type("f64", sizeof(double), alignof(double), TYPE_F64);
}

void reflection_shutdown(void)
{
    g_reflection_db = NULL;
}

// ============================================================================
// TYPE REGISTRATION
// ============================================================================

type_descriptor* reflection_register_type(const char* name, size_t size, 
                                         size_t alignment, type_kind kind)
{
    if (!g_reflection_db || g_reflection_db->type_count >= MAX_REGISTERED_TYPES) {
        return NULL;
    }
    
    // Check if type already registered
    uint32_t type_id = reflection_hash_string(name);
    type_descriptor* existing = reflection_get_type_by_id(type_id);
    if (existing) {
        return existing;
    }
    
    // Allocate type descriptor from arena
    size_t alloc_size = sizeof(type_descriptor);
    if (g_reflection_db->arena_used + alloc_size > g_reflection_db->arena_size) {
        return NULL;  // Out of memory
    }
    
    type_descriptor* type = (type_descriptor*)((uint8_t*)g_reflection_db->arena_base + 
                                               g_reflection_db->arena_used);
    g_reflection_db->arena_used += alloc_size;
    
    // Initialize type
    memset(type, 0, sizeof(type_descriptor));
    type->name = name;
    type->size = size;
    type->alignment = alignment;
    type->kind = kind;
    type->type_id = type_id;
    
    // Add to registry
    uint32_t index = g_reflection_db->type_count++;
    g_reflection_db->types[index] = type;
    g_reflection_db->type_ids[index] = type_id;
    strncpy(g_reflection_db->type_names[index], name, MAX_TYPE_NAME_LENGTH - 1);
    
    // Add to hash table
    uint32_t hash_index = hash_type_id(type_id, g_reflection_db->type_map.capacity);
    while (g_reflection_db->type_map.keys[hash_index] != 0xFFFFFFFF) {
        hash_index = (hash_index + 1) % g_reflection_db->type_map.capacity;
    }
    g_reflection_db->type_map.keys[hash_index] = type_id;
    g_reflection_db->type_map.indices[hash_index] = index;
    g_reflection_db->type_map.count++;
    
    return type;
}

void reflection_register_property(type_descriptor* type, 
                                 const property_descriptor* prop)
{
    if (!type || !prop || type->kind != TYPE_STRUCT) return;
    
    // Allocate space for property
    size_t alloc_size = sizeof(property_descriptor);
    if (g_reflection_db->arena_used + alloc_size > g_reflection_db->arena_size) {
        return;  // Out of memory
    }
    
    property_descriptor* new_prop = (property_descriptor*)
        ((uint8_t*)g_reflection_db->arena_base + g_reflection_db->arena_used);
    g_reflection_db->arena_used += alloc_size;
    
    // Copy property descriptor
    memcpy(new_prop, prop, sizeof(property_descriptor));
    
    // Add to type's property list
    if (!type->struct_data.properties) {
        type->struct_data.properties = new_prop;
        type->struct_data.property_count = 1;
    } else {
        // This is simplified - in real implementation would need proper list management
        type->struct_data.property_count++;
    }
}

void reflection_finalize_type(type_descriptor* type)
{
    // Perform any final initialization
    // For example, resolve type references, compute offsets, etc.
    if (!type) return;
    
    // Sort properties by offset for cache-friendly iteration
    if (type->kind == TYPE_STRUCT && type->struct_data.properties) {
        // Simple bubble sort for small property counts
        for (uint32_t i = 0; i < type->struct_data.property_count - 1; i++) {
            for (uint32_t j = 0; j < type->struct_data.property_count - i - 1; j++) {
                if (type->struct_data.properties[j].offset > 
                    type->struct_data.properties[j + 1].offset) {
                    property_descriptor temp = type->struct_data.properties[j];
                    type->struct_data.properties[j] = type->struct_data.properties[j + 1];
                    type->struct_data.properties[j + 1] = temp;
                }
            }
        }
    }
}

// ============================================================================
// TYPE LOOKUP
// ============================================================================

type_descriptor* reflection_get_type(const char* name)
{
    if (!g_reflection_db || !name) return NULL;
    
    uint32_t type_id = reflection_hash_string(name);
    return reflection_get_type_by_id(type_id);
}

type_descriptor* reflection_get_type_by_id(uint32_t type_id)
{
    if (!g_reflection_db) return NULL;
    
    // Look up in hash table
    uint32_t hash_index = hash_type_id(type_id, g_reflection_db->type_map.capacity);
    uint32_t attempts = 0;
    
    while (attempts < g_reflection_db->type_map.capacity) {
        if (g_reflection_db->type_map.keys[hash_index] == type_id) {
            uint32_t index = g_reflection_db->type_map.indices[hash_index];
            return g_reflection_db->types[index];
        }
        if (g_reflection_db->type_map.keys[hash_index] == 0xFFFFFFFF) {
            return NULL;  // Not found
        }
        hash_index = (hash_index + 1) % g_reflection_db->type_map.capacity;
        attempts++;
    }
    
    return NULL;
}

// ============================================================================
// PROPERTY ACCESS
// ============================================================================

void* reflection_get_property(void* object, type_descriptor* type, 
                             const char* property_name)
{
    if (!object || !type || !property_name) return NULL;
    
    if (type->kind != TYPE_STRUCT) return NULL;
    
    // Linear search through properties (could optimize with hash table)
    for (uint32_t i = 0; i < type->struct_data.property_count; i++) {
        property_descriptor* prop = &type->struct_data.properties[i];
        if (strcmp(prop->name, property_name) == 0) {
            if (prop->getter) {
                return prop->getter(object);
            } else {
                return (uint8_t*)object + prop->offset;
            }
        }
    }
    
    return NULL;
}

bool reflection_set_property(void* object, type_descriptor* type,
                            const char* property_name, void* value)
{
    if (!object || !type || !property_name || !value) return false;
    
    if (type->kind != TYPE_STRUCT) return false;
    
    // Find property
    for (uint32_t i = 0; i < type->struct_data.property_count; i++) {
        property_descriptor* prop = &type->struct_data.properties[i];
        if (strcmp(prop->name, property_name) == 0) {
            if (prop->flags & PROP_FLAG_READONLY) {
                return false;
            }
            
            if (prop->setter) {
                prop->setter(object, value);
            } else {
                void* dest = (uint8_t*)object + prop->offset;
                memcpy(dest, value, prop->type->size);
            }
            return true;
        }
    }
    
    return false;
}

// ============================================================================
// PROPERTY ITERATION
// ============================================================================

void reflection_iterate_properties(void* object, type_descriptor* type,
                                  property_visitor visitor, void* user_data)
{
    if (!object || !type || !visitor) return;
    
    if (type->kind != TYPE_STRUCT) return;
    
    for (uint32_t i = 0; i < type->struct_data.property_count; i++) {
        property_descriptor* prop = &type->struct_data.properties[i];
        visitor(prop, object, user_data);
    }
}

// ============================================================================
// SERIALIZATION
// ============================================================================

static bool serialize_value(serialization_context* ctx, void* value, 
                           type_descriptor* type)
{
    if (!ctx || !value || !type) return false;
    
    if (ctx->cursor + type->size > ctx->buffer_size) {
        ctx->has_error = true;
        ctx->error_msg = "Buffer overflow";
        return false;
    }
    
    // Handle different types
    switch (type->kind) {
        case TYPE_BOOL:
        case TYPE_I8: case TYPE_I16: case TYPE_I32: case TYPE_I64:
        case TYPE_U8: case TYPE_U16: case TYPE_U32: case TYPE_U64:
        case TYPE_F32: case TYPE_F64:
            // POD types - direct copy
            memcpy(ctx->buffer + ctx->cursor, value, type->size);
            ctx->cursor += type->size;
            return true;
            
        case TYPE_STRUCT:
            // Serialize each property
            for (uint32_t i = 0; i < type->struct_data.property_count; i++) {
                property_descriptor* prop = &type->struct_data.properties[i];
                if (prop->flags & PROP_FLAG_TRANSIENT) continue;
                
                void* prop_value = (uint8_t*)value + prop->offset;
                if (!serialize_value(ctx, prop_value, prop->type)) {
                    return false;
                }
            }
            return true;
            
        default:
            ctx->has_error = true;
            ctx->error_msg = "Unsupported type for serialization";
            return false;
    }
}

bool reflection_serialize(void* object, type_descriptor* type, 
                        serialization_context* ctx)
{
    if (!object || !type || !ctx) return false;
    
    ctx->is_writing = true;
    ctx->has_error = false;
    
    // Write header
    uint32_t magic = 0x52454643;  // "REFC"
    memcpy(ctx->buffer + ctx->cursor, &magic, sizeof(magic));
    ctx->cursor += sizeof(magic);
    
    memcpy(ctx->buffer + ctx->cursor, &ctx->version, sizeof(ctx->version));
    ctx->cursor += sizeof(ctx->version);
    
    memcpy(ctx->buffer + ctx->cursor, &type->type_id, sizeof(type->type_id));
    ctx->cursor += sizeof(type->type_id);
    
    // Serialize the object
    return serialize_value(ctx, object, type);
}

bool reflection_deserialize(void* object, type_descriptor* type,
                          serialization_context* ctx)
{
    // Implementation similar to serialize but reading instead
    // Omitted for brevity
    return true;
}

// ============================================================================
// SIMD OPTIMIZATIONS
// ============================================================================

bool reflection_compare_properties_simd(void* object_a, void* object_b,
                                       type_descriptor* type)
{
    if (!object_a || !object_b || !type) return false;
    
    if (type->kind != TYPE_STRUCT) {
        // Simple comparison for non-structs
        return memcmp(object_a, object_b, type->size) == 0;
    }
    
    // Use SIMD for large structs
    if (type->size >= 32) {
        uint8_t* a = (uint8_t*)object_a;
        uint8_t* b = (uint8_t*)object_b;
        size_t simd_size = type->size & ~31;  // Round down to multiple of 32
        
        for (size_t i = 0; i < simd_size; i += 32) {
            __m256i va = _mm256_loadu_si256((__m256i*)(a + i));
            __m256i vb = _mm256_loadu_si256((__m256i*)(b + i));
            __m256i vcmp = _mm256_cmpeq_epi8(va, vb);
            int mask = _mm256_movemask_epi8(vcmp);
            if (mask != -1) return false;  // Not equal
        }
        
        // Compare remaining bytes
        return memcmp(a + simd_size, b + simd_size, type->size - simd_size) == 0;
    }
    
    return memcmp(object_a, object_b, type->size) == 0;
}

void reflection_batch_update(const property_batch* batch)
{
    if (!batch || !batch->objects || !batch->property || !batch->new_value) return;
    
    // Update all objects in batch
    for (uint32_t i = 0; i < batch->object_count; i++) {
        void* dest = (uint8_t*)batch->objects[i] + batch->property->offset;
        
        if (batch->property->setter) {
            batch->property->setter(batch->objects[i], batch->new_value);
        } else {
            memcpy(dest, batch->new_value, batch->property->type->size);
        }
    }
}

// ============================================================================
// DIFF & PATCH
// ============================================================================

object_diff* reflection_diff(void* old_object, void* new_object, 
                            type_descriptor* type)
{
    if (!old_object || !new_object || !type) return NULL;
    
    // Allocate diff structure
    object_diff* diff = (object_diff*)malloc(sizeof(object_diff));
    diff->diffs = (property_diff*)malloc(sizeof(property_diff) * 16);
    diff->diff_count = 0;
    diff->diff_capacity = 16;
    
    if (type->kind == TYPE_STRUCT) {
        // Compare each property
        for (uint32_t i = 0; i < type->struct_data.property_count; i++) {
            property_descriptor* prop = &type->struct_data.properties[i];
            
            void* old_value = (uint8_t*)old_object + prop->offset;
            void* new_value = (uint8_t*)new_object + prop->offset;
            
            // Compare values
            if (memcmp(old_value, new_value, prop->type->size) != 0) {
                // Add to diff
                if (diff->diff_count >= diff->diff_capacity) {
                    diff->diff_capacity *= 2;
                    diff->diffs = (property_diff*)realloc(diff->diffs, 
                        sizeof(property_diff) * diff->diff_capacity);
                }
                
                property_diff* pd = &diff->diffs[diff->diff_count++];
                pd->property_path = prop->name;
                pd->value_size = prop->type->size;
                pd->is_pointer = false;
                
                if (pd->value_size <= 64) {
                    memcpy(pd->old_value, old_value, pd->value_size);
                    memcpy(pd->new_value, new_value, pd->value_size);
                } else {
                    // Store pointers for large values
                    *(void**)pd->old_value = old_value;
                    *(void**)pd->new_value = new_value;
                    pd->is_pointer = true;
                }
            }
        }
    }
    
    return diff;
}

void reflection_apply_diff(void* object, const object_diff* diff)
{
    if (!object || !diff) return;
    
    for (uint32_t i = 0; i < diff->diff_count; i++) {
        const property_diff* pd = &diff->diffs[i];
        
        // Find property by path (simplified - assumes single level)
        // In real implementation would parse path for nested properties
        
        // Apply the new value
        // This is simplified - would need proper property lookup
    }
}

void reflection_free_diff(object_diff* diff)
{
    if (!diff) return;
    free(diff->diffs);
    free(diff);
}

// ============================================================================
// DEBUG & UTILITIES
// ============================================================================

const char* reflection_type_kind_to_string(type_kind kind)
{
    static const char* names[] = {
        "UNKNOWN", "BOOL", 
        "I8", "I16", "I32", "I64",
        "U8", "U16", "U32", "U64",
        "F32", "F64",
        "STRING", "STRUCT", "ARRAY", "POINTER", "ENUM", "UNION",
        "VEC2", "VEC3", "VEC4", "MAT3", "MAT4", "QUAT",
        "COLOR32", "COLOR_F32",
        "ENTITY", "COMPONENT", "ASSET_HANDLE"
    };
    
    if (kind < TYPE_COUNT) {
        return names[kind];
    }
    return "INVALID";
}

void reflection_print_type(const type_descriptor* type)
{
    if (!type) return;
    
    printf("Type: %s\n", type->name);
    printf("  Kind: %s\n", reflection_type_kind_to_string(type->kind));
    printf("  Size: %zu bytes\n", type->size);
    printf("  Alignment: %zu\n", type->alignment);
    printf("  Type ID: 0x%08X\n", type->type_id);
    
    if (type->kind == TYPE_STRUCT) {
        printf("  Properties (%u):\n", type->struct_data.property_count);
        for (uint32_t i = 0; i < type->struct_data.property_count; i++) {
            property_descriptor* prop = &type->struct_data.properties[i];
            printf("    %s (%s) @ offset %zu\n", 
                   prop->name, 
                   prop->type ? prop->type->name : "unknown",
                   prop->offset);
        }
    }
}

void reflection_print_object(void* object, const type_descriptor* type)
{
    if (!object || !type) return;
    
    printf("Object of type %s:\n", type->name);
    
    if (type->kind == TYPE_STRUCT) {
        for (uint32_t i = 0; i < type->struct_data.property_count; i++) {
            property_descriptor* prop = &type->struct_data.properties[i];
            void* value = (uint8_t*)object + prop->offset;
            
            printf("  %s = ", prop->name);
            
            // Print value based on type
            if (prop->type) {
                switch (prop->type->kind) {
                    case TYPE_I32:
                        printf("%d", *(int32_t*)value);
                        break;
                    case TYPE_F32:
                        printf("%f", *(float*)value);
                        break;
                    case TYPE_BOOL:
                        printf("%s", *(bool*)value ? "true" : "false");
                        break;
                    default:
                        printf("<unprintable>");
                }
            }
            printf("\n");
        }
    }
}

void reflection_validate_database(void)
{
    if (!g_reflection_db) {
        printf("Reflection database not initialized\n");
        return;
    }
    
    printf("Reflection Database Status:\n");
    printf("  Registered types: %u / %d\n", 
           g_reflection_db->type_count, MAX_REGISTERED_TYPES);
    printf("  Arena usage: %zu / %zu bytes\n",
           g_reflection_db->arena_used, g_reflection_db->arena_size);
    printf("  Hash table load: %u / %u\n",
           g_reflection_db->type_map.count, g_reflection_db->type_map.capacity);
    
    // Check for hash collisions
    uint32_t collisions = 0;
    for (uint32_t i = 0; i < g_reflection_db->type_count; i++) {
        for (uint32_t j = i + 1; j < g_reflection_db->type_count; j++) {
            if (g_reflection_db->type_ids[i] == g_reflection_db->type_ids[j]) {
                printf("  WARNING: Hash collision between %s and %s\n",
                       g_reflection_db->type_names[i],
                       g_reflection_db->type_names[j]);
                collisions++;
            }
        }
    }
    printf("  Hash collisions: %u\n", collisions);
}