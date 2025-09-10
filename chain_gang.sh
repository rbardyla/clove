#!/bin/bash
# chain_gang.sh - Multiple agents working together until problem is ACTUALLY solved
# This ensures we're working on YOUR codebase, not Claude's assumptions

PROBLEM="$1"
SOLVED=0
ATTEMPTS=0
MAX_ATTEMPTS=50
PROJECT_ROOT="/home/thebackhand/Projects/handmade-engine"

# Ensure we're in the right directory
cd "$PROJECT_ROOT"

# Initial state capture
./update_state.sh

echo "=== CHAIN GANG INITIATED ==="
echo "Problem: $PROBLEM"
echo "Working Directory: $PROJECT_ROOT"
echo "Max Attempts: $MAX_ATTEMPTS"

while [ $SOLVED -eq 0 ] && [ $ATTEMPTS -lt $MAX_ATTEMPTS ]; do
    ATTEMPTS=$((ATTEMPTS + 1))
    echo ""
    echo "=== ATTEMPT $ATTEMPTS/$MAX_ATTEMPTS ==="
    
    # Update context to ensure we see the real codebase
    ./update_state.sh
    
    # Phase 1: Architecture & Analysis
    echo "[1/7] System Architect analyzing problem..."
    timeout 120 claude-code task \
        --subagent system-architect \
        --description "Analyze problem architecture" \
        --prompt "Analyze this problem in context of our ACTUAL codebase (see CURRENT_STATE.md): $PROBLEM. Check existing code patterns, memory layout, and architecture constraints." \
        > .claude/architect_analysis.md 2>&1
    
    # Phase 2: Stubborn Problem Solver - Never gives up
    echo "[2/7] Stubborn Problem Solver attacking the problem..."
    timeout 300 claude-code task \
        --subagent stubborn-problem-solver \
        --description "Solve difficult problem" \
        --prompt "SOLVE THIS COMPLETELY: $PROBLEM. Context from architect: $(cat .claude/architect_analysis.md). You MUST provide working code, not suggestions. Check CURRENT_STATE.md for actual functions." \
        > .claude/solution_attempt.md 2>&1
    
    # Phase 3: Platform Implementation (if needed)
    if grep -q "platform\|win32\|linux\|x11" .claude/solution_attempt.md; then
        echo "[3/7] Platform Layer implementing platform-specific code..."
        timeout 180 claude-code task \
            --subagent platform-layer-implementer \
            --description "Platform implementation" \
            --prompt "Implement platform layer for: $PROBLEM. Based on: $(cat .claude/solution_attempt.md)" \
            > .claude/platform_impl.md 2>&1
    fi
    
    # Phase 4: Code Pessimist - Find all problems
    echo "[4/7] Code Pessimist reviewing for issues..."
    timeout 120 claude-code task \
        --subagent code-pessimist \
        --description "Critical code review" \
        --prompt "Find ALL problems with this solution: $(cat .claude/solution_attempt.md). Be extremely critical. Check against CURRENT_STATE.md for actual codebase state." \
        > .claude/pessimist_review.md 2>&1
    
    # Phase 5: Fix identified issues
    if grep -q "PROBLEM\|CRITICAL\|HIGH" .claude/pessimist_review.md; then
        echo "[5/7] Stubborn Solver fixing identified issues..."
        timeout 300 claude-code task \
            --subagent stubborn-problem-solver \
            --description "Fix all issues" \
            --prompt "FIX ALL THESE ISSUES: $(cat .claude/pessimist_review.md). Original problem: $PROBLEM. You CANNOT give up." \
            > .claude/fixes.md 2>&1
    fi
    
    # Phase 6: Validation
    echo "[6/7] Code Validator checking solution..."
    timeout 120 claude-code task \
        --subagent code-validator \
        --description "Validate solution" \
        --prompt "Validate this solution completely: Problem: $PROBLEM. Check compilation, tests, performance, memory safety. Be thorough." \
        > .claude/validation.log 2>&1
    
    # Phase 7: Actual compilation and test
    echo "[7/7] Running actual validation..."
    
    # Try to compile
    if ./BUILD_ALL.sh debug > .claude/BUILD_OUTPUT.log 2>&1; then
        echo "✓ Compilation successful"
        COMPILE_SUCCESS=1
    else
        echo "✗ Compilation failed"
        COMPILE_SUCCESS=0
        # Extract errors for next iteration
        grep -E "error:|undefined reference" .claude/BUILD_OUTPUT.log | head -20 > .claude/compile_errors.txt
    fi
    
    # Check if tests pass (if compilation succeeded)
    if [ $COMPILE_SUCCESS -eq 1 ]; then
        if timeout 30 ./test_all.sh > .claude/test_output.log 2>&1; then
            echo "✓ Tests passed"
            TEST_SUCCESS=1
        else
            echo "✗ Tests failed"
            TEST_SUCCESS=0
        fi
    else
        TEST_SUCCESS=0
    fi
    
    # Determine if solved
    if [ $COMPILE_SUCCESS -eq 1 ] && [ $TEST_SUCCESS -eq 1 ] && grep -q "PASS" .claude/validation.log; then
        SOLVED=1
        echo ""
        echo "=== ✓ CHAIN GANG SUCCESSFULLY SOLVED THE PROBLEM ==="
        echo "Attempts: $ATTEMPTS"
        echo "Solution validated and working!"
    else
        echo ""
        echo "=== ✗ Not solved yet, preparing next attempt... ==="
        
        # Prepare enhanced problem statement for next iteration
        PROBLEM="$PROBLEM. MUST FIX: "
        
        if [ $COMPILE_SUCCESS -eq 0 ]; then
            PROBLEM="$PROBLEM Compilation errors: $(cat .claude/compile_errors.txt | head -5). "
        fi
        
        if [ $TEST_SUCCESS -eq 0 ]; then
            PROBLEM="$PROBLEM Test failures detected. "
        fi
        
        PROBLEM="$PROBLEM Check CURRENT_STATE.md for actual code. Previous attempt $ATTEMPTS failed."
        
        # Brief pause before retry
        sleep 2
    fi
done

if [ $SOLVED -eq 0 ]; then
    echo ""
    echo "=== ✗ CHAIN GANG EXHAUSTED ==="
    echo "Failed to solve after $MAX_ATTEMPTS attempts"
    echo "Last error state saved in .claude/"
    exit 1
fi

# Clean up temporary files on success
rm -f .claude/*.tmp .claude/architect_analysis.md .claude/solution_attempt.md
echo "Chain gang completed successfully!"