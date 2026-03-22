#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "raylib.h"

#define vec2(xx,yy) ((Vector2) {.x=xx, .y=yy})
#define rect(xx,yy,w,h) ((Rectangle) {.x=xx, .y=yy, .width=w, .height=h})

#define DefineSizedContainer(T, NAME) \
typedef struct NAME { \
    uint32_t count; \
    uint32_t capacity; \
    T* members; \
} NAME

typedef struct SizedContainer {
    uint32_t count;
    uint32_t capacity;
} SizedContainer;

// TODO: add downsizing to dynamic containers
void* resize(void* container_ptr, void* objects, size_t object_size) {
    SizedContainer* container = (SizedContainer*) container_ptr;
    if (container->count >= container->capacity) {
        if (container->capacity == 0) {
            container->capacity = 256;
        } else {
            container->capacity *= 1.5;
        }
        return realloc(objects, container->capacity * object_size);
    }
    return objects;
}

// returns a random float between [0, 1]
static inline float get_random_float() {
    float r = (float)rand() / (float)RAND_MAX;
    return r;
}

Texture2D loadTextureFromImageResized(const char* filename, int newWidth, int newHeight) {
    char path[256];
    sprintf(path, "./assets/%s", filename);
    Image image = LoadImage(path);
    ImageResize(&image, newWidth, newHeight);
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    return texture;
}

Texture2D loadTextureFromImageFlip(const char* filename) {
    char path[256];
    sprintf(path, "./assets/%s", filename);
    Image image = LoadImage(path);
    ImageFlipHorizontal(&image);
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    return texture;
}

#endif