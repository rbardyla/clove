/*
    Handmade OpenGL Function Loader Implementation
*/

#include "handmade_opengl.h"
#include <stdio.h>
#include <stdbool.h>

// Define function pointers
#define X(ret, name, args) PFN##name name = NULL;
GL_FUNCTIONS
#undef X

// Load all OpenGL functions
b32 gl_load_functions(void *(*get_proc_address)(char *)) {
    if (!get_proc_address) {
        printf("ERROR: No OpenGL proc address getter provided\n");
        return false;
    }
    
    b32 success = true;
    
    // Load each function
    #define X(ret, name, args) \
        name = (PFN##name)get_proc_address(#name); \
        if (!name) { \
            printf("ERROR: Failed to load OpenGL function: %s\n", #name); \
            success = false; \
        }
    GL_FUNCTIONS
    #undef X
    
    if (success) {
        printf("Successfully loaded all OpenGL functions\n");
    }
    
    return success;
}