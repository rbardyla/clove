#!/bin/bash
# Wrapper that FORCES Claude Code to see your actual code

# Update state first
./update_state.sh

# Build verification string from actual code
VERIFICATION=$(md5sum src/*.{c,h} | md5sum | cut -d' ' -f1)

# Create the enforced prompt
cat > .claude-force-prompt.tmp << EOF
CRITICAL: You are working on an EXISTING codebase.
Verification hash: $VERIFICATION

BEFORE WRITING ANY CODE:
1. Check CURRENT_STATE.md for what actually exists
2. Verify all functions you call exist in the headers
3. Use the EXACT struct definitions from the files
4. Match the ACTUAL coding style in the codebase

YOU MUST REFERENCE:
$(ls -la src/*.h | awk '{print "- "$9}')

CURRENT BUILD ERRORS TO FIX:
$(grep -E "error:|warning:" BUILD_OUTPUT.log | head -10)

ACTUAL MEMORY LAYOUT:
$(cat MEMORY_LAYOUT.md)

Now execute the user's request using ONLY existing code patterns:
$@
EOF

# Run Claude Code with forced context
claude-code \
  --context-dir . \
  --include "*.c,*.h,*.md" \
  --prompt-file .claude-force-prompt.tmp \
  "$@"

# Verify the output actually compiles
echo "Verifying generated code..."
if ./build.sh > BUILD_OUTPUT.log 2>&1; then
    echo "✓ Generated code compiles"
else
    echo "✗ Generated code FAILED to compile"
    echo "Forcing retry with error context..."
    
    # Auto-retry with compilation errors
    claude-code \
      --prompt "Fix these compilation errors: $(tail -20 BUILD_OUTPUT.log)" \
      --context-dir . \
      --max-attempts 5
fi