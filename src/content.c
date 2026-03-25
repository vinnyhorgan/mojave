#include "content.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>

#include <yyjson.h>

static void mojave_map_reset(MojaveMap *map) {
    if (map == NULL) {
        return;
    }

    free(map->tiles);
    memset(map, 0, sizeof(*map));
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
    yyjson_val *tiles;
    yyjson_val *spawn;
    yyjson_arr_iter iter;
    yyjson_val *tile;
    size_t index;
    size_t expected_count;

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
    spawn = yyjson_obj_get(root, "player_spawn");

    map->width = (int)yyjson_get_int(yyjson_obj_get(root, "width"));
    map->height = (int)yyjson_get_int(yyjson_obj_get(root, "height"));
    map->tile_size = (int)yyjson_get_int(yyjson_obj_get(root, "tile_size"));
    map->player_spawn_x = (int)yyjson_get_int(yyjson_obj_get(spawn, "x"));
    map->player_spawn_y = (int)yyjson_get_int(yyjson_obj_get(spawn, "y"));

    if (map->width <= 0 || map->height <= 0 || map->tile_size <= 0) {
        fprintf(stderr, "Map file '%s' has invalid dimensions\n", path);
        yyjson_doc_free(doc);
        return false;
    }

    if (!yyjson_is_arr(tiles) || !yyjson_is_obj(spawn)) {
        fprintf(stderr, "Map file '%s' is missing required fields\n", path);
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
        map->tiles[index] = (int)yyjson_get_int(tile);
        index += 1;
    }

    snprintf(map->name, sizeof(map->name), "%s",
        yyjson_get_str(yyjson_obj_get(root, "name")) != NULL
            ? yyjson_get_str(yyjson_obj_get(root, "name"))
            : "Unnamed Map");

    yyjson_doc_free(doc);
    return true;
}

void mojave_map_unload(MojaveMap *map) {
    mojave_map_reset(map);
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

    fwrite(json, 1, length, file);
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
