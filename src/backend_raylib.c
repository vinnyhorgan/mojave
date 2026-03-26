#include "backend.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include <raylib.h>

static Camera2D g_camera;
static const float MOJAVE_CAMERA_ZOOM = 2.0f;
static const int MOJAVE_DIALOG_BOX_OFFSET_Y = 220;
static const int MOJAVE_DIALOG_BOX_HEIGHT = 180;
static bool g_show_quest_log;
static bool g_show_inventory;

static Color mojave_floor_color(void) {
    return (Color){60, 92, 65, 255};
}

static Color mojave_wall_color(void) {
    return (Color){87, 73, 54, 255};
}

static float mojave_backend_snap(float value) {
    return roundf(value);
}

static Rectangle mojave_backend_snap_rect(Rectangle rect) {
    rect.x = mojave_backend_snap(rect.x);
    rect.y = mojave_backend_snap(rect.y);
    rect.width = mojave_backend_snap(rect.width);
    rect.height = mojave_backend_snap(rect.height);
    return rect;
}

static void mojave_backend_draw_rect_outline(Rectangle rect, int thickness, Color color) {
    rect = mojave_backend_snap_rect(rect);
    DrawRectangle((int)rect.x, (int)rect.y, (int)rect.width, thickness, color);
    DrawRectangle((int)rect.x, (int)(rect.y + rect.height - thickness), (int)rect.width, thickness, color);
    DrawRectangle((int)rect.x, (int)rect.y, thickness, (int)rect.height, color);
    DrawRectangle((int)(rect.x + rect.width - thickness), (int)rect.y, thickness, (int)rect.height, color);
}

static void mojave_backend_draw_text_fitted(const char *text, int x, int y, int font_size, int max_width, Color color) {
    char buffer[512];
    size_t length;

    if (text == NULL || max_width <= 0) {
        return;
    }

    if (MeasureText(text, font_size) <= max_width) {
        DrawText(text, x, y, font_size, color);
        return;
    }

    length = strlen(text);
    if (length >= sizeof(buffer)) {
        length = sizeof(buffer) - 1;
    }
    memcpy(buffer, text, length);
    buffer[length] = '\0';

    while (length > 0) {
        length -= 1;
        buffer[length] = '\0';
        if (MeasureText(TextFormat("%s...", buffer), font_size) <= max_width) {
            DrawText(TextFormat("%s...", buffer), x, y, font_size, color);
            return;
        }
    }

    DrawText("...", x, y, font_size, color);
}

static void mojave_backend_update_camera(const MojaveGame *game) {
    MojaveVec2 player_position = mojave_game_player_position(game);
    float player_size = mojave_game_player_size();

    g_camera.offset = (Vector2){
        mojave_backend_snap((float)GetScreenWidth() * 0.5f),
        mojave_backend_snap((float)GetScreenHeight() * 0.5f)
    };
    g_camera.target = (Vector2){
        mojave_backend_snap(player_position.x + player_size * 0.5f),
        mojave_backend_snap(player_position.y + player_size * 0.5f)
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
            rect = mojave_backend_snap_rect(rect);

            DrawRectangleRec(rect, tile == 0 ? mojave_floor_color() : mojave_wall_color());
            mojave_backend_draw_rect_outline(rect, 1, (Color){0, 0, 0, 35});
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
        Color fill_color;
        Color outline_color;

        if (npc == NULL) {
            continue;
        }

        rect.x = (float)(npc->spawn_x * map->tile_size + 7);
        rect.y = (float)(npc->spawn_y * map->tile_size + 7);
        rect.width = MOJAVE_NPC_SIZE;
        rect.height = MOJAVE_NPC_SIZE;
        rect = mojave_backend_snap_rect(rect);
        fill_color = (Color){npc->outfit_r, npc->outfit_g, npc->outfit_b, 255};
        outline_color = i == nearby_npc_index ? (Color){236, 204, 110, 255} : (Color){45, 31, 26, 255};

        DrawRectangleRec(rect, fill_color);
        mojave_backend_draw_rect_outline(rect, 2, outline_color);
        DrawText(npc->name, (int)rect.x - 6, (int)rect.y - 18, 10, outline_color);
    }
}

static void mojave_backend_draw_items(const MojaveGame *game) {
    const MojaveMap *map = mojave_game_map(game);
    int nearby_item_index = mojave_game_nearby_item_index(game);
    int i;

    for (i = 0; i < mojave_game_map_item_count(game); i += 1) {
        const MojaveMapItem *item = mojave_game_map_item(game, i);
        const MojaveItemDefinition *definition;
        Rectangle rect;
        Color fill_color;
        Color outline_color;

        if (item == NULL || mojave_game_map_item_collected(game, i)) {
            continue;
        }

        definition = mojave_item_database_find(&game->item_database, item->item_id);
        if (definition == NULL) {
            continue;
        }

        rect.x = (float)(item->spawn_x * map->tile_size + 10);
        rect.y = (float)(item->spawn_y * map->tile_size + 10);
        rect.width = MOJAVE_ITEM_SIZE;
        rect.height = MOJAVE_ITEM_SIZE;
        rect = mojave_backend_snap_rect(rect);
        fill_color = (Color){definition->color_r, definition->color_g, definition->color_b, 255};
        outline_color = i == nearby_item_index ? GOLD : (Color){53, 42, 31, 255};

        DrawRectangleRec(rect, fill_color);
        mojave_backend_draw_rect_outline(rect, 2, outline_color);
    }
}

static void mojave_backend_draw_dialogue(const MojaveGame *game) {
    const MojaveDialogueNode *node = mojave_game_dialogue_node(game);
    const MojaveDialogueChoice *choice;
    int box_x = 40;
    int box_y = GetScreenHeight() - MOJAVE_DIALOG_BOX_OFFSET_Y;
    int box_width = GetScreenWidth() - 80;
    int max_visible_choices = 4;
    int first_visible_choice = 0;
    int i;
    int selected_choice;
    int visible_choice_count;

    if (node == NULL) {
        return;
    }

    selected_choice = mojave_game_dialogue_selected_choice(game);
    visible_choice_count = mojave_game_dialogue_visible_choice_count(game);
    if (visible_choice_count > max_visible_choices) {
        first_visible_choice = selected_choice - max_visible_choices / 2;
        if (first_visible_choice < 0) {
            first_visible_choice = 0;
        }
        if (first_visible_choice > visible_choice_count - max_visible_choices) {
            first_visible_choice = visible_choice_count - max_visible_choices;
        }
    }
    DrawRectangle(box_x, box_y, box_width, MOJAVE_DIALOG_BOX_HEIGHT, Fade(BLACK, 0.88f));
    DrawRectangleLines(box_x, box_y, box_width, MOJAVE_DIALOG_BOX_HEIGHT, RAYWHITE);
    mojave_backend_draw_text_fitted(node->speaker, 60, GetScreenHeight() - 200, 20, box_width - 120, GOLD);
    mojave_backend_draw_text_fitted(node->text, 60, GetScreenHeight() - 168, 20, box_width - 120, RAYWHITE);

    for (i = 0; i < max_visible_choices && first_visible_choice + i < visible_choice_count; i += 1) {
        int choice_index = first_visible_choice + i;
        Color color = choice_index == selected_choice ? GOLD : LIGHTGRAY;

        choice = mojave_game_dialogue_visible_choice(game, choice_index);
        if (choice != NULL) {
            mojave_backend_draw_text_fitted(choice->text, 80, GetScreenHeight() - 132 + i * 22, 20, box_width - 160, color);
        }
    }

    if (visible_choice_count > max_visible_choices) {
        DrawText("More", box_x + box_width - 72, box_y + 124, 20, LIGHTGRAY);
    }

    if (visible_choice_count == 0) {
        DrawText("Enter / Space", GetScreenWidth() - 220, GetScreenHeight() - 74, 20, LIGHTGRAY);
    }
}

static void mojave_backend_draw_interaction_prompt(const MojaveGame *game) {
    const MojaveMapItem *item;
    const MojaveItemDefinition *definition;
    const char *hint = "E / Enter / Space";
    char prompt[256];
    int panel_x = 12;
    int panel_y = GetScreenHeight() - 88;
    int panel_width = 460;
    int panel_height = 64;
    int hint_width;
    int prompt_width;
    int nearby_item_index;

    if (g_show_quest_log || g_show_inventory || mojave_game_dialogue_active(game)) {
        return;
    }

    nearby_item_index = mojave_game_nearby_item_index(game);
    if (nearby_item_index < 0) {
        return;
    }

    item = mojave_game_map_item(game, nearby_item_index);
    if (item == NULL) {
        return;
    }

    definition = mojave_item_database_find(&game->item_database, item->item_id);
    if (definition == NULL) {
        return;
    }

    snprintf(prompt, sizeof(prompt), "Pick up: %s", definition->name);
    hint_width = MeasureText(hint, 20);
    prompt_width = panel_width - 24 - 24;

    DrawRectangle(panel_x, panel_y, panel_width, panel_height, Fade(RAYWHITE, 0.9f));
    DrawRectangleLines(panel_x, panel_y, panel_width, panel_height, DARKGRAY);
    mojave_backend_draw_text_fitted(prompt, panel_x + 12, panel_y + 10, 20, prompt_width, BLACK);
    DrawText(hint, panel_x + panel_width - 12 - hint_width, panel_y + 34, 20, DARKGRAY);
}

static void mojave_backend_draw_quest_log(const MojaveGame *game) {
    int active_count;
    int completed_count;
    int y;
    int i;

    if (!g_show_quest_log) {
        return;
    }

    active_count = mojave_game_active_quest_count(game);
    completed_count = mojave_game_completed_quest_count(game);
    y = 72;

    DrawRectangle(120, 56, GetScreenWidth() - 240, GetScreenHeight() - 112, Fade((Color){28, 24, 18, 255}, 0.94f));
    DrawRectangleLines(120, 56, GetScreenWidth() - 240, GetScreenHeight() - 112, (Color){204, 188, 154, 255});
    DrawText("Quest Log", 148, 78, 20, (Color){232, 214, 170, 255});
    DrawText("Close: J / Tab", GetScreenWidth() - 300, 78, 20, LIGHTGRAY);

    y += 42;
    DrawText("Active", 148, y, 20, GOLD);
    y += 28;
    if (active_count == 0) {
        DrawText("No active quests", 164, y, 20, LIGHTGRAY);
        y += 28;
    }
    for (i = 0; i < active_count; i += 1) {
        const MojaveQuestState *quest = mojave_game_active_quest(game, i);

        if (quest == NULL || quest->definition == NULL) {
            continue;
        }
        DrawText(quest->definition->title, 164, y, 20, RAYWHITE);
        y += 24;
        if (quest->stage >= 0 && quest->stage < quest->definition->stage_count) {
            DrawText(quest->definition->stages[quest->stage], 184, y, 20, (Color){188, 202, 177, 255});
            y += 28;
        }
        y += 6;
    }

    y += 12;
    DrawText("Completed", 148, y, 20, (Color){155, 197, 149, 255});
    y += 28;
    if (completed_count == 0) {
        DrawText("No completed quests", 164, y, 20, LIGHTGRAY);
        return;
    }
    for (i = 0; i < completed_count; i += 1) {
        const MojaveQuestState *quest = mojave_game_completed_quest(game, i);

        if (quest == NULL || quest->definition == NULL) {
            continue;
        }
        DrawText(quest->definition->title, 164, y, 20, (Color){185, 220, 178, 255});
        y += 24;
    }
}

static void mojave_backend_draw_inventory(const MojaveGame *game) {
    int y;
    int i;

    if (!g_show_inventory) {
        return;
    }

    DrawRectangle(120, 56, GetScreenWidth() - 240, GetScreenHeight() - 112, Fade((Color){24, 23, 20, 255}, 0.94f));
    DrawRectangleLines(120, 56, GetScreenWidth() - 240, GetScreenHeight() - 112, (Color){190, 198, 181, 255});
    DrawText("Inventory", 148, 78, 20, (Color){222, 232, 205, 255});
    DrawText("Close: I", GetScreenWidth() - 220, 78, 20, LIGHTGRAY);

    y = 120;
    if (mojave_game_inventory_count(game) == 0) {
        DrawText("Inventory is empty", 164, y, 20, LIGHTGRAY);
        return;
    }

    for (i = 0; i < mojave_game_inventory_count(game); i += 1) {
        const MojaveInventoryEntry *entry = mojave_game_inventory_entry(game, i);
        Rectangle swatch;

        if (entry == NULL || entry->definition == NULL) {
            continue;
        }

        swatch = (Rectangle){148.0f, (float)y + 4.0f, 12.0f, 12.0f};
        DrawRectangleRec(swatch, (Color){entry->definition->color_r, entry->definition->color_g, entry->definition->color_b, 255});
        mojave_backend_draw_rect_outline(swatch, 2, (Color){48, 44, 37, 255});
        DrawText(entry->definition->name, 176, y, 20, RAYWHITE);
        DrawText(entry->definition->description, 176, y + 22, 20, LIGHTGRAY);
        DrawText(TextFormat("x%d", entry->count), GetScreenWidth() - 220, y, 20, (Color){211, 220, 154, 255});
        y += 58;
    }
}

bool mojave_backend_init(const MojaveBackendConfig *config) {
    if (config == NULL) {
        return false;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(config->window_width, config->window_height, config->window_title);
    SetTargetFPS(60);

    g_camera.zoom = MOJAVE_CAMERA_ZOOM;
    g_camera.rotation = 0.0f;
    g_camera.offset = (Vector2){
        mojave_backend_snap((float)config->window_width * 0.5f),
        mojave_backend_snap((float)config->window_height * 0.5f)
    };
    g_camera.target = (Vector2){0.0f, 0.0f};
    g_show_quest_log = false;
    g_show_inventory = false;
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
    MojaveInput input = {0.0f, 0.0f, false, false, false, false, false, false, false};

    input.quest_log_pressed = IsKeyPressed(KEY_J) || IsKeyPressed(KEY_TAB);
    input.inventory_pressed = IsKeyPressed(KEY_I);
    if (input.quest_log_pressed) {
        g_show_quest_log = !g_show_quest_log;
        if (g_show_quest_log) {
            g_show_inventory = false;
        }
    }
    if (input.inventory_pressed) {
        g_show_inventory = !g_show_inventory;
        if (g_show_inventory) {
            g_show_quest_log = false;
        }
    }

    if (g_show_quest_log || g_show_inventory) {
        return input;
    }

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
    const MojaveQuestState *active_quest;
    MojaveVec2 player_position;
    float player_size;
    int quest_title_width;
    int map_width_px;
    int map_height_px;
    Rectangle player_rect;

    if (game == NULL) {
        return;
    }

    map = mojave_game_map(game);
    active_quest = mojave_game_active_quest(game, 0);
    player_position = mojave_game_player_position(game);
    player_size = mojave_game_player_size();

    mojave_backend_update_camera(game);

    map_width_px = map->width * map->tile_size;
    map_height_px = map->height * map->tile_size;
    player_rect = (Rectangle){player_position.x, player_position.y, player_size, player_size};
    player_rect = mojave_backend_snap_rect(player_rect);

    BeginDrawing();
    ClearBackground((Color){205, 197, 176, 255});

    BeginMode2D(g_camera);
    mojave_backend_draw_map(map);
    mojave_backend_draw_items(game);
    mojave_backend_draw_npcs(game);
    DrawRectangleRec(player_rect, (Color){42, 122, 184, 255});
    mojave_backend_draw_rect_outline(player_rect, 2, (Color){14, 37, 58, 255});
    mojave_backend_draw_rect_outline((Rectangle){0.0f, 0.0f, (float)map_width_px, (float)map_height_px}, 1, BLACK);
    EndMode2D();

    DrawRectangle(12, 12, 420, 148, Fade(RAYWHITE, 0.85f));
    DrawRectangleLines(12, 12, 420, 148, DARKGRAY);
    DrawText(map->name, 24, 24, 20, BLACK);
    DrawText("Move: WASD / Arrows", 24, 52, 20, BLACK);
    DrawText("Talk: Enter / Space / E", 24, 74, 20, BLACK);
    DrawText("Quests: J / Tab", 24, 96, 20, BLACK);
    DrawText("Inventory: I", 24, 118, 20, BLACK);
    DrawText(mojave_game_save_loaded(game) ? "Save file found" : "No save loaded yet", 240, 118, 20, DARKGRAY);
    if (!g_show_quest_log && !g_show_inventory && active_quest != NULL && active_quest->definition != NULL && active_quest->stage >= 0 &&
        active_quest->stage < active_quest->definition->stage_count) {
        quest_title_width = MeasureText(active_quest->definition->title, 20);
        DrawRectangle(12, 168, 520, 56, Fade(RAYWHITE, 0.85f));
        DrawRectangleLines(12, 168, 520, 56, DARKGRAY);
        DrawText(active_quest->definition->title, 24, 182, 20, MAROON);
        DrawText(active_quest->definition->stages[active_quest->stage], 36 + quest_title_width, 182, 20, DARKBROWN);
    }

    mojave_backend_draw_dialogue(game);
    mojave_backend_draw_quest_log(game);
    mojave_backend_draw_inventory(game);
    mojave_backend_draw_interaction_prompt(game);

    EndDrawing();
}
