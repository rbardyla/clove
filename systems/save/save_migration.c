/*
    Handmade Save System - Version Migration
    
    Handles save file compatibility across versions
    Supports adding/removing fields and type changes
*/

#include "handmade_save.h"
#include "save_stubs.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

// Version history tracking
typedef struct version_change {
    u32 version;
    char description[256];
    char added_fields[10][64];
    char removed_fields[10][64];
    u32 added_count;
    u32 removed_count;
} version_change;

// Migration context for complex transformations
typedef struct migration_context {
    save_buffer *old_buffer;
    save_buffer *new_buffer;
    u32 old_version;
    u32 new_version;
    
    // Field mapping
    struct {
        char old_name[64];
        char new_name[64];
        u32 old_type;
        u32 new_type;
    } field_mappings[64];
    u32 mapping_count;
    
    // Default values for new fields
    struct {
        char field_name[64];
        union {
            u64 integer;
            f64 floating;
            char string[256];
        } default_value;
        u32 type;
    } defaults[32];
    u32 default_count;
} migration_context;

// Version 0 to 1 migration example
internal b32 migrate_v0_to_v1(save_buffer *old_data, save_buffer *new_data, 
                              u32 old_version, u32 new_version) {
    // PERFORMANCE: Stream processing, minimal memory overhead
    migration_context ctx = {0};
    ctx.old_buffer = old_data;
    ctx.new_buffer = new_data;
    ctx.old_version = old_version;
    ctx.new_version = new_version;
    
    // Example: Version 1 added player stamina system
    // Need to add default stamina values
    
    // Copy header (already validated)
    save_header header;
    save_read_bytes(old_data, &header, sizeof(header));
    header.version = new_version;
    save_write_bytes(new_data, &header, sizeof(header));
    
    // Copy metadata (unchanged)
    save_metadata metadata;
    save_read_bytes(old_data, &metadata, sizeof(metadata));
    save_write_bytes(new_data, &metadata, sizeof(metadata));
    
    // Process chunks
    save_chunk_header chunk_header;
    
    while (old_data->read_offset < old_data->size) {
        // Read chunk header
        u32 chunk_start = old_data->read_offset;
        save_read_bytes(old_data, &chunk_header, sizeof(chunk_header));
        
        if (chunk_header.type == SAVE_CHUNK_END) {
            // Write end marker
            save_write_bytes(new_data, &chunk_header, sizeof(chunk_header));
            break;
        }
        
        // Allocate temporary buffer for chunk data
        u8 chunk_data[SAVE_CHUNK_SIZE];
        
        // Decompress if needed
        if (chunk_header.compressed_size != chunk_header.uncompressed_size) {
            u8 compressed[SAVE_CHUNK_SIZE];
            save_read_bytes(old_data, compressed, chunk_header.compressed_size);
            
            u32 decompressed_size = save_decompress_lz4(compressed, 
                                                        chunk_header.compressed_size,
                                                        chunk_data, SAVE_CHUNK_SIZE);
            
            if (decompressed_size != chunk_header.uncompressed_size) {
                return 0; // Decompression failed
            }
        } else {
            save_read_bytes(old_data, chunk_data, chunk_header.uncompressed_size);
        }
        
        // Process chunk based on type
        save_buffer chunk_buffer = {
            .data = chunk_data,
            .size = chunk_header.uncompressed_size,
            .capacity = SAVE_CHUNK_SIZE,
            .read_offset = 0
        };
        
        // Output buffer for modified chunk
        u8 new_chunk_data[SAVE_CHUNK_SIZE];
        save_buffer new_chunk_buffer = {
            .data = new_chunk_data,
            .size = 0,
            .capacity = SAVE_CHUNK_SIZE,
            .read_offset = 0
        };
        
        switch (chunk_header.type) {
            case SAVE_CHUNK_PLAYER: {
                // Migrate player data - add stamina fields
                
                // Read old player data
                char name[64];
                save_read_string(&chunk_buffer, name, 64);
                u32 level = save_read_u32(&chunk_buffer);
                u32 experience = save_read_u32(&chunk_buffer);
                u32 health = save_read_u32(&chunk_buffer);
                u32 max_health = save_read_u32(&chunk_buffer);
                u32 mana = save_read_u32(&chunk_buffer);
                u32 max_mana = save_read_u32(&chunk_buffer);
                
                f32 position[3];
                position[0] = save_read_f32(&chunk_buffer);
                position[1] = save_read_f32(&chunk_buffer);
                position[2] = save_read_f32(&chunk_buffer);
                f32 rotation[2];
                rotation[0] = save_read_f32(&chunk_buffer);
                rotation[1] = save_read_f32(&chunk_buffer);
                
                // Write new player data with added fields
                save_write_string(&new_chunk_buffer, name);
                save_write_u32(&new_chunk_buffer, level);
                save_write_u32(&new_chunk_buffer, experience);
                save_write_u32(&new_chunk_buffer, health);
                save_write_u32(&new_chunk_buffer, max_health);
                save_write_u32(&new_chunk_buffer, mana);
                save_write_u32(&new_chunk_buffer, max_mana);
                
                // NEW IN V1: Stamina system
                u32 stamina = 100;  // Default value
                u32 max_stamina = 100;
                save_write_u32(&new_chunk_buffer, stamina);
                save_write_u32(&new_chunk_buffer, max_stamina);
                
                save_write_f32(&new_chunk_buffer, position[0]);
                save_write_f32(&new_chunk_buffer, position[1]);
                save_write_f32(&new_chunk_buffer, position[2]);
                save_write_f32(&new_chunk_buffer, rotation[0]);
                save_write_f32(&new_chunk_buffer, rotation[1]);
                
                // Copy remaining data unchanged
                u32 remaining = chunk_header.uncompressed_size - chunk_buffer.read_offset;
                save_write_bytes(&new_chunk_buffer, 
                               chunk_buffer.data + chunk_buffer.read_offset,
                               remaining);
                
                break;
            }
            
            case SAVE_CHUNK_WORLD: {
                // Example: Entity type enum changed
                u32 entity_count = save_read_u32(&chunk_buffer);
                save_write_u32(&new_chunk_buffer, entity_count);
                
                for (u32 i = 0; i < entity_count; i++) {
                    // Read old entity data
                    u32 id = save_read_u32(&chunk_buffer);
                    u32 old_type = save_read_u32(&chunk_buffer);
                    
                    // Map old type to new type
                    u32 new_type = old_type;
                    if (old_type == 5) {  // Old "ENEMY" type
                        new_type = 10;    // Split into "HOSTILE_NPC"
                    }
                    
                    save_write_u32(&new_chunk_buffer, id);
                    save_write_u32(&new_chunk_buffer, new_type);
                    
                    // Copy rest of entity data
                    u8 entity_data[512];
                    u32 entity_size = sizeof(f32) * 10 + sizeof(u32) * 2 + 64; // Simplified
                    save_read_bytes(&chunk_buffer, entity_data, entity_size);
                    save_write_bytes(&new_chunk_buffer, entity_data, entity_size);
                }
                
                break;
            }
            
            default:
                // Unknown or unchanged chunk - copy as-is
                save_write_bytes(&new_chunk_buffer, chunk_data, chunk_header.uncompressed_size);
                break;
        }
        
        // Write migrated chunk
        write_chunk(new_data, chunk_header.type, 
                   new_chunk_buffer.data, new_chunk_buffer.size, 
                   chunk_header.compressed_size != chunk_header.uncompressed_size);
    }
    
    return 1;
}

// Version 1 to 2 migration example
internal b32 migrate_v1_to_v2(save_buffer *old_data, save_buffer *new_data,
                              u32 old_version, u32 new_version) {
    // Example: Version 2 reorganized NPC brain structure
    
    // Copy header with new version
    save_header header;
    save_read_bytes(old_data, &header, sizeof(header));
    header.version = new_version;
    save_write_bytes(new_data, &header, sizeof(header));
    
    // Copy metadata
    save_metadata metadata;
    save_read_bytes(old_data, &metadata, sizeof(metadata));
    save_write_bytes(new_data, &metadata, sizeof(metadata));
    
    // Process chunks
    while (old_data->read_offset < old_data->size) {
        save_chunk_header chunk_header;
        u32 chunk_start = old_data->read_offset;
        save_read_bytes(old_data, &chunk_header, sizeof(chunk_header));
        
        if (chunk_header.type == SAVE_CHUNK_END) {
            save_write_bytes(new_data, &chunk_header, sizeof(chunk_header));
            break;
        }
        
        // Read chunk data
        u8 chunk_data[SAVE_CHUNK_SIZE];
        
        if (chunk_header.compressed_size != chunk_header.uncompressed_size) {
            u8 compressed[SAVE_CHUNK_SIZE];
            save_read_bytes(old_data, compressed, chunk_header.compressed_size);
            save_decompress_lz4(compressed, chunk_header.compressed_size,
                               chunk_data, SAVE_CHUNK_SIZE);
        } else {
            save_read_bytes(old_data, chunk_data, chunk_header.uncompressed_size);
        }
        
        if (chunk_header.type == SAVE_CHUNK_NPCS) {
            // Migrate NPC brain structure
            save_buffer old_chunk = {
                .data = chunk_data,
                .size = chunk_header.uncompressed_size,
                .read_offset = 0
            };
            
            u8 new_chunk_data[SAVE_CHUNK_SIZE];
            save_buffer new_chunk = {
                .data = new_chunk_data,
                .size = 0,
                .capacity = SAVE_CHUNK_SIZE
            };
            
            u32 npc_count = save_read_u32(&old_chunk);
            save_write_u32(&new_chunk, npc_count);
            
            for (u32 i = 0; i < npc_count; i++) {
                u32 entity_id = save_read_u32(&old_chunk);
                save_write_u32(&new_chunk, entity_id);
                
                // Old format: separate LSTM and memory
                u32 lstm_size = save_read_u32(&old_chunk);
                u32 memory_size = save_read_u32(&old_chunk);
                
                // New format: unified neural architecture
                u32 unified_size = lstm_size + memory_size;
                save_write_u32(&new_chunk, unified_size);
                
                // Copy and reorganize weights
                f32 weights[4096];
                save_read_bytes(&old_chunk, weights, lstm_size * sizeof(f32));
                
                // Apply weight transformation for new architecture
                for (u32 j = 0; j < lstm_size; j++) {
                    weights[j] *= 0.8f; // Scale factor for new activation
                }
                
                save_write_bytes(&new_chunk, weights, unified_size * sizeof(f32));
                
                // Copy rest of NPC data
                u32 remaining = 1024; // Simplified - would calculate actual size
                u8 rest[1024];
                save_read_bytes(&old_chunk, rest, remaining);
                save_write_bytes(&new_chunk, rest, remaining);
            }
            
            write_chunk(new_data, SAVE_CHUNK_NPCS, 
                       new_chunk_data, new_chunk.size, 1);
        } else {
            // Copy chunk unchanged
            write_chunk(new_data, chunk_header.type,
                       chunk_data, chunk_header.uncompressed_size,
                       chunk_header.compressed_size != chunk_header.uncompressed_size);
        }
    }
    
    return 1;
}

// Generic field addition helper
internal void add_default_field(save_buffer *buffer, char *field_name, 
                               void *default_value, u32 type) {
    switch (type) {
        case 0: // u32
            save_write_u32(buffer, *(u32*)default_value);
            break;
        case 1: // f32
            save_write_f32(buffer, *(f32*)default_value);
            break;
        case 2: // string
            save_write_string(buffer, (char*)default_value);
            break;
        case 3: // u64
            save_write_u64(buffer, *(u64*)default_value);
            break;
        case 4: // f64
            save_write_f64(buffer, *(f64*)default_value);
            break;
    }
}

// Generic field removal helper
internal b32 skip_field(save_buffer *buffer, u32 type) {
    switch (type) {
        case 0: // u32
            buffer->read_offset += 4;
            break;
        case 1: // f32
            buffer->read_offset += 4;
            break;
        case 2: { // string
            u16 len = save_read_u16(buffer);
            buffer->read_offset += len;
            break;
        }
        case 3: // u64
            buffer->read_offset += 8;
            break;
        case 4: // f64
            buffer->read_offset += 8;
            break;
        default:
            return 0;
    }
    return 1;
}

// Automatic migration builder
internal b32 auto_migrate_chunk(migration_context *ctx, save_chunk_type chunk_type,
                               u8 *old_data, u32 old_size,
                               u8 *new_data, u32 *new_size) {
    // PERFORMANCE: Generic migration with field mapping
    save_buffer old_buffer = {
        .data = old_data,
        .size = old_size,
        .capacity = old_size,
        .read_offset = 0
    };
    
    save_buffer new_buffer = {
        .data = new_data,
        .size = 0,
        .capacity = SAVE_CHUNK_SIZE,
        .read_offset = 0
    };
    
    // Apply field mappings based on chunk type
    switch (chunk_type) {
        case SAVE_CHUNK_PLAYER:
            // Apply player-specific migrations
            for (u32 i = 0; i < ctx->mapping_count; i++) {
                if (strstr(ctx->field_mappings[i].old_name, "player_")) {
                    // Handle field mapping
                }
            }
            break;
            
        case SAVE_CHUNK_WORLD:
            // Apply world-specific migrations
            break;
            
        default:
            // Copy unchanged
            memcpy(new_data, old_data, old_size);
            *new_size = old_size;
            return 1;
    }
    
    *new_size = new_buffer.size;
    return 1;
}

// Migration validation
internal b32 validate_migration(save_buffer *migrated_data, u32 target_version) {
    // Verify migrated data structure
    save_header header;
    u32 saved_offset = migrated_data->read_offset;
    migrated_data->read_offset = 0;
    
    save_read_bytes(migrated_data, &header, sizeof(header));
    
    if (header.magic != SAVE_MAGIC_NUMBER) {
        printf("Migration validation failed: Invalid magic number\n");
        migrated_data->read_offset = saved_offset;
        return 0;
    }
    
    if (header.version != target_version) {
        printf("Migration validation failed: Version mismatch (expected %u, got %u)\n",
               target_version, header.version);
        migrated_data->read_offset = saved_offset;
        return 0;
    }
    
    // Verify chunk structure
    save_metadata metadata;
    save_read_bytes(migrated_data, &metadata, sizeof(metadata));
    
    u32 chunk_count = 0;
    while (migrated_data->read_offset < migrated_data->size) {
        save_chunk_header chunk;
        save_read_bytes(migrated_data, &chunk, sizeof(chunk));
        
        if (chunk.type == SAVE_CHUNK_END) {
            break;
        }
        
        if (chunk.type > SAVE_CHUNK_INVENTORY) {
            printf("Migration validation failed: Invalid chunk type %u\n", chunk.type);
            migrated_data->read_offset = saved_offset;
            return 0;
        }
        
        // Skip chunk data
        migrated_data->read_offset += chunk.compressed_size;
        chunk_count++;
    }
    
    printf("Migration validation passed: %u chunks migrated to version %u\n",
           chunk_count, target_version);
    
    migrated_data->read_offset = saved_offset;
    return 1;
}

// Register all migrations
void save_register_all_migrations(save_system *system) {
    // Register migration functions for each version transition
    save_register_migration(system, 0, migrate_v0_to_v1);
    save_register_migration(system, 1, migrate_v1_to_v2);
    
    // Future migrations would be added here
    // save_register_migration(system, 2, migrate_v2_to_v3);
}

// Migration debugging
// save_dump_migration_info moved to platform stub

// Test migration with sample data
