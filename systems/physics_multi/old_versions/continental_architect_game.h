#ifndef CONTINENTAL_ARCHITECT_GAME_H
#define CONTINENTAL_ARCHITECT_GAME_H

/*
    Continental Architect - Multi-Scale Physics Game
    
    A god-game showcasing the complete MLPDD physics system where players:
    1. Shape continents through tectonic manipulation
    2. Guide river formation and erosion patterns  
    3. Place civilizations that adapt to geological changes
    4. Manage disasters across geological timescales
    5. Optimize for performance across all scales
    
    Demonstrates:
    - Complete multi-scale physics integration
    - Real-time interaction with geological timescales
    - Performance optimization for complex simulations
    - Handmade philosophy: zero dependencies, arena-based, SIMD optimized
*/

// Mock types for standalone compilation
typedef struct arena arena;
typedef void* multi_physics_state;
typedef void* geological_state;
typedef void* fluid_state;
typedef void* structural_state;

// =============================================================================
// GAME STATE
// =============================================================================

typedef enum game_mode {
    MODE_GEOLOGICAL,    // Shape continents (million year timescale)
    MODE_HYDROLOGICAL, // Guide water systems (century timescale) 
    MODE_CIVILIZATIONS, // Manage settlements (decade timescale)
    MODE_DISASTERS      // Handle earthquakes/floods (real-time)
} game_mode;

typedef enum tool_type {
    TOOL_TECTONIC_PUSH,     // Create mountain ranges
    TOOL_TECTONIC_PULL,     // Create rifts and valleys
    TOOL_WATER_SOURCE,      // Add water springs
    TOOL_CIVILIZATION,      // Place settlements
    TOOL_INSPECT,           // View detailed physics data
    TOOL_TIME_CONTROL       // Adjust simulation speed
} tool_type;

typedef struct civilization {
    v3 position;
    f32 population;
    f32 technology_level;
    f32 geological_stability;  // How well adapted to local geology
    f32 water_access;          // Access to fresh water
    f32 resource_access;       // Access to materials
    
    // Survival metrics
    f32 earthquake_resistance;
    f32 flood_resistance;
    f32 drought_resistance;
    
    // History
    f32 age_years;
    f32 peak_population;
    b32 has_survived_disaster;
} civilization;

typedef struct player_input {
    v2 mouse_world_pos;
    b32 left_mouse_down;
    b32 right_mouse_down;
    f32 mouse_wheel_delta;
    
    // Tool selection
    tool_type selected_tool;
    f32 tool_strength;
    f32 tool_radius;
    
    // Time controls
    f32 time_scale_multiplier;
    b32 pause_geological;
    b32 pause_hydrological;
    b32 pause_structural;
} player_input;

typedef struct camera_state {
    v3 position;
    v3 target;
    f32 zoom_level;        // From continental view to local view
    f32 rotation_angle;
    
    // View modes
    b32 show_geological_layers;
    b32 show_water_flow;
    b32 show_stress_patterns;
    b32 show_civilization_status;
    
    // Performance controls
    f32 terrain_detail_level;
    u32 max_particles_visible;
} camera_state;

typedef struct game_state {
    // Core physics simulation
    multi_physics_state* physics;
    
    // Game-specific state
    civilization civilizations[64];
    u32 civilization_count;
    
    // Player interaction
    player_input input;
    camera_state camera;
    game_mode current_mode;
    
    // Scoring and objectives
    f32 total_population;
    f32 civilization_survival_time;
    u32 disasters_survived;
    f32 geological_stability_score;
    
    // Performance tracking
    f64 frame_time_ms;
    f64 physics_time_ms;
    f64 render_time_ms;
    u32 frames_per_second;
    
    // UI state
    b32 show_debug_overlay;
    b32 show_performance_stats;
    b32 show_tutorial;
    f32 ui_scale;
    
    // Memory management
    arena* game_arena;
    arena* temp_arena;
    
    // Statistics
    struct {
        u64 total_geological_years_simulated;
        u64 total_civilizations_created;
        u64 total_disasters_handled;
        f64 total_playtime_seconds;
    } stats;
} game_state;

// =============================================================================
// GAME API
// =============================================================================

// Initialize the game
game_state* continental_architect_init(arena* arena);

// Main game update loop
void continental_architect_update(game_state* game, f32 dt);

// Handle player input
void process_player_input(game_state* game, player_input* input);

// Update camera based on input
void update_camera(camera_state* camera, player_input* input, f32 dt);

// Apply player tools to the physics simulation
void apply_player_tools(game_state* game);

// Update civilizations based on environmental conditions
void update_civilizations(game_state* game, f32 dt);

// Check for and handle disaster events
void handle_disasters(game_state* game, f32 dt);

// Calculate scoring metrics
void update_scoring(game_state* game, f32 dt);

// =============================================================================
// RENDERING AND VISUALIZATION
// =============================================================================

// Render the complete game world
void render_game_world(game_state* game);

// Render terrain based on geological simulation
void render_geological_terrain(geological_state* geo, camera_state* camera);

// Render water systems and flow patterns
void render_hydrological_systems(fluid_state* fluid, camera_state* camera);

// Render civilizations and their status
void render_civilizations(game_state* game);

// Render UI overlays
void render_game_ui(game_state* game);

// Performance visualization
void render_performance_overlay(game_state* game);

// Debug visualizations
void render_debug_overlays(game_state* game);

// =============================================================================
// GAMEPLAY SYSTEMS
// =============================================================================

// Tool implementations
void apply_tectonic_push_tool(geological_state* geo, v2 position, f32 strength, f32 radius);
void apply_tectonic_pull_tool(geological_state* geo, v2 position, f32 strength, f32 radius);
void apply_water_source_tool(fluid_state* fluid, v2 position, f32 flow_rate);
void place_civilization_tool(game_state* game, v2 position);

// Civilization AI
void civilization_adapt_to_environment(civilization* civ, multi_physics_state* physics);
f32 calculate_geological_stability(multi_physics_state* physics, v2 position);
f32 calculate_water_access(multi_physics_state* physics, v2 position);
f32 calculate_resource_access(multi_physics_state* physics, v2 position);

// Disaster detection and response
b32 detect_earthquake_threat(geological_state* geo, v2 position, f32 radius);
b32 detect_flood_threat(fluid_state* fluid, v2 position, f32 radius);
void trigger_earthquake_event(game_state* game, v2 epicenter, f32 magnitude);
void trigger_flood_event(game_state* game, v2 origin, f32 intensity);

// =============================================================================
// PERFORMANCE OPTIMIZATION
// =============================================================================

// Level-of-detail management
void update_simulation_detail_levels(game_state* game);

// Selective physics updates based on camera focus
void optimize_physics_regions(multi_physics_state* physics, camera_state* camera);

// Memory management
void manage_temporary_allocations(game_state* game);

// SIMD optimized batch operations
void batch_update_civilizations_simd(civilization* civs, u32 count, 
                                     multi_physics_state* physics, f32 dt);

#endif // CONTINENTAL_ARCHITECT_GAME_H