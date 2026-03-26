#ifndef MOJAVE_RUNTIME_H
#define MOJAVE_RUNTIME_H

#include <stdbool.h>

#include "../flecs/distr/flecs.h"

#include "content.h"

typedef struct MojaveVec2 {
    float x;
    float y;
} MojaveVec2;

typedef struct MojaveInput {
    float move_x;
    float move_y;
    bool save_pressed;
    bool load_pressed;
    bool interact_pressed;
    bool menu_up_pressed;
    bool menu_down_pressed;
    bool quest_log_pressed;
} MojaveInput;

typedef struct Position {
    float x;
    float y;
} Position;

typedef struct Velocity {
    float x;
    float y;
} Velocity;

typedef struct MojaveFlagState {
    char *id;
    bool value;
} MojaveFlagState;

typedef struct MojaveQuestState {
    const MojaveQuestDefinition *definition;
    int stage;
    bool active;
    bool completed;
} MojaveQuestState;

typedef struct MojaveGame {
    ecs_world_t *world;
    ecs_entity_t player;
    MojaveMap map;
    MojaveDialogue dialogue;
    MojaveQuestLog quest_log;
    MojaveQuestState *quest_states;
    int quest_state_count;
    MojaveFlagState *flags;
    int flag_count;
    const char *save_path;
    bool save_loaded;
    const MojaveDialogueNode *active_dialogue_node;
    int active_npc_index;
    int nearby_npc_index;
    int selected_dialogue_choice;
} MojaveGame;

extern ECS_COMPONENT_DECLARE(Position);
extern ECS_COMPONENT_DECLARE(Velocity);

bool mojave_game_init(MojaveGame *game, const char *map_path, const char *save_path);
void mojave_game_shutdown(MojaveGame *game);
void mojave_game_update(MojaveGame *game, const MojaveInput *input, float dt);

const MojaveMap *mojave_game_map(const MojaveGame *game);
MojaveVec2 mojave_game_player_position(const MojaveGame *game);
float mojave_game_player_size(void);
bool mojave_game_save_loaded(const MojaveGame *game);
bool mojave_game_dialogue_active(const MojaveGame *game);
const MojaveDialogueNode *mojave_game_dialogue_node(const MojaveGame *game);
int mojave_game_dialogue_selected_choice(const MojaveGame *game);
int mojave_game_nearby_npc_index(const MojaveGame *game);
int mojave_game_npc_count(const MojaveGame *game);
const MojaveNpc *mojave_game_npc(const MojaveGame *game, int index);
int mojave_game_active_quest_count(const MojaveGame *game);
const MojaveQuestState *mojave_game_active_quest(const MojaveGame *game, int index);
int mojave_game_completed_quest_count(const MojaveGame *game);
const MojaveQuestState *mojave_game_completed_quest(const MojaveGame *game, int index);

#endif
