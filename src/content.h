#ifndef MOJAVE_CONTENT_H
#define MOJAVE_CONTENT_H

#include <stdbool.h>

#define MOJAVE_MAP_NAME_MAX 64

typedef struct MojaveDialogueChoice {
    char *text;
    char *next_id;
} MojaveDialogueChoice;

typedef struct MojaveDialogueNode {
    char *id;
    char *speaker;
    char *text;
    char *next_id;
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
} MojaveNpc;

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
} MojaveMap;

bool mojave_map_load(const char *path, MojaveMap *map);
void mojave_map_unload(MojaveMap *map);
bool mojave_dialogue_load(const char *path, MojaveDialogue *dialogue);
void mojave_dialogue_unload(MojaveDialogue *dialogue);
const MojaveDialogueNode *mojave_dialogue_find_node(const MojaveDialogue *dialogue, const char *id);
bool mojave_save_write_player(const char *path, float x, float y);
bool mojave_save_read_player(const char *path, float *x, float *y);

#endif
