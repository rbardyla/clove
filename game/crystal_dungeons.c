// crystal_dungeons.c - Implementation of Zelda-inspired game
// Testing and showcasing the handmade engine capabilities

#include "crystal_dungeons.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

// ============================================================================
// CONSTANTS
// ============================================================================

#define PLAYER_SPEED 100.0f
#define PLAYER_ATTACK_RANGE 20.0f
#define SWORD_SWING_SPEED 10.0f
#define KNOCKBACK_FORCE 200.0f
#define INVULNERABLE_TIME 1.0f
#define ROOM_TRANSITION_TIME 0.5f

// ============================================================================
// INITIALIZATION
// ============================================================================

void game_init(game_state* game)
{
    memset(game, 0, sizeof(game_state));
    
    game->current_state = GAME_STATE_PLAYING;
    
    // Initialize player
    player_init(&game->player);
    
    // Create player entity
    game->player.entity = entity_create(game, ENTITY_PLAYER, 
        (v2){ROOM_WIDTH * TILE_SIZE / 2, ROOM_HEIGHT * TILE_SIZE / 2});
    game->player.entity->health = 3.0f;
    game->player.entity->max_health = 3.0f;
    
    // Generate first dungeon
    game->current_dungeon = dungeon_generate(1, 12345);
    game->current_room = game->current_dungeon->entrance;
    room_load(game, game->current_room);
    
    // Setup camera
    game->camera.position = (v2){0, 0};
    game->camera.zoom = 2.0f;
    game->camera.bounds = (rect){
        .min = {0, 0},
        .max = {ROOM_WIDTH * TILE_SIZE, ROOM_HEIGHT * TILE_SIZE}
    };
    
    // Audio
    game->audio.music_volume = 0.7f;
    game->audio.sfx_volume = 1.0f;
}

void game_shutdown(game_state* game)
{
    if (game->current_dungeon) {
        dungeon_destroy(game->current_dungeon);
    }
}

// ============================================================================
// MAIN GAME LOOP
// ============================================================================

void game_update(game_state* game, f32 dt)
{
    game->delta_time = dt;
    
    switch (game->current_state) {
        case GAME_STATE_PLAYING: {
            // Update player
            player_update(&game->player, dt);
            
            // Update all entities
            for (u32 i = 0; i < game->entity_count; i++) {
                entity* e = &game->entities[i];
                if (e->is_active && e->is_alive) {
                    entity_update(e, dt);
                    
                    // Update AI for enemies
                    if (e->type >= ENTITY_SLIME && e->type <= ENTITY_DRAGON) {
                        ai_update(e, game, dt);
                    }
                    
                    // Check collision with player
                    if (e != game->player.entity) {
                        if (entity_check_collision(game->player.entity, e)) {
                            // Handle collision based on entity type
                            switch (e->type) {
                                case ENTITY_HEART:
                                    game->player.entity->health = 
                                        fminf(game->player.entity->health + 1.0f,
                                             game->player.entity->max_health);
                                    entity_destroy(game, e);
                                    break;
                                    
                                case ENTITY_RUPEE:
                                    game->player.rupees++;
                                    entity_destroy(game, e);
                                    break;
                                    
                                case ENTITY_KEY:
                                    game->player.keys++;
                                    entity_destroy(game, e);
                                    break;
                                    
                                default:
                                    // Enemy collision - damage player
                                    if (e->type >= ENTITY_SLIME && e->type <= ENTITY_DRAGON) {
                                        if (game->player.entity->invulnerable_timer <= 0) {
                                            player_take_damage(&game->player, e->damage);
                                            v2 knockback_dir = v2_normalize(
                                                v2_sub(game->player.entity->position, e->position));
                                            entity_apply_knockback(game->player.entity, 
                                                                 knockback_dir, KNOCKBACK_FORCE);
                                        }
                                    }
                                    break;
                            }
                        }
                    }
                }
            }
            
            // Check room clear condition
            if (!game->current_room->is_cleared) {
                bool all_enemies_dead = true;
                for (u32 i = 0; i < game->entity_count; i++) {
                    entity* e = &game->entities[i];
                    if (e->type >= ENTITY_SLIME && e->type <= ENTITY_DRAGON &&
                        e->is_alive) {
                        all_enemies_dead = false;
                        break;
                    }
                }
                
                if (all_enemies_dead) {
                    game->current_room->is_cleared = true;
                    // Open doors, spawn rewards, etc.
                    for (i32 dir = 0; dir < DIR_COUNT; dir++) {
                        if (game->current_room->has_door[dir]) {
                            game->current_room->door_locked[dir] = false;
                        }
                    }
                }
            }
            
            // Handle room transitions
            if (game->camera.is_transitioning) {
                game->camera.transition_timer -= dt;
                if (game->camera.transition_timer <= 0) {
                    game->camera.is_transitioning = false;
                    // Complete transition
                    room_transition(game, game->camera.transition_direction);
                }
            }
        } break;
        
        case GAME_STATE_INVENTORY:
            // Update inventory UI
            break;
            
        case GAME_STATE_DIALOGUE:
            game->ui.dialogue_timer -= dt;
            if (game->ui.dialogue_timer <= 0) {
                game->current_state = GAME_STATE_PLAYING;
            }
            break;
            
        case GAME_STATE_GAME_OVER:
            // Handle game over
            break;
    }
}

void game_render(game_state* game)
{
    // Clear screen
    // renderer_clear(0x1a1a2e);  // Dark blue background
    
    // Apply camera transform
    // renderer_push_transform(game->camera.position, game->camera.zoom);
    
    // Render current room
    if (game->current_room) {
        for (i32 y = 0; y < ROOM_HEIGHT; y++) {
            for (i32 x = 0; x < ROOM_WIDTH; x++) {
                tile_type tile = game->current_room->tiles[y][x];
                v2 pos = {x * TILE_SIZE, y * TILE_SIZE};
                
                // Render tile based on type
                // renderer_draw_tile(tile, pos);
            }
        }
    }
    
    // Render entities
    for (u32 i = 0; i < game->entity_count; i++) {
        if (game->entities[i].is_active) {
            entity_render(&game->entities[i]);
        }
    }
    
    // Render player
    if (game->player.entity) {
        entity_render(game->player.entity);
        
        // Render sword swing
        if (game->player.is_attacking) {
            // Draw sword arc
        }
    }
    
    // renderer_pop_transform();
    
    // Render UI
    ui_render_hud(game);
    
    if (game->current_state == GAME_STATE_INVENTORY) {
        inventory_render(game);
    }
    
    if (game->current_state == GAME_STATE_DIALOGUE) {
        ui_render_dialogue(game);
    }
}

void game_handle_input(game_state* game, input_state* input)
{
    // Movement
    game->input.movement.x = 0;
    game->input.movement.y = 0;
    
    if (input->keyboard.keys[KEY_W] || input->keyboard.keys[KEY_UP]) {
        game->input.movement.y = -1;
    }
    if (input->keyboard.keys[KEY_S] || input->keyboard.keys[KEY_DOWN]) {
        game->input.movement.y = 1;
    }
    if (input->keyboard.keys[KEY_A] || input->keyboard.keys[KEY_LEFT]) {
        game->input.movement.x = -1;
    }
    if (input->keyboard.keys[KEY_D] || input->keyboard.keys[KEY_RIGHT]) {
        game->input.movement.x = 1;
    }
    
    // Normalize diagonal movement
    if (game->input.movement.x != 0 && game->input.movement.y != 0) {
        game->input.movement = v2_normalize(game->input.movement);
    }
    
    // Actions
    game->input.attack_pressed = input->keyboard.keys[KEY_SPACE];
    game->input.use_item_a_pressed = input->keyboard.keys[KEY_Z];
    game->input.use_item_b_pressed = input->keyboard.keys[KEY_X];
    game->input.interact_pressed = input->keyboard.keys[KEY_E];
    game->input.inventory_pressed = input->keyboard.keys[KEY_I];
    
    // Handle inventory toggle
    if (game->input.inventory_pressed) {
        if (game->current_state == GAME_STATE_PLAYING) {
            game->current_state = GAME_STATE_INVENTORY;
        } else if (game->current_state == GAME_STATE_INVENTORY) {
            game->current_state = GAME_STATE_PLAYING;
        }
    }
}

// ============================================================================
// PLAYER
// ============================================================================

void player_init(player* p)
{
    p->max_health = 3.0f;
    p->max_magic = 10.0f;
    p->magic = p->max_magic;
    p->rupees = 0;
    p->arrows = 20;
    p->bombs = 5;
    p->keys = 0;
    
    p->equipped_sword = ITEM_SWORD_WOOD;
    p->equipped_shield = ITEM_SHIELD_WOOD;
    p->equipped_tunic = ITEM_TUNIC_GREEN;
    
    // Start with basic items
    inventory_add_item(p, ITEM_SWORD_WOOD, 1);
    inventory_add_item(p, ITEM_SHIELD_WOOD, 1);
}

void player_update(player* p, f32 dt)
{
    entity* e = p->entity;
    if (!e) return;
    
    // Get game state for input
    game_state* game = (game_state*)((u8*)p - offsetof(game_state, player));
    
    // Movement
    v2 movement = game->input.movement;
    e->velocity = v2_scale(movement, PLAYER_SPEED);
    
    // Update facing direction
    if (fabsf(movement.x) > fabsf(movement.y)) {
        e->facing = movement.x > 0 ? DIR_EAST : DIR_WEST;
    } else if (fabsf(movement.y) > 0.1f) {
        e->facing = movement.y > 0 ? DIR_SOUTH : DIR_NORTH;
    }
    
    // Attack
    if (game->input.attack_pressed && !p->is_attacking && e->attack_cooldown <= 0) {
        player_attack(p);
    }
    
    // Update attack state
    if (p->is_attacking) {
        p->sword_swing_angle += SWORD_SWING_SPEED * dt;
        if (p->sword_swing_angle >= M_PI) {
            p->is_attacking = false;
            p->sword_swing_angle = 0;
            e->attack_cooldown = 0.3f;
        }
        
        // Update sword hitbox
        f32 reach = PLAYER_ATTACK_RANGE;
        v2 offset = {0, 0};
        switch (e->facing) {
            case DIR_NORTH: offset.y = -reach; break;
            case DIR_SOUTH: offset.y = reach; break;
            case DIR_WEST: offset.x = -reach; break;
            case DIR_EAST: offset.x = reach; break;
        }
        
        p->sword_hitbox = (rect){
            .min = v2_add(e->position, offset),
            .max = v2_add(v2_add(e->position, offset), (v2){16, 16})
        };
    }
    
    // Use items
    if (game->input.use_item_a_pressed) {
        player_use_item(p, p->equipped_item_a);
    }
    if (game->input.use_item_b_pressed) {
        player_use_item(p, p->equipped_item_b);
    }
    
    // Interaction
    if (game->input.interact_pressed) {
        // Check for nearby NPCs or objects
        // ...
    }
}

void player_attack(player* p)
{
    p->is_attacking = true;
    p->sword_swing_angle = 0;
    
    // Play sword sound
    // audio_play_sound(SOUND_SWORD_SWING);
    
    // Check for immediate hits
    game_state* game = (game_state*)((u8*)p - offsetof(game_state, player));
    combat_sword_swing(game, p);
}

void player_use_item(player* p, item_type item)
{
    game_state* game = (game_state*)((u8*)p - offsetof(game_state, player));
    
    switch (item) {
        case ITEM_BOW:
            if (p->arrows > 0) {
                p->arrows--;
                v2 dir = {0, 0};
                switch (p->entity->facing) {
                    case DIR_NORTH: dir.y = -1; break;
                    case DIR_SOUTH: dir.y = 1; break;
                    case DIR_WEST: dir.x = -1; break;
                    case DIR_EAST: dir.x = 1; break;
                }
                combat_shoot_projectile(game, p->entity, ENTITY_ARROW, dir);
            }
            break;
            
        case ITEM_BOMBS:
            if (p->bombs > 0) {
                p->bombs--;
                bomb_place(game, p->entity->position);
            }
            break;
            
        case ITEM_BOOMERANG:
            // Implement boomerang logic
            break;
            
        case ITEM_HOOKSHOT:
            // Implement hookshot logic
            break;
    }
}

void player_take_damage(player* p, f32 damage)
{
    if (p->entity->invulnerable_timer > 0) return;
    
    p->entity->health -= damage;
    p->entity->invulnerable_timer = INVULNERABLE_TIME;
    
    // Flash red
    // Play hurt sound
    
    if (p->entity->health <= 0) {
        // Game over
        game_state* game = (game_state*)((u8*)p - offsetof(game_state, player));
        game->current_state = GAME_STATE_GAME_OVER;
    }
}

// ============================================================================
// ENTITY SYSTEM
// ============================================================================

entity* entity_create(game_state* game, entity_type type, v2 position)
{
    if (game->entity_count >= MAX_ENTITIES) return NULL;
    
    entity* e = &game->entities[game->entity_count++];
    memset(e, 0, sizeof(entity));
    
    e->type = type;
    e->position = position;
    e->is_alive = true;
    e->is_active = true;
    
    // Set default properties based on type
    switch (type) {
        case ENTITY_PLAYER:
            e->size = (v2){14, 14};
            e->health = 3;
            e->max_health = 3;
            e->damage = 1;
            e->is_solid = true;
            break;
            
        case ENTITY_SLIME:
            e->size = (v2){12, 12};
            e->health = 1;
            e->max_health = 1;
            e->damage = 0.5f;
            e->is_solid = true;
            e->ai.state = AI_PATROL;
            e->ai.home_position = position;
            break;
            
        case ENTITY_SKELETON:
            e->size = (v2){14, 14};
            e->health = 2;
            e->max_health = 2;
            e->damage = 1;
            e->is_solid = true;
            e->ai.state = AI_PATROL;
            break;
            
        case ENTITY_HEART:
        case ENTITY_RUPEE:
        case ENTITY_KEY:
            e->size = (v2){8, 8};
            e->is_solid = false;
            break;
    }
    
    // Set collision box
    e->collision_box = (rect){
        .min = {-e->size.x/2, -e->size.y/2},
        .max = {e->size.x/2, e->size.y/2}
    };
    
    return e;
}

void entity_destroy(game_state* game, entity* e)
{
    e->is_alive = false;
    e->is_active = false;
    
    if (e->on_death) {
        e->on_death(e);
    }
}

void entity_update(entity* e, f32 dt)
{
    // Update position
    e->position = v2_add(e->position, v2_scale(e->velocity, dt));
    
    // Update knockback
    if (e->knockback_timer > 0) {
        e->knockback_timer -= dt;
        e->position = v2_add(e->position, v2_scale(e->knockback_velocity, dt));
        e->knockback_velocity = v2_scale(e->knockback_velocity, 0.9f); // Friction
    }
    
    // Update invulnerability
    if (e->invulnerable_timer > 0) {
        e->invulnerable_timer -= dt;
    }
    
    // Update attack cooldown
    if (e->attack_cooldown > 0) {
        e->attack_cooldown -= dt;
    }
    
    // Update animation
    e->animation_timer += dt;
    if (e->animation_timer >= 0.1f) {
        e->animation_timer = 0;
        e->animation_frame++;
    }
}

void entity_render(entity* e)
{
    // Placeholder rendering
    // In real implementation, draw sprite based on type and animation frame
    
    // Flash if invulnerable
    if (e->invulnerable_timer > 0) {
        if ((int)(e->invulnerable_timer * 10) % 2 == 0) {
            return; // Don't draw every other frame
        }
    }
    
    // Draw entity sprite
    // renderer_draw_sprite(e->sprite_id, e->position, e->animation_frame);
}

bool entity_check_collision(entity* a, entity* b)
{
    rect a_box = {
        .min = v2_add(a->position, a->collision_box.min),
        .max = v2_add(a->position, a->collision_box.max)
    };
    
    rect b_box = {
        .min = v2_add(b->position, b->collision_box.min),
        .max = v2_add(b->position, b->collision_box.max)
    };
    
    return rect_overlaps(a_box, b_box);
}

void entity_apply_knockback(entity* e, v2 direction, f32 force)
{
    e->knockback_velocity = v2_scale(direction, force);
    e->knockback_timer = 0.3f;
}

// ============================================================================
// AI SYSTEM
// ============================================================================

void ai_update(entity* e, game_state* game, f32 dt)
{
    e->ai.think_timer -= dt;
    if (e->ai.think_timer > 0) return;
    
    e->ai.think_timer = 0.2f; // Think 5 times per second
    
    player* p = &game->player;
    f32 dist_to_player = v2_length(v2_sub(p->entity->position, e->position));
    
    switch (e->ai.state) {
        case AI_IDLE:
            // Just stand there
            e->velocity = (v2){0, 0};
            
            // Check if player is nearby
            if (dist_to_player < 100) {
                e->ai.state = AI_CHASE;
            }
            break;
            
        case AI_PATROL:
            ai_patrol(e, dt);
            
            // Chase if player gets close
            if (dist_to_player < 80) {
                e->ai.state = AI_CHASE;
            }
            break;
            
        case AI_CHASE:
            ai_chase_player(e, p, dt);
            
            // Attack if close enough
            if (dist_to_player < 20) {
                e->ai.state = AI_ATTACK;
                e->ai.state_timer = 0.5f;
            }
            
            // Stop chasing if too far
            if (dist_to_player > 150) {
                e->ai.state = AI_PATROL;
            }
            break;
            
        case AI_ATTACK:
            ai_attack_pattern(e, p);
            
            e->ai.state_timer -= dt;
            if (e->ai.state_timer <= 0) {
                e->ai.state = AI_CHASE;
            }
            break;
    }
}

void ai_patrol(entity* e, f32 dt)
{
    // Simple back and forth patrol
    e->ai.state_timer += dt;
    
    f32 patrol_speed = 30.0f;
    f32 patrol_radius = 50.0f;
    
    e->position.x = e->ai.home_position.x + sinf(e->ai.state_timer) * patrol_radius;
    e->velocity.x = cosf(e->ai.state_timer) * patrol_speed;
}

void ai_chase_player(entity* e, player* p, f32 dt)
{
    v2 to_player = v2_sub(p->entity->position, e->position);
    to_player = v2_normalize(to_player);
    
    f32 chase_speed = 50.0f;
    e->velocity = v2_scale(to_player, chase_speed);
}

void ai_attack_pattern(entity* e, player* p)
{
    // Simple attack - just damage on contact
    // More complex enemies would have special attack patterns
}

// ============================================================================
// DUNGEON GENERATION
// ============================================================================

dungeon* dungeon_generate(u32 floor_number, u32 seed)
{
    srand(seed);
    
    dungeon* d = (dungeon*)malloc(sizeof(dungeon));
    memset(d, 0, sizeof(dungeon));
    
    sprintf(d->name, "Crystal Dungeon Floor %d", floor_number);
    d->floor_number = floor_number;
    d->crystals_total = 1; // One crystal per dungeon
    
    // Generate a simple 3x3 dungeon layout for now
    // In a real implementation, use procedural generation
    
    // Create entrance room
    d->entrance = room_create();
    room_generate(d->entrance, rand());
    d->rooms[d->room_count++] = *d->entrance;
    
    // Create more rooms...
    // Connect them with doors...
    
    return d;
}

void dungeon_destroy(dungeon* d)
{
    free(d);
}

room* room_create(void)
{
    room* r = (room*)malloc(sizeof(room));
    memset(r, 0, sizeof(room));
    
    // Initialize with floor tiles
    for (i32 y = 0; y < ROOM_HEIGHT; y++) {
        for (i32 x = 0; x < ROOM_WIDTH; x++) {
            r->tiles[y][x] = TILE_FLOOR;
        }
    }
    
    // Add walls around edges
    for (i32 x = 0; x < ROOM_WIDTH; x++) {
        r->tiles[0][x] = TILE_WALL;
        r->tiles[ROOM_HEIGHT-1][x] = TILE_WALL;
    }
    for (i32 y = 0; y < ROOM_HEIGHT; y++) {
        r->tiles[y][0] = TILE_WALL;
        r->tiles[y][ROOM_WIDTH-1] = TILE_WALL;
    }
    
    return r;
}

void room_generate(room* r, u32 seed)
{
    srand(seed);
    
    // Add some random obstacles
    for (i32 i = 0; i < 5; i++) {
        i32 x = 2 + rand() % (ROOM_WIDTH - 4);
        i32 y = 2 + rand() % (ROOM_HEIGHT - 4);
        r->tiles[y][x] = TILE_WALL;
    }
    
    // Add doors (centered on each wall)
    r->has_door[DIR_NORTH] = true;
    r->tiles[0][ROOM_WIDTH/2] = TILE_DOOR_LOCKED;
    
    r->has_door[DIR_SOUTH] = true;
    r->tiles[ROOM_HEIGHT-1][ROOM_WIDTH/2] = TILE_DOOR_LOCKED;
    
    r->has_door[DIR_WEST] = true;
    r->tiles[ROOM_HEIGHT/2][0] = TILE_DOOR_LOCKED;
    
    r->has_door[DIR_EAST] = true;
    r->tiles[ROOM_HEIGHT/2][ROOM_WIDTH-1] = TILE_DOOR_LOCKED;
}

void room_load(game_state* game, room* r)
{
    // Clear existing entities (except player)
    entity* player_entity = game->player.entity;
    game->entity_count = 0;
    game->entities[game->entity_count++] = *player_entity;
    game->player.entity = &game->entities[0];
    
    // Spawn enemies
    for (i32 i = 0; i < 3 + rand() % 4; i++) {
        v2 pos = {
            TILE_SIZE * (2 + rand() % (ROOM_WIDTH - 4)),
            TILE_SIZE * (2 + rand() % (ROOM_HEIGHT - 4))
        };
        
        entity_type enemy_type = ENTITY_SLIME + rand() % 3; // Random enemy
        entity_create(game, enemy_type, pos);
    }
    
    // Spawn pickups
    if (rand() % 100 < 30) { // 30% chance for heart
        v2 pos = {
            TILE_SIZE * (2 + rand() % (ROOM_WIDTH - 4)),
            TILE_SIZE * (2 + rand() % (ROOM_HEIGHT - 4))
        };
        entity_create(game, ENTITY_HEART, pos);
    }
}

void room_transition(game_state* game, direction dir)
{
    // Move to next room based on direction
    room* next_room = game->current_room->neighbors[dir];
    if (!next_room) {
        // Create new room if it doesn't exist
        next_room = room_create();
        room_generate(next_room, rand());
        game->current_room->neighbors[dir] = next_room;
        
        // Link back
        direction opposite = (dir + 2) % DIR_COUNT;
        next_room->neighbors[opposite] = game->current_room;
    }
    
    // Load new room
    game->current_room = next_room;
    room_load(game, next_room);
    
    // Position player at opposite edge
    switch (dir) {
        case DIR_NORTH:
            game->player.entity->position.y = (ROOM_HEIGHT - 2) * TILE_SIZE;
            break;
        case DIR_SOUTH:
            game->player.entity->position.y = 2 * TILE_SIZE;
            break;
        case DIR_WEST:
            game->player.entity->position.x = (ROOM_WIDTH - 2) * TILE_SIZE;
            break;
        case DIR_EAST:
            game->player.entity->position.x = 2 * TILE_SIZE;
            break;
    }
}

// ============================================================================
// COMBAT
// ============================================================================

void combat_sword_swing(game_state* game, player* p)
{
    // Check all enemies for hits
    for (u32 i = 0; i < game->entity_count; i++) {
        entity* e = &game->entities[i];
        
        if (e->type >= ENTITY_SLIME && e->type <= ENTITY_DRAGON && e->is_alive) {
            // Check if enemy is in sword hitbox
            rect enemy_box = {
                .min = v2_add(e->position, e->collision_box.min),
                .max = v2_add(e->position, e->collision_box.max)
            };
            
            if (rect_overlaps(p->sword_hitbox, enemy_box)) {
                // Deal damage
                e->health -= p->entity->damage;
                
                // Knockback
                v2 knockback_dir = v2_normalize(v2_sub(e->position, p->entity->position));
                entity_apply_knockback(e, knockback_dir, KNOCKBACK_FORCE * 1.5f);
                
                // Destroy if dead
                if (e->health <= 0) {
                    entity_destroy(game, e);
                    
                    // Chance to drop items
                    if (rand() % 100 < 20) {
                        entity_create(game, ENTITY_HEART, e->position);
                    } else if (rand() % 100 < 50) {
                        entity_create(game, ENTITY_RUPEE, e->position);
                    }
                }
            }
        }
    }
}

void combat_shoot_projectile(game_state* game, entity* source, 
                            entity_type projectile_type, v2 direction)
{
    v2 spawn_pos = v2_add(source->position, v2_scale(direction, 20));
    entity* proj = entity_create(game, projectile_type, spawn_pos);
    
    if (proj) {
        proj->velocity = v2_scale(direction, 200); // Fast projectile
        proj->damage = source->damage;
        proj->is_solid = false;
    }
}

// ============================================================================
// UI
// ============================================================================

void ui_render_hud(game_state* game)
{
    // Draw hearts
    for (i32 i = 0; i < (i32)game->player.entity->max_health; i++) {
        v2 pos = {10 + i * 20, 10};
        bool filled = i < (i32)game->player.entity->health;
        // renderer_draw_heart(pos, filled);
    }
    
    // Draw rupee count
    // renderer_draw_text(va("Rupees: %d", game->player.rupees), (v2){10, 40});
    
    // Draw key count
    // renderer_draw_text(va("Keys: %d", game->player.keys), (v2){10, 60});
    
    // Draw minimap
    if (game->ui.show_minimap) {
        ui_render_minimap(game);
    }
}

void ui_render_minimap(game_state* game)
{
    // Draw minimap in corner
    v2 minimap_pos = {ROOM_WIDTH * TILE_SIZE - 100, 10};
    v2 minimap_size = {90, 90};
    
    // Draw explored rooms
    // ...
}

void inventory_render(game_state* game)
{
    // Draw inventory grid
    // Show equipped items
    // Allow selection and equipping
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

internal bool rect_overlaps(rect a, rect b)
{
    return !(a.max.x < b.min.x || a.min.x > b.max.x ||
             a.max.y < b.min.y || a.min.y > b.max.y);
}