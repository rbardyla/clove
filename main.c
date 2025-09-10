/*
    Minimal Handmade Engine Foundation
    
    This is the absolute minimal main.c that:
    1. Initializes the platform layer
    2. Opens a window with OpenGL context
    3. Sets up memory arenas
    4. Runs a basic main loop
    
    NO dependencies on GUI, renderer, or other systems.
    Just gets a window open as a foundation to build on.
*/

#include "handmade_platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <GL/gl.h>

#include "simple_gui.h"
#include "handmade_assets.h"
#include "handmade_threading.h"

// Forward declarations for threading functions
extern u32 get_cpu_count(void);
extern bool threading_init(MemoryArena* arena);
extern void threading_print_stats(void);
extern void threading_shutdown(void);
extern void process_gui_updates(simple_gui* gui);

// Include the implementation
#include "handmade_threading_integration.c"

// Editor state
typedef struct {
    // Scene objects for hierarchy
    gui_tree_node scene_nodes[16];
    i32 scene_node_count;
    i32 selected_object_index;
    
    // Property values for selected object
    f32 position[3];
    f32 rotation[3];
    f32 scale[3];
    char object_name[64];
    
    // File browser (old - will be replaced by asset browser)
    gui_file_browser file_browser;
    
    // Asset browser - the real deal
    AssetBrowser asset_browser;
    
    // Panel visibility
    bool show_scene_hierarchy;
    bool show_property_inspector;
    bool show_asset_browser;
    bool show_performance;
    
    // Current tool
    i32 current_tool;  // 0=Select, 1=Move, 2=Rotate, 3=Scale
} EditorState;

// Global state for our editor application
typedef struct {
    bool initialized;
    f32 time_accumulator;
    f32 background_color[3];  // RGB background color
    
    // Simple GUI system
    simple_gui gui;
    renderer gui_renderer;
    
    // Editor state
    EditorState editor;
    
    // Threading system
    ThreadPool* thread_pool;
    ThreadPool* render_pool;
    MemoryArena* thread_arena;
    
    // Performance tracking
    u64 frame_count;
    f64 frame_time_accum;
    f64 last_frame_time;
    ThreadPoolStats thread_stats;
} AppState;

static AppState g_app_state = {0};

// Required Game* functions that platform expects
void GameInit(PlatformState* platform) {
    printf("GameInit called\n");
    
    g_app_state.initialized = true;
    g_app_state.time_accumulator = 0.0f;
    g_app_state.background_color[0] = 0.2f;  // Dark blue background
    g_app_state.background_color[1] = 0.3f;
    g_app_state.background_color[2] = 0.4f;
    
    // Allocate memory for threading (64MB)
    g_app_state.thread_arena = (MemoryArena*)malloc(sizeof(MemoryArena));
    g_app_state.thread_arena->size = MEGABYTES(64);
    g_app_state.thread_arena->base = (u8*)malloc(g_app_state.thread_arena->size);
    g_app_state.thread_arena->used = 0;
    
    // Initialize threading system
    u32 cpu_count = get_cpu_count();
    printf("Detected %u CPU cores\n", cpu_count);
    
    g_app_state.thread_pool = thread_pool_create(cpu_count, g_app_state.thread_arena);
    g_app_state.render_pool = thread_pool_create(cpu_count / 2, g_app_state.thread_arena);
    
    // Initialize integrated threading components
    threading_init(g_app_state.thread_arena);
    
    printf("Threading system initialized with %u worker threads\n", cpu_count);
    
    // Set up OpenGL state
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // Initialize GUI renderer
    renderer_init(&g_app_state.gui_renderer, platform->window.width, platform->window.height);
    
    // Initialize simple GUI system
    simple_gui_init(&g_app_state.gui, &g_app_state.gui_renderer);
    
    // Initialize editor state
    EditorState *editor = &g_app_state.editor;
    
    // Set up sample scene hierarchy
    editor->scene_node_count = 6;
    
    // Root Scene node
    editor->scene_nodes[0] = (gui_tree_node){"Scene", true, 0, false};
    
    // Player object with children
    editor->scene_nodes[1] = (gui_tree_node){"Player", true, 1, true};
    editor->scene_nodes[2] = (gui_tree_node){"PlayerMesh", false, 2, false};
    editor->scene_nodes[3] = (gui_tree_node){"PlayerController", false, 2, false};
    
    // Environment objects
    editor->scene_nodes[4] = (gui_tree_node){"Environment", true, 1, false};
    editor->scene_nodes[5] = (gui_tree_node){"Lighting", false, 1, false};
    
    editor->selected_object_index = 1; // Select Player by default
    
    // Initialize selected object properties
    editor->position[0] = 0.0f; editor->position[1] = 0.0f; editor->position[2] = 0.0f;
    editor->rotation[0] = 0.0f; editor->rotation[1] = 0.0f; editor->rotation[2] = 0.0f;
    editor->scale[0] = 1.0f; editor->scale[1] = 1.0f; editor->scale[2] = 1.0f;
    snprintf(editor->object_name, sizeof(editor->object_name), "Player");
    
    // Initialize file browser (keeping for compatibility)
    snprintf(editor->file_browser.path, sizeof(editor->file_browser.path), "/assets/");
    editor->file_browser.selected_file = -1;
    
    // Initialize the real asset browser
    asset_browser_init(&editor->asset_browser, "./assets");
    
    // Show all panels by default
    editor->show_scene_hierarchy = true;
    editor->show_property_inspector = true;
    editor->show_asset_browser = true;
    editor->show_performance = true;
    
    // Default tool is Select
    editor->current_tool = 0;
    
    // Test threading system with parallel task
    printf("Testing parallel execution...\n");
    static f32 test_data[1000];
    thread_pool_parallel_for(g_app_state.thread_pool, 1000, 50,
        [](void* data, u32 index, u32 thread_index) {
            f32* array = (f32*)data;
            array[index] = sinf((f32)index) * cosf((f32)thread_index);
        }, test_data);
    printf("Parallel test completed\n");
    
    printf("OpenGL initialized\n");
    printf("GUI system initialized\n");
    printf("Window size: %dx%d\n", platform->window.width, platform->window.height);
}

void GameUpdate(PlatformState* platform, f32 dt) {
    if (!g_app_state.initialized) return;
    
    // Track frame timing
    g_app_state.frame_count++;
    g_app_state.frame_time_accum += dt;
    g_app_state.last_frame_time = dt;
    
    g_app_state.time_accumulator += dt;
    
    // Update color animation in parallel (demo of parallel work)
    static struct { f32 r, g, b; } color_update;
    thread_pool_parallel_for(g_app_state.thread_pool, 3, 1,
        [](void* data, u32 index, u32 thread_index) {
            f32* colors = (f32*)data;
            f32 time = g_app_state.time_accumulator;
            switch(index) {
                case 0: colors[0] = 0.2f + 0.1f * sinf(time * 0.5f); break;
                case 1: colors[1] = 0.3f + 0.1f * sinf(time * 0.7f); break;
                case 2: colors[2] = 0.4f + 0.1f * sinf(time * 0.3f); break;
            }
        }, &color_update);
    
    g_app_state.background_color[0] = color_update.r;
    g_app_state.background_color[1] = color_update.g;
    g_app_state.background_color[2] = color_update.b;
    
    // Begin GUI frame
    simple_gui_begin_frame(&g_app_state.gui, platform);
    
    // Handle basic input
    if (platform->input.keys[KEY_ESCAPE].pressed) {
        platform->window.should_close = true;
    }
    
    if (platform->input.keys[KEY_SPACE].pressed) {
        printf("Space pressed! Time: %.2f seconds\n", g_app_state.time_accumulator);
    }
    
    EditorState *editor = &g_app_state.editor;
    
    // Handle asset browser input
    asset_browser_handle_input(&editor->asset_browser, platform);
    
    // Toggle panels with function keys
    if (platform->input.keys[KEY_F1].pressed) {
        editor->show_scene_hierarchy = !editor->show_scene_hierarchy;
        printf("Scene Hierarchy panel: %s\n", editor->show_scene_hierarchy ? "ON" : "OFF");
    }
    
    if (platform->input.keys[KEY_F2].pressed) {
        editor->show_property_inspector = !editor->show_property_inspector;
        printf("Property Inspector panel: %s\n", editor->show_property_inspector ? "ON" : "OFF");
    }
    
    if (platform->input.keys[KEY_F3].pressed) {
        editor->show_asset_browser = !editor->show_asset_browser;
        printf("Asset Browser panel: %s\n", editor->show_asset_browser ? "ON" : "OFF");
    }
    
    if (platform->input.keys[KEY_F4].pressed) {
        editor->show_performance = !editor->show_performance;
        printf("Performance panel: %s\n", editor->show_performance ? "ON" : "OFF");
    }
    
    // Tool selection with number keys
    if (platform->input.keys[KEY_1].pressed) {
        editor->current_tool = 0;  // Select
        printf("Tool: Select\n");
    }
    if (platform->input.keys[KEY_2].pressed) {
        editor->current_tool = 1;  // Move
        printf("Tool: Move\n");
    }
    if (platform->input.keys[KEY_3].pressed) {
        editor->current_tool = 2;  // Rotate
        printf("Tool: Rotate\n");
    }
    if (platform->input.keys[KEY_4].pressed) {
        editor->current_tool = 3;  // Scale
        printf("Tool: Scale\n");
    }
    
    // F5 to print threading statistics
    if (platform->input.keys[KEY_F5].pressed) {
        thread_pool_get_stats(g_app_state.thread_pool, &g_app_state.thread_stats);
        threading_print_stats();
    }
    
    // Process any pending GUI updates from worker threads
    process_gui_updates(&g_app_state.gui);
}

bool tool_select_callback(void) { g_app_state.editor.current_tool = 0; return true; }
bool tool_move_callback(void) { g_app_state.editor.current_tool = 1; return true; }
bool tool_rotate_callback(void) { g_app_state.editor.current_tool = 2; return true; }
bool tool_scale_callback(void) { g_app_state.editor.current_tool = 3; return true; }

bool menu_file_new(void) { printf("File -> New\n"); return true; }
bool menu_file_open(void) { printf("File -> Open\n"); return true; }
bool menu_file_save(void) { printf("File -> Save\n"); return true; }
bool menu_edit_undo(void) { printf("Edit -> Undo\n"); return true; }
bool menu_edit_redo(void) { printf("Edit -> Redo\n"); return true; }
bool menu_view_wireframe(void) { printf("View -> Wireframe\n"); return true; }

void GameRender(PlatformState* platform) {
    if (!g_app_state.initialized) return;
    
    EditorState *editor = &g_app_state.editor;
    
    // Set viewport
    glViewport(0, 0, platform->window.width, platform->window.height);
    
    // Clear with dark background (editor style)
    glClearColor(0.15f, 0.15f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Simple 3D scene in center viewport (placeholder)
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Draw a simple wireframe cube to represent the 3D viewport
    glColor3f(0.8f, 0.8f, 0.8f);
    glBegin(GL_LINES);
        // Front face
        glVertex2f(-0.3f, -0.3f); glVertex2f( 0.3f, -0.3f);
        glVertex2f( 0.3f, -0.3f); glVertex2f( 0.3f,  0.3f);
        glVertex2f( 0.3f,  0.3f); glVertex2f(-0.3f,  0.3f);
        glVertex2f(-0.3f,  0.3f); glVertex2f(-0.3f, -0.3f);
        
        // Back face (offset)
        glVertex2f(-0.2f, -0.2f); glVertex2f( 0.4f, -0.2f);
        glVertex2f( 0.4f, -0.2f); glVertex2f( 0.4f,  0.4f);
        glVertex2f( 0.4f,  0.4f); glVertex2f(-0.2f,  0.4f);
        glVertex2f(-0.2f,  0.4f); glVertex2f(-0.2f, -0.2f);
        
        // Connecting lines
        glVertex2f(-0.3f, -0.3f); glVertex2f(-0.2f, -0.2f);
        glVertex2f( 0.3f, -0.3f); glVertex2f( 0.4f, -0.2f);
        glVertex2f( 0.3f,  0.3f); glVertex2f( 0.4f,  0.4f);
        glVertex2f(-0.3f,  0.3f); glVertex2f(-0.2f,  0.4f);
    glEnd();
    
    // === EDITOR INTERFACE ===
    
    i32 screen_width = g_app_state.gui_renderer.width;
    i32 screen_height = g_app_state.gui_renderer.height;
    
    // Define menu items
    static gui_menu_item file_items[] = {
        {"New", true, menu_file_new},
        {"Open", true, menu_file_open},
        {"Save", true, menu_file_save}
    };
    
    static gui_menu_item edit_items[] = {
        {"Undo", true, menu_edit_undo},
        {"Redo", true, menu_edit_redo}
    };
    
    static gui_menu_item view_items[] = {
        {"Wireframe", true, menu_view_wireframe}
    };
    
    // Define menus
    static gui_menu menus[] = {
        {"File", file_items, 3, false},
        {"Edit", edit_items, 2, false},
        {"View", view_items, 1, false}
    };
    
    // Draw menu bar
    simple_gui_menu_bar(&g_app_state.gui, 0, 0, menus, 3);
    
    // Define toolbar tools
    gui_tool_button tools[] = {
        {"S", editor->current_tool == 0, tool_select_callback},   // Select
        {"M", editor->current_tool == 1, tool_move_callback},     // Move
        {"R", editor->current_tool == 2, tool_rotate_callback},   // Rotate
        {"Z", editor->current_tool == 3, tool_scale_callback}     // Scale
    };
    
    // Draw toolbar
    simple_gui_toolbar(&g_app_state.gui, 0, 24, tools, 4);
    
    // Panel dimensions
    i32 panel_width = 280;
    i32 panel_height = 400;
    i32 menu_height = 24;
    i32 toolbar_height = 40;
    
    // === SCENE HIERARCHY PANEL ===
    if (editor->show_scene_hierarchy) {
        gui_panel hierarchy_panel = {
            .x = 10,
            .y = menu_height + toolbar_height + 10,
            .width = panel_width,
            .height = panel_height,
            .title = "Scene Hierarchy",
            .open = &editor->show_scene_hierarchy,
            .collapsed = false,
            .resizable = true
        };
        
        if (simple_gui_begin_panel(&g_app_state.gui, &hierarchy_panel)) {
            // Draw scene tree
            i32 node_y = g_app_state.gui.cursor_y;
            
            for (i32 i = 0; i < editor->scene_node_count; i++) {
                gui_tree_node *node = &editor->scene_nodes[i];
                
                // Only show child nodes if parent is expanded
                bool should_show = true;
                if (node->depth > 0) {
                    // Find parent and check if expanded
                    for (i32 j = i - 1; j >= 0; j--) {
                        if (editor->scene_nodes[j].depth == node->depth - 1) {
                            should_show = editor->scene_nodes[j].expanded;
                            break;
                        }
                    }
                }
                
                if (should_show) {
                    if (simple_gui_tree_node(&g_app_state.gui, g_app_state.gui.cursor_x, node_y, node)) {
                        editor->selected_object_index = i;
                        // Update property values based on selection
                        snprintf(editor->object_name, sizeof(editor->object_name), "%s", node->label);
                        printf("Selected object: %s\n", node->label);
                    }
                    node_y += 20;
                }
            }
            
            simple_gui_end_panel(&g_app_state.gui, &hierarchy_panel);
        }
    }
    
    // === PROPERTY INSPECTOR PANEL ===
    if (editor->show_property_inspector) {
        gui_panel inspector_panel = {
            .x = screen_width - panel_width - 10,
            .y = menu_height + toolbar_height + 10,
            .width = panel_width,
            .height = panel_height,
            .title = "Property Inspector",
            .open = &editor->show_property_inspector,
            .collapsed = false,
            .resizable = true
        };
        
        if (simple_gui_begin_panel(&g_app_state.gui, &inspector_panel)) {
            i32 prop_y = g_app_state.gui.cursor_y;
            
            // Object name
            simple_gui_property_string(&g_app_state.gui, g_app_state.gui.cursor_x, prop_y, "Name:", editor->object_name, sizeof(editor->object_name));
            prop_y += 25;
            
            // Transform section
            simple_gui_text(&g_app_state.gui, g_app_state.gui.cursor_x, prop_y, "Transform:");
            prop_y += 25;
            
            // Position
            simple_gui_property_float(&g_app_state.gui, g_app_state.gui.cursor_x, prop_y, "Position X:", &editor->position[0]);
            prop_y += 20;
            simple_gui_property_float(&g_app_state.gui, g_app_state.gui.cursor_x, prop_y, "Position Y:", &editor->position[1]);
            prop_y += 20;
            simple_gui_property_float(&g_app_state.gui, g_app_state.gui.cursor_x, prop_y, "Position Z:", &editor->position[2]);
            prop_y += 25;
            
            // Rotation  
            simple_gui_property_float(&g_app_state.gui, g_app_state.gui.cursor_x, prop_y, "Rotation X:", &editor->rotation[0]);
            prop_y += 20;
            simple_gui_property_float(&g_app_state.gui, g_app_state.gui.cursor_x, prop_y, "Rotation Y:", &editor->rotation[1]);
            prop_y += 20;
            simple_gui_property_float(&g_app_state.gui, g_app_state.gui.cursor_x, prop_y, "Rotation Z:", &editor->rotation[2]);
            prop_y += 25;
            
            // Scale
            simple_gui_property_float(&g_app_state.gui, g_app_state.gui.cursor_x, prop_y, "Scale X:", &editor->scale[0]);
            prop_y += 20;
            simple_gui_property_float(&g_app_state.gui, g_app_state.gui.cursor_x, prop_y, "Scale Y:", &editor->scale[1]);
            prop_y += 20;
            simple_gui_property_float(&g_app_state.gui, g_app_state.gui.cursor_x, prop_y, "Scale Z:", &editor->scale[2]);
            
            simple_gui_end_panel(&g_app_state.gui, &inspector_panel);
        }
    }
    
    // === ASSET BROWSER PANEL ===
    if (editor->show_asset_browser) {
        gui_panel browser_panel = {
            .x = 10,
            .y = screen_height - panel_height - 10,
            .width = panel_width * 2,
            .height = panel_height / 2,
            .title = "Asset Browser",
            .open = &editor->show_asset_browser,
            .collapsed = false,
            .resizable = true
        };
        
        if (simple_gui_begin_panel(&g_app_state.gui, &browser_panel)) {
            // Use the real asset browser instead of simple file browser
            asset_browser_draw(&editor->asset_browser, &g_app_state.gui,
                             g_app_state.gui.cursor_x, 
                             g_app_state.gui.cursor_y,
                             browser_panel.width - 16,
                             browser_panel.height - 40);
            
            simple_gui_end_panel(&g_app_state.gui, &browser_panel);
        }
    }
    
    // === PERFORMANCE PANEL ===
    if (editor->show_performance) {
        gui_panel perf_panel = {
            .x = 10,
            .y = menu_height + toolbar_height + panel_height + 30,
            .width = 450,
            .height = 280,
            .title = "Performance Monitor (Threading)",
            .open = &editor->show_performance,
            .collapsed = false,
            .resizable = true
        };
        
        if (simple_gui_begin_panel(&g_app_state.gui, &perf_panel)) {
            i32 perf_y = g_app_state.gui.cursor_y;
            char perf_text[256];
            
            // Frame timing
            f32 fps = g_app_state.frame_count > 0 ? 
                      (f32)g_app_state.frame_count / (f32)g_app_state.frame_time_accum : 0.0f;
            f32 frame_ms = g_app_state.last_frame_time * 1000.0f;
            
            snprintf(perf_text, sizeof(perf_text), "FPS: %.1f (%.2f ms)", fps, frame_ms);
            simple_gui_text(&g_app_state.gui, g_app_state.gui.cursor_x, perf_y, perf_text);
            perf_y += 20;
            
            // Threading statistics
            thread_pool_get_stats(g_app_state.thread_pool, &g_app_state.thread_stats);
            
            snprintf(perf_text, sizeof(perf_text), "Jobs: %lu completed / %lu submitted",
                    g_app_state.thread_stats.total_jobs_completed,
                    g_app_state.thread_stats.total_jobs_submitted);
            simple_gui_text(&g_app_state.gui, g_app_state.gui.cursor_x, perf_y, perf_text);
            perf_y += 20;
            
            snprintf(perf_text, sizeof(perf_text), "Active Threads: %u / %u",
                    g_app_state.thread_stats.active_thread_count,
                    g_app_state.thread_pool->thread_count);
            simple_gui_text(&g_app_state.gui, g_app_state.gui.cursor_x, perf_y, perf_text);
            perf_y += 25;
            
            // Per-thread utilization
            simple_gui_text(&g_app_state.gui, g_app_state.gui.cursor_x, perf_y, "Thread Utilization:");
            perf_y += 20;
            
            for (u32 i = 0; i < g_app_state.thread_pool->thread_count && i < 8; i++) {
                f32 utilization = g_app_state.thread_stats.thread_utilization[i];
                u64 jobs = g_app_state.thread_stats.jobs_per_thread[i];
                u64 steals = g_app_state.thread_stats.steal_count_per_thread[i];
                
                snprintf(perf_text, sizeof(perf_text), "Thread %u: %.1f%% | Jobs: %lu | Steals: %lu",
                        i, utilization * 100.0f, jobs, steals);
                simple_gui_text(&g_app_state.gui, g_app_state.gui.cursor_x + 20, perf_y, perf_text);
                perf_y += 18;
            }
            
            perf_y += 10;
            
            // Memory usage
            usize thread_mem_used = g_app_state.thread_arena->used;
            usize thread_mem_total = g_app_state.thread_arena->size;
            f32 mem_percent = (f32)thread_mem_used / (f32)thread_mem_total * 100.0f;
            
            snprintf(perf_text, sizeof(perf_text), "Thread Memory: %.1f MB / %.1f MB (%.1f%%)",
                    (f32)thread_mem_used / (1024.0f * 1024.0f),
                    (f32)thread_mem_total / (1024.0f * 1024.0f),
                    mem_percent);
            simple_gui_text(&g_app_state.gui, g_app_state.gui.cursor_x, perf_y, perf_text);
            
            simple_gui_end_panel(&g_app_state.gui, &perf_panel);
        }
    }
    
    // Status bar at bottom
    simple_gui_separator(&g_app_state.gui, 0, screen_height - 30, screen_width);
    
    // Thread status on left
    char thread_status[128];
    snprintf(thread_status, sizeof(thread_status), "Threads: %u | Jobs: %lu | F5: Thread Stats",
            g_app_state.thread_pool->thread_count,
            g_app_state.thread_stats.total_jobs_completed);
    simple_gui_text(&g_app_state.gui, 10, screen_height - 20, thread_status);
    
    // Controls in center
    simple_gui_text(&g_app_state.gui, screen_width/2 - 150, screen_height - 20, 
                   "F1-F4: Panels | 1-4: Tools | ESC: Quit");
    
    // Current tool on right
    const char* tool_names[] = {"Select", "Move", "Rotate", "Scale"};
    char tool_text[64];
    snprintf(tool_text, sizeof(tool_text), "Tool: %s", tool_names[editor->current_tool]);
    simple_gui_text(&g_app_state.gui, screen_width - 150, screen_height - 20, tool_text);
    
    // End GUI frame
    simple_gui_end_frame(&g_app_state.gui);
}

void GameShutdown(PlatformState* platform) {
    printf("GameShutdown called\n");
    
    // Shutdown threading system
    if (g_app_state.thread_pool) {
        printf("Shutting down threading system...\n");
        threading_print_stats();  // Print final statistics
        threading_shutdown();
        thread_pool_destroy(g_app_state.thread_pool);
        thread_pool_destroy(g_app_state.render_pool);
    }
    
    // Free thread memory arena
    if (g_app_state.thread_arena) {
        free(g_app_state.thread_arena->base);
        free(g_app_state.thread_arena);
    }
    
    // Shutdown GUI
    renderer_shutdown(&g_app_state.gui_renderer);
    
    g_app_state.initialized = false;
}

void GameOnReload(PlatformState* platform) {
    printf("GameOnReload called\n");
    // Nothing to do for hot reload in this minimal version
}