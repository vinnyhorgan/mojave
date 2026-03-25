#include "runtime.h"

#include <math.h>
#include <stdio.h>
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

static bool mojave_rect_hits_solid(const MojaveMap *map, Rectangle rect) {
    int min_x = (int)floorf(rect.x / (float)map->tile_size);
    int max_x = (int)floorf((rect.x + rect.width - 1.0f) / (float)map->tile_size);
    int min_y = (int)floorf(rect.y / (float)map->tile_size);
    int max_y = (int)floorf((rect.y + rect.height - 1.0f) / (float)map->tile_size);
    int x;
    int y;

    for (y = min_y; y <= max_y; y += 1) {
        for (x = min_x; x <= max_x; x += 1) {
            if (mojave_map_is_solid(map, x, y)) {
                return true;
            }
        }
    }

    return false;
}

static void mojave_move_player(const MojaveMap *map, Position *position, Velocity velocity, float dt) {
    Rectangle body;

    /* The player uses a single rectangle for now so collision stays obvious. */
    body.x = position->x;
    body.y = position->y;
    body.width = MOJAVE_PLAYER_SIZE;
    body.height = MOJAVE_PLAYER_SIZE;

    body.x += velocity.x * dt;
    if (!mojave_rect_hits_solid(map, body)) {
        position->x = body.x;
    } else {
        body.x = position->x;
    }

    body.y += velocity.y * dt;
    if (!mojave_rect_hits_solid(map, body)) {
        position->y = body.y;
    }
}

static Velocity mojave_read_input(void) {
    Velocity velocity = {0.0f, 0.0f};

    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) {
        velocity.x -= 1.0f;
    }
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) {
        velocity.x += 1.0f;
    }
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) {
        velocity.y -= 1.0f;
    }
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) {
        velocity.y += 1.0f;
    }

    if (velocity.x != 0.0f || velocity.y != 0.0f) {
        /* Normalize diagonal movement so every direction moves at the same speed. */
        float length = sqrtf(velocity.x * velocity.x + velocity.y * velocity.y);
        velocity.x = (velocity.x / length) * MOJAVE_PLAYER_SPEED;
        velocity.y = (velocity.y / length) * MOJAVE_PLAYER_SPEED;
    }

    return velocity;
}

static void mojave_draw_map(const MojaveMap *map) {
    int x;
    int y;

    for (y = 0; y < map->height; y += 1) {
        for (x = 0; x < map->width; x += 1) {
            int tile = map->tiles[y * map->width + x];
            Rectangle rect = {
                (float)(x * map->tile_size),
                (float)(y * map->tile_size),
                (float)map->tile_size,
                (float)map->tile_size,
            };

            DrawRectangleRec(rect, tile == 0 ? (Color){60, 92, 65, 255} : (Color){87, 73, 54, 255});
            DrawRectangleLinesEx(rect, 1.0f, (Color){0, 0, 0, 35});
        }
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

    game->camera.zoom = 1.0f;
    game->camera.offset = (Vector2){640.0f / 2.0f, 360.0f / 2.0f};
    game->camera.target = (Vector2){position->x, position->y};

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

void mojave_game_update(MojaveGame *game, float dt) {
    Position *position;
    Velocity velocity;

    position = ecs_get_mut(game->world, game->player, Position);
    velocity = mojave_read_input();

    ecs_set(game->world, game->player, Velocity, { velocity.x, velocity.y });

    /* Update flow stays explicit: read input, move, then refresh the camera. */
    mojave_move_player(&game->map, position, velocity, dt);

    game->camera.target.x = position->x + MOJAVE_PLAYER_SIZE * 0.5f;
    game->camera.target.y = position->y + MOJAVE_PLAYER_SIZE * 0.5f;

    if (IsKeyPressed(KEY_F5)) {
        mojave_save_write_player(game->save_path, position->x, position->y);
    }

    if (IsKeyPressed(KEY_F9)) {
        float save_x;
        float save_y;

        if (mojave_save_read_player(game->save_path, &save_x, &save_y)) {
            position->x = save_x;
            position->y = save_y;
            game->save_loaded = true;
        }
    }
}

void mojave_game_draw(const MojaveGame *game) {
    const Position *position;
    Rectangle player_rect;
    int map_width_px;
    int map_height_px;

    position = ecs_get(game->world, game->player, Position);
    map_width_px = game->map.width * game->map.tile_size;
    map_height_px = game->map.height * game->map.tile_size;
    player_rect = (Rectangle){position->x, position->y, MOJAVE_PLAYER_SIZE, MOJAVE_PLAYER_SIZE};

    BeginDrawing();
    ClearBackground((Color){205, 197, 176, 255});

    BeginMode2D(game->camera);
    mojave_draw_map(&game->map);
    DrawRectangleRec(player_rect, (Color){42, 122, 184, 255});
    DrawRectangleLinesEx(player_rect, 2.0f, (Color){14, 37, 58, 255});
    DrawRectangleLines(0, 0, map_width_px, map_height_px, BLACK);
    EndMode2D();

    DrawRectangle(12, 12, 300, 102, Fade(RAYWHITE, 0.85f));
    DrawRectangleLines(12, 12, 300, 102, DARKGRAY);
    DrawText(game->map.name, 24, 24, 20, BLACK);
    DrawText("Move: WASD / Arrows", 24, 52, 18, BLACK);
    DrawText("Save: F5   Load: F9", 24, 74, 18, BLACK);
    DrawText(game->save_loaded ? "Save file found" : "No save loaded yet", 24, 96, 18, DARKGRAY);

    EndDrawing();
}
