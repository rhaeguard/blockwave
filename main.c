#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "raylib.h"

#define GRID_SIZE 25
#define IMG_SIZE 64

#define TILE_WIDTH 64
#define TILE_HEIGHT 32

enum EnemyType {
    TYPE_1,
    TYPE_2
};

typedef struct Enemy {
    Vector2 position;
    enum EnemyType type;
} Enemy;

typedef struct GameState {
    Vector2 mouse_position;
    Enemy enemies[10];
    int enemy_count;
} GameState;

/* global variables start */
int screen_width;
int screen_height;
Texture2D ground_texture;
Texture2D mouseover_texture;
Texture2D enemy_type_1_texture;
Texture2D enemy_type_2_texture;
/* global variables end */

void addEnemy(Vector2 position, enum EnemyType type, GameState* game_state) {
    Enemy* enemy = &game_state->enemies[game_state->enemy_count];
    enemy->position = position;
    enemy->type = type;
    game_state->enemies[game_state->enemy_count++] = *enemy;
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
}

void update(GameState* game_state) {
    float delta_time = GetFrameTime();
    // update enemy
    for (int e = 0; e < game_state->enemy_count; e++){
        Enemy* enemy = &game_state->enemies[e];
        float speed = 0.0;

        if (enemy->type == TYPE_1) {speed = 0.25;}
        else if (enemy->type == TYPE_2) {speed = 0.5;}
        
        enemy->position.x += speed * delta_time;
    }
}

void draw(GameState* game_state) {
    for (int y = 0; y < GRID_SIZE; y++){
        for (int x = 0; x < GRID_SIZE; x++){
            Vector2 grid_coords = {.x = x, .y = y};
            Vector2 iso_coords = toIso(grid_coords);
            Vector2 mouse_coords = game_state->mouse_position;

            if ((int) mouse_coords.x == x && (int) mouse_coords.y == y) {
                DrawTextureV(mouseover_texture, iso_coords, WHITE);
            } else {
                DrawTextureV(ground_texture, iso_coords, WHITE);
            }

        }
    }

    for (int e = 0; e < game_state->enemy_count; e++) {
        Enemy* enemy = &game_state->enemies[e];
        Vector2 iso_coords = toIso(enemy->position);
        Texture2D texture;
        if (enemy->type == TYPE_1) {
            texture = enemy_type_1_texture;
        } else if (enemy->type == TYPE_2) {
            texture = enemy_type_2_texture;
        }
        DrawTextureV(texture, iso_coords, WHITE);
    }
}

int main(void){
    GameState* game_state = malloc(sizeof(GameState));
    if (game_state == NULL) {
        printf("could not allocate memory for game state");
        return 3;
    }

    SetConfigFlags(FLAG_VSYNC_HINT);

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
    enemy_type_1_texture = LoadTextureFromImage(block_34);
    UnloadImage(block_34);

    Image block_14 = LoadImage("./assets/Isometric_Tiles_Pixel_Art/Blocks/blocks_14.png");
    enemy_type_2_texture = LoadTextureFromImage(block_14);
    UnloadImage(block_14);

    Vector2 p1 = {.x = 0, .y = 9}; addEnemy(p1, TYPE_1, game_state);
    Vector2 p2 = {.x = 0, .y = 13}; addEnemy(p2, TYPE_2, game_state);

    while (!WindowShouldClose())
    {
        grab_user_input(game_state);
        update(game_state);

        BeginDrawing();
        ClearBackground(RAYWHITE);

        draw(game_state);

        EndDrawing();
    }

    free(game_state);

    CloseWindow();

    return 0;
}