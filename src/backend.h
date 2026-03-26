#ifndef MOJAVE_BACKEND_H
#define MOJAVE_BACKEND_H

#include <stdbool.h>

#include "runtime.h"

typedef struct MojaveBackendConfig {
    const char *window_title;
    int window_width;
    int window_height;
} MojaveBackendConfig;

bool mojave_backend_init(const MojaveBackendConfig *config);
void mojave_backend_shutdown(void);
bool mojave_backend_should_close(void);
float mojave_backend_frame_time(void);
MojaveInput mojave_backend_poll_input(void);
void mojave_backend_draw(const MojaveGame *game);

#endif
