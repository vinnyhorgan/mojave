#ifndef MOJAVE_COLLISION_H
#define MOJAVE_COLLISION_H

#include <stdbool.h>

#include "content.h"

typedef struct MojaveRect {
    float x;
    float y;
    float w;
    float h;
} MojaveRect;

typedef struct MojaveCollisionMoveResult {
    float x;
    float y;
    bool collided;
} MojaveCollisionMoveResult;

bool mojave_collision_move_rect(const MojaveMap *map,
    const MojaveRect *rect,
    float goal_x,
    float goal_y,
    MojaveCollisionMoveResult *result);

#endif
