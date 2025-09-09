// editor_demo.c - Professional game engine editor demonstration
// Shows off docking, property inspector, asset browser, and more

#include "handmade_editor_dock.h"
#include "../gui/handmade_gui.c"
#include "../gui/handmade_renderer.c"
#include "../gui/handmade_platform_linux.c"
#include <time.h>
#include <stdlib.h>

// Demo application state
typedef struct {
    gui_context gui;
    dock_manager dock;
    renderer render;
    platform_state platform;
    
    // Demo content
    bool show_hierarchy;
    bool show_inspector;
    bool show_viewport;
    bool show_assets;
    bool show_console;
    bool show_profiler;
    bool show_node_editor;
    
    // Demo data
    f32 test_float;
    i32 test_int;
    bool test_bool;
    f32 test_color[4];
    char test_string[256];
    
    // Performance metrics
    f64 frame_start_time;
    f32 delta_time;
    u32 frame_count;
    f32 fps;
    
} editor_app;

// Forward declarations
void render_hierarchy_window(editor_app* app);
void render_inspector_window(editor_app* app);
void render_viewport_window(editor_app* app);
void render_assets_window(editor_app* app);
void render_console_window(editor_app* app);
void render_profiler_window(editor_app* app);
void render_node_editor_window(editor_app* app);
void render_menu_bar(editor_app* app);

int main(int argc, char** argv)
{
    editor_app app = {0};
    
    // Initialize platform
    if (!platform_init(&app.platform, "Professional Game Engine Editor", 1920, 1080)) {
        fprintf(stderr, "Failed to initialize platform\n");
        return 1;
    }
    
    // Initialize renderer
    renderer_init(&app.render, &app.platform);
    
    // Initialize GUI
    gui_init(&app.gui, &app.render, &app.platform);
    
    // Initialize docking system
    dock_init(&app.dock, &app.gui);
    
    // Set initial window states
    app.show_hierarchy = true;
    app.show_inspector = true;
    app.show_viewport = true;
    app.show_assets = true;
    app.show_console = true;
    app.show_profiler = false;
    app.show_node_editor = false;
    
    // Initialize demo data
    app.test_float = 3.14159f;
    app.test_int = 42;
    app.test_bool = true;
    app.test_color[0] = 0.2f;
    app.test_color[1] = 0.4f;
    app.test_color[2] = 0.8f;
    app.test_color[3] = 1.0f;
    strcpy(app.test_string, "Hello, World!");
    
    // Create and apply default layout
    dock_apply_preset_default(&app.dock);
    
    // Main loop
    while (platform_process_events(&app.platform)) {
        // Calculate delta time
        f64 current_time = platform_get_time(&app.platform);
        app.delta_time = (f32)(current_time - app.frame_start_time);
        app.frame_start_time = current_time;
        app.frame_count++;
        
        // Update FPS counter
        if (app.frame_count % 30 == 0) {
            app.fps = 1.0f / app.delta_time;
        }
        
        // Begin frame
        renderer_begin_frame(&app.render);
        gui_begin_frame(&app.gui);
        
        // Update dock layout
        dock_update_layout(&app.dock, app.delta_time);
        
        // Render menu bar
        render_menu_bar(&app);
        
        // Begin main dockspace
        v2 dockspace_pos = {0, 25};  // Below menu bar
        v2 dockspace_size = {(f32)app.platform.window_width, (f32)app.platform.window_height - 25};
        dock_begin_dockspace(&app.dock, "MainDockSpace", dockspace_pos, dockspace_size);
        
        // Render docked windows
        if (app.show_hierarchy) render_hierarchy_window(&app);
        if (app.show_inspector) render_inspector_window(&app);
        if (app.show_viewport) render_viewport_window(&app);
        if (app.show_assets) render_assets_window(&app);
        if (app.show_console) render_console_window(&app);
        if (app.show_profiler) render_profiler_window(&app);
        if (app.show_node_editor) render_node_editor_window(&app);
        
        // End dockspace
        dock_end_dockspace(&app.dock);
        
        // Show performance overlay
        if (app.dock.show_debug_overlay) {
            dock_render_debug_overlay(&app.dock);
        }
        
        // End frame
        gui_end_frame(&app.gui);
        renderer_end_frame(&app.render);
        platform_swap_buffers(&app.platform);
    }
    
    // Cleanup
    dock_shutdown(&app.dock);
    gui_shutdown(&app.gui);
    renderer_shutdown(&app.render);
    platform_shutdown(&app.platform);
    
    return 0;
}

void render_menu_bar(editor_app* app)
{
    if (gui_begin_menu_bar(&app->gui)) {
        if (gui_begin_menu(&app->gui, "File")) {
            if (gui_menu_item(&app->gui, "New Project", "Ctrl+N", false, true)) {
                // TODO: New project
            }
            if (gui_menu_item(&app->gui, "Open Project", "Ctrl+O", false, true)) {
                // TODO: Open project
            }
            if (gui_menu_item(&app->gui, "Save", "Ctrl+S", false, true)) {
                // TODO: Save
            }
            gui_separator(&app->gui);
            if (gui_menu_item(&app->gui, "Exit", "Alt+F4", false, true)) {
                app->platform.quit_requested = true;
            }
            gui_end_menu(&app->gui);
        }
        
        if (gui_begin_menu(&app->gui, "Edit")) {
            if (gui_menu_item(&app->gui, "Undo", "Ctrl+Z", false, true)) {
                // TODO: Undo
            }
            if (gui_menu_item(&app->gui, "Redo", "Ctrl+Y", false, true)) {
                // TODO: Redo
            }
            gui_separator(&app->gui);
            if (gui_menu_item(&app->gui, "Copy", "Ctrl+C", false, true)) {
                // TODO: Copy
            }
            if (gui_menu_item(&app->gui, "Paste", "Ctrl+V", false, true)) {
                // TODO: Paste
            }
            gui_end_menu(&app->gui);
        }
        
        if (gui_begin_menu(&app->gui, "View")) {
            gui_menu_item(&app->gui, "Hierarchy", NULL, app->show_hierarchy, true);
            gui_menu_item(&app->gui, "Inspector", NULL, app->show_inspector, true);
            gui_menu_item(&app->gui, "Viewport", NULL, app->show_viewport, true);
            gui_menu_item(&app->gui, "Assets", NULL, app->show_assets, true);
            gui_menu_item(&app->gui, "Console", NULL, app->show_console, true);
            gui_menu_item(&app->gui, "Profiler", NULL, app->show_profiler, true);
            gui_menu_item(&app->gui, "Node Editor", NULL, app->show_node_editor, true);
            gui_separator(&app->gui);
            gui_menu_item(&app->gui, "Debug Overlay", NULL, app->dock.show_debug_overlay, true);
            gui_end_menu(&app->gui);
        }
        
        if (gui_begin_menu(&app->gui, "Layout")) {
            if (gui_menu_item(&app->gui, "Default", NULL, false, true)) {
                dock_apply_preset_default(&app->dock);
            }
            if (gui_menu_item(&app->gui, "Code", NULL, false, true)) {
                dock_apply_preset_code(&app->dock);
            }
            if (gui_menu_item(&app->gui, "Art", NULL, false, true)) {
                dock_apply_preset_art(&app->dock);
            }
            if (gui_menu_item(&app->gui, "Debug", NULL, false, true)) {
                dock_apply_preset_debug(&app->dock);
            }
            gui_separator(&app->gui);
            if (gui_menu_item(&app->gui, "Save Layout", NULL, false, true)) {
                dock_save_layout(&app->dock, "layout.dock");
            }
            if (gui_menu_item(&app->gui, "Load Layout", NULL, false, true)) {
                dock_load_layout(&app->dock, "layout.dock");
            }
            gui_end_menu(&app->gui);
        }
        
        // Right-aligned FPS counter
        char fps_text[64];
        snprintf(fps_text, sizeof(fps_text), "%.1f FPS | %.2f ms", app->fps, app->delta_time * 1000.0f);
        f32 text_width = strlen(fps_text) * 8.0f;  // Approximate
        gui_same_line(&app->gui, app->platform.window_width - text_width - 20);
        gui_text(&app->gui, "%s", fps_text);
        
        gui_end_menu_bar(&app->gui);
    }
}

void render_hierarchy_window(editor_app* app)
{
    if (dock_begin_window(&app->dock, "Hierarchy", &app->show_hierarchy)) {
        gui_text(&app->gui, "Scene Hierarchy");
        gui_separator(&app->gui);
        
        // Demo scene tree
        if (gui_tree_node(&app->gui, "World")) {
            if (gui_tree_node(&app->gui, "Lights")) {
                gui_bullet_text(&app->gui, "DirectionalLight");
                gui_bullet_text(&app->gui, "PointLight_01");
                gui_bullet_text(&app->gui, "PointLight_02");
                gui_tree_pop(&app->gui);
            }
            
            if (gui_tree_node(&app->gui, "Geometry")) {
                gui_bullet_text(&app->gui, "Floor");
                gui_bullet_text(&app->gui, "Wall_North");
                gui_bullet_text(&app->gui, "Wall_South");
                gui_bullet_text(&app->gui, "Wall_East");
                gui_bullet_text(&app->gui, "Wall_West");
                gui_tree_pop(&app->gui);
            }
            
            if (gui_tree_node(&app->gui, "Actors")) {
                gui_bullet_text(&app->gui, "Player");
                gui_bullet_text(&app->gui, "NPC_01");
                gui_bullet_text(&app->gui, "NPC_02");
                gui_tree_pop(&app->gui);
            }
            
            gui_tree_pop(&app->gui);
        }
        
        dock_end_window(&app->dock);
    }
}

void render_inspector_window(editor_app* app)
{
    if (dock_begin_window(&app->dock, "Inspector", &app->show_inspector)) {
        gui_text(&app->gui, "Properties");
        gui_separator(&app->gui);
        
        // Transform section
        if (gui_tree_node_ex(&app->gui, "Transform", GUI_TREE_NODE_OPENED)) {
            f32 position[3] = {0.0f, 1.0f, -5.0f};
            f32 rotation[3] = {0.0f, 45.0f, 0.0f};
            f32 scale[3] = {1.0f, 1.0f, 1.0f};
            
            gui_drag_float3(&app->gui, "Position", position, 0.1f, -1000.0f, 1000.0f);
            gui_drag_float3(&app->gui, "Rotation", rotation, 1.0f, -360.0f, 360.0f);
            gui_drag_float3(&app->gui, "Scale", scale, 0.01f, 0.01f, 100.0f);
            
            gui_tree_pop(&app->gui);
        }
        
        // Material section
        if (gui_tree_node_ex(&app->gui, "Material", GUI_TREE_NODE_OPENED)) {
            gui_color_edit4(&app->gui, "Albedo", app->test_color);
            gui_slider_float(&app->gui, "Metallic", &app->test_float, 0.0f, 1.0f);
            gui_slider_float(&app->gui, "Roughness", &app->test_float, 0.0f, 1.0f);
            
            gui_tree_pop(&app->gui);
        }
        
        // Physics section
        if (gui_tree_node(&app->gui, "Physics")) {
            gui_checkbox(&app->gui, "Is Kinematic", &app->test_bool);
            gui_checkbox(&app->gui, "Use Gravity", &app->test_bool);
            gui_drag_float(&app->gui, "Mass", &app->test_float, 0.1f, 0.0f, 1000.0f);
            gui_drag_float(&app->gui, "Drag", &app->test_float, 0.01f, 0.0f, 10.0f);
            
            gui_tree_pop(&app->gui);
        }
        
        // Tags section
        if (gui_tree_node(&app->gui, "Tags & Layers")) {
            gui_input_text(&app->gui, "Tag", app->test_string, sizeof(app->test_string));
            
            const char* layers[] = {"Default", "UI", "Player", "Enemy", "Environment"};
            static i32 current_layer = 0;
            gui_combo(&app->gui, "Layer", &current_layer, layers, 5);
            
            gui_tree_pop(&app->gui);
        }
        
        dock_end_window(&app->dock);
    }
}

void render_viewport_window(editor_app* app)
{
    if (dock_begin_window(&app->dock, "Viewport", &app->show_viewport)) {
        v2 content_size = gui_get_content_region_avail(&app->gui);
        
        // Toolbar
        if (gui_button(&app->gui, "Translate")) {}
        gui_same_line(&app->gui, 0);
        if (gui_button(&app->gui, "Rotate")) {}
        gui_same_line(&app->gui, 0);
        if (gui_button(&app->gui, "Scale")) {}
        gui_same_line(&app->gui, 0);
        gui_separator(&app->gui);
        gui_same_line(&app->gui, 0);
        if (gui_button(&app->gui, "Play")) {}
        gui_same_line(&app->gui, 0);
        if (gui_button(&app->gui, "Pause")) {}
        gui_same_line(&app->gui, 0);
        if (gui_button(&app->gui, "Stop")) {}
        
        gui_separator(&app->gui);
        
        // Viewport area (placeholder)
        v2 viewport_size = gui_get_content_region_avail(&app->gui);
        v2 viewport_pos = gui_get_cursor_pos(&app->gui);
        
        // Draw a placeholder 3D scene
        gui_draw_rect_filled(&app->gui, viewport_pos, 
                           v2_add(viewport_pos, viewport_size),
                           (color32){20, 20, 30, 255}, 0.0f);
        
        // Draw grid
        f32 grid_spacing = 50.0f;
        color32 grid_color = {40, 40, 50, 255};
        
        for (f32 x = 0; x < viewport_size.x; x += grid_spacing) {
            gui_draw_line(&app->gui, 
                        v2_add(viewport_pos, (v2){x, 0}),
                        v2_add(viewport_pos, (v2){x, viewport_size.y}),
                        grid_color, 1.0f);
        }
        
        for (f32 y = 0; y < viewport_size.y; y += grid_spacing) {
            gui_draw_line(&app->gui,
                        v2_add(viewport_pos, (v2){0, y}),
                        v2_add(viewport_pos, (v2){viewport_size.x, y}),
                        grid_color, 1.0f);
        }
        
        // Draw some "3D" objects
        v2 center = v2_add(viewport_pos, v2_scale(viewport_size, 0.5f));
        gui_draw_circle_filled(&app->gui, center, 50.0f, 
                             (color32){100, 150, 200, 255}, 32);
        
        // Stats overlay
        char stats[256];
        snprintf(stats, sizeof(stats), 
                "Viewport: %.0fx%.0f\nObjects: 142\nTriangles: 28.5k\nDraw Calls: 87",
                viewport_size.x, viewport_size.y);
        gui_draw_text(&app->gui, v2_add(viewport_pos, (v2){10, 10}),
                    (color32){200, 200, 200, 255}, stats, stats + strlen(stats));
        
        dock_end_window(&app->dock);
    }
}

void render_assets_window(editor_app* app)
{
    if (dock_begin_window(&app->dock, "Assets", &app->show_assets)) {
        // Search bar
        static char search[256] = "";
        gui_input_text(&app->gui, "Search", search, sizeof(search));
        
        gui_separator(&app->gui);
        
        // Asset browser toolbar
        if (gui_button(&app->gui, "Create")) {}
        gui_same_line(&app->gui, 0);
        if (gui_button(&app->gui, "Import")) {}
        gui_same_line(&app->gui, 0);
        if (gui_button(&app->gui, "Refresh")) {}
        
        gui_separator(&app->gui);
        
        // Folder tree
        gui_begin_layout(&app->gui, LAYOUT_HORIZONTAL, 5.0f);
        
        // Left panel - folders
        gui_begin_panel(&app->gui, "Folders");
        if (gui_tree_node(&app->gui, "Assets")) {
            if (gui_tree_node(&app->gui, "Materials")) {
                gui_bullet_text(&app->gui, "Standard");
                gui_bullet_text(&app->gui, "Unlit");
                gui_tree_pop(&app->gui);
            }
            if (gui_tree_node(&app->gui, "Textures")) {
                gui_bullet_text(&app->gui, "Diffuse");
                gui_bullet_text(&app->gui, "Normal");
                gui_tree_pop(&app->gui);
            }
            if (gui_tree_node(&app->gui, "Models")) {
                gui_bullet_text(&app->gui, "Characters");
                gui_bullet_text(&app->gui, "Props");
                gui_tree_pop(&app->gui);
            }
            if (gui_tree_node(&app->gui, "Scripts")) {
                gui_bullet_text(&app->gui, "Player");
                gui_bullet_text(&app->gui, "AI");
                gui_tree_pop(&app->gui);
            }
            gui_tree_pop(&app->gui);
        }
        gui_end_panel(&app->gui);
        
        // Right panel - asset grid
        gui_begin_panel(&app->gui, "AssetGrid");
        
        // Simulate asset thumbnails
        v2 thumbnail_size = {80, 80};
        i32 columns = 6;
        
        gui_begin_grid(&app->gui, columns, 10.0f);
        
        const char* assets[] = {
            "Texture_01", "Texture_02", "Model_01", "Material_01",
            "Script_01", "Prefab_01", "Sound_01", "Animation_01",
            "Shader_01", "Font_01", "Scene_01", "Config_01"
        };
        
        for (i32 i = 0; i < 12; ++i) {
            // Thumbnail background
            v2 pos = gui_get_cursor_pos(&app->gui);
            gui_draw_rect_filled(&app->gui, pos, v2_add(pos, thumbnail_size),
                               (color32){60, 60, 70, 255}, 4.0f);
            
            // Thumbnail icon (placeholder)
            v2 icon_center = v2_add(pos, v2_scale(thumbnail_size, 0.5f));
            gui_draw_circle_filled(&app->gui, icon_center, 20.0f,
                                 (color32){100, 120, 140, 255}, 16);
            
            // Asset name
            gui_set_cursor_pos(&app->gui, v2_add(pos, (v2){0, thumbnail_size.y + 5}));
            gui_text(&app->gui, "%s", assets[i]);
            
            // Advance grid cursor
            gui_advance_cursor(&app->gui, (v2){thumbnail_size.x, thumbnail_size.y + 25});
        }
        
        gui_end_grid(&app->gui);
        gui_end_panel(&app->gui);
        
        gui_end_layout(&app->gui);
        
        dock_end_window(&app->dock);
    }
}

void render_console_window(editor_app* app)
{
    if (dock_begin_window(&app->dock, "Console", &app->show_console)) {
        // Console toolbar
        if (gui_button(&app->gui, "Clear")) {
            // TODO: Clear console
        }
        gui_same_line(&app->gui, 0);
        
        static bool show_info = true;
        static bool show_warnings = true;
        static bool show_errors = true;
        
        gui_checkbox(&app->gui, "Info", &show_info);
        gui_same_line(&app->gui, 0);
        gui_checkbox(&app->gui, "Warnings", &show_warnings);
        gui_same_line(&app->gui, 0);
        gui_checkbox(&app->gui, "Errors", &show_errors);
        
        gui_separator(&app->gui);
        
        // Console output
        v2 console_size = gui_get_content_region_avail(&app->gui);
        
        // Simulate console messages
        if (show_info) {
            gui_text_colored(&app->gui, (color32){200, 200, 200, 255}, 
                           "[12:34:56] Engine initialized successfully");
        }
        if (show_warnings) {
            gui_text_colored(&app->gui, (color32){255, 200, 100, 255},
                           "[12:34:57] Warning: Texture 'grass.png' not found, using default");
        }
        if (show_info) {
            gui_text_colored(&app->gui, (color32){200, 200, 200, 255},
                           "[12:34:58] Level 'demo_level' loaded (142 objects)");
        }
        if (show_errors) {
            gui_text_colored(&app->gui, (color32){255, 100, 100, 255},
                           "[12:35:01] Error: Failed to compile shader 'uber_shader.glsl'");
        }
        if (show_info) {
            gui_text_colored(&app->gui, (color32){200, 200, 200, 255},
                           "[12:35:02] Physics system running at 60Hz");
        }
        
        dock_end_window(&app->dock);
    }
}

void render_profiler_window(editor_app* app)
{
    if (dock_begin_window(&app->dock, "Profiler", &app->show_profiler)) {
        gui_text(&app->gui, "Performance Profiler");
        gui_separator(&app->gui);
        
        // CPU Timeline
        gui_text(&app->gui, "CPU Timeline");
        v2 timeline_size = {gui_get_content_region_avail(&app->gui).x, 100};
        v2 timeline_pos = gui_get_cursor_pos(&app->gui);
        
        gui_draw_rect_filled(&app->gui, timeline_pos,
                           v2_add(timeline_pos, timeline_size),
                           (color32){30, 30, 35, 255}, 2.0f);
        
        // Draw some fake timeline bars
        f32 bar_heights[] = {0.3f, 0.7f, 0.5f, 0.8f, 0.4f, 0.6f, 0.9f, 0.3f};
        f32 bar_width = timeline_size.x / 8;
        
        for (i32 i = 0; i < 8; ++i) {
            v2 bar_pos = {timeline_pos.x + i * bar_width, 
                         timeline_pos.y + timeline_size.y * (1.0f - bar_heights[i])};
            v2 bar_size = {bar_width - 2, timeline_size.y * bar_heights[i]};
            
            color32 bar_color = bar_heights[i] > 0.7f ? 
                              (color32){255, 100, 100, 255} :
                              (color32){100, 200, 100, 255};
            
            gui_draw_rect_filled(&app->gui, bar_pos,
                               v2_add(bar_pos, bar_size),
                               bar_color, 0.0f);
        }
        
        gui_set_cursor_pos(&app->gui, v2_add(timeline_pos, (v2){0, timeline_size.y + 10}));
        
        // Stats
        gui_text(&app->gui, "Frame Time: %.2f ms", app->delta_time * 1000.0f);
        gui_text(&app->gui, "FPS: %.1f", app->fps);
        gui_text(&app->gui, "Draw Calls: %d", app->dock.stats.draw_calls);
        gui_text(&app->gui, "Triangles: 28.5k");
        gui_text(&app->gui, "Memory: 142 MB");
        
        dock_end_window(&app->dock);
    }
}

void render_node_editor_window(editor_app* app)
{
    if (dock_begin_window(&app->dock, "Node Editor", &app->show_node_editor)) {
        gui_text(&app->gui, "Visual Scripting");
        gui_separator(&app->gui);
        
        // Node editor placeholder
        v2 editor_size = gui_get_content_region_avail(&app->gui);
        v2 editor_pos = gui_get_cursor_pos(&app->gui);
        
        // Background
        gui_draw_rect_filled(&app->gui, editor_pos,
                           v2_add(editor_pos, editor_size),
                           (color32){25, 25, 30, 255}, 0.0f);
        
        // Grid
        f32 grid_size = 20.0f;
        color32 grid_color = {35, 35, 40, 255};
        
        for (f32 x = 0; x < editor_size.x; x += grid_size) {
            gui_draw_line(&app->gui,
                        v2_add(editor_pos, (v2){x, 0}),
                        v2_add(editor_pos, (v2){x, editor_size.y}),
                        grid_color, 1.0f);
        }
        
        for (f32 y = 0; y < editor_size.y; y += grid_size) {
            gui_draw_line(&app->gui,
                        v2_add(editor_pos, (v2){0, y}),
                        v2_add(editor_pos, (v2){editor_size.x, y}),
                        grid_color, 1.0f);
        }
        
        // Draw some example nodes
        v2 node1_pos = v2_add(editor_pos, (v2){100, 100});
        v2 node1_size = {150, 100};
        gui_draw_rect_filled(&app->gui, node1_pos,
                           v2_add(node1_pos, node1_size),
                           (color32){60, 60, 70, 255}, 4.0f);
        gui_draw_text(&app->gui, v2_add(node1_pos, (v2){10, 10}),
                    (color32){200, 200, 200, 255}, "Get Player", NULL);
        
        v2 node2_pos = v2_add(editor_pos, (v2){350, 120});
        v2 node2_size = {150, 100};
        gui_draw_rect_filled(&app->gui, node2_pos,
                           v2_add(node2_pos, node2_size),
                           (color32){70, 60, 60, 255}, 4.0f);
        gui_draw_text(&app->gui, v2_add(node2_pos, (v2){10, 10}),
                    (color32){200, 200, 200, 255}, "Set Velocity", NULL);
        
        // Draw connection
        v2 start = v2_add(node1_pos, (v2){node1_size.x, node1_size.y * 0.5f});
        v2 end = v2_add(node2_pos, (v2){0, node2_size.y * 0.5f});
        
        // Bezier curve connection
        v2 control1 = v2_add(start, (v2){50, 0});
        v2 control2 = v2_add(end, (v2){-50, 0});
        
        // Simple line for now (bezier would need more implementation)
        gui_draw_line(&app->gui, start, end, (color32){100, 150, 200, 255}, 2.0f);
        
        dock_end_window(&app->dock);
    }
}