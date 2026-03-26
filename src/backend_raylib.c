#include "backend.h"

#include <raylib.h>

static Camera2D g_camera;

static Color mojave_floor_color(void) {
    return (Color){60, 92, 65, 255};
}

static Color mojave_wall_color(void) {
    return (Color){87, 73, 54, 255};
}

static void mojave_backend_update_camera(const MojaveGame *game) {
    MojaveVec2 player_position = mojave_game_player_position(game);
    float player_size = mojave_game_player_size();

    g_camera.offset = (Vector2){(float)GetScreenWidth() * 0.5f, (float)GetScreenHeight() * 0.5f};
    g_camera.target = (Vector2){
        player_position.x + player_size * 0.5f,
        player_position.y + player_size * 0.5f
    };
}

static void mojave_backend_draw_map(const MojaveMap *map) {
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

            DrawRectangleRec(rect, tile == 0 ? mojave_floor_color() : mojave_wall_color());
            DrawRectangleLinesEx(rect, 1.0f, (Color){0, 0, 0, 35});
        }
    }
}

bool mojave_backend_init(const MojaveBackendConfig *config) {
    if (config == NULL) {
        return false;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(config->window_width, config->window_height, config->window_title);
    SetTargetFPS(60);

    g_camera.zoom = 1.0f;
    g_camera.rotation = 0.0f;
    g_camera.offset = (Vector2){(float)config->window_width * 0.5f, (float)config->window_height * 0.5f};
    g_camera.target = (Vector2){0.0f, 0.0f};
    return true;
}

void mojave_backend_shutdown(void) {
    CloseWindow();
}

bool mojave_backend_should_close(void) {
    return WindowShouldClose();
}

float mojave_backend_frame_time(void) {
    return GetFrameTime();
}

MojaveInput mojave_backend_poll_input(void) {
    MojaveInput input = {0.0f, 0.0f, false, false};

    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) {
        input.move_x -= 1.0f;
    }
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) {
        input.move_x += 1.0f;
    }
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) {
        input.move_y -= 1.0f;
    }
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) {
        input.move_y += 1.0f;
    }

    input.save_pressed = IsKeyPressed(KEY_F5);
    input.load_pressed = IsKeyPressed(KEY_F9);
    return input;
}

void mojave_backend_draw(const MojaveGame *game) {
    const MojaveMap *map;
    MojaveVec2 player_position;
    float player_size;
    int map_width_px;
    int map_height_px;
    Rectangle player_rect;

    if (game == NULL) {
        return;
    }

    map = mojave_game_map(game);
    player_position = mojave_game_player_position(game);
    player_size = mojave_game_player_size();

    mojave_backend_update_camera(game);

    map_width_px = map->width * map->tile_size;
    map_height_px = map->height * map->tile_size;
    player_rect = (Rectangle){player_position.x, player_position.y, player_size, player_size};

    BeginDrawing();
    ClearBackground((Color){205, 197, 176, 255});

    BeginMode2D(g_camera);
    mojave_backend_draw_map(map);
    DrawRectangleRec(player_rect, (Color){42, 122, 184, 255});
    DrawRectangleLinesEx(player_rect, 2.0f, (Color){14, 37, 58, 255});
    DrawRectangleLines(0, 0, map_width_px, map_height_px, BLACK);
    EndMode2D();

    DrawRectangle(12, 12, 300, 102, Fade(RAYWHITE, 0.85f));
    DrawRectangleLines(12, 12, 300, 102, DARKGRAY);
    DrawText(map->name, 24, 24, 20, BLACK);
    DrawText("Move: WASD / Arrows", 24, 52, 18, BLACK);
    DrawText("Save: F5   Load: F9", 24, 74, 18, BLACK);
    DrawText(mojave_game_save_loaded(game) ? "Save file found" : "No save loaded yet", 24, 96, 18, DARKGRAY);

    EndDrawing();
}
