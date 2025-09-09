/*
    Default Achievement Registrations
    Registers comprehensive set of standard game achievements
*/

#include "handmade_achievements.h"
#include <stdio.h>

#define internal static

// Story/Campaign achievements
void
achievements_register_story_achievements(achievement_system *system) {
    // Progress-based story achievements
    achievements_register_unlock(system, "first_steps", "First Steps",
                                "Complete the tutorial", CATEGORY_STORY);
    
    achievements_register_unlock(system, "chapter_1", "Chapter One",
                                "Complete Chapter 1", CATEGORY_STORY);
    
    achievements_register_unlock(system, "chapter_2", "Into the Unknown", 
                                "Complete Chapter 2", CATEGORY_STORY);
    
    achievements_register_unlock(system, "chapter_3", "Rising Action",
                                "Complete Chapter 3", CATEGORY_STORY);
    
    achievements_register_unlock(system, "chapter_4", "The Climax",
                                "Complete Chapter 4", CATEGORY_STORY);
    
    achievements_register_unlock(system, "finale", "The End",
                                "Complete the main story", CATEGORY_STORY);
    
    // Difficulty-based completions
    achievements_register_unlock(system, "story_normal", "Seasoned Adventurer",
                                "Complete story on Normal difficulty", CATEGORY_STORY);
    
    achievements_register_unlock(system, "story_hard", "Veteran Explorer", 
                                "Complete story on Hard difficulty", CATEGORY_STORY);
    
    achievements_register_unlock(system, "story_nightmare", "Legendary Hero",
                                "Complete story on Nightmare difficulty", CATEGORY_STORY);
    
    // Time-based story achievements
    achievements_register_progress(system, "speedrun_story", "Speed Demon",
                                 "Complete story in under 4 hours",
                                 CATEGORY_STORY, "story_completion_time", 4.0f * 3600.0f);
}

// Combat achievements
void
achievements_register_combat_achievements(achievement_system *system) {
    // Kill count achievements
    achievements_register_counter(system, "first_blood", "First Blood",
                                "Defeat your first enemy", CATEGORY_COMBAT,
                                "enemies_killed", 1);
    
    achievements_register_counter(system, "slayer", "Slayer",
                                "Defeat 100 enemies", CATEGORY_COMBAT,
                                "enemies_killed", 100);
    
    achievements_register_counter(system, "destroyer", "Destroyer",
                                "Defeat 500 enemies", CATEGORY_COMBAT,
                                "enemies_killed", 500);
    
    achievements_register_counter(system, "annihilator", "Annihilator",
                                "Defeat 1000 enemies", CATEGORY_COMBAT,
                                "enemies_killed", 1000);
    
    achievements_register_counter(system, "genocide", "Bringer of Extinction", 
                                "Defeat 5000 enemies", CATEGORY_COMBAT,
                                "enemies_killed", 5000);
    
    // Boss achievements
    achievements_register_counter(system, "boss_slayer", "Boss Slayer",
                                "Defeat 10 bosses", CATEGORY_COMBAT,
                                "bosses_killed", 10);
    
    achievements_register_counter(system, "apex_predator", "Apex Predator",
                                "Defeat 25 bosses", CATEGORY_COMBAT,
                                "bosses_killed", 25);
    
    // Weapon mastery
    achievements_register_counter(system, "sword_master", "Sword Master",
                                "Kill 250 enemies with swords", CATEGORY_COMBAT,
                                "sword_kills", 250);
    
    achievements_register_counter(system, "bow_master", "Archer Supreme", 
                                "Kill 250 enemies with bows", CATEGORY_COMBAT,
                                "bow_kills", 250);
    
    achievements_register_counter(system, "magic_master", "Archmage",
                                "Kill 250 enemies with magic", CATEGORY_COMBAT,
                                "magic_kills", 250);
    
    // Special combat achievements
    achievements_register_unlock(system, "perfectionist", "Perfectionist",
                                "Complete a level without taking damage", CATEGORY_COMBAT);
    
    achievements_register_progress(system, "combo_master", "Combo Master",
                                 "Achieve a 50-hit combo", CATEGORY_COMBAT,
                                 "max_combo", 50.0f);
    
    achievements_register_progress(system, "critical_expert", "Critical Expert", 
                                 "Land 100 critical hits", CATEGORY_COMBAT,
                                 "critical_hits", 100.0f);
}

// Exploration achievements  
void
achievements_register_exploration_achievements(achievement_system *system) {
    // Distance traveled
    achievements_register_progress(system, "wanderer", "Wanderer",
                                 "Travel 10 kilometers", CATEGORY_EXPLORATION,
                                 "distance_traveled", 10000.0f);
    
    achievements_register_progress(system, "explorer", "Explorer",
                                 "Travel 50 kilometers", CATEGORY_EXPLORATION,
                                 "distance_traveled", 50000.0f);
    
    achievements_register_progress(system, "nomad", "Nomad",
                                 "Travel 100 kilometers", CATEGORY_EXPLORATION,
                                 "distance_traveled", 100000.0f);
    
    // Area discovery
    achievements_register_progress(system, "scout", "Scout",
                                 "Discover 25% of the world", CATEGORY_EXPLORATION,
                                 "areas_discovered_percent", 25.0f);
    
    achievements_register_progress(system, "cartographer", "Cartographer",
                                 "Discover 75% of the world", CATEGORY_EXPLORATION,
                                 "areas_discovered_percent", 75.0f);
    
    achievements_register_progress(system, "completionist", "World Walker",
                                 "Discover 100% of the world", CATEGORY_EXPLORATION,
                                 "areas_discovered_percent", 100.0f);
    
    // Secrets and hidden areas
    achievements_register_counter(system, "secret_hunter", "Secret Hunter",
                                "Find 10 secret areas", CATEGORY_EXPLORATION,
                                "secrets_found", 10);
    
    achievements_register_counter(system, "master_detective", "Master Detective",
                                "Find 25 secret areas", CATEGORY_EXPLORATION,
                                "secrets_found", 25);
    
    // Special locations
    achievements_register_counter(system, "landmark_visitor", "Landmark Visitor",
                                "Visit 20 landmarks", CATEGORY_EXPLORATION,
                                "landmarks_visited", 20);
    
    achievements_register_unlock(system, "mountain_climber", "Mountain Climber",
                                "Reach the highest point in the world", CATEGORY_EXPLORATION);
    
    achievements_register_unlock(system, "deep_diver", "Deep Diver",
                                "Reach the lowest point in the world", CATEGORY_EXPLORATION);
}

// Collection achievements
void
achievements_register_collection_achievements(achievement_system *system) {
    // General item collection
    achievements_register_counter(system, "hoarder", "Hoarder",
                                "Collect 100 items", CATEGORY_COLLECTION,
                                "items_collected", 100);
    
    achievements_register_counter(system, "collector", "Collector", 
                                "Collect 500 items", CATEGORY_COLLECTION,
                                "items_collected", 500);
    
    achievements_register_counter(system, "packrat", "Packrat",
                                "Collect 1000 items", CATEGORY_COLLECTION,
                                "items_collected", 1000);
    
    // Specific collectibles
    achievements_register_counter(system, "treasure_hunter", "Treasure Hunter",
                                "Find 50 treasure chests", CATEGORY_COLLECTION,
                                "chests_opened", 50);
    
    achievements_register_counter(system, "coin_collector", "Coin Collector",
                                "Collect 10,000 coins", CATEGORY_COLLECTION,
                                "coins_collected", 10000);
    
    achievements_register_counter(system, "gem_enthusiast", "Gem Enthusiast",
                                "Collect 100 precious gems", CATEGORY_COLLECTION,
                                "gems_collected", 100);
    
    // Rare items
    achievements_register_counter(system, "rare_finder", "Rare Finder",
                                "Find 10 rare items", CATEGORY_COLLECTION,
                                "rare_items_found", 10);
    
    achievements_register_counter(system, "legendary_seeker", "Legendary Seeker",
                                "Find 5 legendary items", CATEGORY_COLLECTION,
                                "legendary_items_found", 5);
    
    // Equipment collection
    achievements_register_counter(system, "armory", "Living Armory",
                                "Collect 50 different weapons", CATEGORY_COLLECTION,
                                "unique_weapons", 50);
    
    achievements_register_counter(system, "fashionista", "Fashionista",
                                "Collect 25 different armor sets", CATEGORY_COLLECTION,
                                "unique_armor_sets", 25);
    
    // Completionist collections
    achievements_register_progress(system, "complete_collection", "Perfect Collection",
                                 "Collect 100% of all collectibles", CATEGORY_COLLECTION,
                                 "collection_percentage", 100.0f);
}

// Skill-based achievements
void
achievements_register_skill_achievements(achievement_system *system) {
    // Leveling achievements
    achievements_register_progress(system, "novice", "Novice",
                                 "Reach level 10", CATEGORY_SKILL,
                                 "player_level", 10.0f);
    
    achievements_register_progress(system, "adept", "Adept",
                                 "Reach level 25", CATEGORY_SKILL,
                                 "player_level", 25.0f);
    
    achievements_register_progress(system, "expert", "Expert",
                                 "Reach level 50", CATEGORY_SKILL,
                                 "player_level", 50.0f);
    
    achievements_register_progress(system, "master", "Master",
                                 "Reach level 75", CATEGORY_SKILL,
                                 "player_level", 75.0f);
    
    achievements_register_progress(system, "grandmaster", "Grandmaster",
                                 "Reach maximum level", CATEGORY_SKILL,
                                 "player_level", 100.0f);
    
    // Skill trees
    achievements_register_unlock(system, "skill_specialist", "Specialist",
                                "Max out one skill tree", CATEGORY_SKILL);
    
    achievements_register_unlock(system, "skill_master", "Jack of All Trades",
                                "Max out three skill trees", CATEGORY_SKILL);
    
    achievements_register_unlock(system, "skill_grandmaster", "Renaissance Soul",
                                "Max out all skill trees", CATEGORY_SKILL);
    
    // Crafting skills
    achievements_register_counter(system, "apprentice_crafter", "Apprentice Crafter",
                                "Craft 25 items", CATEGORY_SKILL,
                                "items_crafted", 25);
    
    achievements_register_counter(system, "master_crafter", "Master Crafter",
                                "Craft 100 items", CATEGORY_SKILL,
                                "items_crafted", 100);
    
    achievements_register_counter(system, "legendary_crafter", "Legendary Crafter",
                                "Craft 500 items", CATEGORY_SKILL,
                                "items_crafted", 500);
    
    // Challenge achievements
    achievements_register_unlock(system, "no_death_run", "Deathless",
                                "Complete game without dying", CATEGORY_SKILL);
    
    achievements_register_unlock(system, "pacifist", "Pacifist",
                                "Complete game without killing anyone", CATEGORY_SKILL);
    
    achievements_register_unlock(system, "minimalist", "Minimalist",
                                "Complete game using only starter equipment", CATEGORY_SKILL);
}

// Social/Multiplayer achievements  
void
achievements_register_social_achievements(achievement_system *system) {
    achievements_register_counter(system, "social_butterfly", "Social Butterfly",
                                "Play with 10 different players", CATEGORY_SOCIAL,
                                "unique_coop_players", 10);
    
    achievements_register_counter(system, "team_player", "Team Player",
                                "Complete 25 multiplayer missions", CATEGORY_SOCIAL,
                                "coop_missions_complete", 25);
    
    achievements_register_counter(system, "pvp_novice", "PvP Novice",
                                "Win 10 PvP matches", CATEGORY_SOCIAL,
                                "pvp_wins", 10);
    
    achievements_register_counter(system, "pvp_veteran", "PvP Veteran", 
                                "Win 100 PvP matches", CATEGORY_SOCIAL,
                                "pvp_wins", 100);
    
    achievements_register_unlock(system, "helping_hand", "Helping Hand",
                                "Revive 50 teammates", CATEGORY_SOCIAL);
}

// Meta achievements (achievements about achievements)
void
achievements_register_meta_achievements(achievement_system *system) {
    achievements_register_counter(system, "achiever", "Achiever",
                                "Unlock 10 achievements", CATEGORY_META,
                                "achievements_unlocked", 10);
    
    achievements_register_counter(system, "completionist_bronze", "Bronze Completionist",
                                "Unlock 25 achievements", CATEGORY_META,
                                "achievements_unlocked", 25);
    
    achievements_register_counter(system, "completionist_silver", "Silver Completionist",
                                "Unlock 50 achievements", CATEGORY_META,
                                "achievements_unlocked", 50);
    
    achievements_register_counter(system, "completionist_gold", "Gold Completionist",
                                "Unlock 75 achievements", CATEGORY_META,
                                "achievements_unlocked", 75);
    
    achievements_register_counter(system, "completionist_platinum", "Platinum Completionist",
                                "Unlock all achievements", CATEGORY_META,
                                "achievements_unlocked", 100);
    
    // Category completion
    achievements_register_unlock(system, "story_complete", "Story Master",
                                "Complete all story achievements", CATEGORY_META);
    
    achievements_register_unlock(system, "combat_complete", "Combat Master",
                                "Complete all combat achievements", CATEGORY_META);
    
    achievements_register_unlock(system, "exploration_complete", "Exploration Master",
                                "Complete all exploration achievements", CATEGORY_META);
}

// Register all default statistics
internal void
achievements_register_default_stats(achievement_system *system) {
    // Core gameplay stats
    achievements_register_stat(system, "enemies_killed", "Enemies Defeated", STAT_INT);
    achievements_register_stat(system, "bosses_killed", "Bosses Defeated", STAT_INT);
    achievements_register_stat(system, "player_deaths", "Deaths", STAT_INT);
    achievements_register_stat(system, "player_level", "Character Level", STAT_INT);
    achievements_register_stat(system, "playtime_minutes", "Playtime (minutes)", STAT_INT);
    
    // Combat stats
    achievements_register_stat(system, "damage_dealt", "Total Damage Dealt", STAT_FLOAT);
    achievements_register_stat(system, "damage_taken", "Total Damage Taken", STAT_FLOAT);
    achievements_register_stat(system, "critical_hits", "Critical Hits", STAT_INT);
    achievements_register_stat(system, "max_combo", "Max Combo", STAT_INT);
    achievements_register_stat(system, "sword_kills", "Sword Kills", STAT_INT);
    achievements_register_stat(system, "bow_kills", "Bow Kills", STAT_INT);
    achievements_register_stat(system, "magic_kills", "Magic Kills", STAT_INT);
    
    // Exploration stats
    achievements_register_stat(system, "distance_traveled", "Distance Traveled (m)", STAT_FLOAT);
    achievements_register_stat(system, "areas_discovered", "Areas Discovered", STAT_INT);
    achievements_register_stat(system, "areas_discovered_percent", "World Discovery %", STAT_FLOAT);
    achievements_register_stat(system, "secrets_found", "Secret Areas Found", STAT_INT);
    achievements_register_stat(system, "landmarks_visited", "Landmarks Visited", STAT_INT);
    
    // Collection stats
    achievements_register_stat(system, "items_collected", "Items Collected", STAT_INT);
    achievements_register_stat(system, "chests_opened", "Chests Opened", STAT_INT);
    achievements_register_stat(system, "coins_collected", "Coins Collected", STAT_INT);
    achievements_register_stat(system, "gems_collected", "Gems Collected", STAT_INT);
    achievements_register_stat(system, "rare_items_found", "Rare Items Found", STAT_INT);
    achievements_register_stat(system, "legendary_items_found", "Legendary Items Found", STAT_INT);
    achievements_register_stat(system, "unique_weapons", "Unique Weapons", STAT_INT);
    achievements_register_stat(system, "unique_armor_sets", "Unique Armor Sets", STAT_INT);
    achievements_register_stat(system, "collection_percentage", "Collection %", STAT_FLOAT);
    
    // Skill stats
    achievements_register_stat(system, "skills_learned", "Skills Learned", STAT_INT);
    achievements_register_stat(system, "skill_points_earned", "Skill Points Earned", STAT_INT);
    achievements_register_stat(system, "items_crafted", "Items Crafted", STAT_INT);
    
    // Time-based stats
    achievements_register_stat(system, "story_completion_time", "Story Completion Time", STAT_TIME);
    achievements_register_stat(system, "fastest_level_time", "Fastest Level Time", STAT_TIME);
    
    // Social stats
    achievements_register_stat(system, "unique_coop_players", "Unique Co-op Players", STAT_INT);
    achievements_register_stat(system, "coop_missions_complete", "Co-op Missions Complete", STAT_INT);
    achievements_register_stat(system, "pvp_wins", "PvP Wins", STAT_INT);
    achievements_register_stat(system, "pvp_losses", "PvP Losses", STAT_INT);
    
    // Meta stats
    achievements_register_stat(system, "achievements_unlocked", "Achievements Unlocked", STAT_INT);
    achievements_register_stat(system, "achievement_points", "Achievement Points", STAT_INT);
}

// Register all default achievements and statistics
void
achievements_register_all_defaults(achievement_system *system) {
    // Register statistics first (achievements depend on them)
    achievements_register_default_stats(system);
    
    // Register achievements by category
    achievements_register_story_achievements(system);
    achievements_register_combat_achievements(system);
    achievements_register_exploration_achievements(system);
    achievements_register_collection_achievements(system);
    achievements_register_skill_achievements(system);
    achievements_register_social_achievements(system);
    achievements_register_meta_achievements(system);
    
    printf("Registered %u achievements and %u statistics\\n",
           system->achievement_count, system->stat_count);
}