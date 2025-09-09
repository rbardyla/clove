// sprite_assets.c - Sprite asset implementation with procedural generation

#include "sprite_assets.h"
#include <string.h>

// ============================================================================
// PROCEDURAL COLOR PALETTE
// ============================================================================

// Classic 16-color palette inspired by NES
static const u32 PALETTE[] = {
    0xFF000000,  // 0: Black
    0xFF1D2B53,  // 1: Dark Blue
    0xFF7E2553,  // 2: Dark Purple
    0xFF008751,  // 3: Dark Green
    0xFFAB5236,  // 4: Brown
    0xFF5F574F,  // 5: Dark Gray
    0xFFC2C3C7,  // 6: Light Gray
    0xFFFFF1E8,  // 7: White
    0xFFFF004D,  // 8: Red
    0xFFFFA300,  // 9: Orange
    0xFFFFEC27,  // 10: Yellow
    0xFF00E436,  // 11: Green
    0xFF29ADFF,  // 12: Blue
    0xFF83769C,  // 13: Indigo
    0xFFFF77A8,  // 14: Pink
    0xFFFFCCAA,  // 15: Peach
};

// Color indices for different sprite types
#define COL_BLACK 0
#define COL_DARK_BLUE 1
#define COL_DARK_PURPLE 2
#define COL_DARK_GREEN 3
#define COL_BROWN 4
#define COL_DARK_GRAY 5
#define COL_LIGHT_GRAY 6
#define COL_WHITE 7
#define COL_RED 8
#define COL_ORANGE 9
#define COL_YELLOW 10
#define COL_GREEN 11
#define COL_BLUE 12
#define COL_INDIGO 13
#define COL_PINK 14
#define COL_PEACH 15

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

static void clear_sprite(u32* pixels, u32 color) {
    for (int i = 0; i < SPRITE_SIZE * SPRITE_SIZE; i++) {
        pixels[i] = color;
    }
}

static void set_pixel(u32* pixels, int x, int y, u32 color) {
    if (x >= 0 && x < SPRITE_SIZE && y >= 0 && y < SPRITE_SIZE) {
        pixels[y * SPRITE_SIZE + x] = color;
    }
}

static void draw_line(u32* pixels, int x0, int y0, int x1, int y1, u32 color) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    
    while (1) {
        set_pixel(pixels, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void draw_rect(u32* pixels, int x, int y, int w, int h, u32 color, bool fill) {
    if (fill) {
        for (int py = y; py < y + h; py++) {
            for (int px = x; px < x + w; px++) {
                set_pixel(pixels, px, py, color);
            }
        }
    } else {
        draw_line(pixels, x, y, x + w - 1, y, color);
        draw_line(pixels, x + w - 1, y, x + w - 1, y + h - 1, color);
        draw_line(pixels, x + w - 1, y + h - 1, x, y + h - 1, color);
        draw_line(pixels, x, y + h - 1, x, y, color);
    }
}

static void draw_circle(u32* pixels, int cx, int cy, int r, u32 color, bool fill) {
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x*x + y*y <= r*r) {
                if (fill || (x*x + y*y >= (r-1)*(r-1))) {
                    set_pixel(pixels, cx + x, cy + y, color);
                }
            }
        }
    }
}

// ============================================================================
// SPRITE GENERATION - PLAYER
// ============================================================================

void sprite_generate_player(u32* pixels, u32 sprite_index) {
    clear_sprite(pixels, 0x00000000);  // Transparent
    
    // Simple player sprite - top-down view
    // Body
    draw_rect(pixels, 5, 4, 6, 8, PALETTE[COL_GREEN], true);
    
    // Head
    draw_circle(pixels, 8, 4, 2, PALETTE[COL_PEACH], true);
    
    // Hair
    set_pixel(pixels, 6, 2, PALETTE[COL_BROWN]);
    set_pixel(pixels, 7, 2, PALETTE[COL_BROWN]);
    set_pixel(pixels, 8, 2, PALETTE[COL_BROWN]);
    set_pixel(pixels, 9, 2, PALETTE[COL_BROWN]);
    set_pixel(pixels, 10, 2, PALETTE[COL_BROWN]);
    
    // Arms
    draw_rect(pixels, 3, 6, 2, 4, PALETTE[COL_PEACH], true);
    draw_rect(pixels, 11, 6, 2, 4, PALETTE[COL_PEACH], true);
    
    // Sword (if attacking)
    if (sprite_index >= 12 && sprite_index <= 15) {
        switch (sprite_index) {
            case 12: // Attack down
                draw_rect(pixels, 7, 12, 2, 4, PALETTE[COL_LIGHT_GRAY], true);
                break;
            case 13: // Attack up
                draw_rect(pixels, 7, 0, 2, 4, PALETTE[COL_LIGHT_GRAY], true);
                break;
            case 14: // Attack left
                draw_rect(pixels, 0, 7, 4, 2, PALETTE[COL_LIGHT_GRAY], true);
                break;
            case 15: // Attack right
                draw_rect(pixels, 12, 7, 4, 2, PALETTE[COL_LIGHT_GRAY], true);
                break;
        }
    }
    
    // Eyes
    set_pixel(pixels, 7, 4, PALETTE[COL_BLACK]);
    set_pixel(pixels, 9, 4, PALETTE[COL_BLACK]);
}

// ============================================================================
// SPRITE GENERATION - ENEMIES
// ============================================================================

void sprite_generate_enemy(u32* pixels, enemy_sprite sprite) {
    clear_sprite(pixels, 0x00000000);
    
    switch (sprite) {
        case SPRITE_SLIME_1:
        case SPRITE_SLIME_2: {
            // Green slime blob
            int offset = (sprite == SPRITE_SLIME_2) ? 1 : 0;
            draw_circle(pixels, 8, 10 - offset, 5, PALETTE[COL_GREEN], true);
            draw_circle(pixels, 8, 11 - offset, 4, PALETTE[COL_GREEN], true);
            
            // Eyes
            set_pixel(pixels, 6, 8 - offset, PALETTE[COL_BLACK]);
            set_pixel(pixels, 10, 8 - offset, PALETTE[COL_BLACK]);
            break;
        }
        
        case SPRITE_SKELETON_DOWN_1:
        case SPRITE_SKELETON_DOWN_2: {
            // Skeleton body
            draw_rect(pixels, 6, 5, 4, 6, PALETTE[COL_WHITE], true);
            
            // Skull
            draw_circle(pixels, 8, 4, 2, PALETTE[COL_WHITE], true);
            set_pixel(pixels, 7, 4, PALETTE[COL_BLACK]);
            set_pixel(pixels, 9, 4, PALETTE[COL_BLACK]);
            
            // Bones (arms)
            draw_line(pixels, 5, 6, 3, 8, PALETTE[COL_WHITE]);
            draw_line(pixels, 10, 6, 12, 8, PALETTE[COL_WHITE]);
            
            // Legs
            int leg_offset = (sprite == SPRITE_SKELETON_DOWN_2) ? 1 : 0;
            draw_line(pixels, 7, 11, 6 - leg_offset, 14, PALETTE[COL_WHITE]);
            draw_line(pixels, 9, 11, 10 + leg_offset, 14, PALETTE[COL_WHITE]);
            break;
        }
        
        case SPRITE_BAT_1:
        case SPRITE_BAT_2: {
            // Bat body
            draw_circle(pixels, 8, 8, 2, PALETTE[COL_DARK_PURPLE], true);
            
            // Wings
            int wing_y = (sprite == SPRITE_BAT_2) ? 7 : 8;
            // Left wing
            draw_line(pixels, 5, wing_y, 2, wing_y - 2, PALETTE[COL_DARK_PURPLE]);
            draw_line(pixels, 2, wing_y - 2, 2, wing_y + 2, PALETTE[COL_DARK_PURPLE]);
            draw_line(pixels, 2, wing_y + 2, 5, wing_y, PALETTE[COL_DARK_PURPLE]);
            
            // Right wing
            draw_line(pixels, 11, wing_y, 14, wing_y - 2, PALETTE[COL_DARK_PURPLE]);
            draw_line(pixels, 14, wing_y - 2, 14, wing_y + 2, PALETTE[COL_DARK_PURPLE]);
            draw_line(pixels, 14, wing_y + 2, 11, wing_y, PALETTE[COL_DARK_PURPLE]);
            
            // Eyes
            set_pixel(pixels, 7, 8, PALETTE[COL_RED]);
            set_pixel(pixels, 9, 8, PALETTE[COL_RED]);
            break;
        }
        
        case SPRITE_KNIGHT_STAND:
        case SPRITE_KNIGHT_WALK_1:
        case SPRITE_KNIGHT_WALK_2:
        case SPRITE_KNIGHT_ATTACK: {
            // Armor body
            draw_rect(pixels, 5, 5, 6, 7, PALETTE[COL_DARK_GRAY], true);
            
            // Helmet
            draw_rect(pixels, 6, 2, 4, 4, PALETTE[COL_LIGHT_GRAY], true);
            
            // Shield
            draw_rect(pixels, 3, 6, 2, 4, PALETTE[COL_BLUE], true);
            
            // Sword
            if (sprite == SPRITE_KNIGHT_ATTACK) {
                draw_rect(pixels, 12, 5, 3, 1, PALETTE[COL_LIGHT_GRAY], true);
                set_pixel(pixels, 15, 5, PALETTE[COL_WHITE]);
            } else {
                draw_rect(pixels, 11, 6, 1, 4, PALETTE[COL_LIGHT_GRAY], true);
            }
            
            // Eyes through visor
            set_pixel(pixels, 7, 4, PALETTE[COL_BLACK]);
            set_pixel(pixels, 9, 4, PALETTE[COL_BLACK]);
            break;
        }
        
        case SPRITE_WIZARD_STAND:
        case SPRITE_WIZARD_CAST: {
            // Robe
            draw_rect(pixels, 5, 6, 6, 8, PALETTE[COL_DARK_BLUE], true);
            
            // Hat
            draw_line(pixels, 8, 1, 5, 4, PALETTE[COL_DARK_BLUE]);
            draw_line(pixels, 8, 1, 11, 4, PALETTE[COL_DARK_BLUE]);
            draw_line(pixels, 5, 4, 11, 4, PALETTE[COL_DARK_BLUE]);
            
            // Face
            draw_circle(pixels, 8, 5, 1, PALETTE[COL_PEACH], true);
            
            // Staff
            draw_rect(pixels, 12, 3, 1, 10, PALETTE[COL_BROWN], true);
            
            // Magic orb
            if (sprite == SPRITE_WIZARD_CAST) {
                draw_circle(pixels, 12, 2, 2, PALETTE[COL_YELLOW], true);
            } else {
                draw_circle(pixels, 12, 2, 1, PALETTE[COL_BLUE], true);
            }
            
            // Beard
            set_pixel(pixels, 8, 6, PALETTE[COL_WHITE]);
            set_pixel(pixels, 8, 7, PALETTE[COL_WHITE]);
            break;
        }
        
        default:
            // Unknown enemy - red square
            draw_rect(pixels, 4, 4, 8, 8, PALETTE[COL_RED], true);
            break;
    }
}

// ============================================================================
// SPRITE GENERATION - TILES
// ============================================================================

void sprite_generate_tile(u32* pixels, tile_sprite sprite) {
    switch (sprite) {
        case SPRITE_FLOOR:
            // Stone floor pattern
            clear_sprite(pixels, PALETTE[COL_DARK_GRAY]);
            // Add some texture
            for (int y = 0; y < SPRITE_SIZE; y += 4) {
                for (int x = 0; x < SPRITE_SIZE; x += 4) {
                    set_pixel(pixels, x, y, PALETTE[COL_LIGHT_GRAY]);
                }
            }
            break;
            
        case SPRITE_WALL:
            // Brick wall
            clear_sprite(pixels, PALETTE[COL_BROWN]);
            // Brick lines
            for (int y = 0; y < SPRITE_SIZE; y += 4) {
                draw_line(pixels, 0, y, SPRITE_SIZE-1, y, PALETTE[COL_BLACK]);
            }
            for (int y = 0; y < SPRITE_SIZE; y += 8) {
                for (int x = 0; x < SPRITE_SIZE; x += 8) {
                    draw_line(pixels, x, y, x, y+4, PALETTE[COL_BLACK]);
                }
            }
            for (int y = 4; y < SPRITE_SIZE; y += 8) {
                for (int x = 4; x < SPRITE_SIZE; x += 8) {
                    draw_line(pixels, x, y, x, y+4, PALETTE[COL_BLACK]);
                }
            }
            break;
            
        case SPRITE_WATER_1:
        case SPRITE_WATER_2: {
            // Animated water
            u32 water_color = (sprite == SPRITE_WATER_1) ? PALETTE[COL_BLUE] : PALETTE[COL_DARK_BLUE];
            clear_sprite(pixels, water_color);
            // Wave pattern
            for (int y = 2; y < SPRITE_SIZE; y += 4) {
                for (int x = 0; x < SPRITE_SIZE; x += 2) {
                    int offset = (sprite == SPRITE_WATER_2) ? 1 : 0;
                    set_pixel(pixels, (x + offset) % SPRITE_SIZE, y, PALETTE[COL_WHITE]);
                }
            }
            break;
        }
            
        case SPRITE_LAVA_1:
        case SPRITE_LAVA_2: {
            // Animated lava
            u32 lava_color = (sprite == SPRITE_LAVA_1) ? PALETTE[COL_RED] : PALETTE[COL_ORANGE];
            clear_sprite(pixels, lava_color);
            // Bubbles
            int offset = (sprite == SPRITE_LAVA_2) ? 2 : 0;
            draw_circle(pixels, 4 + offset, 4, 1, PALETTE[COL_YELLOW], true);
            draw_circle(pixels, 12 - offset, 8, 1, PALETTE[COL_YELLOW], true);
            draw_circle(pixels, 8, 12 + offset/2, 1, PALETTE[COL_YELLOW], true);
            break;
        }
            
        case SPRITE_DOOR_CLOSED:
            // Closed door
            clear_sprite(pixels, PALETTE[COL_BROWN]);
            draw_rect(pixels, 2, 0, 12, 16, PALETTE[COL_BROWN], true);
            draw_rect(pixels, 4, 2, 8, 12, PALETTE[COL_DARK_GRAY], true);
            // Handle
            draw_circle(pixels, 12, 8, 1, PALETTE[COL_YELLOW], true);
            break;
            
        case SPRITE_DOOR_OPEN:
            // Open door (dark passage)
            clear_sprite(pixels, PALETTE[COL_BLACK]);
            draw_rect(pixels, 2, 0, 2, 16, PALETTE[COL_BROWN], true);
            draw_rect(pixels, 12, 0, 2, 16, PALETTE[COL_BROWN], true);
            break;
            
        case SPRITE_CHEST_CLOSED:
            // Treasure chest closed
            clear_sprite(pixels, 0x00000000);
            draw_rect(pixels, 3, 6, 10, 8, PALETTE[COL_BROWN], true);
            draw_rect(pixels, 3, 6, 10, 4, PALETTE[COL_YELLOW], true);
            // Lock
            draw_circle(pixels, 8, 10, 1, PALETTE[COL_BLACK], true);
            break;
            
        case SPRITE_CHEST_OPEN:
            // Treasure chest open
            clear_sprite(pixels, 0x00000000);
            draw_rect(pixels, 3, 10, 10, 4, PALETTE[COL_BROWN], true);
            draw_rect(pixels, 3, 6, 10, 2, PALETTE[COL_BROWN], true);
            draw_rect(pixels, 4, 4, 8, 2, PALETTE[COL_BROWN], true);
            // Sparkle
            set_pixel(pixels, 8, 8, PALETTE[COL_YELLOW]);
            set_pixel(pixels, 7, 7, PALETTE[COL_YELLOW]);
            set_pixel(pixels, 9, 7, PALETTE[COL_YELLOW]);
            break;
            
        case SPRITE_TORCH_1:
        case SPRITE_TORCH_2: {
            // Animated torch
            clear_sprite(pixels, 0x00000000);
            // Torch base
            draw_rect(pixels, 7, 8, 2, 8, PALETTE[COL_BROWN], true);
            // Fire
            int flame_offset = (sprite == SPRITE_TORCH_2) ? 1 : 0;
            draw_circle(pixels, 8, 5 - flame_offset, 2, PALETTE[COL_ORANGE], true);
            draw_circle(pixels, 8, 4 - flame_offset, 1, PALETTE[COL_YELLOW], true);
            break;
        }
            
        default:
            // Unknown tile - checkerboard
            for (int y = 0; y < SPRITE_SIZE; y++) {
                for (int x = 0; x < SPRITE_SIZE; x++) {
                    pixels[y * SPRITE_SIZE + x] = ((x + y) % 2) ? 
                        PALETTE[COL_PINK] : PALETTE[COL_BLACK];
                }
            }
            break;
    }
}

// ============================================================================
// SPRITE GENERATION - ITEMS
// ============================================================================

void sprite_generate_item(u32* pixels, item_sprite sprite) {
    clear_sprite(pixels, 0x00000000);
    
    switch (sprite) {
        case SPRITE_SWORD_WOOD:
            // Wooden sword
            draw_rect(pixels, 7, 2, 2, 10, PALETTE[COL_BROWN], true);
            draw_rect(pixels, 5, 10, 6, 2, PALETTE[COL_BROWN], true);
            break;
            
        case SPRITE_SWORD_IRON:
            // Iron sword
            draw_rect(pixels, 7, 2, 2, 10, PALETTE[COL_LIGHT_GRAY], true);
            draw_rect(pixels, 5, 10, 6, 2, PALETTE[COL_DARK_GRAY], true);
            set_pixel(pixels, 8, 2, PALETTE[COL_WHITE]);
            break;
            
        case SPRITE_SWORD_CRYSTAL:
            // Crystal sword
            draw_rect(pixels, 7, 2, 2, 10, PALETTE[COL_BLUE], true);
            draw_rect(pixels, 5, 10, 6, 2, PALETTE[COL_INDIGO], true);
            // Sparkle
            set_pixel(pixels, 6, 4, PALETTE[COL_WHITE]);
            set_pixel(pixels, 10, 6, PALETTE[COL_WHITE]);
            break;
            
        case SPRITE_HEART_FULL:
            // Full heart
            set_pixel(pixels, 6, 5, PALETTE[COL_RED]);
            set_pixel(pixels, 7, 5, PALETTE[COL_RED]);
            set_pixel(pixels, 9, 5, PALETTE[COL_RED]);
            set_pixel(pixels, 10, 5, PALETTE[COL_RED]);
            draw_rect(pixels, 5, 6, 7, 3, PALETTE[COL_RED], true);
            draw_rect(pixels, 6, 9, 5, 2, PALETTE[COL_RED], true);
            draw_rect(pixels, 7, 11, 3, 1, PALETTE[COL_RED], true);
            set_pixel(pixels, 8, 12, PALETTE[COL_RED]);
            break;
            
        case SPRITE_HEART_HALF:
            // Half heart
            set_pixel(pixels, 6, 5, PALETTE[COL_RED]);
            set_pixel(pixels, 7, 5, PALETTE[COL_RED]);
            draw_rect(pixels, 5, 6, 3, 3, PALETTE[COL_RED], true);
            draw_rect(pixels, 6, 9, 2, 2, PALETTE[COL_RED], true);
            set_pixel(pixels, 7, 11, PALETTE[COL_RED]);
            set_pixel(pixels, 8, 12, PALETTE[COL_RED]);
            break;
            
        case SPRITE_HEART_EMPTY:
            // Empty heart outline
            set_pixel(pixels, 6, 5, PALETTE[COL_DARK_GRAY]);
            set_pixel(pixels, 7, 5, PALETTE[COL_DARK_GRAY]);
            set_pixel(pixels, 9, 5, PALETTE[COL_DARK_GRAY]);
            set_pixel(pixels, 10, 5, PALETTE[COL_DARK_GRAY]);
            draw_rect(pixels, 5, 6, 7, 3, PALETTE[COL_DARK_GRAY], false);
            draw_line(pixels, 6, 9, 10, 9, PALETTE[COL_DARK_GRAY]);
            draw_line(pixels, 7, 10, 9, 10, PALETTE[COL_DARK_GRAY]);
            set_pixel(pixels, 8, 11, PALETTE[COL_DARK_GRAY]);
            break;
            
        case SPRITE_RUPEE_GREEN:
            // Green rupee
            draw_rect(pixels, 6, 4, 4, 2, PALETTE[COL_GREEN], true);
            draw_rect(pixels, 5, 6, 6, 4, PALETTE[COL_GREEN], true);
            draw_rect(pixels, 6, 10, 4, 2, PALETTE[COL_GREEN], true);
            set_pixel(pixels, 8, 7, PALETTE[COL_WHITE]);
            break;
            
        case SPRITE_KEY:
            // Key
            draw_circle(pixels, 5, 5, 2, PALETTE[COL_YELLOW], true);
            draw_rect(pixels, 7, 5, 6, 2, PALETTE[COL_YELLOW], true);
            set_pixel(pixels, 11, 5, 0x00000000);
            set_pixel(pixels, 13, 5, 0x00000000);
            set_pixel(pixels, 13, 6, 0x00000000);
            break;
            
        case SPRITE_BOMB:
            // Bomb
            draw_circle(pixels, 8, 9, 3, PALETTE[COL_BLACK], true);
            draw_rect(pixels, 7, 5, 2, 3, PALETTE[COL_BROWN], true);
            // Fuse
            set_pixel(pixels, 8, 4, PALETTE[COL_ORANGE]);
            set_pixel(pixels, 8, 3, PALETTE[COL_YELLOW]);
            break;
            
        default:
            // Unknown item - question mark
            draw_rect(pixels, 6, 4, 4, 8, PALETTE[COL_PINK], true);
            set_pixel(pixels, 8, 6, PALETTE[COL_WHITE]);
            set_pixel(pixels, 8, 8, PALETTE[COL_WHITE]);
            set_pixel(pixels, 8, 10, PALETTE[COL_WHITE]);
            break;
    }
}

// ============================================================================
// SPRITE GENERATION - EFFECTS
// ============================================================================

void sprite_generate_effect(u32* pixels, effect_sprite sprite) {
    clear_sprite(pixels, 0x00000000);
    
    switch (sprite) {
        case SPRITE_EXPLOSION_1:
        case SPRITE_EXPLOSION_2:
        case SPRITE_EXPLOSION_3:
        case SPRITE_EXPLOSION_4: {
            // Explosion animation
            int frame = sprite - SPRITE_EXPLOSION_1;
            int radius = 2 + frame * 2;
            u32 color = (frame < 2) ? PALETTE[COL_YELLOW] : PALETTE[COL_ORANGE];
            
            if (frame < 3) {
                draw_circle(pixels, 8, 8, radius, color, false);
                draw_circle(pixels, 8, 8, radius-1, PALETTE[COL_RED], false);
            } else {
                // Dissipating smoke
                set_pixel(pixels, 4, 4, PALETTE[COL_DARK_GRAY]);
                set_pixel(pixels, 12, 4, PALETTE[COL_DARK_GRAY]);
                set_pixel(pixels, 4, 12, PALETTE[COL_DARK_GRAY]);
                set_pixel(pixels, 12, 12, PALETTE[COL_DARK_GRAY]);
            }
            break;
        }
            
        case SPRITE_SLASH_H:
            // Horizontal slash
            draw_line(pixels, 2, 8, 14, 8, PALETTE[COL_WHITE]);
            draw_line(pixels, 3, 7, 13, 7, PALETTE[COL_LIGHT_GRAY]);
            draw_line(pixels, 3, 9, 13, 9, PALETTE[COL_LIGHT_GRAY]);
            break;
            
        case SPRITE_SLASH_V:
            // Vertical slash
            draw_line(pixels, 8, 2, 8, 14, PALETTE[COL_WHITE]);
            draw_line(pixels, 7, 3, 7, 13, PALETTE[COL_LIGHT_GRAY]);
            draw_line(pixels, 9, 3, 9, 13, PALETTE[COL_LIGHT_GRAY]);
            break;
            
        case SPRITE_MAGIC_SPARKLE_1:
        case SPRITE_MAGIC_SPARKLE_2: {
            // Magic sparkle
            int offset = (sprite == SPRITE_MAGIC_SPARKLE_2) ? 1 : 0;
            set_pixel(pixels, 8, 8, PALETTE[COL_WHITE]);
            set_pixel(pixels, 8 - 2 + offset, 8, PALETTE[COL_YELLOW]);
            set_pixel(pixels, 8 + 2 - offset, 8, PALETTE[COL_YELLOW]);
            set_pixel(pixels, 8, 8 - 2 + offset, PALETTE[COL_YELLOW]);
            set_pixel(pixels, 8, 8 + 2 - offset, PALETTE[COL_YELLOW]);
            break;
        }
            
        default:
            break;
    }
}

// ============================================================================
// SPRITE GENERATION - UI
// ============================================================================

void sprite_generate_ui(u32* pixels, u32 sprite_index) {
    clear_sprite(pixels, 0x00000000);
    
    // Simple UI elements
    switch (sprite_index) {
        case 0: // Border top-left
            draw_line(pixels, 0, 0, 15, 0, PALETTE[COL_WHITE]);
            draw_line(pixels, 0, 0, 0, 15, PALETTE[COL_WHITE]);
            break;
            
        case 1: // Border top
            draw_line(pixels, 0, 0, 15, 0, PALETTE[COL_WHITE]);
            break;
            
        case 2: // Border top-right
            draw_line(pixels, 0, 0, 15, 0, PALETTE[COL_WHITE]);
            draw_line(pixels, 15, 0, 15, 15, PALETTE[COL_WHITE]);
            break;
            
        case 3: // Border left
            draw_line(pixels, 0, 0, 0, 15, PALETTE[COL_WHITE]);
            break;
            
        case 4: // Border right
            draw_line(pixels, 15, 0, 15, 15, PALETTE[COL_WHITE]);
            break;
            
        case 5: // Border bottom-left
            draw_line(pixels, 0, 15, 15, 15, PALETTE[COL_WHITE]);
            draw_line(pixels, 0, 0, 0, 15, PALETTE[COL_WHITE]);
            break;
            
        case 6: // Border bottom
            draw_line(pixels, 0, 15, 15, 15, PALETTE[COL_WHITE]);
            break;
            
        case 7: // Border bottom-right
            draw_line(pixels, 0, 15, 15, 15, PALETTE[COL_WHITE]);
            draw_line(pixels, 15, 0, 15, 15, PALETTE[COL_WHITE]);
            break;
            
        default:
            // Fill with UI background color
            clear_sprite(pixels, PALETTE[COL_DARK_BLUE]);
            break;
    }
}

// ============================================================================
// ASSET MANAGER
// ============================================================================

void sprite_assets_init(sprite_assets* assets) {
    memset(assets, 0, sizeof(sprite_assets));
    
    // Allocate pixel buffer for generation
    assets->generator.buffer_size = SHEET_WIDTH * SHEET_HEIGHT * sizeof(u32);
    assets->generator.pixel_buffer = (u32*)malloc(assets->generator.buffer_size);
    
    // Generate all sprite sheets
    sprite_assets_generate_sheets(assets);
    
    // Create default animations
    sprite_assets_create_default_animations(assets);
}

void sprite_assets_shutdown(sprite_assets* assets) {
    if (assets->generator.pixel_buffer) {
        free(assets->generator.pixel_buffer);
        assets->generator.pixel_buffer = NULL;
    }
    
    // TODO: Free GPU textures
}

void sprite_assets_generate_sheets(sprite_assets* assets) {
    // Generate each sprite sheet
    for (int sheet_id = 0; sheet_id < SHEET_COUNT; sheet_id++) {
        sprite_sheet* sheet = &assets->sheets[sheet_id];
        sheet->width = SHEET_WIDTH;
        sheet->height = SHEET_HEIGHT;
        sheet->sprite_width = SPRITE_SIZE;
        sheet->sprite_height = SPRITE_SIZE;
        sheet->sprites_per_row = SPRITES_PER_ROW;
        sheet->total_sprites = (SHEET_WIDTH / SPRITE_SIZE) * (SHEET_HEIGHT / SPRITE_SIZE);
        sheet->is_loaded = true;
        
        // Clear buffer
        memset(assets->generator.pixel_buffer, 0, assets->generator.buffer_size);
        
        // Generate sprites for this sheet
        u32 sprite_pixels[SPRITE_SIZE * SPRITE_SIZE];
        
        for (u32 sprite_idx = 0; sprite_idx < sheet->total_sprites; sprite_idx++) {
            // Calculate position in sheet
            u32 x = (sprite_idx % SPRITES_PER_ROW) * SPRITE_SIZE;
            u32 y = (sprite_idx / SPRITES_PER_ROW) * SPRITE_SIZE;
            
            // Generate sprite based on sheet type
            switch (sheet_id) {
                case SHEET_PLAYER:
                    sprite_generate_player(sprite_pixels, sprite_idx);
                    break;
                    
                case SHEET_ENEMIES:
                    sprite_generate_enemy(sprite_pixels, sprite_idx);
                    break;
                    
                case SHEET_TILES:
                    sprite_generate_tile(sprite_pixels, sprite_idx);
                    break;
                    
                case SHEET_ITEMS:
                    sprite_generate_item(sprite_pixels, sprite_idx);
                    break;
                    
                case SHEET_EFFECTS:
                    sprite_generate_effect(sprite_pixels, sprite_idx);
                    break;
                    
                case SHEET_UI:
                    sprite_generate_ui(sprite_pixels, sprite_idx);
                    break;
            }
            
            // Copy sprite to sheet buffer
            for (u32 sy = 0; sy < SPRITE_SIZE; sy++) {
                for (u32 sx = 0; sx < SPRITE_SIZE; sx++) {
                    u32 sheet_x = x + sx;
                    u32 sheet_y = y + sy;
                    u32 sheet_idx = sheet_y * SHEET_WIDTH + sheet_x;
                    assets->generator.pixel_buffer[sheet_idx] = sprite_pixels[sy * SPRITE_SIZE + sx];
                }
            }
        }
        
        // TODO: Upload to GPU as texture
        // sheet->texture_id = gpu_create_texture(assets->generator.pixel_buffer, 
        //                                        SHEET_WIDTH, SHEET_HEIGHT);
        
        // For now, just store a dummy texture ID
        sheet->texture_id = sheet_id + 1;
        
        // Set sheet name
        switch (sheet_id) {
            case SHEET_PLAYER: strcpy(sheet->name, "Player"); break;
            case SHEET_ENEMIES: strcpy(sheet->name, "Enemies"); break;
            case SHEET_TILES: strcpy(sheet->name, "Tiles"); break;
            case SHEET_ITEMS: strcpy(sheet->name, "Items"); break;
            case SHEET_EFFECTS: strcpy(sheet->name, "Effects"); break;
            case SHEET_UI: strcpy(sheet->name, "UI"); break;
        }
    }
    
    assets->sheet_count = SHEET_COUNT;
}

sprite_sheet* sprite_assets_get_sheet(sprite_assets* assets, sprite_sheet_id id) {
    if (id < assets->sheet_count) {
        return &assets->sheets[id];
    }
    return NULL;
}

// ============================================================================
// ANIMATION SYSTEM
// ============================================================================

sprite_animation* sprite_create_animation(sprite_assets* assets, const char* name) {
    if (assets->animation_count >= MAX_ANIMATIONS) {
        return NULL;
    }
    
    sprite_animation* anim = &assets->animations[assets->animation_count++];
    strcpy(anim->name, name);
    anim->frame_count = 0;
    anim->loop = true;
    anim->total_duration = 0;
    
    return anim;
}

void sprite_animation_add_frame(sprite_animation* anim, u32 sheet_id, 
                               u32 sprite_index, f32 duration) {
    if (anim->frame_count >= MAX_FRAMES_PER_ANIMATION) {
        return;
    }
    
    sprite_frame* frame = &anim->frames[anim->frame_count++];
    frame->sheet_id = sheet_id;
    frame->sprite_index = sprite_index;
    frame->duration = duration;
    
    anim->total_duration += duration;
}

sprite_animation* sprite_get_animation(sprite_assets* assets, const char* name) {
    for (u32 i = 0; i < assets->animation_count; i++) {
        if (strcmp(assets->animations[i].name, name) == 0) {
            return &assets->animations[i];
        }
    }
    return NULL;
}

void animation_play(animation_state* state, sprite_animation* anim) {
    state->current_anim = anim;
    state->current_frame = 0;
    state->frame_timer = 0;
    state->is_playing = true;
    state->playback_speed = 1.0f;
}

void animation_stop(animation_state* state) {
    state->is_playing = false;
}

void animation_update(animation_state* state, f32 dt) {
    if (!state->is_playing || !state->current_anim) {
        return;
    }
    
    state->frame_timer += dt * state->playback_speed;
    
    sprite_frame* frame = &state->current_anim->frames[state->current_frame];
    
    if (state->frame_timer >= frame->duration) {
        state->frame_timer -= frame->duration;
        state->current_frame++;
        
        if (state->current_frame >= state->current_anim->frame_count) {
            if (state->current_anim->loop) {
                state->current_frame = 0;
            } else {
                state->current_frame = state->current_anim->frame_count - 1;
                state->is_playing = false;
            }
        }
    }
}

sprite_frame* animation_get_current_frame(animation_state* state) {
    if (!state->current_anim || state->current_frame >= state->current_anim->frame_count) {
        return NULL;
    }
    
    return &state->current_anim->frames[state->current_frame];
}

// ============================================================================
// UTILITIES
// ============================================================================

rect sprite_get_uv_rect(sprite_sheet* sheet, u32 sprite_index) {
    u32 x = (sprite_index % sheet->sprites_per_row) * sheet->sprite_width;
    u32 y = (sprite_index / sheet->sprites_per_row) * sheet->sprite_height;
    
    f32 u0 = (f32)x / (f32)sheet->width;
    f32 v0 = (f32)y / (f32)sheet->height;
    f32 u1 = (f32)(x + sheet->sprite_width) / (f32)sheet->width;
    f32 v1 = (f32)(y + sheet->sprite_height) / (f32)sheet->height;
    
    rect uv;
    uv.min = v2_make(u0, v0);
    uv.max = v2_make(u1, v1);
    
    return uv;
}

v2 sprite_index_to_coords(u32 sprite_index, u32 sprites_per_row) {
    v2 coords;
    coords.x = (f32)(sprite_index % sprites_per_row);
    coords.y = (f32)(sprite_index / sprites_per_row);
    return coords;
}

// ============================================================================
// DEFAULT ANIMATIONS
// ============================================================================

void sprite_assets_create_default_animations(sprite_assets* assets) {
    // Player walk animations
    sprite_animation* walk_down = sprite_create_animation(assets, "player_walk_down");
    sprite_animation_add_frame(walk_down, SHEET_PLAYER, SPRITE_PLAYER_WALK_DOWN_1, 0.2f);
    sprite_animation_add_frame(walk_down, SHEET_PLAYER, SPRITE_PLAYER_WALK_DOWN_2, 0.2f);
    
    sprite_animation* walk_up = sprite_create_animation(assets, "player_walk_up");
    sprite_animation_add_frame(walk_up, SHEET_PLAYER, SPRITE_PLAYER_WALK_UP_1, 0.2f);
    sprite_animation_add_frame(walk_up, SHEET_PLAYER, SPRITE_PLAYER_WALK_UP_2, 0.2f);
    
    sprite_animation* walk_left = sprite_create_animation(assets, "player_walk_left");
    sprite_animation_add_frame(walk_left, SHEET_PLAYER, SPRITE_PLAYER_WALK_LEFT_1, 0.2f);
    sprite_animation_add_frame(walk_left, SHEET_PLAYER, SPRITE_PLAYER_WALK_LEFT_2, 0.2f);
    
    sprite_animation* walk_right = sprite_create_animation(assets, "player_walk_right");
    sprite_animation_add_frame(walk_right, SHEET_PLAYER, SPRITE_PLAYER_WALK_RIGHT_1, 0.2f);
    sprite_animation_add_frame(walk_right, SHEET_PLAYER, SPRITE_PLAYER_WALK_RIGHT_2, 0.2f);
    
    // Enemy animations
    sprite_animation* slime_idle = sprite_create_animation(assets, "slime_idle");
    sprite_animation_add_frame(slime_idle, SHEET_ENEMIES, SPRITE_SLIME_1, 0.5f);
    sprite_animation_add_frame(slime_idle, SHEET_ENEMIES, SPRITE_SLIME_2, 0.5f);
    
    sprite_animation* bat_fly = sprite_create_animation(assets, "bat_fly");
    sprite_animation_add_frame(bat_fly, SHEET_ENEMIES, SPRITE_BAT_1, 0.1f);
    sprite_animation_add_frame(bat_fly, SHEET_ENEMIES, SPRITE_BAT_2, 0.1f);
    
    // Tile animations
    sprite_animation* water = sprite_create_animation(assets, "water");
    sprite_animation_add_frame(water, SHEET_TILES, SPRITE_WATER_1, 0.5f);
    sprite_animation_add_frame(water, SHEET_TILES, SPRITE_WATER_2, 0.5f);
    
    sprite_animation* lava = sprite_create_animation(assets, "lava");
    sprite_animation_add_frame(lava, SHEET_TILES, SPRITE_LAVA_1, 0.3f);
    sprite_animation_add_frame(lava, SHEET_TILES, SPRITE_LAVA_2, 0.3f);
    
    sprite_animation* torch = sprite_create_animation(assets, "torch");
    sprite_animation_add_frame(torch, SHEET_TILES, SPRITE_TORCH_1, 0.2f);
    sprite_animation_add_frame(torch, SHEET_TILES, SPRITE_TORCH_2, 0.2f);
    
    // Effect animations
    sprite_animation* explosion = sprite_create_animation(assets, "explosion");
    sprite_animation_add_frame(explosion, SHEET_EFFECTS, SPRITE_EXPLOSION_1, 0.1f);
    sprite_animation_add_frame(explosion, SHEET_EFFECTS, SPRITE_EXPLOSION_2, 0.1f);
    sprite_animation_add_frame(explosion, SHEET_EFFECTS, SPRITE_EXPLOSION_3, 0.1f);
    sprite_animation_add_frame(explosion, SHEET_EFFECTS, SPRITE_EXPLOSION_4, 0.1f);
    explosion->loop = false;
    
    sprite_animation* sparkle = sprite_create_animation(assets, "sparkle");
    sprite_animation_add_frame(sparkle, SHEET_EFFECTS, SPRITE_MAGIC_SPARKLE_1, 0.15f);
    sprite_animation_add_frame(sparkle, SHEET_EFFECTS, SPRITE_MAGIC_SPARKLE_2, 0.15f);
}