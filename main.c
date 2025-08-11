#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include "raylib.h"
#include "raymath.h"

#define GRID_SIZE 30

uint8_t TILE_WIDTH = 64;
uint8_t TILE_HEIGHT = 32;
float enemy_positions[GRID_SIZE] = {0.0};
float VERTICAL_OFFSET;
float HORIZONTAL_OFFSET;

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
    Vector2 start_grid_coord;
    Vector2 target_grid_coord;
    Vector2 current_screen_coord;
    float move_pct; // progress till dest
    float life;
} Enemy;

typedef struct Defense {
    double last_attacked;
    float life;
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
    bool is_active;
} GameObject;

typedef struct GameObjects {
    GameObject* objects;
    uint32_t count;
    uint32_t capacity;
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
uint16_t screen_width;
uint16_t screen_height;
Texture2D ground_grass_texture;
Texture2D ground_grass_treaded_texture;
Texture2D ground_pavement_texture;
Texture2D ground_sand_texture;
Texture2D ground_sand_treaded_texture;
Texture2D mouseover_texture;
Texture2D white_full_overlay_texture;
Texture2D white_half_overlay_texture;
Texture2D GAME_OBJECT_TEXTURES[10];
/* global variables end */

// This function returns the screen coordinates
// given the grid coordinates
Vector2 toScreenCoords(Vector2 coord, bool translate_by_half_width) {
    // calculate screen coordinates
    float x = (coord.x - coord.y) * (TILE_WIDTH / 2);
    float y = (coord.x + coord.y) * (TILE_HEIGHT / 2);

    // some translation
    x -= (TILE_WIDTH / 2) * translate_by_half_width;
    x += HORIZONTAL_OFFSET;
    y += VERTICAL_OFFSET;

    return vec2(x, y);
}

// This function returns the grid coordinates
// given the screen coordinates
Vector2 toGridCoords(Vector2 screen, bool snap_to_grid) {
    screen.x -= HORIZONTAL_OFFSET;
    screen.y -= VERTICAL_OFFSET;

    float x = (screen.x / (TILE_WIDTH / 2) + screen.y / (TILE_HEIGHT / 2)) / 2;
    float y = (screen.y / (TILE_HEIGHT / 2) -(screen.x / (TILE_WIDTH / 2))) / 2;

    if (snap_to_grid) {
        x = ceilf(x);
        y = ceilf(y);
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
    game_object->game_object.enemy.start_grid_coord = vec2(0, position.y);
    game_object->game_object.enemy.target_grid_coord = vec2(GRID_SIZE-1, position.y);
    game_object->game_object.enemy.move_pct = 0.0;
    game_object->game_object.enemy.life = 100; // will be different by the enemy type

    game_object->position = position;
    game_object->sub_type = type;
    game_object->is_active = true;

    game_state->game_objects.objects[game_state->game_objects.count++] = *game_object;
}

void addDefense(Vector2 position, enum GeneralObjectType type, GameState* game_state) {
    resize(&game_state->game_objects);

    GameObject* game_object = &(game_state->game_objects.objects[game_state->game_objects.count]); 
    game_object->type = DEFENSE;

    game_object->game_object.defense.last_attacked = GetTime();
    game_object->position = position;
    game_object->sub_type = type;
    game_object->is_active = true;
    
    game_state->game_objects.objects[game_state->game_objects.count++] = *game_object;
}

void addProjectile(float x, float y, enum GeneralObjectType type, GameState* game_state) {
    resize(&game_state->game_objects);

    GameObject* game_object = &(game_state->game_objects.objects[game_state->game_objects.count]); 
    game_object->type = PROJECTILE;

    game_object->position = vec2(x, y);
    game_object->sub_type = type;
    game_object->is_active = true;

    game_state->game_objects.objects[game_state->game_objects.count++] = *game_object;
}

int checkProjectileCollision(GameObject* projectile, GameState* game_state) {
    Vector2 pp = projectile->position;

    for (int i=0; i < game_state->game_objects.count; i++) {
        GameObject obj = game_state->game_objects.objects[i];
        if (obj.position.y != pp.y) { continue; }

        if (obj.type == ENEMY) {
            Vector2 ep = obj.position;
            bool collison_detected = (ep.x + 1) > pp.x;
            if (collison_detected) {
                return i;
            }
        }
    }

    return -1;
}

void grabUserInput(GameState* game_state) {
    game_state->mouse_position = toGridCoords(GetMousePosition(), true);

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        int mpx = game_state->mouse_position.x;
        int mpy = game_state->mouse_position.y;
        if (mpx >= 0 && mpx < GRID_SIZE && mpy >= 0 && mpy < GRID_SIZE) {
            if (mpx > 5 && mpx < GRID_SIZE - 2) {
                addDefense(game_state->mouse_position, DEFENDER_TYPE_1, game_state);
            }
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

            if (enemy->game_object.enemy.life <= 0) {
                enemy->is_active = 0;
                remove_count++;
                continue;
            } 

            float speed = 0;

            if (enemy->sub_type == ENEMY_TYPE_1) {speed = 0.025;}
            else if (enemy->sub_type == ENEMY_TYPE_2) {speed = 0.010;}

            speed *=1.3;

            float dt = delta_time;
            enemy->game_object.enemy.move_pct += speed * dt;
            enemy->game_object.enemy.move_pct = Clamp(enemy->game_object.enemy.move_pct, 0, 1);
            Vector2 interpolated_grid_coord = Vector2Lerp(
                enemy->game_object.enemy.start_grid_coord, 
                enemy->game_object.enemy.target_grid_coord, 
                enemy->game_object.enemy.move_pct
            );
            enemy->game_object.enemy.current_screen_coord = toScreenCoords(interpolated_grid_coord, true);
            // this is necessary for depth sorting
            enemy->position.x = roundf(interpolated_grid_coord.x);
            enemy->position.y = roundf(interpolated_grid_coord.y);
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

            int collided_object_pos = checkProjectileCollision(&game_state->game_objects.objects[e], game_state);
            
            if (game_state->game_objects.objects[e].position.x < 0 || collided_object_pos != -1) {
                game_state->game_objects.objects[e].is_active = 0;
                remove_count++;

                GameObject* enemy = &game_state->game_objects.objects[collided_object_pos];
                enemy->game_object.enemy.life -= 40;
            }

        }
    }

    // UpdateParticles(delta_time);

    qsort(game_state->game_objects.objects, game_state->game_objects.count, sizeof(GameObject), compareGameObjects);
    game_state->game_objects.count -= remove_count;
}

void draw(GameState* game_state) {
    for (int e = 0; e < game_state->game_objects.count; e++) {
        GameObject object = game_state->game_objects.objects[e];

        if (object.type == ENEMY) {
            Enemy enemy = object.game_object.enemy;
            int y_pos = (int)ceilf(object.position.y);
            Vector2 grid_coords = Vector2Lerp(enemy.start_grid_coord, enemy.target_grid_coord, enemy.move_pct);
            enemy_positions[y_pos] = grid_coords.x;
        }
    }

    // draw the grid
    for (int y = 0; y < GRID_SIZE; y++){
        for (int x = 0; x < GRID_SIZE; x++){
            Vector2 grid_coords = vec2(x, y);
            Vector2 screen_coords = toScreenCoords(grid_coords, true);
            Vector2 mouse_coords = game_state->mouse_position;

            Texture2D* ground_texture = &ground_grass_texture;
            Texture2D* treaded_texture = &ground_grass_treaded_texture;

            if (x >= GRID_SIZE - 2) {
                ground_texture = &ground_pavement_texture;
            } else if (x <= 5) {
                ground_texture = &ground_sand_texture;
                treaded_texture = &ground_sand_treaded_texture;
            }

            if ((int) mouse_coords.y == y) {
                if ((int) mouse_coords.x == x && (x > 5 && x < GRID_SIZE - 2)) {
                    DrawTextureV(mouseover_texture, screen_coords, WHITE);
                } else {
                    if (enemy_positions[y] > x) {
                        DrawTextureV(*treaded_texture, screen_coords, WHITE);
                    } else {
                        DrawTextureV(*ground_texture, screen_coords, WHITE);
                    }
                }
                DrawTextureV(white_full_overlay_texture, screen_coords, WHITE);
            } else {
                if (enemy_positions[y] > x) {
                    DrawTextureV(*treaded_texture, screen_coords, WHITE);
                } else {
                    DrawTextureV(*ground_texture, screen_coords, WHITE);
                }
            }
        }
    }

    // draw the chars and objects
    for (int e = 0; e < game_state->game_objects.count; e++) {
        GameObject object = game_state->game_objects.objects[e];
        Texture2D texture = GAME_OBJECT_TEXTURES[object.sub_type];

        if (object.type == DEFENSE) {
            Vector2 screen_coords = toScreenCoords(object.position, true);
            screen_coords.y -= TILE_HEIGHT;
            DrawTextureV(texture, screen_coords, WHITE);

            // draw charging animation
            float diff = GetTime() - object.game_object.defense.last_attacked;
            float pct = diff / 4.0;
            BeginScissorMode((int) screen_coords.x, (int) ceil(screen_coords.y + 2 * TILE_HEIGHT * (1 - pct)), TILE_WIDTH, 2 * TILE_HEIGHT * pct);
                DrawTextureV(white_half_overlay_texture, screen_coords, WHITE);
            EndScissorMode();
        } else if (object.type == ENEMY) {
            Vector2 screen_coords = object.game_object.enemy.current_screen_coord;
            screen_coords.y -= TILE_HEIGHT;
            DrawTextureV(texture, screen_coords, WHITE);
        } else if (object.type == PROJECTILE) {
            Vector2 screen_coords = toScreenCoords(object.position, true);
            screen_coords.y -= TILE_HEIGHT;
            DrawTextureV(texture, vec2(screen_coords.x + TILE_WIDTH/4, screen_coords.y + TILE_WIDTH/4), WHITE);
        }
    }

}

Texture2D loadTextureFromImage(char* filename) {
    char path[256];
    sprintf(path, "./assets/Isometric_Tiles_Pixel_Art/%s", filename);
    Image image = LoadImage(path);
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    return texture;
}

int main(void){
    GameState game_state = {0};
    GameObjects objs = {0};
    game_state.game_objects = objs;

    SetConfigFlags(FLAG_VSYNC_HINT);
    SetConfigFlags(FLAG_FULLSCREEN_MODE);
   
    SetTargetFPS(30);
    InitWindow(0, 0, "blockwave");

    int monitor = GetCurrentMonitor();
    screen_width = GetMonitorWidth(monitor);
    screen_height = GetMonitorHeight(monitor);
    HORIZONTAL_OFFSET = screen_width / 2.0;
    VERTICAL_OFFSET = ((GRID_SIZE + GRID_SIZE) * (TILE_HEIGHT / 2.0)) / 4.0;

    ground_grass_texture = loadTextureFromImage("Blocks/blocks_1.png");
    ground_grass_treaded_texture = loadTextureFromImage("Blocks/blocks_1_treaded.png");
    ground_pavement_texture = loadTextureFromImage("Blocks/blocks_56.png");
    ground_sand_texture = loadTextureFromImage("Blocks/blocks_32.png");
    ground_sand_treaded_texture = loadTextureFromImage("Blocks/blocks_32_treaded.png");
    mouseover_texture = loadTextureFromImage("Blocks/blocks_99.png");
    white_full_overlay_texture = loadTextureFromImage("Blocks/overlay.png");
    white_half_overlay_texture = loadTextureFromImage("Blocks/half_overlay.png");

    Texture2D enemy_type_1_texture = loadTextureFromImage("Blocks/blocks_30.png");
    Texture2D enemy_type_2_texture = loadTextureFromImage("Blocks/blocks_31.png");
    Texture2D defender_type_1_texture = loadTextureFromImage("Blocks/blocks_24.png");
    Texture2D defender_type_2_texture = loadTextureFromImage("Blocks/blocks_58.png");

    // load the long way, because it needs preprocessing.
    Image block_12 = LoadImage("./assets/Isometric_Tiles_Pixel_Art/Blocks/blocks_12.png");
    ImageResize(&block_12, TILE_WIDTH / 2, TILE_WIDTH / 2);
    Texture2D projectile_1_texture = LoadTextureFromImage(block_12);
    UnloadImage(block_12);

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
        grabUserInput(&game_state);
        update(&game_state);

        BeginDrawing();
            ClearBackground(RAYWHITE);
            draw(&game_state);
        EndDrawing();
    }

    {
        // free
        free(game_state.game_objects.objects);

        UnloadTexture(ground_grass_texture);
        UnloadTexture(ground_grass_treaded_texture);
        UnloadTexture(ground_sand_texture);
        UnloadTexture(ground_sand_treaded_texture);
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