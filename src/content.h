#ifndef MOJAVE_CONTENT_H
#define MOJAVE_CONTENT_H

#include <stdbool.h>

#define MOJAVE_MAP_NAME_MAX 64

typedef struct MojaveMap {
    char name[MOJAVE_MAP_NAME_MAX];
    int width;
    int height;
    int tile_size;
    int player_spawn_x;
    int player_spawn_y;
    int *tiles;
} MojaveMap;

bool mojave_map_load(const char *path, MojaveMap *map);
void mojave_map_unload(MojaveMap *map);
bool mojave_save_write_player(const char *path, float x, float y);
bool mojave_save_read_player(const char *path, float *x, float *y);

#endif
