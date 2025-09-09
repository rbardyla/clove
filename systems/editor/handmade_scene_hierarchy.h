#ifndef HANDMADE_SCENE_HIERARCHY_H
#define HANDMADE_SCENE_HIERARCHY_H

/*
    Scene Hierarchy Panel
    
    Displays and manages the scene graph with game objects, components,
    and their relationships. Supports multi-selection, drag & drop,
    search/filtering, and context menus.
*/

#include "handmade_main_editor.h"

#define SCENE_MAX_OBJECTS 65536
#define SCENE_MAX_DEPTH 32
#define SCENE_MAX_NAME_LENGTH 128
#define SCENE_MAX_SELECTION 256

// GameObject flags
typedef enum gameobject_flags {
    GAMEOBJECT_ACTIVE = (1 << 0),
    GAMEOBJECT_STATIC = (1 << 1),
    GAMEOBJECT_HIDDEN = (1 << 2),
    GAMEOBJECT_LOCKED = (1 << 3),
    GAMEOBJECT_PREFAB = (1 << 4),
    GAMEOBJECT_PREFAB_INSTANCE = (1 << 5),
    GAMEOBJECT_DONT_SAVE = (1 << 6),
    GAMEOBJECT_EDITOR_ONLY = (1 << 7)
} gameobject_flags;

// Component types
typedef enum component_type {
    COMPONENT_TRANSFORM = 0,
    COMPONENT_MESH_RENDERER,
    COMPONENT_COLLIDER,
    COMPONENT_RIGIDBODY,
    COMPONENT_LIGHT,
    COMPONENT_CAMERA,
    COMPONENT_AUDIO_SOURCE,
    COMPONENT_AUDIO_LISTENER,
    COMPONENT_PARTICLE_SYSTEM,
    COMPONENT_ANIMATOR,
    COMPONENT_SCRIPT,
    COMPONENT_UI_CANVAS,
    COMPONENT_UI_TEXT,
    COMPONENT_UI_BUTTON,
    COMPONENT_UI_IMAGE,
    COMPONENT_TERRAIN,
    COMPONENT_NAV_MESH_AGENT,
    COMPONENT_COUNT
} component_type;

// Transform component (every object has one)
typedef struct transform_component {
    v3 position;
    quat rotation;
    v3 scale;
    
    // Hierarchy
    uint32_t parent_id;
    uint32_t first_child_id;
    uint32_t next_sibling_id;
    
    // Cached world transform
    m4x4 local_to_world;
    m4x4 world_to_local;
    bool world_transform_dirty;
} transform_component;

// Base component structure
typedef struct component_base {
    component_type type;
    uint32_t gameobject_id;
    bool enabled;
    void* data;
} component_base;

// GameObject
typedef struct gameobject {
    uint32_t id;
    char name[SCENE_MAX_NAME_LENGTH];
    uint32_t flags;
    uint32_t layer;
    uint32_t tag;
    
    // Components
    transform_component transform;
    component_base* components[16];
    uint32_t component_count;
    
    // Editor metadata
    bool is_expanded_in_hierarchy;
    uint32_t icon_index;
    v4 editor_color;
} gameobject;

// Scene
typedef struct scene {
    char name[128];
    char path[512];
    
    // Objects
    gameobject* objects;
    uint32_t object_count;
    uint32_t object_capacity;
    uint32_t next_object_id;
    
    // Hierarchy root
    uint32_t root_object_id;
    
    // Layers and tags
    char layer_names[32][64];
    char tag_names[128][64];
    uint32_t tag_count;
    
    // Scene settings
    v4 ambient_color;
    v3 gravity;
    f32 time_scale;
} scene;

// Selection
typedef struct selection_state {
    uint32_t selected_ids[SCENE_MAX_SELECTION];
    uint32_t selection_count;
    uint32_t primary_selection;
    uint32_t last_selected;
    
    // Multi-select state
    bool range_select_active;
    uint32_t range_select_start;
    uint32_t range_select_end;
} selection_state;

// Hierarchy filter
typedef struct hierarchy_filter {
    char search_text[256];
    uint32_t type_mask;
    uint32_t layer_mask;
    uint32_t tag_mask;
    bool show_hidden;
    bool show_inactive;
} hierarchy_filter;

// Drag & drop state
typedef struct drag_drop_state {
    bool is_dragging;
    uint32_t* dragged_ids;
    uint32_t dragged_count;
    uint32_t drop_target_id;
    int32_t drop_position; // -1 = before, 0 = on, 1 = after
    bool valid_drop;
} drag_drop_state;

// Scene hierarchy panel
typedef struct scene_hierarchy {
    // Scene
    scene* current_scene;
    scene* scenes[8];
    uint32_t scene_count;
    uint32_t active_scene_index;
    
    // Selection
    selection_state selection;
    
    // Display
    hierarchy_filter filter;
    bool show_components;
    bool show_preview;
    f32 row_height;
    f32 indent_width;
    
    // Interaction
    drag_drop_state drag_drop;
    bool rename_active;
    uint32_t rename_object_id;
    char rename_buffer[SCENE_MAX_NAME_LENGTH];
    
    // Context menu
    bool context_menu_open;
    v2 context_menu_pos;
    uint32_t context_object_id;
    
    // Icons
    uint32_t object_icons[32];
    uint32_t component_icons[COMPONENT_COUNT];
    
    // Callbacks
    void (*on_selection_changed)(scene_hierarchy* hierarchy);
    void (*on_object_created)(scene_hierarchy* hierarchy, uint32_t object_id);
    void (*on_object_deleted)(scene_hierarchy* hierarchy, uint32_t object_id);
    void (*on_object_renamed)(scene_hierarchy* hierarchy, uint32_t object_id);
    void (*on_hierarchy_changed)(scene_hierarchy* hierarchy);
    
    // Memory
    arena* arena;
} scene_hierarchy;

// =============================================================================
// SCENE API
// =============================================================================

// Scene management
scene* scene_create(arena* arena, const char* name);
void scene_destroy(scene* s);
bool scene_save(scene* s, const char* path);
bool scene_load(scene* s, const char* path);
void scene_clear(scene* s);

// GameObject management
uint32_t scene_create_object(scene* s, const char* name);
uint32_t scene_create_child(scene* s, uint32_t parent_id, const char* name);
void scene_destroy_object(scene* s, uint32_t object_id);
gameobject* scene_get_object(scene* s, uint32_t object_id);
gameobject* scene_find_object(scene* s, const char* name);
uint32_t scene_duplicate_object(scene* s, uint32_t object_id);

// Hierarchy manipulation
void scene_set_parent(scene* s, uint32_t child_id, uint32_t parent_id);
void scene_detach_from_parent(scene* s, uint32_t object_id);
uint32_t* scene_get_children(scene* s, uint32_t parent_id, uint32_t* count);
uint32_t scene_get_child_count(scene* s, uint32_t parent_id);
bool scene_is_ancestor(scene* s, uint32_t ancestor_id, uint32_t descendant_id);

// Component management  
component_base* scene_add_component(scene* s, uint32_t object_id, component_type type);
void scene_remove_component(scene* s, uint32_t object_id, component_type type);
component_base* scene_get_component(scene* s, uint32_t object_id, component_type type);
component_base** scene_get_components(scene* s, uint32_t object_id, uint32_t* count);

// Transform operations
void scene_update_transforms(scene* s);
m4x4 scene_get_world_transform(scene* s, uint32_t object_id);
void scene_set_world_position(scene* s, uint32_t object_id, v3 position);
void scene_set_world_rotation(scene* s, uint32_t object_id, quat rotation);
v3 scene_get_world_position(scene* s, uint32_t object_id);
quat scene_get_world_rotation(scene* s, uint32_t object_id);

// =============================================================================
// SCENE HIERARCHY API
// =============================================================================

// Initialization
scene_hierarchy* scene_hierarchy_create(arena* arena);
void scene_hierarchy_destroy(scene_hierarchy* hierarchy);

// Scene management
void scene_hierarchy_set_scene(scene_hierarchy* hierarchy, scene* s);
void scene_hierarchy_new_scene(scene_hierarchy* hierarchy);
void scene_hierarchy_open_scene(scene_hierarchy* hierarchy, const char* path);
void scene_hierarchy_save_scene(scene_hierarchy* hierarchy);

// Selection
void scene_hierarchy_select(scene_hierarchy* hierarchy, uint32_t object_id);
void scene_hierarchy_select_multiple(scene_hierarchy* hierarchy, uint32_t* ids, uint32_t count);
void scene_hierarchy_add_to_selection(scene_hierarchy* hierarchy, uint32_t object_id);
void scene_hierarchy_remove_from_selection(scene_hierarchy* hierarchy, uint32_t object_id);
void scene_hierarchy_clear_selection(scene_hierarchy* hierarchy);
void scene_hierarchy_select_all(scene_hierarchy* hierarchy);
bool scene_hierarchy_is_selected(scene_hierarchy* hierarchy, uint32_t object_id);
uint32_t* scene_hierarchy_get_selection(scene_hierarchy* hierarchy, uint32_t* count);

// Filtering
void scene_hierarchy_set_filter(scene_hierarchy* hierarchy, const char* search_text);
void scene_hierarchy_set_type_filter(scene_hierarchy* hierarchy, uint32_t type_mask);
void scene_hierarchy_set_layer_filter(scene_hierarchy* hierarchy, uint32_t layer_mask);
void scene_hierarchy_clear_filter(scene_hierarchy* hierarchy);

// GUI
void scene_hierarchy_draw_panel(scene_hierarchy* hierarchy, gui_context* gui);
void scene_hierarchy_draw_object_row(scene_hierarchy* hierarchy, gui_context* gui, 
                                     uint32_t object_id, f32 indent);
void scene_hierarchy_draw_context_menu(scene_hierarchy* hierarchy, gui_context* gui);

// Drag & Drop
void scene_hierarchy_begin_drag(scene_hierarchy* hierarchy, uint32_t* object_ids, uint32_t count);
void scene_hierarchy_update_drag(scene_hierarchy* hierarchy, v2 mouse_pos);
void scene_hierarchy_end_drag(scene_hierarchy* hierarchy);
bool scene_hierarchy_accept_drop(scene_hierarchy* hierarchy, uint32_t target_id);

// Operations
void scene_hierarchy_create_object(scene_hierarchy* hierarchy, const char* name);
void scene_hierarchy_duplicate_selected(scene_hierarchy* hierarchy);
void scene_hierarchy_delete_selected(scene_hierarchy* hierarchy);
void scene_hierarchy_rename_object(scene_hierarchy* hierarchy, uint32_t object_id);
void scene_hierarchy_cut(scene_hierarchy* hierarchy);
void scene_hierarchy_copy(scene_hierarchy* hierarchy);
void scene_hierarchy_paste(scene_hierarchy* hierarchy);

// Utilities
void scene_hierarchy_expand_all(scene_hierarchy* hierarchy);
void scene_hierarchy_collapse_all(scene_hierarchy* hierarchy);
void scene_hierarchy_focus_object(scene_hierarchy* hierarchy, uint32_t object_id);
void scene_hierarchy_frame_selected(scene_hierarchy* hierarchy);

#endif // HANDMADE_SCENE_HIERARCHY_H