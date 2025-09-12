# Handmade Engine Asset Browser

## Overview

A complete, functional asset loading and browsing system integrated into the handmade engine editor. Built from scratch with zero external dependencies (except for OS-level file APIs).

## Features Implemented

### 1. File System Integration
- **Directory Scanning**: Uses `dirent.h` to scan filesystem recursively
- **File Metadata**: Tracks file size, modification time, and type
- **Navigation**: Browse folders, navigate up/down directory tree
- **Real-time Updates**: Rescan directories on navigation

### 2. Asset Type Support

#### Textures
- **BMP Format**: Full support with native loader
- **Placeholder Generation**: Creates colored placeholders for unsupported formats
- **OpenGL Integration**: Direct loading to GPU textures
- **Formats Detected**: .png, .jpg, .jpeg, .bmp, .tga (BMP fully supported)

#### Models
- **OBJ Format**: Complete parser for vertices, normals, texture coords, faces
- **Indexed Geometry**: Proper triangle index handling
- **Multiple Objects**: Support for complex OBJ files

#### Sounds
- **WAV Format**: PCM audio loading with header parsing
- **Metadata**: Sample rate, channels, bit depth extraction
- **Memory Management**: Efficient sample buffer allocation

#### Shaders
- **GLSL Detection**: Recognizes .glsl, .vert, .frag files
- **Type Identification**: Proper categorization for shader assets

### 3. Thumbnail System
- **Texture Thumbnails**: Downsampled preview generation
- **Model Thumbnails**: Placeholder icons (extendable to 3D preview)
- **GPU Acceleration**: OpenGL texture-based thumbnails
- **Configurable Size**: Scalable thumbnail dimensions

### 4. GUI Integration
- **Asset Grid View**: Thumbnail grid with names
- **List View**: Detailed list with file sizes
- **Selection System**: Click to select, double-click to load/open
- **Visual Feedback**: Hover effects, selection highlighting
- **Type Coloring**: Visual distinction between asset types

### 5. Editor Integration
- **Panel System**: Fully integrated with editor panel layout
- **Keyboard Navigation**: Arrow keys, Enter to open
- **Mouse Interaction**: Click, double-click, hover
- **Toggle Views**: Switch between thumbnail and list modes

## File Structure

```
handmade-engine/
├── handmade_assets.h        # Asset system interface
├── handmade_assets.c        # Asset loading implementation
├── main.c                   # Editor with integrated asset browser
├── build_minimal.sh         # Build script with asset system
├── create_test_assets.c     # Test asset generator
├── test_asset_browser.sh    # Testing script
└── assets/                  # Asset directory
    ├── textures/           # BMP textures
    ├── models/             # OBJ models
    ├── sounds/             # WAV sounds
    └── shaders/            # GLSL shaders
```

## Building

```bash
# Build the complete editor with asset browser
./build_minimal.sh

# Generate test assets (if needed)
gcc -o create_test_assets create_test_assets.c -lm
./create_test_assets

# Run the editor
./handmade_foundation
```

## Usage

### Controls
- **F1-F4**: Toggle editor panels
- **Arrow Keys**: Navigate assets
- **Enter**: Open folder or load asset
- **Mouse Click**: Select asset
- **Double Click**: Load asset or enter folder
- **ESC**: Exit editor

### Asset Browser Features
1. Navigate directories by double-clicking folders
2. View asset thumbnails in grid mode
3. Switch to list view for detailed information
4. Load textures directly to GPU
5. Parse and load 3D models
6. Support for audio assets

## Technical Details

### Memory Management
- Arena allocators for asset data
- Zero heap allocations in hot paths
- Efficient thumbnail caching
- Proper cleanup on asset unload

### Performance
- Lazy loading: Assets loaded on demand
- Efficient directory scanning
- GPU-accelerated thumbnail rendering
- Minimal memory footprint

### Extensibility
- Easy to add new asset types
- Modular loader architecture
- Plugin-ready design
- Clear separation of concerns

## Implementation Highlights

### Asset Type Detection
```c
AssetType asset_get_type_from_extension(const char *filename);
```
Automatic file type detection based on extension.

### Texture Loading
```c
bool asset_load_texture(Asset *asset);
```
Native BMP loader with OpenGL texture creation.

### Model Parsing
```c
bool asset_load_obj_model(Asset *asset);
```
Complete OBJ file parser with vertex/face support.

### Thumbnail Generation
```c
bool asset_generate_thumbnail(Asset *asset);
```
Automatic thumbnail creation for visual assets.

## Future Enhancements

While fully functional, the system could be extended with:
- PNG/JPG support (via minimal STB integration)
- Async asset loading
- Asset hot-reloading
- Metadata caching
- Search/filter functionality
- Drag-and-drop to scene
- Asset preview window
- Material editing
- Asset importing pipeline

## Philosophy

This asset browser follows the handmade philosophy:
- **No External Dependencies**: Everything built from scratch
- **Full Control**: Complete understanding of every line
- **Performance First**: Optimized for speed
- **Simplicity**: Clear, maintainable code
- **Practical**: Focuses on what games actually need

## Testing

Run the test script to verify functionality:
```bash
./test_asset_browser.sh
```

This will:
1. Verify the build
2. Check asset directory
3. Count available assets
4. Launch the editor with asset browser

## Conclusion

The asset browser is a complete, production-ready system that demonstrates:
- Filesystem interaction
- Multiple file format parsing
- GPU resource management
- GUI integration
- Professional editor features

All built from scratch in pure C with zero external dependencies!