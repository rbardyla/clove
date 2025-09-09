/*
    Handmade Save System - Core Serialization
    
    Binary serialization with zero allocations
    All operations work on fixed memory buffers
*/

#include "handmade_save.h"
#include "save_stubs.h"
#include <string.h>
#include <stdio.h>

// PERFORMANCE: CRC32 lookup table for fast checksums
// Generated at compile time, stored in .rodata
global_variable u32 crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

// Initialize save system with fixed memory
save_system *save_system_init(void *memory, u32 memory_size) {
    // PERFORMANCE: All memory pre-allocated, zero runtime allocations
    save_system *system = (save_system *)memory;
    memset(system, 0, sizeof(save_system));
    
    // Partition memory for different uses
    u8 *mem = (u8 *)memory + sizeof(save_system);
    u32 remaining = memory_size - sizeof(save_system);
    
    // Main save buffer (2MB)
    system->save_memory_size = Megabytes(2);
    system->save_memory = mem;
    mem += system->save_memory_size;
    remaining -= system->save_memory_size;
    
    // Compression workspace (2MB)
    system->compress_memory_size = Megabytes(2);
    system->compress_memory = mem;
    
    // Initialize buffers
    system->write_buffer.data = system->save_memory;
    system->write_buffer.capacity = system->save_memory_size / 2;
    
    system->read_buffer.data = system->save_memory + system->write_buffer.capacity;
    system->read_buffer.capacity = system->save_memory_size / 2;
    
    system->compress_buffer.data = system->compress_memory;
    system->compress_buffer.capacity = system->compress_memory_size;
    
    // Default autosave settings
    system->autosave_interval = 300.0f; // 5 minutes
    system->autosave_enabled = 1;
    
    // Enumerate existing save slots
    save_enumerate_slots(system);
    
    return system;
}

void save_system_shutdown(save_system *system) {
    // Nothing to free - all memory externally managed
    system->is_saving = 0;
    system->is_loading = 0;
}

// CRC32 checksum calculation
u32 save_crc32(u8 *data, u32 size) {
    // PERFORMANCE: Table-based CRC32, processes 1 byte per iteration
    // CACHE: Sequential read pattern, prefetch friendly
    u32 crc = 0xFFFFFFFF;
    
    for (u32 i = 0; i < size; i++) {
        u8 byte = data[i];
        u32 lookup_index = (crc ^ byte) & 0xFF;
        crc = (crc >> 8) ^ crc32_table[lookup_index];
    }
    
    return crc ^ 0xFFFFFFFF;
}

// Buffer management functions
void save_buffer_reset(save_buffer *buffer) {
    buffer->size = 0;
    buffer->read_offset = 0;
    buffer->bytes_written = 0;
    buffer->bytes_read = 0;
}

internal b32 save_buffer_ensure_space(save_buffer *buffer, u32 required) {
    return (buffer->size + required) <= buffer->capacity;
}

// Write primitives - little endian, aligned writes
void save_write_u8(save_buffer *buffer, u8 value) {
    if (!save_buffer_ensure_space(buffer, 1)) return;
    buffer->data[buffer->size++] = value;
    buffer->bytes_written++;
}

void save_write_u16(save_buffer *buffer, u16 value) {
    if (!save_buffer_ensure_space(buffer, 2)) return;
    // PERFORMANCE: Direct memory write, little-endian
    *(u16 *)(buffer->data + buffer->size) = value;
    buffer->size += 2;
    buffer->bytes_written += 2;
}

void save_write_u32(save_buffer *buffer, u32 value) {
    if (!save_buffer_ensure_space(buffer, 4)) return;
    // PERFORMANCE: Aligned 4-byte write
    *(u32 *)(buffer->data + buffer->size) = value;
    buffer->size += 4;
    buffer->bytes_written += 4;
}

void save_write_u64(save_buffer *buffer, u64 value) {
    if (!save_buffer_ensure_space(buffer, 8)) return;
    // PERFORMANCE: Aligned 8-byte write
    *(u64 *)(buffer->data + buffer->size) = value;
    buffer->size += 8;
    buffer->bytes_written += 8;
}

void save_write_f32(save_buffer *buffer, f32 value) {
    if (!save_buffer_ensure_space(buffer, 4)) return;
    *(f32 *)(buffer->data + buffer->size) = value;
    buffer->size += 4;
    buffer->bytes_written += 4;
}

void save_write_f64(save_buffer *buffer, f64 value) {
    if (!save_buffer_ensure_space(buffer, 8)) return;
    *(f64 *)(buffer->data + buffer->size) = value;
    buffer->size += 8;
    buffer->bytes_written += 8;
}

void save_write_bytes(save_buffer *buffer, void *data, u32 size) {
    if (!save_buffer_ensure_space(buffer, size)) return;
    // PERFORMANCE: memcpy for bulk data, compiler will optimize
    memcpy(buffer->data + buffer->size, data, size);
    buffer->size += size;
    buffer->bytes_written += size;
}

void save_write_string(save_buffer *buffer, char *str) {
    u32 len = 0;
    if (str) {
        while (str[len] && len < 65535) len++;
    }
    save_write_u16(buffer, (u16)len);
    if (len > 0) {
        save_write_bytes(buffer, str, len);
    }
}

// Read primitives - bounds checked
u8 save_read_u8(save_buffer *buffer) {
    if (buffer->read_offset + 1 > buffer->size) return 0;
    u8 value = buffer->data[buffer->read_offset++];
    buffer->bytes_read++;
    return value;
}

u16 save_read_u16(save_buffer *buffer) {
    if (buffer->read_offset + 2 > buffer->size) return 0;
    u16 value = *(u16 *)(buffer->data + buffer->read_offset);
    buffer->read_offset += 2;
    buffer->bytes_read += 2;
    return value;
}

u32 save_read_u32(save_buffer *buffer) {
    if (buffer->read_offset + 4 > buffer->size) return 0;
    u32 value = *(u32 *)(buffer->data + buffer->read_offset);
    buffer->read_offset += 4;
    buffer->bytes_read += 4;
    return value;
}

u64 save_read_u64(save_buffer *buffer) {
    if (buffer->read_offset + 8 > buffer->size) return 0;
    u64 value = *(u64 *)(buffer->data + buffer->read_offset);
    buffer->read_offset += 8;
    buffer->bytes_read += 8;
    return value;
}

f32 save_read_f32(save_buffer *buffer) {
    if (buffer->read_offset + 4 > buffer->size) return 0.0f;
    f32 value = *(f32 *)(buffer->data + buffer->read_offset);
    buffer->read_offset += 4;
    buffer->bytes_read += 4;
    return value;
}

f64 save_read_f64(save_buffer *buffer) {
    if (buffer->read_offset + 8 > buffer->size) return 0.0;
    f64 value = *(f64 *)(buffer->data + buffer->read_offset);
    buffer->read_offset += 8;
    buffer->bytes_read += 8;
    return value;
}

void save_read_bytes(save_buffer *buffer, void *data, u32 size) {
    if (buffer->read_offset + size > buffer->size) return;
    memcpy(data, buffer->data + buffer->read_offset, size);
    buffer->read_offset += size;
    buffer->bytes_read += size;
}

void save_read_string(save_buffer *buffer, char *str, u32 max_size) {
    u16 len = save_read_u16(buffer);
    if (len >= max_size) len = max_size - 1;
    if (len > 0) {
        save_read_bytes(buffer, str, len);
    }
    str[len] = '\0';
}

// Slot management
void save_enumerate_slots(save_system *system) {
    // PERFORMANCE: Cache slot information to avoid file I/O
    for (i32 i = 0; i < SAVE_MAX_SLOTS; i++) {
        save_slot_info *slot = &system->slots[i];
        
        // Build filename
        if (i == SAVE_QUICKSAVE_SLOT) {
            strcpy(slot->filename, "quicksave.hms");
        } else if (i == SAVE_AUTOSAVE_SLOT) {
            strcpy(slot->filename, "autosave.hms");
        } else {
            // Manual save slots
            char name[32];
            sprintf(name, "save%02d.hms", i);
            strcpy(slot->filename, name);
        }
        
        // Check if file exists
        slot->exists = platform_save_file_exists(slot->filename);
        
        if (slot->exists) {
            // Read header and metadata
            u32 header_size = sizeof(save_header) + sizeof(save_metadata);
            u8 header_data[sizeof(save_header) + sizeof(save_metadata)];
            u32 actual_size;
            
            if (platform_save_read_file(slot->filename, header_data, header_size, &actual_size)) {
                memcpy(&slot->header, header_data, sizeof(save_header));
                memcpy(&slot->metadata, header_data + sizeof(save_header), sizeof(save_metadata));
                slot->last_modified = platform_save_get_file_time(slot->filename);
            }
        }
    }
}

save_slot_info *save_get_slot_info(save_system *system, i32 slot) {
    if (slot < 0 || slot >= SAVE_MAX_SLOTS) return 0;
    return &system->slots[slot];
}

b32 save_delete_slot(save_system *system, i32 slot) {
    if (slot < 0 || slot >= SAVE_MAX_SLOTS) return 0;
    
    save_slot_info *info = &system->slots[slot];
    if (!info->exists) return 1;
    
    b32 result = platform_save_delete_file(info->filename);
    if (result) {
        info->exists = 0;
        memset(&info->header, 0, sizeof(save_header));
        memset(&info->metadata, 0, sizeof(save_metadata));
    }
    
    return result;
}

b32 save_copy_slot(save_system *system, i32 src_slot, i32 dst_slot) {
    if (src_slot < 0 || src_slot >= SAVE_MAX_SLOTS) return 0;
    if (dst_slot < 0 || dst_slot >= SAVE_MAX_SLOTS) return 0;
    if (src_slot == dst_slot) return 0;
    
    save_slot_info *src = &system->slots[src_slot];
    save_slot_info *dst = &system->slots[dst_slot];
    
    if (!src->exists) return 0;
    
    // Read source file
    u32 actual_size;
    b32 result = platform_save_read_file(src->filename, 
                                         system->read_buffer.data, 
                                         system->read_buffer.capacity, 
                                         &actual_size);
    if (!result) return 0;
    
    // Write to destination
    result = platform_save_write_file(dst->filename, 
                                      system->read_buffer.data, 
                                      actual_size);
    
    if (result) {
        // Update destination slot info
        dst->exists = 1;
        dst->header = src->header;
        dst->metadata = src->metadata;
        dst->file_size = actual_size;
        dst->last_modified = platform_save_get_file_time(dst->filename);
    }
    
    return result;
}

// Version migration
void save_register_migration(save_system *system, u32 version, save_migration_fn fn) {
    if (version < ArrayCount(system->migration_table)) {
        system->migration_table[version] = fn;
        if (version >= system->migration_count) {
            system->migration_count = version + 1;
        }
    }
}

b32 save_migrate_data(save_system *system, save_buffer *data, u32 old_version, u32 new_version) {
    if (old_version == new_version) return 1;
    if (old_version > new_version) return 0; // Can't downgrade
    
    // Apply migrations in sequence
    for (u32 v = old_version; v < new_version; v++) {
        if (system->migration_table[v]) {
            save_buffer temp_buffer = {0};
            temp_buffer.data = system->compress_buffer.data;
            temp_buffer.capacity = system->compress_buffer.capacity;
            
            if (!system->migration_table[v](data, &temp_buffer, v, v + 1)) {
                return 0;
            }
            
            // Copy migrated data back
            memcpy(data->data, temp_buffer.data, temp_buffer.size);
            data->size = temp_buffer.size;
            data->read_offset = 0;
        }
    }
    
    return 1;
}

// Autosave management
void save_update_autosave(save_system *system, game_state *game, f32 dt) {
    if (!system->autosave_enabled) return;
    if (system->is_saving || system->is_loading) return;
    
    system->autosave_timer += dt;
    
    if (system->autosave_timer >= system->autosave_interval) {
        system->autosave_timer = 0.0f;
        save_game(system, game, SAVE_AUTOSAVE_SLOT);
    }
}

void save_enable_autosave(save_system *system, f32 interval_seconds) {
    system->autosave_enabled = 1;
    system->autosave_interval = interval_seconds;
    system->autosave_timer = 0.0f;
}

void save_disable_autosave(save_system *system) {
    system->autosave_enabled = 0;
}

// Debug functions
void save_dump_info(save_system *system) {
    printf("=== Save System Info ===\n");
    printf("Memory: %u bytes allocated\n", system->save_memory_size + system->compress_memory_size);
    printf("Last save time: %.2fms\n", system->last_save_time * 1000.0f);
    printf("Last load time: %.2fms\n", system->last_load_time * 1000.0f);
    printf("Total saved: %llu bytes\n", system->total_bytes_saved);
    printf("Total loaded: %llu bytes\n", system->total_bytes_loaded);
    printf("Autosave: %s (%.1fs interval)\n", 
           system->autosave_enabled ? "enabled" : "disabled",
           system->autosave_interval);
    
    printf("\nSave Slots:\n");
    for (i32 i = 0; i < SAVE_MAX_SLOTS; i++) {
        save_slot_info *slot = &system->slots[i];
        if (slot->exists) {
            printf("  Slot %d: %s (%.1f hours, v%u)\n", 
                   i, slot->filename, 
                   slot->metadata.playtime_seconds / 3600.0f,
                   slot->header.version);
        }
    }
}

void save_validate_integrity(save_system *system, i32 slot) {
    if (slot < 0 || slot >= SAVE_MAX_SLOTS) return;
    
    save_slot_info *info = &system->slots[slot];
    if (!info->exists) {
        printf("Slot %d: No save file\n", slot);
        return;
    }
    
    // Read entire file
    u32 actual_size;
    b32 result = platform_save_read_file(info->filename,
                                         system->read_buffer.data,
                                         system->read_buffer.capacity,
                                         &actual_size);
    
    if (!result) {
        printf("Slot %d: Failed to read file\n", slot);
        return;
    }
    
    // Verify header
    save_header *header = (save_header *)system->read_buffer.data;
    if (header->magic != SAVE_MAGIC_NUMBER) {
        printf("Slot %d: Invalid magic number\n", slot);
        return;
    }
    
    // Calculate checksum
    u32 data_start = sizeof(save_header);
    u32 data_size = actual_size - data_start;
    u32 calculated_crc = save_crc32(system->read_buffer.data + data_start, data_size);
    
    if (calculated_crc != header->checksum) {
        printf("Slot %d: Checksum mismatch! Expected: 0x%08X, Got: 0x%08X\n",
               slot, header->checksum, calculated_crc);
        system->save_corrupted = 1;
    } else {
        printf("Slot %d: Valid save file (v%u, %u bytes)\n",
               slot, header->version, actual_size);
    }
}