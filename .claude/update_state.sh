#!/bin/bash
# update_state.sh - Run this BEFORE every Claude Code invocation

echo "=== UPDATING CONTEXT STATE ===" 

# 1. Generate current codebase snapshot
echo "## Current Codebase State" > CURRENT_STATE.md
echo "Generated: $(date)" >> CURRENT_STATE.md
echo "" >> CURRENT_STATE.md

# 2. List all actual functions
echo "### Existing Functions" >> CURRENT_STATE.md
ctags -x --c-kinds=f src/*.c | awk '{print "- "$1" ("$2":"$3")"}' >> CURRENT_STATE.md

# 3. Current struct definitions
echo "### Existing Structures" >> CURRENT_STATE.md
grep -h "^typedef struct" src/*.h >> CURRENT_STATE.md

# 4. Current build status
echo "### Last Build Result" >> CURRENT_STATE.md
tail -n 50 BUILD_OUTPUT.log >> CURRENT_STATE.md

# 5. Memory usage reality
echo "### Actual Memory Usage" >> CURRENT_STATE.md
./build/editor --dump-memory-stats >> CURRENT_STATE.md

# 6. Performance reality
echo "### Current Performance" >> CURRENT_STATE.md
./build/editor --quick-benchmark >> CURRENT_STATE.md

echo "Context state updated. Claude Code will now see reality."