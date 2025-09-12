#ifndef DEBUG_OVERLAY_COMPLETE_H
#define DEBUG_OVERLAY_COMPLETE_H

// Complete debug overlay API - no stubs, fully functional

// Initialize overlay with target FPS (e.g., 60.0)
void debug_overlay_init(float target_fps);

// Call at start of frame
void debug_overlay_begin_frame(void);

// Call at end of frame
void debug_overlay_end_frame(void);

// Track rendering events
void debug_overlay_draw_call(int triangles);
void debug_overlay_state_change(void);

// Render the overlay (call last in frame)
void debug_overlay_render(int viewport_width, int viewport_height);

// User controls
void debug_overlay_toggle(void);        // F1
void debug_overlay_toggle_graphs(void); // F2
void debug_overlay_toggle_hints(void);  // F3

#endif // DEBUG_OVERLAY_COMPLETE_H