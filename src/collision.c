#include "collision.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static const float MOJAVE_COLLISION_DELTA = 1e-10f;

typedef struct MojaveCollisionHit {
    bool overlaps;
    float ti;
    float move_x;
    float move_y;
    float normal_x;
    float normal_y;
    float touch_x;
    float touch_y;
    MojaveRect other_rect;
    int tile_index;
} MojaveCollisionHit;

static float mojave_absf(float value) {
    return value < 0.0f ? -value : value;
}

static float mojave_nearest(float value, float a, float b) {
    return mojave_absf(a - value) < mojave_absf(b - value) ? a : b;
}

static int mojave_signf(float value) {
    if (value > 0.0f) {
        return 1;
    }
    if (value < 0.0f) {
        return -1;
    }
    return 0;
}

static bool mojave_rect_contains_point(const MojaveRect *rect, float px, float py) {
    return px - rect->x > MOJAVE_COLLISION_DELTA &&
        py - rect->y > MOJAVE_COLLISION_DELTA &&
        rect->x + rect->w - px > MOJAVE_COLLISION_DELTA &&
        rect->y + rect->h - py > MOJAVE_COLLISION_DELTA;
}

static float mojave_rect_square_distance(const MojaveRect *a, const MojaveRect *b) {
    float dx = a->x - b->x + (a->w - b->w) * 0.5f;
    float dy = a->y - b->y + (a->h - b->h) * 0.5f;

    return dx * dx + dy * dy;
}

static void mojave_rect_get_nearest_corner(const MojaveRect *rect, float px, float py, float *out_x, float *out_y) {
    *out_x = mojave_nearest(px, rect->x, rect->x + rect->w);
    *out_y = mojave_nearest(py, rect->y, rect->y + rect->h);
}

static bool mojave_rect_get_segment_intersection_indices(const MojaveRect *rect,
    float x1,
    float y1,
    float x2,
    float y2,
    float *io_ti1,
    float *io_ti2,
    float *out_nx1,
    float *out_ny1,
    float *out_nx2,
    float *out_ny2) {
    float ti1 = *io_ti1;
    float ti2 = *io_ti2;
    float dx = x2 - x1;
    float dy = y2 - y1;
    float nx1 = 0.0f;
    float ny1 = 0.0f;
    float nx2 = 0.0f;
    float ny2 = 0.0f;
    int side;

    for (side = 0; side < 4; side += 1) {
        float nx;
        float ny;
        float p;
        float q;
        float r;

        if (side == 0) {
            nx = -1.0f;
            ny = 0.0f;
            p = -dx;
            q = x1 - rect->x;
        } else if (side == 1) {
            nx = 1.0f;
            ny = 0.0f;
            p = dx;
            q = rect->x + rect->w - x1;
        } else if (side == 2) {
            nx = 0.0f;
            ny = -1.0f;
            p = -dy;
            q = y1 - rect->y;
        } else {
            nx = 0.0f;
            ny = 1.0f;
            p = dy;
            q = rect->y + rect->h - y1;
        }

        if (p == 0.0f) {
            if (q <= 0.0f) {
                return false;
            }
        } else {
            r = q / p;
            if (p < 0.0f) {
                if (r > ti2) {
                    return false;
                }
                if (r > ti1) {
                    ti1 = r;
                    nx1 = nx;
                    ny1 = ny;
                }
            } else {
                if (r < ti1) {
                    return false;
                }
                if (r < ti2) {
                    ti2 = r;
                    nx2 = nx;
                    ny2 = ny;
                }
            }
        }
    }

    *io_ti1 = ti1;
    *io_ti2 = ti2;
    *out_nx1 = nx1;
    *out_ny1 = ny1;
    *out_nx2 = nx2;
    *out_ny2 = ny2;
    return true;
}

static bool mojave_rect_detect_collision(const MojaveRect *item_rect,
    const MojaveRect *other_rect,
    float goal_x,
    float goal_y,
    MojaveCollisionHit *hit) {
    float dx = goal_x - item_rect->x;
    float dy = goal_y - item_rect->y;
    MojaveRect diff_rect = {
        other_rect->x - item_rect->x - item_rect->w,
        other_rect->y - item_rect->y - item_rect->h,
        item_rect->w + other_rect->w,
        item_rect->h + other_rect->h,
    };
    bool overlaps = false;
    float ti = 0.0f;
    float nx = 0.0f;
    float ny = 0.0f;
    float tx;
    float ty;

    if (mojave_rect_contains_point(&diff_rect, 0.0f, 0.0f)) {
        float px;
        float py;
        float wi;
        float hi;

        mojave_rect_get_nearest_corner(&diff_rect, 0.0f, 0.0f, &px, &py);
        wi = fminf(item_rect->w, mojave_absf(px));
        hi = fminf(item_rect->h, mojave_absf(py));
        ti = -wi * hi;
        overlaps = true;
    } else {
        float ti1 = -INFINITY;
        float ti2 = INFINITY;
        float nx1 = 0.0f;
        float ny1 = 0.0f;
        float nx2 = 0.0f;
        float ny2 = 0.0f;

        if (mojave_rect_get_segment_intersection_indices(
                &diff_rect,
                0.0f,
                0.0f,
                dx,
                dy,
                &ti1,
                &ti2,
                &nx1,
                &ny1,
                &nx2,
                &ny2) &&
            ti1 < 1.0f &&
            mojave_absf(ti1 - ti2) >= MOJAVE_COLLISION_DELTA &&
            ((0.0f < ti1 + MOJAVE_COLLISION_DELTA) || (ti1 == 0.0f && ti2 > 0.0f))) {
            ti = ti1;
            nx = nx1;
            ny = ny1;
        } else {
            return false;
        }
    }

    if (overlaps) {
        if (dx == 0.0f && dy == 0.0f) {
            float px;
            float py;

            mojave_rect_get_nearest_corner(&diff_rect, 0.0f, 0.0f, &px, &py);
            if (mojave_absf(px) < mojave_absf(py)) {
                py = 0.0f;
            } else {
                px = 0.0f;
            }
            nx = (float)mojave_signf(px);
            ny = (float)mojave_signf(py);
            tx = item_rect->x + px;
            ty = item_rect->y + py;
        } else {
            float ti1 = -INFINITY;
            float ti2 = 1.0f;
            float nx1 = 0.0f;
            float ny1 = 0.0f;
            float nx2 = 0.0f;
            float ny2 = 0.0f;

            if (!mojave_rect_get_segment_intersection_indices(
                    &diff_rect,
                    0.0f,
                    0.0f,
                    dx,
                    dy,
                    &ti1,
                    &ti2,
                    &nx1,
                    &ny1,
                    &nx2,
                    &ny2)) {
                return false;
            }
            nx = nx1;
            ny = ny1;
            tx = item_rect->x + dx * ti1;
            ty = item_rect->y + dy * ti1;
        }
    } else {
        tx = item_rect->x + dx * ti;
        ty = item_rect->y + dy * ti;
    }

    hit->overlaps = overlaps;
    hit->ti = ti;
    hit->move_x = dx;
    hit->move_y = dy;
    hit->normal_x = nx;
    hit->normal_y = ny;
    hit->touch_x = tx;
    hit->touch_y = ty;
    hit->other_rect = *other_rect;
    return true;
}

static bool mojave_map_tile_is_solid(const MojaveMap *map, int x, int y) {
    if (x < 0 || y < 0 || x >= map->width || y >= map->height) {
        return true;
    }

    return map->tiles[y * map->width + x] != 0;
}

static bool mojave_collision_is_better(const MojaveCollisionHit *candidate,
    const MojaveCollisionHit *best,
    const MojaveRect *item_rect) {
    if (candidate->ti != best->ti) {
        return candidate->ti < best->ti;
    }

    return mojave_rect_square_distance(item_rect, &candidate->other_rect) <
        mojave_rect_square_distance(item_rect, &best->other_rect);
}

static bool mojave_collision_project(const MojaveMap *map,
    const MojaveRect *item_rect,
    float goal_x,
    float goal_y,
    const bool *visited_tiles,
    MojaveCollisionHit *hit) {
    float left = fminf(item_rect->x, goal_x);
    float top = fminf(item_rect->y, goal_y);
    float right = fmaxf(item_rect->x + item_rect->w, goal_x + item_rect->w);
    float bottom = fmaxf(item_rect->y + item_rect->h, goal_y + item_rect->h);
    int tile_left = (int)floorf(left / (float)map->tile_size);
    int tile_top = (int)floorf(top / (float)map->tile_size);
    int tile_right = (int)floorf((right - 1.0f) / (float)map->tile_size);
    int tile_bottom = (int)floorf((bottom - 1.0f) / (float)map->tile_size);
    bool found = false;
    int tile_y;

    for (tile_y = tile_top; tile_y <= tile_bottom; tile_y += 1) {
        int tile_x;

        for (tile_x = tile_left; tile_x <= tile_right; tile_x += 1) {
            MojaveRect other_rect;
            MojaveCollisionHit candidate;
            int tile_index;

            if (!mojave_map_tile_is_solid(map, tile_x, tile_y)) {
                continue;
            }

            if (tile_x < 0 || tile_y < 0 || tile_x >= map->width || tile_y >= map->height) {
                continue;
            }

            tile_index = tile_y * map->width + tile_x;
            if (visited_tiles[tile_index]) {
                continue;
            }

            other_rect.x = (float)(tile_x * map->tile_size);
            other_rect.y = (float)(tile_y * map->tile_size);
            other_rect.w = (float)map->tile_size;
            other_rect.h = (float)map->tile_size;

            if (!mojave_rect_detect_collision(item_rect, &other_rect, goal_x, goal_y, &candidate)) {
                continue;
            }

            candidate.tile_index = tile_index;
            if (!found || mojave_collision_is_better(&candidate, hit, item_rect)) {
                *hit = candidate;
                found = true;
            }
        }
    }

    return found;
}

bool mojave_collision_move_rect(const MojaveMap *map,
    const MojaveRect *rect,
    float goal_x,
    float goal_y,
    MojaveCollisionMoveResult *result) {
    MojaveRect current_rect;
    bool *visited_tiles;
    bool collided = false;
    int tile_count;

    if (map == NULL || rect == NULL || result == NULL || map->tiles == NULL) {
        return false;
    }

    if (goal_x < 0.0f) {
        goal_x = 0.0f;
    }
    if (goal_y < 0.0f) {
        goal_y = 0.0f;
    }
    if (goal_x > (float)(map->width * map->tile_size) - rect->w) {
        goal_x = (float)(map->width * map->tile_size) - rect->w;
    }
    if (goal_y > (float)(map->height * map->tile_size) - rect->h) {
        goal_y = (float)(map->height * map->tile_size) - rect->h;
    }

    tile_count = map->width * map->height;
    visited_tiles = calloc((size_t)tile_count, sizeof(*visited_tiles));
    if (visited_tiles == NULL) {
        return false;
    }

    current_rect = *rect;
    while (true) {
        MojaveCollisionHit hit;

        if (!mojave_collision_project(map, &current_rect, goal_x, goal_y, visited_tiles, &hit)) {
            break;
        }

        collided = true;
        visited_tiles[hit.tile_index] = true;

        if (hit.move_x != 0.0f || hit.move_y != 0.0f) {
            if (hit.normal_x != 0.0f) {
                goal_x = hit.touch_x;
            } else {
                goal_y = hit.touch_y;
            }
        }

        current_rect.x = hit.touch_x;
        current_rect.y = hit.touch_y;
    }

    free(visited_tiles);
    result->x = goal_x;
    result->y = goal_y;
    result->collided = collided;
    return true;
}
