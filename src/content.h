#ifndef MOJAVE_CONTENT_H
#define MOJAVE_CONTENT_H

#include <stdbool.h>

#define MOJAVE_MAP_NAME_MAX 64

typedef enum MojaveEventActionType {
    MOJAVE_EVENT_ACTION_NONE = 0,
    MOJAVE_EVENT_ACTION_SET_FLAG,
    MOJAVE_EVENT_ACTION_START_QUEST,
    MOJAVE_EVENT_ACTION_SET_QUEST_STAGE,
    MOJAVE_EVENT_ACTION_COMPLETE_QUEST,
    MOJAVE_EVENT_ACTION_GIVE_ITEM,
    MOJAVE_EVENT_ACTION_REMOVE_ITEM,
} MojaveEventActionType;

typedef enum MojaveConditionType {
    MOJAVE_CONDITION_NONE = 0,
    MOJAVE_CONDITION_HAS_ITEM,
} MojaveConditionType;

typedef struct MojaveCondition {
    MojaveConditionType type;
    char *item_id;
    int item_count;
} MojaveCondition;

typedef struct MojaveEventAction {
    MojaveEventActionType type;
    char *flag_id;
    bool flag_value;
    char *quest_id;
    char *item_id;
    int item_count;
    int quest_stage;
} MojaveEventAction;

typedef struct MojaveQuestDefinition {
    char *id;
    char *title;
    char **stages;
    int stage_count;
} MojaveQuestDefinition;

typedef struct MojaveQuestLog {
    MojaveQuestDefinition *quests;
    int quest_count;
} MojaveQuestLog;

typedef struct MojaveDialogueChoice {
    char *text;
    char *next_id;
    MojaveCondition *conditions;
    int condition_count;
    MojaveEventAction *actions;
    int action_count;
} MojaveDialogueChoice;

typedef struct MojaveDialogueNode {
    char *id;
    char *speaker;
    char *text;
    char *next_id;
    MojaveCondition *conditions;
    int condition_count;
    MojaveEventAction *actions;
    int action_count;
    MojaveDialogueChoice *choices;
    int choice_count;
    bool is_end;
} MojaveDialogueNode;

typedef struct MojaveDialogue {
    char *start_id;
    MojaveDialogueNode *nodes;
    int node_count;
} MojaveDialogue;

typedef struct MojaveNpc {
    char *id;
    char *name;
    char *dialogue_path;
    int spawn_x;
    int spawn_y;
    int sprite_width;
    int sprite_height;
    int origin_x;
    int origin_y;
    unsigned char body_r;
    unsigned char body_g;
    unsigned char body_b;
    unsigned char outfit_r;
    unsigned char outfit_g;
    unsigned char outfit_b;
    unsigned char accent_r;
    unsigned char accent_g;
    unsigned char accent_b;
    unsigned char skin_r;
    unsigned char skin_g;
    unsigned char skin_b;
} MojaveNpc;

typedef struct MojaveItemDefinition {
    char *id;
    char *name;
    char *description;
    bool stackable;
    unsigned char color_r;
    unsigned char color_g;
    unsigned char color_b;
} MojaveItemDefinition;

typedef struct MojaveItemDatabase {
    MojaveItemDefinition *items;
    int item_count;
} MojaveItemDatabase;

typedef struct MojaveMapItem {
    char *item_id;
    int spawn_x;
    int spawn_y;
} MojaveMapItem;

typedef struct MojaveMap {
    char name[MOJAVE_MAP_NAME_MAX];
    int width;
    int height;
    int tile_size;
    int player_spawn_x;
    int player_spawn_y;
    int *tiles;
    MojaveNpc *npcs;
    int npc_count;
    MojaveMapItem *items;
    int item_count;
} MojaveMap;

bool mojave_map_load(const char *path, MojaveMap *map);
void mojave_map_unload(MojaveMap *map);
bool mojave_item_database_load(const char *path, MojaveItemDatabase *item_database);
void mojave_item_database_unload(MojaveItemDatabase *item_database);
const MojaveItemDefinition *mojave_item_database_find(const MojaveItemDatabase *item_database, const char *id);
bool mojave_quest_log_load(const char *path, MojaveQuestLog *quest_log);
void mojave_quest_log_unload(MojaveQuestLog *quest_log);
const MojaveQuestDefinition *mojave_quest_log_find(const MojaveQuestLog *quest_log, const char *id);
bool mojave_dialogue_load(const char *path, MojaveDialogue *dialogue);
void mojave_dialogue_unload(MojaveDialogue *dialogue);
const MojaveDialogueNode *mojave_dialogue_find_node(const MojaveDialogue *dialogue, const char *id);
bool mojave_save_write_player(const char *path, float x, float y);
bool mojave_save_read_player(const char *path, float *x, float *y);

#endif
