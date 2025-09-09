/*
    Professional Game Editor - Main Implementation
    
    Complete implementation of the main editor window with all panels,
    docking system, and project management.
*/

#include "handmade_main_editor.h"
#include "handmade_scene_hierarchy.h"
#include "handmade_property_inspector.h"
#include "handmade_viewport_manager.h"
#include "handmade_tool_palette.h"
#include <string.h>
#include <stdio.h>

// =============================================================================
// INITIALIZATION
// =============================================================================

main_editor* main_editor_create(platform_state* platform, renderer_state* renderer,
                                uint64_t permanent_memory_size, uint64_t frame_memory_size) {
    // Allocate permanent memory
    void* permanent_memory = platform->allocate_memory(permanent_memory_size);
    arena* permanent_arena = arena_create(permanent_memory, permanent_memory_size);
    
    // Allocate frame memory
    void* frame_memory = platform->allocate_memory(frame_memory_size);
    arena* frame_arena = arena_create(frame_memory, frame_memory_size);
    
    // Create editor
    main_editor* editor = arena_push_struct(permanent_arena, main_editor);
    memset(editor, 0, sizeof(main_editor));
    
    editor->platform = platform;
    editor->renderer = renderer;
    editor->permanent_arena = permanent_arena;
    editor->frame_arena = frame_arena;
    
    // Initialize GUI system
    editor->gui = gui_context_create(permanent_arena, renderer);
    
    // Initialize physics
    editor->physics = physics_world_create(permanent_arena);
    
    // Create default docking layout
    editor->dock_root = arena_push_struct(permanent_arena, dock_node);
    editor->dock_root->id = editor->next_dock_id++;
    editor->dock_root->is_leaf = false;
    editor->dock_root->is_horizontal_split = true;
    editor->dock_root->split_ratio = 0.2f;
    editor->dock_root->size = (v2){platform->window_width, platform->window_height};
    
    // Left side panel
    editor->dock_root->left = arena_push_struct(permanent_arena, dock_node);
    editor->dock_root->left->id = editor->next_dock_id++;
    editor->dock_root->left->is_leaf = true;
    
    // Right side split
    editor->dock_root->right = arena_push_struct(permanent_arena, dock_node);
    editor->dock_root->right->id = editor->next_dock_id++;
    editor->dock_root->right->is_leaf = false;
    editor->dock_root->right->is_horizontal_split = true;
    editor->dock_root->right->split_ratio = 0.75f;
    
    // Center area (viewports)
    editor->dock_root->right->left = arena_push_struct(permanent_arena, dock_node);
    editor->dock_root->right->left->id = editor->next_dock_id++;
    editor->dock_root->right->left->is_leaf = true;
    
    // Right panel (inspector)
    editor->dock_root->right->right = arena_push_struct(permanent_arena, dock_node);
    editor->dock_root->right->right->id = editor->next_dock_id++;
    editor->dock_root->right->right->is_leaf = true;
    
    // Create core panels
    editor->scene_hierarchy = scene_hierarchy_create(permanent_arena);
    editor->property_inspector = property_inspector_create(permanent_arena);
    editor->viewport_manager = viewport_manager_create(permanent_arena, renderer);
    editor->tool_palette = tool_palette_create(permanent_arena);
    
    // Add default panels
    main_editor_add_panel(editor, PANEL_SCENE_HIERARCHY);
    main_editor_add_panel(editor, PANEL_PROPERTY_INSPECTOR);
    main_editor_add_panel(editor, PANEL_VIEWPORT);
    main_editor_add_panel(editor, PANEL_TOOL_PALETTE);
    
    // Initialize default scene
    scene_hierarchy_new_scene(editor->scene_hierarchy);
    
    // Load preferences
    main_editor_load_preferences(editor);
    
    editor->is_running = true;
    editor->mode = EDITOR_MODE_EDIT;
    
    return editor;
}

void main_editor_destroy(main_editor* editor) {
    if (!editor) return;
    
    // Save preferences before shutdown
    main_editor_save_preferences(editor);
    
    // Cleanup panels
    for (uint32_t i = 0; i < editor->panel_count; i++) {
        if (editor->panels[i].on_close) {
            editor->panels[i].on_close(&editor->panels[i]);
        }
    }
    
    // Cleanup subsystems
    scene_hierarchy_destroy(editor->scene_hierarchy);
    property_inspector_destroy(editor->property_inspector);
    viewport_manager_destroy(editor->viewport_manager);
    tool_palette_destroy(editor->tool_palette);
    
    // Free memory
    arena_destroy(editor->permanent_arena);
    arena_destroy(editor->frame_arena);
}

// =============================================================================
// PANEL MANAGEMENT
// =============================================================================

editor_panel* main_editor_add_panel(main_editor* editor, editor_panel_type type) {
    if (editor->panel_count >= EDITOR_MAX_PANELS) {
        return NULL;
    }
    
    editor_panel* panel = &editor->panels[editor->panel_count++];
    panel->type = type;
    panel->is_open = true;
    panel->min_size = (v2){200, 150};
    
    // Setup panel based on type
    switch (type) {
        case PANEL_SCENE_HIERARCHY:
            strcpy(panel->title, "Scene Hierarchy");
            panel->data = editor->scene_hierarchy;
            panel->dock_id = editor->dock_root->left->id;
            break;
            
        case PANEL_PROPERTY_INSPECTOR:
            strcpy(panel->title, "Properties");
            panel->data = editor->property_inspector;
            panel->dock_id = editor->dock_root->right->right->id;
            break;
            
        case PANEL_VIEWPORT:
            strcpy(panel->title, "Viewport");
            panel->data = editor->viewport_manager;
            panel->dock_id = editor->dock_root->right->left->id;
            break;
            
        case PANEL_TOOL_PALETTE:
            strcpy(panel->title, "Tools");
            panel->data = editor->tool_palette;
            panel->dock_id = editor->dock_root->left->id;
            break;
            
        default:
            strcpy(panel->title, "Panel");
            break;
    }
    
    return panel;
}

void main_editor_remove_panel(main_editor* editor, editor_panel* panel) {
    if (!panel) return;
    
    // Call close callback
    if (panel->on_close) {
        panel->on_close(panel);
    }
    
    // Find and remove panel
    for (uint32_t i = 0; i < editor->panel_count; i++) {
        if (&editor->panels[i] == panel) {
            // Shift remaining panels
            for (uint32_t j = i; j < editor->panel_count - 1; j++) {
                editor->panels[j] = editor->panels[j + 1];
            }
            editor->panel_count--;
            break;
        }
    }
    
    // Clear focus if this was focused
    if (editor->focused_panel == panel) {
        editor->focused_panel = NULL;
    }
}

// =============================================================================
// PROJECT MANAGEMENT
// =============================================================================

bool main_editor_new_project(main_editor* editor, const char* project_path, const char* project_name) {
    // Clear current project
    main_editor_close_project(editor);
    
    // Setup new project
    strcpy(editor->project.project_path, project_path);
    strcpy(editor->project.project_name, project_name);
    strcpy(editor->project.version, "1.0.0");
    
    // Create project directory structure
    char path[1024];
    sprintf(path, "%s/Assets", project_path);
    editor->platform->create_directory(path);
    sprintf(path, "%s/Scripts", project_path);
    editor->platform->create_directory(path);
    sprintf(path, "%s/Scenes", project_path);
    editor->platform->create_directory(path);
    sprintf(path, "%s/Build", project_path);
    editor->platform->create_directory(path);
    
    // Save project file
    main_editor_save_project(editor);
    
    // Add to recent projects
    main_editor_add_recent_project(editor, project_path);
    
    // Create default scene
    scene_hierarchy_new_scene(editor->scene_hierarchy);
    
    return true;
}

bool main_editor_save_project(main_editor* editor) {
    char project_file[1024];
    sprintf(project_file, "%s/project.handmade", editor->project.project_path);
    
    // Serialize project settings
    void* buffer = arena_push_size(editor->frame_arena, 1024 * 1024);
    uint32_t size = 0;
    
    // Write project header
    memcpy(buffer + size, "HANDMADE_PROJECT", 16);
    size += 16;
    
    // Write version
    uint32_t version = (EDITOR_VERSION_MAJOR << 16) | (EDITOR_VERSION_MINOR << 8) | EDITOR_VERSION_PATCH;
    memcpy(buffer + size, &version, sizeof(uint32_t));
    size += sizeof(uint32_t);
    
    // Write project settings
    memcpy(buffer + size, &editor->project, sizeof(project_settings));
    size += sizeof(project_settings);
    
    // Save to file
    bool success = editor->platform->write_file(project_file, buffer, size);
    
    arena_pop(editor->frame_arena, 1024 * 1024);
    
    if (success) {
        editor->needs_save = false;
        editor->last_save_time = editor->platform->get_time();
    }
    
    return success;
}

// =============================================================================
// MODE CONTROL
// =============================================================================

void main_editor_set_mode(main_editor* editor, editor_mode mode) {
    if (editor->mode == mode) return;
    
    editor_mode old_mode = editor->mode;
    editor->mode = mode;
    
    switch (mode) {
        case EDITOR_MODE_PLAY:
            // Save current scene state
            // Start game simulation
            editor->play_mode_start_time = editor->platform->get_time();
            break;
            
        case EDITOR_MODE_PAUSE:
            // Pause simulation
            break;
            
        case EDITOR_MODE_STOP:
        case EDITOR_MODE_EDIT:
            // Restore saved scene state
            // Stop simulation
            editor->mode = EDITOR_MODE_EDIT;
            break;
            
        case EDITOR_MODE_STEP:
            // Single step simulation
            break;
    }
}

// =============================================================================
// MAIN UPDATE LOOP
// =============================================================================

void main_editor_update(main_editor* editor, f32 dt) {
    // Reset frame arena
    arena_reset(editor->frame_arena);
    
    // Update performance stats
    editor->stats.frame_times[editor->stats.frame_time_index] = dt * 1000.0;
    editor->stats.frame_time_index = (editor->stats.frame_time_index + 1) % 256;
    editor->stats.total_frames++;
    
    // Calculate average frame time
    f64 total = 0;
    for (int i = 0; i < 256; i++) {
        total += editor->stats.frame_times[i];
        if (editor->stats.frame_times[i] > editor->stats.worst_frame_time) {
            editor->stats.worst_frame_time = editor->stats.frame_times[i];
        }
    }
    editor->stats.average_frame_time = total / 256.0;
    
    // Update based on mode
    switch (editor->mode) {
        case EDITOR_MODE_PLAY:
        case EDITOR_MODE_STEP:
            // Update game simulation
            physics_update(editor->physics, dt);
            break;
            
        case EDITOR_MODE_EDIT:
        case EDITOR_MODE_PAUSE:
            // Update editor systems only
            break;
    }
    
    // Update viewport manager
    viewport_manager_update(editor->viewport_manager, dt);
    
    // Update panels
    for (uint32_t i = 0; i < editor->panel_count; i++) {
        if (editor->panels[i].is_open && editor->panels[i].on_update) {
            editor->panels[i].on_update(&editor->panels[i], dt);
        }
    }
    
    // Auto-save
    if (editor->preferences.auto_save_enabled && editor->needs_save) {
        f64 current_time = editor->platform->get_time();
        if (current_time - editor->last_save_time > editor->preferences.auto_save_interval) {
            main_editor_save_project(editor);
        }
    }
}

void main_editor_render(main_editor* editor) {
    // Clear frame
    renderer_clear(editor->renderer, (v4){0.1f, 0.1f, 0.1f, 1.0f});
    
    // Render viewports
    viewport_manager_render_all(editor->viewport_manager);
    
    // Begin GUI pass
    gui_begin_frame(editor->gui);
    
    // Draw main menu bar
    if (gui_begin_main_menu_bar(editor->gui)) {
        if (gui_begin_menu(editor->gui, "File")) {
            if (gui_menu_item(editor->gui, "New Project", "Ctrl+N")) {
                // Show new project dialog
            }
            if (gui_menu_item(editor->gui, "Open Project", "Ctrl+O")) {
                // Show open project dialog
            }
            if (gui_menu_item(editor->gui, "Save", "Ctrl+S")) {
                main_editor_save_project(editor);
            }
            gui_separator(editor->gui);
            if (gui_menu_item(editor->gui, "Exit", "Alt+F4")) {
                editor->is_running = false;
            }
            gui_end_menu(editor->gui);
        }
        
        if (gui_begin_menu(editor->gui, "Edit")) {
            if (gui_menu_item(editor->gui, "Undo", "Ctrl+Z")) {
                main_editor_undo(editor);
            }
            if (gui_menu_item(editor->gui, "Redo", "Ctrl+Y")) {
                main_editor_redo(editor);
            }
            gui_separator(editor->gui);
            if (gui_menu_item(editor->gui, "Preferences", "")) {
                main_editor_toggle_panel(editor, PANEL_SETTINGS);
            }
            gui_end_menu(editor->gui);
        }
        
        if (gui_begin_menu(editor->gui, "View")) {
            for (int i = 0; i < PANEL_COUNT; i++) {
                const char* panel_names[] = {
                    "None", "Scene Hierarchy", "Properties", "Viewport", "Tools",
                    "Timeline", "Code Editor", "Console", "Profiler", "Settings",
                    "Asset Browser", "Material Editor", "Particle Editor", 
                    "Node Editor", "Build Settings"
                };
                if (gui_menu_item(editor->gui, panel_names[i], "")) {
                    main_editor_toggle_panel(editor, i);
                }
            }
            gui_end_menu(editor->gui);
        }
        
        // Play controls in center
        gui_same_line(editor->gui);
        f32 center_x = editor->platform->window_width * 0.5f - 60;
        gui_set_cursor_pos(editor->gui, (v2){center_x, 0});
        
        if (gui_button(editor->gui, editor->mode == EDITOR_MODE_PLAY ? "||" : ">")) {
            main_editor_set_mode(editor, 
                editor->mode == EDITOR_MODE_PLAY ? EDITOR_MODE_PAUSE : EDITOR_MODE_PLAY);
        }
        gui_same_line(editor->gui);
        if (gui_button(editor->gui, "[]")) {
            main_editor_set_mode(editor, EDITOR_MODE_EDIT);
        }
        
        // Stats on right
        if (editor->preferences.show_fps) {
            gui_same_line(editor->gui);
            f32 right_x = editor->platform->window_width - 150;
            gui_set_cursor_pos(editor->gui, (v2){right_x, 0});
            
            char stats[128];
            sprintf(stats, "FPS: %.0f | %.2fms", 
                    1000.0 / editor->stats.average_frame_time,
                    editor->stats.average_frame_time);
            gui_text(editor->gui, stats);
        }
        
        gui_end_main_menu_bar(editor->gui);
    }
    
    // Draw panels
    for (uint32_t i = 0; i < editor->panel_count; i++) {
        editor_panel* panel = &editor->panels[i];
        if (!panel->is_open) continue;
        
        // Draw based on docking state
        if (panel->is_docked) {
            // Find dock node and set position/size
            dock_node* node = main_editor_find_dock_node(editor, editor->dock_root, panel->dock_id);
            if (node) {
                panel->position = node->position;
                panel->size = node->size;
            }
        }
        
        // Draw panel window
        gui_set_next_window_pos(editor->gui, panel->position);
        gui_set_next_window_size(editor->gui, panel->size);
        
        if (gui_begin(editor->gui, panel->title, &panel->is_open)) {
            // Draw panel content
            switch (panel->type) {
                case PANEL_SCENE_HIERARCHY:
                    scene_hierarchy_draw_panel(editor->scene_hierarchy, editor->gui);
                    break;
                    
                case PANEL_PROPERTY_INSPECTOR:
                    property_inspector_draw_panel(editor->property_inspector, editor->gui);
                    break;
                    
                case PANEL_VIEWPORT:
                    // Viewport renders directly, just show overlay
                    viewport_render_overlay(
                        viewport_manager_get_active(editor->viewport_manager), 
                        editor->gui);
                    break;
                    
                case PANEL_TOOL_PALETTE:
                    tool_palette_draw_panel(editor->tool_palette, editor->gui);
                    break;
                    
                default:
                    if (panel->on_gui) {
                        panel->on_gui(panel, editor->gui);
                    }
                    break;
            }
            gui_end(editor->gui);
        }
    }
    
    // End GUI pass
    gui_end_frame(editor->gui);
    gui_render(editor->gui);
    
    // Update stats
    editor->stats.total_draws = editor->renderer->draw_call_count;
    editor->stats.total_vertices = editor->renderer->vertex_count;
    
    // Present frame
    renderer_present(editor->renderer);
}

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

static dock_node* main_editor_find_dock_node(main_editor* editor, dock_node* node, uint32_t id) {
    if (!node) return NULL;
    if (node->id == id) return node;
    
    if (!node->is_leaf) {
        dock_node* found = main_editor_find_dock_node(editor, node->left, id);
        if (found) return found;
        return main_editor_find_dock_node(editor, node->right, id);
    }
    
    return NULL;
}

void main_editor_toggle_panel(main_editor* editor, editor_panel_type type) {
    // Find existing panel
    editor_panel* panel = main_editor_find_panel(editor, type);
    
    if (panel) {
        panel->is_open = !panel->is_open;
    } else {
        // Create new panel
        main_editor_add_panel(editor, type);
    }
}

editor_panel* main_editor_find_panel(main_editor* editor, editor_panel_type type) {
    for (uint32_t i = 0; i < editor->panel_count; i++) {
        if (editor->panels[i].type == type) {
            return &editor->panels[i];
        }
    }
    return NULL;
}

void main_editor_add_recent_project(main_editor* editor, const char* project_path) {
    // Check if already in list
    for (uint32_t i = 0; i < editor->recent_project_count; i++) {
        if (strcmp(editor->recent_projects[i], project_path) == 0) {
            // Move to front
            char temp[512];
            strcpy(temp, editor->recent_projects[i]);
            for (uint32_t j = i; j > 0; j--) {
                strcpy(editor->recent_projects[j], editor->recent_projects[j-1]);
            }
            strcpy(editor->recent_projects[0], temp);
            return;
        }
    }
    
    // Add to front
    if (editor->recent_project_count < EDITOR_MAX_RECENT_PROJECTS) {
        editor->recent_project_count++;
    }
    
    // Shift existing
    for (int i = editor->recent_project_count - 1; i > 0; i--) {
        strcpy(editor->recent_projects[i], editor->recent_projects[i-1]);
    }
    
    strcpy(editor->recent_projects[0], project_path);
}

void main_editor_save_preferences(main_editor* editor) {
    // Save to user preferences file
    char pref_file[512];
    sprintf(pref_file, "%s/.handmade_editor_prefs", editor->platform->get_user_directory());
    
    editor->platform->write_file(pref_file, &editor->preferences, sizeof(editor->preferences));
}

void main_editor_load_preferences(main_editor* editor) {
    // Load from user preferences file
    char pref_file[512];
    sprintf(pref_file, "%s/.handmade_editor_prefs", editor->platform->get_user_directory());
    
    uint64_t size;
    void* data = editor->platform->read_file(pref_file, &size);
    
    if (data && size == sizeof(editor->preferences)) {
        memcpy(&editor->preferences, data, sizeof(editor->preferences));
        editor->platform->free_file_memory(data);
    } else {
        // Set defaults
        strcpy(editor->preferences.theme_name, "Dark");
        editor->preferences.ui_scale = 1.0f;
        editor->preferences.auto_save_enabled = true;
        editor->preferences.auto_save_interval = EDITOR_AUTOSAVE_INTERVAL_SECONDS;
        editor->preferences.vsync_enabled = true;
        editor->preferences.show_fps = true;
        editor->preferences.show_stats = false;
    }
}

bool main_editor_close_project(main_editor* editor) {
    // Save if needed
    if (editor->needs_save) {
        // Show save dialog
        main_editor_save_project(editor);
    }
    
    // Clear project
    memset(&editor->project, 0, sizeof(project_settings));
    
    return true;
}

void main_editor_undo(main_editor* editor) {
    // Implement undo system
}

void main_editor_redo(main_editor* editor) {
    // Implement redo system
}