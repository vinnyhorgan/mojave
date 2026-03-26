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
    free(map->npcs);
    free(map->tiles);
    memset(map, 0, sizeof(*map));
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
    for (i = 0; i < node->choice_count; i += 1) {
        free(node->choices[i].text);
        free(node->choices[i].next_id);
    }
    free(node->choices);
    memset(node, 0, sizeof(*node));
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

    if (!yyjson_is_obj(value)) {
        return false;
    }

    r = yyjson_obj_get(value, "r");
    g = yyjson_obj_get(value, "g");
    b = yyjson_obj_get(value, "b");
    if (!yyjson_is_int(r) || !yyjson_is_int(g) || !yyjson_is_int(b)) {
        return false;
    }

    *out_r = (unsigned char)yyjson_get_int(r);
    *out_g = (unsigned char)yyjson_get_int(g);
    *out_b = (unsigned char)yyjson_get_int(b);
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

    yyjson_doc_free(doc);
    return true;
}

void mojave_map_unload(MojaveMap *map) {
    mojave_map_reset(map);
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

        if (node->id == NULL || node->speaker == NULL || node->text == NULL) {
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

                if (!yyjson_is_obj(choice_value) || !yyjson_is_str(choice_text) || !yyjson_is_str(choice_next)) {
                    fprintf(stderr, "Dialogue file '%s' has an invalid choice\n", path);
                    mojave_dialogue_reset(dialogue);
                    yyjson_doc_free(doc);
                    return false;
                }

                node->choices[choice_index].text = mojave_strdup(yyjson_get_str(choice_text));
                node->choices[choice_index].next_id = mojave_strdup(yyjson_get_str(choice_next));
                if (node->choices[choice_index].text == NULL || node->choices[choice_index].next_id == NULL) {
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
