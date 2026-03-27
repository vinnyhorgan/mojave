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
    bool inventory_pressed;
} MojaveInput;

typedef struct Position {
    float x;
    float y;
} Position;

typedef struct Velocity {
    float x;
    float y;
} Velocity;

typedef struct CollisionBox {
    float w;
    float h;
} CollisionBox;

typedef struct Renderable {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} Renderable;

typedef struct Team {
    int team_id;
} Team;

typedef struct Hp {
    float current;
    float max;
} Hp;

typedef struct ItemRef {
    const MojaveItemDefinition *definition;
} ItemRef;

typedef struct DialoguRef {
    const MojaveDialogue *dialogue;
    const char *start_id;
} DialoguRef;

typedef struct ActiveDialogue {
    const char *node_id;
} ActiveDialogue;

typedef enum {
    MOJAVE_TEAM_NONE = 0,
    MOJAVE_TEAM_PLAYER = 1,
    MOJAVE_TEAM_FRIENDLY = 2,
    MOJAVE_TEAM_HOSTILE = 4,
} MojaveTeam;

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

typedef struct MojaveInventoryEntry {
    const MojaveItemDefinition *definition;
    int count;
} MojaveInventoryEntry;

typedef struct MojaveGame {
    ecs_world_t *world;
    ecs_entity_t player;
    ecs_query_t *movement_query;
    MojaveMap map;
    MojaveDialogue dialogue;
    MojaveItemDatabase item_database;
    MojaveQuestLog quest_log;
    MojaveQuestState *quest_states;
    int quest_state_count;
    MojaveFlagState *flags;
    int flag_count;
    bool *map_item_collected;
    MojaveInventoryEntry *inventory;
    int inventory_count;
    const char *save_path;
    bool save_loaded;
    const MojaveDialogueNode *active_dialogue_node;
    int active_npc_index;
    int nearby_npc_index;
    int nearby_item_index;
    int selected_dialogue_choice;
} MojaveGame;

extern ECS_COMPONENT_DECLARE(Position);
extern ECS_COMPONENT_DECLARE(Velocity);
extern ECS_COMPONENT_DECLARE(CollisionBox);
extern ECS_COMPONENT_DECLARE(Renderable);
extern ECS_COMPONENT_DECLARE(Team);
extern ECS_COMPONENT_DECLARE(Hp);
extern ECS_COMPONENT_DECLARE(ItemRef);
extern ECS_COMPONENT_DECLARE(DialoguRef);
extern ECS_COMPONENT_DECLARE(ActiveDialogue);

extern const float MOJAVE_PLAYER_SIZE;
extern const float MOJAVE_NPC_SIZE;
extern const float MOJAVE_ITEM_SIZE;

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
int mojave_game_dialogue_visible_choice_count(const MojaveGame *game);
const MojaveDialogueChoice *mojave_game_dialogue_visible_choice(const MojaveGame *game, int index);
int mojave_game_nearby_npc_index(const MojaveGame *game);
int mojave_game_npc_count(const MojaveGame *game);
const MojaveNpc *mojave_game_npc(const MojaveGame *game, int index);
int mojave_game_nearby_item_index(const MojaveGame *game);
int mojave_game_map_item_count(const MojaveGame *game);
const MojaveMapItem *mojave_game_map_item(const MojaveGame *game, int index);
bool mojave_game_map_item_collected(const MojaveGame *game, int index);
int mojave_game_inventory_count(const MojaveGame *game);
const MojaveInventoryEntry *mojave_game_inventory_entry(const MojaveGame *game, int index);
int mojave_game_active_quest_count(const MojaveGame *game);
const MojaveQuestState *mojave_game_active_quest(const MojaveGame *game, int index);
int mojave_game_completed_quest_count(const MojaveGame *game);
const MojaveQuestState *mojave_game_completed_quest(const MojaveGame *game, int index);

ecs_entity_t mojave_game_spawn_player_ecs(MojaveGame *game, float x, float y);
ecs_entity_t mojave_game_spawn_npc_ecs(MojaveGame *game, const MojaveNpc *npc_def, const MojaveDialogue *dialogue);
ecs_entity_t mojave_game_spawn_item_ecs(MojaveGame *game, const MojaveItemDefinition *item_def, float x, float y);

void mojave_game_damage_entity(MojaveGame *game, ecs_entity_t entity, float damage);
float mojave_game_get_entity_hp(MojaveGame *game, ecs_entity_t entity);
bool mojave_game_entity_is_alive(MojaveGame *game, ecs_entity_t entity);

ecs_entity_t mojave_game_get_player(const MojaveGame *game);

typedef struct {
    float x;
    float y;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} MojaveRenderData;

bool mojave_game_get_entity_render_data(const MojaveGame *game, ecs_entity_t entity, MojaveRenderData *out);

typedef struct {
    float x;
    float y;
    float w;
    float h;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
    bool collected;
    const char *name;
} MojaveItemRenderData;

bool mojave_game_get_item_render_data(const MojaveGame *game, int index, MojaveItemRenderData *out);

typedef struct {
    float x;
    float y;
    float w;
    float h;
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
    const char *name;
} MojaveNpcRenderData;

bool mojave_game_get_npc_render_data(const MojaveGame *game, int index, MojaveNpcRenderData *out);

int mojave_map_get_width(const MojaveMap *map);
int mojave_map_get_height(const MojaveMap *map);
int mojave_map_get_tile_size(const MojaveMap *map);
int mojave_map_get_tile(const MojaveMap *map, int x, int y);
const char *mojave_map_get_name(const MojaveMap *map);

#endif
