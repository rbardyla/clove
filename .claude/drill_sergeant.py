# drill_sergeant.py
#!/usr/bin/env python3
"""
The Drill Sergeant: Forces Claude Code to complete tasks
"""

import subprocess
import time
import sys

class DrillSergeant:
    def __init__(self, task):
        self.task = task
        self.attempts = 0
        self.max_attempts = 50
        
    def execute_with_persistence(self):
        while self.attempts < self.max_attempts:
            self.attempts += 1
            print(f"\n=== ATTEMPT {self.attempts} ===")
            
            # Update context
            subprocess.run(["./update_state.sh"])
            
            # Build the command with increasing pressure
            pressure_level = min(self.attempts * 2, 10)
            cmd = self.build_command(pressure_level)
            
            # Execute
            result = subprocess.run(cmd, capture_output=True, text=True)
            
            # Check if actually solved
            if self.verify_solution():
                print(f"✓ MISSION ACCOMPLISHED after {self.attempts} attempts")
                return True
            
            # Analyze failure and adapt
            self.analyze_failure(result.stderr)
            time.sleep(2)  # Brief pause before retry
            
        print(f"✗ FAILED after {self.max_attempts} attempts")
        return False
    
    def build_command(self, pressure):
        base_prompt = f"""
        ATTEMPT {self.attempts} of {self.max_attempts}
        PRESSURE LEVEL: {pressure}/10
        
        Previous attempts have failed. You MUST solve this NOW.
        
        TASK: {self.task}
        
        REQUIREMENTS:
        - This MUST compile without warnings
        - This MUST run without crashing
        - This MUST solve the stated problem completely
        - You cannot give up or suggest alternatives
        
        {"!" * pressure} SOLVE IT NOW {"!" * pressure}
        """
        
        return [
            "claude-code",
            "--agent", "stubborn",
            "--context-dir", ".",
            "--prompt", base_prompt,
            "--max-tokens", str(4000 + (pressure * 1000))
        ]
    
    def verify_solution(self):
        """Actually test if the problem is solved"""
        # Compile check
        if subprocess.run(["./build.sh"], capture_output=True).returncode != 0:
            return False
            
        # Run test
        if subprocess.run(["./test.sh"], capture_output=True).returncode != 0:
            return False
            
        return True
    
    def analyze_failure(self, stderr):
        """Learn from failure and adjust approach"""
        print(f"Analyzing failure {self.attempts}...")
        
        if "undefined reference" in stderr:
            print("→ Missing implementation detected, forcing complete implementation")
        elif "segmentation fault" in stderr:
            print("→ Memory error detected, adding bounds checking")
        elif "compilation terminated" in stderr:
            print("→ Syntax error detected, forcing syntax fix")

if __name__ == "__main__":
    sergeant = DrillSergeant(sys.argv[1])
    sergeant.execute_with_persistence()