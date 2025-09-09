#!/usr/bin/env python3
# brute_force_mode.py

import itertools
import subprocess

def brute_force_solution(problem):
    """
    Generate every possible solution until one works
    """
    approaches = [
        "iterative", "recursive", "dynamic programming",
        "brute force", "divide and conquer", "greedy",
        "backtracking", "memoization", "bit manipulation"
    ]
    
    data_structures = [
        "array", "linked list", "hash table", "tree",
        "graph", "heap", "stack", "queue"
    ]
    
    # Try every combination
    for approach, structure in itertools.product(approaches, data_structures):
        prompt = f"""
        Solve this problem using {approach} approach with {structure}.
        Problem: {problem}
        
        You MUST provide a complete, working solution.
        No excuses. No alternatives. Just solve it.
        """
        
        result = subprocess.run([
            "claude-code",
            "--prompt", prompt,
            "--max-attempts", "10"
        ], capture_output=True)
        
        if test_solution():
            print(f"✓ SOLVED with {approach} + {structure}")
            return True
    
    return False