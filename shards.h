#ifndef SHARDS_H
#define SHARDS_H

#include <stdint.h>
#include <stdlib.h>
#include "raylib.h"
#include "raymath.h"
#include "utils.h"

typedef struct Shard {
    float life;
    float radius;
    Vector2 position;
    Vector2 velocity;
    Vector2 poly_points[7];
    float angles[7];
    uint8_t count;
    Color color;
} Shard;

Vector2 shard_get_point(float angle, Vector2 e_radius) {
    float theta = angle * DEG2RAD;

    float x = e_radius.x * cosf(theta);
    float y = e_radius.y * sinf(theta);

    return vec2(x, y);
}

int compare_floats(const void *a, const void *b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;

    if (fa < fb) return -1;
    if (fa > fb) return  1;
    return 0;   // equal
}

void shards_set_angles(Shard* shard) {
    int n = 3+ rand() % 5; // at most  7 angles

    for (uint8_t i = 0; i < n; i++) {
        float r = (float)rand() / (float)RAND_MAX;
        shard->angles[i] = r * 355.23;
    }

    shard->count = n;
    qsort(shard->angles, shard->count, sizeof(float), compare_floats);
}

void shards_get_polygon(
    Vector2 e_radius,
    Shard* shard
) {
    qsort(shard->angles, shard->count, sizeof(float), compare_floats);

    for (uint8_t i = 0; i < shard->count; i++) {
        Vector2 pt = shard_get_point(shard->angles[i], e_radius);
        shard->poly_points[i] = Vector2Add(pt, shard->position);
    }
}

void shard_update(Shard* shard) {
    shard->life -= 0.0167 * 2;

    if (shard->life <= 0) {
        return;
    }

    shard->position = Vector2Add(shard->position, shard->velocity);

    for (uint8_t i = 0; i < shard->count; i++) {
        shard->angles[i] += 10.0;
    }

    Vector2 e_radius = vec2(
        shard->radius * 1.5,
        shard->radius
    );

    shards_get_polygon(e_radius, shard);
}

void shard_draw(const Shard* shard) {
    if (shard->life <= 0) {
        return;
    }

    float alpha = shard->life / 2.0;

    Vector2 p0 = shard->poly_points[0];

    uint8_t count = shard->count;

    for (uint8_t i=1; i<count-1; i++) {
        Vector2 p1 = shard->poly_points[i%count];
        Vector2 p2 = shard->poly_points[i+1];

        DrawTriangle(
            p2,
            p1,
            p0,
            ColorAlpha(shard->color, alpha)
        );
    }
}

int compare_shards(const void *a, const void *b) {
  Shard *o1 = ((Shard *)a);
  Shard *o2 = ((Shard *)b);
  if (o1->life <= 0)
    return 1;
  if (o2->life <= 0)
    return -1;
  return 0;
}

#endif