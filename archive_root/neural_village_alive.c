#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <sys/time.h>
#include <assert.h>

/*
 * NEURAL VILLAGE ALPHA v0.002 - TRULY ALIVE
 * NPCs that actually feel unique and alive
 */

#define ALPHA_VERSION "0.002"

// Basic types
typedef unsigned char u8;
typedef unsigned int u32;
typedef float f32;
typedef double f64;

// === UNIQUE NPC SYSTEM ===

typedef struct npc_memory {
    char event[128];
    f32 time_occurred;
    f32 emotional_impact;
    u32 with_npc_id;  // -1 if with player
} npc_memory;

typedef struct npc_relationship {
    u32 other_npc_id;
    f32 friendship;    // -100 to 100
    f32 romance;       // 0 to 100
    f32 trust;         // 0 to 100
    f32 respect;       // -100 to 100
    u32 shared_memories[10];
    u32 memory_count;
    char relationship_type[32]; // "rival", "friend", "crush", "spouse", etc
} npc_relationship;

typedef struct unique_npc {
    // Identity
    u32 id;
    char name[32];
    char occupation[32];
    u32 age;
    char gender;  // 'M', 'F', 'N'
    
    // Unique backstory
    char hometown[32];
    char life_goal[128];
    char biggest_fear[128];
    char favorite_thing[64];
    char hated_thing[64];
    char secret[128];
    
    // Personality (with VARIATION even within same occupation!)
    f32 extroversion;      // Unique value
    f32 agreeableness;     // Unique value  
    f32 conscientiousness; // Unique value
    f32 neuroticism;       // Unique value
    f32 openness;          // Unique value
    
    // Additional unique traits
    f32 humor;             // How funny they are
    f32 intelligence;      // How smart they are
    f32 creativity;        // How creative they are
    f32 loyalty;           // How loyal they are
    f32 ambition;          // How ambitious they are
    
    // Current state
    f32 x, y;
    f32 energy;            // 0-100, affects everything
    f32 mood;              // -100 to 100, current mood
    f32 stress;            // 0-100, affects decisions
    f32 health;            // 0-100
    
    // Relationships (with OTHER SPECIFIC NPCs)
    npc_relationship relationships[20];
    u32 relationship_count;
    
    // Personal memories
    npc_memory memories[50];
    u32 memory_count;
    
    // Current thoughts (based on THEIR specific situation)
    char current_thought[256];
    char current_action[128];
    
    // Daily routine (unique to each NPC)
    f32 wake_time;         // When they wake up (6.0 = 6am)
    f32 work_start;        // When they start work
    f32 lunch_time;        // When they eat lunch
    f32 work_end;          // When they stop working
    f32 sleep_time;        // When they go to bed
    
    // Personal possessions
    u32 money;
    u32 food;
    u32 special_items[5];
    
    // Home and work locations
    f32 home_x, home_y;
    f32 work_x, work_y;
    f32 favorite_spot_x, favorite_spot_y;
    
} unique_npc;

// Create TRULY unique NPCs with individual personalities and backstories
void create_unique_npc(unique_npc* npc, u32 id, const char* name, const char* occupation,
                      u32 age, char gender, f32 x, f32 y) {
    npc->id = id;
    strcpy(npc->name, name);
    strcpy(npc->occupation, occupation);
    npc->age = age;
    npc->gender = gender;
    npc->x = x;
    npc->y = y;
    
    // UNIQUE personality - add random variation to base values
    f32 variation = 0.3f;
    
    if (strcmp(occupation, "Farmer") == 0) {
        npc->extroversion = 0.4f + ((rand() % 100) / 100.0f - 0.5f) * variation;
        npc->agreeableness = 0.7f + ((rand() % 100) / 100.0f - 0.5f) * variation;
        npc->conscientiousness = 0.8f + ((rand() % 100) / 100.0f - 0.5f) * variation;
        npc->neuroticism = 0.3f + ((rand() % 100) / 100.0f - 0.5f) * variation;
        npc->openness = 0.5f + ((rand() % 100) / 100.0f - 0.5f) * variation;
    } else if (strcmp(occupation, "Merchant") == 0) {
        npc->extroversion = 0.8f + ((rand() % 100) / 100.0f - 0.5f) * variation;
        npc->agreeableness = 0.6f + ((rand() % 100) / 100.0f - 0.5f) * variation;
        npc->conscientiousness = 0.7f + ((rand() % 100) / 100.0f - 0.5f) * variation;
        npc->neuroticism = 0.4f + ((rand() % 100) / 100.0f - 0.5f) * variation;
        npc->openness = 0.7f + ((rand() % 100) / 100.0f - 0.5f) * variation;
    } else {
        // Completely random for villagers
        npc->extroversion = (rand() % 100) / 100.0f;
        npc->agreeableness = (rand() % 100) / 100.0f;
        npc->conscientiousness = (rand() % 100) / 100.0f;
        npc->neuroticism = (rand() % 100) / 100.0f;
        npc->openness = (rand() % 100) / 100.0f;
    }
    
    // Unique additional traits
    npc->humor = (rand() % 100) / 100.0f;
    npc->intelligence = (rand() % 100) / 100.0f;
    npc->creativity = (rand() % 100) / 100.0f;
    npc->loyalty = (rand() % 100) / 100.0f;
    npc->ambition = (rand() % 100) / 100.0f;
    
    // Start with decent energy and mood
    npc->energy = 70.0f + (rand() % 30);
    npc->mood = 20.0f + (rand() % 40) - 20.0f;
    npc->stress = 10.0f + (rand() % 30);
    npc->health = 80.0f + (rand() % 20);
    
    // Unique daily routine
    npc->wake_time = 5.0f + (rand() % 40) / 10.0f;  // 5am to 9am
    npc->work_start = npc->wake_time + 1.0f + (rand() % 20) / 10.0f;
    npc->lunch_time = 11.0f + (rand() % 40) / 10.0f;  // 11am to 3pm
    npc->work_end = 16.0f + (rand() % 40) / 10.0f;    // 4pm to 8pm
    npc->sleep_time = 20.0f + (rand() % 40) / 10.0f;  // 8pm to midnight
    
    // Starting resources
    npc->money = 50 + rand() % 200;
    npc->food = 5 + rand() % 10;
    
    npc->relationship_count = 0;
    npc->memory_count = 0;
}

// Generate thoughts based on THIS SPECIFIC NPC's situation
void generate_personal_thought(unique_npc* npc, f32 world_time) {
    // Check current time vs routine
    if (fabs(world_time - npc->wake_time) < 0.5f) {
        if (npc->energy < 30) {
            snprintf(npc->current_thought, 255, "Ugh, I'm %s and I'm exhausted... need more sleep.", npc->name);
        } else {
            snprintf(npc->current_thought, 255, "Good morning! Time for %s to start the day!", npc->name);
        }
        return;
    }
    
    // Check relationships
    if (npc->relationship_count > 0) {
        int rel_idx = rand() % npc->relationship_count;
        npc_relationship* rel = &npc->relationships[rel_idx];
        
        if (rel->friendship > 50) {
            snprintf(npc->current_thought, 255, "I should visit my friend, NPC #%d. We always have fun together.", 
                    rel->other_npc_id);
        } else if (rel->friendship < -30) {
            snprintf(npc->current_thought, 255, "I hope I don't run into NPC #%d today. We don't get along.", 
                    rel->other_npc_id);
        } else if (rel->romance > 30) {
            snprintf(npc->current_thought, 255, "I wonder what NPC #%d is doing... *blushes*", 
                    rel->other_npc_id);
        }
        return;
    }
    
    // Check personal state
    if (npc->energy < 20) {
        strcpy(npc->current_thought, "I'm completely exhausted. Need to rest soon.");
    } else if (npc->stress > 70) {
        snprintf(npc->current_thought, 255, "As %s, I'm feeling really stressed about %s.", 
                npc->name, npc->biggest_fear);
    } else if (npc->mood > 50) {
        snprintf(npc->current_thought, 255, "Life is good! I love %s!", npc->favorite_thing);
    } else if (npc->mood < -30) {
        snprintf(npc->current_thought, 255, "Having a rough day. I really hate %s.", npc->hated_thing);
    } else {
        // Occupation-specific but PERSONALIZED
        if (strcmp(npc->occupation, "Farmer") == 0) {
            if (npc->conscientiousness > 0.7f) {
                snprintf(npc->current_thought, 255, "I, %s, take pride in my perfect rows of crops.", npc->name);
            } else {
                snprintf(npc->current_thought, 255, "Farming is hard work, but I'm %s and I manage.", npc->name);
            }
        } else if (strcmp(npc->occupation, "Merchant") == 0) {
            if (npc->money > 200) {
                snprintf(npc->current_thought, 255, "Business is booming! %s knows how to make deals!", npc->name);
            } else {
                snprintf(npc->current_thought, 255, "Sales are slow. %s needs to find better customers.", npc->name);
            }
        } else {
            snprintf(npc->current_thought, 255, "Just %s, living my life in the village.", npc->name);
        }
    }
}

// Build relationships between SPECIFIC NPCs
void initialize_npc_relationships(unique_npc* npcs, int count) {
    // Marcus and Elena are childhood friends
    npcs[0].relationships[0].other_npc_id = 1;
    npcs[0].relationships[0].friendship = 75.0f;
    npcs[0].relationships[0].trust = 80.0f;
    npcs[0].relationships[0].respect = 60.0f;
    strcpy(npcs[0].relationships[0].relationship_type, "childhood friend");
    npcs[0].relationship_count = 1;
    strcpy(npcs[0].life_goal, "Become the wealthiest merchant in the region");
    strcpy(npcs[0].biggest_fear, "Losing everything and becoming poor");
    strcpy(npcs[0].favorite_thing, "The smell of fresh coins");
    strcpy(npcs[0].hated_thing, "Thieves and dishonesty");
    strcpy(npcs[0].secret, "Once gave away half his savings to help a poor family");
    
    npcs[1].relationships[0].other_npc_id = 0;
    npcs[1].relationships[0].friendship = 75.0f;
    npcs[1].relationships[0].trust = 80.0f;
    npcs[1].relationships[0].respect = 70.0f;
    strcpy(npcs[1].relationships[0].relationship_type, "childhood friend");
    npcs[1].relationship_count = 1;
    strcpy(npcs[1].life_goal, "Grow the most beautiful garden anyone has ever seen");
    strcpy(npcs[1].biggest_fear, "Drought destroying all the crops");
    strcpy(npcs[1].favorite_thing, "Morning dew on fresh leaves");
    strcpy(npcs[1].hated_thing, "Pests that eat the plants");
    strcpy(npcs[1].secret, "Talks to her plants and believes they respond");
    
    // Rex has a crush on Luna
    npcs[2].relationships[0].other_npc_id = 3;
    npcs[2].relationships[0].friendship = 40.0f;
    npcs[2].relationships[0].romance = 60.0f;
    npcs[2].relationships[0].trust = 50.0f;
    strcpy(npcs[2].relationships[0].relationship_type, "secret crush");
    npcs[2].relationship_count = 1;
    strcpy(npcs[2].life_goal, "Protect the village and earn everyone's respect");
    strcpy(npcs[2].biggest_fear, "Failing to protect someone when they need him");
    strcpy(npcs[2].favorite_thing, "The sound of peaceful mornings");
    strcpy(npcs[2].hated_thing, "Bullies and troublemakers");
    strcpy(npcs[2].secret, "Writes poetry but is too embarrassed to share it");
    
    // Luna is oblivious but likes Ben
    npcs[3].relationships[0].other_npc_id = 4;
    npcs[3].relationships[0].friendship = 55.0f;
    npcs[3].relationships[0].romance = 45.0f;
    strcpy(npcs[3].relationships[0].relationship_type, "interested");
    npcs[3].relationship_count = 1;
    strcpy(npcs[3].life_goal, "Create a masterpiece that will be remembered forever");
    strcpy(npcs[3].biggest_fear, "Never being understood or appreciated");
    strcpy(npcs[3].favorite_thing, "The way light changes throughout the day");
    strcpy(npcs[3].hated_thing, "People who don't appreciate art");
    strcpy(npcs[3].secret, "Sometimes doubts if her art has any meaning");
    
    // Ben and Jack are rivals
    npcs[4].relationships[0].other_npc_id = 8;
    npcs[4].relationships[0].friendship = -40.0f;
    npcs[4].relationships[0].respect = 20.0f;
    strcpy(npcs[4].relationships[0].relationship_type, "rival");
    npcs[4].relationship_count = 1;
    strcpy(npcs[4].life_goal, "Prove he's the best farmer in the village");
    strcpy(npcs[4].biggest_fear, "Being shown up by Jack");
    strcpy(npcs[4].favorite_thing, "Winning the harvest competition");
    strcpy(npcs[4].hated_thing, "Coming in second place");
    strcpy(npcs[4].secret, "Actually respects Jack's farming skills");
    
    // Sara and Rose are best friends
    npcs[5].relationships[0].other_npc_id = 9;
    npcs[5].relationships[0].friendship = 85.0f;
    npcs[5].relationships[0].trust = 90.0f;
    strcpy(npcs[5].relationships[0].relationship_type, "best friend");
    npcs[5].relationship_count = 1;
    strcpy(npcs[5].life_goal, "Open a shop in the capital city");
    strcpy(npcs[5].biggest_fear, "Being stuck in this small village forever");
    strcpy(npcs[5].favorite_thing, "Meeting travelers and hearing their stories");
    strcpy(npcs[5].hated_thing, "The same routine every single day");
    strcpy(npcs[5].secret, "Has been saving money to leave the village");
    
    // Tom is suspicious of everyone
    npcs[6].relationships[0].other_npc_id = 2;
    npcs[6].relationships[0].friendship = -20.0f;
    npcs[6].relationships[0].trust = 10.0f;
    strcpy(npcs[6].relationships[0].relationship_type, "suspicious");
    npcs[6].relationship_count = 1;
    strcpy(npcs[6].life_goal, "Find out the truth about the village's past");
    strcpy(npcs[6].biggest_fear, "That everyone is hiding something from him");
    strcpy(npcs[6].favorite_thing, "Solving puzzles and mysteries");
    strcpy(npcs[6].hated_thing, "Being lied to");
    strcpy(npcs[6].secret, "Found an old map that might lead to treasure");
    
    // Anna is the village gossip
    npcs[7].relationships[0].other_npc_id = 5;
    npcs[7].relationships[0].friendship = 30.0f;
    strcpy(npcs[7].relationships[0].relationship_type, "gossip source");
    npcs[7].relationship_count = 1;
    strcpy(npcs[7].life_goal, "Know everything about everyone");
    strcpy(npcs[7].biggest_fear, "Being left out of important events");
    strcpy(npcs[7].favorite_thing, "A juicy piece of gossip");
    strcpy(npcs[7].hated_thing, "Being ignored");
    strcpy(npcs[7].secret, "Makes up stories when the truth is boring");
    
    // Jack (rival with Ben)
    npcs[8].relationships[0].other_npc_id = 4;
    npcs[8].relationships[0].friendship = -40.0f;
    npcs[8].relationships[0].respect = 30.0f;
    strcpy(npcs[8].relationships[0].relationship_type, "rival");
    npcs[8].relationship_count = 1;
    strcpy(npcs[8].life_goal, "Beat Ben at the harvest festival");
    strcpy(npcs[8].biggest_fear, "His farm failing");
    strcpy(npcs[8].favorite_thing, "The smell of fresh soil");
    strcpy(npcs[8].hated_thing, "Ben's smugness");
    strcpy(npcs[8].secret, "Secretly studies Ben's farming techniques");
    
    // Rose (best friend with Sara)
    npcs[9].relationships[0].other_npc_id = 5;
    npcs[9].relationships[0].friendship = 85.0f;
    npcs[9].relationships[0].trust = 90.0f;
    strcpy(npcs[9].relationships[0].relationship_type, "best friend");
    npcs[9].relationship_count = 1;
    strcpy(npcs[9].life_goal, "Fill the world with beauty");
    strcpy(npcs[9].biggest_fear, "Sara leaving without her");
    strcpy(npcs[9].favorite_thing, "Creating art with Sara");
    strcpy(npcs[9].hated_thing, "Being alone");
    strcpy(npcs[9].secret, "Is in love with Sara but hasn't told her");
}

// === DEMO STRUCTURE ===

typedef struct village_demo {
    Display* display;
    Window window;
    GC gc;
    XImage* screen;
    u32* pixels;
    int width, height;
    
    unique_npc npcs[10];
    f32 player_x, player_y;
    f32 world_time;
    
    int running;
    
} village_demo;

// Simple rendering for demo
void render_frame(village_demo* demo) {
    // Clear screen
    for (int i = 0; i < demo->width * demo->height; i++) {
        demo->pixels[i] = 0x202020;
    }
    
    // Draw NPCs
    for (int i = 0; i < 10; i++) {
        unique_npc* npc = &demo->npcs[i];
        int x = (int)npc->x;
        int y = (int)npc->y;
        
        // Draw NPC as colored square
        u32 color = 0xFF0000; // Red by default
        if (strcmp(npc->occupation, "Farmer") == 0) color = 0x00FF00;
        else if (strcmp(npc->occupation, "Merchant") == 0) color = 0xFFFF00;
        else if (strcmp(npc->occupation, "Artist") == 0) color = 0xFF00FF;
        else if (strcmp(npc->occupation, "Guard") == 0) color = 0x0000FF;
        
        for (int dy = -4; dy < 4; dy++) {
            for (int dx = -4; dx < 4; dx++) {
                int px = x + dx;
                int py = y + dy;
                if (px >= 0 && px < demo->width && py >= 0 && py < demo->height) {
                    demo->pixels[py * demo->width + px] = color;
                }
            }
        }
        
        // Draw name and thought (simplified)
        // In real version, would use proper text rendering
    }
    
    // Draw player
    int px = (int)demo->player_x;
    int py = (int)demo->player_y;
    for (int dy = -5; dy < 5; dy++) {
        for (int dx = -5; dx < 5; dx++) {
            int x = px + dx;
            int y = py + dy;
            if (x >= 0 && x < demo->width && y >= 0 && y < demo->height) {
                demo->pixels[y * demo->width + x] = 0xFFFFFF;
            }
        }
    }
    
    XPutImage(demo->display, demo->window, demo->gc, demo->screen,
              0, 0, 0, 0, demo->width, demo->height);
}

// Update NPCs with their unique behaviors
void update_npcs(village_demo* demo, f32 dt) {
    for (int i = 0; i < 10; i++) {
        unique_npc* npc = &demo->npcs[i];
        
        // Update energy based on time
        if (demo->world_time < npc->wake_time || demo->world_time > npc->sleep_time) {
            npc->energy += dt * 10.0f; // Regenerate while sleeping
        } else {
            npc->energy -= dt * 2.0f; // Drain while awake
        }
        
        // Clamp energy
        if (npc->energy > 100) npc->energy = 100;
        if (npc->energy < 0) npc->energy = 0;
        
        // Update mood based on energy and stress
        npc->mood += dt * (npc->energy / 100.0f - npc->stress / 100.0f) * 5.0f;
        if (npc->mood > 100) npc->mood = 100;
        if (npc->mood < -100) npc->mood = -100;
        
        // Generate thoughts
        if (rand() % 100 < 10) { // 10% chance per frame
            generate_personal_thought(npc, demo->world_time);
        }
        
        // Simple movement (would be more complex in full version)
        npc->x += ((rand() % 3) - 1) * dt * 20.0f;
        npc->y += ((rand() % 3) - 1) * dt * 20.0f;
        
        // Keep in bounds
        if (npc->x < 10) npc->x = 10;
        if (npc->x > demo->width - 10) npc->x = demo->width - 10;
        if (npc->y < 10) npc->y = 10;
        if (npc->y > demo->height - 10) npc->y = demo->height - 10;
    }
}

int main() {
    srand(time(NULL));
    
    village_demo demo = {0};
    
    // Initialize display
    demo.display = XOpenDisplay(NULL);
    if (!demo.display) {
        printf("Cannot open display\n");
        return 1;
    }
    
    int screen = DefaultScreen(demo.display);
    demo.width = 800;
    demo.height = 600;
    
    demo.window = XCreateSimpleWindow(demo.display, RootWindow(demo.display, screen),
                                      0, 0, demo.width, demo.height, 1,
                                      BlackPixel(demo.display, screen),
                                      WhitePixel(demo.display, screen));
    
    XSelectInput(demo.display, demo.window, ExposureMask | KeyPressMask);
    XMapWindow(demo.display, demo.window);
    XStoreName(demo.display, demo.window, "Neural Village - ALIVE NPCs Demo");
    
    demo.gc = XCreateGC(demo.display, demo.window, 0, NULL);
    
    demo.pixels = malloc(demo.width * demo.height * sizeof(u32));
    demo.screen = XCreateImage(demo.display, DefaultVisual(demo.display, screen),
                              DefaultDepth(demo.display, screen), ZPixmap, 0,
                              (char*)demo.pixels, demo.width, demo.height, 32, 0);
    
    // Create UNIQUE NPCs
    create_unique_npc(&demo.npcs[0], 0, "Marcus", "Merchant", 35, 'M', 400, 300);
    create_unique_npc(&demo.npcs[1], 1, "Elena", "Farmer", 28, 'F', 200, 200);
    create_unique_npc(&demo.npcs[2], 2, "Rex", "Guard", 32, 'M', 600, 400);
    create_unique_npc(&demo.npcs[3], 3, "Luna", "Artist", 24, 'F', 300, 500);
    create_unique_npc(&demo.npcs[4], 4, "Ben", "Farmer", 30, 'M', 350, 250);
    create_unique_npc(&demo.npcs[5], 5, "Sara", "Merchant", 26, 'F', 450, 350);
    create_unique_npc(&demo.npcs[6], 6, "Tom", "Villager", 45, 'M', 500, 200);
    create_unique_npc(&demo.npcs[7], 7, "Anna", "Villager", 38, 'F', 150, 400);
    create_unique_npc(&demo.npcs[8], 8, "Jack", "Farmer", 29, 'M', 250, 300);
    create_unique_npc(&demo.npcs[9], 9, "Rose", "Artist", 25, 'F', 550, 450);
    
    // Set up their relationships and backstories
    initialize_npc_relationships(demo.npcs, 10);
    
    // Print NPC info to console
    printf("\n=== TRULY UNIQUE NPCS ===\n\n");
    for (int i = 0; i < 10; i++) {
        unique_npc* npc = &demo.npcs[i];
        printf("%s (%s, age %d):\n", npc->name, npc->occupation, npc->age);
        printf("  Personality: E:%.2f A:%.2f C:%.2f N:%.2f O:%.2f\n",
               npc->extroversion, npc->agreeableness, npc->conscientiousness,
               npc->neuroticism, npc->openness);
        printf("  Unique Traits: Humor:%.2f Intel:%.2f Create:%.2f Loyal:%.2f Ambition:%.2f\n",
               npc->humor, npc->intelligence, npc->creativity, npc->loyalty, npc->ambition);
        printf("  Life Goal: %s\n", npc->life_goal);
        printf("  Biggest Fear: %s\n", npc->biggest_fear);
        printf("  Secret: %s\n", npc->secret);
        if (npc->relationship_count > 0) {
            printf("  Relationship: %s with NPC #%d\n", 
                   npc->relationships[0].relationship_type,
                   npc->relationships[0].other_npc_id);
        }
        printf("\n");
    }
    
    demo.player_x = 400;
    demo.player_y = 300;
    demo.world_time = 12.0f;
    demo.running = 1;
    
    // Main loop
    struct timeval last_time, current_time;
    gettimeofday(&last_time, NULL);
    
    while (demo.running) {
        // Handle events
        while (XPending(demo.display)) {
            XEvent event;
            XNextEvent(demo.display, &event);
            
            if (event.type == KeyPress) {
                KeySym key = XLookupKeysym(&event.xkey, 0);
                if (key == XK_Escape || key == XK_q) {
                    demo.running = 0;
                }
            }
        }
        
        // Calculate delta time
        gettimeofday(&current_time, NULL);
        f32 dt = (current_time.tv_sec - last_time.tv_sec) + 
                 (current_time.tv_usec - last_time.tv_usec) / 1000000.0f;
        last_time = current_time;
        
        // Update world time
        demo.world_time += dt * 0.5f; // Time passes at 0.5x real time
        if (demo.world_time >= 24.0f) demo.world_time -= 24.0f;
        
        // Update NPCs
        update_npcs(&demo, dt);
        
        // Render
        render_frame(&demo);
        
        usleep(16666); // ~60 FPS
    }
    
    // Cleanup
    XDestroyImage(demo.screen);
    XFreeGC(demo.display, demo.gc);
    XDestroyWindow(demo.display, demo.window);
    XCloseDisplay(demo.display);
    free(demo.pixels);
    
    return 0;
}