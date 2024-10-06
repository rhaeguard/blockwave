#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "raylib.h"
#include "raymath.h"

#define GRID_SIZE 25

#define TILE_WIDTH 64
#define TILE_HEIGHT 32
#define VERTICAL_OFFSET 100.0

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
    Vector2 start;
    Vector2 target;
    Vector2 current_iso_coord;
    float move_pct; // progress till dest
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

Vector2 vec2(float x, float y) {
    return (Vector2) {.x=x, .y=y};
}

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
Texture2D white_full_overlay_texture;
Texture2D white_half_overlay_texture;
Texture2D GAME_OBJECT_TEXTURES[10];
/* global variables end */

Vector2 toIso(Vector2 coord, bool translate_by_half_width) {
    // calculate screen coordinates
    float x = (coord.x - coord.y) * (TILE_WIDTH / 2);
    float y = (coord.x + coord.y) * (TILE_HEIGHT / 2);

    // some translation
    x -= (TILE_WIDTH / 2) * translate_by_half_width;
    x += screen_width / 2;
    y += VERTICAL_OFFSET;

    return vec2(x, y);
}

Vector2 fromIso(Vector2 screen, bool snap_to_grid) {
    screen.x -= screen_width / 2;
    screen.y -= VERTICAL_OFFSET;

    float x = (screen.x / (TILE_WIDTH / 2) + screen.y / (TILE_HEIGHT / 2)) / 2;
    float y = (screen.y / (TILE_HEIGHT / 2) -(screen.x / (TILE_WIDTH / 2))) / 2;

    if (snap_to_grid) {
        x = floorf(x);
        y = floorf(y);
    }

    return vec2(x, y);
}

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

    // movement related parameters
    game_object->game_object.enemy.start = toIso(vec2(0, position.y), false);
    game_object->game_object.enemy.target = toIso(vec2(GRID_SIZE-1, position.y), false);
    game_object->game_object.enemy.move_pct = 0.0;

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

    game_object->position = vec2(x, y);
    game_object->sub_type = type;
    game_object->is_active = 1;

    game_state->game_objects.objects[game_state->game_objects.count++] = *game_object;
}

void grab_user_input(GameState* game_state) {
    game_state->mouse_position = fromIso(GetMousePosition(), true);

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
            GameObject* enemy = &game_state->game_objects.objects[e];
            int speed = 0;

            if (enemy->sub_type == ENEMY_TYPE_1) {speed = 25;}
            else if (enemy->sub_type == ENEMY_TYPE_2) {speed = 10;}

            enemy->game_object.enemy.move_pct += speed * (delta_time / 1000);
            enemy->game_object.enemy.move_pct = Clamp(enemy->game_object.enemy.move_pct, 0, 1);
            // calculate the current iso position
            enemy->game_object.enemy.current_iso_coord = Vector2Lerp(
                enemy->game_object.enemy.start,
                enemy->game_object.enemy.target,
                enemy->game_object.enemy.move_pct
            );
            // this is necessary for depth sorting
            enemy->position = fromIso(enemy->game_object.enemy.current_iso_coord, false);
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

    qsort(game_state->game_objects.objects, game_state->game_objects.count, sizeof(GameObject), compareGameObjects);
    game_state->game_objects.count -= remove_count;
}

void draw(GameState* game_state) {
    for (int y = 0; y < GRID_SIZE; y++){
        for (int x = 0; x < GRID_SIZE; x++){
            Vector2 grid_coords = vec2(x, y);
            Vector2 iso_coords = toIso(grid_coords, true);
            Vector2 mouse_coords = game_state->mouse_position;

            if ((int) mouse_coords.y == y) {
                if ((int) mouse_coords.x == x) {
                    DrawTextureV(mouseover_texture, iso_coords, WHITE);
                } else {
                    DrawTextureV(ground_texture, iso_coords, WHITE);
                }
                DrawTextureV(white_full_overlay_texture, iso_coords, WHITE);
            } else {
                DrawTextureV(ground_texture, iso_coords, WHITE);
            }

        }
    }

    for (int e = 0; e < game_state->game_objects.count; e++) {
        GameObject object = game_state->game_objects.objects[e];
        Texture2D texture = GAME_OBJECT_TEXTURES[object.sub_type];

        if (object.type == DEFENSE) {
            Vector2 iso_coords = toIso(object.position, true);
            iso_coords.y -= TILE_HEIGHT;
            DrawTextureV(texture, iso_coords, WHITE);

            // draw charging animation
            float diff = GetTime() - object.game_object.defense.last_attacked;
            float pct = diff / 4.0;
            BeginScissorMode((int) iso_coords.x, (int) ceil(iso_coords.y + 2 * TILE_HEIGHT * (1 - pct)), TILE_WIDTH, 2 * TILE_HEIGHT * pct);
                DrawTextureV(white_half_overlay_texture, iso_coords, WHITE);
            EndScissorMode();
        } else if (object.type == ENEMY) {
            Vector2 iso_coords = object.game_object.enemy.current_iso_coord;
            iso_coords.x -= TILE_WIDTH / 2;
            iso_coords.y -= TILE_HEIGHT;
            DrawTextureV(texture, iso_coords, WHITE);
        } else if (object.type == PROJECTILE) {
            Vector2 iso_coords = toIso(object.position, true);
            iso_coords.y -= TILE_HEIGHT;
            DrawTextureV(texture, iso_coords, WHITE);
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

    Image block_30 = LoadImage("./assets/Isometric_Tiles_Pixel_Art/Blocks/blocks_30.png");
    Texture2D enemy_type_1_texture = LoadTextureFromImage(block_30);
    UnloadImage(block_30);

    Image block_31 = LoadImage("./assets/Isometric_Tiles_Pixel_Art/Blocks/blocks_31.png");
    Texture2D enemy_type_2_texture = LoadTextureFromImage(block_31);
    UnloadImage(block_31);

    Image block_24 = LoadImage("./assets/Isometric_Tiles_Pixel_Art/Blocks/blocks_24.png");
    Texture2D defender_type_1_texture = LoadTextureFromImage(block_24);
    UnloadImage(block_24);

    Image block_58 = LoadImage("./assets/Isometric_Tiles_Pixel_Art/Blocks/blocks_58.png");
    Texture2D defender_type_2_texture = LoadTextureFromImage(block_58);
    UnloadImage(block_58);

    Image block_12 = LoadImage("./assets/Isometric_Tiles_Pixel_Art/Blocks/blocks_12.png");
    ImageResize(&block_12, TILE_WIDTH / 2, TILE_WIDTH / 2);
    Texture2D projectile_1_texture = LoadTextureFromImage(block_12);
    UnloadImage(block_12);

    Image white_full_overlay = LoadImage("./assets/Isometric_Tiles_Pixel_Art/Blocks/overlay.png");
    white_full_overlay_texture = LoadTextureFromImage(white_full_overlay);
    UnloadImage(white_full_overlay);

    Image white_half_overlay = LoadImage("./assets/Isometric_Tiles_Pixel_Art/Blocks/half_overlay.png");
    white_half_overlay_texture = LoadTextureFromImage(white_half_overlay);
    UnloadImage(white_half_overlay);

    GAME_OBJECT_TEXTURES[ENEMY_TYPE_1] = enemy_type_1_texture;
    GAME_OBJECT_TEXTURES[ENEMY_TYPE_2] = enemy_type_2_texture;
    GAME_OBJECT_TEXTURES[DEFENDER_TYPE_1] = defender_type_1_texture;
    GAME_OBJECT_TEXTURES[DEFENDER_TYPE_2] = defender_type_2_texture;
    GAME_OBJECT_TEXTURES[PROJECTILE_TYPE_1] = projectile_1_texture;

    addEnemy(vec2(0, 9), ENEMY_TYPE_1, &game_state);
    addEnemy(vec2(0, 13), ENEMY_TYPE_2, &game_state);
    addEnemy(vec2(0, 18), ENEMY_TYPE_2, &game_state);

    while (!WindowShouldClose())
    {
        grab_user_input(&game_state);
        update(&game_state);

        BeginDrawing();
        ClearBackground(RAYWHITE);

        draw(&game_state);

        char text[255];
        sprintf(text, "fps: %d\ncount: %d\ncap: %d\n", GetFPS(), game_state.game_objects.count, game_state.game_objects.capacity);

        DrawText(text, 10, 0, 60, BLACK);

        EndDrawing();
    }

    {
        // free
        free(game_state.game_objects.objects);

        UnloadTexture(ground_texture);
        UnloadTexture(mouseover_texture);
        UnloadTexture(white_full_overlay_texture);
        UnloadTexture(white_half_overlay_texture);
        
        UnloadTexture(enemy_type_1_texture);
        UnloadTexture(enemy_type_2_texture);
        UnloadTexture(defender_type_1_texture);
        UnloadTexture(defender_type_2_texture);
        UnloadTexture(projectile_1_texture);
    }


    CloseWindow();

    return 0;
}