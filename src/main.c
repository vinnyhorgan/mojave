#include <stdio.h>

#include <raylib.h>

#include "runtime.h"

int main(void) {
    MojaveGame game;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "Mojave");
    SetTargetFPS(60);

    if (!mojave_game_init(&game, "data/first_map.json", "data/save.json")) {
        fprintf(stderr, "Failed to initialize Mojave\n");
        CloseWindow();
        return 1;
    }

    while (!WindowShouldClose()) {
        mojave_game_update(&game, GetFrameTime());
        mojave_game_draw(&game);
    }

    mojave_game_shutdown(&game);
    CloseWindow();
    return 0;
}
