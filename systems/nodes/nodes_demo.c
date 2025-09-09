// nodes_demo.c - Demonstration of node-based visual programming system
// PERFORMANCE: Showcases real-time execution, parallel nodes, AI behaviors

#include "handmade_nodes.h"
#include "../gui/handmade_gui.h"
#include "../gui/handmade_renderer.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

// Demo state
typedef struct {
    node_graph_t *main_graph;
    node_graph_t *ai_graph;
    node_graph_t *shader_graph;
    node_graph_t *procedural_graph;
    
    node_execution_context_t exec_context;
    node_theme_t theme;
    
    // Demo entities
    struct {
        f32 x, y;
        f32 vx, vy;
        f32 health;
        u32 color;
    } entities[100];
    i32 entity_count;
    
    // Performance tracking
    u64 frame_count;
    f32 avg_frame_time;
} demo_state_t;

static demo_state_t g_demo;

// Forward declarations
void create_example_game_logic(node_graph_t *graph);
void create_ai_behavior_tree(node_graph_t *graph);
void create_shader_graph(node_graph_t *graph);
void create_procedural_generator(node_graph_t *graph);

// Initialize demo
void demo_init(void) {
    // Allocate memory pool for nodes
    void *node_memory = malloc(Megabytes(16));
    nodes_init(node_memory, Megabytes(16));
    
    // Register all node types
    extern void nodes_library_init(void);
    nodes_library_init();
    
    // Create graphs
    g_demo.main_graph = node_graph_create("Game Logic");
    g_demo.ai_graph = node_graph_create("AI Behavior");
    g_demo.shader_graph = node_graph_create("Shader Graph");
    g_demo.procedural_graph = node_graph_create("Procedural Content");
    
    // Build example graphs
    create_example_game_logic(g_demo.main_graph);
    create_ai_behavior_tree(g_demo.ai_graph);
    create_shader_graph(g_demo.shader_graph);
    create_procedural_generator(g_demo.procedural_graph);
    
    // Set theme
    g_demo.theme = node_dark_theme();
    
    // Initialize entities
    g_demo.entity_count = 20;
    for (i32 i = 0; i < g_demo.entity_count; ++i) {
        g_demo.entities[i].x = (f32)(rand() % 800);
        g_demo.entities[i].y = (f32)(rand() % 600);
        g_demo.entities[i].vx = (f32)(rand() % 10 - 5);
        g_demo.entities[i].vy = (f32)(rand() % 10 - 5);
        g_demo.entities[i].health = 100.0f;
        g_demo.entities[i].color = RGB(rand() % 256, rand() % 256, rand() % 256);
    }
}

// Create example game logic graph
void create_example_game_logic(node_graph_t *graph) {
    // Player input handling
    node_t *on_update = node_create(graph, node_get_type_id("On Update"), 100, 100);
    node_t *get_input = node_create(graph, node_get_type_id("Get Input"), 300, 100);
    
    // Movement calculation
    node_t *multiply_speed = node_create(graph, node_get_type_id("Multiply"), 500, 100);
    node_t *speed_const = node_create(graph, node_get_type_id("Float Constant"), 500, 200);
    if (speed_const) {
        *(f32*)speed_const->custom_data = 5.0f;  // Speed value
    }
    
    // Health system
    node_t *health_check = node_create(graph, node_get_type_id("Less"), 300, 300);
    node_t *health_threshold = node_create(graph, node_get_type_id("Float Constant"), 100, 350);
    if (health_threshold) {
        *(f32*)health_threshold->custom_data = 20.0f;
    }
    
    node_t *branch = node_create(graph, node_get_type_id("Branch"), 500, 300);
    node_t *play_warning = node_create(graph, node_get_type_id("Play Sound"), 700, 250);
    node_t *normal_update = node_create(graph, node_get_type_id("Update Entity"), 700, 350);
    
    // Scoring system
    node_t *score_counter = node_create(graph, node_get_type_id("Int Variable"), 100, 500);
    node_t *add_score = node_create(graph, node_get_type_id("Add"), 300, 500);
    node_t *score_increment = node_create(graph, node_get_type_id("Int Constant"), 300, 600);
    if (score_increment) {
        *(i32*)score_increment->custom_data = 10;
    }
    
    // Debug output
    node_t *print_score = node_create(graph, node_get_type_id("Print"), 500, 500);
    
    // Connect nodes
    /*
    if (on_update && get_input) {
        node_connect(graph, on_update->id, 0, get_input->id, 0);
    }
    
    if (get_input && multiply_speed && speed_const) {
        node_connect(graph, get_input->id, 1, multiply_speed->id, 0);
        node_connect(graph, speed_const->id, 0, multiply_speed->id, 1);
    }
    
    if (health_check && health_threshold && branch) {
        node_connect(graph, health_threshold->id, 0, health_check->id, 1);
        node_connect(graph, health_check->id, 0, branch->id, 1);
    }
    
    if (branch && play_warning && normal_update) {
        node_connect(graph, branch->id, 0, play_warning->id, 0);  // True branch
        node_connect(graph, branch->id, 1, normal_update->id, 0);  // False branch
    }
    */
    
    // Compile graph
    node_graph_compile(graph);
}

// Create AI behavior tree
void create_ai_behavior_tree(node_graph_t *graph) {
    // Root selector
    node_t *root = node_create(graph, node_get_type_id("Sequence"), 400, 50);
    
    // Perception branch
    node_t *can_see_player = node_create(graph, node_get_type_id("Can See Player"), 200, 150);
    node_t *distance_check = node_create(graph, node_get_type_id("Distance Check"), 200, 250);
    
    // Decision branch
    node_t *selector = node_create(graph, node_get_type_id("Selector"), 400, 200);
    
    // Attack behavior
    node_t *attack_seq = node_create(graph, node_get_type_id("Sequence"), 250, 350);
    node_t *move_to_player = node_create(graph, node_get_type_id("Move To Target"), 150, 450);
    node_t *attack = node_create(graph, node_get_type_id("Attack"), 350, 450);
    
    // Patrol behavior
    node_t *patrol_seq = node_create(graph, node_get_type_id("Sequence"), 550, 350);
    node_t *pick_waypoint = node_create(graph, node_get_type_id("Pick Waypoint"), 450, 450);
    node_t *move_to_waypoint = node_create(graph, node_get_type_id("Move To Target"), 650, 450);
    
    // Idle behavior
    node_t *idle = node_create(graph, node_get_type_id("Idle"), 400, 550);
    
    // Connect behavior tree
    // Root connects to perception checks
    // Selector chooses between attack, patrol, or idle
    // Each behavior is a sequence of actions
    
    node_graph_compile(graph);
}

// Create shader graph (node-based shader editor)
void create_shader_graph(node_graph_t *graph) {
    // Inputs
    node_t *uv_coord = node_create(graph, node_get_type_id("UV Coordinates"), 100, 100);
    node_t *time = node_create(graph, node_get_type_id("Time"), 100, 200);
    
    // Wave distortion
    node_t *multiply_time = node_create(graph, node_get_type_id("Multiply"), 300, 200);
    node_t *wave_speed = node_create(graph, node_get_type_id("Float Constant"), 300, 300);
    if (wave_speed) {
        *(f32*)wave_speed->custom_data = 2.0f;
    }
    
    node_t *sin_wave = node_create(graph, node_get_type_id("Sin"), 500, 200);
    node_t *wave_amplitude = node_create(graph, node_get_type_id("Float Constant"), 500, 300);
    if (wave_amplitude) {
        *(f32*)wave_amplitude->custom_data = 0.1f;
    }
    
    node_t *multiply_amp = node_create(graph, node_get_type_id("Multiply"), 700, 250);
    
    // Color mixing
    node_t *color1 = node_create(graph, node_get_type_id("Color Constant"), 100, 400);
    node_t *color2 = node_create(graph, node_get_type_id("Color Constant"), 100, 500);
    node_t *lerp_color = node_create(graph, node_get_type_id("Lerp"), 400, 450);
    
    // Output
    node_t *output = node_create(graph, node_get_type_id("Shader Output"), 700, 450);
    
    node_graph_compile(graph);
}

// Create procedural content generator
void create_procedural_generator(node_graph_t *graph) {
    // Seed input
    node_t *seed = node_create(graph, node_get_type_id("Int Variable"), 100, 100);
    
    // Noise generation
    node_t *noise = node_create(graph, node_get_type_id("Perlin Noise"), 300, 100);
    node_t *octaves = node_create(graph, node_get_type_id("Int Constant"), 300, 200);
    if (octaves) {
        *(i32*)octaves->custom_data = 4;
    }
    
    // Threshold for features
    node_t *threshold = node_create(graph, node_get_type_id("Greater"), 500, 100);
    node_t *threshold_value = node_create(graph, node_get_type_id("Float Constant"), 500, 200);
    if (threshold_value) {
        *(f32*)threshold_value->custom_data = 0.5f;
    }
    
    // Feature placement
    node_t *branch = node_create(graph, node_get_type_id("Branch"), 700, 150);
    node_t *place_tree = node_create(graph, node_get_type_id("Spawn Entity"), 900, 100);
    node_t *place_rock = node_create(graph, node_get_type_id("Spawn Entity"), 900, 200);
    
    // Loop for grid generation
    node_t *for_x = node_create(graph, node_get_type_id("For Loop"), 100, 350);
    node_t *for_y = node_create(graph, node_get_type_id("For Loop"), 300, 350);
    
    node_t *grid_size = node_create(graph, node_get_type_id("Int Constant"), 100, 450);
    if (grid_size) {
        *(i32*)grid_size->custom_data = 100;
    }
    
    node_graph_compile(graph);
}

// Update demo
void demo_update(f32 dt) {
    g_demo.frame_count++;
    
    // Update entities
    for (i32 i = 0; i < g_demo.entity_count; ++i) {
        g_demo.entities[i].x += g_demo.entities[i].vx * dt;
        g_demo.entities[i].y += g_demo.entities[i].vy * dt;
        
        // Bounce off walls
        if (g_demo.entities[i].x < 0 || g_demo.entities[i].x > 800) {
            g_demo.entities[i].vx = -g_demo.entities[i].vx;
        }
        if (g_demo.entities[i].y < 0 || g_demo.entities[i].y > 600) {
            g_demo.entities[i].vy = -g_demo.entities[i].vy;
        }
    }
    
    // Execute main game logic graph
    g_demo.exec_context.user_data = &g_demo;
    executor_execute_graph(g_demo.main_graph, &g_demo.exec_context);
    
    // Execute AI for each entity
    for (i32 i = 0; i < g_demo.entity_count; ++i) {
        g_demo.exec_context.user_data = &g_demo.entities[i];
        executor_execute_graph(g_demo.ai_graph, &g_demo.exec_context);
    }
    
    // Update performance stats
    f32 frame_time = (f32)(g_demo.exec_context.total_cycles / 3000000.0);
    g_demo.avg_frame_time = g_demo.avg_frame_time * 0.95f + frame_time * 0.05f;
}

// Render demo
void demo_render(renderer *r, i32 width, i32 height) {
    // Clear background
    renderer_clear(r, rgb(32, 32, 32));
    
    // Render node graph
    extern void node_graph_render(renderer *r, node_graph_t *graph, node_theme_t *theme,
                                  i32 screen_width, i32 screen_height);
    node_graph_render(r, g_demo.main_graph, &g_demo.theme, width, height);
    
    // Render entities
    for (i32 i = 0; i < g_demo.entity_count; ++i) {
        i32 x = (i32)g_demo.entities[i].x;
        i32 y = (i32)g_demo.entities[i].y;
        
        // Health bar
        i32 bar_width = (i32)(40 * g_demo.entities[i].health / 100.0f);
        renderer_fill_rect(r, x - 20, y - 30, bar_width, 4, rgb(0, 255, 0));
        renderer_fill_rect(r, x - 20 + bar_width, y - 30, 40 - bar_width, 4, rgb(255, 0, 0));
        
        // Entity
        renderer_fill_circle(r, x, y, 10, 
                           rgba((g_demo.entities[i].color >> 16) & 0xFF,
                                (g_demo.entities[i].color >> 8) & 0xFF,
                                g_demo.entities[i].color & 0xFF, 255));
    }
    
    // Render performance overlay
    char perf_text[256];
    snprintf(perf_text, 256, "Demo FPS: %.0f | Frame: %.2f ms | Nodes: %d | Entities: %d",
            1000.0f / g_demo.avg_frame_time, g_demo.avg_frame_time,
            g_demo.main_graph->node_count, g_demo.entity_count);
    renderer_text(r, 10, height - 30, perf_text, rgb(255, 255, 0));
    
    // Instructions
    renderer_text(r, 10, height - 50, "Q: Quick Add | RMB: Context Menu | Scroll: Zoom | MMB: Pan", 
                 rgb(200, 200, 200));
    renderer_text(r, 10, height - 65, "Ctrl+C/V: Copy/Paste | Delete: Remove | Ctrl+Z/Y: Undo/Redo",
                 rgb(200, 200, 200));
}

// Interactive tutorial
void demo_tutorial(gui_context *gui) {
    static i32 tutorial_step = 0;
    static const char *tutorial_texts[] = {
        "Welcome to the Node-Based Visual Programming System!",
        "This system allows you to create game logic without writing code.",
        "Right-click to open the context menu and add nodes.",
        "Connect nodes by dragging from output pins to input pins.",
        "Press Q for quick node search.",
        "Use the minimap to navigate large graphs.",
        "Enable performance overlay to see execution times.",
        "Try creating a simple calculation: Add two numbers.",
        "Experiment with flow control: Branch and Loop nodes.",
        "Save your graphs and export them as C code!",
        NULL
    };
    
    if (tutorial_texts[tutorial_step] == NULL) {
        tutorial_step = 0;  // Loop tutorial
    }
    
    // Show tutorial panel
    gui_begin_panel(gui, gui->platform->window_width / 2 - 200, 10, 400, 80, "Tutorial");
    gui_label(gui, tutorial_texts[tutorial_step]);
    
    gui_begin_layout(gui, LAYOUT_HORIZONTAL, 10);
    if (gui_button(gui, "Previous") && tutorial_step > 0) {
        tutorial_step--;
    }
    if (gui_button(gui, "Next") && tutorial_texts[tutorial_step + 1] != NULL) {
        tutorial_step++;
    }
    if (gui_button(gui, "Skip")) {
        tutorial_step = 9;
    }
    gui_end_layout(gui);
    
    gui_end_panel(gui);
}

// Benchmark different graph sizes
void demo_benchmark(void) {
    printf("=== Node System Performance Benchmark ===\n\n");
    
    // Test different graph sizes
    i32 sizes[] = {10, 50, 100, 500, 1000};
    
    for (i32 s = 0; s < 5; ++s) {
        i32 size = sizes[s];
        
        // Create test graph
        node_graph_t *test_graph = node_graph_create("Benchmark");
        
        // Create chain of math nodes
        node_t *prev = NULL;
        for (i32 i = 0; i < size; ++i) {
            node_t *node = node_create(test_graph, node_get_type_id("Add"),
                                      (f32)(i * 150), 100);
            
            if (prev && node) {
                node_connect(test_graph, prev->id, 0, node->id, 0);
            }
            prev = node;
        }
        
        // Compile
        u64 compile_start = ReadCPUTimer();
        node_graph_compile(test_graph);
        u64 compile_time = ReadCPUTimer() - compile_start;
        
        // Execute multiple times
        node_execution_context_t ctx = {0};
        u64 exec_start = ReadCPUTimer();
        
        for (i32 i = 0; i < 1000; ++i) {
            executor_execute_graph(test_graph, &ctx);
        }
        
        u64 exec_time = ReadCPUTimer() - exec_start;
        
        printf("Graph size: %d nodes\n", size);
        printf("  Compile: %.3f ms\n", compile_time / 3000000.0);
        printf("  Execute (1000x): %.3f ms\n", exec_time / 3000000.0);
        printf("  Per execution: %.3f us\n", exec_time / 3000000.0);
        printf("  Throughput: %.0f executions/sec\n\n", 
               3000000000.0 / (exec_time / 1000.0));
        
        node_graph_destroy(test_graph);
    }
    
    // Test cache performance
    printf("Cache Performance:\n");
    u64 hits, misses;
    i32 entries;
    executor_get_cache_stats(&hits, &misses, &entries);
    printf("  Hits: %llu, Misses: %llu\n", hits, misses);
    printf("  Hit rate: %.1f%%\n", 100.0 * hits / (hits + misses + 1));
    printf("  Cache entries: %d\n\n", entries);
}

// Main demo entry point
void nodes_demo_main(void) {
    printf("Node-Based Visual Programming System Demo\n");
    printf("=========================================\n\n");
    
    // Initialize
    demo_init();
    
    // Run benchmark
    demo_benchmark();
    
    // Save example graphs
    node_graph_save(g_demo.main_graph, "game_logic.nodes");
    node_graph_save(g_demo.ai_graph, "ai_behavior.nodes");
    node_graph_save(g_demo.shader_graph, "shader.nodes");
    node_graph_save(g_demo.procedural_graph, "procedural.nodes");
    
    // Export to C code
    node_graph_export_c(g_demo.main_graph, "game_logic_generated.c");
    
    printf("\nDemo graphs saved and exported!\n");
    printf("Files created:\n");
    printf("  - game_logic.nodes\n");
    printf("  - ai_behavior.nodes\n");
    printf("  - shader.nodes\n");
    printf("  - procedural.nodes\n");
    printf("  - game_logic_generated.c\n");
}