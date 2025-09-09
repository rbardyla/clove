/*
    Enhanced DNC Implementation
    Production-ready neural memory for game NPCs
*/

#include "handmade_dnc_enhanced.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// =============================================================================
// MEMORY IMPORTANCE CALCULATION
// =============================================================================

f32 dnc_calculate_importance(enhanced_dnc *dnc, dnc_memory *memory, importance_factors *factors) {
    // Multi-factor importance as requested by senior dev
    f32 importance = 0.0f;
    
    // Emotional intensity (0.0 - 1.0) - highly emotional events are memorable
    f32 emotional_weight = 0.3f;
    importance += factors->emotional_intensity * emotional_weight;
    
    // Recency weight with exponential decay
    f64 time_since = dnc->memory_clock - memory->timestamp;
    f32 recency = expf(-(f32)time_since / 3600.0f); // Decay over hours
    importance += recency * 0.2f;
    
    // Frequency bonus - repeated patterns are important
    importance += fminf(factors->frequency_bonus * 0.1f, 0.2f);
    
    // Player involvement is always significant
    if (factors->player_significance > 0.0f) {
        importance += 0.25f;
    }
    
    // Narrative markers for story events
    importance += factors->narrative_marker * 0.25f;
    
    // Relationship significance
    relationship *rel = dnc_get_relationship(dnc, "player");
    if (rel) {
        f32 relationship_importance = (fabsf(rel->trust) + fabsf(rel->affection)) * 0.1f;
        importance += fminf(relationship_importance, 0.2f);
    }
    
    return fminf(importance, 1.0f);
}

// =============================================================================
// TWO-TIER MEMORY CONSOLIDATION
// =============================================================================

internal void consolidate_to_long_term(enhanced_dnc *dnc) {
    // Find the least important long-term memory if full
    if (dnc->long_term_count >= DNC_LONG_TERM_SIZE) {
        f32 min_importance = 1.0f;
        u32 min_index = 0;
        
        for (u32 i = 0; i < dnc->long_term_count; i++) {
            if (dnc->long_term[i].total_importance < min_importance) {
                min_importance = dnc->long_term[i].total_importance;
                min_index = i;
            }
        }
        
        // Check if any short-term memory is more important
        for (u32 i = 0; i < dnc->short_term_count; i++) {
            if (dnc->short_term[i].total_importance > min_importance) {
                // Replace the least important long-term memory
                dnc->metrics.forgotten_memories++;
                dnc->long_term[min_index] = dnc->short_term[i];
                dnc->long_term[min_index].is_consolidated = true;
                
                // Remove from short-term
                for (u32 j = i; j < dnc->short_term_count - 1; j++) {
                    dnc->short_term[j] = dnc->short_term[j + 1];
                }
                dnc->short_term_count--;
                dnc->metrics.consolidations++;
                return;
            }
        }
    } else {
        // Move most important short-term to long-term
        f32 max_importance = 0.0f;
        u32 max_index = 0;
        
        for (u32 i = 0; i < dnc->short_term_count; i++) {
            if (dnc->short_term[i].total_importance > max_importance) {
                max_importance = dnc->short_term[i].total_importance;
                max_index = i;
            }
        }
        
        if (max_importance > dnc->consolidation_threshold) {
            dnc->long_term[dnc->long_term_count] = dnc->short_term[max_index];
            dnc->long_term[dnc->long_term_count].is_consolidated = true;
            dnc->long_term_count++;
            
            // Remove from short-term
            for (u32 j = max_index; j < dnc->short_term_count - 1; j++) {
                dnc->short_term[j] = dnc->short_term[j + 1];
            }
            dnc->short_term_count--;
            dnc->metrics.consolidations++;
        }
    }
}

void dnc_consolidate_memories(enhanced_dnc *dnc) {
    // Consolidate when short-term is getting full
    while (dnc->short_term_count > DNC_SHORT_TERM_SIZE - 5) {
        consolidate_to_long_term(dnc);
    }
    
    // Update importance scores based on access patterns
    for (u32 i = 0; i < dnc->long_term_count; i++) {
        f64 time_since_access = dnc->memory_clock - dnc->long_term[i].last_access;
        f32 access_decay = expf(-(f32)time_since_access / 7200.0f); // 2 hour half-life
        
        // Memories that aren't accessed gradually lose importance
        dnc->long_term[i].total_importance *= (0.99f + 0.01f * access_decay);
    }
}

// =============================================================================
// SMOOTH EMOTIONAL TRANSITIONS
// =============================================================================

void dnc_update_emotions(enhanced_dnc *dnc, f32 dt) {
    // Smooth transitions using decay factor as suggested
    f32 decay = powf(DNC_EMOTION_DECAY, dt);
    f32 response = 1.0f - decay;
    
    // Smoothly transition each emotion component
    dnc->emotions.happiness = dnc->emotions.happiness * decay + 
                             dnc->emotions.target_happiness * response;
    dnc->emotions.anger = dnc->emotions.anger * decay + 
                          dnc->emotions.target_anger * response;
    dnc->emotions.fear = dnc->emotions.fear * decay + 
                        dnc->emotions.target_fear * response;
    dnc->emotions.sadness = dnc->emotions.sadness * decay + 
                           dnc->emotions.target_sadness * response;
    dnc->emotions.surprise = dnc->emotions.surprise * decay + 
                            dnc->emotions.target_surprise * response;
    dnc->emotions.disgust = dnc->emotions.disgust * decay + 
                           dnc->emotions.target_disgust * response;
    
    // Apply personality-based baseline mood
    f32 baseline_influence = 0.1f * dt;
    dnc->emotions.happiness += (dnc->emotions.baseline_mood - dnc->emotions.happiness) * baseline_influence;
}

void dnc_set_emotional_target(enhanced_dnc *dnc, emotional_state *target) {
    // Set target emotions for smooth transition
    dnc->emotions.target_happiness = target->happiness;
    dnc->emotions.target_anger = target->anger;
    dnc->emotions.target_fear = target->fear;
    dnc->emotions.target_sadness = target->sadness;
    dnc->emotions.target_surprise = target->surprise;
    dnc->emotions.target_disgust = target->disgust;
}

// =============================================================================
// MULTI-DIMENSIONAL RELATIONSHIPS
// =============================================================================

relationship *dnc_get_relationship(enhanced_dnc *dnc, char *actor_id) {
    for (u32 i = 0; i < dnc->relationship_count; i++) {
        if (strcmp(dnc->relationships[i].actor_id, actor_id) == 0) {
            return &dnc->relationships[i];
        }
    }
    return NULL;
}

void dnc_update_relationship(enhanced_dnc *dnc, char *actor_id, f32 trust_delta, f32 affection_delta) {
    relationship *rel = dnc_get_relationship(dnc, actor_id);
    
    if (!rel && dnc->relationship_count < DNC_MAX_RELATIONSHIPS) {
        // Create new relationship
        rel = &dnc->relationships[dnc->relationship_count++];
        strncpy(rel->actor_id, actor_id, 63);
        rel->trust = 0.0f;
        rel->affection = 0.0f;
        rel->respect = 0.0f;
        rel->fear = 0.0f;
        rel->familiarity = 0.0f;
        rel->relationship_type = REL_STRANGER;
    }
    
    if (rel) {
        // Update with bounded changes
        rel->trust = fmaxf(-1.0f, fminf(1.0f, rel->trust + trust_delta));
        rel->affection = fmaxf(-1.0f, fminf(1.0f, rel->affection + affection_delta));
        
        // Familiarity always increases slightly with interaction
        rel->familiarity = fminf(1.0f, rel->familiarity + 0.01f);
        rel->total_interactions++;
        rel->last_interaction = dnc->memory_clock;
        
        // Update relationship type based on dimensions
        if (rel->familiarity < 0.1f) {
            rel->relationship_type = REL_STRANGER;
        } else if (rel->affection > 0.7f && rel->trust > 0.5f) {
            rel->relationship_type = REL_FRIEND;
        } else if (rel->affection < -0.5f || rel->trust < -0.5f) {
            rel->relationship_type = REL_ENEMY;
        } else if (rel->respect > 0.7f && rel->familiarity > 0.5f) {
            rel->relationship_type = REL_MENTOR;
        } else {
            rel->relationship_type = REL_ACQUAINTANCE;
        }
    }
}

// =============================================================================
// SEMANTIC MEMORY INDEXING
// =============================================================================

u32 dnc_semantic_hash(char *content) {
    // Simple but effective string hash for semantic bucketing
    u32 hash = 5381;
    while (*content) {
        hash = ((hash << 5) + hash) + *content++;
    }
    return hash % DNC_SEMANTIC_BUCKETS;
}

internal void index_memory(enhanced_dnc *dnc, dnc_memory *memory) {
    u32 bucket = memory->semantic_hash % DNC_SEMANTIC_BUCKETS;
    
    // Grow bucket if needed
    if (dnc->memory_index.bucket_sizes[bucket] >= dnc->memory_index.bucket_capacity[bucket]) {
        u32 new_capacity = dnc->memory_index.bucket_capacity[bucket] * 2 + 8;
        dnc_memory *new_bucket = realloc(dnc->memory_index.buckets[bucket], 
                                       new_capacity * sizeof(dnc_memory));
        if (new_bucket) {
            dnc->memory_index.buckets[bucket] = new_bucket;
            dnc->memory_index.bucket_capacity[bucket] = new_capacity;
        }
    }
    
    // Add to bucket
    if (dnc->memory_index.bucket_sizes[bucket] < dnc->memory_index.bucket_capacity[bucket]) {
        dnc->memory_index.buckets[bucket][dnc->memory_index.bucket_sizes[bucket]++] = *memory;
    }
}

dnc_memory *dnc_find_similar_memories(enhanced_dnc *dnc, dnc_memory *query, u32 max_results) {
    static dnc_memory results[100];
    u32 result_count = 0;
    
    // Start with the same semantic bucket
    u32 bucket = query->semantic_hash % DNC_SEMANTIC_BUCKETS;
    
    for (u32 i = 0; i < dnc->memory_index.bucket_sizes[bucket] && result_count < max_results; i++) {
        f32 similarity = dnc_memory_similarity(query, &dnc->memory_index.buckets[bucket][i]);
        if (similarity > 0.5f) {
            results[result_count++] = dnc->memory_index.buckets[bucket][i];
        }
    }
    
    // Check adjacent buckets for more results
    u32 adjacent[] = {(bucket - 1) % DNC_SEMANTIC_BUCKETS, (bucket + 1) % DNC_SEMANTIC_BUCKETS};
    for (u32 b = 0; b < 2 && result_count < max_results; b++) {
        u32 check_bucket = adjacent[b];
        for (u32 i = 0; i < dnc->memory_index.bucket_sizes[check_bucket] && result_count < max_results; i++) {
            f32 similarity = dnc_memory_similarity(query, &dnc->memory_index.buckets[check_bucket][i]);
            if (similarity > 0.6f) { // Higher threshold for adjacent buckets
                results[result_count++] = dnc->memory_index.buckets[check_bucket][i];
            }
        }
    }
    
    return result_count > 0 ? results : NULL;
}

f32 dnc_memory_similarity(dnc_memory *a, dnc_memory *b) {
    f32 similarity = 0.0f;
    
    // Semantic similarity (matching hashes)
    if (a->semantic_hash == b->semantic_hash) similarity += 0.3f;
    if (a->actor_hash == b->actor_hash) similarity += 0.2f;
    if (a->action_hash == b->action_hash) similarity += 0.2f;
    
    // Temporal proximity (events close in time)
    f64 time_diff = fabs(a->timestamp - b->timestamp);
    if (time_diff < 300.0) { // Within 5 minutes
        similarity += 0.2f * (1.0f - time_diff / 300.0f);
    }
    
    // Spatial proximity
    f32 dist = sqrtf(powf(a->location.x - b->location.x, 2) +
                    powf(a->location.y - b->location.y, 2) +
                    powf(a->location.z - b->location.z, 2));
    if (dist < 10.0f) {
        similarity += 0.1f * (1.0f - dist / 10.0f);
    }
    
    return similarity;
}

// =============================================================================
// WEIGHTED RESPONSE GENERATION
// =============================================================================

npc_response dnc_generate_response(enhanced_dnc *dnc, response_context *context) {
    npc_response response = {0};
    
    // 1. Memory influence - recall relevant memories
    dnc_memory query = {0};
    strncpy(query.content, context->current_situation, 255);
    query.semantic_hash = dnc_semantic_hash(context->current_situation);
    query.actor_hash = dnc_actor_hash(context->speaker_id);
    
    dnc_memory *relevant = dnc_find_similar_memories(dnc, &query, 5);
    f32 memory_influence = 0.0f;
    
    if (relevant) {
        // Memories affect response based on past outcomes
        for (u32 i = 0; i < 5 && relevant[i].content[0]; i++) {
            memory_influence += relevant[i].emotional_valence * relevant[i].total_importance;
        }
        memory_influence = fmaxf(-1.0f, fminf(1.0f, memory_influence));
    }
    
    // 2. Personality influence
    f32 personality_influence = 0.0f;
    
    if (context->is_combat) {
        personality_influence = dnc->traits.bravery - dnc->traits.neuroticism;
    } else if (context->is_trade) {
        personality_influence = dnc->traits.greed + dnc->traits.conscientiousness;
    } else if (context->is_social) {
        personality_influence = dnc->traits.extraversion + dnc->traits.agreeableness;
    }
    
    // 3. Emotional influence
    f32 emotional_influence = dnc->emotions.happiness - dnc->emotions.anger - dnc->emotions.fear;
    
    // 4. Relationship influence
    f32 relationship_influence = 0.0f;
    relationship *rel = dnc_get_relationship(dnc, context->speaker_id);
    if (rel) {
        relationship_influence = rel->trust * 0.5f + rel->affection * 0.3f + rel->respect * 0.2f;
    }
    
    // Weighted combination
    f32 weights[4] = {0.3f, 0.25f, 0.25f, 0.2f}; // Memory, personality, emotion, relationship
    f32 total_influence = memory_influence * weights[0] +
                         personality_influence * weights[1] +
                         emotional_influence * weights[2] +
                         relationship_influence * weights[3];
    
    // Generate response based on combined influence
    response.memory_influence = memory_influence;
    response.personality_influence = personality_influence;
    response.emotion_influence = emotional_influence;
    response.relationship_influence = relationship_influence;
    response.confidence = fabsf(total_influence);
    
    // Build response text based on influences
    if (total_influence > 0.5f) {
        // Positive response
        if (context->is_trade) {
            snprintf(response.text, 511, "Of course! I'd be happy to trade with you.");
            snprintf(response.action, 127, "ACCEPT_TRADE");
        } else if (context->is_combat) {
            snprintf(response.text, 511, "I'll fight alongside you!");
            snprintf(response.action, 127, "JOIN_COMBAT");
        } else {
            snprintf(response.text, 511, "It's good to see you, %s!", context->speaker_id);
            snprintf(response.action, 127, "GREET_WARMLY");
        }
    } else if (total_influence < -0.5f) {
        // Negative response
        if (context->is_trade) {
            snprintf(response.text, 511, "I don't trust you enough for that.");
            snprintf(response.action, 127, "REJECT_TRADE");
        } else if (context->is_combat) {
            snprintf(response.text, 511, "You're on your own!");
            snprintf(response.action, 127, "FLEE");
        } else {
            snprintf(response.text, 511, "Leave me alone.");
            snprintf(response.action, 127, "DISMISS");
        }
    } else {
        // Neutral response
        snprintf(response.text, 511, "I see. What do you need?");
        snprintf(response.action, 127, "LISTEN");
    }
    
    // Modify for personality verbosity
    if (dnc->traits.verbosity < 0.3f) {
        // Make response shorter
        response.text[20] = '\0';
    }
    
    return response;
}

// =============================================================================
// INTER-NPC COMMUNICATION
// =============================================================================

void dnc_share_gossip(enhanced_dnc *sender, enhanced_dnc *receiver, dnc_memory *memory) {
    if (receiver->gossip_count >= 10) return;
    
    gossip_message *gossip = &receiver->gossip_queue[receiver->gossip_count++];
    strncpy(gossip->source_npc, sender->npc_id, 63);
    gossip->shared_memory = *memory;
    
    // Trust affects how much the gossip is believed
    relationship *rel = dnc_get_relationship(receiver, sender->npc_id);
    gossip->trust_modifier = rel ? rel->trust : 0.0f;
    
    // Add some distortion based on sender's personality
    gossip->distortion = (1.0f - sender->traits.conscientiousness) * 0.2f;
}

void dnc_process_gossip(enhanced_dnc *dnc) {
    for (u32 i = 0; i < dnc->gossip_count; i++) {
        gossip_message *gossip = &dnc->gossip_queue[i];
        
        // Decide whether to internalize this gossip as memory
        f32 believability = 0.5f + gossip->trust_modifier * 0.5f;
        
        if ((f32)rand() / RAND_MAX < believability) {
            // Add to memories with reduced importance
            if (dnc->short_term_count < DNC_SHORT_TERM_SIZE) {
                dnc_memory *new_memory = &dnc->short_term[dnc->short_term_count++];
                *new_memory = gossip->shared_memory;
                new_memory->total_importance *= (0.7f + gossip->trust_modifier * 0.3f);
                
                // Mark as gossip
                char gossip_prefix[64];
                snprintf(gossip_prefix, 63, "[Heard from %s] ", gossip->source_npc);
                memmove(new_memory->content + strlen(gossip_prefix), 
                       new_memory->content, 
                       255 - strlen(gossip_prefix));
                memcpy(new_memory->content, gossip_prefix, strlen(gossip_prefix));
            }
        }
    }
    
    dnc->gossip_count = 0;
}

// =============================================================================
// INITIALIZATION
// =============================================================================

enhanced_dnc *dnc_create(char *npc_id, char *npc_name, personality *traits) {
    enhanced_dnc *dnc = calloc(1, sizeof(enhanced_dnc));
    if (!dnc) return NULL;
    
    strncpy(dnc->npc_id, npc_id, 63);
    strncpy(dnc->npc_name, npc_name, 127);
    dnc->traits = *traits;
    
    // Initialize emotional baseline from personality
    dnc->emotions.baseline_mood = (traits->extraversion + traits->agreeableness - traits->neuroticism) / 3.0f;
    dnc->emotions.emotional_inertia = 1.0f - traits->neuroticism; // Neurotic = fast emotional changes
    
    // Set consolidation parameters
    dnc->consolidation_threshold = 0.4f;
    dnc->forgetting_rate = 0.001f;
    dnc->ewc_lambda = 0.5f;
    
    // Initialize memory index
    for (u32 i = 0; i < DNC_SEMANTIC_BUCKETS; i++) {
        dnc->memory_index.buckets[i] = malloc(8 * sizeof(dnc_memory));
        dnc->memory_index.bucket_capacity[i] = 8;
        dnc->memory_index.bucket_sizes[i] = 0;
    }
    
    return dnc;
}

void dnc_destroy(enhanced_dnc *dnc) {
    if (!dnc) return;
    
    // Free memory index
    for (u32 i = 0; i < DNC_SEMANTIC_BUCKETS; i++) {
        free(dnc->memory_index.buckets[i]);
    }
    
    free(dnc->fisher_information);
    free(dnc->optimal_weights);
    free(dnc);
}

// =============================================================================
// PERSONALITY TEMPLATES
// =============================================================================

personality dnc_personality_template_friendly(void) {
    personality p = {
        .openness = 0.7f,
        .conscientiousness = 0.6f,
        .extraversion = 0.8f,
        .agreeableness = 0.9f,
        .neuroticism = 0.2f,
        .bravery = 0.5f,
        .loyalty = 0.8f,
        .greed = 0.2f,
        .humor = 0.7f,
        .curiosity = 0.6f,
        .aggression = 0.1f,
        .risk_tolerance = 0.4f,
        .social_need = 0.8f,
        .independence = 0.3f,
        .morality = 0.8f,
        .verbosity = 0.6f,
        .formality = 0.3f,
        .emotionality = 0.7f
    };
    return p;
}

personality dnc_personality_template_guard(void) {
    personality p = {
        .openness = 0.3f,
        .conscientiousness = 0.9f,
        .extraversion = 0.4f,
        .agreeableness = 0.4f,
        .neuroticism = 0.3f,
        .bravery = 0.8f,
        .loyalty = 0.95f,
        .greed = 0.1f,
        .humor = 0.2f,
        .curiosity = 0.3f,
        .aggression = 0.6f,
        .risk_tolerance = 0.7f,
        .social_need = 0.3f,
        .independence = 0.2f,
        .morality = 0.7f,
        .verbosity = 0.3f,
        .formality = 0.8f,
        .emotionality = 0.2f
    };
    return p;
}