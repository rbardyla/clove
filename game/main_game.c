// main_game.c - Main entry point for Crystal Dungeons
// Integrates all engine systems and runs the game

#include "crystal_dungeons.h"
#include "../src/handmade_platform_linux.c"
#include "../systems/renderer/handmade_renderer.c"
#include "../systems/physics/handmade_physics.c"
#include "../systems/audio/handmade_audio.c"
#include "../systems/ai/handmade_neural.c"
#include <stdio.h>
#include <time.h>

// ============================================================================
// SPRITE SYSTEM
// ============================================================================

#define SPRITE_SHEET_WIDTH 512
#define SPRITE_SHEET_HEIGHT 512
#define SPRITE_SIZE 16

typedef struct sprite {
    u32 texture_id;
    rect uv_rect;
    v2 size;
    v2 origin;  // Pivot point
} sprite;

typedef struct sprite_sheet {
    u32 texture_id;
    sprite sprites[256];
    u32 sprite_count;
} sprite_sheet;

typedef struct sprite_renderer {
    sprite_sheet* sheets[16];
    u32 sheet_count;
    
    // Batching
    struct {
        v2 positions[1024];
        rect uvs[1024];
        color32 colors[1024];
        u32 count;
    } batch;
    
    // GPU resources
    u32 vbo;
    u32 vao;
    u32 shader;
} sprite_renderer;

sprite_renderer g_sprite_renderer = {0};

// Initialize sprite renderer
void sprite_renderer_init(sprite_renderer* sr)
{
    // Create shader for sprite rendering
    const char* vertex_shader = 
        "#version 330 core\n"
        "layout(location = 0) in vec2 aPos;\n"
        "layout(location = 1) in vec2 aTexCoord;\n"
        "layout(location = 2) in vec4 aColor;\n"
        "out vec2 TexCoord;\n"
        "out vec4 Color;\n"
        "uniform mat4 projection;\n"
        "void main() {\n"
        "    gl_Position = projection * vec4(aPos, 0.0, 1.0);\n"
        "    TexCoord = aTexCoord;\n"
        "    Color = aColor;\n"
        "}\n";
    
    const char* fragment_shader =
        "#version 330 core\n"
        "in vec2 TexCoord;\n"
        "in vec4 Color;\n"
        "out vec4 FragColor;\n"
        "uniform sampler2D texture1;\n"
        "void main() {\n"
        "    FragColor = texture(texture1, TexCoord) * Color;\n"
        "}\n";
    
    // Compile shaders (using OpenGL)
    // sr->shader = compile_shader(vertex_shader, fragment_shader);
    
    // Create vertex buffer
    // glGenVertexArrays(1, &sr->vao);
    // glGenBuffers(1, &sr->vbo);
}

// Create sprite sheet from texture
sprite_sheet* sprite_sheet_create(const char* texture_path, u32 sprite_width, u32 sprite_height)
{
    sprite_sheet* sheet = (sprite_sheet*)malloc(sizeof(sprite_sheet));
    sheet->sprite_count = 0;
    
    // Load texture
    // sheet->texture_id = texture_load(texture_path);
    
    // Auto-slice into sprites
    u32 cols = SPRITE_SHEET_WIDTH / sprite_width;
    u32 rows = SPRITE_SHEET_HEIGHT / sprite_height;
    
    for (u32 y = 0; y < rows; y++) {
        for (u32 x = 0; x < cols; x++) {
            sprite* s = &sheet->sprites[sheet->sprite_count++];
            s->texture_id = sheet->texture_id;
            s->size = (v2){(f32)sprite_width, (f32)sprite_height};
            s->origin = (v2){sprite_width * 0.5f, sprite_height * 0.5f};
            
            // Calculate UV coordinates
            f32 u0 = (f32)(x * sprite_width) / SPRITE_SHEET_WIDTH;
            f32 v0 = (f32)(y * sprite_height) / SPRITE_SHEET_HEIGHT;
            f32 u1 = (f32)((x + 1) * sprite_width) / SPRITE_SHEET_WIDTH;
            f32 v1 = (f32)((y + 1) * sprite_height) / SPRITE_SHEET_HEIGHT;
            
            s->uv_rect = (rect){{u0, v0}, {u1, v1}};
        }
    }
    
    return sheet;
}

// Draw sprite
void sprite_draw(sprite* s, v2 position, f32 rotation, v2 scale, color32 tint)
{
    if (g_sprite_renderer.batch.count >= 1024) {
        // Flush batch
        sprite_renderer_flush(&g_sprite_renderer);
    }
    
    // Add to batch
    u32 i = g_sprite_renderer.batch.count++;
    g_sprite_renderer.batch.positions[i] = position;
    g_sprite_renderer.batch.uvs[i] = s->uv_rect;
    g_sprite_renderer.batch.colors[i] = tint;
}

// Flush sprite batch
void sprite_renderer_flush(sprite_renderer* sr)
{
    if (sr->batch.count == 0) return;
    
    // Upload batch data to GPU and render
    // ...
    
    sr->batch.count = 0;
}

// ============================================================================
// SPRITE DEFINITIONS
// ============================================================================

typedef enum {
    // Player sprites
    SPRITE_PLAYER_DOWN_1 = 0,
    SPRITE_PLAYER_DOWN_2,
    SPRITE_PLAYER_UP_1,
    SPRITE_PLAYER_UP_2,
    SPRITE_PLAYER_LEFT_1,
    SPRITE_PLAYER_LEFT_2,
    SPRITE_PLAYER_RIGHT_1,
    SPRITE_PLAYER_RIGHT_2,
    
    // Enemy sprites
    SPRITE_SLIME_1 = 16,
    SPRITE_SLIME_2,
    SPRITE_SKELETON_1,
    SPRITE_SKELETON_2,
    SPRITE_BAT_1,
    SPRITE_BAT_2,
    
    // Item sprites
    SPRITE_SWORD = 32,
    SPRITE_SHIELD,
    SPRITE_BOW,
    SPRITE_ARROW,
    SPRITE_BOMB,
    SPRITE_KEY,
    SPRITE_HEART,
    SPRITE_RUPEE,
    
    // Tile sprites
    SPRITE_FLOOR = 48,
    SPRITE_WALL,
    SPRITE_DOOR_CLOSED,
    SPRITE_DOOR_OPEN,
    SPRITE_STAIRS,
    SPRITE_CHEST_CLOSED,
    SPRITE_CHEST_OPEN,
    SPRITE_SWITCH_OFF,
    SPRITE_SWITCH_ON,
    
} sprite_id;

sprite_sheet* g_main_sprite_sheet = NULL;

// Get sprite for entity
sprite* get_entity_sprite(entity* e)
{
    if (!g_main_sprite_sheet) return NULL;
    
    sprite_id id = SPRITE_PLAYER_DOWN_1;
    
    switch (e->type) {
        case ENTITY_PLAYER:
            // Animate based on direction and movement
            switch (e->facing) {
                case DIR_NORTH:
                    id = SPRITE_PLAYER_UP_1 + (e->animation_frame % 2);
                    break;
                case DIR_SOUTH:
                    id = SPRITE_PLAYER_DOWN_1 + (e->animation_frame % 2);
                    break;
                case DIR_WEST:
                    id = SPRITE_PLAYER_LEFT_1 + (e->animation_frame % 2);
                    break;
                case DIR_EAST:
                    id = SPRITE_PLAYER_RIGHT_1 + (e->animation_frame % 2);
                    break;
            }
            break;
            
        case ENTITY_SLIME:
            id = SPRITE_SLIME_1 + (e->animation_frame % 2);
            break;
            
        case ENTITY_SKELETON:
            id = SPRITE_SKELETON_1 + (e->animation_frame % 2);
            break;
            
        case ENTITY_HEART:
            id = SPRITE_HEART;
            break;
            
        case ENTITY_RUPEE:
            id = SPRITE_RUPEE;
            break;
            
        case ENTITY_KEY:
            id = SPRITE_KEY;
            break;
    }
    
    return &g_main_sprite_sheet->sprites[id];
}

// ============================================================================
// ENHANCED GAME RENDERING
// ============================================================================

void game_render_enhanced(game_state* game, renderer* r)
{
    // Clear screen with dungeon color
    renderer_clear(r, (color32){24, 20, 37, 255});
    
    // Setup 2D orthographic projection
    mat4 projection = mat4_ortho(0, ROOM_WIDTH * TILE_SIZE * game->camera.zoom,
                                 ROOM_HEIGHT * TILE_SIZE * game->camera.zoom, 0,
                                 -1, 1);
    
    // Apply camera transform
    mat4 view = mat4_translate(-game->camera.position.x, -game->camera.position.y, 0);
    mat4 view_projection = mat4_multiply(projection, view);
    
    // Render tiles
    if (game->current_room) {
        for (i32 y = 0; y < ROOM_HEIGHT; y++) {
            for (i32 x = 0; x < ROOM_WIDTH; x++) {
                tile_type tile = game->current_room->tiles[y][x];
                v2 pos = {x * TILE_SIZE, y * TILE_SIZE};
                
                sprite_id tile_sprite = SPRITE_FLOOR;
                switch (tile) {
                    case TILE_WALL: tile_sprite = SPRITE_WALL; break;
                    case TILE_DOOR_LOCKED: tile_sprite = SPRITE_DOOR_CLOSED; break;
                    case TILE_DOOR_OPEN: tile_sprite = SPRITE_DOOR_OPEN; break;
                    case TILE_CHEST: tile_sprite = SPRITE_CHEST_CLOSED; break;
                    case TILE_STAIRS_DOWN: tile_sprite = SPRITE_STAIRS; break;
                    default: tile_sprite = SPRITE_FLOOR; break;
                }
                
                if (g_main_sprite_sheet) {
                    sprite* s = &g_main_sprite_sheet->sprites[tile_sprite];
                    sprite_draw(s, pos, 0, (v2){1, 1}, (color32){255, 255, 255, 255});
                }
            }
        }
    }
    
    // Sort entities by Y position for depth
    // (Simple bubble sort for small entity count)
    for (u32 i = 0; i < game->entity_count - 1; i++) {
        for (u32 j = 0; j < game->entity_count - i - 1; j++) {
            if (game->entities[j].position.y > game->entities[j + 1].position.y) {
                entity temp = game->entities[j];
                game->entities[j] = game->entities[j + 1];
                game->entities[j + 1] = temp;
            }
        }
    }
    
    // Render entities with sprites
    for (u32 i = 0; i < game->entity_count; i++) {
        entity* e = &game->entities[i];
        if (!e->is_active) continue;
        
        // Flash effect when invulnerable
        color32 tint = {255, 255, 255, 255};
        if (e->invulnerable_timer > 0) {
            if ((int)(e->invulnerable_timer * 10) % 2 == 0) {
                tint.a = 128;  // Semi-transparent
            }
        }
        
        sprite* s = get_entity_sprite(e);
        if (s) {
            sprite_draw(s, e->position, 0, (v2){1, 1}, tint);
        }
    }
    
    // Render sword swing effect
    if (game->player.is_attacking) {
        // Draw sword arc
        v2 sword_pos = game->player.entity->position;
        f32 angle = game->player.sword_swing_angle;
        
        switch (game->player.entity->facing) {
            case DIR_NORTH: angle -= M_PI/2; sword_pos.y -= 10; break;
            case DIR_SOUTH: angle += M_PI/2; sword_pos.y += 10; break;
            case DIR_WEST: angle += M_PI; sword_pos.x -= 10; break;
            case DIR_EAST: sword_pos.x += 10; break;
        }
        
        if (g_main_sprite_sheet) {
            sprite* sword = &g_main_sprite_sheet->sprites[SPRITE_SWORD];
            sprite_draw(sword, sword_pos, angle, (v2){1.2f, 1.2f}, 
                       (color32){255, 255, 255, 255});
        }
    }
    
    // Flush sprite batch
    sprite_renderer_flush(&g_sprite_renderer);
    
    // Render UI on top
    game_render_ui_enhanced(game, r);
}

void game_render_ui_enhanced(game_state* game, renderer* r)
{
    // Draw heart containers
    for (i32 i = 0; i < (i32)game->player.entity->max_health; i++) {
        v2 pos = {10 + i * 20, 10};
        
        if (g_main_sprite_sheet) {
            sprite* heart = &g_main_sprite_sheet->sprites[SPRITE_HEART];
            color32 tint = (i < (i32)game->player.entity->health) ?
                          (color32){255, 255, 255, 255} :
                          (color32){64, 64, 64, 255};  // Empty heart
            sprite_draw(heart, pos, 0, (v2){1, 1}, tint);
        }
    }
    
    // Draw rupee counter
    if (g_main_sprite_sheet) {
        sprite* rupee = &g_main_sprite_sheet->sprites[SPRITE_RUPEE];
        sprite_draw(rupee, (v2){10, 35}, 0, (v2){1, 1}, (color32){255, 255, 255, 255});
    }
    renderer_text(r, 30, 40, (color32){255, 255, 255, 255}, "x%03d", game->player.rupees);
    
    // Draw key counter
    if (g_main_sprite_sheet) {
        sprite* key = &g_main_sprite_sheet->sprites[SPRITE_KEY];
        sprite_draw(key, (v2){10, 55}, 0, (v2){1, 1}, (color32){255, 255, 255, 255});
    }
    renderer_text(r, 30, 60, (color32){255, 255, 255, 255}, "x%02d", game->player.keys);
    
    // Draw equipped items
    v2 item_a_pos = {ROOM_WIDTH * TILE_SIZE - 60, 10};
    v2 item_b_pos = {ROOM_WIDTH * TILE_SIZE - 35, 10};
    
    // Draw item slots
    renderer_rect(r, item_a_pos.x - 2, item_a_pos.y - 2, 20, 20, (color32){80, 80, 80, 255});
    renderer_rect(r, item_b_pos.x - 2, item_b_pos.y - 2, 20, 20, (color32){80, 80, 80, 255});
    
    // Draw equipped items
    // ... (draw item sprites)
    
    // Draw minimap
    if (game->ui.show_minimap) {
        v2 minimap_pos = {ROOM_WIDTH * TILE_SIZE - 100, ROOM_HEIGHT * TILE_SIZE - 100};
        renderer_rect_filled(r, minimap_pos.x, minimap_pos.y, 90, 90, 
                           (color32){0, 0, 0, 180});
        
        // Draw room layout
        // Current room in center
        renderer_rect_filled(r, minimap_pos.x + 40, minimap_pos.y + 40, 10, 10,
                           (color32){255, 255, 0, 255});
        
        // Adjacent rooms
        if (game->current_room->has_door[DIR_NORTH])
            renderer_rect_filled(r, minimap_pos.x + 40, minimap_pos.y + 25, 10, 10,
                               (color32){100, 100, 100, 255});
        if (game->current_room->has_door[DIR_SOUTH])
            renderer_rect_filled(r, minimap_pos.x + 40, minimap_pos.y + 55, 10, 10,
                               (color32){100, 100, 100, 255});
        if (game->current_room->has_door[DIR_WEST])
            renderer_rect_filled(r, minimap_pos.x + 25, minimap_pos.y + 40, 10, 10,
                               (color32){100, 100, 100, 255});
        if (game->current_room->has_door[DIR_EAST])
            renderer_rect_filled(r, minimap_pos.x + 55, minimap_pos.y + 40, 10, 10,
                               (color32){100, 100, 100, 255});
    }
}

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================

int main(int argc, char** argv)
{
    printf("Crystal Dungeons - A Handmade Adventure\n");
    printf("========================================\n");
    printf("Controls:\n");
    printf("  WASD/Arrows - Move\n");
    printf("  Space - Attack\n");
    printf("  Z - Use Item A\n");
    printf("  X - Use Item B\n");
    printf("  I - Inventory\n");
    printf("  E - Interact\n");
    printf("  M - Toggle Minimap\n");
    printf("\n");
    
    // Initialize platform
    platform_state platform = {0};
    if (!platform_init(&platform, "Crystal Dungeons", 1024, 704)) {  // 64x44 tiles at 16px
        fprintf(stderr, "Failed to initialize platform!\n");
        return 1;
    }
    
    // Initialize renderer
    renderer render = {0};
    renderer_init(&render, &platform);
    
    // Initialize physics
    physics_world physics = {0};
    physics_init(&physics);
    
    // Initialize audio
    audio_system audio = {0};
    audio_init(&audio);
    audio_set_master_volume(&audio, 0.7f);
    
    // Initialize sprite system
    sprite_renderer_init(&g_sprite_renderer);
    
    // Load sprite sheet (would normally load from file)
    // g_main_sprite_sheet = sprite_sheet_create("assets/sprites.png", 16, 16);
    
    // Initialize game
    game_state game = {0};
    game_init(&game);
    game.ui.show_minimap = true;
    
    // Play dungeon music
    // audio_play_music(&audio, "assets/dungeon_theme.ogg", true);
    
    // Game loop
    f64 last_time = platform_get_time(&platform);
    f64 accumulator = 0.0;
    const f64 fixed_timestep = 1.0 / 60.0;  // 60 FPS
    
    while (platform_process_events(&platform)) {
        f64 current_time = platform_get_time(&platform);
        f64 frame_time = current_time - last_time;
        last_time = current_time;
        
        // Cap frame time to prevent spiral of death
        if (frame_time > 0.25) frame_time = 0.25;
        
        accumulator += frame_time;
        
        // Fixed timestep update
        while (accumulator >= fixed_timestep) {
            // Handle input
            game_handle_input(&game, &platform.input);
            
            // Toggle minimap
            if (platform.keyboard.keys[KEY_M] && !platform.keyboard.prev_keys[KEY_M]) {
                game.ui.show_minimap = !game.ui.show_minimap;
            }
            
            // Update game
            game_update(&game, (f32)fixed_timestep);
            
            // Update physics
            physics_step(&physics, (f32)fixed_timestep);
            
            accumulator -= fixed_timestep;
        }
        
        // Render
        renderer_begin_frame(&render);
        game_render_enhanced(&game, &render);
        renderer_end_frame(&render);
        
        platform_swap_buffers(&platform);
        
        // FPS counter
        static u32 frame_count = 0;
        static f64 fps_timer = 0;
        frame_count++;
        fps_timer += frame_time;
        
        if (fps_timer >= 1.0) {
            char title[256];
            snprintf(title, sizeof(title), "Crystal Dungeons - FPS: %d", frame_count);
            platform_set_window_title(&platform, title);
            frame_count = 0;
            fps_timer = 0;
        }
    }
    
    // Cleanup
    game_shutdown(&game);
    audio_shutdown(&audio);
    physics_shutdown(&physics);
    renderer_shutdown(&render);
    platform_shutdown(&platform);
    
    printf("\nThanks for playing Crystal Dungeons!\n");
    return 0;
}