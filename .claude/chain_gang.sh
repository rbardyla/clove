#!/bin/bash
# chain_gang.sh - Multiple agents working until success

PROBLEM="$1"
SOLVED=0
GANG_SIZE=5

while [ $SOLVED -eq 0 ]; do
    echo "=== CHAIN GANG WORKING ON: $PROBLEM ==="
    
    # Agent 1: Architect
    claude-code --agent architect "Break down: $PROBLEM" > breakdown.md
    
    # Agent 2: Implementer  
    claude-code --agent implementer --context breakdown.md "Implement: $PROBLEM" > implementation.c
    
    # Agent 3: Debugger
    claude-code --agent debugger --context implementation.c "Debug and fix: $PROBLEM" > fixed.c
    
    # Agent 4: Optimizer
    claude-code --agent optimizer --context fixed.c "Optimize: $PROBLEM" > optimized.c
    
    # Agent 5: Validator
    claude-code --agent validator --context optimized.c "Validate: $PROBLEM" > validation.log
    
    # Check if solved
    if grep -q "VALIDATION PASSED" validation.log; then
        SOLVED=1
        echo "✓ CHAIN GANG SOLVED THE PROBLEM"
    else
        echo "✗ Not solved yet, continuing..."
        PROBLEM="Fix issues in validation.log and solve: $PROBLEM"
    fi
done