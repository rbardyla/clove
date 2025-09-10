#include "handmade_profiler_enhanced.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main() {
    printf("Basic profiler test\n");
    
    profiler_init_params params = {0};
    params.thread_count = 1;
    params.event_buffer_size = 1024 * 1024;
    params.enable_gpu_profiling = 0;
    params.enable_network_profiling = 0; 
    params.enable_memory_tracking = 1;
    
    profiler_system_init(&params);
    
    for (int frame = 0; frame < 60; frame++) {
        profiler_begin_frame();
        
        profiler_push_timer("test_function", 0xFFFFFF);
        usleep(1000); // 1ms work
        profiler_pop_timer();
        
        profiler_end_frame();
    }
    
    printf("Profiler test complete\n");
    profiler_shutdown();
    return 0;
}
