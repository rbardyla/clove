// Test file for Continental Editor V4
// This demonstrates the handmade text editor

#include <stdio.h>

void test_function() {
    printf("Hello from the handmade editor!\n");
    
    // Demonstrate cursor movement
    int x = 10;
    int y = 20;
    
    for (int i = 0; i < x; i++) {
        printf("Line %d\n", i);
    }
}

int main() {
    test_function();
    return 0;
}