#!/bin/bash
# update_state.sh - Captures current codebase state to prevent Claude hallucinations
# Run this BEFORE every Claude Code invocation to ensure it sees YOUR code

PROJECT_ROOT="/home/thebackhand/Projects/handmade-engine"
STATE_FILE="$PROJECT_ROOT/CURRENT_STATE.md"

echo "=== UPDATING CODEBASE STATE ===" 

# Header
echo "# Current Codebase State" > "$STATE_FILE"
echo "Generated: $(date)" >> "$STATE_FILE"
echo "Project: Handmade Game Engine" >> "$STATE_FILE"
echo "" >> "$STATE_FILE"

# Git status
echo "## Git Status" >> "$STATE_FILE"
echo '```' >> "$STATE_FILE"
git status --short >> "$STATE_FILE" 2>&1
echo '```' >> "$STATE_FILE"
echo "" >> "$STATE_FILE"

# Current branch and last commit
echo "## Current Branch" >> "$STATE_FILE"
echo '```' >> "$STATE_FILE"
git branch --show-current >> "$STATE_FILE" 2>&1
git log -1 --oneline >> "$STATE_FILE" 2>&1
echo '```' >> "$STATE_FILE"
echo "" >> "$STATE_FILE"

# Existing source files
echo "## Source Files" >> "$STATE_FILE"
echo "### Core Engine Files" >> "$STATE_FILE"
find . -name "*.c" -o -name "*.h" -type f | grep -v "\.claude" | head -50 | while read -r file; do
    echo "- $file" >> "$STATE_FILE"
done
echo "" >> "$STATE_FILE"

# System directories
echo "### System Directories" >> "$STATE_FILE"
if [ -d "systems" ]; then
    ls -d systems/*/ 2>/dev/null | while read -r dir; do
        echo "- $dir ($(ls "$dir"/*.c "$dir"/*.h 2>/dev/null | wc -l) files)" >> "$STATE_FILE"
    done
fi
echo "" >> "$STATE_FILE"

# Existing Functions (from headers)
echo "## Public API Functions" >> "$STATE_FILE"
echo "### Core Functions" >> "$STATE_FILE"
if [ -f "game.h" ]; then
    grep -E "^[a-z_]+.*\(" game.h 2>/dev/null | sed 's/^/- /' >> "$STATE_FILE"
fi
if [ -f "handmade.h" ]; then
    grep -E "^[a-z_]+.*\(" handmade.h 2>/dev/null | sed 's/^/- /' >> "$STATE_FILE"
fi
echo "" >> "$STATE_FILE"

# Platform functions
echo "### Platform Functions" >> "$STATE_FILE"
if [ -f "handmade_platform.h" ]; then
    grep -E "^platform_" handmade_platform.h 2>/dev/null | sed 's/^/- /' >> "$STATE_FILE"
fi
echo "" >> "$STATE_FILE"

# Struct definitions
echo "## Key Structures" >> "$STATE_FILE"
echo '```c' >> "$STATE_FILE"
grep -h "^typedef struct" *.h 2>/dev/null | head -20 >> "$STATE_FILE"
echo '```' >> "$STATE_FILE"
echo "" >> "$STATE_FILE"

# Memory layout (if available)
echo "## Memory Layout" >> "$STATE_FILE"
if [ -f "MEMORY_LAYOUT.md" ]; then
    cat MEMORY_LAYOUT.md >> "$STATE_FILE"
else
    echo "Arena-based allocation:" >> "$STATE_FILE"
    echo "- Persistent: Game state, assets" >> "$STATE_FILE"
    echo "- Transient: Frame data" >> "$STATE_FILE"
    echo "- Debug: Profiling, visualization" >> "$STATE_FILE"
fi
echo "" >> "$STATE_FILE"

# Build status
echo "## Last Build Status" >> "$STATE_FILE"
if [ -f ".claude/BUILD_OUTPUT.log" ]; then
    echo '```' >> "$STATE_FILE"
    tail -20 .claude/BUILD_OUTPUT.log >> "$STATE_FILE" 2>&1
    echo '```' >> "$STATE_FILE"
else
    echo "No recent build output" >> "$STATE_FILE"
fi
echo "" >> "$STATE_FILE"

# Compilation flags
echo "## Compilation Flags" >> "$STATE_FILE"
echo '```bash' >> "$STATE_FILE"
if [ -f "BUILD_ALL.sh" ]; then
    grep -E "gcc|clang" BUILD_ALL.sh | head -5 >> "$STATE_FILE"
else
    echo "-O3 -march=native -mavx2 -mfma -ffast-math" >> "$STATE_FILE"
    echo "-Wall -Wextra -Wno-unused-parameter" >> "$STATE_FILE"
fi
echo '```' >> "$STATE_FILE"
echo "" >> "$STATE_FILE"

# Performance metrics (if available)
echo "## Performance Metrics" >> "$STATE_FILE"
if [ -x "binaries/perf_test" ]; then
    echo "Running quick performance check..." >> "$STATE_FILE"
    timeout 2 ./binaries/perf_test --quick 2>/dev/null | head -10 >> "$STATE_FILE"
else
    echo "Target: 60+ FPS with 10,000 objects" >> "$STATE_FILE"
    echo "Memory budget: 4GB total" >> "$STATE_FILE"
fi
echo "" >> "$STATE_FILE"

# Recent errors/warnings
echo "## Recent Compilation Issues" >> "$STATE_FILE"
if [ -f ".claude/compile_errors.txt" ]; then
    echo '```' >> "$STATE_FILE"
    cat .claude/compile_errors.txt >> "$STATE_FILE"
    echo '```' >> "$STATE_FILE"
else
    echo "No recent compilation errors" >> "$STATE_FILE"
fi
echo "" >> "$STATE_FILE"

# Test results
echo "## Test Results" >> "$STATE_FILE"
if [ -f ".claude/test_output.log" ]; then
    echo '```' >> "$STATE_FILE"
    grep -E "PASS|FAIL|Error" .claude/test_output.log | tail -10 >> "$STATE_FILE"
    echo '```' >> "$STATE_FILE"
else
    echo "No recent test results" >> "$STATE_FILE"
fi
echo "" >> "$STATE_FILE"

# Active TODOs
echo "## Active Tasks" >> "$STATE_FILE"
if [ -f "TODO.md" ]; then
    head -10 TODO.md >> "$STATE_FILE"
else
    echo "No TODO.md file found" >> "$STATE_FILE"
fi
echo "" >> "$STATE_FILE"

# IMPORTANT: Checksum of actual code
echo "## Code Verification" >> "$STATE_FILE"
echo "Checksums of actual source files:" >> "$STATE_FILE"
echo '```' >> "$STATE_FILE"
find . -name "*.c" -o -name "*.h" | grep -v ".claude" | head -20 | while read -r file; do
    if [ -f "$file" ]; then
        checksum=$(md5sum "$file" 2>/dev/null | cut -d' ' -f1)
        echo "$file: $checksum" >> "$STATE_FILE"
    fi
done
echo '```' >> "$STATE_FILE"
echo "" >> "$STATE_FILE"

# System capabilities check
echo "## System Capabilities" >> "$STATE_FILE"
echo "### Build Scripts" >> "$STATE_FILE"
ls *.sh 2>/dev/null | grep -E "build|test" | head -10 | while read -r script; do
    echo "- $script" >> "$STATE_FILE"
done
echo "" >> "$STATE_FILE"

echo "### Binaries" >> "$STATE_FILE"
if [ -d "binaries" ]; then
    ls binaries/ 2>/dev/null | head -10 | while read -r binary; do
        echo "- binaries/$binary" >> "$STATE_FILE"
    done
fi
echo "" >> "$STATE_FILE"

# Platform detection
echo "## Platform Configuration" >> "$STATE_FILE"
echo "OS: $(uname -s)" >> "$STATE_FILE"
echo "Architecture: $(uname -m)" >> "$STATE_FILE"
echo "CPU: $(grep -m1 "model name" /proc/cpuinfo 2>/dev/null | cut -d: -f2 | xargs)" >> "$STATE_FILE"
echo "" >> "$STATE_FILE"

# Final verification
echo "## Verification Complete" >> "$STATE_FILE"
echo "State captured at: $(date '+%Y-%m-%d %H:%M:%S')" >> "$STATE_FILE"
echo "Use this state to ensure Claude Code works with YOUR actual code, not assumptions." >> "$STATE_FILE"

echo "✓ Context state updated in $STATE_FILE"
echo "  Claude Code will now see your actual codebase state"