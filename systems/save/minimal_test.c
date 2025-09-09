/*
    Minimal save system test
    Tests just the core serialization without compression
*/

#include "handmade_save.h"
#include "save_stubs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    printf("=== Minimal Save System Test ===\n");
    
    // Allocate memory
    void *memory = malloc(Megabytes(8));
    if (!memory) {
        printf("Failed to allocate memory\n");
        return 1;
    }
    
    // Initialize save system
    save_system *system = save_system_init(memory, Megabytes(8));
    printf("Save system initialized\n");
    
    // Test basic serialization
    save_buffer *buffer = &system->write_buffer;
    save_buffer_reset(buffer);
    
    // Write some test data
    save_write_u32(buffer, 0xDEADBEEF);
    save_write_f32(buffer, 3.14159f);
    save_write_string(buffer, "Hello World");
    
    printf("Written %u bytes to buffer\n", buffer->size);
    
    // Read it back
    buffer->read_offset = 0;
    u32 test_u32 = save_read_u32(buffer);
    f32 test_f32 = save_read_f32(buffer);
    char test_string[64];
    save_read_string(buffer, test_string, 64);
    
    printf("Read back:\n");
    printf("  u32: 0x%08X (expected: 0xDEADBEEF)\n", test_u32);
    printf("  f32: %.5f (expected: 3.14159)\n", test_f32);
    printf("  str: '%s' (expected: 'Hello World')\n", test_string);
    
    // Test CRC32
    u8 test_data[] = {0x01, 0x02, 0x03, 0x04};
    u32 crc = save_crc32(test_data, 4);
    printf("CRC32 test: 0x%08X\n", crc);
    
    // Cleanup
    save_system_shutdown(system);
    free(memory);
    
    printf("Test completed successfully!\n");
    return 0;
}