#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include "raylib.h"
#include "raymath.h"
#include <time.h>

// #define GRID_SIZE 15
#define GRID_WIDTH 30
#define GRID_HEIGHT 10

#define DEBUG 1
#define DEBUG_PRINT if (DEBUG) printf
#define DRAW_GRID_BOUNDING_BOX if (false)

#define COMPARE_FUNC(T) \
int compare##T(const void* a, const void* b) {  \
    T* o1 = ( (T*) a );                         \
    T* o2 = ( (T*) b );                         \
    if (o1->life <= 0) return 1;                \
    if (o2->life <= 0) return -1;               \
    Vector2 p1 = o1->current_grid_coord;        \
    Vector2 p2 = o2->current_grid_coord;        \
    return isometricViewCompareVec2(&p1, &p2);  \
}

float TILE_WIDTH = 64;
float TILE_HEIGHT = 32;
// TODO: might need a better data structure
float enemy_positions[GRID_HEIGHT] = {0.0};
float VERTICAL_OFFSET;
float HORIZONTAL_OFFSET;
Vector2 DUMMY_REFERENCE = {.x = 99999, .y = 99999};

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
    Vector2 start_grid_coord;
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

enum TextureIds {
    TEXTURE_GROUND_GRASS,
    TEXTURE_GROUND_GRASS_TREADED,
    TEXTURE_GROUND_PAVEMENT,
    TEXTURE_GROUND_SAND,
    TEXTURE_GROUND_SAND_TREADED,
    TEXTURE_MOUSEOVER,
    TEXTURE_WHITE_FULL_OVERLAY,
    TEXTURE_WHITE_HALF_OVERLAY,
    TEXTURE_MACHINE_GUN,
    TEXTURE_ENEMY_TYPE_1,
    TEXTURE_ENEMY_TYPE_2,
    TEXTURE_DEFENDER_TYPE_1,
    TEXTURE_DEFENDER_TYPE_2,
    TEXTURE_PROJECTILE_1,
    TEXTURE_COUNT
};

Vector2 vec2(float x, float y) {
    return (Vector2) {.x=x, .y=y};
}

// returns a random float between [0, 1]
float get_random_float() {
    float r = (float)rand() / (float)RAND_MAX;
    return r;
}

// Shards start
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

typedef struct Shards {
    uint32_t count;
    uint32_t capacity;
    Shard* members;
} Shards;

Vector2 shard_get_point(float angle, Vector2 e_radius) {
    float theta = angle * DEG2RAD;

    float x = e_radius.x * cosf(theta);
    float y = e_radius.y * sinf(theta);

    return (Vector2) {.x = x, .y = y};
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

    Vector2 e_radius = {
        .x = shard->radius * 1.5,
        .y = shard->radius
    };

    shards_get_polygon(e_radius, shard);
}

void shard_draw(Shard* shard) {
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
// Shards end

// TODO: add downsizing
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

/* global variables start */
Enemies enemies;
Defenses defenses;
Projectiles projectiles;
Shards shards;
Vector2 mouse_position;
//
uint16_t screen_width;
uint16_t screen_height;
Texture2D ALL_TEXTURES[TEXTURE_COUNT + 1];
Texture2D GAME_OBJECT_TEXTURES[10];
/* global variables end */

// This function returns the screen coordinates
// given the grid coordinates
Vector2 toScreenCoords(Vector2 coord) {
    // calculate screen coordinates
    float x = (coord.x - coord.y) * (TILE_WIDTH / 2.0);
    float y = (coord.x + coord.y) * (TILE_HEIGHT / 2.0);

    // some translation
    // x -= TILE_WIDTH / 2.0;
    x += HORIZONTAL_OFFSET;
    y += VERTICAL_OFFSET;

    return vec2(x, y);
}

// This function returns the grid coordinates
// given the screen coordinates
Vector2 toGridCoords(Vector2 screen) {
    screen.x -= HORIZONTAL_OFFSET;
    screen.y -= VERTICAL_OFFSET;

    float x = (screen.x / (TILE_WIDTH / 2.0) + screen.y / (TILE_HEIGHT / 2.0)) / 2;
    float y = (screen.y / (TILE_HEIGHT / 2.0) -(screen.x / (TILE_WIDTH / 2.0))) / 2;

    // snap to grid
    x = ceilf(x);
    y = ceilf(y);

    return vec2(x, y);
}

int isometricViewCompareVec2(Vector2* p1, Vector2* p2) {
    if (p1->y < p2->y) {
        return -1;
    } else if (p1->y > p2->y) {
        return 1;
    } 
    return p1->x - p2->x;
}

COMPARE_FUNC(Enemy);
COMPARE_FUNC(Defense);
COMPARE_FUNC(Projectile);

int compareShard(const void *a, const void *b) {
  Shard *o1 = ((Shard *)a);
  Shard *o2 = ((Shard *)b);
  if (o1->life <= 0)
    return 1;
  if (o2->life <= 0)
    return -1;
  return 0;
}

uint8_t isometricViewCompare(
    Defense* d,
    Enemy* e,
    Projectile* p
) {
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
    enemy->target_grid_coord = vec2(GRID_WIDTH-1, grid_coord.y);
    enemy->move_pct = 0.0;
    enemy->life = 100; // will be different by the enemy type

    enemy->current_grid_coord = grid_coord;
}

void addDefense(Vector2 position, enum DefenseType type) {
    defenses.members = resize(&defenses, defenses.members, sizeof(Defense));

    Defense* defense = &(defenses.members[defenses.count++]); 
    defense->type = type;
    defense->current_grid_coord = position;
    defense->last_attacked = GetTime();
    defense->life = 100;
}

void addProjectile(float x, float y, Vector2 start_grid_coord, enum ProjectileType type) {
    projectiles.members = resize(&projectiles, projectiles.members, sizeof(Projectile));
    DEBUG_PRINT("P: count=%d, cap=%d\n", projectiles.count, projectiles.capacity);

    Projectile* projectile = &(projectiles.members[projectiles.count++]); 
    projectile->type = type;
    projectile->current_grid_coord = vec2(x, y);
    projectile->start_grid_coord = vec2(start_grid_coord.x, start_grid_coord.y);
    projectile->life = 100;
}

void addShard(float x, float y, float angle, float speed, float radius, float life, Color color) {
    shards.members = resize(&shards, shards.members, sizeof(Shard));

    float angle_in_radians = angle * DEG2RAD;

    Shard* shard = &(shards.members[shards.count++]); 
    shard->position = vec2(x, y);
    shard->life = life;
    shard->velocity = (Vector2) {
        .x = cosf(angle_in_radians) * speed,
        .y = -sinf(angle_in_radians) * speed,
    };
    shard->color = color;
    shard->radius = radius;

    shards_set_angles(shard);
}

int checkProjectileCollision(Projectile* projectile) {
    Vector2 pp = projectile->current_grid_coord;
    Vector2 st = projectile->start_grid_coord;

    for (int i=0; i < enemies.count; i++) {
        Enemy* enemy = &(enemies.members[i]); 
        Vector2 ep = enemy->current_grid_coord;
        if (ep.y != pp.y) { continue; }

        bool collison_detected = (ep.x + 1 > pp.x) && (ep.x <= st.x);
        if (collison_detected) {
            return i;
        }
    }

    return -1;
}

int checkEnemyDefenseCollision(Defense* defense) {
    Vector2 dp = defense->current_grid_coord;

    for (int i=0; i < enemies.count; i++) {
        Enemy* enemy = &(enemies.members[i]); 
        Vector2 ep = enemy->current_grid_coord;
        if (ep.y != dp.y) { continue; }

        bool collison_detected = (ep.x + 1 > dp.x);
        if (collison_detected) {
            return i;
        }
    }

    return -1;
}

void grabUserInput() {
    mouse_position = toGridCoords(GetMousePosition());

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        int mpx = mouse_position.x;
        int mpy = mouse_position.y;
        if (mpx >= 0 && mpx < GRID_WIDTH && mpy >= 0 && mpy < GRID_HEIGHT) {
            if (mpx > 5 && mpx < GRID_WIDTH - 2) {
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

        enemy->move_pct += speed * delta_time;
        enemy->move_pct = Clamp(enemy->move_pct, 0, 1);
        Vector2 interpolated_grid_coord = Vector2Lerp(
            enemy->start_grid_coord, 
            enemy->target_grid_coord, 
            enemy->move_pct
        );
        enemy->current_screen_coord = toScreenCoords(interpolated_grid_coord);
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

        int collided_object_pos = checkEnemyDefenseCollision(defense);

        if (collided_object_pos != -1) {
            defense->life = 0;
            remove_count += 1;

            Vector2 explosion_center = {
                .x = defense->current_grid_coord.x + 0.5,
                .y = defense->current_grid_coord.y - 0.5
            };

            Vector2 screen_coords = toScreenCoords(explosion_center);

            for (float f=0.0; f < 100.0; f += 0.5) {
                addShard(
                    screen_coords.x,
                    screen_coords.y,
                    3.6*f, 
                    0.008 * screen_width * get_random_float(),
                    (screen_width / 256.0) * (get_random_float() / 2), 
                    get_random_float(), 
                    BLUE
                );
            }
            continue;
        }

        // TODO: projectile generation should be based on charging a certain bar 
        // which would be higher/lower depending on the effectiveness of the projectile
        double time_passed = GetTime() - defense->last_attacked;
        if (time_passed < 4.0) { continue; }

        Vector2 p = defense->current_grid_coord;
        addProjectile(p.x-1, p.y, p, PROJECTILE_FAST);
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
        
        // went out of bounds OR hit an enemy
        if (projectile->current_grid_coord.x < 0 || collided_object_pos != -1) {
            projectile->life = 0;
            remove_count++;
            
            Vector2 explosion_center;

            // hit an enemy
            if (collided_object_pos != -1) {
                Enemy* hit_enemy = &(enemies.members[collided_object_pos]);
                explosion_center.x = hit_enemy->current_grid_coord.x + 1;
                explosion_center.y = hit_enemy->current_grid_coord.y - 0.5;
            } else {
                explosion_center.x = 0;
                explosion_center.y = projectile->current_grid_coord.y;
            }

            Vector2 screen_coords = toScreenCoords(explosion_center);

            for (float f=0.0; f < 100.0; f += 0.5) {
                addShard(
                    screen_coords.x,
                    screen_coords.y,
                    3.6*f, 
                    0.008 * screen_width * get_random_float(),
                    (screen_width / 256.0) * (get_random_float() / 2), 
                    get_random_float(), 
                    RED
                );
            }
            DEBUG_PRINT("added shards [cap:%d, count:%d]\n", shards.capacity, shards.count);
        }

        // collided with enemy, update enemy health
        if (collided_object_pos != -1) {
            Enemy* enemy = (&enemies.members[collided_object_pos]);
            enemy->life -= 40;
            Vector2 screen_coords = {
                .x=enemy->current_screen_coord.x,
                .y=enemy->current_screen_coord.y
            };
            screen_coords.y -= TILE_HEIGHT;
            screen_coords.x += TILE_WIDTH/2.0 + TILE_WIDTH/4.0;
            screen_coords.y += TILE_WIDTH/2.0 + TILE_WIDTH/4.0;
        }
    }

    qsort(projectiles.members, projectiles.count, sizeof(Projectile), compareProjectile);
    projectiles.count -= remove_count;

    // update shards
    remove_count = 0;
    for (int i=0; i < shards.count; i++) {
        Shard* shard = &(shards.members[i]);
        shard_update(shard);
        if (shard->life <= 0) {
            remove_count++;
        }
    }

    qsort(shards.members, shards.count, sizeof(Shard), compareShard);
    shards.count -= remove_count;
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
    float minx = INT_MAX; 
    float miny = INT_MAX;
    float maxx = INT_MIN;
    float maxy = INT_MIN;
    for (int y = 0; y < GRID_HEIGHT; y++){
        for (int x = 0; x < GRID_WIDTH; x++){
            Vector2 grid_coords = vec2(x, y);
            Vector2 screen_coords = toScreenCoords(grid_coords);
            
            DRAW_GRID_BOUNDING_BOX {
                float xx = screen_coords.x;
                float yy = screen_coords.y;
                if (xx < minx) {minx = xx;}
                if (xx > maxx) {maxx = xx;}

                if (yy < miny) {miny = yy;}
                if (yy > maxy) {maxy = yy;}
            }

            Texture2D* ground_texture = &ALL_TEXTURES[TEXTURE_GROUND_GRASS];
            Texture2D* treaded_texture = &ALL_TEXTURES[TEXTURE_GROUND_GRASS_TREADED];

            if (x >= GRID_WIDTH - 2) {
                ground_texture = &ALL_TEXTURES[TEXTURE_GROUND_PAVEMENT];
            } else if (x <= 5) {
                ground_texture = &ALL_TEXTURES[TEXTURE_GROUND_SAND];
                treaded_texture = &ALL_TEXTURES[TEXTURE_GROUND_SAND_TREADED];
            }

            if (enemy_positions[y] > x) {
                DrawTextureV(*treaded_texture, screen_coords, WHITE);
            } else {
                DrawTextureV(*ground_texture, screen_coords, WHITE);
            }

            int mpx = mouse_position.x;
            int mpy = mouse_position.y;

            if (mpy == y && mpx >= 0 && mpx < GRID_WIDTH) {
                if (mpx == x && (x > 5 && x < GRID_WIDTH - 2)) {
                    DrawTextureV(ALL_TEXTURES[TEXTURE_MOUSEOVER], screen_coords, WHITE);
                }
                DrawTextureV(ALL_TEXTURES[TEXTURE_WHITE_FULL_OVERLAY], screen_coords, WHITE);
            }
        }
    }

    // grid bounding box
    Rectangle r = {.x=minx, .y=miny, .width=maxx-minx + TILE_WIDTH, .height=maxy-miny+TILE_HEIGHT*2};
    DRAW_GRID_BOUNDING_BOX DrawRectangleLinesEx(r, 2.0, BLUE);

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
                
                Vector2 screen_coords = toScreenCoords(defense->current_grid_coord);
                screen_coords.y -= TILE_HEIGHT;
                
                Texture2D texture = GAME_OBJECT_TEXTURES[defense->type];
                DrawTextureV(texture, screen_coords, WHITE);

                // draw charging animation
                float diff = GetTime() - defense->last_attacked;
                float pct = diff / 4.0;
                BeginScissorMode((int) screen_coords.x, (int) ceil(screen_coords.y + 2 * TILE_HEIGHT * (1 - pct)), TILE_WIDTH, 2 * TILE_HEIGHT * pct);
                    DrawTextureV(ALL_TEXTURES[TEXTURE_WHITE_HALF_OVERLAY], screen_coords, WHITE);
                EndScissorMode();
            } else if (smallest == 2) {
                ei++;

                Texture2D texture = GAME_OBJECT_TEXTURES[enemy->type];

                Vector2 screen_coords = enemy->current_screen_coord;
                screen_coords.y -= TILE_HEIGHT;
                DrawTextureV(texture, screen_coords, WHITE);

                // DrawCircleV(vec2(screen_coords.x + TILE_WIDTH/2.0 + TILE_WIDTH/4.0, screen_coords.y + TILE_WIDTH/2.0 + TILE_WIDTH/4.0), 5.0, RED);
                // DrawRectangleLines(
                //     (int )(screen_coords.x), 
                //     (int) (screen_coords.y), 
                //     texture.width, 
                //     texture.height, 
                //     BLACK
                // );
            } else if (smallest == 3) {
                pi++;

                Texture2D texture = GAME_OBJECT_TEXTURES[projectile->type];
                Vector2 screen_coords = toScreenCoords(projectile->current_grid_coord);
                screen_coords.y -= TILE_HEIGHT;
                DrawTextureV(texture, vec2(screen_coords.x + TILE_WIDTH/4.0, screen_coords.y + TILE_WIDTH/4.0), WHITE);
                // DrawRectangleLines(
                //     (int )(screen_coords.x + TILE_WIDTH/4.0), 
                //     (int) (screen_coords.y + TILE_WIDTH/4.0), 
                //     texture.width, 
                //     texture.height, 
                //     BLACK
                // );
                // // [aaaa.aa]; [aaaa.aa]
                // char buf[21];
                // sprintf( buf, "[%.2f]; [%.2f]", (projectile->current_grid_coord.x), (projectile->current_grid_coord.y));
                // DrawText(buf, 
                //     (int )(screen_coords.x + TILE_WIDTH/4.0), 
                //     (int) (screen_coords.y + TILE_WIDTH/4.0), 
                //     15, 
                //     BLACK
                // );
            } else {
                // what??
            }
        }
    }

    {
        // DEBUG_PRINT("drawing shards [cap:%d, count:%d]\n", shards.capacity, shards.count);
        for (int i=0; i < shards.count; i++) {
            Shard* shard = &(shards.members[i]);
            shard_draw(shard);
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
    enemies = (Enemies){0};
    enemies.members = resize(&enemies, enemies.members, sizeof(Enemy));

    defenses = (Defenses){0};
    defenses.members = resize(&defenses, defenses.members, sizeof(Defense));

    projectiles = (Projectiles){0};
    projectiles.members = resize(&projectiles, projectiles.members, sizeof(Projectile));

    shards = (Shards) {0};
    shards.members = resize(&shards, shards.members, sizeof(Shard));
}

int main(void){
    init();

    SetConfigFlags(FLAG_VSYNC_HINT);
    // SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    SetConfigFlags(FLAG_FULLSCREEN_MODE);
   
    SetTargetFPS(30);
    InitWindow(0, 0, "blockwave");

    screen_width = GetScreenWidth();
    screen_height = GetScreenHeight();

    //  2560 x 1440
    TILE_HEIGHT = (32.0 * screen_height) / 1440;
    TILE_WIDTH = 2 * TILE_HEIGHT;

    float iso_width = (GRID_HEIGHT + GRID_WIDTH) * (TILE_WIDTH / 2.0);
    float iso_height = (GRID_HEIGHT + GRID_WIDTH) * (TILE_HEIGHT / 2);
    HORIZONTAL_OFFSET = (screen_width - iso_width) / 2.0 + GRID_HEIGHT * TILE_WIDTH / 2.0;
    VERTICAL_OFFSET = (screen_height - iso_height) / 2.0;

    ALL_TEXTURES[TEXTURE_GROUND_GRASS] = loadTextureFromImage("Blocks/blocks_1.png");
    ALL_TEXTURES[TEXTURE_GROUND_GRASS_TREADED] = loadTextureFromImage("Blocks/blocks_1_treaded.png");
    ALL_TEXTURES[TEXTURE_GROUND_PAVEMENT] = loadTextureFromImage("Blocks/blocks_56.png");
    ALL_TEXTURES[TEXTURE_GROUND_SAND] = loadTextureFromImage("Blocks/blocks_32.png");
    ALL_TEXTURES[TEXTURE_GROUND_SAND_TREADED] = loadTextureFromImage("Blocks/blocks_32_treaded.png");
    ALL_TEXTURES[TEXTURE_MOUSEOVER] = loadTextureFromImage("Blocks/blocks_99.png");
    ALL_TEXTURES[TEXTURE_WHITE_FULL_OVERLAY] = loadTextureFromImage("Blocks/overlay.png");
    ALL_TEXTURES[TEXTURE_WHITE_HALF_OVERLAY] = loadTextureFromImage("Blocks/half_overlay.png");
    ALL_TEXTURES[TEXTURE_MACHINE_GUN] = loadTextureFromImage("Blocks/mgun.png");
    ALL_TEXTURES[TEXTURE_ENEMY_TYPE_1] = loadTextureFromImage("Blocks/blocks_30.png");
    ALL_TEXTURES[TEXTURE_ENEMY_TYPE_2] = loadTextureFromImage("Blocks/blocks_31.png");
    ALL_TEXTURES[TEXTURE_DEFENDER_TYPE_1] = loadTextureFromImage("Blocks/blocks_24.png");
    ALL_TEXTURES[TEXTURE_DEFENDER_TYPE_2] = loadTextureFromImage("Blocks/blocks_58.png");

    // load the long way, because it needs preprocessing.
    Image block_12 = LoadImage("./assets/Isometric_Tiles_Pixel_Art/Blocks/blocks_12.png");
    ImageResize(&block_12, TILE_WIDTH / 2, TILE_WIDTH / 2);
    ALL_TEXTURES[TEXTURE_PROJECTILE_1] = LoadTextureFromImage(block_12);
    UnloadImage(block_12);

    GAME_OBJECT_TEXTURES[ENEMY_SLOW] = ALL_TEXTURES[TEXTURE_ENEMY_TYPE_1];
    GAME_OBJECT_TEXTURES[ENEMY_FAST] = ALL_TEXTURES[TEXTURE_ENEMY_TYPE_2];
    GAME_OBJECT_TEXTURES[DEFENSE_SLOW] = ALL_TEXTURES[TEXTURE_MACHINE_GUN];
    GAME_OBJECT_TEXTURES[DEFENSE_FAST] = ALL_TEXTURES[TEXTURE_DEFENDER_TYPE_1];
    GAME_OBJECT_TEXTURES[PROJECTILE_FAST] = ALL_TEXTURES[TEXTURE_PROJECTILE_1];

    srand(193397);
    for (int i=0; i < 3; i++) {
        int y = rand() % GRID_HEIGHT;
        addEnemy(vec2(0, y), ENEMY_SLOW);
    }

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
        DEBUG_PRINT("freeing...\n");
        free(defenses.members);
        DEBUG_PRINT("freed defenses\n");
        free(enemies.members);
        DEBUG_PRINT("freed enemies\n");
        free(projectiles.members);
        DEBUG_PRINT("freed projectiles\n");
        free(shards.members);
        DEBUG_PRINT("freed shards\n");
        
        DEBUG_PRINT("unloading textures...\n");
        for(uint8_t i=0; i<TEXTURE_COUNT; i++) {
            UnloadTexture(ALL_TEXTURES[i]);
        }
        DEBUG_PRINT("unloading textures...done!\n");

    }

    CloseWindow();

    return 0;
}