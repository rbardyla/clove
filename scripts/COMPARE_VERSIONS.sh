#!/bin/bash
# COMPARE_VERSIONS.sh - Compare Claude's assumptions vs actual codebase reality

echo "=== CLAUDE ASSUMPTION VS REALITY CHECKER ==="
echo ""

# Test 1: Check if Claude knows what files actually exist
echo "[TEST 1] File Existence Check"
echo "Asking Claude what files exist..."

# Create a test prompt
cat > .claude/test_assumption.md << 'EOF'
List the main source files in this project. Just list filenames, no explanation.
EOF

# Get Claude's assumption
claude-code --prompt "$(cat .claude/test_assumption.md)" > .claude/claude_files.txt 2>&1

# Get actual files
find . -name "*.c" -o -name "*.h" | grep -v ".claude" | sort > .claude/actual_files.txt

echo "Claude thinks these files exist:"
grep -E "\.(c|h)" .claude/claude_files.txt | head -10

echo ""
echo "Actually existing files:"
head -10 .claude/actual_files.txt

echo ""
HALLUCINATED=$(grep -E "\.(c|h)" .claude/claude_files.txt | while read -r file; do
    if ! [ -f "$file" ]; then
        echo "  ✗ HALLUCINATED: $file"
    fi
done)

if [ -n "$HALLUCINATED" ]; then
    echo "Claude hallucinated these files:"
    echo "$HALLUCINATED"
else
    echo "✓ No hallucinated files detected"
fi

echo ""
echo "----------------------------------------"
echo ""

# Test 2: Function existence check
echo "[TEST 2] Function Existence Check"
echo "Asking Claude about main functions..."

cat > .claude/test_functions.md << 'EOF'
What are the main initialization functions in this codebase? List function names only.
EOF

claude-code --prompt "$(cat .claude/test_functions.md)" > .claude/claude_functions.txt 2>&1

echo "Claude thinks these functions exist:"
grep -E "^[a-z_]+.*\(" .claude/claude_functions.txt | head -10

echo ""
echo "Actually existing functions (from headers):"
grep -h -E "^[a-z_]+.*\(" *.h 2>/dev/null | head -10

echo ""
echo "----------------------------------------"
echo ""

# Test 3: Compare with reality-enforced version
echo "[TEST 3] Reality Enforcement Comparison"
echo ""

# Run same prompt with reality wrapper
echo "Running with reality enforcement..."
./update_state.sh > /dev/null 2>&1

cat > .claude/test_prompt.md << 'EOF'
Write a simple function that initializes the game engine. Include actual function calls from this codebase.
EOF

echo "Without reality enforcement:"
claude-code --prompt "$(cat .claude/test_prompt.md)" > .claude/without_reality.txt 2>&1
echo "Response preview:"
head -20 .claude/without_reality.txt

echo ""
echo "With reality enforcement:"
./claude-code-real.sh "$(cat .claude/test_prompt.md)" > .claude/with_reality.txt 2>&1
echo "Response preview:"
head -20 .claude/with_reality.txt

echo ""
echo "----------------------------------------"
echo ""

# Test 4: Compilation test
echo "[TEST 4] Compilation Success Rate"
echo ""

# Create a test task
cat > .claude/compile_test.md << 'EOF'
Add a simple debug logging function to the codebase that prints frame timing.
EOF

echo "Asking Claude to add code without context..."
claude-code --prompt "$(cat .claude/compile_test.md)" > .claude/no_context_attempt.txt 2>&1

# Check if it compiles
echo "Testing compilation of Claude's code..."
if ./BUILD_ALL.sh debug > .claude/compile_test.log 2>&1; then
    echo "✓ Code compiled successfully!"
else
    echo "✗ Compilation failed"
    echo "Errors:"
    grep -E "error:" .claude/compile_test.log | head -5
fi

# Reset any changes
git checkout -- . 2>/dev/null

echo ""
echo "Now with reality enforcement..."
./claude-code-real.sh "$(cat .claude/compile_test.md)" > .claude/context_attempt.txt 2>&1

if ./BUILD_ALL.sh debug > .claude/compile_test2.log 2>&1; then
    echo "✓ Reality-enforced code compiled successfully!"
else
    echo "✗ Reality-enforced compilation also failed"
    echo "Errors:"
    grep -E "error:" .claude/compile_test2.log | head -5
fi

# Reset changes
git checkout -- . 2>/dev/null

echo ""
echo "=== COMPARISON COMPLETE ==="
echo ""
echo "Summary:"
echo "1. File hallucination check: Complete"
echo "2. Function assumption check: Complete" 
echo "3. Reality enforcement: Demonstrated"
echo "4. Compilation success: Tested"
echo ""
echo "Use ./claude-code-real.sh instead of claude-code for production work!"
echo "Use ./chain_gang.sh for difficult problems that need persistence!"