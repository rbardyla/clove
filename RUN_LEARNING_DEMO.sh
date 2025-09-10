#!/bin/bash

clear
echo "╔══════════════════════════════════════════════════════════════════════════════╗"
echo "║               NEURAL VILLAGE - REAL LEARNING DEMONSTRATION                   ║"
echo "║                    NPCs That Actually Remember You!                          ║"
echo "╚══════════════════════════════════════════════════════════════════════════════╝"
echo
echo "🧠 KEY FEATURES IN THIS VERSION:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo
echo "✓ NPCs REMEMBER every interaction with you"
echo "✓ They REFERENCE shared memories in conversation" 
echo "✓ Their PERSONALITY CHANGES based on experiences"
echo "✓ They LEARN FACTS about you over time"
echo "✓ Their TRUST and FAMILIARITY grow/decay based on actions"
echo "✓ Full LOGGING system proves learning is happening"
echo
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo
echo "HOW TO TEST THE LEARNING:"
echo
echo "1. Talk to an NPC - they'll say 'Nice to meet you!'"
echo "2. Walk away and come back - they'll say 'Oh, you're back!'"
echo "3. Keep interacting - they start calling you 'friend'"
echo "4. Press TAB - see their memories of you!"
echo "5. Check neural_village_learning.log for proof"
echo
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo
echo "CONTROLS:"
echo "  WASD/Arrows - Move"
echo "  ENTER - Talk (builds memories!)"
echo "  TAB - Debug view (see memories!)"
echo "  ESC - Quit"
echo
echo "Starting in 3..."
sleep 1
echo "2..."
sleep 1
echo "1..."
sleep 1

# Clear the log for fresh demo
echo "=== NEW LEARNING SESSION ===" > neural_village_learning.log

./neural_village_alpha_learning

echo
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "LEARNING DEMO COMPLETE!"
echo
echo "Check neural_village_learning.log to see all the learning that happened!"
echo "Every memory formed, every personality shift, every relationship change!"
echo
echo "This proves NPCs can learn and remember in just 35KB of code!"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"