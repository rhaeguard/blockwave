#include <inttypes.h>
#include <stddef.h>
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

enum GameObjectType {
    GO_ENEMY, 
    GO_DEFENSE, 
    GO_PROJECTILE, 
    GO_COUNT
};

typedef struct GameObject {
    enum GameObjectType type;
    uint32_t id;
} GameObject;

uint32_t last_id = 0;

uint32_t generate_id() {
    last_id += 1;
    return last_id;
}

enum EnemyType {
    ENEMY_SLOW ,
    ENEMY_FAST,
    ENEMY_COUNT
};

typedef struct Enemy {
    Vector2 start_grid_coord;
    Vector2 target_grid_coord;
    Vector2 current_grid_coord;
    Vector2 current_screen_coord;
    float move_pct; // progress till dest
    float life;
    uint32_t id;
    enum EnemyType type;
} Enemy;

typedef struct Enemies {
    uint32_t count;
    uint32_t capacity;
    Enemy* members;
} Enemies;

enum DefenseType {
    DEFENSE_SLOW = ENEMY_COUNT + 1,
    DEFENSE_FAST,
    DEFENSE_COUNT,
};

typedef struct Defense {
    Vector2 current_grid_coord;
    double last_attacked;
    float life;
    uint32_t id;
    enum DefenseType type;
} Defense;

typedef struct Defenses {
    uint32_t count;
    uint32_t capacity;
    Defense* members;
} Defenses;

enum ProjectileType {
    PROJECTILE_SLOW = DEFENSE_COUNT + 1,
    PROJECTILE_FAST,
    PROJECTILE_COUNT
};

typedef struct Projectile {
    Vector2 current_grid_coord;
    uint32_t id;
    enum ProjectileType type;
    float life;
} Projectile;

typedef struct Projectiles {
    uint32_t count;
    uint32_t capacity;
    Projectile* members;
} Projectiles;

typedef struct SizedContainer {
    uint32_t count;
    uint32_t capacity;
} SizedContainer;

Vector2 vec2(float x, float y) {
    return (Vector2) {.x=x, .y=y};
}

void* resize(void* container_ptr, void* objects, size_t object_size) {
    SizedContainer* container = (SizedContainer*) container_ptr;
    if (container->count >= container->capacity) {
        if (container->capacity == 0) {
            container->capacity = 256;
        } else {
            container->capacity *= 2;
        }
        return realloc(objects, container->capacity * object_size);
    }
    return objects;
}

/* global variables start */
Enemies enemies;
Defenses defenses;
Projectiles projectiles;
Vector2 mouse_position;
//
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
    float x = (coord.x - coord.y) * (TILE_WIDTH / 2.0);
    float y = (coord.x + coord.y) * (TILE_HEIGHT / 2.0);

    // some translation
    x -= (TILE_WIDTH / 2.0) * translate_by_half_width;
    x += HORIZONTAL_OFFSET;
    y += VERTICAL_OFFSET;

    return vec2(x, y);
}

// This function returns the grid coordinates
// given the screen coordinates
Vector2 toGridCoords(Vector2 screen, bool snap_to_grid) {
    screen.x -= HORIZONTAL_OFFSET;
    screen.y -= VERTICAL_OFFSET;

    float x = (screen.x / (TILE_WIDTH / 2.0) + screen.y / (TILE_HEIGHT / 2.0)) / 2;
    float y = (screen.y / (TILE_HEIGHT / 2.0) -(screen.x / (TILE_WIDTH / 2.0))) / 2;

    if (snap_to_grid) {
        x = ceilf(x);
        y = ceilf(y);
    }

    return vec2(x, y);
}

int compareEnemy(const void* a, const void* b) {
    Enemy* o1 = ( (Enemy*) a );
    Enemy* o2 = ( (Enemy*) b );

    if (o1->life <= 0) return 1;
    if (o2->life <= 0) return -1;

    Vector2 p1 = o1->current_grid_coord;
    Vector2 p2 = o2->current_grid_coord;

    if (p1.y < p2.y) {
        return -1;
    } else if (p1.y > p2.y) {
        return 1;
    } 
    
    return p1.x - p2.x;
}

int compareDefense(const void* a, const void* b) {
    Defense* o1 = ( (Defense*) a );
    Defense* o2 = ( (Defense*) b );

    if (o1->life <= 0) return 1;
    if (o2->life <= 0) return -1;

    Vector2 p1 = o1->current_grid_coord;
    Vector2 p2 = o2->current_grid_coord;

    if (p1.y < p2.y) {
        return -1;
    } else if (p1.y > p2.y) {
        return 1;
    } 
    
    return p1.x - p2.x;
}

int compareProjectile(const void* a, const void* b) {
    Projectile* o1 = ( (Projectile*) a );
    Projectile* o2 = ( (Projectile*) b );

    if (o1->life <= 0) return 1;
    if (o2->life <= 0) return -1;

    Vector2 p1 = o1->current_grid_coord;
    Vector2 p2 = o2->current_grid_coord;

    if (p1.y < p2.y) {
        return -1;
    } else if (p1.y > p2.y) {
        return 1;
    } 
    
    return p1.x - p2.x;
}

int isometricViewCompareVec2(Vector2* p1, Vector2* p2) {
    if (p1->y < p2->y) {
        return -1;
    } else if (p1->y > p2->y) {
        return 1;
    } 
    return p1->x - p2->x;
}

uint8_t isometricViewCompare(
    Defense* d,
    Enemy* e,
    Projectile* p
) {
    Vector2 DUMMY_REFERENCE = {.x = 99999, .y = 99999};
    Vector2 defense_grid_coords = d == NULL ? DUMMY_REFERENCE : d->current_grid_coord;
    Vector2 enemy_grid_coords = e == NULL ? DUMMY_REFERENCE : e->current_grid_coord;
    Vector2 projectile_grid_coords = p == NULL ? DUMMY_REFERENCE : p->current_grid_coord;

    bool defenseIsBehind = isometricViewCompareVec2(&defense_grid_coords, &enemy_grid_coords) == -1;
    if (defenseIsBehind) {
        bool defenseIsBehindAll = isometricViewCompareVec2(&defense_grid_coords, &projectile_grid_coords) == -1;
        if (defenseIsBehindAll) {
            return 1;
        }
    } else {
        // enemy is behind the defense.
        bool enemyIsBehindAll = isometricViewCompareVec2(&enemy_grid_coords, &projectile_grid_coords) == -1;
        if (enemyIsBehindAll) {
            return 2;
        }
    }

    return 3;
}

void addEnemy(Vector2 grid_coord, enum EnemyType type) {
    enemies.members = resize(&enemies, enemies.members, sizeof(Enemy));

    Enemy* enemy = &(enemies.members[enemies.count++]); 
    enemy->type = type;

    // movement related parameters
    enemy->start_grid_coord = vec2(0, grid_coord.y);
    enemy->target_grid_coord = vec2(GRID_SIZE-1, grid_coord.y);
    enemy->move_pct = 0.0;
    enemy->life = 100; // will be different by the enemy type
    enemy->id = generate_id();

    enemy->current_grid_coord = grid_coord;
}

void addDefense(Vector2 position, enum DefenseType type) {
    defenses.members = resize(&defenses, defenses.members, sizeof(Defense));

    Defense* defense = &(defenses.members[defenses.count++]); 
    defense->type = type;
    defense->current_grid_coord = position;
    defense->last_attacked = GetTime();
    defense->life = 100;
    defense->id = generate_id();
}

void addProjectile(float x, float y, enum ProjectileType type) {
    projectiles.members = resize(&projectiles, projectiles.members, sizeof(Projectile));

    Projectile* projectile = &(projectiles.members[projectiles.count++]); 
    projectile->type = type;
    projectile->current_grid_coord = vec2(x, y);
    projectile->id = generate_id();
    projectile->life = 100;
}

int checkProjectileCollision(Projectile* projectile) {
    Vector2 pp = projectile->current_grid_coord;

    for (int i=0; i < enemies.count; i++) {
        Enemy* enemy = &(enemies.members[i]); 
        Vector2 ep = enemy->current_grid_coord;
        if (ep.y != pp.y) { continue; }

        bool collison_detected = (ep.x + 1) > pp.x;
        if (collison_detected) {
            return i;
        }
    }

    return -1;
}

void grabUserInput() {
    mouse_position = toGridCoords(GetMousePosition(), true);

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        int mpx = mouse_position.x;
        int mpy = mouse_position.y;
        if (mpx >= 0 && mpx < GRID_SIZE && mpy >= 0 && mpy < GRID_SIZE) {
            if (mpx > 5 && mpx < GRID_SIZE - 2) {
                addDefense(mouse_position, DEFENSE_SLOW);
            }
        }
    }
}

void update() {
    float delta_time = GetFrameTime();

    // update enemies
    int remove_count = 0;
    for (int i=0; i < enemies.count; i++) {
        Enemy* enemy = &(enemies.members[i]);

        if (enemy->life <= 0) {
            remove_count++;
            continue;
        } 

        float speed = 0;

        if (enemy->type == ENEMY_FAST) {speed = 0.025;}
        else if (enemy->type == ENEMY_SLOW) {speed = 0.010;}

        speed *=1.3;

        float dt = delta_time;
        enemy->move_pct += speed * dt;
        enemy->move_pct = Clamp(enemy->move_pct, 0, 1);
        Vector2 interpolated_grid_coord = Vector2Lerp(
            enemy->start_grid_coord, 
            enemy->target_grid_coord, 
            enemy->move_pct
        );
        enemy->current_screen_coord = toScreenCoords(interpolated_grid_coord, true);
        // this is necessary for depth sorting
        enemy->current_grid_coord.x = roundf(interpolated_grid_coord.x);
        enemy->current_grid_coord.y = roundf(interpolated_grid_coord.y);
    }

    qsort(enemies.members, enemies.count, sizeof(Enemy), compareEnemy);
    enemies.count -= remove_count;

    // update defenses
    remove_count = 0;
    for (int i=0; i < defenses.count; i++) {
        Defense* defense = &(defenses.members[i]);
        // TODO: projectile generation should be based on charging a certain bar 
        // which would be higher/lower depending on the effectiveness of the projectile
        double time_passed = GetTime() - defense->last_attacked;
        if (time_passed < 4.0) { continue; }

        Vector2 p = defense->current_grid_coord;
        addProjectile(p.x-1, p.y, PROJECTILE_FAST);
        defense->last_attacked = GetTime();
    }

    qsort(defenses.members, defenses.count, sizeof(Defense), compareDefense);
    defenses.count -= remove_count;

    // update projectiles
    remove_count = 0;
    for (int i=0; i < projectiles.count; i++) {
        Projectile* projectile = &(projectiles.members[i]);
        // TODO: projectiles will move with different speeds
        projectile->current_grid_coord.x -= 2 * delta_time;

        int collided_object_pos = checkProjectileCollision(projectile);
        
        if (projectile->current_grid_coord.x < 0 || collided_object_pos != -1) {
            projectile->life = 0;
            remove_count++;
            (&enemies.members[collided_object_pos])->life -= 40;
        }
    }

    qsort(projectiles.members, projectiles.count, sizeof(Projectile), compareProjectile);
    projectiles.count -= remove_count;
}

void draw() {
    for (int i=0; i < enemies.count; i++) {
        Enemy* enemy = &(enemies.members[i]);

        if (enemy->life <= 0) {
            continue;
        }

        int y_pos = (int)ceilf(enemy->current_grid_coord.y);
        Vector2 grid_coords = Vector2Lerp(enemy->start_grid_coord, enemy->target_grid_coord, enemy->move_pct);
        enemy_positions[y_pos] = grid_coords.x;
    }

    // draw the grid
    for (int y = 0; y < GRID_SIZE; y++){
        for (int x = 0; x < GRID_SIZE; x++){
            Vector2 grid_coords = vec2(x, y);
            Vector2 screen_coords = toScreenCoords(grid_coords, true);

            Texture2D* ground_texture = &ground_grass_texture;
            Texture2D* treaded_texture = &ground_grass_treaded_texture;

            if (x >= GRID_SIZE - 2) {
                ground_texture = &ground_pavement_texture;
            } else if (x <= 5) {
                ground_texture = &ground_sand_texture;
                treaded_texture = &ground_sand_treaded_texture;
            }

            if ((int) mouse_position.y == y) {
                if ((int) mouse_position.x == x && (x > 5 && x < GRID_SIZE - 2)) {
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

    {
        // draw the chars and objects
        // we're basically doing a k-way "merge" here.
        // each individual array is sorted
        // so we keep picking the element in the far back to be rendered first
        size_t di = 0, ei = 0, pi = 0;

        while (
            di < defenses.count || ei < enemies.count || pi < projectiles.count
        ) {
            Defense* defense = di < defenses.count ? &defenses.members[di] : NULL;
            Enemy* enemy = ei < enemies.count ? &enemies.members[ei] : NULL;
            Projectile* projectile = pi < projectiles.count ? &projectiles.members[pi] : NULL;
            
            uint8_t smallest = isometricViewCompare(defense, enemy, projectile);

            if (smallest == 1) {
                di++;
                
                Vector2 screen_coords = toScreenCoords(defense->current_grid_coord, true);
                screen_coords.y -= TILE_HEIGHT;
                
                Texture2D texture = GAME_OBJECT_TEXTURES[defense->type];
                DrawTextureV(texture, screen_coords, WHITE);

                // draw charging animation
                float diff = GetTime() - defense->last_attacked;
                float pct = diff / 4.0;
                BeginScissorMode((int) screen_coords.x, (int) ceil(screen_coords.y + 2 * TILE_HEIGHT * (1 - pct)), TILE_WIDTH, 2 * TILE_HEIGHT * pct);
                    DrawTextureV(white_half_overlay_texture, screen_coords, WHITE);
                EndScissorMode();
            } else if (smallest == 2) {
                ei++;

                Texture2D texture = GAME_OBJECT_TEXTURES[enemy->type];

                Vector2 screen_coords = enemy->current_screen_coord;
                screen_coords.y -= TILE_HEIGHT;
                DrawTextureV(texture, screen_coords, WHITE);
            } else if (smallest == 3) {
                pi++;

                Texture2D texture = GAME_OBJECT_TEXTURES[projectile->type];
                Vector2 screen_coords = toScreenCoords(projectile->current_grid_coord, true);
                screen_coords.y -= TILE_HEIGHT;
                DrawTextureV(texture, vec2(screen_coords.x + TILE_WIDTH/4.0, screen_coords.y + TILE_WIDTH/4.0), WHITE);
            } else {
                // what??
            }
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

void init(void) {
    enemies = (Enemies){.count=0, .capacity=0, .members=NULL};
    enemies.members = resize(&enemies, enemies.members, sizeof(Enemy));

    defenses = (Defenses){0};
    defenses.members = resize(&defenses, defenses.members, sizeof(Defense));

    projectiles = (Projectiles){0};
    projectiles.members = resize(&projectiles, projectiles.members, sizeof(Projectile));
}

int main(void){
    init();

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

    GAME_OBJECT_TEXTURES[ENEMY_SLOW] = enemy_type_1_texture;
    GAME_OBJECT_TEXTURES[ENEMY_FAST] = enemy_type_2_texture;
    GAME_OBJECT_TEXTURES[DEFENSE_SLOW] = defender_type_1_texture;
    GAME_OBJECT_TEXTURES[DEFENSE_FAST] = defender_type_2_texture;
    GAME_OBJECT_TEXTURES[PROJECTILE_FAST] = projectile_1_texture;

    addEnemy(vec2(0, 9), ENEMY_SLOW);
    addEnemy(vec2(0, 13), ENEMY_FAST);
    addEnemy(vec2(0, 18), ENEMY_FAST);

    while (!WindowShouldClose())
    {
        grabUserInput();
        update();

        BeginDrawing();
            ClearBackground(RAYWHITE);
            draw();
        EndDrawing();
    }

    {
        // free
        free(defenses.members);
        free(enemies.members);
        free(projectiles.members);

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