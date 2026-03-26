#include "backend.h"

#include <raylib.h>

static Camera2D g_camera;

static const float MOJAVE_NPC_DRAW_SIZE = 18.0f;

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

static void mojave_backend_draw_npcs(const MojaveGame *game) {
    int i;
    int nearby_npc_index = mojave_game_nearby_npc_index(game);
    const MojaveMap *map = mojave_game_map(game);

    for (i = 0; i < mojave_game_npc_count(game); i += 1) {
        const MojaveNpc *npc = mojave_game_npc(game, i);
        Rectangle rect;
        Color fill;

        if (npc == NULL) {
            continue;
        }

        rect.x = (float)(npc->spawn_x * map->tile_size + 7);
        rect.y = (float)(npc->spawn_y * map->tile_size + 7);
        rect.width = MOJAVE_NPC_DRAW_SIZE;
        rect.height = MOJAVE_NPC_DRAW_SIZE;
        fill = i == nearby_npc_index ? (Color){205, 160, 76, 255} : (Color){161, 92, 70, 255};

        DrawRectangleRec(rect, fill);
        DrawRectangleLinesEx(rect, 2.0f, (Color){56, 28, 21, 255});
        DrawText(npc->name, (int)rect.x - 6, (int)rect.y - 18, 10, BLACK);
    }
}

static void mojave_backend_draw_dialogue(const MojaveGame *game) {
    const MojaveDialogueNode *node = mojave_game_dialogue_node(game);
    int i;
    int selected_choice;

    if (node == NULL) {
        return;
    }

    selected_choice = mojave_game_dialogue_selected_choice(game);
    DrawRectangle(40, GetScreenHeight() - 220, GetScreenWidth() - 80, 180, Fade(BLACK, 0.88f));
    DrawRectangleLines(40, GetScreenHeight() - 220, GetScreenWidth() - 80, 180, RAYWHITE);
    DrawText(node->speaker, 60, GetScreenHeight() - 200, 20, GOLD);
    DrawText(node->text, 60, GetScreenHeight() - 168, 20, RAYWHITE);

    for (i = 0; i < node->choice_count; i += 1) {
        Color color = i == selected_choice ? GOLD : LIGHTGRAY;
        DrawText(node->choices[i].text, 80, GetScreenHeight() - 132 + i * 22, 20, color);
    }

    if (node->choice_count == 0) {
        DrawText("Enter / Space", GetScreenWidth() - 220, GetScreenHeight() - 74, 20, LIGHTGRAY);
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
    MojaveInput input = {0.0f, 0.0f, false, false, false, false, false};

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
    input.interact_pressed = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_E);
    input.menu_up_pressed = IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP);
    input.menu_down_pressed = IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN);
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
    mojave_backend_draw_npcs(game);
    DrawRectangleRec(player_rect, (Color){42, 122, 184, 255});
    DrawRectangleLinesEx(player_rect, 2.0f, (Color){14, 37, 58, 255});
    DrawRectangleLines(0, 0, map_width_px, map_height_px, BLACK);
    EndMode2D();

    DrawRectangle(12, 12, 300, 112, Fade(RAYWHITE, 0.85f));
    DrawRectangleLines(12, 12, 300, 112, DARKGRAY);
    DrawText(map->name, 24, 24, 20, BLACK);
    DrawText("Move: WASD / Arrows", 24, 52, 20, BLACK);
    DrawText("Talk: Enter / Space / E", 24, 74, 20, BLACK);
    DrawText(mojave_game_save_loaded(game) ? "Save file found" : "No save loaded yet", 24, 96, 20, DARKGRAY);

    mojave_backend_draw_dialogue(game);

    EndDrawing();
}
