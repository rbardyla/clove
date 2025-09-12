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


**POST-TASK REQUIREMENTS:**
1. Verify your solution compiles with the actual build system
2. Ensure it follows the handmade philosophy (zero dependencies)
3. Check that performance targets are met (60+ FPS)
4. Validate memory usage stays within arena bounds

Remember: You are modifying an EXISTING, WORKING codebase. Every change must integrate with what's already there.

## Current Codebase Summary
Files: 264
Systems: 23
Last build: Unknown
