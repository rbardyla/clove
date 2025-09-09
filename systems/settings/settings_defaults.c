/*
    Default Settings Registration
    Registers all standard game settings with sensible defaults
*/

#include "handmade_settings.h"

#define internal static

// Video/Display settings
void
settings_register_video_settings(settings_system *system) {
    // Display resolution options
    char *resolution_options[] = {
        "1920x1080", "2560x1440", "3840x2160",
        "1366x768", "1600x900", "1280x720"
    };
    settings_register_enum(system, "resolution", "Display resolution", 
                          CATEGORY_VIDEO, resolution_options, 6, 0, 0);
    
    settings_register_bool(system, "fullscreen", "Enable fullscreen mode",
                          CATEGORY_VIDEO, 0, 0);
    
    settings_register_bool(system, "borderless", "Borderless windowed mode",
                          CATEGORY_VIDEO, 0, 0);
    
    settings_register_bool(system, "vsync", "Vertical synchronization",
                          CATEGORY_VIDEO, 1, 0);
    
    settings_register_int(system, "refresh_rate", "Monitor refresh rate (Hz)",
                         CATEGORY_VIDEO, 60, 30, 240, 0);
    
    settings_register_int(system, "fov", "Field of view (degrees)",
                         CATEGORY_VIDEO, 90, 60, 120, 0);
    
    // UI settings
    settings_register_float(system, "ui_scale", "UI scale factor",
                           CATEGORY_VIDEO, 1.0f, 0.5f, 2.0f, 0);
    
    settings_register_int(system, "brightness", "Display brightness",
                         CATEGORY_VIDEO, 50, 0, 100, 0);
    
    settings_register_int(system, "contrast", "Display contrast",
                         CATEGORY_VIDEO, 50, 0, 100, 0);
    
    settings_register_float(system, "gamma", "Gamma correction",
                           CATEGORY_VIDEO, 2.2f, 1.0f, 3.0f, SETTING_ADVANCED);
}

// Graphics quality settings
void
settings_register_graphics_settings(settings_system *system) {
    // Quality presets
    char *quality_presets[] = {
        "Low", "Medium", "High", "Ultra", "Custom"
    };
    settings_register_enum(system, "graphics_preset", "Graphics quality preset",
                          CATEGORY_GRAPHICS, quality_presets, 5, 2, 0);
    
    // Individual quality settings
    char *texture_quality[] = {"Low", "Medium", "High", "Ultra"};
    settings_register_enum(system, "texture_quality", "Texture quality",
                          CATEGORY_GRAPHICS, texture_quality, 4, 2, 0);
    
    char *shadow_quality[] = {"Off", "Low", "Medium", "High", "Ultra"};
    settings_register_enum(system, "shadow_quality", "Shadow quality",
                          CATEGORY_GRAPHICS, shadow_quality, 5, 2, 0);
    
    char *aa_options[] = {"Off", "FXAA", "MSAA 2x", "MSAA 4x", "MSAA 8x", "TAA"};
    settings_register_enum(system, "antialiasing", "Anti-aliasing",
                          CATEGORY_GRAPHICS, aa_options, 6, 1, 0);
    
    settings_register_int(system, "anisotropic_filtering", "Anisotropic filtering",
                         CATEGORY_GRAPHICS, 8, 1, 16, 0);
    
    settings_register_float(system, "render_scale", "Render scale",
                           CATEGORY_GRAPHICS, 1.0f, 0.5f, 2.0f, SETTING_ADVANCED);
    
    // Effects
    settings_register_bool(system, "bloom", "Bloom effect",
                          CATEGORY_GRAPHICS, 1, 0);
    
    settings_register_bool(system, "motion_blur", "Motion blur",
                          CATEGORY_GRAPHICS, 0, 0);
    
    settings_register_bool(system, "depth_of_field", "Depth of field",
                          CATEGORY_GRAPHICS, 1, 0);
    
    settings_register_bool(system, "screen_space_reflections", "Screen space reflections",
                          CATEGORY_GRAPHICS, 1, SETTING_ADVANCED);
    
    settings_register_bool(system, "ambient_occlusion", "Ambient occlusion",
                          CATEGORY_GRAPHICS, 1, 0);
    
    // Performance
    settings_register_int(system, "max_fps", "Maximum FPS limit",
                         CATEGORY_GRAPHICS, 0, 0, 300, 0); // 0 = unlimited
    
    settings_register_bool(system, "reduce_input_lag", "Reduce input lag",
                          CATEGORY_GRAPHICS, 0, SETTING_ADVANCED);
}

// Audio settings
void
settings_register_audio_settings(settings_system *system) {
    // Volume controls
    settings_register_float(system, "master_volume", "Master volume",
                           CATEGORY_AUDIO, 1.0f, 0.0f, 1.0f, 0);
    
    settings_register_float(system, "music_volume", "Music volume",
                           CATEGORY_AUDIO, 0.7f, 0.0f, 1.0f, 0);
    
    settings_register_float(system, "sfx_volume", "Sound effects volume",
                           CATEGORY_AUDIO, 0.8f, 0.0f, 1.0f, 0);
    
    settings_register_float(system, "voice_volume", "Voice/dialogue volume",
                           CATEGORY_AUDIO, 0.9f, 0.0f, 1.0f, 0);
    
    settings_register_float(system, "ui_volume", "UI sounds volume",
                           CATEGORY_AUDIO, 0.6f, 0.0f, 1.0f, 0);
    
    // Audio quality
    char *audio_quality[] = {"Low", "Medium", "High", "Lossless"};
    settings_register_enum(system, "audio_quality", "Audio quality",
                          CATEGORY_AUDIO, audio_quality, 4, 2, 0);
    
    char *audio_device[] = {"Default", "Speakers", "Headphones", "USB Audio"};
    settings_register_enum(system, "audio_device", "Audio output device",
                          CATEGORY_AUDIO, audio_device, 4, 0, 0);
    
    // Spatial audio
    settings_register_bool(system, "3d_audio", "3D positional audio",
                          CATEGORY_AUDIO, 1, 0);
    
    settings_register_bool(system, "surround_sound", "Surround sound (5.1/7.1)",
                          CATEGORY_AUDIO, 0, 0);
    
    settings_register_bool(system, "reverb", "Environmental reverb",
                          CATEGORY_AUDIO, 1, 0);
    
    // Advanced audio
    settings_register_int(system, "audio_buffer_size", "Audio buffer size",
                         CATEGORY_AUDIO, 1024, 256, 4096, SETTING_ADVANCED);
    
    settings_register_int(system, "sample_rate", "Audio sample rate (Hz)",
                         CATEGORY_AUDIO, 44100, 22050, 96000, SETTING_ADVANCED);
}

// Control settings
void
settings_register_control_settings(settings_system *system) {
    // Mouse settings
    settings_register_float(system, "mouse_sensitivity", "Mouse sensitivity",
                           CATEGORY_CONTROLS, 1.0f, 0.1f, 5.0f, 0);
    
    settings_register_bool(system, "mouse_invert_y", "Invert mouse Y-axis",
                          CATEGORY_CONTROLS, 0, 0);
    
    settings_register_bool(system, "mouse_acceleration", "Mouse acceleration",
                          CATEGORY_CONTROLS, 0, 0);
    
    settings_register_bool(system, "raw_mouse_input", "Raw mouse input",
                          CATEGORY_CONTROLS, 1, SETTING_ADVANCED);
    
    // Keyboard settings
    settings_register_int(system, "key_repeat_delay", "Key repeat delay (ms)",
                         CATEGORY_CONTROLS, 250, 100, 1000, SETTING_ADVANCED);
    
    settings_register_int(system, "key_repeat_rate", "Key repeat rate (Hz)",
                         CATEGORY_CONTROLS, 30, 10, 100, SETTING_ADVANCED);
    
    // Controller settings
    settings_register_bool(system, "controller_enabled", "Enable controller support",
                          CATEGORY_CONTROLS, 1, 0);
    
    settings_register_float(system, "controller_deadzone", "Controller analog deadzone",
                           CATEGORY_CONTROLS, 0.15f, 0.0f, 0.5f, 0);
    
    settings_register_bool(system, "controller_vibration", "Controller vibration",
                          CATEGORY_CONTROLS, 1, 0);
    
    // Key bindings (would be registered as hotkeys)
    settings_register_string(system, "key_forward", "Move forward key",
                            CATEGORY_CONTROLS, "W", SETTING_HOTKEY);
    
    settings_register_string(system, "key_backward", "Move backward key",
                            CATEGORY_CONTROLS, "S", SETTING_HOTKEY);
    
    settings_register_string(system, "key_left", "Move left key",
                            CATEGORY_CONTROLS, "A", SETTING_HOTKEY);
    
    settings_register_string(system, "key_right", "Move right key",
                            CATEGORY_CONTROLS, "D", SETTING_HOTKEY);
    
    settings_register_string(system, "key_jump", "Jump key",
                            CATEGORY_CONTROLS, "Space", SETTING_HOTKEY);
    
    settings_register_string(system, "key_crouch", "Crouch key",
                            CATEGORY_CONTROLS, "Ctrl", SETTING_HOTKEY);
    
    settings_register_string(system, "key_sprint", "Sprint key",
                            CATEGORY_CONTROLS, "Shift", SETTING_HOTKEY);
    
    settings_register_string(system, "key_interact", "Interact key",
                            CATEGORY_CONTROLS, "E", SETTING_HOTKEY);
    
    settings_register_string(system, "key_inventory", "Open inventory key",
                            CATEGORY_CONTROLS, "I", SETTING_HOTKEY);
    
    settings_register_string(system, "key_map", "Open map key",
                            CATEGORY_CONTROLS, "M", SETTING_HOTKEY);
    
    settings_register_string(system, "key_pause", "Pause/menu key",
                            CATEGORY_CONTROLS, "Escape", SETTING_HOTKEY);
}

// Gameplay settings
void
settings_register_gameplay_settings(settings_system *system) {
    // Difficulty
    char *difficulty_levels[] = {"Easy", "Normal", "Hard", "Expert"};
    settings_register_enum(system, "difficulty", "Game difficulty",
                          CATEGORY_GAMEPLAY, difficulty_levels, 4, 1, 0);
    
    // UI/HUD options
    settings_register_bool(system, "show_hud", "Show HUD elements",
                          CATEGORY_GAMEPLAY, 1, 0);
    
    settings_register_bool(system, "show_minimap", "Show minimap",
                          CATEGORY_GAMEPLAY, 1, 0);
    
    settings_register_bool(system, "show_crosshair", "Show crosshair",
                          CATEGORY_GAMEPLAY, 1, 0);
    
    settings_register_bool(system, "show_damage_numbers", "Show damage numbers",
                          CATEGORY_GAMEPLAY, 1, 0);
    
    settings_register_bool(system, "show_tooltips", "Show item tooltips",
                          CATEGORY_GAMEPLAY, 1, 0);
    
    // Auto-save settings
    settings_register_bool(system, "autosave_enabled", "Enable auto-save",
                          CATEGORY_GAMEPLAY, 1, 0);
    
    settings_register_int(system, "autosave_interval", "Auto-save interval (minutes)",
                         CATEGORY_GAMEPLAY, 5, 1, 30, 0);
    
    // Accessibility
    settings_register_bool(system, "colorblind_support", "Colorblind accessibility",
                          CATEGORY_GAMEPLAY, 0, 0);
    
    settings_register_bool(system, "subtitles", "Show subtitles",
                          CATEGORY_GAMEPLAY, 0, 0);
    
    settings_register_float(system, "subtitle_size", "Subtitle text size",
                           CATEGORY_GAMEPLAY, 1.0f, 0.5f, 2.0f, 0);
    
    // Performance gameplay options
    settings_register_int(system, "max_entities", "Maximum entities on screen",
                         CATEGORY_GAMEPLAY, 1000, 100, 5000, SETTING_ADVANCED);
    
    settings_register_float(system, "lod_distance", "Level of detail distance",
                           CATEGORY_GAMEPLAY, 1.0f, 0.1f, 2.0f, SETTING_ADVANCED);
}

// Network/Multiplayer settings
void
settings_register_network_settings(settings_system *system) {
    settings_register_string(system, "player_name", "Player name",
                            CATEGORY_NETWORK, "Player", 0);
    
    settings_register_int(system, "network_port", "Network port",
                         CATEGORY_NETWORK, 7777, 1024, 65535, SETTING_ADVANCED);
    
    settings_register_int(system, "max_ping", "Maximum ping (ms)",
                         CATEGORY_NETWORK, 150, 50, 500, 0);
    
    settings_register_bool(system, "show_ping", "Show network ping",
                          CATEGORY_NETWORK, 0, 0);
    
    settings_register_bool(system, "auto_connect", "Auto-connect to servers",
                          CATEGORY_NETWORK, 0, 0);
    
    char *server_regions[] = {"Auto", "US East", "US West", "Europe", "Asia"};
    settings_register_enum(system, "preferred_region", "Preferred server region",
                          CATEGORY_NETWORK, server_regions, 5, 0, 0);
}

// Debug settings (development only)
void
settings_register_debug_settings(settings_system *system) {
    settings_register_bool(system, "show_fps", "Show FPS counter",
                          CATEGORY_DEBUG, 0, 0);
    
    settings_register_bool(system, "show_debug_info", "Show debug information",
                          CATEGORY_DEBUG, 0, SETTING_ADVANCED);
    
    settings_register_bool(system, "wireframe_mode", "Wireframe rendering",
                          CATEGORY_DEBUG, 0, SETTING_ADVANCED);
    
    settings_register_bool(system, "collision_debug", "Show collision shapes",
                          CATEGORY_DEBUG, 0, SETTING_ADVANCED);
    
    settings_register_bool(system, "ai_debug", "Show AI debug info",
                          CATEGORY_DEBUG, 0, SETTING_ADVANCED);
    
    settings_register_int(system, "log_level", "Debug log level",
                         CATEGORY_DEBUG, 2, 0, 4, SETTING_ADVANCED);
    
    settings_register_bool(system, "console_enabled", "Enable debug console",
                          CATEGORY_DEBUG, 0, SETTING_ADVANCED);
}

// Accessibility settings
void
settings_register_accessibility_settings(settings_system *system) {
    settings_register_float(system, "text_scale", "Text size scale",
                           CATEGORY_ACCESSIBILITY, 1.0f, 0.5f, 2.0f, 0);
    
    settings_register_bool(system, "high_contrast", "High contrast mode",
                          CATEGORY_ACCESSIBILITY, 0, 0);
    
    settings_register_bool(system, "reduce_motion", "Reduce motion effects",
                          CATEGORY_ACCESSIBILITY, 0, 0);
    
    char *colorblind_types[] = {"None", "Protanopia", "Deuteranopia", "Tritanopia"};
    settings_register_enum(system, "colorblind_type", "Colorblind type",
                          CATEGORY_ACCESSIBILITY, colorblind_types, 4, 0, 0);
    
    settings_register_bool(system, "audio_cues", "Enhanced audio cues",
                          CATEGORY_ACCESSIBILITY, 0, 0);
    
    settings_register_bool(system, "closed_captions", "Closed captions",
                          CATEGORY_ACCESSIBILITY, 0, 0);
    
    settings_register_float(system, "ui_animation_speed", "UI animation speed",
                           CATEGORY_ACCESSIBILITY, 1.0f, 0.0f, 2.0f, 0);
    
    settings_register_bool(system, "one_button_mode", "Single button mode",
                          CATEGORY_ACCESSIBILITY, 0, SETTING_ADVANCED);
}

// Register all default settings
void
settings_register_all_defaults(settings_system *system) {
    settings_register_video_settings(system);
    settings_register_graphics_settings(system);
    settings_register_audio_settings(system);
    settings_register_control_settings(system);
    settings_register_gameplay_settings(system);
    settings_register_network_settings(system);
    settings_register_debug_settings(system);
    settings_register_accessibility_settings(system);
}