#include "runtime.h"

#include <math.h>
#include <string.h>

static const float MOJAVE_PLAYER_SIZE = 18.0f;
static const float MOJAVE_PLAYER_SPEED = 180.0f;

ECS_COMPONENT_DECLARE(Position) = 0;
ECS_COMPONENT_DECLARE(Velocity) = 0;

static int mojave_map_tile_at(const MojaveMap *map, int x, int y) {
    if (x < 0 || y < 0 || x >= map->width || y >= map->height) {
        return 1;
    }

    return map->tiles[y * map->width + x];
}

static bool mojave_map_is_solid(const MojaveMap *map, int x, int y) {
    return mojave_map_tile_at(map, x, y) != 0;
}

static bool mojave_rect_hits_solid(const MojaveMap *map, float x, float y, float width, float height) {
    int min_x = (int)floorf(x / (float)map->tile_size);
    int max_x = (int)floorf((x + width - 1.0f) / (float)map->tile_size);
    int min_y = (int)floorf(y / (float)map->tile_size);
    int max_y = (int)floorf((y + height - 1.0f) / (float)map->tile_size);
    int tile_x;
    int tile_y;

    for (tile_y = min_y; tile_y <= max_y; tile_y += 1) {
        for (tile_x = min_x; tile_x <= max_x; tile_x += 1) {
            if (mojave_map_is_solid(map, tile_x, tile_y)) {
                return true;
            }
        }
    }

    return false;
}

static Velocity mojave_velocity_from_input(const MojaveInput *input) {
    Velocity velocity = {0.0f, 0.0f};

    if (input == NULL) {
        return velocity;
    }

    velocity.x = input->move_x;
    velocity.y = input->move_y;

    if (velocity.x != 0.0f || velocity.y != 0.0f) {
        float length = sqrtf(velocity.x * velocity.x + velocity.y * velocity.y);

        /* Normalize diagonal movement so every direction moves at the same speed. */
        velocity.x = (velocity.x / length) * MOJAVE_PLAYER_SPEED;
        velocity.y = (velocity.y / length) * MOJAVE_PLAYER_SPEED;
    }

    return velocity;
}

static void mojave_move_player(const MojaveMap *map, Position *position, Velocity velocity, float dt) {
    float next_x;
    float next_y;

    if (position == NULL) {
        return;
    }

    /* The player uses a single rectangle for now so collision stays obvious. */
    next_x = position->x + velocity.x * dt;
    next_y = position->y + velocity.y * dt;

    if (!mojave_rect_hits_solid(map, next_x, position->y, MOJAVE_PLAYER_SIZE, MOJAVE_PLAYER_SIZE)) {
        position->x = next_x;
    }

    if (!mojave_rect_hits_solid(map, position->x, next_y, MOJAVE_PLAYER_SIZE, MOJAVE_PLAYER_SIZE)) {
        position->y = next_y;
    }
}

bool mojave_game_init(MojaveGame *game, const char *map_path, const char *save_path) {
    Position *position;

    if (game == NULL) {
        return false;
    }

    memset(game, 0, sizeof(*game));
    game->save_path = save_path;

    if (!mojave_map_load(map_path, &game->map)) {
        return false;
    }

    game->world = ecs_init();
    if (game->world == NULL) {
        mojave_map_unload(&game->map);
        return false;
    }

    ECS_COMPONENT_DEFINE(game->world, Position);
    ECS_COMPONENT_DEFINE(game->world, Velocity);

    /* ECS is intentionally used very lightly here: one player entity, two plain components. */
    game->player = ecs_new(game->world);
    ecs_set(game->world, game->player, Position,
        { (float)(game->map.player_spawn_x * game->map.tile_size + 7),
          (float)(game->map.player_spawn_y * game->map.tile_size + 7) });
    ecs_set(game->world, game->player, Velocity, {0.0f, 0.0f});

    position = ecs_get_mut(game->world, game->player, Position);
    if (mojave_save_read_player(save_path, &position->x, &position->y)) {
        game->save_loaded = true;
    }

    return true;
}

void mojave_game_shutdown(MojaveGame *game) {
    if (game == NULL) {
        return;
    }

    if (game->world != NULL) {
        ecs_fini(game->world);
        game->world = NULL;
    }

    mojave_map_unload(&game->map);
}

void mojave_game_update(MojaveGame *game, const MojaveInput *input, float dt) {
    Position *position;
    Velocity velocity;

    if (game == NULL || game->world == NULL) {
        return;
    }

    position = ecs_get_mut(game->world, game->player, Position);
    velocity = mojave_velocity_from_input(input);
    ecs_set(game->world, game->player, Velocity, {velocity.x, velocity.y});

    /* Update flow stays explicit: read input, move, then handle save or load. */
    mojave_move_player(&game->map, position, velocity, dt);

    if (input != NULL && input->save_pressed) {
        mojave_save_write_player(game->save_path, position->x, position->y);
    }

    if (input != NULL && input->load_pressed) {
        float save_x;
        float save_y;

        if (mojave_save_read_player(game->save_path, &save_x, &save_y)) {
            position->x = save_x;
            position->y = save_y;
            game->save_loaded = true;
        }
    }
}

const MojaveMap *mojave_game_map(const MojaveGame *game) {
    if (game == NULL) {
        return NULL;
    }

    return &game->map;
}

MojaveVec2 mojave_game_player_position(const MojaveGame *game) {
    const Position *position;

    if (game == NULL || game->world == NULL) {
        return (MojaveVec2){0.0f, 0.0f};
    }

    position = ecs_get(game->world, game->player, Position);
    if (position == NULL) {
        return (MojaveVec2){0.0f, 0.0f};
    }

    return (MojaveVec2){position->x, position->y};
}

float mojave_game_player_size(void) {
    return MOJAVE_PLAYER_SIZE;
}

bool mojave_game_save_loaded(const MojaveGame *game) {
    if (game == NULL) {
        return false;
    }

    return game->save_loaded;
}
