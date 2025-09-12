# Current Codebase State
Generated: Wed 10 Sep 2025 11:02:23 AM ADT
Project: Handmade Game Engine

## Git Status
```
?? .claude-code-config.yaml
?? .claude-code-persistence.yaml
?? .claude-code-ultimate.yaml
?? .claude/actual_files.txt
?? .claude/architect_analysis.md
?? .claude/claude_files.txt
?? .claude/claude_functions.txt
?? .claude/compile_errors.txt
?? .claude/compile_test.md
?? .claude/context_attempt.txt
?? .claude/no_context_attempt.txt
?? .claude/pessimist_review.md
?? .claude/reality_prompt.md
?? .claude/solution_attempt.md
?? .claude/test_assumption.md
?? .claude/test_functions.md
?? .claude/test_prompt.md
?? .claude/with_reality.txt
?? .claude/without_reality.txt
?? ALPHA_README.md
?? COMPARE_VERSIONS.sh
?? CURRENT_STATE.md
?? EDITOR_FEATURES.md
?? FOUNDATION_README.md
?? NEURAL_VILLAGE_TECH_DEMO.md
?? RECORD_TECH_DEMO.md
?? RUN_EMERGENT.sh
?? RUN_FIXED.sh
?? RUN_FULL_DEMO.sh
?? RUN_LEARNING_DEMO.sh
?? RUN_SMOOTH.sh
?? TECH_DEMO_PRESENTATION.sh
?? build_alpha.sh
?? build_minimal.sh
?? chain_gang.sh
?? claude-code-real.sh
?? fix_text_and_interaction.c
?? gui_adapter.h
?? gui_simple.c
?? handmade_foundation
?? main.c
?? main_testable.c
?? minimal_renderer.c
?? minimal_renderer.h
?? neural_village_alive
?? neural_village_alive.c
?? neural_village_alpha
?? neural_village_alpha.c
?? neural_village_alpha_dynamic
?? neural_village_alpha_enhanced
?? neural_village_alpha_fixed
?? neural_village_alpha_fixed.c
?? neural_village_alpha_full
?? neural_village_alpha_learning
?? neural_village_alpha_learning.c
?? neural_village_alpha_unique
?? neural_village_alpha_wrapped
?? neural_village_complete
?? neural_village_complete.c
?? neural_village_emergent
?? neural_village_emergent.c
?? neural_village_emergent_smooth
?? neural_village_emergent_smooth.c
?? neural_village_learning.c
?? neural_village_smooth
?? neural_village_smooth.c
?? quest_generator
?? quest_generator.c
?? run_alpha_final.sh
?? run_alpha_unique.sh
?? simple_gui.c
?? simple_gui.h
?? test_alpha_enhanced.sh
?? test_alpha_wrapped.sh
?? test_foundation
?? test_foundation.c
?? test_neural_ai
?? test_neural_ai.c
?? update_state.sh
?? village_economy
?? village_economy.c
?? zelda_ai_evolution
?? zelda_ai_evolution.c
?? zelda_neural_village
?? zelda_neural_village.c
```

## Current Branch
```
main
260d15a working demo
```

## Source Files
### Core Engine Files
- ./handmade_octree.h
- ./main_testable.c
- ./memory_demo.c
- ./game/sprite_assets.h
- ./game/crystal_dungeons.h
- ./game/neural_enemies.c
- ./game/game_standalone.c
- ./game/sprite_assets.c
- ./game/game_types.h
- ./game/game_audio.c
- ./game/main_game.c
- ./game/crystal_dungeons.c
- ./test_neural_simple.c
- ./simple_gui.h
- ./neural_village_emergent_smooth.c
- ./production_platform_resilient.c
- ./neural_village_smooth.c
- ./handmade_hotreload.h
- ./zelda_village.c
- ./professional_editor_demo.c
- ./gui_simple.c
- ./neural_village_alpha_learning.c
- ./neural_village_alive.c
- ./editor_game_demo.c
- ./simple_editor.c
- ./fix_text_and_interaction.c
- ./zelda_ai_evolution.c
- ./editor_main.c
- ./test_optimized_editor.c
- ./zelda_tiles.c
- ./production_editor.c
- ./zelda_simple.c
- ./simple_enhanced_editor.c
- ./gui_adapter.h
- ./resilient_editor.c
- ./quest_generator.c
- ./neural_village_learning.c
- ./handmade_debugger.h
- ./main.c
- ./minimal_renderer.c
- ./build/actual_editor.c
- ./build/platform_main.c
- ./build/physics_test.c
- ./build/unified_editor.c
- ./build/enhanced_editor_unity.c
- ./build/integrated_editor_unity.c
- ./build/resilient_standalone.c
- ./build/production_editor_unity.c
- ./build/unity_build.c
- ./build/simple_enhanced_editor_unity.c

### System Directories
- systems/achievements/ (5 files)
- systems/ai/ (3 files)
- systems/animation/ (1 files)
- systems/audio/ (6 files)
- systems/blueprint/ (7 files)
- systems/editor/ (16 files)
- systems/editor_neural/ (5 files)
- systems/gui/ (7 files)
- systems/hotreload/ (5 files)
- systems/jit/ (11 files)
- systems/network/ (6 files)
- systems/neural/ (10 files)
- systems/nodes/ (9 files)
- systems/particles/ (6 files)
- systems/physics/ (10 files)
- systems/renderer/ (11 files)
- systems/save/ (11 files)
- systems/script/ (8 files)
- systems/settings/ (6 files)
- systems/steam/ (5 files)
- systems/systems/ (0 files)
- systems/vulkan/ (7 files)
- systems/world_gen/ (7 files)

## Public API Functions
### Core Functions

### Platform Functions

## Key Structures
```c
typedef struct EditorGUI EditorGUI;
typedef struct EditorPanel EditorPanel;
typedef struct {
typedef struct {
typedef struct {
typedef struct {
typedef struct { f32 x, y; } v2;
typedef struct { f32 x, y, z; } v3;
typedef struct { f32 x, y, z, w; } v4;
typedef struct {
typedef struct {
typedef struct {
typedef struct gui_context {
typedef struct breakpoint {
typedef struct watch_expression {
typedef struct memory_view {
typedef struct neural_debugger {
typedef struct physics_debugger {
typedef struct entity_debugger {
typedef struct profiler_debugger {
```

## Memory Layout
Arena-based allocation:
- Persistent: Game state, assets
- Transient: Frame data
- Debug: Profiling, visualization

## Last Build Status
```
[3/9] Building GUI Framework...
Building Handmade GUI Framework...
AVX2 support detected, enabling AVX2 optimizations
Compiling GUI demo...
cc1: fatal error: gui_demo.c: No such file or directory
compilation terminated.
cc1: fatal error: handmade_gui.c: No such file or directory
compilation terminated.
cc1: fatal error: handmade_renderer.c: No such file or directory
compilation terminated.
handmade_platform_linux.c:78:13: warning: ‘UpdateInput’ declared ‘static’ but never defined [-Wunused-function]
   78 | static void UpdateInput(void);
      |             ^~~~~~~~~~~
handmade_platform_linux.c:79:14: warning: ‘WorkerThreadProc’ declared ‘static’ but never defined [-Wunused-function]
   79 | static void* WorkerThreadProc(void* param);
      |              ^~~~~~~~~~~~~~~~
handmade_platform_linux.c: In function ‘LinuxShowErrorBox’:
handmade_platform_linux.c:219:5: warning: ignoring return value of ‘system’ declared with attribute ‘warn_unused_result’ [-Wunused-result]
  219 |     system(command);
      |     ^~~~~~~~~~~~~~~
```

## Compilation Flags
```bash
gcc -O3 -march=native -c *.c 2>/dev/null || true
gcc -O3 -march=native -c *.c 2>/dev/null || true
```

## Performance Metrics
Target: 60+ FPS with 10,000 objects
Memory budget: 4GB total

## Recent Compilation Issues
```
cc1: fatal error: gui_demo.c: No such file or directory
cc1: fatal error: handmade_gui.c: No such file or directory
cc1: fatal error: handmade_renderer.c: No such file or directory
```

## Test Results
No recent test results

## Active Tasks
No TODO.md file found

## Code Verification
Checksums of actual source files:
```
./handmade_octree.h: f44d206e9838e063d6b1f5f936e956e9
./main_testable.c: 9798cb36a978dc01393ea7cce24f47d9
./memory_demo.c: fd468f3ecc0eb670a46e0a438a83d347
./game/sprite_assets.h: 9159d72c93f44df04b04b5bc364a85d0
./game/crystal_dungeons.h: e768b3d6202cf28fca5bbe25c7323a3a
./game/neural_enemies.c: 08c0a5fe7fc87d1c81a1e7f4c5431db0
./game/game_standalone.c: 5b73a7bcadd633233272ddbbdcccd9de
./game/sprite_assets.c: 954c548dd3939c09ba896fffff5c1a32
./game/game_types.h: 76fe783ee39a3b440c01dc2f58800f44
./game/game_audio.c: b23fc87c33031609c2da3ac698c45c66
./game/main_game.c: 18a4e5ae021ce346f17cc02db8d9e3a2
./game/crystal_dungeons.c: ffd6cd5ab970b5a8d84d249c0d6d2a50
./test_neural_simple.c: 524b62a1d062bc8ad0a09668b432b37a
./simple_gui.h: 1fb9a5c13944e03715d9cf6a93e331cf
./neural_village_emergent_smooth.c: cab423581e78ac55f15209fb82870742
./production_platform_resilient.c: a95258216cc7f57bfc27c6f6efdbebbf
./neural_village_smooth.c: 9bda8ed69c56b81729b0e7087b0ddaa4
./handmade_hotreload.h: 693ea21bce4022e3f2cab6a630c3a952
./zelda_village.c: 074687008ea641f66ed5d9a60dfe982a
./professional_editor_demo.c: bb9e102c726597a14f56f218ebea82bf
```

## System Capabilities
### Build Scripts
- build_actual_editor.sh
- build_alpha.sh
- build_debug_editor.sh
- build_editor_final.sh
- build_editor.sh
- build_enhanced_editor.sh
- build_game.sh
- build_game_simple.sh
- build_hotreload.sh
- build_integrated_editor.sh

### Binaries
- binaries/demo_ewc
- binaries/dnc_demo
- binaries/handmade_engine
- binaries/handmade_neural
- binaries/lstm_example
- binaries/lstm_simple_test
- binaries/neural_example
- binaries/standalone_neural_debug
- binaries/test_neural

## Platform Configuration
OS: Linux
Architecture: x86_64
CPU: Intel(R) Core(TM) i7-10750H CPU @ 2.60GHz

## Verification Complete
State captured at: 2025-09-10 11:02:23
Use this state to ensure Claude Code works with YOUR actual code, not assumptions.
