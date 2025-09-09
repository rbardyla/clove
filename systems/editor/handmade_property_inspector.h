#ifndef HANDMADE_PROPERTY_INSPECTOR_H
#define HANDMADE_PROPERTY_INSPECTOR_H

/*
    Property Inspector Panel
    
    Dynamic property editing system that can inspect and modify any
    component or object in the scene. Supports custom property drawers,
    undo/redo, and real-time preview.
*/

#include "handmade_main_editor.h"
#include "handmade_scene_hierarchy.h"

#define INSPECTOR_MAX_PROPERTIES 256
#define INSPECTOR_MAX_PROPERTY_PATH 256
#define INSPECTOR_MAX_CUSTOM_EDITORS 64

// Property types
typedef enum property_type {
    PROPERTY_BOOL = 0,
    PROPERTY_INT32,
    PROPERTY_UINT32,
    PROPERTY_FLOAT,
    PROPERTY_DOUBLE,
    PROPERTY_VECTOR2,
    PROPERTY_VECTOR3,
    PROPERTY_VECTOR4,
    PROPERTY_QUATERNION,
    PROPERTY_MATRIX3,
    PROPERTY_MATRIX4,
    PROPERTY_COLOR3,
    PROPERTY_COLOR4,
    PROPERTY_STRING,
    PROPERTY_ENUM,
    PROPERTY_FLAGS,
    PROPERTY_OBJECT_REFERENCE,
    PROPERTY_ASSET_REFERENCE,
    PROPERTY_ARRAY,
    PROPERTY_STRUCT,
    PROPERTY_CURVE,
    PROPERTY_GRADIENT,
    PROPERTY_COUNT
} property_type;

// Property attributes
typedef enum property_attributes {
    PROPERTY_ATTR_READONLY = (1 << 0),
    PROPERTY_ATTR_HIDDEN = (1 << 1),
    PROPERTY_ATTR_ADVANCED = (1 << 2),
    PROPERTY_ATTR_SLIDER = (1 << 3),
    PROPERTY_ATTR_MULTILINE = (1 << 4),
    PROPERTY_ATTR_FILE_PATH = (1 << 5),
    PROPERTY_ATTR_DIRECTORY_PATH = (1 << 6),
    PROPERTY_ATTR_COLOR_PICKER = (1 << 7),
    PROPERTY_ATTR_LAYER_MASK = (1 << 8),
    PROPERTY_ATTR_TAG = (1 << 9),
    PROPERTY_ATTR_ANIMATABLE = (1 << 10),
    PROPERTY_ATTR_HDR = (1 << 11),
    PROPERTY_ATTR_NORMALIZED = (1 << 12)
} property_attributes;

// Property metadata
typedef struct property_metadata {
    char name[64];
    char display_name[64];
    char tooltip[256];
    property_type type;
    uint32_t attributes;
    
    // Type-specific metadata
    union {
        struct {
            int32_t min, max;
        } int_range;
        
        struct {
            f32 min, max;
        } float_range;
        
        struct {
            char** options;
            uint32_t option_count;
        } enum_data;
        
        struct {
            uint32_t max_length;
        } string_data;
        
        struct {
            property_type element_type;
            uint32_t max_elements;
        } array_data;
    };
    
    // Custom drawer
    void (*custom_drawer)(struct property_metadata* meta, void* data, gui_context* gui);
} property_metadata;

// Property definition
typedef struct property_definition {
    property_metadata metadata;
    uint32_t offset;  // Offset in struct
    uint32_t size;    // Size of property
    
    // Getter/Setter (optional)
    void (*getter)(void* object, void* value);
    void (*setter)(void* object, const void* value);
    
    // Change callback
    void (*on_changed)(void* object, const void* old_value, const void* new_value);
} property_definition;

// Type information for reflection
typedef struct type_info {
    uint32_t type_id;
    char type_name[64];
    char display_name[64];
    uint32_t size;
    
    // Properties
    property_definition* properties;
    uint32_t property_count;
    
    // Custom inspector
    void (*custom_inspector)(void* object, struct property_inspector* inspector, gui_context* gui);
    
    // Serialization
    void (*serialize)(void* object, void* buffer, uint32_t* size);
    void (*deserialize)(void* object, const void* buffer, uint32_t size);
} type_info;

// Inspectable object
typedef struct inspectable_object {
    void* data;
    type_info* type;
    char instance_name[128];
    uint32_t instance_id;
    
    // For component inspection
    component_type component_type;
    uint32_t gameobject_id;
} inspectable_object;

// Property change for undo/redo
typedef struct property_change {
    uint32_t object_id;
    uint32_t property_index;
    uint8_t old_value[256];
    uint8_t new_value[256];
    uint32_t value_size;
} property_change;

// Multi-edit state
typedef struct multi_edit_state {
    inspectable_object* objects[256];
    uint32_t object_count;
    bool has_multiple_values[INSPECTOR_MAX_PROPERTIES];
    bool is_editing;
} multi_edit_state;

// Property inspector state
typedef struct property_inspector {
    // Type registry
    type_info* registered_types[256];
    uint32_t registered_type_count;
    
    // Current inspection
    inspectable_object* current_objects[256];
    uint32_t current_object_count;
    
    // Multi-edit
    multi_edit_state multi_edit;
    
    // Display state
    bool show_advanced;
    bool lock_selection;
    bool show_preview;
    bool debug_mode;
    f32 label_width;
    
    // Search/filter
    char search_text[256];
    bool search_active;
    property_type type_filter;
    
    // Expanded states
    bool expanded_categories[64];
    bool expanded_arrays[INSPECTOR_MAX_PROPERTIES];
    
    // Drag state
    bool is_dragging;
    uint32_t drag_property_index;
    f32 drag_start_value;
    f32 drag_current_value;
    
    // Color picker state
    bool color_picker_open;
    uint32_t color_picker_property;
    v4 color_picker_value;
    
    // Curve editor state
    bool curve_editor_open;
    uint32_t curve_editor_property;
    void* curve_editor_data;
    
    // Custom editors
    struct {
        type_info* type;
        void (*editor_func)(void* data, property_inspector* inspector, gui_context* gui);
    } custom_editors[INSPECTOR_MAX_CUSTOM_EDITORS];
    uint32_t custom_editor_count;
    
    // Undo buffer
    property_change* undo_buffer;
    uint32_t undo_buffer_size;
    uint32_t undo_position;
    
    // Callbacks
    void (*on_property_changed)(property_inspector* inspector, uint32_t object_id, 
                               uint32_t property_index, const void* new_value);
    void (*on_selection_changed)(property_inspector* inspector);
    
    // Memory
    arena* arena;
} property_inspector;

// =============================================================================
// TYPE REGISTRY API
// =============================================================================

// Type registration
void type_registry_init(property_inspector* inspector);
void type_registry_register(property_inspector* inspector, type_info* type);
type_info* type_registry_get(property_inspector* inspector, uint32_t type_id);
type_info* type_registry_find(property_inspector* inspector, const char* type_name);

// Built-in type registration
void register_transform_type(property_inspector* inspector);
void register_mesh_renderer_type(property_inspector* inspector);
void register_collider_types(property_inspector* inspector);
void register_light_type(property_inspector* inspector);
void register_camera_type(property_inspector* inspector);
void register_audio_types(property_inspector* inspector);
void register_physics_types(property_inspector* inspector);
void register_ui_types(property_inspector* inspector);

// =============================================================================
// PROPERTY INSPECTOR API
// =============================================================================

// Initialization
property_inspector* property_inspector_create(arena* arena);
void property_inspector_destroy(property_inspector* inspector);

// Inspection
void property_inspector_inspect(property_inspector* inspector, void* object, type_info* type);
void property_inspector_inspect_gameobject(property_inspector* inspector, gameobject* obj);
void property_inspector_inspect_component(property_inspector* inspector, component_base* component);
void property_inspector_inspect_multiple(property_inspector* inspector, 
                                        inspectable_object* objects, uint32_t count);
void property_inspector_clear(property_inspector* inspector);

// Property editing
void property_inspector_set_property(property_inspector* inspector, uint32_t property_index, 
                                    const void* value);
void property_inspector_get_property(property_inspector* inspector, uint32_t property_index, 
                                    void* value);
bool property_inspector_begin_edit(property_inspector* inspector, uint32_t property_index);
void property_inspector_end_edit(property_inspector* inspector);

// Custom editors
void property_inspector_register_custom_editor(property_inspector* inspector, type_info* type,
                                              void (*editor_func)(void*, property_inspector*, gui_context*));

// GUI
void property_inspector_draw_panel(property_inspector* inspector, gui_context* gui);
void property_inspector_draw_property(property_inspector* inspector, gui_context* gui,
                                     property_definition* prop, void* data);
void property_inspector_draw_property_field(property_inspector* inspector, gui_context* gui,
                                           property_metadata* meta, void* data);

// Property drawers
void property_drawer_bool(property_metadata* meta, void* data, gui_context* gui);
void property_drawer_int(property_metadata* meta, void* data, gui_context* gui);
void property_drawer_float(property_metadata* meta, void* data, gui_context* gui);
void property_drawer_vector2(property_metadata* meta, void* data, gui_context* gui);
void property_drawer_vector3(property_metadata* meta, void* data, gui_context* gui);
void property_drawer_vector4(property_metadata* meta, void* data, gui_context* gui);
void property_drawer_color(property_metadata* meta, void* data, gui_context* gui);
void property_drawer_string(property_metadata* meta, void* data, gui_context* gui);
void property_drawer_enum(property_metadata* meta, void* data, gui_context* gui);
void property_drawer_flags(property_metadata* meta, void* data, gui_context* gui);
void property_drawer_object_ref(property_metadata* meta, void* data, gui_context* gui);
void property_drawer_asset_ref(property_metadata* meta, void* data, gui_context* gui);
void property_drawer_array(property_metadata* meta, void* data, gui_context* gui);
void property_drawer_curve(property_metadata* meta, void* data, gui_context* gui);
void property_drawer_gradient(property_metadata* meta, void* data, gui_context* gui);

// Undo/Redo
void property_inspector_push_undo(property_inspector* inspector, property_change* change);
void property_inspector_undo(property_inspector* inspector);
void property_inspector_redo(property_inspector* inspector);
void property_inspector_clear_undo(property_inspector* inspector);

// Utilities
void property_inspector_copy_properties(property_inspector* inspector);
void property_inspector_paste_properties(property_inspector* inspector);
void property_inspector_reset_properties(property_inspector* inspector);
void property_inspector_save_preset(property_inspector* inspector, const char* name);
void property_inspector_load_preset(property_inspector* inspector, const char* name);

// Serialization
void property_inspector_serialize(property_inspector* inspector, void* buffer, uint32_t* size);
void property_inspector_deserialize(property_inspector* inspector, const void* buffer, uint32_t size);

#endif // HANDMADE_PROPERTY_INSPECTOR_H