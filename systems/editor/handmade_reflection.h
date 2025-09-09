// handmade_reflection.h - Zero-dependency runtime type introspection
// PERFORMANCE: Compile-time metadata generation, zero allocations at runtime
// TARGET: <0.05ms property enumeration for 100 properties

#ifndef HANDMADE_REFLECTION_H
#define HANDMADE_REFLECTION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ============================================================================
// TYPE SYSTEM
// ============================================================================

typedef enum {
    TYPE_UNKNOWN = 0,
    TYPE_BOOL,
    TYPE_I8, TYPE_I16, TYPE_I32, TYPE_I64,
    TYPE_U8, TYPE_U16, TYPE_U32, TYPE_U64,
    TYPE_F32, TYPE_F64,
    TYPE_STRING,
    TYPE_STRUCT,
    TYPE_ARRAY,
    TYPE_POINTER,
    TYPE_ENUM,
    TYPE_UNION,
    
    // Math types
    TYPE_VEC2, TYPE_VEC3, TYPE_VEC4,
    TYPE_MAT3, TYPE_MAT4,
    TYPE_QUAT,
    TYPE_COLOR32, TYPE_COLOR_F32,
    
    // Engine types
    TYPE_ENTITY,
    TYPE_COMPONENT,
    TYPE_ASSET_HANDLE,
    
    TYPE_COUNT
} type_kind;

typedef enum {
    PROP_FLAG_NONE = 0,
    PROP_FLAG_READONLY = (1 << 0),
    PROP_FLAG_HIDDEN = (1 << 1),
    PROP_FLAG_ADVANCED = (1 << 2),
    PROP_FLAG_TRANSIENT = (1 << 3),  // Don't serialize
    PROP_FLAG_DIRTY = (1 << 4),      // Changed since last frame
    PROP_FLAG_ANIMATABLE = (1 << 5),
    PROP_FLAG_CONST = (1 << 6),
    PROP_FLAG_STATIC = (1 << 7),
} property_flags;

// UI hints for property editor
typedef enum {
    UI_DEFAULT = 0,
    UI_SLIDER,
    UI_DRAG,
    UI_COLOR_PICKER,
    UI_FILE_PATH,
    UI_ASSET_PICKER,
    UI_DROPDOWN,
    UI_BITMASK,
    UI_CURVE_EDITOR,
    UI_GRADIENT_EDITOR,
} ui_widget_type;

// Forward declarations
typedef struct type_descriptor type_descriptor;
typedef struct property_descriptor property_descriptor;
typedef struct enum_value enum_value;
typedef struct array_descriptor array_descriptor;

// Function pointers for custom behavior
typedef void* (*property_getter)(void* object);
typedef void (*property_setter)(void* object, void* value);
typedef void (*serialize_func)(void* object, void* stream);
typedef void (*deserialize_func)(void* object, void* stream);
typedef bool (*validate_func)(void* value);

// ============================================================================
// DESCRIPTORS
// ============================================================================

// Describes a single enum value
typedef struct enum_value {
    const char* name;
    int64_t value;
} enum_value;

// Describes an array type
typedef struct array_descriptor {
    type_descriptor* element_type;
    size_t element_size;
    size_t count;         // 0 for dynamic arrays
    size_t max_count;     // Maximum allowed elements
    bool is_dynamic;
} array_descriptor;

// UI hints for property editor
typedef struct property_ui_hints {
    ui_widget_type widget;
    
    // Range for numeric types
    union {
        struct { float min, max, step; } f32_range;
        struct { int32_t min, max, step; } i32_range;
    };
    
    // Dropdown options
    const char** dropdown_options;
    uint32_t dropdown_count;
    
    // Asset filters
    const char* asset_filter;  // e.g., "*.png;*.jpg"
    
    // Tooltip
    const char* tooltip;
    
    // Category for grouping
    const char* category;
} property_ui_hints;

// Complete type descriptor
struct type_descriptor {
    const char* name;
    const char* namespace;  // Optional namespace/module
    size_t size;
    size_t alignment;
    type_kind kind;
    uint32_t type_id;      // Unique ID (hash of name)
    
    // Type-specific data
    union {
        // Struct/class
        struct {
            property_descriptor* properties;
            uint32_t property_count;
            type_descriptor* base_type;  // Inheritance
        } struct_data;
        
        // Array
        array_descriptor array_data;
        
        // Pointer
        type_descriptor* pointed_type;
        
        // Enum
        struct {
            enum_value* values;
            uint32_t value_count;
            type_descriptor* underlying_type;
        } enum_data;
    };
    
    // Method table for custom behavior
    struct {
        serialize_func serialize;
        deserialize_func deserialize;
        validate_func validate;
        void (*constructor)(void* memory);
        void (*destructor)(void* memory);
        void (*copy)(void* dst, const void* src);
        bool (*equals)(const void* a, const void* b);
        uint32_t (*hash)(const void* object);
    } methods;
};

// Property descriptor - describes a field/property in a struct
struct property_descriptor {
    const char* name;
    const char* display_name;
    type_descriptor* type;
    size_t offset;              // Byte offset in struct
    property_flags flags;
    
    // UI metadata
    property_ui_hints ui_hints;
    
    // Custom getter/setter (optional)
    property_getter getter;
    property_setter setter;
    
    // Attributes (key-value pairs)
    struct {
        const char* key;
        const char* value;
    } attributes[8];
    uint32_t attribute_count;
};

// ============================================================================
// REFLECTION DATABASE
// ============================================================================

#define MAX_REGISTERED_TYPES 1024
#define MAX_TYPE_NAME_LENGTH 128

typedef struct reflection_database {
    // Type registry - Structure of Arrays for cache efficiency
    type_descriptor* types[MAX_REGISTERED_TYPES];
    uint32_t type_ids[MAX_REGISTERED_TYPES];
    char type_names[MAX_REGISTERED_TYPES][MAX_TYPE_NAME_LENGTH];
    uint32_t type_count;
    
    // Fast lookup via hash table
    struct {
        uint32_t* keys;       // Type IDs
        uint32_t* indices;    // Index into types array
        uint32_t capacity;
        uint32_t count;
    } type_map;
    
    // Memory arena for dynamic allocations
    void* arena_base;
    size_t arena_size;
    size_t arena_used;
} reflection_database;

// Global reflection database
extern reflection_database* g_reflection_db;

// ============================================================================
// REGISTRATION API
// ============================================================================

// Initialize reflection system
void reflection_init(void* arena_memory, size_t arena_size);
void reflection_shutdown(void);

// Type registration
type_descriptor* reflection_register_type(const char* name, size_t size, 
                                         size_t alignment, type_kind kind);
void reflection_register_property(type_descriptor* type, 
                                 const property_descriptor* prop);
void reflection_finalize_type(type_descriptor* type);

// Type lookup
type_descriptor* reflection_get_type(const char* name);
type_descriptor* reflection_get_type_by_id(uint32_t type_id);

// Property access
void* reflection_get_property(void* object, type_descriptor* type, 
                             const char* property_name);
bool reflection_set_property(void* object, type_descriptor* type,
                            const char* property_name, void* value);

// Property iteration
typedef void (*property_visitor)(const property_descriptor* prop, 
                                void* object, void* user_data);
void reflection_iterate_properties(void* object, type_descriptor* type,
                                  property_visitor visitor, void* user_data);

// ============================================================================
// SERIALIZATION
// ============================================================================

typedef struct serialization_context {
    uint8_t* buffer;
    size_t buffer_size;
    size_t cursor;
    bool is_writing;
    
    // Version info
    uint32_t version;
    uint32_t min_version;
    
    // Error handling
    const char* error_msg;
    bool has_error;
} serialization_context;

// Serialize/deserialize objects
bool reflection_serialize(void* object, type_descriptor* type, 
                        serialization_context* ctx);
bool reflection_deserialize(void* object, type_descriptor* type,
                          serialization_context* ctx);

// JSON serialization (human readable)
bool reflection_to_json(void* object, type_descriptor* type,
                       char* json_buffer, size_t buffer_size);
bool reflection_from_json(void* object, type_descriptor* type,
                        const char* json_string);

// ============================================================================
// DIFF & PATCH
// ============================================================================

typedef struct property_diff {
    const char* property_path;  // e.g., "transform.position.x"
    uint8_t old_value[64];      // Store small values inline
    uint8_t new_value[64];
    size_t value_size;
    bool is_pointer;            // If true, values are pointers
} property_diff;

typedef struct object_diff {
    property_diff* diffs;
    uint32_t diff_count;
    uint32_t diff_capacity;
} object_diff;

// Compute differences between objects
object_diff* reflection_diff(void* old_object, void* new_object, 
                            type_descriptor* type);
void reflection_apply_diff(void* object, const object_diff* diff);
void reflection_free_diff(object_diff* diff);

// ============================================================================
// CODE GENERATION MACROS
// ============================================================================

// Use these macros to generate reflection data at compile time

#define REFLECT_STRUCT_BEGIN(StructName) \
    static property_descriptor StructName##_properties[] = {

#define REFLECT_PROPERTY(PropertyName, DisplayName, Flags, ...) \
    { \
        .name = #PropertyName, \
        .display_name = DisplayName, \
        .type = NULL, /* Will be set in init */ \
        .offset = offsetof(StructName, PropertyName), \
        .flags = Flags, \
        .ui_hints = { __VA_ARGS__ }, \
    },

#define REFLECT_STRUCT_END(StructName) \
    }; \
    static type_descriptor StructName##_type = { \
        .name = #StructName, \
        .size = sizeof(StructName), \
        .alignment = alignof(StructName), \
        .kind = TYPE_STRUCT, \
        .struct_data = { \
            .properties = StructName##_properties, \
            .property_count = sizeof(StructName##_properties) / sizeof(property_descriptor), \
        }, \
    };

// Simpler macro for basic types
#define REFLECT_TYPE(TypeName, TypeKind) \
    static type_descriptor TypeName##_type = { \
        .name = #TypeName, \
        .size = sizeof(TypeName), \
        .alignment = alignof(TypeName), \
        .kind = TypeKind, \
    };

// ============================================================================
// PERFORMANCE HELPERS
// ============================================================================

// SIMD-optimized property comparison
bool reflection_compare_properties_simd(void* object_a, void* object_b,
                                       type_descriptor* type);

// Batch property updates
typedef struct property_batch {
    void** objects;
    property_descriptor* property;
    void* new_value;
    uint32_t object_count;
} property_batch;

void reflection_batch_update(const property_batch* batch);

// Property change notifications
typedef void (*property_changed_callback)(void* object, 
                                         const property_descriptor* prop,
                                         void* old_value, void* new_value);

void reflection_watch_property(void* object, const char* property_name,
                              property_changed_callback callback);
void reflection_unwatch_property(void* object, const char* property_name);

// ============================================================================
// UTILITIES
// ============================================================================

// Type name utilities
const char* reflection_type_kind_to_string(type_kind kind);
uint32_t reflection_hash_string(const char* str);

// Size and alignment
size_t reflection_get_array_size(const array_descriptor* array);
size_t reflection_align_offset(size_t offset, size_t alignment);

// Debug helpers
void reflection_print_type(const type_descriptor* type);
void reflection_print_object(void* object, const type_descriptor* type);
void reflection_validate_database(void);

#endif // HANDMADE_REFLECTION_H