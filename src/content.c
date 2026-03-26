#include "content.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yyjson.h>

static void mojave_map_reset(MojaveMap *map) {
    int i;

    if (map == NULL) {
        return;
    }

    for (i = 0; i < map->npc_count; i += 1) {
        free(map->npcs[i].id);
        free(map->npcs[i].name);
        free(map->npcs[i].dialogue_path);
    }
    for (i = 0; i < map->item_count; i += 1) {
        free(map->items[i].item_id);
    }
    free(map->items);
    free(map->npcs);
    free(map->tiles);
    memset(map, 0, sizeof(*map));
}

static void mojave_item_database_reset(MojaveItemDatabase *item_database) {
    int i;

    if (item_database == NULL) {
        return;
    }

    for (i = 0; i < item_database->item_count; i += 1) {
        free(item_database->items[i].id);
        free(item_database->items[i].name);
        free(item_database->items[i].description);
    }
    free(item_database->items);
    memset(item_database, 0, sizeof(*item_database));
}

static void mojave_dialogue_node_reset(MojaveDialogueNode *node) {
    int i;

    if (node == NULL) {
        return;
    }

    free(node->id);
    free(node->speaker);
    free(node->text);
    free(node->next_id);
    for (i = 0; i < node->condition_count; i += 1) {
        free(node->conditions[i].item_id);
    }
    free(node->conditions);
    for (i = 0; i < node->action_count; i += 1) {
        free(node->actions[i].flag_id);
        free(node->actions[i].quest_id);
        free(node->actions[i].item_id);
    }
    free(node->actions);
    for (i = 0; i < node->choice_count; i += 1) {
        int condition_index;
        int action_index;

        free(node->choices[i].text);
        free(node->choices[i].next_id);
        for (condition_index = 0; condition_index < node->choices[i].condition_count; condition_index += 1) {
            free(node->choices[i].conditions[condition_index].item_id);
        }
        free(node->choices[i].conditions);
        for (action_index = 0; action_index < node->choices[i].action_count; action_index += 1) {
            free(node->choices[i].actions[action_index].flag_id);
            free(node->choices[i].actions[action_index].quest_id);
            free(node->choices[i].actions[action_index].item_id);
        }
        free(node->choices[i].actions);
    }
    free(node->choices);
    memset(node, 0, sizeof(*node));
}

static void mojave_quest_log_reset(MojaveQuestLog *quest_log) {
    int i;

    if (quest_log == NULL) {
        return;
    }

    for (i = 0; i < quest_log->quest_count; i += 1) {
        int stage_index;

        free(quest_log->quests[i].id);
        free(quest_log->quests[i].title);
        for (stage_index = 0; stage_index < quest_log->quests[i].stage_count; stage_index += 1) {
            free(quest_log->quests[i].stages[stage_index]);
        }
        free(quest_log->quests[i].stages);
    }
    free(quest_log->quests);
    memset(quest_log, 0, sizeof(*quest_log));
}

static void mojave_dialogue_reset(MojaveDialogue *dialogue) {
    int i;

    if (dialogue == NULL) {
        return;
    }

    free(dialogue->start_id);
    for (i = 0; i < dialogue->node_count; i += 1) {
        mojave_dialogue_node_reset(&dialogue->nodes[i]);
    }
    free(dialogue->nodes);
    memset(dialogue, 0, sizeof(*dialogue));
}

static char *mojave_strdup(const char *value) {
    size_t length;
    char *copy;

    if (value == NULL) {
        return NULL;
    }

    length = strlen(value) + 1;
    copy = malloc(length);
    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, value, length);
    return copy;
}

static bool mojave_json_read_rgb(yyjson_val *value, unsigned char *out_r, unsigned char *out_g, unsigned char *out_b) {
    yyjson_val *r;
    yyjson_val *g;
    yyjson_val *b;
    int ri, gi, bi;

    if (!yyjson_is_obj(value)) {
        return false;
    }

    r = yyjson_obj_get(value, "r");
    g = yyjson_obj_get(value, "g");
    b = yyjson_obj_get(value, "b");
    if (!yyjson_is_int(r) || !yyjson_is_int(g) || !yyjson_is_int(b)) {
        return false;
    }

    ri = (int)yyjson_get_int(r);
    gi = (int)yyjson_get_int(g);
    bi = (int)yyjson_get_int(b);
    if (ri < 0 || ri > 255 || gi < 0 || gi > 255 || bi < 0 || bi > 255) {
        return false;
    }

    *out_r = (unsigned char)ri;
    *out_g = (unsigned char)gi;
    *out_b = (unsigned char)bi;
    return true;
}

static MojaveEventActionType mojave_event_action_type_from_string(const char *type_name) {
    if (type_name == NULL) {
        return MOJAVE_EVENT_ACTION_NONE;
    }
    if (strcmp(type_name, "set_flag") == 0) {
        return MOJAVE_EVENT_ACTION_SET_FLAG;
    }
    if (strcmp(type_name, "start_quest") == 0) {
        return MOJAVE_EVENT_ACTION_START_QUEST;
    }
    if (strcmp(type_name, "set_quest_stage") == 0) {
        return MOJAVE_EVENT_ACTION_SET_QUEST_STAGE;
    }
    if (strcmp(type_name, "complete_quest") == 0) {
        return MOJAVE_EVENT_ACTION_COMPLETE_QUEST;
    }
    if (strcmp(type_name, "give_item") == 0) {
        return MOJAVE_EVENT_ACTION_GIVE_ITEM;
    }
    if (strcmp(type_name, "remove_item") == 0) {
        return MOJAVE_EVENT_ACTION_REMOVE_ITEM;
    }
    return MOJAVE_EVENT_ACTION_NONE;
}

static MojaveConditionType mojave_condition_type_from_string(const char *type_name) {
    if (type_name == NULL) {
        return MOJAVE_CONDITION_NONE;
    }
    if (strcmp(type_name, "has_item") == 0) {
        return MOJAVE_CONDITION_HAS_ITEM;
    }
    return MOJAVE_CONDITION_NONE;
}

static bool mojave_conditions_load(yyjson_val *conditions_value, MojaveCondition **conditions, int *condition_count) {
    yyjson_arr_iter condition_iter;
    yyjson_val *condition_value;
    int index;

    *conditions = NULL;
    *condition_count = 0;

    if (conditions_value == NULL) {
        return true;
    }
    if (!yyjson_is_arr(conditions_value)) {
        return false;
    }

    *condition_count = (int)yyjson_arr_size(conditions_value);
    *conditions = calloc((size_t)*condition_count, sizeof(**conditions));
    if (*conditions == NULL && *condition_count > 0) {
        return false;
    }

    index = 0;
    yyjson_arr_iter_init(conditions_value, &condition_iter);
    while ((condition_value = yyjson_arr_iter_next(&condition_iter)) != NULL) {
        MojaveCondition *condition = &(*conditions)[index];
        yyjson_val *type = yyjson_obj_get(condition_value, "type");
        yyjson_val *item = yyjson_obj_get(condition_value, "item");
        yyjson_val *count = yyjson_obj_get(condition_value, "count");

        if (!yyjson_is_obj(condition_value) || !yyjson_is_str(type)) {
            return false;
        }

        condition->type = mojave_condition_type_from_string(yyjson_get_str(type));
        if (condition->type == MOJAVE_CONDITION_NONE) {
            return false;
        }

        if (condition->type == MOJAVE_CONDITION_HAS_ITEM) {
            if (!yyjson_is_str(item)) {
                return false;
            }
            condition->item_id = mojave_strdup(yyjson_get_str(item));
            condition->item_count = yyjson_is_int(count) ? (int)yyjson_get_int(count) : 1;
            if (condition->item_id == NULL || condition->item_count <= 0) {
                return false;
            }
        }

        index += 1;
    }

    return true;
}

static bool mojave_event_actions_load(yyjson_val *actions_value, MojaveEventAction **actions, int *action_count) {
    yyjson_arr_iter action_iter;
    yyjson_val *action_value;
    int index;

    *actions = NULL;
    *action_count = 0;

    if (actions_value == NULL) {
        return true;
    }
    if (!yyjson_is_arr(actions_value)) {
        return false;
    }

    *action_count = (int)yyjson_arr_size(actions_value);
    *actions = calloc((size_t)*action_count, sizeof(**actions));
    if (*actions == NULL && *action_count > 0) {
        return false;
    }

    index = 0;
    yyjson_arr_iter_init(actions_value, &action_iter);
    while ((action_value = yyjson_arr_iter_next(&action_iter)) != NULL) {
        MojaveEventAction *action = &(*actions)[index];
        yyjson_val *type = yyjson_obj_get(action_value, "type");
        yyjson_val *flag = yyjson_obj_get(action_value, "flag");
        yyjson_val *value = yyjson_obj_get(action_value, "value");
        yyjson_val *quest = yyjson_obj_get(action_value, "quest");
        yyjson_val *stage = yyjson_obj_get(action_value, "stage");
        yyjson_val *item = yyjson_obj_get(action_value, "item");
        yyjson_val *count = yyjson_obj_get(action_value, "count");

        if (!yyjson_is_obj(action_value) || !yyjson_is_str(type)) {
            return false;
        }

        action->type = mojave_event_action_type_from_string(yyjson_get_str(type));
        if (action->type == MOJAVE_EVENT_ACTION_NONE) {
            return false;
        }

        if (action->type == MOJAVE_EVENT_ACTION_SET_FLAG) {
            if (!yyjson_is_str(flag) || !yyjson_is_bool(value)) {
                return false;
            }
            action->flag_id = mojave_strdup(yyjson_get_str(flag));
            action->flag_value = yyjson_get_bool(value);
            if (action->flag_id == NULL) {
                return false;
            }
        } else if (action->type == MOJAVE_EVENT_ACTION_GIVE_ITEM || action->type == MOJAVE_EVENT_ACTION_REMOVE_ITEM) {
            if (!yyjson_is_str(item)) {
                return false;
            }
            action->item_id = mojave_strdup(yyjson_get_str(item));
            action->item_count = yyjson_is_int(count) ? (int)yyjson_get_int(count) : 1;
            if (action->item_id == NULL || action->item_count <= 0) {
                return false;
            }
        } else {
            if (!yyjson_is_str(quest)) {
                return false;
            }
            action->quest_id = mojave_strdup(yyjson_get_str(quest));
            if (action->quest_id == NULL) {
                return false;
            }
            if (action->type == MOJAVE_EVENT_ACTION_SET_QUEST_STAGE) {
                if (!yyjson_is_int(stage)) {
                    return false;
                }
                action->quest_stage = (int)yyjson_get_int(stage);
            }
        }

        index += 1;
    }

    return true;
}

static bool mojave_json_read_file(const char *path, yyjson_doc **doc) {
    yyjson_read_err err;

    *doc = yyjson_read_file(path, 0, NULL, &err);
    if (*doc != NULL) {
        return true;
    }

    fprintf(stderr, "Failed to read JSON file '%s': %s at byte %zu\n",
        path,
        err.msg,
        err.pos);
    return false;
}

static bool mojave_file_exists(const char *path) {
    FILE *file = fopen(path, "rb");

    if (file == NULL) {
        return false;
    }

    fclose(file);
    return true;
}

bool mojave_map_load(const char *path, MojaveMap *map) {
    yyjson_doc *doc;
    yyjson_val *root;
    yyjson_val *items;
    yyjson_val *npcs;
    yyjson_val *tiles;
    yyjson_val *spawn;
    yyjson_arr_iter iter;
    yyjson_val *tile;
    yyjson_val *name;
    yyjson_val *width;
    yyjson_val *height;
    yyjson_val *tile_size;
    yyjson_val *spawn_x;
    yyjson_val *spawn_y;
    size_t index;
    size_t expected_count;
    int npc_index;
    int item_index;

    if (path == NULL || map == NULL) {
        return false;
    }

    mojave_map_reset(map);

    if (!mojave_json_read_file(path, &doc)) {
        return false;
    }

    root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        fprintf(stderr, "Map file '%s' must contain a JSON object\n", path);
        yyjson_doc_free(doc);
        return false;
    }

    tiles = yyjson_obj_get(root, "tiles");
    items = yyjson_obj_get(root, "items");
    npcs = yyjson_obj_get(root, "npcs");
    spawn = yyjson_obj_get(root, "player_spawn");
    name = yyjson_obj_get(root, "name");
    width = yyjson_obj_get(root, "width");
    height = yyjson_obj_get(root, "height");
    tile_size = yyjson_obj_get(root, "tile_size");

    if (!yyjson_is_arr(tiles) || !yyjson_is_obj(spawn)) {
        fprintf(stderr, "Map file '%s' is missing required fields\n", path);
        yyjson_doc_free(doc);
        return false;
    }

    spawn_x = yyjson_obj_get(spawn, "x");
    spawn_y = yyjson_obj_get(spawn, "y");

    if (!yyjson_is_int(width) || !yyjson_is_int(height) || !yyjson_is_int(tile_size) ||
        !yyjson_is_int(spawn_x) || !yyjson_is_int(spawn_y)) {
        fprintf(stderr, "Map file '%s' has invalid numeric fields\n", path);
        yyjson_doc_free(doc);
        return false;
    }

    map->width = (int)yyjson_get_int(width);
    map->height = (int)yyjson_get_int(height);
    map->tile_size = (int)yyjson_get_int(tile_size);
    map->player_spawn_x = (int)yyjson_get_int(spawn_x);
    map->player_spawn_y = (int)yyjson_get_int(spawn_y);

    if (map->width <= 0 || map->height <= 0 || map->tile_size <= 0) {
        fprintf(stderr, "Map file '%s' has invalid dimensions\n", path);
        yyjson_doc_free(doc);
        return false;
    }
    if (map->width > 1024 || map->height > 1024) {
        fprintf(stderr, "Map file '%s' has excessively large dimensions (%dx%d)\n", path, map->width, map->height);
        yyjson_doc_free(doc);
        return false;
    }
    expected_count = (size_t)map->width * (size_t)map->height;
    if (yyjson_arr_size(tiles) != expected_count) {
        fprintf(stderr,
            "Map file '%s' has %zu tiles but expected %zu\n",
            path,
            yyjson_arr_size(tiles),
            expected_count);
        yyjson_doc_free(doc);
        return false;
    }

    map->tiles = calloc(expected_count, sizeof(*map->tiles));
    if (map->tiles == NULL) {
        fprintf(stderr, "Out of memory while loading map '%s'\n", path);
        yyjson_doc_free(doc);
        return false;
    }

    /* Keep the map data flat and simple so it is easy to inspect in memory. */
    index = 0;
    yyjson_arr_iter_init(tiles, &iter);
    while ((tile = yyjson_arr_iter_next(&iter)) != NULL) {
        if (!yyjson_is_int(tile)) {
            fprintf(stderr, "Map file '%s' contains a non-integer tile value\n", path);
            yyjson_doc_free(doc);
            mojave_map_reset(map);
            return false;
        }

        map->tiles[index] = (int)yyjson_get_int(tile);
        index += 1;
    }

    snprintf(map->name, sizeof(map->name), "%s",
        yyjson_get_str(name) != NULL
            ? yyjson_get_str(name)
            : "Unnamed Map");

    if (npcs != NULL) {
        yyjson_arr_iter npc_iter;
        yyjson_val *npc_value;

        if (!yyjson_is_arr(npcs)) {
            fprintf(stderr, "Map file '%s' has an invalid npc list\n", path);
            yyjson_doc_free(doc);
            mojave_map_reset(map);
            return false;
        }

        map->npc_count = (int)yyjson_arr_size(npcs);
        map->npcs = calloc((size_t)map->npc_count, sizeof(*map->npcs));
        if (map->npcs == NULL && map->npc_count > 0) {
            yyjson_doc_free(doc);
            mojave_map_reset(map);
            return false;
        }

        npc_index = 0;
        yyjson_arr_iter_init(npcs, &npc_iter);
        while ((npc_value = yyjson_arr_iter_next(&npc_iter)) != NULL) {
            MojaveNpc *npc = &map->npcs[npc_index];
            yyjson_val *npc_id = yyjson_obj_get(npc_value, "id");
            yyjson_val *npc_name = yyjson_obj_get(npc_value, "name");
            yyjson_val *npc_dialogue = yyjson_obj_get(npc_value, "dialogue");
            yyjson_val *npc_spawn = yyjson_obj_get(npc_value, "spawn");
            yyjson_val *npc_sprite = yyjson_obj_get(npc_value, "sprite");
            yyjson_val *npc_palette = yyjson_obj_get(npc_value, "palette");
            yyjson_val *npc_spawn_x;
            yyjson_val *npc_spawn_y;
            yyjson_val *sprite_width;
            yyjson_val *sprite_height;
            yyjson_val *sprite_origin_x;
            yyjson_val *sprite_origin_y;
            yyjson_val *palette_body;
            yyjson_val *palette_outfit;
            yyjson_val *palette_accent;
            yyjson_val *palette_skin;

            if (!yyjson_is_obj(npc_value) || !yyjson_is_str(npc_id) || !yyjson_is_str(npc_name) ||
                !yyjson_is_str(npc_dialogue) || !yyjson_is_obj(npc_spawn) ||
                !yyjson_is_obj(npc_sprite) || !yyjson_is_obj(npc_palette)) {
                fprintf(stderr, "Map file '%s' has an invalid npc\n", path);
                yyjson_doc_free(doc);
                mojave_map_reset(map);
                return false;
            }

            npc_spawn_x = yyjson_obj_get(npc_spawn, "x");
            npc_spawn_y = yyjson_obj_get(npc_spawn, "y");
            if (!yyjson_is_int(npc_spawn_x) || !yyjson_is_int(npc_spawn_y)) {
                fprintf(stderr, "Map file '%s' has an npc with invalid spawn data\n", path);
                yyjson_doc_free(doc);
                mojave_map_reset(map);
                return false;
            }

            sprite_width = yyjson_obj_get(npc_sprite, "width");
            sprite_height = yyjson_obj_get(npc_sprite, "height");
            sprite_origin_x = yyjson_obj_get(npc_sprite, "origin_x");
            sprite_origin_y = yyjson_obj_get(npc_sprite, "origin_y");
            palette_body = yyjson_obj_get(npc_palette, "body");
            palette_outfit = yyjson_obj_get(npc_palette, "outfit");
            palette_accent = yyjson_obj_get(npc_palette, "accent");
            palette_skin = yyjson_obj_get(npc_palette, "skin");

            if (!yyjson_is_int(sprite_width) || !yyjson_is_int(sprite_height) ||
                !yyjson_is_int(sprite_origin_x) || !yyjson_is_int(sprite_origin_y) ||
                !mojave_json_read_rgb(palette_body, &npc->body_r, &npc->body_g, &npc->body_b) ||
                !mojave_json_read_rgb(palette_outfit, &npc->outfit_r, &npc->outfit_g, &npc->outfit_b) ||
                !mojave_json_read_rgb(palette_accent, &npc->accent_r, &npc->accent_g, &npc->accent_b) ||
                !mojave_json_read_rgb(palette_skin, &npc->skin_r, &npc->skin_g, &npc->skin_b)) {
                fprintf(stderr, "Map file '%s' has invalid npc visual data\n", path);
                yyjson_doc_free(doc);
                mojave_map_reset(map);
                return false;
            }

            npc->id = mojave_strdup(yyjson_get_str(npc_id));
            npc->name = mojave_strdup(yyjson_get_str(npc_name));
            npc->dialogue_path = mojave_strdup(yyjson_get_str(npc_dialogue));
            npc->spawn_x = (int)yyjson_get_int(npc_spawn_x);
            npc->spawn_y = (int)yyjson_get_int(npc_spawn_y);
            npc->sprite_width = (int)yyjson_get_int(sprite_width);
            npc->sprite_height = (int)yyjson_get_int(sprite_height);
            npc->origin_x = (int)yyjson_get_int(sprite_origin_x);
            npc->origin_y = (int)yyjson_get_int(sprite_origin_y);

            if (npc->id == NULL || npc->name == NULL || npc->dialogue_path == NULL) {
                yyjson_doc_free(doc);
                mojave_map_reset(map);
                return false;
            }

            npc_index += 1;
        }
    }

    if (items != NULL) {
        yyjson_arr_iter item_iter;
        yyjson_val *item_value;

        if (!yyjson_is_arr(items)) {
            fprintf(stderr, "Map file '%s' has an invalid item list\n", path);
            yyjson_doc_free(doc);
            mojave_map_reset(map);
            return false;
        }

        map->item_count = (int)yyjson_arr_size(items);
        map->items = calloc((size_t)map->item_count, sizeof(*map->items));
        if (map->items == NULL && map->item_count > 0) {
            yyjson_doc_free(doc);
            mojave_map_reset(map);
            return false;
        }

        item_index = 0;
        yyjson_arr_iter_init(items, &item_iter);
        while ((item_value = yyjson_arr_iter_next(&item_iter)) != NULL) {
            MojaveMapItem *item = &map->items[item_index];
            yyjson_val *item_id = yyjson_obj_get(item_value, "id");
            yyjson_val *spawn = yyjson_obj_get(item_value, "spawn");
            yyjson_val *spawn_x = yyjson_obj_get(spawn, "x");
            yyjson_val *spawn_y = yyjson_obj_get(spawn, "y");

            if (!yyjson_is_obj(item_value) || !yyjson_is_str(item_id) || !yyjson_is_obj(spawn) ||
                !yyjson_is_int(spawn_x) || !yyjson_is_int(spawn_y)) {
                fprintf(stderr, "Map file '%s' has an invalid item\n", path);
                yyjson_doc_free(doc);
                mojave_map_reset(map);
                return false;
            }

            item->item_id = mojave_strdup(yyjson_get_str(item_id));
            item->spawn_x = (int)yyjson_get_int(spawn_x);
            item->spawn_y = (int)yyjson_get_int(spawn_y);
            if (item->item_id == NULL) {
                yyjson_doc_free(doc);
                mojave_map_reset(map);
                return false;
            }

            item_index += 1;
        }
    }

    yyjson_doc_free(doc);
    return true;
}

void mojave_map_unload(MojaveMap *map) {
    mojave_map_reset(map);
}

bool mojave_item_database_load(const char *path, MojaveItemDatabase *item_database) {
    yyjson_doc *doc;
    yyjson_val *root;
    yyjson_val *items;
    yyjson_arr_iter item_iter;
    yyjson_val *item_value;
    int item_index;

    if (path == NULL || item_database == NULL) {
        return false;
    }

    mojave_item_database_reset(item_database);
    if (!mojave_json_read_file(path, &doc)) {
        return false;
    }

    root = yyjson_doc_get_root(doc);
    items = yyjson_obj_get(root, "items");
    if (!yyjson_is_obj(root) || !yyjson_is_arr(items)) {
        fprintf(stderr, "Item file '%s' is missing required fields\n", path);
        yyjson_doc_free(doc);
        return false;
    }

    item_database->item_count = (int)yyjson_arr_size(items);
    item_database->items = calloc((size_t)item_database->item_count, sizeof(*item_database->items));
    if (item_database->items == NULL && item_database->item_count > 0) {
        yyjson_doc_free(doc);
        return false;
    }

    item_index = 0;
    yyjson_arr_iter_init(items, &item_iter);
    while ((item_value = yyjson_arr_iter_next(&item_iter)) != NULL) {
        MojaveItemDefinition *item = &item_database->items[item_index];
        yyjson_val *id = yyjson_obj_get(item_value, "id");
        yyjson_val *name = yyjson_obj_get(item_value, "name");
        yyjson_val *description = yyjson_obj_get(item_value, "description");
        yyjson_val *stackable = yyjson_obj_get(item_value, "stackable");
        yyjson_val *color = yyjson_obj_get(item_value, "color");

        if (!yyjson_is_obj(item_value) || !yyjson_is_str(id) || !yyjson_is_str(name) ||
            !yyjson_is_str(description) || !yyjson_is_bool(stackable) ||
            !mojave_json_read_rgb(color, &item->color_r, &item->color_g, &item->color_b)) {
            fprintf(stderr, "Item file '%s' contains an invalid item\n", path);
            mojave_item_database_reset(item_database);
            yyjson_doc_free(doc);
            return false;
        }

        item->id = mojave_strdup(yyjson_get_str(id));
        item->name = mojave_strdup(yyjson_get_str(name));
        item->description = mojave_strdup(yyjson_get_str(description));
        item->stackable = yyjson_get_bool(stackable);
        if (item->id == NULL || item->name == NULL || item->description == NULL) {
            mojave_item_database_reset(item_database);
            yyjson_doc_free(doc);
            return false;
        }

        item_index += 1;
    }

    yyjson_doc_free(doc);
    return true;
}

void mojave_item_database_unload(MojaveItemDatabase *item_database) {
    mojave_item_database_reset(item_database);
}

const MojaveItemDefinition *mojave_item_database_find(const MojaveItemDatabase *item_database, const char *id) {
    int i;

    if (item_database == NULL || id == NULL) {
        return NULL;
    }

    for (i = 0; i < item_database->item_count; i += 1) {
        if (item_database->items[i].id != NULL && strcmp(item_database->items[i].id, id) == 0) {
            return &item_database->items[i];
        }
    }

    return NULL;
}

bool mojave_quest_log_load(const char *path, MojaveQuestLog *quest_log) {
    yyjson_doc *doc;
    yyjson_val *root;
    yyjson_val *quests;
    yyjson_arr_iter quest_iter;
    yyjson_val *quest_value;
    int quest_index;

    if (path == NULL || quest_log == NULL) {
        return false;
    }

    mojave_quest_log_reset(quest_log);
    if (!mojave_json_read_file(path, &doc)) {
        return false;
    }

    root = yyjson_doc_get_root(doc);
    quests = yyjson_obj_get(root, "quests");
    if (!yyjson_is_obj(root) || !yyjson_is_arr(quests)) {
        fprintf(stderr, "Quest file '%s' is missing required fields\n", path);
        yyjson_doc_free(doc);
        return false;
    }

    quest_log->quest_count = (int)yyjson_arr_size(quests);
    quest_log->quests = calloc((size_t)quest_log->quest_count, sizeof(*quest_log->quests));
    if (quest_log->quests == NULL && quest_log->quest_count > 0) {
        yyjson_doc_free(doc);
        return false;
    }

    quest_index = 0;
    yyjson_arr_iter_init(quests, &quest_iter);
    while ((quest_value = yyjson_arr_iter_next(&quest_iter)) != NULL) {
        MojaveQuestDefinition *quest = &quest_log->quests[quest_index];
        yyjson_val *id = yyjson_obj_get(quest_value, "id");
        yyjson_val *title = yyjson_obj_get(quest_value, "title");
        yyjson_val *stages = yyjson_obj_get(quest_value, "stages");
        yyjson_arr_iter stage_iter;
        yyjson_val *stage_value;
        int stage_index;

        if (!yyjson_is_obj(quest_value) || !yyjson_is_str(id) || !yyjson_is_str(title) || !yyjson_is_arr(stages)) {
            fprintf(stderr, "Quest file '%s' contains an invalid quest\n", path);
            quest_log->quest_count = quest_index;
            mojave_quest_log_reset(quest_log);
            yyjson_doc_free(doc);
            return false;
        }

        quest->id = mojave_strdup(yyjson_get_str(id));
        quest->title = mojave_strdup(yyjson_get_str(title));
        quest->stage_count = (int)yyjson_arr_size(stages);
        quest->stages = calloc((size_t)quest->stage_count, sizeof(*quest->stages));
        if (quest->id == NULL || quest->title == NULL || (quest->stages == NULL && quest->stage_count > 0)) {
            quest_log->quest_count = quest_index;
            mojave_quest_log_reset(quest_log);
            yyjson_doc_free(doc);
            return false;
        }

        stage_index = 0;
        yyjson_arr_iter_init(stages, &stage_iter);
        while ((stage_value = yyjson_arr_iter_next(&stage_iter)) != NULL) {
            if (!yyjson_is_str(stage_value)) {
                quest_log->quest_count = quest_index;
                mojave_quest_log_reset(quest_log);
                yyjson_doc_free(doc);
                return false;
            }
            quest->stages[stage_index] = mojave_strdup(yyjson_get_str(stage_value));
            if (quest->stages[stage_index] == NULL) {
                quest_log->quest_count = quest_index;
                mojave_quest_log_reset(quest_log);
                yyjson_doc_free(doc);
                return false;
            }
            stage_index += 1;
        }

        quest_index += 1;
    }

    yyjson_doc_free(doc);
    return true;
}

void mojave_quest_log_unload(MojaveQuestLog *quest_log) {
    mojave_quest_log_reset(quest_log);
}

const MojaveQuestDefinition *mojave_quest_log_find(const MojaveQuestLog *quest_log, const char *id) {
    int i;

    if (quest_log == NULL || id == NULL) {
        return NULL;
    }

    for (i = 0; i < quest_log->quest_count; i += 1) {
        if (quest_log->quests[i].id != NULL && strcmp(quest_log->quests[i].id, id) == 0) {
            return &quest_log->quests[i];
        }
    }

    return NULL;
}

bool mojave_dialogue_load(const char *path, MojaveDialogue *dialogue) {
    yyjson_doc *doc;
    yyjson_val *root;
    yyjson_val *start;
    yyjson_val *nodes;
    yyjson_arr_iter node_iter;
    yyjson_val *node_value;
    int node_index;

    if (path == NULL || dialogue == NULL) {
        return false;
    }

    mojave_dialogue_reset(dialogue);
    if (!mojave_json_read_file(path, &doc)) {
        return false;
    }

    root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        fprintf(stderr, "Dialogue file '%s' must contain a JSON object\n", path);
        yyjson_doc_free(doc);
        return false;
    }

    start = yyjson_obj_get(root, "start");
    nodes = yyjson_obj_get(root, "nodes");
    if (!yyjson_is_str(start) || !yyjson_is_arr(nodes)) {
        fprintf(stderr, "Dialogue file '%s' is missing required fields\n", path);
        yyjson_doc_free(doc);
        return false;
    }

    dialogue->start_id = mojave_strdup(yyjson_get_str(start));
    if (dialogue->start_id == NULL) {
        yyjson_doc_free(doc);
        return false;
    }

    dialogue->node_count = (int)yyjson_arr_size(nodes);
    dialogue->nodes = calloc((size_t)dialogue->node_count, sizeof(*dialogue->nodes));
    if (dialogue->nodes == NULL && dialogue->node_count > 0) {
        mojave_dialogue_reset(dialogue);
        yyjson_doc_free(doc);
        return false;
    }

    node_index = 0;
    yyjson_arr_iter_init(nodes, &node_iter);
    while ((node_value = yyjson_arr_iter_next(&node_iter)) != NULL) {
        MojaveDialogueNode *node = &dialogue->nodes[node_index];
        yyjson_val *id = yyjson_obj_get(node_value, "id");
        yyjson_val *speaker = yyjson_obj_get(node_value, "speaker");
        yyjson_val *text = yyjson_obj_get(node_value, "text");
        yyjson_val *next = yyjson_obj_get(node_value, "next");
        yyjson_val *is_end = yyjson_obj_get(node_value, "end");
        yyjson_val *conditions = yyjson_obj_get(node_value, "conditions");
        yyjson_val *actions = yyjson_obj_get(node_value, "actions");
        yyjson_val *choices = yyjson_obj_get(node_value, "choices");

        if (!yyjson_is_obj(node_value) || !yyjson_is_str(id) || !yyjson_is_str(speaker) || !yyjson_is_str(text)) {
            fprintf(stderr, "Dialogue file '%s' contains an invalid node\n", path);
            mojave_dialogue_reset(dialogue);
            yyjson_doc_free(doc);
            return false;
        }

        node->id = mojave_strdup(yyjson_get_str(id));
        node->speaker = mojave_strdup(yyjson_get_str(speaker));
        node->text = mojave_strdup(yyjson_get_str(text));
        node->next_id = yyjson_is_str(next) ? mojave_strdup(yyjson_get_str(next)) : NULL;
        node->is_end = yyjson_is_bool(is_end) ? yyjson_get_bool(is_end) : false;

        if (node->id == NULL || node->speaker == NULL || node->text == NULL ||
            !mojave_conditions_load(conditions, &node->conditions, &node->condition_count) ||
            !mojave_event_actions_load(actions, &node->actions, &node->action_count)) {
            dialogue->node_count = node_index;
            mojave_dialogue_reset(dialogue);
            yyjson_doc_free(doc);
            return false;
        }

        if (choices != NULL) {
            yyjson_arr_iter choice_iter;
            yyjson_val *choice_value;
            int choice_index;

            if (!yyjson_is_arr(choices)) {
                fprintf(stderr, "Dialogue file '%s' has an invalid choices array\n", path);
                mojave_dialogue_reset(dialogue);
                yyjson_doc_free(doc);
                return false;
            }

            node->choice_count = (int)yyjson_arr_size(choices);
            node->choices = calloc((size_t)node->choice_count, sizeof(*node->choices));
            if (node->choices == NULL && node->choice_count > 0) {
                mojave_dialogue_reset(dialogue);
                yyjson_doc_free(doc);
                return false;
            }

            choice_index = 0;
            yyjson_arr_iter_init(choices, &choice_iter);
            while ((choice_value = yyjson_arr_iter_next(&choice_iter)) != NULL) {
                yyjson_val *choice_text = yyjson_obj_get(choice_value, "text");
                yyjson_val *choice_next = yyjson_obj_get(choice_value, "next");
                yyjson_val *choice_conditions = yyjson_obj_get(choice_value, "conditions");
                yyjson_val *choice_actions = yyjson_obj_get(choice_value, "actions");

                if (!yyjson_is_obj(choice_value) || !yyjson_is_str(choice_text) || !yyjson_is_str(choice_next)) {
                    fprintf(stderr, "Dialogue file '%s' has an invalid choice\n", path);
                    mojave_dialogue_reset(dialogue);
                    yyjson_doc_free(doc);
                    return false;
                }

                node->choices[choice_index].text = mojave_strdup(yyjson_get_str(choice_text));
                node->choices[choice_index].next_id = mojave_strdup(yyjson_get_str(choice_next));
                if (node->choices[choice_index].text == NULL || node->choices[choice_index].next_id == NULL ||
                    !mojave_conditions_load(
                        choice_conditions,
                        &node->choices[choice_index].conditions,
                        &node->choices[choice_index].condition_count) ||
                    !mojave_event_actions_load(
                        choice_actions,
                        &node->choices[choice_index].actions,
                        &node->choices[choice_index].action_count)) {
                    dialogue->node_count = node_index;
                    mojave_dialogue_reset(dialogue);
                    yyjson_doc_free(doc);
                    return false;
                }

                choice_index += 1;
            }
        }

        node_index += 1;
    }

    yyjson_doc_free(doc);
    return true;
}

void mojave_dialogue_unload(MojaveDialogue *dialogue) {
    mojave_dialogue_reset(dialogue);
}

const MojaveDialogueNode *mojave_dialogue_find_node(const MojaveDialogue *dialogue, const char *id) {
    int i;

    if (dialogue == NULL || id == NULL) {
        return NULL;
    }

    for (i = 0; i < dialogue->node_count; i += 1) {
        if (dialogue->nodes[i].id != NULL && strcmp(dialogue->nodes[i].id, id) == 0) {
            return &dialogue->nodes[i];
        }
    }

    return NULL;
}

bool mojave_save_write_player(const char *path, float x, float y) {
    yyjson_mut_doc *doc;
    yyjson_mut_val *root;
    char *json;
    size_t length;
    FILE *file;

    if (path == NULL) {
        return false;
    }

    doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) {
        return false;
    }

    root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_real(doc, root, "player_x", x);
    yyjson_mut_obj_add_real(doc, root, "player_y", y);

    json = yyjson_mut_write(doc, YYJSON_WRITE_PRETTY, &length);
    yyjson_mut_doc_free(doc);
    if (json == NULL) {
        return false;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "Failed to open save file '%s' for writing\n", path);
        free(json);
        return false;
    }

    if (fwrite(json, 1, length, file) != length) {
        fprintf(stderr, "Failed to write save file '%s'\n", path);
        fclose(file);
        free(json);
        return false;
    }

    fclose(file);
    free(json);
    return true;
}

bool mojave_save_read_player(const char *path, float *x, float *y) {
    yyjson_doc *doc;
    yyjson_val *root;

    if (path == NULL || x == NULL || y == NULL) {
        return false;
    }

    if (!mojave_file_exists(path)) {
        return false;
    }

    if (!mojave_json_read_file(path, &doc)) {
        return false;
    }

    root = yyjson_doc_get_root(doc);
    if (!yyjson_is_obj(root)) {
        yyjson_doc_free(doc);
        return false;
    }

    *x = (float)yyjson_get_real(yyjson_obj_get(root, "player_x"));
    *y = (float)yyjson_get_real(yyjson_obj_get(root, "player_y"));

    yyjson_doc_free(doc);
    return true;
}
