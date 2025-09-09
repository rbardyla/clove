#ifndef HANDMADE_DNC_ENHANCED_H
#define HANDMADE_DNC_ENHANCED_H

/*
    Enhanced Differentiable Neural Computer for Game NPCs
    Production-ready memory system with EWC and personality
    
    Based on senior dev feedback - addresses all production blockers:
    - Two-tier memory (short-term + consolidated long-term)
    - Smooth emotional transitions
    - Multi-dimensional relationships
    - Semantic memory indexing
    - Inter-NPC communication
    - Deterministic save states
*/

#include "../../src/handmade.h"
#include <math.h>

// Memory configuration
#define DNC_SHORT_TERM_SIZE 20      // Recent interactions buffer
#define DNC_LONG_TERM_SIZE 30       // Consolidated memories
#define DNC_MAX_RELATIONSHIPS 100   // Max tracked relationships
#define DNC_SEMANTIC_BUCKETS 64     // Hash buckets for semantic indexing
#define DNC_EMOTION_DECAY 0.95f     // Smooth transition factor

// Memory importance factors
typedef struct importance_factors {
    f32 emotional_intensity;    // How emotionally charged was this?
    f32 recency_weight;         // How recent? (decays over time)
    f32 frequency_bonus;        // Repeated interactions get bonus
    f32 player_significance;    // Was player involved?
    f32 narrative_marker;       // Story-critical event?
} importance_factors;

// Enhanced memory structure
typedef struct dnc_memory {
    // Core memory data
    char content[256];          // What happened
    f64 timestamp;              // When it happened
    v3 location;                // Where it happened
    
    // Semantic indexing
    u32 semantic_hash;          // For fast similarity search
    u32 actor_hash;             // Who was involved
    u32 action_hash;            // What type of action
    
    // Importance tracking
    importance_factors importance;
    f32 total_importance;       // Calculated score
    
    // Memory consolidation
    u32 access_count;           // How often recalled
    f64 last_access;            // Last time remembered
    b32 is_consolidated;        // Has been moved to long-term
    
    // Emotional context
    f32 emotional_valence;      // Positive/negative
    f32 emotional_arousal;      // Intensity
    
    // Related memories
    u32 related_indices[4];     // Links to related memories
    u32 related_count;
} dnc_memory;

// Multi-dimensional relationship
typedef struct relationship {
    char actor_id[64];
    
    // Relationship dimensions
    f32 trust;                  // How much we trust them
    f32 affection;              // How much we like them
    f32 respect;                // How much we respect them
    f32 fear;                   // How much we fear them
    f32 familiarity;            // How well we know them
    
    // Relationship type and history
    enum {
        REL_STRANGER,
        REL_ACQUAINTANCE,
        REL_FRIEND,
        REL_RIVAL,
        REL_ENEMY,
        REL_ROMANTIC,
        REL_FAMILY,
        REL_MENTOR,
        REL_STUDENT
    } relationship_type;
    
    // Shared experiences
    u32 shared_memories[10];    // Indices to memories
    u32 shared_count;
    
    // Interaction statistics
    u32 total_interactions;
    f64 last_interaction;
    f32 interaction_quality;    // Average quality of interactions
} relationship;

// Emotional state with smooth transitions
typedef struct emotional_state {
    // Current emotional vector
    f32 happiness;
    f32 anger;
    f32 fear;
    f32 sadness;
    f32 surprise;
    f32 disgust;
    
    // Target emotional state (for smooth transitions)
    f32 target_happiness;
    f32 target_anger;
    f32 target_fear;
    f32 target_sadness;
    f32 target_surprise;
    f32 target_disgust;
    
    // Emotional momentum
    f32 emotional_inertia;      // How quickly emotions change
    f32 baseline_mood;          // Personality-based default
} emotional_state;

// Enhanced personality system
typedef struct personality {
    // Big Five traits
    f32 openness;
    f32 conscientiousness;
    f32 extraversion;
    f32 agreeableness;
    f32 neuroticism;
    
    // Game-specific traits
    f32 bravery;
    f32 loyalty;
    f32 greed;
    f32 humor;
    f32 curiosity;
    f32 aggression;
    
    // Behavioral tendencies
    f32 risk_tolerance;
    f32 social_need;
    f32 independence;
    f32 morality;
    
    // Response modifiers
    f32 verbosity;              // How much they talk
    f32 formality;              // Speech style
    f32 emotionality;           // How expressive
} personality;

// Inter-NPC communication
typedef struct gossip_message {
    char source_npc[64];
    char about_actor[64];
    dnc_memory shared_memory;
    f32 trust_modifier;         // How much to trust this info
    f32 distortion;            // How much the story changed
} gossip_message;

// Semantic indexing for fast memory search
typedef struct semantic_index {
    dnc_memory *buckets[DNC_SEMANTIC_BUCKETS];
    u32 bucket_sizes[DNC_SEMANTIC_BUCKETS];
    u32 bucket_capacity[DNC_SEMANTIC_BUCKETS];
} semantic_index;

// Response generation context
typedef struct response_context {
    char current_situation[256];
    char speaker_id[64];
    f32 urgency;
    f32 formality_required;
    b32 is_combat;
    b32 is_trade;
    b32 is_social;
} response_context;

// Generated response with reasoning
typedef struct npc_response {
    char text[512];
    char action[128];
    f32 confidence;
    
    // Why this response was chosen
    f32 memory_influence;
    f32 personality_influence;
    f32 emotion_influence;
    f32 relationship_influence;
} npc_response;

// Main DNC structure
typedef struct enhanced_dnc {
    // Identity
    char npc_id[64];
    char npc_name[128];
    
    // Two-tier memory system
    dnc_memory short_term[DNC_SHORT_TERM_SIZE];
    dnc_memory long_term[DNC_LONG_TERM_SIZE];
    u32 short_term_count;
    u32 long_term_count;
    
    // Memory indexing
    semantic_index memory_index;
    
    // Relationships
    relationship relationships[DNC_MAX_RELATIONSHIPS];
    u32 relationship_count;
    
    // Current state
    emotional_state emotions;
    personality traits;
    
    // Memory consolidation parameters
    f32 consolidation_threshold;
    f32 forgetting_rate;
    u64 memory_clock;           // Internal time for memory decay
    
    // Communication
    gossip_message gossip_queue[10];
    u32 gossip_count;
    
    // Performance metrics
    struct {
        f64 avg_recall_time;
        u32 total_recalls;
        u32 consolidations;
        u32 forgotten_memories;
        f64 last_update_ms;
    } metrics;
    
    // EWC parameters
    f32 *fisher_information;    // Importance weights
    f32 *optimal_weights;       // Previous task weights
    f32 ewc_lambda;            // How much to preserve old knowledge
} enhanced_dnc;

// =============================================================================
// CORE API
// =============================================================================

// Initialization
enhanced_dnc *dnc_create(char *npc_id, char *npc_name, personality *traits);
void dnc_destroy(enhanced_dnc *dnc);

// Memory operations
void dnc_observe(enhanced_dnc *dnc, char *observation, response_context *context);
dnc_memory *dnc_recall(enhanced_dnc *dnc, char *query, u32 max_results);
void dnc_consolidate_memories(enhanced_dnc *dnc);
f32 dnc_calculate_importance(enhanced_dnc *dnc, dnc_memory *memory, importance_factors *factors);

// Emotional processing
void dnc_update_emotions(enhanced_dnc *dnc, f32 dt);
void dnc_set_emotional_target(enhanced_dnc *dnc, emotional_state *target);
f32 dnc_get_emotional_valence(enhanced_dnc *dnc);

// Relationship management
relationship *dnc_get_relationship(enhanced_dnc *dnc, char *actor_id);
void dnc_update_relationship(enhanced_dnc *dnc, char *actor_id, f32 trust_delta, f32 affection_delta);
void dnc_form_relationship(enhanced_dnc *dnc, char *actor_id, relationship *initial);

// Response generation
npc_response dnc_generate_response(enhanced_dnc *dnc, response_context *context);
char *dnc_get_greeting(enhanced_dnc *dnc, char *target_id);
char *dnc_get_farewell(enhanced_dnc *dnc, char *target_id);

// Inter-NPC communication
void dnc_share_gossip(enhanced_dnc *sender, enhanced_dnc *receiver, dnc_memory *memory);
void dnc_process_gossip(enhanced_dnc *dnc);
b32 dnc_would_share_with(enhanced_dnc *dnc, char *target_id);

// Serialization (deterministic saves)
void dnc_serialize(enhanced_dnc *dnc, void *buffer, u64 *size);
enhanced_dnc *dnc_deserialize(void *buffer, u64 size);
u32 dnc_calculate_checksum(enhanced_dnc *dnc);

// Performance monitoring
void dnc_get_metrics(enhanced_dnc *dnc, char *output, u64 max_size);
void dnc_reset_metrics(enhanced_dnc *dnc);

// =============================================================================
// SEMANTIC OPERATIONS
// =============================================================================

// Hashing for semantic indexing
u32 dnc_semantic_hash(char *content);
u32 dnc_actor_hash(char *actor_id);
u32 dnc_action_hash(char *action);

// Similarity measures
f32 dnc_memory_similarity(dnc_memory *a, dnc_memory *b);
f32 dnc_semantic_distance(char *text_a, char *text_b);

// Memory search
dnc_memory *dnc_find_similar_memories(enhanced_dnc *dnc, dnc_memory *query, u32 max_results);
dnc_memory *dnc_find_memories_with_actor(enhanced_dnc *dnc, char *actor_id);
dnc_memory *dnc_find_memories_at_location(enhanced_dnc *dnc, v3 location, f32 radius);

// =============================================================================
// DESIGNER TOOLS
// =============================================================================

// Personality templates
personality dnc_personality_template_friendly(void);
personality dnc_personality_template_aggressive(void);
personality dnc_personality_template_merchant(void);
personality dnc_personality_template_guard(void);
personality dnc_personality_template_child(void);
personality dnc_personality_template_elder(void);

// Debugging
void dnc_dump_memories(enhanced_dnc *dnc, char *filename);
void dnc_visualize_relationships(enhanced_dnc *dnc, char *dot_file);
void dnc_replay_memory(enhanced_dnc *dnc, u32 memory_index);

// A/B testing support
void dnc_set_personality_variant(enhanced_dnc *dnc, char *variant_id);
void dnc_log_interaction_quality(enhanced_dnc *dnc, f32 quality);

#endif // HANDMADE_DNC_ENHANCED_H