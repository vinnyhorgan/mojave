#include "runtime.h"

#include "collision.h"

#include <math.h>
#include <string.h>

static const float MOJAVE_PLAYER_SIZE = 18.0f;
static const float MOJAVE_PLAYER_SPEED = 180.0f;
static const char *MOJAVE_DIALOGUE_PATH = "data/intro_dialogue.json";

ECS_COMPONENT_DECLARE(Position) = 0;
ECS_COMPONENT_DECLARE(Velocity) = 0;

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
    MojaveRect player_rect;
    MojaveCollisionMoveResult move_result;

    if (position == NULL) {
        return;
    }

    player_rect.x = position->x;
    player_rect.y = position->y;
    player_rect.w = MOJAVE_PLAYER_SIZE;
    player_rect.h = MOJAVE_PLAYER_SIZE;

    if (mojave_collision_move_rect(
            map,
            &player_rect,
            position->x + velocity.x * dt,
            position->y + velocity.y * dt,
            &move_result)) {
        position->x = move_result.x;
        position->y = move_result.y;
    }
}

static void mojave_game_set_dialogue_node(MojaveGame *game, const char *node_id) {
    const MojaveDialogueNode *node;

    if (game == NULL || node_id == NULL) {
        return;
    }

    node = mojave_dialogue_find_node(&game->dialogue, node_id);
    game->active_dialogue_node = node;
    game->selected_dialogue_choice = 0;
}

static void mojave_game_start_dialogue(MojaveGame *game) {
    if (game == NULL || game->dialogue.start_id == NULL) {
        return;
    }

    mojave_game_set_dialogue_node(game, game->dialogue.start_id);
}

static void mojave_game_end_dialogue(MojaveGame *game) {
    if (game == NULL) {
        return;
    }

    game->active_dialogue_node = NULL;
    game->selected_dialogue_choice = 0;
}

static void mojave_game_update_dialogue(MojaveGame *game, const MojaveInput *input) {
    const MojaveDialogueNode *node;

    if (game == NULL || input == NULL) {
        return;
    }

    node = game->active_dialogue_node;
    if (node == NULL) {
        if (input->interact_pressed) {
            mojave_game_start_dialogue(game);
        }
        return;
    }

    if (node->choice_count > 0) {
        if (input->menu_up_pressed) {
            game->selected_dialogue_choice -= 1;
            if (game->selected_dialogue_choice < 0) {
                game->selected_dialogue_choice = node->choice_count - 1;
            }
        }
        if (input->menu_down_pressed) {
            game->selected_dialogue_choice += 1;
            if (game->selected_dialogue_choice >= node->choice_count) {
                game->selected_dialogue_choice = 0;
            }
        }
    }

    if (!input->interact_pressed) {
        return;
    }

    if (node->choice_count > 0) {
        mojave_game_set_dialogue_node(game, node->choices[game->selected_dialogue_choice].next_id);
    } else if (node->next_id != NULL) {
        mojave_game_set_dialogue_node(game, node->next_id);
    } else if (node->is_end) {
        mojave_game_end_dialogue(game);
    } else {
        mojave_game_end_dialogue(game);
    }

    if (game->active_dialogue_node == NULL) {
        mojave_game_end_dialogue(game);
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

    if (!mojave_dialogue_load(MOJAVE_DIALOGUE_PATH, &game->dialogue)) {
        mojave_map_unload(&game->map);
        return false;
    }

    game->world = ecs_init();
    if (game->world == NULL) {
        mojave_dialogue_unload(&game->dialogue);
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

    mojave_dialogue_unload(&game->dialogue);
    mojave_map_unload(&game->map);
}

void mojave_game_update(MojaveGame *game, const MojaveInput *input, float dt) {
    Position *position;
    Velocity velocity;
    bool dialogue_was_active;

    if (game == NULL || game->world == NULL) {
        return;
    }

    dialogue_was_active = game->active_dialogue_node != NULL;
    mojave_game_update_dialogue(game, input);
    if (dialogue_was_active || game->active_dialogue_node != NULL) {
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

bool mojave_game_dialogue_active(const MojaveGame *game) {
    return game != NULL && game->active_dialogue_node != NULL;
}

const MojaveDialogueNode *mojave_game_dialogue_node(const MojaveGame *game) {
    if (game == NULL) {
        return NULL;
    }

    return game->active_dialogue_node;
}

int mojave_game_dialogue_selected_choice(const MojaveGame *game) {
    if (game == NULL) {
        return 0;
    }

    return game->selected_dialogue_choice;
}
