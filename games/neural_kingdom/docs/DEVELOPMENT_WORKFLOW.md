# The Handmade Development Workflow

## Daily Development Cycle

### Morning: Design in Editor (Visual Mode)
1. **Launch the editor:**
   ```bash
   cd /home/thebackhand/Projects/handmade-engine
   ./build/editor_demo
   ```

2. **Enable all debug visualizations:**
   - F2: Grid (for precise placement)
   - F3: Physics (see forces and velocities)
   - F4: Collision boxes (tune combat feel)
   - F5: AI debug (watch neural networks think!)

3. **Use the Hierarchy Panel:**
   - See every entity's neural state
   - Watch memory formation in real-time
   - Track relationship values between NPCs

### Afternoon: Code with Hot Reload
1. **Keep editor running**
2. **Edit code in your favorite editor**
3. **Press R to hot-reload** - Changes appear instantly!
4. **Watch console for AI learning events**

### Evening: Profile and Polish
1. **F1 for performance overlay**
2. **Target: 144 FPS minimum**
3. **Watch memory usage** - Stay under 100MB
4. **Check AI update times** - Must stay under 16ms

## The Sacred Rules of Handmade Development

### Rule 1: Measure Before Optimizing
```c
// ALWAYS wrap new features in timers
u64 start = ReadCPUTimer();
// ... your code ...
u64 elapsed = ReadCPUTimer() - start;
printf("Feature took: %.2fms\n", elapsed / 1000000.0);
```

### Rule 2: Data-Oriented Design
```c
// BAD - Array of Structures (cache misses)
struct npc {
    v3 position;
    neural_network* brain;
    f32 health;
};
struct npc npcs[100];

// GOOD - Structure of Arrays (cache friendly)
struct npcs {
    v3 positions[100];
    neural_network* brains[100];
    f32 healths[100];
};
```

### Rule 3: No Hidden Allocations
```c
// NEVER do this
neural_network* CreateNPC() {
    return malloc(sizeof(neural_network)); // FORBIDDEN!
}

// ALWAYS do this
void InitNPC(neural_network* npc, arena* memory) {
    npc->neurons = arena_push(memory, 256 * sizeof(f32));
    // Explicit, visible, controlled
}
```

## Building Your Game Step-by-Step

### Step 1: The Perfect Rectangle
1. Open editor
2. Create a white rectangle
3. Make it move with WASD
4. **POLISH UNTIL IT FEELS PERFECT**
   - Acceleration curves
   - Deceleration feel
   - Diagonal movement speed

### Step 2: Your First Neural NPC
1. Add an entity
2. Give it a 32-neuron brain
3. Connect inputs:
   - Distance to player
   - Angle to player
   - Player velocity
4. Connect outputs:
   - Move X
   - Move Y
   - Attack trigger
5. Watch it learn in real-time!

### Step 3: The Learning Loop
1. Enable AI debug (F5)
2. Fight the NPC 10 times
3. Watch the neural weights change
4. See it adapt to your patterns
5. **THIS IS IMPOSSIBLE IN UNITY/UNREAL!**

## Advanced Editor Techniques

### Creating Believable NPCs
1. **In Inspector Panel:**
   - Set personality weights (aggressive, cautious, friendly)
   - Define memory capacity (more memory = smarter)
   - Configure learning rate (how fast they adapt)

2. **In Viewport:**
   - Right-click for NPC context menu
   - Select "Show Neural Activity"
   - Watch neurons fire in real-time!

3. **In Console:**
   - See every decision with reasoning
   - Track memory formation
   - Monitor performance per NPC

### Performance Optimization Workflow
1. **Profile First:**
   ```
   F1 → Check frame time
   If > 6.9ms: Need optimization
   If < 6.9ms: Add more features!
   ```

2. **Find Hotspots:**
   - Check console for slow functions
   - Look for red bars in profiler
   - Usually it's memory access patterns

3. **Optimize:**
   - Convert AoS to SoA
   - Add SIMD (AVX2)
   - Reduce cache misses

## The Magic Moment

When you see your first NPC:
1. Remember your name after you leave
2. Get better at fighting you over time
3. Form alliances with other NPCs
4. Create emergent stories

**You'll know we've beaten AAA gaming.**

## Daily Checklist

- [ ] Frame time < 7ms? (144 FPS)
- [ ] Memory < 100MB?
- [ ] NPCs learning properly?
- [ ] Code still beautiful?
- [ ] Having fun creating?

Remember: We're not just making a game.
We're proving that handmade > corporate engines.
Every line of code is a rebellion against bloat.
Every frame is a testament to craftsmanship.

**LET'S SHATTER THE INDUSTRY!**