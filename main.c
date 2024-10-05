#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "raylib.h"

#define GRID_SIZE 25

#define TILE_WIDTH 64
#define TILE_HEIGHT 32

enum GeneralObjectType {
    ENEMY_TYPE_1,
    ENEMY_TYPE_2,
    DEFENDER_TYPE_1,
    DEFENDER_TYPE_2,
    PROJECTILE_TYPE_1,
};

enum GameObjectType {
    ENEMY,
    DEFENSE,
    PROJECTILE,
};

typedef struct Enemy {
    int enemy; // placeholder
} Enemy;

typedef struct Defense {
    double last_attacked;
} Defense;

typedef union GameObjectValue {
    Enemy enemy;
    Defense defense;
};

typedef struct GameObject {
    union GameObjectValue game_object;
    Vector2 position;
    enum GameObjectType type;
    enum GeneralObjectType sub_type;
    int is_active;
} GameObject;

typedef struct GameObjects {
    GameObject* objects;
    int count;
    int capacity;
} GameObjects;

typedef struct GameState {
    Vector2 mouse_position;
    GameObjects game_objects;

} GameState;

void resize(GameObjects* container) {
    if (container->count >= container->capacity) {
        if (container->capacity == 0) {
            container->capacity = 256;
        } else {
            container->capacity *= 2;
        }
        container->objects = realloc(container->objects, container->capacity * sizeof(GameObject));
    }
}

/* global variables start */
int screen_width;
int screen_height;
Texture2D ground_texture;
Texture2D mouseover_texture;
Texture2D GAME_OBJECT_TEXTURES[10];
/* global variables end */

int compareGameObjects(const void* a, const void* b) {
    GameObject* o1 = ( (GameObject*) a );
    GameObject* o2 = ( (GameObject*) b );

    if (!o1->is_active) return 1;
    if (!o2->is_active) return -1;

    Vector2 p1 = o1->position;
    Vector2 p2 = o2->position;

    if (p1.y < p2.y) {
        return -1;
    } else if (p1.y > p2.y) {
        return 1;
    } 
    
    return p1.x - p2.x;
}

void addEnemy(Vector2 position, enum GeneralObjectType type, GameState* game_state) {
    resize(&game_state->game_objects);

    GameObject* game_object = &(game_state->game_objects.objects[game_state->game_objects.count]); 
    game_object->type = ENEMY;

    game_object->position = position;
    game_object->sub_type = type;
    game_object->is_active = 1;

    game_state->game_objects.objects[game_state->game_objects.count++] = *game_object;
}

void addDefense(Vector2 position, enum GeneralObjectType type, GameState* game_state) {
    resize(&game_state->game_objects);

    GameObject* game_object = &(game_state->game_objects.objects[game_state->game_objects.count]); 
    game_object->type = DEFENSE;

    game_object->game_object.defense.last_attacked = GetTime();
    game_object->position = position;
    game_object->sub_type = type;
    game_object->is_active = 1;
    
    game_state->game_objects.objects[game_state->game_objects.count++] = *game_object;
}

void addProjectile(float x, float y, enum GeneralObjectType type, GameState* game_state) {
    resize(&game_state->game_objects);

    GameObject* game_object = &(game_state->game_objects.objects[game_state->game_objects.count]); 
    game_object->type = PROJECTILE;

    Vector2 position = {.x=x, .y=y};
    game_object->position = position;
    game_object->sub_type = type;
    game_object->is_active = 1;

    game_state->game_objects.objects[game_state->game_objects.count++] = *game_object;
}

Vector2 fromIso(Vector2 screen) {
    Vector2 result = {};

    screen.x -= screen_width / 2;
    screen.y -= 100;

    // So final actual commands are:
    result.x = (screen.x / (TILE_WIDTH / 2) + screen.y / (TILE_HEIGHT / 2)) / 2;
    result.y = (screen.y / (TILE_HEIGHT / 2) -(screen.x / (TILE_WIDTH / 2))) / 2;

    result.x = floor(result.x);
    result.y = floor(result.y);

    return result;
}

Vector2 toIso(Vector2 coord) {
    Vector2 result = {};

    // calculate screen coordinates
    result.x = (coord.x - coord.y) * (TILE_WIDTH / 2);
    result.y = (coord.x + coord.y) * (TILE_HEIGHT / 2);

    // some translation
    result.x -= TILE_WIDTH / 2;
    result.x += screen_width / 2;
    result.y += 100;

    return result;
}

void grab_user_input(GameState* game_state) {
    game_state->mouse_position = fromIso(GetMousePosition());

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        int mpx = game_state->mouse_position.x;
        int mpy = game_state->mouse_position.y;
        if (mpx >= 0 && mpx < GRID_SIZE && mpy >= 0 && mpy < GRID_SIZE) {
            addDefense(game_state->mouse_position, DEFENDER_TYPE_1, game_state);
        }
    }
}

void update(GameState* game_state) {
    float delta_time = GetFrameTime();
    // update enemy
    int count = game_state -> game_objects.count;
    int remove_count = 0;
    for (int e = 0; e < count; e++){
        enum GameObjectType object_type = game_state->game_objects.objects[e].type;

        if (object_type == ENEMY) {
            float speed = 0.0;

            if (game_state->game_objects.objects[e].sub_type == ENEMY_TYPE_1) {speed = 0.125;}
            else if (game_state->game_objects.objects[e].sub_type == ENEMY_TYPE_2) {speed = 0.25;}
            
            game_state->game_objects.objects[e].position.x += speed * delta_time; 
        } else if (object_type == DEFENSE) {
            // TODO: projectile generation should be based on charging a certain bar which would be higher/lower depending on the effectiveness of the projectile
            double last_attacked = (game_state->game_objects.objects[e].game_object.defense).last_attacked;
            double time_passed = GetTime() - last_attacked;

            if (time_passed < 4.0) { continue; }

            Vector2 p = game_state->game_objects.objects[e].position;
            addProjectile(p.x-1, p.y, PROJECTILE_TYPE_1, game_state);
            game_state->game_objects.objects[e].game_object.defense.last_attacked = GetTime();
        } else if (object_type == PROJECTILE) {
            // TODO: projectiles will move with different speeds
            game_state->game_objects.objects[e].position.x -= 2 * delta_time;

            if (game_state->game_objects.objects[e].position.x < 0) {
                game_state->game_objects.objects[e].is_active = 0;
                remove_count++;
            }
        }
    }

    // TODO: only do this if a new projectile is added
    qsort(game_state->game_objects.objects, game_state->game_objects.count, sizeof(GameObject), compareGameObjects);
    game_state->game_objects.count -= remove_count;
}

void draw(GameState* game_state) {
    for (int y = 0; y < GRID_SIZE; y++){
        for (int x = 0; x < GRID_SIZE; x++){
            Vector2 grid_coords = {.x = x, .y = y};
            Vector2 iso_coords = toIso(grid_coords);
            Vector2 mouse_coords = game_state->mouse_position;

            if ((int) mouse_coords.y == y) {
                if ((int) mouse_coords.x == x) {
                    DrawTextureV(mouseover_texture, iso_coords, WHITE);
                } else {
                    DrawTextureV(ground_texture, iso_coords, WHITE);
                }
                DrawTextureV(GAME_OBJECT_TEXTURES[5], iso_coords, WHITE);
            } else {
                DrawTextureV(ground_texture, iso_coords, WHITE);
            }

        }
    }

    for (int e = 0; e < game_state->game_objects.count; e++) {
        Vector2 iso_coords = toIso(game_state->game_objects.objects[e].position);
        iso_coords.y -= TILE_HEIGHT;
        Texture2D texture = GAME_OBJECT_TEXTURES[game_state->game_objects.objects[e].sub_type];
        DrawTextureV(texture, iso_coords, WHITE);
        if (game_state->game_objects.objects[e].type == DEFENSE) {
            float diff = GetTime() - game_state->game_objects.objects[e].game_object.defense.last_attacked;
            float pct = diff / 4.0;
            BeginScissorMode((int) iso_coords.x, (int) ceil(iso_coords.y + 2 * TILE_HEIGHT * (1 - pct)), TILE_WIDTH, 2 * TILE_HEIGHT * pct);
                DrawTextureV(GAME_OBJECT_TEXTURES[5], iso_coords, WHITE);
            EndScissorMode();
        }
    }
}

int main(void){
    GameState game_state = {};
    GameObjects objs = {0};
    game_state.game_objects = objs;

    SetConfigFlags(FLAG_VSYNC_HINT);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    
    InitWindow(0, 0, "blockwave");
    SetTargetFPS(60);

    int monitor = GetCurrentMonitor();
    screen_width = GetMonitorWidth(monitor);
    screen_height = GetMonitorHeight(monitor);
    SetWindowSize(screen_width, GetMonitorHeight(monitor));

    Image block_1 = LoadImage("./assets/Isometric_Tiles_Pixel_Art/Blocks/blocks_1.png");
    ground_texture = LoadTextureFromImage(block_1);
    UnloadImage(block_1);

    Image block_99 = LoadImage("./assets/Isometric_Tiles_Pixel_Art/Blocks/blocks_99.png");
    mouseover_texture = LoadTextureFromImage(block_99);
    UnloadImage(block_99);

    Image block_34 = LoadImage("./assets/Isometric_Tiles_Pixel_Art/Blocks/blocks_34.png");
    Texture2D enemy_type_1_texture = LoadTextureFromImage(block_34);
    UnloadImage(block_34);

    Image block_14 = LoadImage("./assets/Isometric_Tiles_Pixel_Art/Blocks/blocks_14.png");
    Texture2D enemy_type_2_texture = LoadTextureFromImage(block_14);
    UnloadImage(block_14);

    Image block_30 = LoadImage("./assets/Isometric_Tiles_Pixel_Art/Blocks/blocks_30.png");
    Texture2D defender_type_1_texture = LoadTextureFromImage(block_30);
    UnloadImage(block_30);

    Image block_31 = LoadImage("./assets/Isometric_Tiles_Pixel_Art/Blocks/blocks_31.png");
    Texture2D defender_type_2_texture = LoadTextureFromImage(block_31);
    UnloadImage(block_31);

    Image block_12 = LoadImage("./assets/Isometric_Tiles_Pixel_Art/Blocks/blocks_12.png");
    ImageResize(&block_12, TILE_WIDTH / 2, TILE_WIDTH / 2);
    Texture2D projectile_1_texture = LoadTextureFromImage(block_12);
    UnloadImage(block_12);

    Image white_overlay = LoadImage("./assets/Isometric_Tiles_Pixel_Art/Blocks/overlay.png");
    Texture2D white_overlay_texture = LoadTextureFromImage(white_overlay);
    UnloadImage(white_overlay);

    GAME_OBJECT_TEXTURES[ENEMY_TYPE_1] = enemy_type_1_texture;
    GAME_OBJECT_TEXTURES[ENEMY_TYPE_2] = enemy_type_2_texture;
    GAME_OBJECT_TEXTURES[DEFENDER_TYPE_1] = defender_type_1_texture;
    GAME_OBJECT_TEXTURES[DEFENDER_TYPE_2] = defender_type_2_texture;
    GAME_OBJECT_TEXTURES[PROJECTILE_TYPE_1] = projectile_1_texture;
    GAME_OBJECT_TEXTURES[5] = white_overlay_texture;

    Vector2 p1 = {.x = 0, .y = 9}; addEnemy(p1, ENEMY_TYPE_1, &game_state);
    Vector2 p2 = {.x = 0, .y = 13}; addEnemy(p2, ENEMY_TYPE_2, &game_state);
    Vector2 p3 = {.x = 0, .y = 18}; addEnemy(p3, ENEMY_TYPE_2, &game_state);

    while (!WindowShouldClose())
    {
        grab_user_input(&game_state);
        update(&game_state);

        BeginDrawing();
        ClearBackground(RAYWHITE);

        draw(&game_state);

        char text[255];
        sprintf(text, "fps: %d\ncount: %d\ncap: %d", GetFPS(), game_state.game_objects.count, game_state.game_objects.capacity);

        DrawText(text, 10, 0, 60, BLACK);

        EndDrawing();
    }

    {
        // free
        free(game_state.game_objects.objects);

        UnloadTexture(ground_texture);
        UnloadTexture(mouseover_texture);
        UnloadTexture(enemy_type_1_texture);
        UnloadTexture(enemy_type_2_texture);
        UnloadTexture(defender_type_1_texture);
        UnloadTexture(defender_type_2_texture);
        UnloadTexture(projectile_1_texture);
        UnloadTexture(white_overlay_texture);
    }


    CloseWindow();

    return 0;
}