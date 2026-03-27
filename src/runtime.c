#include "runtime.h"

#include "collision.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yyjson.h>

const float MOJAVE_PLAYER_SIZE = 18.0f;
const float MOJAVE_PLAYER_SPEED = 180.0f;
const float MOJAVE_NPC_SIZE = 18.0f;
const float MOJAVE_ITEM_SIZE = 12.0f;
static const float MOJAVE_NPC_INTERACT_RANGE = 42.0f;
static const float MOJAVE_ITEM_PICKUP_RANGE = 30.0f;
static const char *MOJAVE_ITEM_DATABASE_PATH = "data/items.json";
static const char *MOJAVE_QUEST_LOG_PATH = "data/quests.json";

ECS_COMPONENT_DECLARE(Position) = 0;
ECS_COMPONENT_DECLARE(Velocity) = 0;
ECS_COMPONENT_DECLARE(CollisionBox) = 0;
ECS_COMPONENT_DECLARE(Renderable) = 0;
ECS_COMPONENT_DECLARE(Team) = 0;
ECS_COMPONENT_DECLARE(Hp) = 0;
ECS_COMPONENT_DECLARE(ItemRef) = 0;
ECS_COMPONENT_DECLARE(DialoguRef) = 0;
ECS_COMPONENT_DECLARE(ActiveDialogue) = 0;
ECS_COMPONENT_DECLARE(Npc) = 0;
ECS_COMPONENT_DECLARE(Item) = 0;

static const MojaveMap *g_map = NULL;

static bool mojave_game_add_inventory_item(MojaveGame *game, const MojaveItemDefinition *definition);
static bool mojave_game_remove_inventory_item(MojaveGame *game, const char *item_id, int count);
static bool mojave_game_collect_item_at_index(MojaveGame *game, int item_index);
static bool mojave_game_validate_dialogue(const MojaveGame *game, const char *path, const MojaveDialogue *dialogue);
static bool mojave_game_validate_content(const MojaveGame *game);

static void MovementSystem(ecs_iter_t *it) {
    Position *p = ecs_field(it, Position, 1);
    Velocity *v = ecs_field(it, Velocity, 2);

    for (int i = 0; i < it->count; i += 1) {
        p[i].x += v[i].x * it->delta_time;
        p[i].y += v[i].y * it->delta_time;
    }
}

static void TilemapCollisionSystem(ecs_iter_t *it) {
    Position *p = ecs_field(it, Position, 1);
    Velocity *v = ecs_field(it, Velocity, 2);
    CollisionBox *cb = ecs_field(it, CollisionBox, 3);

    if (g_map == NULL) {
        return;
    }

    for (int i = 0; i < it->count; i += 1) {
        MojaveRect rect = {p[i].x, p[i].y, cb[i].w, cb[i].h};
        MojaveCollisionMoveResult result;

        if (mojave_collision_move_rect(g_map, &rect, p[i].x + v[i].x * it->delta_time, p[i].y + v[i].y * it->delta_time, &result)) {
            p[i].x = result.x;
            p[i].y = result.y;
            if (result.collided) {
                v[i].x = 0.0f;
                v[i].y = 0.0f;
            }
        }
    }
}

static void CombatSystem(ecs_iter_t *it) {
    Hp *hp = ecs_field(it, Hp, 1);

    for (int i = 0; i < it->count; i += 1) {
        if (hp[i].current <= 0.0f) {
            ecs_delete(it->world, it->entities[i]);
        }
    }
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

        velocity.x = (velocity.x / length) * MOJAVE_PLAYER_SPEED;
        velocity.y = (velocity.y / length) * MOJAVE_PLAYER_SPEED;
    }

    return velocity;
}

static MojaveFlagState *mojave_game_get_flag(MojaveGame *game, const char *flag_id) {
    int i;

    if (game == NULL || flag_id == NULL) {
        return NULL;
    }

    for (i = 0; i < game->flag_count; i += 1) {
        if (strcmp(game->flags[i].id, flag_id) == 0) {
            return &game->flags[i];
        }
    }

    return NULL;
}

static bool mojave_game_set_flag(MojaveGame *game, const char *flag_id, bool value) {
    MojaveFlagState *flag;
    MojaveFlagState *flags;
    MojaveFlagState *old_flags;

    if (game == NULL || flag_id == NULL) {
        return false;
    }

    flag = mojave_game_get_flag(game, flag_id);
    if (flag != NULL) {
        flag->value = value;
        return true;
    }

    old_flags = game->flags;
    flags = realloc(game->flags, (size_t)(game->flag_count + 1) * sizeof(*game->flags));
    if (flags == NULL) {
        return false;
    }
    game->flags = flags;

    game->flags[game->flag_count].id = malloc(strlen(flag_id) + 1);
    if (game->flags[game->flag_count].id == NULL) {
        game->flags = old_flags;
        free(flags);
        return false;
    }
    memcpy(game->flags[game->flag_count].id, flag_id, strlen(flag_id) + 1);
    game->flags[game->flag_count].value = value;
    game->flag_count += 1;
    return true;
}

static MojaveQuestState *mojave_game_find_quest_state(MojaveGame *game, const char *quest_id) {
    int i;

    if (game == NULL || quest_id == NULL) {
        return NULL;
    }

    for (i = 0; i < game->quest_state_count; i += 1) {
        if (game->quest_states[i].definition != NULL && strcmp(game->quest_states[i].definition->id, quest_id) == 0) {
            return &game->quest_states[i];
        }
    }

    return NULL;
}

static void mojave_game_start_quest(MojaveGame *game, const char *quest_id) {
    MojaveQuestState *quest_state = mojave_game_find_quest_state(game, quest_id);

    if (quest_state == NULL || quest_state->completed) {
        return;
    }

    quest_state->active = true;
    if (quest_state->stage < 0) {
        quest_state->stage = 0;
    }
}

static void mojave_game_set_quest_stage(MojaveGame *game, const char *quest_id, int stage) {
    MojaveQuestState *quest_state = mojave_game_find_quest_state(game, quest_id);

    if (quest_state == NULL || quest_state->definition == NULL || quest_state->completed) {
        return;
    }

    if (stage < 0) {
        fprintf(stderr, "Quest '%s' stage %d is negative, clamping to 0\n", quest_id, stage);
        stage = 0;
    }
    if (stage >= quest_state->definition->stage_count) {
        fprintf(stderr, "Quest '%s' stage %d exceeds max %d, clamping to %d\n",
            quest_id, stage, quest_state->definition->stage_count - 1, quest_state->definition->stage_count - 1);
        stage = quest_state->definition->stage_count - 1;
    }

    quest_state->active = true;
    quest_state->stage = stage;
}

static void mojave_game_complete_quest(MojaveGame *game, const char *quest_id) {
    MojaveQuestState *quest_state = mojave_game_find_quest_state(game, quest_id);

    if (quest_state == NULL || quest_state->definition == NULL) {
        return;
    }

    quest_state->completed = true;
    quest_state->active = false;
    if (quest_state->definition->stage_count > 0) {
        quest_state->stage = quest_state->definition->stage_count - 1;
    }
}

static void mojave_game_run_actions(MojaveGame *game, const MojaveEventAction *actions, int action_count) {
    int i;

    if (game == NULL || actions == NULL) {
        return;
    }

    for (i = 0; i < action_count; i += 1) {
        switch (actions[i].type) {
            case MOJAVE_EVENT_ACTION_SET_FLAG:
                mojave_game_set_flag(game, actions[i].flag_id, actions[i].flag_value);
                break;
            case MOJAVE_EVENT_ACTION_START_QUEST:
                mojave_game_start_quest(game, actions[i].quest_id);
                break;
            case MOJAVE_EVENT_ACTION_SET_QUEST_STAGE:
                mojave_game_set_quest_stage(game, actions[i].quest_id, actions[i].quest_stage);
                break;
            case MOJAVE_EVENT_ACTION_COMPLETE_QUEST:
                mojave_game_complete_quest(game, actions[i].quest_id);
                break;
            case MOJAVE_EVENT_ACTION_GIVE_ITEM: {
                const MojaveItemDefinition *definition = mojave_item_database_find(&game->item_database, actions[i].item_id);
                int give_index;

                for (give_index = 0; definition != NULL && give_index < actions[i].item_count; give_index += 1) {
                    mojave_game_add_inventory_item(game, definition);
                }
                break;
            }
            case MOJAVE_EVENT_ACTION_REMOVE_ITEM:
                mojave_game_remove_inventory_item(game, actions[i].item_id, actions[i].item_count);
                break;
            case MOJAVE_EVENT_ACTION_NONE:
            default:
                break;
        }
    }
}

static int mojave_game_find_nearby_npc(const MojaveGame *game) {
    MojaveVec2 player_position;
    float player_center_x;
    float player_center_y;
    float best_distance_sq = 0.0f;
    int best_index = -1;
    int i;

    if (game == NULL || game->npc_entities == NULL) {
        return -1;
    }

    player_position = mojave_game_player_position(game);
    player_center_x = player_position.x + MOJAVE_PLAYER_SIZE * 0.5f;
    player_center_y = player_position.y + MOJAVE_PLAYER_SIZE * 0.5f;

    for (i = 0; i < game->map.npc_count; i += 1) {
        const Position *pos;
        float npc_center_x;
        float npc_center_y;
        float dx;
        float dy;
        float distance_sq;

        pos = ecs_get(game->world, game->npc_entities[i], Position);
        if (pos == NULL) {
            continue;
        }

        npc_center_x = pos->x + MOJAVE_NPC_SIZE * 0.5f;
        npc_center_y = pos->y + MOJAVE_NPC_SIZE * 0.5f;
        dx = npc_center_x - player_center_x;
        dy = npc_center_y - player_center_y;
        distance_sq = dx * dx + dy * dy;

        if (distance_sq <= MOJAVE_NPC_INTERACT_RANGE * MOJAVE_NPC_INTERACT_RANGE &&
            (best_index < 0 || distance_sq < best_distance_sq)) {
            best_distance_sq = distance_sq;
            best_index = i;
        }
    }

    return best_index;
}

static int mojave_game_find_nearby_item(const MojaveGame *game) {
    MojaveVec2 player_position;
    float player_center_x;
    float player_center_y;
    float best_distance_sq = 0.0f;
    int best_index = -1;
    int i;

    if (game == NULL || game->item_entities == NULL) {
        return -1;
    }

    player_position = mojave_game_player_position(game);
    player_center_x = player_position.x + MOJAVE_PLAYER_SIZE * 0.5f;
    player_center_y = player_position.y + MOJAVE_PLAYER_SIZE * 0.5f;

    for (i = 0; i < game->map.item_count; i += 1) {
        const Position *pos;
        float item_center_x;
        float item_center_y;
        float dx;
        float dy;
        float distance_sq;

        if (game->item_entities[i] == 0) {
            continue;
        }

        pos = ecs_get(game->world, game->item_entities[i], Position);
        if (pos == NULL) {
            continue;
        }

        item_center_x = pos->x + MOJAVE_ITEM_SIZE * 0.5f;
        item_center_y = pos->y + MOJAVE_ITEM_SIZE * 0.5f;
        dx = item_center_x - player_center_x;
        dy = item_center_y - player_center_y;
        distance_sq = dx * dx + dy * dy;

        if (distance_sq <= MOJAVE_ITEM_PICKUP_RANGE * MOJAVE_ITEM_PICKUP_RANGE &&
            (best_index < 0 || distance_sq < best_distance_sq)) {
            best_distance_sq = distance_sq;
            best_index = i;
        }
    }

    return best_index;
}

static bool mojave_game_add_inventory_item(MojaveGame *game, const MojaveItemDefinition *definition) {
    MojaveInventoryEntry *inventory;
    int i;

    if (game == NULL || definition == NULL) {
        return false;
    }

    for (i = 0; i < game->inventory_count; i += 1) {
        if (definition->stackable && game->inventory[i].definition == definition) {
            game->inventory[i].count += 1;
            return true;
        }
    }

    inventory = realloc(game->inventory, (size_t)(game->inventory_count + 1) * sizeof(*game->inventory));
    if (inventory == NULL) {
        return false;
    }
    game->inventory = inventory;
    game->inventory[game->inventory_count].definition = definition;
    game->inventory[game->inventory_count].count = 1;
    game->inventory_count += 1;
    return true;
}

static int mojave_game_inventory_item_count(const MojaveGame *game, const char *item_id) {
    int i;

    if (game == NULL || item_id == NULL) {
        return 0;
    }

    for (i = 0; i < game->inventory_count; i += 1) {
        if (game->inventory[i].definition != NULL && strcmp(game->inventory[i].definition->id, item_id) == 0) {
            return game->inventory[i].count;
        }
    }

    return 0;
}

static bool mojave_game_remove_inventory_item(MojaveGame *game, const char *item_id, int count) {
    int i;

    if (game == NULL || item_id == NULL || count <= 0) {
        return false;
    }

    for (i = 0; i < game->inventory_count; i += 1) {
        if (game->inventory[i].definition != NULL && strcmp(game->inventory[i].definition->id, item_id) == 0) {
            if (game->inventory[i].count < count) {
                return false;
            }
            game->inventory[i].count -= count;
            if (game->inventory[i].count == 0) {
                int tail_count = game->inventory_count - i - 1;

                if (tail_count > 0) {
                    memmove(&game->inventory[i], &game->inventory[i + 1], (size_t)tail_count * sizeof(*game->inventory));
                }
                game->inventory_count -= 1;
            }
            return true;
        }
    }

    return false;
}

static bool mojave_game_collect_item_at_index(MojaveGame *game, int item_index) {
    const MojaveItemDefinition *definition;
    ecs_entity_t entity;

    if (game == NULL || item_index < 0 || item_index >= game->map.item_count) {
        return false;
    }
    if (game->item_entities == NULL) {
        return false;
    }
    if (game->map_item_collected != NULL && game->map_item_collected[item_index]) {
        return false;
    }

    entity = game->item_entities[item_index];
    if (entity == 0) {
        return false;
    }

    definition = mojave_item_database_find(&game->item_database, game->map.items[item_index].item_id);
    if (definition == NULL || !mojave_game_add_inventory_item(game, definition)) {
        return false;
    }

    ecs_delete(game->world, entity);
    game->item_entities[item_index] = 0;
    if (game->map_item_collected != NULL) {
        game->map_item_collected[item_index] = true;
    }
    if (game->nearby_item_index == item_index) {
        game->nearby_item_index = -1;
    }
    return true;
}

static bool mojave_game_conditions_met(const MojaveGame *game, const MojaveCondition *conditions, int condition_count) {
    int i;

    if (conditions == NULL) {
        return true;
    }

    for (i = 0; i < condition_count; i += 1) {
        switch (conditions[i].type) {
            case MOJAVE_CONDITION_HAS_ITEM:
                if (mojave_game_inventory_item_count(game, conditions[i].item_id) < conditions[i].item_count) {
                    return false;
                }
                break;
            case MOJAVE_CONDITION_NONE:
            default:
                break;
        }
    }

    return true;
}

static int mojave_game_dialogue_choice_index_from_visible(const MojaveGame *game, int visible_index) {
    const MojaveDialogueNode *node;
    int current_visible_index = 0;
    int i;

    if (game == NULL || visible_index < 0) {
        return -1;
    }

    node = game->active_dialogue_node;
    if (node == NULL) {
        return -1;
    }

    for (i = 0; i < node->choice_count; i += 1) {
        if (!mojave_game_conditions_met(game, node->choices[i].conditions, node->choices[i].condition_count)) {
            continue;
        }
        if (current_visible_index == visible_index) {
            return i;
        }
        current_visible_index += 1;
    }

    return -1;
}

static bool mojave_game_validate_dialogue_actions(const MojaveGame *game,
    const char *path,
    const MojaveEventAction *actions,
    int action_count) {
    int i;

    for (i = 0; i < action_count; i += 1) {
        switch (actions[i].type) {
            case MOJAVE_EVENT_ACTION_SET_FLAG:
                if (actions[i].flag_id == NULL) {
                    fprintf(stderr, "Dialogue '%s' has a set_flag action without a flag id\n", path);
                    return false;
                }
                break;
            case MOJAVE_EVENT_ACTION_START_QUEST:
            case MOJAVE_EVENT_ACTION_SET_QUEST_STAGE:
            case MOJAVE_EVENT_ACTION_COMPLETE_QUEST:
                if (actions[i].quest_id == NULL || mojave_quest_log_find(&game->quest_log, actions[i].quest_id) == NULL) {
                    fprintf(stderr, "Dialogue '%s' references unknown quest '%s'\n",
                        path,
                        actions[i].quest_id != NULL ? actions[i].quest_id : "<null>");
                    return false;
                }
                break;
            case MOJAVE_EVENT_ACTION_GIVE_ITEM:
            case MOJAVE_EVENT_ACTION_REMOVE_ITEM:
                if (actions[i].item_id == NULL || mojave_item_database_find(&game->item_database, actions[i].item_id) == NULL) {
                    fprintf(stderr, "Dialogue '%s' references unknown item '%s'\n",
                        path,
                        actions[i].item_id != NULL ? actions[i].item_id : "<null>");
                    return false;
                }
                break;
            case MOJAVE_EVENT_ACTION_NONE:
            default:
                break;
        }
    }

    return true;
}

static bool mojave_game_validate_dialogue_conditions(const MojaveGame *game,
    const char *path,
    const MojaveCondition *conditions,
    int condition_count) {
    int i;

    for (i = 0; i < condition_count; i += 1) {
        switch (conditions[i].type) {
            case MOJAVE_CONDITION_HAS_ITEM:
                if (conditions[i].item_id == NULL || mojave_item_database_find(&game->item_database, conditions[i].item_id) == NULL) {
                    fprintf(stderr, "Dialogue '%s' references unknown item '%s' in a condition\n",
                        path,
                        conditions[i].item_id != NULL ? conditions[i].item_id : "<null>");
                    return false;
                }
                break;
            case MOJAVE_CONDITION_NONE:
            default:
                break;
        }
    }

    return true;
}

static bool mojave_game_validate_dialogue(const MojaveGame *game, const char *path, const MojaveDialogue *dialogue) {
    int i;

    if (game == NULL || path == NULL || dialogue == NULL || dialogue->start_id == NULL) {
        return false;
    }
    if (mojave_dialogue_find_node(dialogue, dialogue->start_id) == NULL) {
        fprintf(stderr, "Dialogue '%s' start node '%s' does not exist\n", path, dialogue->start_id);
        return false;
    }

    for (i = 0; i < dialogue->node_count; i += 1) {
        const MojaveDialogueNode *node = &dialogue->nodes[i];
        int choice_index;

        if (node->next_id != NULL && mojave_dialogue_find_node(dialogue, node->next_id) == NULL) {
            fprintf(stderr, "Dialogue '%s' node '%s' points to unknown next node '%s'\n", path, node->id, node->next_id);
            return false;
        }
        if (!mojave_game_validate_dialogue_conditions(game, path, node->conditions, node->condition_count) ||
            !mojave_game_validate_dialogue_actions(game, path, node->actions, node->action_count)) {
            return false;
        }

        for (choice_index = 0; choice_index < node->choice_count; choice_index += 1) {
            const MojaveDialogueChoice *choice = &node->choices[choice_index];

            if (mojave_dialogue_find_node(dialogue, choice->next_id) == NULL) {
                fprintf(stderr, "Dialogue '%s' node '%s' has a choice pointing to unknown node '%s'\n",
                    path,
                    node->id,
                    choice->next_id);
                return false;
            }
            if (!mojave_game_validate_dialogue_conditions(game, path, choice->conditions, choice->condition_count) ||
                !mojave_game_validate_dialogue_actions(game, path, choice->actions, choice->action_count)) {
                return false;
            }
        }
    }

    return true;
}

static bool mojave_game_validate_content(const MojaveGame *game) {
    int i;

    if (game == NULL) {
        return false;
    }

    for (i = 0; i < game->map.item_count; i += 1) {
        if (mojave_item_database_find(&game->item_database, game->map.items[i].item_id) == NULL) {
            fprintf(stderr, "Map references unknown item '%s'\n", game->map.items[i].item_id);
            return false;
        }
    }

    for (i = 0; i < game->map.npc_count; i += 1) {
        MojaveDialogue dialogue = {0};
        bool ok;

        if (!mojave_dialogue_load(game->map.npcs[i].dialogue_path, &dialogue)) {
            fprintf(stderr, "Failed to preload dialogue '%s' for npc '%s'\n",
                game->map.npcs[i].dialogue_path,
                game->map.npcs[i].id != NULL ? game->map.npcs[i].id : "<unknown>");
            return false;
        }
        ok = mojave_game_validate_dialogue(game, game->map.npcs[i].dialogue_path, &dialogue);
        mojave_dialogue_unload(&dialogue);
        if (!ok) {
            return false;
        }
    }

    return true;
}

static bool mojave_game_write_save(MojaveGame *game, float player_x, float player_y) {
    yyjson_mut_doc *doc;
    yyjson_mut_val *root;
    yyjson_mut_val *flags;
    yyjson_mut_val *quests;
    yyjson_mut_val *inventory;
    yyjson_mut_val *collected_items;
    char *json;
    size_t length;
    FILE *file;
    int i;

    if (game == NULL || game->save_path == NULL) {
        return false;
    }

    doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) {
        return false;
    }

    root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_real(doc, root, "player_x", player_x);
    yyjson_mut_obj_add_real(doc, root, "player_y", player_y);

    flags = yyjson_mut_arr(doc);
    yyjson_mut_obj_add_val(doc, root, "flags", flags);
    for (i = 0; i < game->flag_count; i += 1) {
        yyjson_mut_val *flag = yyjson_mut_obj(doc);
        yyjson_mut_obj_add_strcpy(doc, flag, "id", game->flags[i].id);
        yyjson_mut_obj_add_bool(doc, flag, "value", game->flags[i].value);
        yyjson_mut_arr_add_val(flags, flag);
    }

    quests = yyjson_mut_arr(doc);
    yyjson_mut_obj_add_val(doc, root, "quests", quests);
    for (i = 0; i < game->quest_state_count; i += 1) {
        yyjson_mut_val *quest = yyjson_mut_obj(doc);

        if (game->quest_states[i].definition == NULL) {
            continue;
        }
        yyjson_mut_obj_add_strcpy(doc, quest, "id", game->quest_states[i].definition->id);
        yyjson_mut_obj_add_int(doc, quest, "stage", game->quest_states[i].stage);
        yyjson_mut_obj_add_bool(doc, quest, "active", game->quest_states[i].active);
        yyjson_mut_obj_add_bool(doc, quest, "completed", game->quest_states[i].completed);
        yyjson_mut_arr_add_val(quests, quest);
    }

    inventory = yyjson_mut_arr(doc);
    yyjson_mut_obj_add_val(doc, root, "inventory", inventory);
    for (i = 0; i < game->inventory_count; i += 1) {
        yyjson_mut_val *entry = yyjson_mut_obj(doc);

        if (game->inventory[i].definition == NULL) {
            continue;
        }
        yyjson_mut_obj_add_strcpy(doc, entry, "id", game->inventory[i].definition->id);
        yyjson_mut_obj_add_int(doc, entry, "count", game->inventory[i].count);
        yyjson_mut_arr_add_val(inventory, entry);
    }

    collected_items = yyjson_mut_arr(doc);
    yyjson_mut_obj_add_val(doc, root, "collected_items", collected_items);
    for (i = 0; i < game->map.item_count; i += 1) {
        yyjson_mut_arr_add_bool(doc, collected_items, game->map_item_collected != NULL && game->map_item_collected[i]);
    }

    json = yyjson_mut_write(doc, YYJSON_WRITE_PRETTY, &length);
    yyjson_mut_doc_free(doc);
    if (json == NULL) {
        return false;
    }

    file = fopen(game->save_path, "wb");
    if (file == NULL) {
        free(json);
        return false;
    }
    if (fwrite(json, 1, length, file) != length) {
        fclose(file);
        free(json);
        return false;
    }
    fclose(file);
    free(json);
    return true;
}

static bool mojave_game_read_save(MojaveGame *game, float *player_x, float *player_y) {
    yyjson_doc *doc;
    yyjson_val *root;
    yyjson_val *flags;
    yyjson_val *quests;
    yyjson_val *inventory;
    yyjson_val *collected_items;
    yyjson_arr_iter iter;
    yyjson_val *value;
    int index;

    if (game == NULL || player_x == NULL || player_y == NULL || game->save_path == NULL) {
        return false;
    }

    doc = yyjson_read_file(game->save_path, 0, NULL, NULL);
    if (doc == NULL) {
        return false;
    }
    root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return false;
    }

    *player_x = (float)yyjson_get_real(yyjson_obj_get(root, "player_x"));
    *player_y = (float)yyjson_get_real(yyjson_obj_get(root, "player_y"));

    flags = yyjson_obj_get(root, "flags");
    if (yyjson_is_arr(flags)) {
        yyjson_arr_iter_init(flags, &iter);
        while ((value = yyjson_arr_iter_next(&iter)) != NULL) {
            yyjson_val *id = yyjson_obj_get(value, "id");
            yyjson_val *flag_value = yyjson_obj_get(value, "value");

            if (yyjson_is_str(id) && yyjson_is_bool(flag_value)) {
                mojave_game_set_flag(game, yyjson_get_str(id), yyjson_get_bool(flag_value));
            }
        }
    }

    quests = yyjson_obj_get(root, "quests");
    if (yyjson_is_arr(quests)) {
        yyjson_arr_iter_init(quests, &iter);
        while ((value = yyjson_arr_iter_next(&iter)) != NULL) {
            yyjson_val *id = yyjson_obj_get(value, "id");
            yyjson_val *stage = yyjson_obj_get(value, "stage");
            yyjson_val *active = yyjson_obj_get(value, "active");
            yyjson_val *completed = yyjson_obj_get(value, "completed");
            MojaveQuestState *quest_state;

            if (!yyjson_is_str(id)) {
                continue;
            }
            quest_state = mojave_game_find_quest_state(game, yyjson_get_str(id));
            if (quest_state == NULL) {
                continue;
            }
            if (yyjson_is_int(stage)) {
                quest_state->stage = (int)yyjson_get_int(stage);
            }
            if (yyjson_is_bool(active)) {
                quest_state->active = yyjson_get_bool(active);
            }
            if (yyjson_is_bool(completed)) {
                quest_state->completed = yyjson_get_bool(completed);
            }
        }
    }

    inventory = yyjson_obj_get(root, "inventory");
    if (yyjson_is_arr(inventory)) {
        yyjson_arr_iter_init(inventory, &iter);
        while ((value = yyjson_arr_iter_next(&iter)) != NULL) {
            yyjson_val *id = yyjson_obj_get(value, "id");
            yyjson_val *count = yyjson_obj_get(value, "count");
            const MojaveItemDefinition *definition;
            int item_count;
            int i;

            if (!yyjson_is_str(id) || !yyjson_is_int(count)) {
                continue;
            }
            definition = mojave_item_database_find(&game->item_database, yyjson_get_str(id));
            item_count = (int)yyjson_get_int(count);
            for (i = 0; definition != NULL && i < item_count; i += 1) {
                mojave_game_add_inventory_item(game, definition);
            }
        }
    }

    collected_items = yyjson_obj_get(root, "collected_items");
    if (yyjson_is_arr(collected_items) && game->map_item_collected != NULL) {
        index = 0;
        yyjson_arr_iter_init(collected_items, &iter);
        while ((value = yyjson_arr_iter_next(&iter)) != NULL && index < game->map.item_count) {
            if (yyjson_is_bool(value)) {
                game->map_item_collected[index] = yyjson_get_bool(value);
            }
            index += 1;
        }
    }

    yyjson_doc_free(doc);
    return true;
}

static bool mojave_game_load_npc_dialogue(MojaveGame *game, int npc_index) {
    MojaveDialogue dialogue = {0};
    const DialoguRef *dialogue_ref;
    ecs_entity_t npc_entity;

    if (game == NULL || npc_index < 0 || npc_index >= game->map.npc_count) {
        return false;
    }

    if (game->npc_entities == NULL) {
        return false;
    }

    npc_entity = game->npc_entities[npc_index];
    dialogue_ref = ecs_get(game->world, npc_entity, DialoguRef);
    if (dialogue_ref == NULL || dialogue_ref->dialogue_path == NULL) {
        return false;
    }

    if (!mojave_dialogue_load(dialogue_ref->dialogue_path, &dialogue)) {
        return false;
    }
    if (!mojave_game_validate_dialogue(game, dialogue_ref->dialogue_path, &dialogue)) {
        mojave_dialogue_unload(&dialogue);
        return false;
    }

    mojave_dialogue_unload(&game->dialogue);
    game->dialogue = dialogue;
    game->active_npc_index = npc_index;
    return true;
}

static void mojave_game_set_dialogue_node(MojaveGame *game, const char *node_id) {
    const MojaveDialogueNode *node;

    if (game == NULL || node_id == NULL) {
        return;
    }

    node = mojave_dialogue_find_node(&game->dialogue, node_id);
    if (node != NULL && !mojave_game_conditions_met(game, node->conditions, node->condition_count)) {
        node = NULL;
    }
    game->active_dialogue_node = node;
    game->selected_dialogue_choice = 0;
    if (game->npc_entities != NULL && game->active_npc_index >= 0 && game->active_npc_index < game->map.npc_count) {
        ecs_set(game->world,
            game->npc_entities[game->active_npc_index],
            ActiveDialogue,
            {node != NULL ? node->id : NULL});
    }
    if (node != NULL) {
        mojave_game_run_actions(game, node->actions, node->action_count);
    }
}

static void mojave_game_start_dialogue(MojaveGame *game) {
    int npc_index;

    if (game == NULL) {
        return;
    }

    npc_index = game->nearby_npc_index;
    if (npc_index < 0 || !mojave_game_load_npc_dialogue(game, npc_index) || game->dialogue.start_id == NULL) {
        return;
    }

    mojave_game_set_dialogue_node(game, game->dialogue.start_id);
}

static void mojave_game_end_dialogue(MojaveGame *game) {
    int active_npc_index;

    if (game == NULL) {
        return;
    }

    active_npc_index = game->active_npc_index;
    game->active_dialogue_node = NULL;
    game->active_npc_index = -1;
    game->selected_dialogue_choice = 0;
    if (game->npc_entities != NULL && active_npc_index >= 0 && active_npc_index < game->map.npc_count) {
        ecs_set(game->world, game->npc_entities[active_npc_index], ActiveDialogue, {NULL});
    }
}

static void mojave_game_update_dialogue(MojaveGame *game, const MojaveInput *input) {
    const MojaveDialogueNode *node;
    int visible_choice_count;

    if (game == NULL || input == NULL) {
        return;
    }

    game->nearby_npc_index = mojave_game_find_nearby_npc(game);
    game->nearby_item_index = mojave_game_find_nearby_item(game);
    node = game->active_dialogue_node;
    if (node == NULL) {
        if (input->interact_pressed) {
            if (game->nearby_item_index >= 0) {
                mojave_game_collect_item_at_index(game, game->nearby_item_index);
            } else {
                mojave_game_start_dialogue(game);
            }
        }
        return;
    }

    visible_choice_count = mojave_game_dialogue_visible_choice_count(game);
    if (visible_choice_count > 0) {
        if (input->menu_up_pressed) {
            game->selected_dialogue_choice -= 1;
            if (game->selected_dialogue_choice < 0) {
                game->selected_dialogue_choice = visible_choice_count - 1;
            }
        }
        if (input->menu_down_pressed) {
            game->selected_dialogue_choice += 1;
            if (game->selected_dialogue_choice >= visible_choice_count) {
                game->selected_dialogue_choice = 0;
            }
        }
    }

    if (!input->interact_pressed) {
        return;
    }

    if (visible_choice_count > 0) {
        int actual_choice_index = mojave_game_dialogue_choice_index_from_visible(game, game->selected_dialogue_choice);

        if (actual_choice_index < 0) {
            return;
        }
        mojave_game_run_actions(
            game,
            node->choices[actual_choice_index].actions,
            node->choices[actual_choice_index].action_count
        );
        mojave_game_set_dialogue_node(game, node->choices[actual_choice_index].next_id);
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
    int i;

    if (game == NULL) {
        return false;
    }

    memset(game, 0, sizeof(*game));
    game->save_path = save_path;

    if (!mojave_map_load(map_path, &game->map)) {
        return false;
    }

    if (!mojave_item_database_load(MOJAVE_ITEM_DATABASE_PATH, &game->item_database)) {
        mojave_map_unload(&game->map);
        return false;
    }

    if (!mojave_quest_log_load(MOJAVE_QUEST_LOG_PATH, &game->quest_log)) {
        mojave_item_database_unload(&game->item_database);
        mojave_map_unload(&game->map);
        return false;
    }

    if (!mojave_game_validate_content(game)) {
        mojave_item_database_unload(&game->item_database);
        mojave_quest_log_unload(&game->quest_log);
        mojave_map_unload(&game->map);
        return false;
    }

    game->world = ecs_init();
    if (game->world == NULL) {
        mojave_item_database_unload(&game->item_database);
        mojave_quest_log_unload(&game->quest_log);
        mojave_map_unload(&game->map);
        return false;
    }

    game->active_npc_index = -1;
    game->nearby_npc_index = -1;
    game->nearby_item_index = -1;
    game->map_item_collected = calloc((size_t)game->map.item_count, sizeof(*game->map_item_collected));
    if (game->map_item_collected == NULL && game->map.item_count > 0) {
        ecs_fini(game->world);
        game->world = NULL;
        mojave_item_database_unload(&game->item_database);
        mojave_quest_log_unload(&game->quest_log);
        mojave_map_unload(&game->map);
        return false;
    }
    game->quest_state_count = game->quest_log.quest_count;
    game->quest_states = calloc((size_t)game->quest_state_count, sizeof(*game->quest_states));
    if (game->quest_states == NULL && game->quest_state_count > 0) {
        ecs_fini(game->world);
        game->world = NULL;
        free(game->map_item_collected);
        game->map_item_collected = NULL;
        mojave_item_database_unload(&game->item_database);
        mojave_quest_log_unload(&game->quest_log);
        mojave_map_unload(&game->map);
        return false;
    }
    for (i = 0; i < game->quest_state_count; i += 1) {
        game->quest_states[i].definition = &game->quest_log.quests[i];
        game->quest_states[i].stage = -1;
    }

    ECS_COMPONENT_DEFINE(game->world, Position);
    ECS_COMPONENT_DEFINE(game->world, Velocity);
    ECS_COMPONENT_DEFINE(game->world, CollisionBox);
    ECS_COMPONENT_DEFINE(game->world, Renderable);
    ECS_COMPONENT_DEFINE(game->world, Team);
    ECS_COMPONENT_DEFINE(game->world, Hp);
    ECS_COMPONENT_DEFINE(game->world, ItemRef);
    ECS_COMPONENT_DEFINE(game->world, DialoguRef);
    ECS_COMPONENT_DEFINE(game->world, ActiveDialogue);
    ECS_COMPONENT_DEFINE(game->world, Npc);
    ECS_COMPONENT_DEFINE(game->world, Item);

    ECS_SYSTEM(game->world, MovementSystem, EcsOnUpdate, Position, Velocity);
    ECS_SYSTEM(game->world, TilemapCollisionSystem, EcsOnUpdate, Position, Velocity, CollisionBox);
    ECS_SYSTEM(game->world, CombatSystem, EcsOnUpdate, Hp);

    g_map = &game->map;

    game->player = mojave_game_spawn_player_ecs(
        game,
        (float)(game->map.player_spawn_x * game->map.tile_size + 7),
        (float)(game->map.player_spawn_y * game->map.tile_size + 7));

    position = ecs_get_mut(game->world, game->player, Position);
    if (mojave_game_read_save(game, &position->x, &position->y)) {
        game->save_loaded = true;
    }

    if (game->map.npc_count > 0) {
        game->npc_entities = malloc((size_t)game->map.npc_count * sizeof(*game->npc_entities));
        if (game->npc_entities == NULL) {
            ecs_fini(game->world);
            game->world = NULL;
            free(game->quest_states);
            game->quest_states = NULL;
            free(game->map_item_collected);
            game->map_item_collected = NULL;
            mojave_item_database_unload(&game->item_database);
            mojave_quest_log_unload(&game->quest_log);
            mojave_map_unload(&game->map);
            return false;
        }
        for (i = 0; i < game->map.npc_count; i += 1) {
            game->npc_entities[i] = mojave_game_spawn_npc_ecs(game, &game->map.npcs[i], NULL);
        }
    }

    if (game->map.item_count > 0) {
        const MojaveItemDefinition *def;
        game->item_entities = malloc((size_t)game->map.item_count * sizeof(*game->item_entities));
        if (game->item_entities == NULL) {
            ecs_fini(game->world);
            game->world = NULL;
            free(game->npc_entities);
            game->npc_entities = NULL;
            free(game->quest_states);
            game->quest_states = NULL;
            free(game->map_item_collected);
            game->map_item_collected = NULL;
            mojave_item_database_unload(&game->item_database);
            mojave_quest_log_unload(&game->quest_log);
            mojave_map_unload(&game->map);
            return false;
        }
        for (i = 0; i < game->map.item_count; i += 1) {
            float wx = (float)(game->map.items[i].spawn_x * game->map.tile_size + 10);
            float wy = (float)(game->map.items[i].spawn_y * game->map.tile_size + 10);
            def = mojave_item_database_find(&game->item_database, game->map.items[i].item_id);
            game->item_entities[i] = mojave_game_spawn_item_ecs(game, def, wx, wy, i);
        }
    }

    return true;
}

ecs_entity_t mojave_game_spawn_player_ecs(MojaveGame *game, float x, float y) {
    ecs_entity_t e = ecs_new(game->world);
    ecs_set(game->world, e, Position, {x, y});
    ecs_set(game->world, e, Velocity, {0.0f, 0.0f});
    ecs_set(game->world, e, CollisionBox, {MOJAVE_PLAYER_SIZE, MOJAVE_PLAYER_SIZE});
    ecs_set(game->world, e, Renderable, {42, 122, 184, 255});
    ecs_set(game->world, e, Team, {MOJAVE_TEAM_PLAYER});
    ecs_set(game->world, e, Hp, {100.0f, 100.0f});
    return e;
}

ecs_entity_t mojave_game_spawn_npc_ecs(MojaveGame *game, const MojaveNpc *npc_def, const MojaveDialogue *dialogue) {
    ecs_entity_t e = ecs_new(game->world);
    float wx = (float)(npc_def->spawn_x * game->map.tile_size + 7);
    float wy = (float)(npc_def->spawn_y * game->map.tile_size + 7);
    (void)dialogue;
    ecs_set(game->world, e, Position, {wx, wy});
    ecs_set(game->world, e, Velocity, {0.0f, 0.0f});
    ecs_set(game->world, e, CollisionBox, {MOJAVE_NPC_SIZE, MOJAVE_NPC_SIZE});
    ecs_set(game->world, e, Renderable, {npc_def->outfit_r, npc_def->outfit_g, npc_def->outfit_b, 255});
    ecs_set(game->world, e, Team, {MOJAVE_TEAM_FRIENDLY});
    ecs_set(game->world, e, DialoguRef, {npc_def->dialogue_path});
    ecs_set(game->world, e, ActiveDialogue, {NULL});
    ecs_set(game->world, e, Npc, {npc_def->name});
    return e;
}

ecs_entity_t mojave_game_spawn_item_ecs(MojaveGame *game, const MojaveItemDefinition *item_def, float x, float y, int map_index) {
    ecs_entity_t e = ecs_new(game->world);
    ecs_set(game->world, e, Position, {x, y});
    ecs_set(game->world, e, CollisionBox, {MOJAVE_ITEM_SIZE, MOJAVE_ITEM_SIZE});
    ecs_set(game->world, e, Renderable, {item_def->color_r, item_def->color_g, item_def->color_b, 255});
    ecs_set(game->world, e, ItemRef, {item_def});
    ecs_set(game->world, e, Item, {map_index});
    return e;
}

void mojave_game_damage_entity(MojaveGame *game, ecs_entity_t entity, float damage) {
    Hp *hp = ecs_get_mut(game->world, entity, Hp);
    if (hp != NULL) {
        hp->current -= damage;
        if (hp->current < 0.0f) {
            hp->current = 0.0f;
        }
    }
}

float mojave_game_get_entity_hp(MojaveGame *game, ecs_entity_t entity) {
    const Hp *hp = ecs_get(game->world, entity, Hp);
    return hp ? hp->current : 0.0f;
}

bool mojave_game_entity_is_alive(MojaveGame *game, ecs_entity_t entity) {
    const Hp *hp = ecs_get(game->world, entity, Hp);
    return hp != NULL && hp->current > 0.0f;
}

ecs_entity_t mojave_game_get_player(const MojaveGame *game) {
    return game->player;
}

bool mojave_game_get_entity_render_data(const MojaveGame *game, ecs_entity_t entity, MojaveRenderData *out) {
    const Position *pos = ecs_get(game->world, entity, Position);
    const Renderable *r = ecs_get(game->world, entity, Renderable);

    if (pos == NULL || r == NULL) {
        return false;
    }

    out->x = pos->x;
    out->y = pos->y;
    out->r = r->r;
    out->g = r->g;
    out->b = r->b;
    out->a = r->a;
    return true;
}

bool mojave_game_get_item_render_data(const MojaveGame *game, int index, MojaveItemRenderData *out) {
    const Position *pos;
    const Renderable *r;
    const ItemRef *item_ref;
    ecs_entity_t entity;

    if (game == NULL || index < 0 || index >= game->map.item_count) {
        return false;
    }

    if (game->item_entities == NULL) {
        return false;
    }

    entity = game->item_entities[index];
    if (entity == 0) {
        out->collected = true;
        return true;
    }

    pos = ecs_get(game->world, entity, Position);
    r = ecs_get(game->world, entity, Renderable);
    item_ref = ecs_get(game->world, entity, ItemRef);

    if (pos == NULL || r == NULL || item_ref == NULL) {
        return false;
    }

    out->x = pos->x;
    out->y = pos->y;
    out->w = MOJAVE_ITEM_SIZE;
    out->h = MOJAVE_ITEM_SIZE;
    out->r = r->r;
    out->g = r->g;
    out->b = r->b;
    out->a = r->a;
    out->collected = false;
    out->name = item_ref->definition ? item_ref->definition->name : "Unknown";
    return true;
}

bool mojave_game_get_npc_render_data(const MojaveGame *game, int index, MojaveNpcRenderData *out) {
    const Position *pos;
    const Renderable *r;
    const Npc *npc_comp;
    ecs_entity_t entity;

    if (game == NULL || index < 0 || index >= game->map.npc_count) {
        return false;
    }

    if (game->npc_entities == NULL) {
        return false;
    }

    entity = game->npc_entities[index];
    pos = ecs_get(game->world, entity, Position);
    r = ecs_get(game->world, entity, Renderable);
    npc_comp = ecs_get(game->world, entity, Npc);

    if (pos == NULL || r == NULL || npc_comp == NULL) {
        return false;
    }

    out->x = pos->x;
    out->y = pos->y;
    out->w = MOJAVE_NPC_SIZE;
    out->h = MOJAVE_NPC_SIZE;
    out->r = r->r;
    out->g = r->g;
    out->b = r->b;
    out->a = r->a;
    out->name = npc_comp->name;
    return true;
}

int mojave_map_get_width(const MojaveMap *map) {
    if (map == NULL) {
        return 0;
    }
    return map->width;
}

int mojave_map_get_height(const MojaveMap *map) {
    if (map == NULL) {
        return 0;
    }
    return map->height;
}

int mojave_map_get_tile_size(const MojaveMap *map) {
    if (map == NULL) {
        return 0;
    }
    return map->tile_size;
}

int mojave_map_get_tile(const MojaveMap *map, int x, int y) {
    if (map == NULL || x < 0 || y < 0 || x >= map->width || y >= map->height) {
        return 1;
    }
    return map->tiles[y * map->width + x];
}

const char *mojave_map_get_name(const MojaveMap *map) {
    if (map == NULL) {
        return "";
    }
    return map->name;
}

void mojave_game_shutdown(MojaveGame *game) {
    if (game == NULL) {
        return;
    }

    if (game->world != NULL) {
        ecs_fini(game->world);
        game->world = NULL;
    }

    if (game->flags != NULL) {
        int i;

        for (i = 0; i < game->flag_count; i += 1) {
            free(game->flags[i].id);
        }
        free(game->flags);
        game->flags = NULL;
        game->flag_count = 0;
    }
    free(game->quest_states);
    game->quest_states = NULL;
    game->quest_state_count = 0;
    free(game->npc_entities);
    game->npc_entities = NULL;
    free(game->item_entities);
    game->item_entities = NULL;
    free(game->inventory);
    game->inventory = NULL;
    game->inventory_count = 0;
    free(game->map_item_collected);
    game->map_item_collected = NULL;
    mojave_dialogue_unload(&game->dialogue);
    mojave_item_database_unload(&game->item_database);
    mojave_quest_log_unload(&game->quest_log);
    mojave_map_unload(&game->map);
}

void mojave_game_update(MojaveGame *game, const MojaveInput *input, float dt) {
    Position *position;
    Velocity velocity;
    bool dialogue_was_active;

    if (game == NULL || game->world == NULL) {
        return;
    }

    if (input != NULL && !mojave_game_dialogue_active(game)) {
        if (input->quest_log_pressed) {
            game->show_quest_log = !game->show_quest_log;
            if (game->show_quest_log) {
                game->show_inventory = false;
            }
        }
        if (input->inventory_pressed) {
            game->show_inventory = !game->show_inventory;
            if (game->show_inventory) {
                game->show_quest_log = false;
            }
        }
    }

    dialogue_was_active = game->active_dialogue_node != NULL;
    mojave_game_update_dialogue(game, input);
    if (dialogue_was_active || game->active_dialogue_node != NULL) {
        return;
    }

    if (game->show_quest_log || game->show_inventory) {
        return;
    }

    position = ecs_get_mut(game->world, game->player, Position);
    velocity = mojave_velocity_from_input(input);
    ecs_set(game->world, game->player, Velocity, {velocity.x, velocity.y});

    ecs_progress(game->world, dt);

    if (input != NULL && input->save_pressed) {
        if (mojave_game_write_save(game, position->x, position->y)) {
            game->save_loaded = true;
        }
    }

    if (input != NULL && input->load_pressed) {
        float save_x;
        float save_y;
        int i;

        free(game->inventory);
        game->inventory = NULL;
        game->inventory_count = 0;
        if (game->flags != NULL) {
            for (i = 0; i < game->flag_count; i += 1) {
                free(game->flags[i].id);
            }
            free(game->flags);
            game->flags = NULL;
            game->flag_count = 0;
        }
        for (i = 0; i < game->quest_state_count; i += 1) {
            game->quest_states[i].stage = -1;
            game->quest_states[i].active = false;
            game->quest_states[i].completed = false;
        }
        if (game->map_item_collected != NULL) {
            memset(game->map_item_collected, 0, (size_t)game->map.item_count * sizeof(*game->map_item_collected));
        }
        if (mojave_game_read_save(game, &save_x, &save_y)) {
            position->x = save_x;
            position->y = save_y;
            game->save_loaded = true;
        }
        if (game->item_entities != NULL) {
            const MojaveItemDefinition *def;
            for (i = 0; i < game->map.item_count; i += 1) {
                if (game->item_entities[i] != 0) {
                    ecs_delete(game->world, game->item_entities[i]);
                }
                if (game->map_item_collected != NULL && game->map_item_collected[i]) {
                    game->item_entities[i] = 0;
                } else {
                    float wx = (float)(game->map.items[i].spawn_x * game->map.tile_size + 10);
                    float wy = (float)(game->map.items[i].spawn_y * game->map.tile_size + 10);
                    def = mojave_item_database_find(&game->item_database, game->map.items[i].item_id);
                    game->item_entities[i] = mojave_game_spawn_item_ecs(game, def, wx, wy, i);
                }
            }
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

bool mojave_game_ui_blocking(const MojaveGame *game) {
    return game != NULL && (game->show_quest_log || game->show_inventory || game->active_dialogue_node != NULL);
}

bool mojave_game_show_quest_log(const MojaveGame *game) {
    return game != NULL && game->show_quest_log;
}

bool mojave_game_show_inventory(const MojaveGame *game) {
    return game != NULL && game->show_inventory;
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

int mojave_game_dialogue_visible_choice_count(const MojaveGame *game) {
    const MojaveDialogueNode *node;
    int count = 0;
    int i;

    if (game == NULL) {
        return 0;
    }

    node = game->active_dialogue_node;
    if (node == NULL) {
        return 0;
    }

    for (i = 0; i < node->choice_count; i += 1) {
        if (mojave_game_conditions_met(game, node->choices[i].conditions, node->choices[i].condition_count)) {
            count += 1;
        }
    }

    return count;
}

const MojaveDialogueChoice *mojave_game_dialogue_visible_choice(const MojaveGame *game, int index) {
    const MojaveDialogueNode *node;
    int actual_index;

    if (game == NULL) {
        return NULL;
    }

    node = game->active_dialogue_node;
    actual_index = mojave_game_dialogue_choice_index_from_visible(game, index);
    if (node == NULL || actual_index < 0) {
        return NULL;
    }

    return &node->choices[actual_index];
}

int mojave_game_nearby_npc_index(const MojaveGame *game) {
    if (game == NULL) {
        return -1;
    }

    return game->nearby_npc_index;
}

int mojave_game_npc_count(const MojaveGame *game) {
    if (game == NULL) {
        return 0;
    }

    return game->map.npc_count;
}

const MojaveNpc *mojave_game_npc(const MojaveGame *game, int index) {
    if (game == NULL || index < 0 || index >= game->map.npc_count) {
        return NULL;
    }

    return &game->map.npcs[index];
}

int mojave_game_nearby_item_index(const MojaveGame *game) {
    if (game == NULL) {
        return -1;
    }

    return game->nearby_item_index;
}

int mojave_game_map_item_count(const MojaveGame *game) {
    if (game == NULL) {
        return 0;
    }

    return game->map.item_count;
}

const MojaveMapItem *mojave_game_map_item(const MojaveGame *game, int index) {
    if (game == NULL || index < 0 || index >= game->map.item_count) {
        return NULL;
    }

    return &game->map.items[index];
}

bool mojave_game_map_item_collected(const MojaveGame *game, int index) {
    if (game == NULL || index < 0 || index >= game->map.item_count || game->map_item_collected == NULL) {
        return false;
    }

    return game->map_item_collected[index];
}

int mojave_game_inventory_count(const MojaveGame *game) {
    if (game == NULL) {
        return 0;
    }

    return game->inventory_count;
}

const MojaveInventoryEntry *mojave_game_inventory_entry(const MojaveGame *game, int index) {
    if (game == NULL || index < 0 || index >= game->inventory_count) {
        return NULL;
    }

    return &game->inventory[index];
}

int mojave_game_active_quest_count(const MojaveGame *game) {
    int count = 0;
    int i;

    if (game == NULL) {
        return 0;
    }

    for (i = 0; i < game->quest_state_count; i += 1) {
        if (game->quest_states[i].active) {
            count += 1;
        }
    }

    return count;
}

const MojaveQuestState *mojave_game_active_quest(const MojaveGame *game, int index) {
    int active_index = 0;
    int i;

    if (game == NULL || index < 0) {
        return NULL;
    }

    for (i = 0; i < game->quest_state_count; i += 1) {
        if (!game->quest_states[i].active) {
            continue;
        }
        if (active_index == index) {
            return &game->quest_states[i];
        }
        active_index += 1;
    }

    return NULL;
}

int mojave_game_completed_quest_count(const MojaveGame *game) {
    int count = 0;
    int i;

    if (game == NULL) {
        return 0;
    }

    for (i = 0; i < game->quest_state_count; i += 1) {
        if (game->quest_states[i].completed) {
            count += 1;
        }
    }

    return count;
}

const MojaveQuestState *mojave_game_completed_quest(const MojaveGame *game, int index) {
    int completed_index = 0;
    int i;

    if (game == NULL || index < 0) {
        return NULL;
    }

    for (i = 0; i < game->quest_state_count; i += 1) {
        if (!game->quest_states[i].completed) {
            continue;
        }
        if (completed_index == index) {
            return &game->quest_states[i];
        }
        completed_index += 1;
    }

    return NULL;
}
