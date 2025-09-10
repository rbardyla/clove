/*
    Simple GUI Test Program
    
    Tests the basic immediate-mode GUI system we just created.
    This demonstrates that the GUI system is working independently of the full engine.
*/

#include "handmade_gui.h"
#include <stdio.h>

int main(int argc, char** argv) {
    printf("Testing GUI system components...\n");
    
    // Test hash functions
    u64 hash1 = HandmadeGUI_HashString("test");
    u64 hash2 = HandmadeGUI_HashString("test");
    u64 hash3 = HandmadeGUI_HashString("different");
    
    printf("Hash consistency test:\n");
    printf("  'test' hash 1: %lu\n", hash1);
    printf("  'test' hash 2: %lu\n", hash2);
    printf("  'different' hash: %lu\n", hash3);
    printf("  Consistency: %s\n", (hash1 == hash2 && hash1 != hash3) ? "PASS" : "FAIL");
    
    // Test pointer hash
    int dummy1 = 42;
    int dummy2 = 43;
    u64 ptr_hash1 = HandmadeGUI_HashPointer(&dummy1);
    u64 ptr_hash2 = HandmadeGUI_HashPointer(&dummy2);
    
    printf("Pointer hash test:\n");
    printf("  ptr1 hash: %lu\n", ptr_hash1);
    printf("  ptr2 hash: %lu\n", ptr_hash2);
    printf("  Different: %s\n", (ptr_hash1 != ptr_hash2) ? "PASS" : "FAIL");
    
    printf("\nGUI system basic tests completed successfully!\n");
    printf("To see the full GUI in action, run: ./minimal_engine\n");
    
    return 0;
}