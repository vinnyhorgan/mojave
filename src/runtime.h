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
} MojaveInput;

typedef struct Position {
    float x;
    float y;
} Position;

typedef struct Velocity {
    float x;
    float y;
} Velocity;

typedef struct MojaveGame {
    ecs_world_t *world;
    ecs_entity_t player;
    MojaveMap map;
    const char *save_path;
    bool save_loaded;
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

#endif
