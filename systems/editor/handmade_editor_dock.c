// handmade_editor_dock.c - Professional docking system implementation
// PERFORMANCE: Cache-coherent tree traversal, zero allocations per frame

#include "handmade_editor_dock.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

internal dock_node* dock_find_node_at_pos_recursive(dock_node* node, v2 pos);

// ============================================================================
// INTERNAL UTILITIES
// ============================================================================

internal inline rect
rect_make(v2 pos, v2 size)
{
    rect r;
    r.min = pos;
    r.max = v2_add(pos, size);
    return r;
}

internal inline bool
rect_contains_point(rect r, v2 p)
{
    return (p.x >= r.min.x && p.x <= r.max.x && 
            p.y >= r.min.y && p.y <= r.max.y);
}

internal inline v2
rect_get_center(rect r)
{
    v2 center;
    center.x = (r.min.x + r.max.x) * 0.5f;
    center.y = (r.min.y + r.max.y) * 0.5f;
    return center;
}

internal inline f32
lerp(f32 a, f32 b, f32 t)
{
    return a + (b - a) * t;
}

internal inline v2
v2_lerp(v2 a, v2 b, f32 t)
{
    v2 result;
    result.x = lerp(a.x, b.x, t);
    result.y = lerp(a.y, b.y, t);
    return result;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void 
dock_init(dock_manager* dm, gui_context* gui)
{
    memset(dm, 0, sizeof(*dm));
    
    dm->gui = gui;
    
    // Initialize free list
    for (u32 i = 0; i < MAX_DOCK_NODES; ++i) {
        dm->free_node_indices[i] = MAX_DOCK_NODES - 1 - i;
    }
    dm->free_node_count = MAX_DOCK_NODES;
    
    // Default configuration
    dm->min_node_size = DOCK_MIN_SIZE;
    dm->splitter_size = DOCK_SPLITTER_SIZE;
    dm->tab_height = DOCK_TAB_HEIGHT;
    dm->animation_speed = DOCK_ANIMATION_SPEED;
    
    // Default theme colors
    dm->color_splitter = (color32){60, 60, 60, 255};
    dm->color_splitter_hover = (color32){80, 80, 80, 255};
    dm->color_splitter_active = (color32){100, 100, 100, 255};
    dm->color_tab_bg = (color32){45, 45, 45, 255};
    dm->color_tab_active = (color32){60, 60, 60, 255};
    dm->color_tab_hover = (color32){55, 55, 55, 255};
    dm->color_drop_overlay = (color32){100, 150, 200, 100};
    dm->color_window_bg = (color32){35, 35, 35, 255};
}

void 
dock_shutdown(dock_manager* dm)
{
    // Clean up any allocated resources
    // In this handmade approach, we don't have dynamic allocations
    memset(dm, 0, sizeof(*dm));
}

// ============================================================================
// NODE MANAGEMENT
// ============================================================================

dock_node* 
dock_alloc_node(dock_manager* dm)
{
    if (dm->free_node_count == 0) {
        return NULL;
    }
    
    u32 index = dm->free_node_indices[--dm->free_node_count];
    dock_node* node = &dm->nodes[index];
    
    dock_clear_node(node);
    node->id = gui_get_id_int(dm->gui, (i32)index);
    
    dm->node_count++;
    return node;
}

void 
dock_free_node(dock_manager* dm, dock_node* node)
{
    if (!node) return;
    
    u32 index = (u32)(node - dm->nodes);
    if (index >= MAX_DOCK_NODES) return;
    
    dock_clear_node(node);
    dm->free_node_indices[dm->free_node_count++] = index;
    dm->node_count--;
}

void 
dock_clear_node(dock_node* node)
{
    memset(node, 0, sizeof(*node));
}

// ============================================================================
// DOCKSPACE MANAGEMENT
// ============================================================================

void 
dock_begin_dockspace(dock_manager* dm, const char* id, v2 pos, v2 size)
{
    dm->viewport_pos = pos;
    dm->viewport_size = size;
    
    if (!dm->root) {
        dm->root = dock_alloc_node(dm);
        dm->root->pos = pos;
        dm->root->size = size;
        dm->root->content_pos = pos;
        dm->root->content_size = size;
        dm->root->is_visible = true;
    } else {
        // Update root position and size
        dm->root->pos = pos;
        dm->root->size = size;
    }
    
    // Begin timing
    dm->stats.tree_traversal_cycles = __builtin_ia32_rdtsc();
}

void 
dock_end_dockspace(dock_manager* dm)
{
    // Update layout
    dock_calculate_layout_recursive(dm->root);
    
    // Handle input
    v2 mouse_pos = gui_get_mouse_pos(dm->gui);
    bool mouse_down = gui_is_mouse_down(dm->gui, 0);
    bool mouse_clicked = gui_is_mouse_clicked(dm->gui, 0);
    
    dock_handle_input(dm, mouse_pos, mouse_down, mouse_clicked);
    
    // Render
    dock_render(dm);
    
    // Update stats
    dm->stats.tree_traversal_cycles = __builtin_ia32_rdtsc() - dm->stats.tree_traversal_cycles;
}

// ============================================================================
// WINDOW MANAGEMENT
// ============================================================================

dock_window* 
dock_register_window(dock_manager* dm, const char* title, gui_id id)
{
    if (dm->window_count >= ARRAY_COUNT(dm->windows)) {
        return NULL;
    }
    
    dock_window* window = &dm->windows[dm->window_count++];
    window->id = id;
    strncpy(window->title, title, sizeof(window->title) - 1);
    window->undocked_size = (v2){400, 300};
    window->undocked_pos = (v2){100, 100};
    window->is_visible = true;
    
    return window;
}

bool 
dock_begin_window(dock_manager* dm, const char* title, bool* p_open)
{
    dock_window* window = dock_find_window(dm, title);
    if (!window) {
        gui_id id = gui_get_id_str(dm->gui, title);
        window = dock_register_window(dm, title, id);
        if (!window) return false;
    }
    
    if (p_open) {
        window->is_visible = *p_open;
        if (!*p_open) return false;
    }
    
    if (window->is_docked && window->dock_node) {
        // Window is docked - set up content area
        dock_node* node = window->dock_node;
        
        // Account for tab bar
        v2 content_pos = node->content_pos;
        v2 content_size = node->content_size;
        
        if (node->window_count > 1) {
            content_pos.y += dm->tab_height;
            content_size.y -= dm->tab_height;
        }
        
        // Set GUI cursor position for content
        gui_set_cursor_pos(dm->gui, content_pos);
        
        // Check if this window's tab is active
        if (node->windows[node->active_tab] == window) {
            return true;
        }
        return false;
    } else {
        // Window is floating - use regular GUI window
        return gui_begin_window(dm->gui, title, &window->is_visible, 
                              GUI_WINDOW_MOVEABLE | GUI_WINDOW_RESIZABLE);
    }
}

void 
dock_end_window(dock_manager* dm)
{
    // If using docked window, no cleanup needed
    // If floating, end GUI window
    gui_end_window(dm->gui);
}

// ============================================================================
// DOCKING OPERATIONS
// ============================================================================

void 
dock_dock_window(dock_manager* dm, dock_window* window, dock_node* target, dock_drop_zone zone)
{
    if (!window || !target) return;
    
    // Remove from current node if docked
    if (window->is_docked && window->dock_node) {
        dock_node* old_node = window->dock_node;
        
        // Find and remove window
        for (i32 i = 0; i < old_node->window_count; ++i) {
            if (old_node->windows[i] == window) {
                // Shift remaining windows
                for (i32 j = i; j < old_node->window_count - 1; ++j) {
                    old_node->windows[j] = old_node->windows[j + 1];
                }
                old_node->window_count--;
                
                if (old_node->active_tab >= old_node->window_count && old_node->window_count > 0) {
                    old_node->active_tab = old_node->window_count - 1;
                }
                break;
            }
        }
        
        // If old node is empty and not root, remove it
        if (old_node->window_count == 0 && old_node != dm->root) {
            dock_merge_node(dm, old_node);
        }
    }
    
    // Handle different drop zones
    switch (zone) {
        case DOCK_DROP_CENTER:
        case DOCK_DROP_TAB: {
            // Add to target node's tab group
            if (target->window_count < MAX_WINDOWS_PER_DOCK) {
                target->windows[target->window_count++] = window;
                target->active_tab = target->window_count - 1;
                window->dock_node = target;
                window->is_docked = true;
            }
        } break;
        
        case DOCK_DROP_LEFT:
        case DOCK_DROP_RIGHT: {
            dock_node* new_node = dock_split_node(dm, target, DOCK_SPLIT_HORIZONTAL, 0.5f);
            if (new_node) {
                i32 child_index = (zone == DOCK_DROP_LEFT) ? 0 : 1;
                dock_node* child = target->children[child_index];
                child->windows[0] = window;
                child->window_count = 1;
                child->active_tab = 0;
                window->dock_node = child;
                window->is_docked = true;
            }
        } break;
        
        case DOCK_DROP_TOP:
        case DOCK_DROP_BOTTOM: {
            dock_node* new_node = dock_split_node(dm, target, DOCK_SPLIT_VERTICAL, 0.5f);
            if (new_node) {
                i32 child_index = (zone == DOCK_DROP_TOP) ? 0 : 1;
                dock_node* child = target->children[child_index];
                child->windows[0] = window;
                child->window_count = 1;
                child->active_tab = 0;
                window->dock_node = child;
                window->is_docked = true;
            }
        } break;
    }
}

void 
dock_undock_window(dock_manager* dm, dock_window* window)
{
    if (!window || !window->is_docked) return;
    
    dock_node* node = window->dock_node;
    if (!node) return;
    
    // Remove from node
    for (i32 i = 0; i < node->window_count; ++i) {
        if (node->windows[i] == window) {
            // Shift remaining windows
            for (i32 j = i; j < node->window_count - 1; ++j) {
                node->windows[j] = node->windows[j + 1];
            }
            node->window_count--;
            
            if (node->active_tab >= node->window_count && node->window_count > 0) {
                node->active_tab = node->window_count - 1;
            }
            break;
        }
    }
    
    // Clear docking state
    window->is_docked = false;
    window->dock_node = NULL;
    
    // Position floating window at mouse
    v2 mouse_pos = gui_get_mouse_pos(dm->gui);
    window->undocked_pos = mouse_pos;
    
    // Clean up empty node
    if (node->window_count == 0 && node != dm->root) {
        dock_merge_node(dm, node);
    }
}

// ============================================================================
// NODE OPERATIONS
// ============================================================================

dock_node* 
dock_split_node(dock_manager* dm, dock_node* node, dock_split_type split, f32 ratio)
{
    if (!node || node->split_type != DOCK_SPLIT_NONE) {
        return NULL;  // Already split
    }
    
    // Allocate children
    dock_node* child0 = dock_alloc_node(dm);
    dock_node* child1 = dock_alloc_node(dm);
    
    if (!child0 || !child1) {
        if (child0) dock_free_node(dm, child0);
        if (child1) dock_free_node(dm, child1);
        return NULL;
    }
    
    // Move windows to first child
    for (i32 i = 0; i < node->window_count; ++i) {
        child0->windows[i] = node->windows[i];
        node->windows[i]->dock_node = child0;
    }
    child0->window_count = node->window_count;
    child0->active_tab = node->active_tab;
    
    // Clear parent node's windows
    node->window_count = 0;
    node->active_tab = 0;
    
    // Set up split
    node->split_type = split;
    node->split_ratio = ratio;
    node->split_ratio_target = ratio;
    node->children[0] = child0;
    node->children[1] = child1;
    
    child0->parent = node;
    child1->parent = node;
    child0->is_visible = true;
    child1->is_visible = true;
    
    // Calculate initial layout
    dock_calculate_layout_recursive(node);
    
    return node;
}

void 
dock_merge_node(dock_manager* dm, dock_node* node)
{
    if (!node || !node->parent) return;
    
    dock_node* parent = node->parent;
    dock_node* sibling = (parent->children[0] == node) ? 
                        parent->children[1] : parent->children[0];
    
    if (!sibling) return;
    
    // Move sibling's content to parent
    if (sibling->split_type == DOCK_SPLIT_NONE) {
        // Sibling is a leaf - move its windows to parent
        for (i32 i = 0; i < sibling->window_count; ++i) {
            parent->windows[i] = sibling->windows[i];
            sibling->windows[i]->dock_node = parent;
        }
        parent->window_count = sibling->window_count;
        parent->active_tab = sibling->active_tab;
        parent->split_type = DOCK_SPLIT_NONE;
    } else {
        // Sibling has children - adopt them
        parent->split_type = sibling->split_type;
        parent->split_ratio = sibling->split_ratio;
        parent->split_ratio_target = sibling->split_ratio_target;
        parent->children[0] = sibling->children[0];
        parent->children[1] = sibling->children[1];
        
        if (parent->children[0]) parent->children[0]->parent = parent;
        if (parent->children[1]) parent->children[1]->parent = parent;
    }
    
    // Free the nodes
    dock_free_node(dm, node);
    dock_free_node(dm, sibling);
}

// ============================================================================
// LAYOUT CALCULATION
// ============================================================================

void 
dock_calculate_layout_recursive(dock_node* node)
{
    if (!node) return;
    
    node->content_pos = node->pos;
    node->content_size = node->size;
    
    if (node->split_type != DOCK_SPLIT_NONE && node->children[0] && node->children[1]) {
        dock_node* child0 = node->children[0];
        dock_node* child1 = node->children[1];
        
        if (node->split_type == DOCK_SPLIT_HORIZONTAL) {
            // Left/Right split
            f32 split_x = node->pos.x + node->size.x * node->split_ratio;
            
            child0->pos = node->pos;
            child0->size.x = split_x - node->pos.x - DOCK_SPLITTER_SIZE * 0.5f;
            child0->size.y = node->size.y;
            
            child1->pos.x = split_x + DOCK_SPLITTER_SIZE * 0.5f;
            child1->pos.y = node->pos.y;
            child1->size.x = node->pos.x + node->size.x - child1->pos.x;
            child1->size.y = node->size.y;
        } else {
            // Top/Bottom split
            f32 split_y = node->pos.y + node->size.y * node->split_ratio;
            
            child0->pos = node->pos;
            child0->size.x = node->size.x;
            child0->size.y = split_y - node->pos.y - DOCK_SPLITTER_SIZE * 0.5f;
            
            child1->pos.x = node->pos.x;
            child1->pos.y = split_y + DOCK_SPLITTER_SIZE * 0.5f;
            child1->size.x = node->size.x;
            child1->size.y = node->pos.y + node->size.y - child1->pos.y;
        }
        
        // Recursively calculate children
        dock_calculate_layout_recursive(child0);
        dock_calculate_layout_recursive(child1);
    }
}

// ============================================================================
// UPDATE & ANIMATION
// ============================================================================

void 
dock_update_layout(dock_manager* dm, f32 dt)
{
    if (!dm->root) return;
    
    // Update animations
    dock_update_node_recursive(dm, dm->root, dt);
    
    // Animate preview
    dock_animate_preview(&dm->preview, dt, dm->animation_speed);
}

void 
dock_update_node_recursive(dock_manager* dm, dock_node* node, f32 dt)
{
    if (!node) return;
    
    // Animate split ratio
    if (node->split_type != DOCK_SPLIT_NONE) {
        dock_animate_split_ratio(node, dt, dm->animation_speed);
        
        // Update children
        dock_update_node_recursive(dm, node->children[0], dt);
        dock_update_node_recursive(dm, node->children[1], dt);
    }
    
    // Animate tabs
    for (i32 i = 0; i < node->window_count; ++i) {
        dock_window* window = node->windows[i];
        
        // Smooth tab hover animation
        f32 target_hover = (dm->hot_tab == window->id) ? 1.0f : 0.0f;
        window->tab_hover_t = lerp(window->tab_hover_t, target_hover, dt * dm->animation_speed);
        
        // Smooth tab active animation
        f32 target_active = (i == node->active_tab) ? 1.0f : 0.0f;
        window->tab_active_t = lerp(window->tab_active_t, target_active, dt * dm->animation_speed);
    }
}

void 
dock_animate_split_ratio(dock_node* node, f32 dt, f32 speed)
{
    if (fabsf(node->split_ratio - node->split_ratio_target) > 0.001f) {
        node->split_ratio = lerp(node->split_ratio, node->split_ratio_target, dt * speed);
    } else {
        node->split_ratio = node->split_ratio_target;
    }
}

void 
dock_animate_preview(dock_preview* preview, f32 dt, f32 speed)
{
    if (preview->active) {
        preview->alpha = lerp(preview->alpha, preview->alpha_target, dt * speed);
    } else {
        preview->alpha = lerp(preview->alpha, 0.0f, dt * speed * 2.0f);
    }
}

// ============================================================================
// RENDERING
// ============================================================================

void 
dock_render(dock_manager* dm)
{
    if (!dm->root) return;
    
    dm->stats.render_cycles = __builtin_ia32_rdtsc();
    dm->stats.draw_calls = 0;
    dm->stats.windows_rendered = 0;
    
    // Render all nodes recursively
    dock_render_node_recursive(dm, dm->root);
    
    // Render drag preview
    if (dm->preview.active && dm->preview.alpha > 0.01f) {
        color32 overlay_color = dm->color_drop_overlay;
        overlay_color.a = (u8)(overlay_color.a * dm->preview.alpha);
        
        gui_draw_rect_filled(dm->gui, 
                           dm->preview.preview_rect.min,
                           dm->preview.preview_rect.max,
                           overlay_color, 4.0f);
    }
    
    dm->stats.render_cycles = __builtin_ia32_rdtsc() - dm->stats.render_cycles;
}

void 
dock_render_node_recursive(dock_manager* dm, dock_node* node)
{
    if (!node || !node->is_visible) return;
    
    if (node->split_type != DOCK_SPLIT_NONE) {
        // Render splitter
        rect splitter_rect;
        
        if (node->split_type == DOCK_SPLIT_HORIZONTAL) {
            f32 split_x = node->pos.x + node->size.x * node->split_ratio;
            splitter_rect.min = (v2){split_x - dm->splitter_size * 0.5f, node->pos.y};
            splitter_rect.max = (v2){split_x + dm->splitter_size * 0.5f, node->pos.y + node->size.y};
        } else {
            f32 split_y = node->pos.y + node->size.y * node->split_ratio;
            splitter_rect.min = (v2){node->pos.x, split_y - dm->splitter_size * 0.5f};
            splitter_rect.max = (v2){node->pos.x + node->size.x, split_y + dm->splitter_size * 0.5f};
        }
        
        // Determine splitter color based on interaction state
        color32 splitter_color = dm->color_splitter;
        if (dm->resize.is_resizing && dm->resize.resize_node == node) {
            splitter_color = dm->color_splitter_active;
        } else if (dock_is_over_splitter(node, gui_get_mouse_pos(dm->gui), dm->splitter_size)) {
            splitter_color = dm->color_splitter_hover;
        }
        
        gui_draw_rect_filled(dm->gui, splitter_rect.min, splitter_rect.max, splitter_color, 0.0f);
        dm->stats.draw_calls++;
        
        // Render children
        dock_render_node_recursive(dm, node->children[0]);
        dock_render_node_recursive(dm, node->children[1]);
    } else if (node->window_count > 0) {
        // Render window content area background
        gui_draw_rect_filled(dm->gui, node->content_pos, 
                           v2_add(node->content_pos, node->content_size),
                           dm->color_window_bg, 0.0f);
        dm->stats.draw_calls++;
        
        // Render tab bar if multiple windows
        if (node->window_count > 1) {
            dock_render_tab_bar(dm, node);
        }
        
        dm->stats.windows_rendered += node->window_count;
    }
}

void 
dock_render_tab_bar(dock_manager* dm, dock_node* node)
{
    v2 tab_bar_pos = node->content_pos;
    v2 tab_bar_size = (v2){node->content_size.x, dm->tab_height};
    
    // Background
    gui_draw_rect_filled(dm->gui, tab_bar_pos, 
                       v2_add(tab_bar_pos, tab_bar_size),
                       dm->color_tab_bg, 0.0f);
    dm->stats.draw_calls++;
    
    // Calculate tab width
    f32 max_tab_width = 150.0f;
    f32 tab_width = fminf(max_tab_width, tab_bar_size.x / (f32)node->window_count);
    
    // Render each tab
    for (i32 i = 0; i < node->window_count; ++i) {
        dock_window* window = node->windows[i];
        
        rect tab_rect;
        tab_rect.min.x = tab_bar_pos.x + i * tab_width;
        tab_rect.min.y = tab_bar_pos.y;
        tab_rect.max.x = tab_rect.min.x + tab_width - 1.0f;
        tab_rect.max.y = tab_bar_pos.y + dm->tab_height;
        
        bool is_active = (i == node->active_tab);
        bool is_hover = rect_contains_point(tab_rect, gui_get_mouse_pos(dm->gui));
        
        dock_render_tab(dm, window, tab_rect, is_active, is_hover);
    }
}

void 
dock_render_tab(dock_manager* dm, dock_window* window, rect tab_rect, bool is_active, bool is_hover)
{
    // Interpolate tab color based on state
    color32 tab_color = dm->color_tab_bg;
    
    if (is_active) {
        tab_color = dm->color_tab_active;
    } else if (is_hover || window->tab_hover_t > 0.01f) {
        // Blend between normal and hover color
        f32 t = window->tab_hover_t;
        tab_color.r = (u8)lerp(dm->color_tab_bg.r, dm->color_tab_hover.r, t);
        tab_color.g = (u8)lerp(dm->color_tab_bg.g, dm->color_tab_hover.g, t);
        tab_color.b = (u8)lerp(dm->color_tab_bg.b, dm->color_tab_hover.b, t);
    }
    
    // Draw tab background
    gui_draw_rect_filled(dm->gui, tab_rect.min, tab_rect.max, tab_color, 2.0f);
    
    // Draw active indicator
    if (window->tab_active_t > 0.01f) {
        rect indicator_rect;
        indicator_rect.min.x = tab_rect.min.x;
        indicator_rect.min.y = tab_rect.max.y - 2.0f;
        indicator_rect.max.x = tab_rect.max.x;
        indicator_rect.max.y = tab_rect.max.y;
        
        color32 indicator_color;
        indicator_color.r = 100;
        indicator_color.g = 150;
        indicator_color.b = 200;
        indicator_color.a = (u8)(255 * window->tab_active_t);
        gui_draw_rect_filled(dm->gui, indicator_rect.min, indicator_rect.max, indicator_color, 0.0f);
    }
    
    // Draw tab text
    v2 text_pos;
    text_pos.x = tab_rect.min.x + 8.0f;
    text_pos.y = tab_rect.min.y + (dm->tab_height - 16.0f) * 0.5f;
    
    color32 text_color;
    text_color.r = 220;
    text_color.g = 220;
    text_color.b = 220;
    text_color.a = 255;
    gui_draw_text(dm->gui, text_pos, text_color, 
                 window->title, window->title + strlen(window->title));
    
    dm->stats.draw_calls += 3;
}

// ============================================================================
// INPUT HANDLING
// ============================================================================

void 
dock_handle_input(dock_manager* dm, v2 mouse_pos, bool mouse_down, bool mouse_clicked)
{
    // Handle resize
    if (dm->resize.is_resizing) {
        if (mouse_down) {
            // Continue resizing
            dock_node* node = dm->resize.resize_node;
            
            if (node->split_type == DOCK_SPLIT_HORIZONTAL) {
                f32 new_ratio = (mouse_pos.x - node->pos.x) / node->size.x;
                node->split_ratio_target = fmaxf(0.1f, fminf(0.9f, new_ratio));
            } else {
                f32 new_ratio = (mouse_pos.y - node->pos.y) / node->size.y;
                node->split_ratio_target = fmaxf(0.1f, fminf(0.9f, new_ratio));
            }
        } else {
            // End resize
            dm->resize.is_resizing = false;
            dm->resize.resize_node = NULL;
        }
        return;
    }
    
    // Check for splitter hover/click
    dock_node* hover_splitter_node = NULL;
    
    // Simple traversal to find splitter under mouse
    // We'll do this manually instead of using a callback
    if (dm->root) {
        dock_node* queue[MAX_DOCK_NODES];
        u32 head = 0;
        u32 tail = 0;
        
        queue[tail++] = dm->root;
        
        while (head < tail && !hover_splitter_node) {
            dock_node* node = queue[head++];
            
            if (node->split_type != DOCK_SPLIT_NONE && 
                dock_is_over_splitter(node, mouse_pos, dm->splitter_size)) {
                hover_splitter_node = node;
                break;
            }
            
            if (node->split_type != DOCK_SPLIT_NONE) {
                if (node->children[0]) queue[tail++] = node->children[0];
                if (node->children[1]) queue[tail++] = node->children[1];
            }
        }
    }
    
    if (hover_splitter_node && mouse_clicked) {
        // Start resize
        dm->resize.is_resizing = true;
        dm->resize.resize_node = hover_splitter_node;
        dm->resize.resize_start_pos = mouse_pos;
        dm->resize.resize_start_ratio = hover_splitter_node->split_ratio;
    }
    
    // Handle tab clicks
    dock_node* clicked_node = dock_find_node_at_pos(dm, mouse_pos);
    if (clicked_node && clicked_node->window_count > 1) {
        v2 tab_bar_pos = clicked_node->content_pos;
        v2 tab_bar_size;
        tab_bar_size.x = clicked_node->content_size.x;
        tab_bar_size.y = dm->tab_height;
        rect tab_bar_rect = rect_make(tab_bar_pos, tab_bar_size);
        
        if (rect_contains_point(tab_bar_rect, mouse_pos) && mouse_clicked) {
            f32 max_tab_width = 150.0f;
            f32 tab_width = fminf(max_tab_width, clicked_node->content_size.x / (f32)clicked_node->window_count);
            
            i32 clicked_tab = (i32)((mouse_pos.x - tab_bar_pos.x) / tab_width);
            if (clicked_tab >= 0 && clicked_tab < clicked_node->window_count) {
                clicked_node->active_tab = clicked_tab;
            }
        }
    }
}

bool 
dock_is_over_splitter(dock_node* node, v2 mouse_pos, f32 threshold)
{
    if (!node || node->split_type == DOCK_SPLIT_NONE) return false;
    
    if (node->split_type == DOCK_SPLIT_HORIZONTAL) {
        f32 split_x = node->pos.x + node->size.x * node->split_ratio;
        return fabsf(mouse_pos.x - split_x) <= threshold && 
               mouse_pos.y >= node->pos.y && 
               mouse_pos.y <= node->pos.y + node->size.y;
    } else {
        f32 split_y = node->pos.y + node->size.y * node->split_ratio;
        return fabsf(mouse_pos.y - split_y) <= threshold && 
               mouse_pos.x >= node->pos.x && 
               mouse_pos.x <= node->pos.x + node->size.x;
    }
}

// ============================================================================
// QUERIES
// ============================================================================

dock_node* 
dock_find_node_at_pos(dock_manager* dm, v2 pos)
{
    if (!dm->root) return NULL;
    
    // Recursive search
    return dock_find_node_at_pos_recursive(dm->root, pos);
}

internal dock_node*
dock_find_node_at_pos_recursive(dock_node* node, v2 pos)
{
    if (!node) return NULL;
    
    rect node_rect = rect_make(node->pos, node->size);
    if (!rect_contains_point(node_rect, pos)) return NULL;
    
    if (node->split_type != DOCK_SPLIT_NONE) {
        // Check children
        dock_node* result = dock_find_node_at_pos_recursive(node->children[0], pos);
        if (result) return result;
        
        result = dock_find_node_at_pos_recursive(node->children[1], pos);
        if (result) return result;
    }
    
    return node;
}

dock_window* 
dock_find_window(dock_manager* dm, const char* title)
{
    for (u32 i = 0; i < dm->window_count; ++i) {
        if (strcmp(dm->windows[i].title, title) == 0) {
            return &dm->windows[i];
        }
    }
    return NULL;
}

dock_drop_zone 
dock_get_drop_zone(dock_node* node, v2 mouse_pos)
{
    if (!node) return DOCK_DROP_NONE;
    
    rect node_rect = rect_make(node->pos, node->size);
    if (!rect_contains_point(node_rect, mouse_pos)) return DOCK_DROP_NONE;
    
    v2 center = rect_get_center(node_rect);
    v2 relative;
    relative.x = (mouse_pos.x - center.x) / (node->size.x * 0.5f);
    relative.y = (mouse_pos.y - center.y) / (node->size.y * 0.5f);
    
    // Central area for tab drop
    if (fabsf(relative.x) < 0.3f && fabsf(relative.y) < 0.3f) {
        return DOCK_DROP_TAB;
    }
    
    // Edge areas for split
    if (fabsf(relative.x) > fabsf(relative.y)) {
        return (relative.x < 0) ? DOCK_DROP_LEFT : DOCK_DROP_RIGHT;
    } else {
        return (relative.y < 0) ? DOCK_DROP_TOP : DOCK_DROP_BOTTOM;
    }
}

// ============================================================================
// TRAVERSAL
// ============================================================================

void 
dock_traverse_breadth_first(dock_manager* dm, dock_node_visitor visitor, void* user_data)
{
    if (!dm->root || !visitor) return;
    
    // Queue for breadth-first traversal
    dock_node* queue[MAX_DOCK_NODES];
    u32 head = 0;
    u32 tail = 0;
    
    queue[tail++] = dm->root;
    
    while (head < tail) {
        dock_node* node = queue[head++];
        
        visitor(node, user_data);
        dm->stats.nodes_traversed++;
        
        if (node->split_type != DOCK_SPLIT_NONE) {
            if (node->children[0]) queue[tail++] = node->children[0];
            if (node->children[1]) queue[tail++] = node->children[1];
        }
    }
}

// ============================================================================
// LAYOUT PRESETS
// ============================================================================

void 
dock_apply_preset_default(dock_manager* dm)
{
    // Clear existing layout
    if (dm->root) {
        // TODO: Properly clean up all nodes
        memset(dm->nodes, 0, sizeof(dm->nodes));
        dm->node_count = 0;
        dm->free_node_count = MAX_DOCK_NODES;
        for (u32 i = 0; i < MAX_DOCK_NODES; ++i) {
            dm->free_node_indices[i] = MAX_DOCK_NODES - 1 - i;
        }
    }
    
    // Create default layout
    dm->root = dock_alloc_node(dm);
    dm->root->pos = dm->viewport_pos;
    dm->root->size = dm->viewport_size;
    dm->root->is_visible = true;
    
    // Split horizontally: left (tools) | right (main)
    dock_split_node(dm, dm->root, DOCK_SPLIT_HORIZONTAL, 0.2f);
    
    // Split right side vertically: top (viewport) | bottom (assets)
    dock_split_node(dm, dm->root->children[1], DOCK_SPLIT_VERTICAL, 0.7f);
    
    // Register and dock default windows
    dock_window* hierarchy = dock_register_window(dm, "Hierarchy", gui_get_id_str(dm->gui, "Hierarchy"));
    dock_window* inspector = dock_register_window(dm, "Inspector", gui_get_id_str(dm->gui, "Inspector"));
    dock_window* viewport = dock_register_window(dm, "Viewport", gui_get_id_str(dm->gui, "Viewport"));
    dock_window* assets = dock_register_window(dm, "Assets", gui_get_id_str(dm->gui, "Assets"));
    dock_window* console = dock_register_window(dm, "Console", gui_get_id_str(dm->gui, "Console"));
    
    // Dock windows
    if (hierarchy) dock_dock_window(dm, hierarchy, dm->root->children[0], DOCK_DROP_TAB);
    if (inspector) dock_dock_window(dm, inspector, dm->root->children[0], DOCK_DROP_TAB);
    if (viewport) dock_dock_window(dm, viewport, dm->root->children[1]->children[0], DOCK_DROP_TAB);
    if (assets) dock_dock_window(dm, assets, dm->root->children[1]->children[1], DOCK_DROP_TAB);
    if (console) dock_dock_window(dm, console, dm->root->children[1]->children[1], DOCK_DROP_TAB);
}

void 
dock_apply_preset_code(dock_manager* dm)
{
    // Code editing layout: large center area with side panels
    // TODO: Implement code-focused layout
}

void 
dock_apply_preset_art(dock_manager* dm)
{
    // Art/level design layout: large viewport with tool panels
    // TODO: Implement art-focused layout
}

void 
dock_apply_preset_debug(dock_manager* dm)
{
    // Debug layout: profiler, console, memory viewer
    // TODO: Implement debug-focused layout
}