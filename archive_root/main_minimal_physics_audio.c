/*
    HANDMADE ENGINE WITH RENDERER, GUI, 2D PHYSICS, AND AUDIO
    
    Extends the minimal engine with physics and audio:
    - Integrates seamlessly with existing renderer and GUI
    - 2D physics with collision detection
    - Audio system with collision sounds
    - Interactive demo with sound effects
    - Maintains 60fps performance target
    - Zero external dependencies (except ALSA for audio)
*/

#include "handmade_platform.h"
#include "handmade_renderer.h"
#include "handmade_gui.h"
#include "handmade_physics_2d.h"
#include "systems/audio/handmade_audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <GL/gl.h>

// Application state with renderer, GUI, physics, and audio
typedef struct {
    bool initialized;
    f32 time_accumulator;
    
    // Core systems
    Renderer renderer;
    HandmadeGUI gui;
    Physics2DWorld physics;
    audio_system audio;
    
    // Memory pools - HANDMADE PHILOSOPHY: No malloc/free in hot paths!
    MemoryArena physics_arena;
    MemoryArena audio_arena;
    // Pre-allocated static memory - deterministic, no fragmentation
    u8 physics_memory[MEGABYTES(2)];
    u8 audio_memory[MEGABYTES(8)];
    
    // Demo state
    bool physics_enabled;
    bool physics_paused;
    bool audio_enabled;
    f32 spawn_timer;
    
    // Audio handles
    audio_handle collision_sound_soft;
    audio_handle collision_sound_hard;
    audio_handle collision_sound_metal;
    audio_handle background_music;
    
    // Audio settings
    f32 master_volume;
    f32 effects_volume;
    
    // UI panels
    bool show_renderer_panel;
    bool show_physics_panel;
    bool show_audio_panel;
    bool show_stats_panel;
    
    // Mouse interaction
    RigidBody2D* dragged_body;
    v2 mouse_world_pos;
    
    // Demo objects
    f32 demo_rotation;
    
    // Performance tracking
    f32 fps_timer;
    u32 frame_count;
    f32 current_fps;
    f32 frame_time_ms;
} GameState;

static GameState g_state = {0};

// Generate simple procedural collision sound using arena allocator - NO MALLOC/FREE!
static audio_handle GenerateCollisionSound(MemoryArena* arena, audio_system* audio, f32 frequency, f32 duration, f32 volume) {
    if (!arena || !audio || !g_state.audio_enabled) return AUDIO_INVALID_HANDLE;
    
    u32 sample_rate = AUDIO_SAMPLE_RATE;
    u32 frame_count = (u32)(duration * sample_rate);
    u32 size = frame_count * 2 * sizeof(int16_t); // Stereo
    
    // Arena allocation - handmade philosophy: no malloc/free in hot paths!
    if (duration > 1.0f || frame_count > 44100) {
        return AUDIO_INVALID_HANDLE; // Prevent excessive memory usage
    }
    
    // Allocate from arena - deterministic, fast, no fragmentation
    u64 arena_start = arena->used;
    int16_t* samples = (int16_t*)((u8*)arena->base + arena->used);
    arena->used += size;
    
    if (arena->used > arena->size) {
        arena->used = arena_start; // Rollback allocation
        return AUDIO_INVALID_HANDLE;
    }
    
    // Generate sine wave
    for (u32 i = 0; i < frame_count; i++) {
        f32 t = (f32)i / sample_rate;
        f32 sample = sinf(2.0f * 3.14159f * frequency * t);
        
        // Apply envelope (fade out)
        f32 envelope = 1.0f - (t / duration);
        sample *= envelope * volume * 16384.0f; // Scale to 16-bit
        
        int16_t sample_int = (int16_t)sample;
        samples[i * 2] = sample_int;     // Left channel
        samples[i * 2 + 1] = sample_int; // Right channel
    }
    
    // Load the generated sound into audio system
    audio_handle handle = audio_load_wav_from_memory(audio, samples, size);
    
    // Arena memory stays allocated for lifetime of sounds - no free() needed!
    return handle;
}

// Play collision sound based on impact strength
static void PlayCollisionSound(audio_system* audio, v2 impact_point, f32 impact_strength) {
    if (!audio || !g_state.audio_enabled) return;
    
    // Map impact strength to volume and pitch
    f32 volume = fminf(impact_strength * 0.2f, 1.0f);
    f32 pitch = 0.8f + impact_strength * 0.05f;
    
    // Generate a quick beep sound if we don't have pre-loaded sounds
    if (g_state.collision_sound_soft == AUDIO_INVALID_HANDLE) {
        // Generate procedural sounds using pre-allocated audio arena - NO malloc/free!
        MemoryArena* audio_arena = &g_state.audio_arena;
        g_state.collision_sound_soft = GenerateCollisionSound(audio_arena, audio, 200.0f, 0.1f, 0.5f);
        g_state.collision_sound_hard = GenerateCollisionSound(audio_arena, audio, 400.0f, 0.15f, 0.7f);
        g_state.collision_sound_metal = GenerateCollisionSound(audio_arena, audio, 800.0f, 0.08f, 0.6f);
    }
    
    // Choose sound based on impact strength
    audio_handle sound;
    if (impact_strength < 2.0f) {
        sound = g_state.collision_sound_soft;
    } else if (impact_strength < 5.0f) {
        sound = g_state.collision_sound_hard;
    } else {
        sound = g_state.collision_sound_metal;
    }
    
    // Play the sound with calculated parameters
    if (sound != AUDIO_INVALID_HANDLE) {
        // Calculate stereo pan based on impact position
        f32 pan = fmaxf(-1.0f, fminf(1.0f, impact_point.x * 0.2f));
        
        audio_handle voice = audio_play_sound(audio, sound, volume, pan);
        if (voice != AUDIO_INVALID_HANDLE) {
            audio_set_voice_pitch(audio, voice, pitch);
        }
    }
}

// Convert screen to world coordinates
static v2 ScreenToWorld(v2 screen_pos, Camera2D* camera, u32 viewport_width, u32 viewport_height) {
    v2 ndc;
    ndc.x = (screen_pos.x / (f32)viewport_width) * 2.0f - 1.0f;
    ndc.y = 1.0f - (screen_pos.y / (f32)viewport_height) * 2.0f;
    
    v2 world;
    world.x = (ndc.x / camera->zoom) + camera->position.x;
    world.y = (ndc.y / camera->zoom) + camera->position.y;
    
    return world;
}

// Find body at position
static RigidBody2D* FindBodyAtPosition(Physics2DWorld* physics, v2 world_pos) {
    for (u32 i = 0; i < physics->max_bodies; i++) {
        RigidBody2D* body = &physics->bodies[i];
        if (!body->active) continue;
        
        if (body->shape.type == SHAPE_2D_CIRCLE) {
            v2 delta = v2_sub(world_pos, body->position);
            if (v2_length_sq(delta) <= body->shape.circle.radius * body->shape.circle.radius) {
                return body;
            }
        } else if (body->shape.type == SHAPE_2D_BOX) {
            v2 half = body->shape.box.half_extents;
            v2 local_pos = v2_sub(world_pos, body->position);
            local_pos = v2_rotate(local_pos, -body->rotation);
            
            if (fabsf(local_pos.x) <= half.x && fabsf(local_pos.y) <= half.y) {
                return body;
            }
        }
    }
    return NULL;
}

// Create physics demo scene
static void CreatePhysicsScene(GameState* state) {
    Physics2DWorld* physics = &state->physics;
    Physics2DReset(physics);
    
    // Create boundaries
    RigidBody2D* ground = Physics2DCreateBody(physics, V2(0, -2.8f), BODY_TYPE_STATIC);
    Physics2DSetBoxShape(ground, V2(6.0f, 0.2f));
    ground->color = COLOR(0.2f, 0.2f, 0.2f, 1.0f);
    
    RigidBody2D* left = Physics2DCreateBody(physics, V2(-4.0f, 0), BODY_TYPE_STATIC);
    Physics2DSetBoxShape(left, V2(0.2f, 3.5f));
    left->color = COLOR(0.2f, 0.2f, 0.2f, 1.0f);
    
    RigidBody2D* right = Physics2DCreateBody(physics, V2(4.0f, 0), BODY_TYPE_STATIC);
    Physics2DSetBoxShape(right, V2(0.2f, 3.5f));
    right->color = COLOR(0.2f, 0.2f, 0.2f, 1.0f);
    
    // Create angled platforms
    RigidBody2D* plat1 = Physics2DCreateBody(physics, V2(-2.0f, -0.5f), BODY_TYPE_STATIC);
    Physics2DSetBoxShape(plat1, V2(1.5f, 0.1f));
    plat1->rotation = 0.4f;
    plat1->color = COLOR(0.3f, 0.3f, 0.3f, 1.0f);
    
    RigidBody2D* plat2 = Physics2DCreateBody(physics, V2(2.0f, 0.0f), BODY_TYPE_STATIC);
    Physics2DSetBoxShape(plat2, V2(1.5f, 0.1f));
    plat2->rotation = -0.4f;
    plat2->color = COLOR(0.3f, 0.3f, 0.3f, 1.0f);
    
    // Create center obstacle
    RigidBody2D* center = Physics2DCreateBody(physics, V2(0, 0.5f), BODY_TYPE_STATIC);
    Physics2DSetCircleShape(center, 0.3f);
    center->color = COLOR(0.3f, 0.3f, 0.3f, 1.0f);
    
    // Create initial dynamic bodies
    for (int i = 0; i < 20; i++) {
        f32 x = -2.5f + (rand() % 50) / 10.0f;
        f32 y = 1.0f + (rand() % 15) / 10.0f;
        
        RigidBody2D* body = Physics2DCreateBody(physics, V2(x, y), BODY_TYPE_DYNAMIC);
        
        if (rand() % 2 == 0) {
            // Circle
            f32 radius = 0.1f + (rand() % 20) / 100.0f;
            Physics2DSetCircleShape(body, radius);
            body->color = COLOR(
                0.4f + (rand() % 60) / 100.0f,
                0.4f + (rand() % 60) / 100.0f,
                0.7f + (rand() % 30) / 100.0f,
                1.0f
            );
        } else {
            // Box
            f32 size = 0.1f + (rand() % 20) / 100.0f;
            Physics2DSetBoxShape(body, V2(size, size));
            body->rotation = (rand() % 628) / 100.0f;
            body->color = COLOR(
                0.7f + (rand() % 30) / 100.0f,
                0.4f + (rand() % 60) / 100.0f,
                0.4f + (rand() % 60) / 100.0f,
                1.0f
            );
        }
        
        body->material.restitution = 0.3f + (rand() % 40) / 100.0f;
        body->material.friction = 0.5f + (rand() % 50) / 100.0f;
    }
}

// Spawn new physics body
static void SpawnPhysicsBody(GameState* state, v2 position, bool is_circle) {
    RigidBody2D* body = Physics2DCreateBody(&state->physics, position, BODY_TYPE_DYNAMIC);
    if (!body) return;
    
    if (is_circle) {
        f32 radius = 0.08f + (rand() % 20) / 100.0f;
        Physics2DSetCircleShape(body, radius);
        body->color = COLOR(
            0.3f + (rand() % 70) / 100.0f,
            0.5f + (rand() % 50) / 100.0f,
            0.3f + (rand() % 70) / 100.0f,
            1.0f
        );
    } else {
        f32 size = 0.08f + (rand() % 20) / 100.0f;
        Physics2DSetBoxShape(body, V2(size * 1.2f, size));
        body->rotation = (rand() % 628) / 100.0f;
        body->color = COLOR(
            0.5f + (rand() % 50) / 100.0f,
            0.3f + (rand() % 70) / 100.0f,
            0.3f + (rand() % 70) / 100.0f,
            1.0f
        );
    }
    
    body->material.restitution = 0.2f + (rand() % 60) / 100.0f;
    body->material.friction = 0.4f + (rand() % 60) / 100.0f;
    
    // Give initial velocity
    body->velocity = V2(
        ((rand() % 200) - 100) / 200.0f,
        -((rand() % 100) / 200.0f)
    );
}

// Game initialization
void GameInit(PlatformState* platform) {
    printf("=== HANDMADE ENGINE WITH PHYSICS AND AUDIO ===\n");
    printf("Window size: %dx%d\n", platform->window.width, platform->window.height);
    
    // Initialize random
    srand(time(NULL));
    
    g_state.initialized = true;
    g_state.time_accumulator = 0.0f;
    g_state.demo_rotation = 0.0f;
    g_state.physics_enabled = true;
    g_state.physics_paused = false;
    g_state.audio_enabled = true;
    g_state.master_volume = 0.8f;
    g_state.effects_volume = 1.0f;
    
    // Initialize performance tracking
    g_state.fps_timer = 0.0f;
    g_state.frame_count = 0;
    g_state.current_fps = 0.0f;
    g_state.frame_time_ms = 0.0f;
    
    // Initialize renderer
    if (!RendererInit(&g_state.renderer, platform->window.width, platform->window.height)) {
        printf("Failed to initialize renderer!\n");
        return;
    }
    
    // Initialize GUI
    if (!HandmadeGUI_Init(&g_state.gui, &g_state.renderer)) {
        printf("Failed to initialize GUI!\n");
        return;
    }
    
    // Initialize physics arena - HANDMADE: No heap allocation!
    g_state.physics_arena.base = g_state.physics_memory;
    g_state.physics_arena.size = sizeof(g_state.physics_memory);
    g_state.physics_arena.used = 0;
    
    if (!Physics2DInit(&g_state.physics, &g_state.physics_arena, 300)) {
        printf("Failed to initialize physics!\n");
        g_state.physics_enabled = false;
    } else {
        CreatePhysicsScene(&g_state);
        g_state.physics.debug_draw_enabled = true;
    }
    
    // Initialize audio arena - HANDMADE: No heap allocation!
    g_state.audio_arena.base = g_state.audio_memory;
    g_state.audio_arena.size = sizeof(g_state.audio_memory);
    g_state.audio_arena.used = 0;
    
    if (!audio_init(&g_state.audio, &g_state.audio_arena)) {
        printf("Warning: Failed to initialize audio system\n");
        g_state.audio_enabled = false;
    } else {
        printf("Audio system initialized\n");
        
        // Set default volumes
        audio_set_master_volume(&g_state.audio, g_state.master_volume);
        audio_set_sound_volume(&g_state.audio, g_state.effects_volume);
        
        // Note: We'll generate simple collision sounds procedurally
        // since we don't have WAV files yet
        g_state.collision_sound_soft = AUDIO_INVALID_HANDLE;
        g_state.collision_sound_hard = AUDIO_INVALID_HANDLE;
        g_state.collision_sound_metal = AUDIO_INVALID_HANDLE;
    }
    
    // Initialize UI state
    g_state.show_renderer_panel = true;
    g_state.show_physics_panel = true;
    g_state.show_audio_panel = false;
    g_state.show_stats_panel = true;
    
    // Set initial camera
    g_state.renderer.camera.zoom = 0.35f;
    g_state.renderer.camera.position = V2(0, 0);
    
    printf("All systems initialized\n");
    printf("\nControls:\n");
    printf("  ESC          - Quit\n");
    printf("  SPACE        - Pause/Resume physics\n");
    printf("  R            - Reset physics scene\n");
    printf("  C/B          - Spawn circles/boxes (hold)\n");
    printf("  Mouse Drag   - Move physics bodies\n");
    printf("  WASD         - Move camera\n");
    printf("  QE           - Zoom camera\n");
    printf("  1/2/3/4      - Toggle UI panels (Renderer/Physics/Stats/Audio)\n");
    printf("\nAudio Features:\n");
    printf("  - Collision sounds with impact strength\n");
    printf("  - Procedural sound generation\n");
    printf("  - Volume controls in Audio panel (press 4)\n");
}

void GameUpdate(PlatformState* platform, f32 dt) {
    if (!g_state.initialized) return;
    
    g_state.time_accumulator += dt;
    g_state.demo_rotation += dt * 0.5f;
    
    // Update FPS tracking
    g_state.frame_time_ms = dt * 1000.0f;
    g_state.frame_count++;
    g_state.fps_timer += dt;
    
    // Calculate FPS every 0.5 seconds for smooth display
    if (g_state.fps_timer >= 0.5f) {
        g_state.current_fps = (f32)g_state.frame_count / g_state.fps_timer;
        g_state.frame_count = 0;
        g_state.fps_timer = 0.0f;
    }
    
    // Handle input
    if (platform->input.keys[KEY_ESCAPE].pressed) {
        platform->window.should_close = true;
    }
    
    if (platform->input.keys[KEY_SPACE].pressed && g_state.physics_enabled) {
        g_state.physics_paused = !g_state.physics_paused;
    }
    
    if (platform->input.keys[KEY_R].pressed && g_state.physics_enabled) {
        CreatePhysicsScene(&g_state);
    }
    
    // Toggle panels
    if (platform->input.keys[KEY_1].pressed) {
        g_state.show_renderer_panel = !g_state.show_renderer_panel;
    }
    if (platform->input.keys[KEY_2].pressed) {
        g_state.show_physics_panel = !g_state.show_physics_panel;
    }
    if (platform->input.keys[KEY_3].pressed) {
        g_state.show_stats_panel = !g_state.show_stats_panel;
    }
    if (platform->input.keys[KEY_4].pressed) {
        g_state.show_audio_panel = !g_state.show_audio_panel;
    }
    
    // Camera controls
    Camera2D* camera = &g_state.renderer.camera;
    f32 camera_speed = 3.0f * dt;
    f32 zoom_speed = 2.0f * dt;
    
    if (platform->input.keys[KEY_W].down) {
        camera->position.y += camera_speed / camera->zoom;
    }
    if (platform->input.keys[KEY_S].down) {
        camera->position.y -= camera_speed / camera->zoom;
    }
    if (platform->input.keys[KEY_A].down) {
        camera->position.x -= camera_speed / camera->zoom;
    }
    if (platform->input.keys[KEY_D].down) {
        camera->position.x += camera_speed / camera->zoom;
    }
    if (platform->input.keys[KEY_Q].down) {
        camera->zoom *= (1.0f - zoom_speed);
        if (camera->zoom < 0.1f) camera->zoom = 0.1f;
    }
    if (platform->input.keys[KEY_E].down) {
        camera->zoom *= (1.0f + zoom_speed);
        if (camera->zoom > 2.0f) camera->zoom = 2.0f;
    }
    
    // Physics interaction
    if (g_state.physics_enabled) {
        // Convert mouse to world coordinates
        g_state.mouse_world_pos = ScreenToWorld(
            V2(platform->input.mouse_x, platform->input.mouse_y),
            camera,
            g_state.renderer.viewport_width,
            g_state.renderer.viewport_height
        );
        
        // Mouse dragging
        if (platform->input.mouse[MOUSE_LEFT].pressed) {
            g_state.dragged_body = FindBodyAtPosition(&g_state.physics, g_state.mouse_world_pos);
        }
        
        if (platform->input.mouse[MOUSE_LEFT].down && g_state.dragged_body) {
            if (g_state.dragged_body->type == BODY_TYPE_DYNAMIC) {
                v2 delta = v2_sub(g_state.mouse_world_pos, g_state.dragged_body->position);
                g_state.dragged_body->velocity = v2_scale(delta, 8.0f);
            }
        } else {
            g_state.dragged_body = NULL;
        }
        
        // Spawn bodies
        g_state.spawn_timer -= dt;
        if (g_state.spawn_timer <= 0) {
            if (platform->input.keys[KEY_C].down) {
                SpawnPhysicsBody(&g_state, g_state.mouse_world_pos, true);
                g_state.spawn_timer = 0.1f;
            }
            if (platform->input.keys[KEY_B].down) {
                SpawnPhysicsBody(&g_state, g_state.mouse_world_pos, false);
                g_state.spawn_timer = 0.1f;
            }
        }
        
        // Update physics
        if (!g_state.physics_paused) {
            Physics2DStep(&g_state.physics, dt);
            
            // Check for new collisions and play sounds
            if (g_state.audio_enabled && g_state.physics.contact_count > 0) {
                // Simple collision detection: play sound for new contacts
                for (u32 i = 0; i < g_state.physics.contact_count; i++) {
                    Contact2D* contact = &g_state.physics.contacts[i];
                    
                    // Calculate impact strength based on relative velocity
                    RigidBody2D* bodyA = contact->body_a;
                    RigidBody2D* bodyB = contact->body_b;
                    
                    if (bodyA && bodyB) {
                        v2 relVel = v2_sub(bodyA->velocity, bodyB->velocity);
                        f32 impact = v2_length(relVel);
                        
                        // Only play sound for significant impacts
                        if (impact > 0.5f) {
                            PlayCollisionSound(&g_state.audio, contact->point, impact);
                        }
                    }
                }
            }
        }
        
        // Update audio system
        if (g_state.audio_enabled) {
            audio_update(&g_state.audio, dt);
        }
    }
    
    // Update viewport if resized
    if (platform->window.resized) {
        RendererSetViewport(&g_state.renderer, platform->window.width, platform->window.height);
    }
}

void GameRender(PlatformState* platform) {
    if (!g_state.initialized) return;
    
    // Animated background
    f32 time = g_state.time_accumulator;
    f32 r = 0.05f + 0.03f * sinf(time * 0.5f);
    f32 g = 0.08f + 0.03f * sinf(time * 0.7f);
    f32 b = 0.12f + 0.03f * sinf(time * 0.3f);
    
    glClearColor(r, g, b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Begin rendering
    RendererBeginFrame(&g_state.renderer);
    HandmadeGUI_BeginFrame(&g_state.gui, platform);
    
    // Draw non-physics demo objects (background decoration)
    if (!g_state.physics_enabled) {
        // Draw some animated shapes if physics is disabled
        for (int i = 0; i < 5; i++) {
            f32 offset_x = -2.0f + i * 1.0f;
            f32 offset_y = sinf(time + i * 0.5f) * 0.5f;
            Color color = COLOR(
                0.5f + 0.3f * sinf(time + i * 1.2f),
                0.5f + 0.3f * sinf(time + i * 1.7f + 1.0f),
                0.5f + 0.3f * sinf(time + i * 2.1f + 2.0f),
                0.8f
            );
            RendererDrawCircle(&g_state.renderer, V2(offset_x, offset_y), 0.2f, color, 32);
        }
    }
    
    // Draw physics world
    if (g_state.physics_enabled) {
        Physics2DDebugDraw(&g_state.physics, &g_state.renderer);
        
        // Draw mouse cursor
        RendererDrawCircle(&g_state.renderer, g_state.mouse_world_pos, 0.02f, COLOR_WHITE, 16);
        
        // Draw drag line
        if (g_state.dragged_body) {
            RendererDrawLine(&g_state.renderer, g_state.mouse_world_pos, 
                           g_state.dragged_body->position, 0.02f, COLOR_YELLOW);
        }
    }
    
    // === GUI PANELS ===
    
    // Renderer Panel
    if (g_state.show_renderer_panel) {
        GUIPanel renderer_panel = {
            .position = V2(10.0f, 10.0f),
            .size = V2(200.0f, 120.0f),
            .title = "Renderer",
            .open = &g_state.show_renderer_panel,
            .has_close_button = true,
            .is_draggable = true
        };
        
        if (HandmadeGUI_BeginPanel(&g_state.gui, &renderer_panel)) {
            v2 cursor = HandmadeGUI_GetCursor(&g_state.gui);
            
            char text[128];
            snprintf(text, sizeof(text), "Camera: %.2f, %.2f", 
                    g_state.renderer.camera.position.x, g_state.renderer.camera.position.y);
            HandmadeGUI_Label(&g_state.gui, cursor, text);
            
            cursor.y -= 20.0f;
            snprintf(text, sizeof(text), "Zoom: %.2f", g_state.renderer.camera.zoom);
            HandmadeGUI_Label(&g_state.gui, cursor, text);
            
            cursor.y -= 20.0f;
            snprintf(text, sizeof(text), "Viewport: %dx%d", 
                    g_state.renderer.viewport_width, g_state.renderer.viewport_height);
            HandmadeGUI_Label(&g_state.gui, cursor, text);
            
            HandmadeGUI_EndPanel(&g_state.gui);
        }
    }
    
    // Physics Panel
    if (g_state.show_physics_panel && g_state.physics_enabled) {
        GUIPanel physics_panel = {
            .position = V2(220.0f, 10.0f),
            .size = V2(220.0f, 200.0f),
            .title = "Physics",
            .open = &g_state.show_physics_panel,
            .has_close_button = true,
            .is_draggable = true
        };
        
        if (HandmadeGUI_BeginPanel(&g_state.gui, &physics_panel)) {
            v2 cursor = HandmadeGUI_GetCursor(&g_state.gui);
            
            // Pause button
            if (HandmadeGUI_Button(&g_state.gui, cursor, V2(80.0f, 25.0f), 
                                  g_state.physics_paused ? "Resume" : "Pause")) {
                g_state.physics_paused = !g_state.physics_paused;
            }
            
            cursor.y -= 35.0f;
            
            // Reset button
            if (HandmadeGUI_Button(&g_state.gui, cursor, V2(80.0f, 25.0f), "Reset")) {
                CreatePhysicsScene(&g_state);
            }
            
            cursor.y -= 35.0f;
            
            // Debug options
            HandmadeGUI_Checkbox(&g_state.gui, cursor, "Show AABBs", 
                                &g_state.physics.debug_draw_aabb);
            
            cursor.y -= 25.0f;
            HandmadeGUI_Checkbox(&g_state.gui, cursor, "Show Velocities", 
                                &g_state.physics.debug_draw_velocities);
            
            cursor.y -= 25.0f;
            HandmadeGUI_Checkbox(&g_state.gui, cursor, "Show Contacts", 
                                &g_state.physics.debug_draw_contacts);
            
            HandmadeGUI_EndPanel(&g_state.gui);
        }
    }
    
    // Stats Panel
    if (g_state.show_stats_panel) {
        GUIPanel stats_panel = {
            .position = V2(450.0f, 10.0f),
            .size = V2(180.0f, 160.0f),
            .title = "Statistics",
            .open = &g_state.show_stats_panel,
            .has_close_button = true,
            .is_draggable = true
        };
        
        if (HandmadeGUI_BeginPanel(&g_state.gui, &stats_panel)) {
            v2 cursor = HandmadeGUI_GetCursor(&g_state.gui);
            
            char text[128];
            
            // FPS and performance stats first - most important!
            snprintf(text, sizeof(text), "FPS: %.1f", g_state.current_fps);
            Color fps_color = g_state.current_fps >= 60.0f ? COLOR(0.3f, 0.9f, 0.3f, 1.0f) : COLOR(0.9f, 0.3f, 0.3f, 1.0f);
            HandmadeGUI_Text(&g_state.gui, cursor, text, 1.0f, fps_color);
            
            cursor.y -= 20.0f;
            snprintf(text, sizeof(text), "Frame: %.2fms", g_state.frame_time_ms);
            HandmadeGUI_Label(&g_state.gui, cursor, text);
            
            cursor.y -= 20.0f;
            snprintf(text, sizeof(text), "Time: %.2f", g_state.time_accumulator);
            HandmadeGUI_Label(&g_state.gui, cursor, text);
            
            if (g_state.physics_enabled) {
                cursor.y -= 20.0f;
                snprintf(text, sizeof(text), "Bodies: %u/%u", 
                        g_state.physics.body_count, g_state.physics.max_bodies);
                HandmadeGUI_Label(&g_state.gui, cursor, text);
                
                cursor.y -= 20.0f;
                snprintf(text, sizeof(text), "Contacts: %u", g_state.physics.contact_count);
                HandmadeGUI_Label(&g_state.gui, cursor, text);
                
                cursor.y -= 20.0f;
                snprintf(text, sizeof(text), "Checks: %u", g_state.physics.collision_checks);
                HandmadeGUI_Label(&g_state.gui, cursor, text);
                
                cursor.y -= 20.0f;
                snprintf(text, sizeof(text), "Status: %s", 
                        g_state.physics_paused ? "PAUSED" : "RUNNING");
                HandmadeGUI_Label(&g_state.gui, cursor, text);
            }
            
            HandmadeGUI_EndPanel(&g_state.gui);
        }
    }
    
    // Audio Panel
    if (g_state.show_audio_panel) {
        GUIPanel audio_panel = {
            .position = V2(450.0f, 180.0f),
            .size = V2(200.0f, 180.0f),
            .title = "Audio Settings",
            .open = &g_state.show_audio_panel,
            .has_close_button = true,
            .is_draggable = true
        };
        
        if (HandmadeGUI_BeginPanel(&g_state.gui, &audio_panel)) {
            v2 cursor = HandmadeGUI_GetCursor(&g_state.gui);
            
            char text[128];
            
            // Audio status
            snprintf(text, sizeof(text), "Audio: %s", 
                    g_state.audio_enabled ? "ENABLED" : "DISABLED");
            HandmadeGUI_Label(&g_state.gui, cursor, text);
            
            if (g_state.audio_enabled) {
                // Master volume slider
                cursor.y -= 30.0f;
                HandmadeGUI_Label(&g_state.gui, cursor, "Master Volume:");
                cursor.y -= 25.0f;
                // Simple volume display and adjustment with buttons
                char vol_text[64];
                snprintf(vol_text, sizeof(vol_text), "%.0f%%", g_state.master_volume * 100.0f);
                HandmadeGUI_Label(&g_state.gui, V2(cursor.x + 100.0f, cursor.y), vol_text);
                
                if (HandmadeGUI_Button(&g_state.gui, V2(cursor.x + 140.0f, cursor.y), V2(20.0f, 20.0f), "-")) {
                    g_state.master_volume = fmaxf(0.0f, g_state.master_volume - 0.1f);
                    audio_set_master_volume(&g_state.audio, g_state.master_volume);
                }
                if (HandmadeGUI_Button(&g_state.gui, V2(cursor.x + 165.0f, cursor.y), V2(20.0f, 20.0f), "+")) {
                    g_state.master_volume = fminf(1.0f, g_state.master_volume + 0.1f);
                    audio_set_master_volume(&g_state.audio, g_state.master_volume);
                }
                
                // Effects volume slider
                cursor.y -= 30.0f;
                HandmadeGUI_Label(&g_state.gui, cursor, "Effects Volume:");
                cursor.y -= 25.0f;
                // Simple effects volume display and adjustment with buttons
                snprintf(vol_text, sizeof(vol_text), "%.0f%%", g_state.effects_volume * 100.0f);
                HandmadeGUI_Label(&g_state.gui, V2(cursor.x + 100.0f, cursor.y), vol_text);
                
                if (HandmadeGUI_Button(&g_state.gui, V2(cursor.x + 140.0f, cursor.y), V2(20.0f, 20.0f), "-")) {
                    g_state.effects_volume = fmaxf(0.0f, g_state.effects_volume - 0.1f);
                    audio_set_sound_volume(&g_state.audio, g_state.effects_volume);
                }
                if (HandmadeGUI_Button(&g_state.gui, V2(cursor.x + 165.0f, cursor.y), V2(20.0f, 20.0f), "+")) {
                    g_state.effects_volume = fminf(1.0f, g_state.effects_volume + 0.1f);
                    audio_set_sound_volume(&g_state.audio, g_state.effects_volume);
                }
                
                // Audio stats
                cursor.y -= 30.0f;
                snprintf(text, sizeof(text), "Active: %u voices", 
                        audio_get_active_voices(&g_state.audio));
                HandmadeGUI_Label(&g_state.gui, cursor, text);
                
                cursor.y -= 20.0f;
                snprintf(text, sizeof(text), "CPU: %.1f%%", 
                        audio_get_cpu_usage(&g_state.audio) * 100.0f);
                HandmadeGUI_Label(&g_state.gui, cursor, text);
            }
            
            HandmadeGUI_EndPanel(&g_state.gui);
        }
    }
    
    // HANDMADE PERFORMANCE DISPLAY - Casey would approve!
    v2 fps_pos = V2(10.0f, (f32)g_state.gui.renderer->viewport_height - 30.0f);
    char fps_text[128];
    
    // Performance status: GREEN = handmade target achieved, RED = needs optimization
    bool performance_target_met = g_state.current_fps >= 60.0f && g_state.frame_time_ms <= 16.67f;
    Color perf_color = performance_target_met ? COLOR(0.2f, 1.0f, 0.2f, 1.0f) : COLOR(1.0f, 0.2f, 0.2f, 1.0f);
    
    snprintf(fps_text, sizeof(fps_text), "HANDMADE ENGINE: %.1f FPS (%.3fms) %s", 
             g_state.current_fps, g_state.frame_time_ms,
             performance_target_met ? "✓ FAST" : "✗ SLOW");
    HandmadeGUI_Text(&g_state.gui, fps_pos, fps_text, 1.8f, perf_color);
    
    // Memory usage display (arena-based, predictable)
    fps_pos.y -= 25.0f;
    f32 physics_mb = (f32)g_state.physics_arena.used / (1024.0f * 1024.0f);
    f32 audio_mb = (f32)g_state.audio_arena.used / (1024.0f * 1024.0f);
    snprintf(fps_text, sizeof(fps_text), "MEMORY: Physics %.2fMB | Audio %.2fMB | ZERO MALLOC!", 
             physics_mb, audio_mb);
    HandmadeGUI_Text(&g_state.gui, fps_pos, fps_text, 1.0f, COLOR(0.8f, 0.8f, 1.0f, 1.0f));
    
    // Title and instructions
    v2 overlay_pos = V2(10.0f, (f32)g_state.gui.renderer->viewport_height - 100.0f);
    HandmadeGUI_Text(&g_state.gui, overlay_pos, "Handmade Engine + Physics + Audio", 1.2f, COLOR_WHITE);
    
    overlay_pos.y -= 25.0f;
    HandmadeGUI_Text(&g_state.gui, overlay_pos, "C/B spawn | Mouse drag | 1/2/3/4 panels", 
                    1.0f, COLOR(0.8f, 0.8f, 0.8f, 1.0f));
    
    overlay_pos.y -= 20.0f;
    HandmadeGUI_Text(&g_state.gui, overlay_pos, "WASD move | QE zoom | Space pause", 
                    1.0f, COLOR(0.8f, 0.8f, 0.8f, 1.0f));
    
    // End rendering
    HandmadeGUI_EndFrame(&g_state.gui);
    RendererEndFrame(&g_state.renderer);
}

void GameShutdown(PlatformState* platform) {
    printf("Shutting down engine with physics and audio\n");
    
    // Cleanup
    HandmadeGUI_Shutdown(&g_state.gui);
    RendererShutdown(&g_state.renderer);
    
    if (g_state.physics_enabled) {
        Physics2DShutdown(&g_state.physics);
    }
    
    if (g_state.audio_enabled) {
        audio_shutdown(&g_state.audio);
    }
    
    // HANDMADE: No free() needed - static memory management!
    // Physics and audio memory are static arrays, automatically cleaned up
    
    g_state.initialized = false;
}

void GameOnReload(PlatformState* platform) {
    printf("Game hot-reloaded\n");
}