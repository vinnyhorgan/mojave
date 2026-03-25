#ifndef MOJAVE_RUNTIME_H
#define MOJAVE_RUNTIME_H

#include <stdbool.h>

#include "../flecs/distr/flecs.h"
#include <raylib.h>

#include "content.h"

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
    Camera2D camera;
    const char *save_path;
    bool save_loaded;
} MojaveGame;

extern ECS_COMPONENT_DECLARE(Position);
extern ECS_COMPONENT_DECLARE(Velocity);

bool mojave_game_init(MojaveGame *game, const char *map_path, const char *save_path);
void mojave_game_shutdown(MojaveGame *game);
void mojave_game_update(MojaveGame *game, float dt);
void mojave_game_draw(const MojaveGame *game);

#endif
