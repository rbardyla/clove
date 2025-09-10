#!/bin/bash
# claude-code-real.sh - Wrapper that FORCES Claude Code to see your actual code
# This prevents hallucinations and ensures Claude works with YOUR codebase

PROJECT_ROOT="/home/thebackhand/Projects/handmade-engine"
cd "$PROJECT_ROOT"

# Update state first - CRITICAL
echo "=== CLAUDE CODE REALITY ENFORCER ==="
./update_state.sh

# Build verification string from actual code
VERIFICATION=$(find . -name "*.c" -o -name "*.h" | grep -v ".claude" | xargs md5sum 2>/dev/null | md5sum | cut -d' ' -f1)
echo "Code verification hash: $VERIFICATION"

# Get all arguments
USER_REQUEST="$@"

# Check if this is an agent task
if [[ "$1" == "task" ]] || [[ "$1" == "agent" ]]; then
    # Pass through to normal claude-code with context
    exec claude-code "$@"
fi

# Create the enforced prompt that includes reality checks
cat > .claude/reality_prompt.md << 'EOF'
# CRITICAL CONTEXT - YOU ARE WORKING ON AN EXISTING CODEBASE

**BEFORE DOING ANYTHING:**
1. Read CURRENT_STATE.md - it contains the ACTUAL state of the codebase
2. All functions you reference MUST exist in the listed headers
3. All structures MUST match those defined in the actual files
4. Build flags and compilation options are documented - USE THEM

**VERIFICATION REQUIREMENTS:**
- Every function call must reference an actual function from CURRENT_STATE.md
- Every struct must match the actual definitions
- Every file path must be real (check Source Files section)
- Memory layout must follow the arena-based system described

**COMMON HALLUCINATION PATTERNS TO AVOID:**
- DO NOT assume standard library functions exist (we use custom implementations)
- DO NOT create new external dependencies (handmade = zero dependencies)
- DO NOT assume certain systems exist without checking systems/ directory
- DO NOT use malloc/free - we use arena allocators exclusively
- DO NOT assume test frameworks - check what actually exists

**YOUR TASK:**
EOF

echo "$USER_REQUEST" >> .claude/reality_prompt.md

cat >> .claude/reality_prompt.md << 'EOF'

**POST-TASK REQUIREMENTS:**
1. Verify your solution compiles with the actual build system
2. Ensure it follows the handmade philosophy (zero dependencies)
3. Check that performance targets are met (60+ FPS)
4. Validate memory usage stays within arena bounds

Remember: You are modifying an EXISTING, WORKING codebase. Every change must integrate with what's already there.
EOF

# Add current state summary
echo "" >> .claude/reality_prompt.md
echo "## Current Codebase Summary" >> .claude/reality_prompt.md
echo "Files: $(find . -name "*.c" -o -name "*.h" | grep -v ".claude" | wc -l)" >> .claude/reality_prompt.md
echo "Systems: $(ls -d systems/*/ 2>/dev/null | wc -l)" >> .claude/reality_prompt.md
echo "Last build: $(grep -m1 "Build" .claude/BUILD_OUTPUT.log 2>/dev/null || echo "Unknown")" >> .claude/reality_prompt.md

# Run Claude Code with enforced context
echo "Executing Claude Code with reality enforcement..."
claude-code \
    --context-dir "$PROJECT_ROOT" \
    --include "*.c,*.h,*.md,*.sh" \
    --exclude ".claude/agents/*" \
    --prompt "$(cat .claude/reality_prompt.md)" \
    2>&1 | tee .claude/last_execution.log

# Post-execution validation
echo ""
echo "=== POST-EXECUTION VALIDATION ==="

# Check if any files were modified
MODIFIED_FILES=$(git status --short | grep -E "^ M " | wc -l)
if [ "$MODIFIED_FILES" -gt 0 ]; then
    echo "Files modified: $MODIFIED_FILES"
    
    # Try to compile
    echo "Attempting compilation..."
    if ./BUILD_ALL.sh debug > .claude/BUILD_OUTPUT.log 2>&1; then
        echo "✓ Code compiles successfully!"
        
        # Run quick test
        if [ -x "./test_all.sh" ]; then
            echo "Running tests..."
            if timeout 10 ./test_all.sh > .claude/test_quick.log 2>&1; then
                echo "✓ Tests pass!"
            else
                echo "✗ Tests failed - reviewing with pessimist agent..."
                
                # Auto-invoke pessimist to find issues
                claude-code task \
                    --subagent code-pessimist \
                    --description "Review test failures" \
                    --prompt "Tests failed after changes. Review the test output and identify issues: $(tail -50 .claude/test_quick.log)"
            fi
        fi
    else
        echo "✗ Compilation failed!"
        echo "Errors:"
        grep -E "error:|undefined" .claude/BUILD_OUTPUT.log | head -10
        
        echo ""
        echo "Auto-fixing compilation errors..."
        
        # Prepare fix prompt
        ERRORS=$(grep -E "error:|undefined" .claude/BUILD_OUTPUT.log | head -20)
        
        # Use stubborn agent to fix
        claude-code task \
            --subagent stubborn-problem-solver \
            --description "Fix compilation errors" \
            --prompt "FIX THESE COMPILATION ERRORS: $ERRORS. Check CURRENT_STATE.md for actual functions. You MUST make it compile."
    fi
else
    echo "No files modified"
fi

echo ""
echo "=== Reality enforcement complete ==="
echo "Verification hash: $VERIFICATION"