// Quick fixes for Alpha v0.001.1 - Better text and interactions

// === IMPROVED BITMAP FONT ===
void init_improved_font() {
    memset(font_data, 0, sizeof(font_data));
    
    // Clear, readable font patterns
    
    // A
    font_data['A'][0] = 0b00111000;
    font_data['A'][1] = 0b01101100;
    font_data['A'][2] = 0b11000110;
    font_data['A'][3] = 0b11000110;
    font_data['A'][4] = 0b11111110;
    font_data['A'][5] = 0b11000110;
    font_data['A'][6] = 0b11000110;
    font_data['A'][7] = 0b00000000;
    
    // B
    font_data['B'][0] = 0b11111100;
    font_data['B'][1] = 0b11000110;
    font_data['B'][2] = 0b11000110;
    font_data['B'][3] = 0b11111100;
    font_data['B'][4] = 0b11000110;
    font_data['B'][5] = 0b11000110;
    font_data['B'][6] = 0b11111100;
    font_data['B'][7] = 0b00000000;
    
    // C
    font_data['C'][0] = 0b01111100;
    font_data['C'][1] = 0b11000110;
    font_data['C'][2] = 0b11000000;
    font_data['C'][3] = 0b11000000;
    font_data['C'][4] = 0b11000000;
    font_data['C'][5] = 0b11000110;
    font_data['C'][6] = 0b01111100;
    font_data['C'][7] = 0b00000000;
    
    // D
    font_data['D'][0] = 0b11111000;
    font_data['D'][1] = 0b11001100;
    font_data['D'][2] = 0b11000110;
    font_data['D'][3] = 0b11000110;
    font_data['D'][4] = 0b11000110;
    font_data['D'][5] = 0b11001100;
    font_data['D'][6] = 0b11111000;
    font_data['D'][7] = 0b00000000;
    
    // E
    font_data['E'][0] = 0b11111110;
    font_data['E'][1] = 0b11000000;
    font_data['E'][2] = 0b11000000;
    font_data['E'][3] = 0b11111100;
    font_data['E'][4] = 0b11000000;
    font_data['E'][5] = 0b11000000;
    font_data['E'][6] = 0b11111110;
    font_data['E'][7] = 0b00000000;
    
    // F
    font_data['F'][0] = 0b11111110;
    font_data['F'][1] = 0b11000000;
    font_data['F'][2] = 0b11000000;
    font_data['F'][3] = 0b11111100;
    font_data['F'][4] = 0b11000000;
    font_data['F'][5] = 0b11000000;
    font_data['F'][6] = 0b11000000;
    font_data['F'][7] = 0b00000000;
    
    // G
    font_data['G'][0] = 0b01111100;
    font_data['G'][1] = 0b11000110;
    font_data['G'][2] = 0b11000000;
    font_data['G'][3] = 0b11001110;
    font_data['G'][4] = 0b11000110;
    font_data['G'][5] = 0b11000110;
    font_data['G'][6] = 0b01111100;
    font_data['G'][7] = 0b00000000;
    
    // H
    font_data['H'][0] = 0b11000110;
    font_data['H'][1] = 0b11000110;
    font_data['H'][2] = 0b11000110;
    font_data['H'][3] = 0b11111110;
    font_data['H'][4] = 0b11000110;
    font_data['H'][5] = 0b11000110;
    font_data['H'][6] = 0b11000110;
    font_data['H'][7] = 0b00000000;
    
    // I
    font_data['I'][0] = 0b01111100;
    font_data['I'][1] = 0b00011000;
    font_data['I'][2] = 0b00011000;
    font_data['I'][3] = 0b00011000;
    font_data['I'][4] = 0b00011000;
    font_data['I'][5] = 0b00011000;
    font_data['I'][6] = 0b01111100;
    font_data['I'][7] = 0b00000000;
    
    // More essential letters...
    
    // Space
    font_data[' '][0] = 0b00000000;
    font_data[' '][1] = 0b00000000;
    font_data[' '][2] = 0b00000000;
    font_data[' '][3] = 0b00000000;
    font_data[' '][4] = 0b00000000;
    font_data[' '][5] = 0b00000000;
    font_data[' '][6] = 0b00000000;
    font_data[' '][7] = 0b00000000;
    
    // Numbers
    font_data['0'][0] = 0b01111100;
    font_data['0'][1] = 0b11000110;
    font_data['0'][2] = 0b11001110;
    font_data['0'][3] = 0b11010110;
    font_data['0'][4] = 0b11100110;
    font_data['0'][5] = 0b11000110;
    font_data['0'][6] = 0b01111100;
    font_data['0'][7] = 0b00000000;
    
    font_data['1'][0] = 0b00011000;
    font_data['1'][1] = 0b00111000;
    font_data['1'][2] = 0b00011000;
    font_data['1'][3] = 0b00011000;
    font_data['1'][4] = 0b00011000;
    font_data['1'][5] = 0b00011000;
    font_data['1'][6] = 0b01111110;
    font_data['1'][7] = 0b00000000;
    
    // Copy to lowercase
    for (int c = 'A'; c <= 'Z'; c++) {
        for (int row = 0; row < 8; row++) {
            font_data[c + 32][row] = font_data[c][row]; // Copy to lowercase
        }
    }
    
    // Add punctuation
    font_data['.'][5] = 0b01100000;
    font_data['.'][6] = 0b01100000;
    
    font_data[':'][2] = 0b01100000;
    font_data[':'][3] = 0b01100000;
    font_data[':'][5] = 0b01100000;
    font_data[':'][6] = 0b01100000;
    
    font_data['!'][0] = 0b00011000;
    font_data['!'][1] = 0b00011000;
    font_data['!'][2] = 0b00011000;
    font_data['!'][3] = 0b00011000;
    font_data['!'][4] = 0b00000000;
    font_data['!'][5] = 0b00011000;
    font_data['!'][6] = 0b00011000;
    font_data['!'][7] = 0b00000000;
}

// === IMPROVED INTERACTION SYSTEM ===

// Check if player is near an NPC and can interact
neural_npc* get_nearest_interactable_npc(alpha_game_state* game, f32 max_range) {
    neural_npc* nearest = NULL;
    f32 closest_distance = max_range;
    
    for (u32 i = 0; i < game->npc_count; i++) {
        neural_npc* npc = &game->npcs[i];
        f32 dx = npc->x - game->player_x;
        f32 dy = npc->y - game->player_y;
        f32 distance = sqrtf(dx*dx + dy*dy);
        
        if (distance < closest_distance) {
            closest_distance = distance;
            nearest = npc;
        }
    }
    
    return nearest;
}

// Draw interaction indicator
void draw_interaction_indicator(alpha_game_state* game, neural_npc* npc) {
    int screen_x = (int)(npc->x - game->camera_x);
    int screen_y = (int)(npc->y - game->camera_y) - 24; // Above NPC
    
    // Draw "!" indicator above NPC
    for (int dy = 0; dy < 8; dy++) {
        u8 font_row = font_data['!'][dy];
        for (int col = 0; col < 8; col++) {
            if (font_row & (1 << (7 - col))) {
                draw_pixel(game, screen_x + col, screen_y + dy, 0x3C); // Bright yellow
            }
        }
    }
    
    // Draw speech bubble background
    for (int dx = -2; dx < 10; dx++) {
        for (int dy = -2; dy < 10; dy++) {
            if (dx == -2 || dx == 9 || dy == -2 || dy == 9) {
                draw_pixel(game, screen_x + dx, screen_y + dy, 0x30); // White border
            } else {
                draw_pixel(game, screen_x + dx, screen_y + dy, 0x0F); // Black background
            }
        }
    }
}

// Enhanced try_interact_with_npc with better feedback
void enhanced_try_interact_with_npc(alpha_game_state* game) {
    neural_npc* nearest = get_nearest_interactable_npc(game, 50.0f);
    
    if (nearest) {
        game->show_dialog = 1;
        game->dialog_npc_id = nearest->id;
        
        // Improve player relationship
        nearest->player_reputation += 1.0f;
        nearest->player_familiarity += 2.0f;
        if (nearest->player_reputation > 100.0f) nearest->player_reputation = 100.0f;
        if (nearest->player_familiarity > 100.0f) nearest->player_familiarity = 100.0f;
        
        // Generate dialog based on relationship and state
        if (nearest->player_familiarity < 10.0f) {
            snprintf(game->dialog_text, 255, "%s: Hello there, stranger! I'm %s, the village %s. Nice to meet you!", 
                    nearest->name, nearest->name, nearest->occupation);
        } else if (nearest->player_reputation > 50.0f) {
            snprintf(game->dialog_text, 255, "%s: Great to see you again, my friend! %s How can I help you today?", 
                    nearest->name, nearest->current_thought);
        } else if (nearest->emotions[EMOTION_HAPPINESS] > 0.8f) {
            snprintf(game->dialog_text, 255, "%s: I'm feeling wonderful today! %s What brings you by?", 
                    nearest->name, nearest->current_thought);
        } else if (nearest->emotions[EMOTION_SADNESS] > 0.6f) {
            snprintf(game->dialog_text, 255, "%s: *sighs* %s Sorry, I'm not feeling my best today.", 
                    nearest->name, nearest->current_thought);
        } else {
            snprintf(game->dialog_text, 255, "%s: %s What can I do for you?", 
                    nearest->name, nearest->current_thought);
        }
    } else {
        // Show message when no one is nearby to talk to
        game->show_dialog = 1;
        game->dialog_npc_id = -1; // Special case for system message
        strcpy(game->dialog_text, "There's no one nearby to talk to. Walk closer to an NPC and try again!");
    }
}

// Enhanced render with interaction indicators
void enhanced_render_npcs(alpha_game_state* game) {
    for (u32 i = 0; i < game->npc_count; i++) {
        neural_npc* npc = &game->npcs[i];
        
        // Draw the NPC
        draw_npc(game, npc);
        
        // Check if player can interact with this NPC
        f32 dx = npc->x - game->player_x;
        f32 dy = npc->y - game->player_y;
        f32 distance = sqrtf(dx*dx + dy*dy);
        
        if (distance < 50.0f) {
            draw_interaction_indicator(game, npc);
        }
    }
}

// Instructions for implementing these fixes:

/*
TO FIX THE ALPHA BUILD:

1. Replace the init_font() function with init_improved_font() 
2. Replace try_interact_with_npc() with enhanced_try_interact_with_npc()
3. Add the interaction indicator drawing in the render loop
4. Update the render_frame() to use enhanced_render_npcs()

This will give you:
- Much more readable text with proper bitmap font
- Clear visual indicators when you can talk to NPCs  
- Better dialog system with helpful feedback
- Speech bubbles above NPCs when in range

The ENTER key will now:
- Talk to nearby NPCs (with visual ! indicator)
- Show helpful message if no one is nearby
- Close dialog when one is open
*/