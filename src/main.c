#include <stdio.h>

#include "backend.h"
#include "runtime.h"

int main(void) {
    MojaveBackendConfig backend_config = {
        "Mojave",
        1280,
        720,
    };
    MojaveGame game;

    if (!mojave_backend_init(&backend_config)) {
        fprintf(stderr, "Failed to initialize backend\n");
        return 1;
    }

    if (!mojave_game_init(&game, "data/first_map.json", "data/save.json")) {
        fprintf(stderr, "Failed to initialize Mojave\n");
        mojave_backend_shutdown();
        return 1;
    }

    while (!mojave_backend_should_close()) {
        MojaveInput input = mojave_backend_poll_input();

        mojave_game_update(&game, &input, mojave_backend_frame_time());
        mojave_backend_draw(&game);
    }

    mojave_game_shutdown(&game);
    mojave_backend_shutdown();
    return 0;
}
