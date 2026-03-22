#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include "raylib.h"
#include "raymath.h"
#include <time.h>
#include "utils.h"
#include "shards.h"

#define DEBUG 1
#define DEBUG_PRINT if (DEBUG) printf
#define DRAW_GRID_BOUNDING_BOX if (false)
#define ADD_ENEMIES if (true)

// this macro assume the 'isometric_view_compare_vec2' function is declared when it is used
#define DEFINE_COMPARE_FUNC(T) \
int compare##T(const void* a, const void* b) {  \
    T* o1 = ( (T*) a );                         \
    T* o2 = ( (T*) b );                         \
    if (o1->life <= 0) return 1;                \
    if (o2->life <= 0) return -1;               \
    Vector2 p1 = o1->current_grid_coord;        \
    Vector2 p2 = o2->current_grid_coord;        \
    return isometric_view_compare_vec2(&p1, &p2);  \
}

// this macro assume the 'grid' variable is declared when it is used
#define grid_cell_at(x,y) (grid.cells[y*grid.width+x])

float TILE_WIDTH = 64;
float TILE_HEIGHT = 32;
float VERTICAL_OFFSET;
float HORIZONTAL_OFFSET;
Vector2 DUMMY_REFERENCE = {.x=99999, .y=99999};

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
    ENEMY_FIRST = 0, 
    ENEMY_TYPE_1 = 0,
    ENEMY_TYPE_2,
    ENEMY_COUNT
};

typedef struct Enemy {
    Vector2 current_grid_coord;
    Vector2 current_screen_coord;
    Vector2 direction;
    Vector2 distance_per_second;
    float life;
    enum EnemyType type;
} Enemy;

enum DefenseType {
    DEFENSE_FIRST = ENEMY_COUNT + 1, 
    DEFENSE_TYPE_1 = ENEMY_COUNT + 1,
    DEFENSE_TYPE_2,
    DEFENSE_TYPE_3,
    DEFENSE_COUNT,
};

typedef struct Defense {
    Vector2 current_grid_coord;
    double last_attacked;
    float life;
    enum DefenseType type;
} Defense;


enum ProjectileType {
    PROJECTILE_FIRST = DEFENSE_COUNT + 1, 
    PROJECTILE_TYPE_1 = DEFENSE_COUNT + 1,
    PROJECTILE_TYPE_2,
    PROJECTILE_TYPE_3,
    PROJECTILE_COUNT
};

typedef struct Projectile {
    Vector2 current_grid_coord;
    Vector2 start_grid_coord;
    enum ProjectileType type;
    float life;
    float damage;
} Projectile;

enum TextureIds {
    TEXTURE_GROUND_GRASS,
    TEXTURE_GROUND_GRASS_TREADED,
    TEXTURE_GROUND_PAVEMENT,
    TEXTURE_GROUND_PAVEMENT_TREADED,
    TEXTURE_GROUND_SAND,
    TEXTURE_GROUND_SAND_TREADED,
    TEXTURE_GROUND_TARGET,
    TEXTURE_GROUND_WATER,
    TEXTURE_MOUSEOVER,
    TEXTURE_WHITE_FULL_OVERLAY,
    TEXTURE_WHITE_HALF_OVERLAY,
    TEXTURE_WHITE_BLOCK_OVERLAY,
    TEXTURE_ENEMY_TYPE_1,
    TEXTURE_ENEMY_TYPE_2,
    TEXTURE_DEFENDER_TYPE_1,
    TEXTURE_DEFENDER_TYPE_2,
    TEXTURE_DEFENDER_TYPE_3,
    TEXTURE_PROJECTILE_1,
    TEXTURE_PROJECTILE_2,
    TEXTURE_PROJECTILE_3,
    TEXTURE_RED,
    TEXTURE_COUNT
};

typedef struct DefenseItem {
    enum DefenseType type;
    enum ProjectileType projectile_type;
    float charging_cadence_seconds;
    float damage;
    double last_dispensed;
    double wait_time_per_dispense_seconds;
    uint32_t count_dispensed;
    bool is_unlocked;
} DefenseItem;

typedef struct Inventory {
    // defense options and their charging state
    DefenseItem defense_items[DEFENSE_COUNT - DEFENSE_FIRST];
    uint8_t defense_items_size;
} Inventory;

typedef struct HUD {
    int hovered_box_index;
    int selected_box_index;
    int total_slot_count;
    float box_slot_size;
    float clearing;
    Rectangle bbox;
    Color bg_color;
} HUD;

enum GridCellType {
    GRASS,
    SAND,
    PAVEMENT,
    WATER,
    NONE,
    TARGET,
};

typedef struct GridCell {
    enum GridCellType type;
    Vector2 tread_direction;
} GridCell;

typedef struct Grid {
    uint16_t width;
    uint16_t height;
    GridCell* cells; // 2D array of cells
} Grid;

DefineSizedContainer(Enemy, Enemies);
DefineSizedContainer(Defense, Defenses);
DefineSizedContainer(Projectile, Projectiles);
DefineSizedContainer(Shard, Shards);

/* global variables start */
Grid grid;
Inventory inventory;
HUD hud;
Enemies enemies;
Defenses defenses;
Projectiles projectiles;
Shards shards;
// user inputs start
Vector2 mouse_position_on_grid;
Vector2 mouse_position_on_screen;
bool is_left_mouse_button_released = false;
// user inputs end
//
uint16_t screen_width;
uint16_t screen_height;
Texture2D ALL_TEXTURES[TEXTURE_COUNT + 1];
Texture2D GAME_OBJECT_TEXTURES[10];
/* global variables end */

// This function returns the screen coordinates
// given the grid coordinates
static inline Vector2 to_screen_coords(Vector2 grid_coords) {
    // calculate screen coordinates
    float x = (grid_coords.x - grid_coords.y) * (TILE_WIDTH / 2.0);
    float y = (grid_coords.x + grid_coords.y) * (TILE_HEIGHT / 2.0);

    // some translation
    x += HORIZONTAL_OFFSET;
    y += VERTICAL_OFFSET;

    return vec2(x, y);
}

// This function returns the grid coordinates
// given the screen coordinates
static inline Vector2 to_grid_coords(Vector2 screen) {
    screen.x -= HORIZONTAL_OFFSET;
    screen.y -= VERTICAL_OFFSET;

    float x = (screen.x / (TILE_WIDTH / 2.0) + screen.y / (TILE_HEIGHT / 2.0)) / 2;
    float y = (screen.y / (TILE_HEIGHT / 2.0) -(screen.x / (TILE_WIDTH / 2.0))) / 2;

    // snap to grid
    x = floorf(x);
    y = floorf(y);

    return vec2(x, y);
}

static inline int isometric_view_compare_vec2(const Vector2* p1, const Vector2* p2) {
    if (p1->y < p2->y) {
        return -1;
    } else if (p1->y > p2->y) {
        return 1;
    } 
    return p1->x - p2->x;
}

DEFINE_COMPARE_FUNC(Enemy)
DEFINE_COMPARE_FUNC(Defense)
DEFINE_COMPARE_FUNC(Projectile)

enum GameObjectType isometric_view_compare(
    const Defense* d,
    const Enemy* e,
    const Projectile* p
) {
    Vector2 defense_grid_coords = d == NULL ? DUMMY_REFERENCE : d->current_grid_coord;
    Vector2 enemy_grid_coords = e == NULL ? DUMMY_REFERENCE : e->current_grid_coord;
    Vector2 projectile_grid_coords = p == NULL ? DUMMY_REFERENCE : p->current_grid_coord;

    bool defenseIsBehind = isometric_view_compare_vec2(&defense_grid_coords, &enemy_grid_coords) == -1;
    if (defenseIsBehind) {
        bool defenseIsBehindAll = isometric_view_compare_vec2(&defense_grid_coords, &projectile_grid_coords) == -1;
        if (defenseIsBehindAll) {
            return GO_DEFENSE;
        }
    } else {
        // enemy is behind the defense.
        bool enemyIsBehindAll = isometric_view_compare_vec2(&enemy_grid_coords, &projectile_grid_coords) == -1;
        if (enemyIsBehindAll) {
            return GO_ENEMY;
        }
    }

    return GO_PROJECTILE;
}

void add_enemy(Vector2 grid_coord, enum EnemyType type) {
    enemies.members = resize(&enemies, enemies.members, sizeof(Enemy));

    Enemy* enemy = &(enemies.members[enemies.count++]); 
    enemy->type = type;

    // movement related parameters
    if (type == ENEMY_TYPE_1) {
        enemy->distance_per_second = vec2(0.03, 0.03);
    } else if (type == ENEMY_TYPE_2) {
        enemy->distance_per_second = vec2(0.05, 0.05);
    } else {
        enemy->distance_per_second = vec2(0.00, 0.00);
    }
    enemy->direction = vec2(1, 0);
    enemy->life = 100; // will be different by the enemy type

    enemy->current_grid_coord = grid_coord;
}

void add_defense(Vector2 position, enum DefenseType type) {
    defenses.members = resize(&defenses, defenses.members, sizeof(Defense));

    Defense* defense = &(defenses.members[defenses.count++]); 
    defense->type = type;
    defense->current_grid_coord = position;
    defense->last_attacked = GetTime();
    defense->life = 100;
}

void add_projectile(float x, float y, Vector2 start_grid_coord, enum ProjectileType type, float damage) {
    projectiles.members = resize(&projectiles, projectiles.members, sizeof(Projectile));
    DEBUG_PRINT("P: count=%d, cap=%d\n", projectiles.count, projectiles.capacity);

    Projectile* projectile = &(projectiles.members[projectiles.count++]); 
    projectile->type = type;
    projectile->current_grid_coord = vec2(x, y);
    projectile->start_grid_coord = vec2(start_grid_coord.x, start_grid_coord.y);
    projectile->life = 100;
    projectile->damage = damage;
}

void add_shard(float x, float y, float angle, float speed, float radius, float life, Color color) {
    shards.members = resize(&shards, shards.members, sizeof(Shard));

    float angle_in_radians = angle * DEG2RAD;

    Shard* shard = &(shards.members[shards.count++]); 
    shard->position = vec2(x, y);
    shard->life = life;
    shard->velocity = vec2(
        cosf(angle_in_radians) * speed,
        -sinf(angle_in_radians) * speed
    );
    shard->color = color;
    shard->radius = radius;

    shards_set_angles(shard);
}

int check_projectile_collision(const Projectile* projectile) {
    Vector2 pp = projectile->current_grid_coord;
    Vector2 st = projectile->start_grid_coord;

    for (uint32_t i=0; i < enemies.count; i++) {
        const Enemy* enemy = &(enemies.members[i]); 
        Vector2 ep = enemy->current_grid_coord;
        if (ep.y != pp.y) { continue; }

        bool collison_detected = (ep.x + 1 > pp.x) && (ep.x <= st.x);
        if (collison_detected) {
            return i;
        }
    }

    return -1;
}

int check_enemy_defense_collision(Defense* defense) {
    Vector2 dp = defense->current_grid_coord;

    for (uint32_t i=0; i < enemies.count; i++) {
        const Enemy* enemy = &(enemies.members[i]); 
        Vector2 ep = enemy->current_grid_coord;
        if (ep.y != dp.y) { continue; }

        bool collison_detected = (ep.x + 1 > dp.x);
        if (collison_detected) {
            return i;
        }
    }

    return -1;
}

void grab_user_input() {
    mouse_position_on_screen = GetMousePosition();
    mouse_position_on_grid = to_grid_coords(mouse_position_on_screen);
    is_left_mouse_button_released = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
}

void process_user_input() {
    {// handle screen interactions

        // mouse is on the HUD
        if (CheckCollisionPointRec(mouse_position_on_screen, hud.bbox)) {
            // TODO: use more accurate technique to pick a slot in the HUD
            float x_norm = (
                mouse_position_on_screen.x - (hud.bbox.x + hud.clearing)
            );
            hud.hovered_box_index = (int)(x_norm / hud.box_slot_size);
        } else {
            hud.hovered_box_index = -1;
        }
    }

    if (is_left_mouse_button_released) {
        {// handle grid interactions
            int mpx = mouse_position_on_grid.x;
            int mpy = mouse_position_on_grid.y;
            
            if (mpx >= 0 && mpx < grid.width && mpy >= 0 && mpy < grid.height) {
                // TODO: this is hardcoded boundary check so that we cannot add defense in the sand or concrete; needs to be removed once we have dynamic maps
                GridCell cell = grid_cell_at(mpx, mpy);
                if (cell.type == GRASS) {
                    if (hud.selected_box_index != -1) {
                        enum DefenseType type = inventory.defense_items[hud.selected_box_index].type;
                        add_defense(mouse_position_on_grid, type);
                        inventory.defense_items[hud.selected_box_index].last_dispensed = GetTime();
                        inventory.defense_items[hud.selected_box_index].count_dispensed += 1;
                    }
                }
            }
        }

        {// handle screen interactions
            if (hud.hovered_box_index < 0 || hud.hovered_box_index >= inventory.defense_items_size) {
                // if out of bounds, it becomes unselected
                hud.selected_box_index = -1;
            } else {
                DefenseItem item = inventory.defense_items[hud.hovered_box_index];
                bool is_selectable =  GetTime() - item.last_dispensed > item.wait_time_per_dispense_seconds;
                if (is_selectable) {
                    hud.selected_box_index = hud.hovered_box_index;
                } else {
                    hud.selected_box_index = -1;
                }
            }
        }
    }
}

void update() {
    float delta_time = GetFrameTime();

    ADD_ENEMIES {// keep enemy count consistent
        for (uint32_t i=0; i < 1-enemies.count; i++) {
            while (true) {
                int y = rand() % grid.height;
                
                bool is_slot_occupied = false;
                for (uint32_t i=0; i < enemies.count; i++) {
                    Enemy* enemy = &(enemies.members[i]);
                    if (enemy->current_grid_coord.y == y) {
                        is_slot_occupied = true;
                        break;
                    }
                }
                
                if (is_slot_occupied == false) {
                    bool is_type_1 = rand() % 2 == 0;
                    add_enemy(vec2(0, y), is_type_1 ? ENEMY_TYPE_1 : ENEMY_TYPE_2);
                    break;
                }
            }
        }
    }

    // update enemies
    int remove_count = 0;
    for (uint32_t i=0; i < enemies.count; i++) {
        Enemy* enemy = &(enemies.members[i]);

        if (enemy->life <= 0) {
            remove_count++;
            continue;
        }

        // grid.dimension * enemy.distance_per_second * delta_time * enemy.direction
        Vector2 delta_distance = Vector2Multiply(
            Vector2Scale(
                Vector2Multiply(enemy->distance_per_second, vec2(grid.width, grid.height)), 
                delta_time
            ), 
            enemy->direction
        );

        Vector2 interpolated_grid_coord = Vector2Add(
            enemy->current_grid_coord, 
            delta_distance
        );

        enemy->current_screen_coord = to_screen_coords(interpolated_grid_coord);

        int ex = (int)ceilf(interpolated_grid_coord.x);
        int ey = (int)ceilf(interpolated_grid_coord.y);

        GridCell cell = grid_cell_at(ex, ey);

        // this is necessary for depth sorting
        enemy->current_grid_coord.x = Clamp(interpolated_grid_coord.x, 0, grid.width-1);
        enemy->current_grid_coord.y = Clamp(interpolated_grid_coord.y, 0, grid.height-1);

        if (cell.type == WATER) {
            enemy->direction.x = 0;
            enemy->direction.y = -1;
        } else {
            enemy->direction.x = 1;
            enemy->direction.y = 0;
        }

        // make note of the paths they have treaded
        int y_pos = (int)ceilf(enemy->current_grid_coord.y);
        int x_pos = (int)Clamp(interpolated_grid_coord.x, 0, interpolated_grid_coord.x);
        grid_cell_at(x_pos, y_pos).tread_direction = enemy->direction;

    }

    qsort(enemies.members, enemies.count, sizeof(Enemy), compareEnemy);
    enemies.count -= remove_count;

    // update defenses
    remove_count = 0;
    for (uint32_t i=0; i < defenses.count; i++) {
        Defense* defense = &(defenses.members[i]);

        int collided_object_pos = check_enemy_defense_collision(defense);

        if (collided_object_pos != -1) {
            // TODO: defense object dies if it's put behind a living enemy even though it's not on the line of direct attack
            defense->life = 0;
            remove_count += 1;

            Vector2 explosion_center = vec2(
                defense->current_grid_coord.x + 0.5,
                defense->current_grid_coord.y - 0.5
            );

            Vector2 screen_coords = to_screen_coords(explosion_center);

            for (float f=0.0; f < 100.0; f += 0.5) {
                add_shard(
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

        // TODO: projectile generation should be based on charging a certain bar which would be higher/lower depending on the effectiveness of the projectile
        double time_passed = GetTime() - defense->last_attacked;
        double min_wait_time = inventory.defense_items[defense->type - DEFENSE_FIRST].charging_cadence_seconds;
        if (time_passed < min_wait_time) { continue; }

        // TODO: better way to quickly check if enemy is on this lane is needed
        // TODO: only shoot if there's an enemy on the lane
        Vector2 p = defense->current_grid_coord;
        DefenseItem item = inventory.defense_items[defense->type - DEFENSE_FIRST];
        add_projectile(p.x-1, p.y, p, item.projectile_type, item.damage);
        defense->last_attacked = GetTime();
    }

    qsort(defenses.members, defenses.count, sizeof(Defense), compareDefense);
    defenses.count -= remove_count;

    // update projectiles
    remove_count = 0;
    for (uint32_t i=0; i < projectiles.count; i++) {
        Projectile* projectile = &(projectiles.members[i]);
        // TODO: projectiles will move with different speeds
        projectile->current_grid_coord.x -= 2 * delta_time;

        int collided_object_pos = check_projectile_collision(projectile);
        
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

            Vector2 screen_coords = to_screen_coords(explosion_center);

            for (float f=0.0; f < 100.0; f += 0.5) {
                add_shard(
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
            enemy->life -= projectile->damage;
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
    for (uint32_t i=0; i < shards.count; i++) {
        Shard* shard = &(shards.members[i]);
        shard_update(shard);
        if (shard->life <= 0) {
            remove_count++;
        }
    }

    qsort(shards.members, shards.count, sizeof(Shard), compare_shards);
    shards.count -= remove_count;
}

void draw_game_elements() {
    // draw the grid
    float minx = INT_MAX; 
    float miny = INT_MAX;
    float maxx = INT_MIN;
    float maxy = INT_MIN;
    for (int y = 0; y < grid.height; y++){
        for (int x = 0; x < grid.width; x++){
            Vector2 grid_coords = vec2(x, y);
            Vector2 screen_coords = to_screen_coords(grid_coords);
            
            DRAW_GRID_BOUNDING_BOX {
                float xx = screen_coords.x;
                float yy = screen_coords.y;
                if (xx < minx) {minx = xx;}
                if (xx > maxx) {maxx = xx;}

                if (yy < miny) {miny = yy;}
                if (yy > maxy) {maxy = yy;}
            }

            GridCell cell = grid_cell_at(x, y);
            
            Texture2D* ground_texture = &ALL_TEXTURES[TEXTURE_GROUND_GRASS];
            Texture2D* treaded_texture = &ALL_TEXTURES[TEXTURE_GROUND_GRASS_TREADED];

            if (cell.type == PAVEMENT) {
                ground_texture = &ALL_TEXTURES[TEXTURE_GROUND_PAVEMENT];
                treaded_texture = &ALL_TEXTURES[TEXTURE_GROUND_PAVEMENT_TREADED];
            } else if (cell.type == SAND) {
                ground_texture = &ALL_TEXTURES[TEXTURE_GROUND_SAND];
                treaded_texture = &ALL_TEXTURES[TEXTURE_GROUND_SAND_TREADED];
            } else if (cell.type == TARGET) {
                ground_texture = &ALL_TEXTURES[TEXTURE_GROUND_TARGET];
                treaded_texture = &ALL_TEXTURES[TEXTURE_GROUND_TARGET];
            } else if (cell.type == WATER) {
                int sign = (int)(GetTime()) % 2 == 0 ? -1 : 1;
                Texture texture = ALL_TEXTURES[TEXTURE_GROUND_WATER]; 
                DrawTextureV(texture, screen_coords, WHITE);
                Rectangle source = rect(0, 0, sign * texture.width, texture.height);
                DrawTextureRec(texture, source, screen_coords, WHITE);
                continue;
            } else if (cell.type == NONE) {
                // do not render if the cell is of none type
                continue;
            }

            Vector2 tread_dir = cell.tread_direction;

            if (tread_dir.x == 1 && tread_dir.y == 0) {
                // DOWN
                DrawTextureV(*treaded_texture, screen_coords, WHITE);
            } else if (tread_dir.x == 0 && tread_dir.y != 0) {
                // LEFT or RIGHT
                Texture texture = *treaded_texture; 
                DrawTextureV(texture, screen_coords, WHITE);
                Rectangle source = rect(0, 0, -texture.width, texture.height);
                DrawTextureRec(texture, source, screen_coords, WHITE);
            } else {
                // UNTREADED
                DrawTextureV(*ground_texture, screen_coords, WHITE);
            }

            int mpx = mouse_position_on_grid.x;
            int mpy = mouse_position_on_grid.y;

            if (mpy == y && mpx >= 0 && mpx < grid.width) {
                if (mpx == x && cell.type == GRASS) {
                    DrawTextureV(ALL_TEXTURES[TEXTURE_MOUSEOVER], screen_coords, WHITE);
                }
                DrawTextureV(ALL_TEXTURES[TEXTURE_WHITE_FULL_OVERLAY], screen_coords, WHITE);
            }
        }
    }

    // grid bounding box
    Rectangle r = rect(minx,miny,maxx-minx+TILE_WIDTH,maxy-miny+TILE_HEIGHT*2);
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
            
            enum GameObjectType smallest = isometric_view_compare(defense, enemy, projectile);

            if (smallest == GO_DEFENSE) {
                di++;
                
                Vector2 screen_coords = to_screen_coords(defense->current_grid_coord);
                screen_coords.y -= TILE_HEIGHT;
                
                Texture2D texture = GAME_OBJECT_TEXTURES[defense->type];
                DrawTextureV(texture, screen_coords, WHITE);

                // draw charging animation
                float diff = GetTime() - defense->last_attacked;
                double min_wait_time = inventory.defense_items[defense->type - DEFENSE_FIRST].charging_cadence_seconds;
                float pct = diff / min_wait_time;
                BeginScissorMode((int) screen_coords.x, (int) ceil(screen_coords.y + 2 * TILE_HEIGHT * (1 - pct)), TILE_WIDTH, 2 * TILE_HEIGHT * pct);
                    DrawTextureV(ALL_TEXTURES[TEXTURE_WHITE_HALF_OVERLAY], screen_coords, WHITE);
                EndScissorMode();
            } else if (smallest == GO_ENEMY) {
                ei++;

                Texture2D texture = GAME_OBJECT_TEXTURES[enemy->type];

                Vector2 screen_coords = enemy->current_screen_coord;
                screen_coords.y -= TILE_HEIGHT;
                DrawTextureV(texture, screen_coords, WHITE);
            } else if (smallest == GO_PROJECTILE) {
                pi++;

                Texture2D texture = GAME_OBJECT_TEXTURES[projectile->type];
                Vector2 screen_coords = to_screen_coords(projectile->current_grid_coord);
                screen_coords.y -= TILE_HEIGHT;
                DrawTextureV(texture, vec2(screen_coords.x + TILE_WIDTH/4.0, screen_coords.y + TILE_WIDTH/4.0), WHITE);
            }
        }
    }

    {
        for (uint32_t i=0; i < shards.count; i++) {
            Shard* shard = &(shards.members[i]);
            shard_draw(shard);
        }
    }
}

void draw_hud() {
    DrawRectangleRec(hud.bbox, hud.bg_color);

    // draw the inventory
    float box_slot_size = hud.box_slot_size;
    float x = hud.bbox.x;
    float y = hud.bbox.y + (hud.bbox.height - box_slot_size) / 2;
    for (int i=0; i < inventory.defense_items_size;i++) {
        DefenseItem item = inventory.defense_items[i];
        Color color = RED;
        if (!item.is_unlocked) {
            color = ColorAlpha(color, 0.5);
        } else if (i == hud.selected_box_index) {
            color = GREEN;
        } else if (i == hud.hovered_box_index) {
            color = WHITE;
        }
        x += hud.clearing;
        DrawRectangleV(vec2(x, y), vec2(box_slot_size, box_slot_size), color);

        {
            Texture2D texture = GAME_OBJECT_TEXTURES[item.type];
            BeginScissorMode(x, y, box_slot_size, box_slot_size);
                DrawTextureEx(texture, vec2(x, y), 0, box_slot_size / TILE_WIDTH, WHITE);
            EndScissorMode();
            
            float diff =  GetTime() - item.last_dispensed;
            float pct = diff /  item.wait_time_per_dispense_seconds;
            if (pct < 1) {
                // draw charging animation
                BeginScissorMode(x, ceilf(y + box_slot_size * (1 - pct)), box_slot_size, ceilf(box_slot_size * pct));
                    DrawTextureEx(ALL_TEXTURES[TEXTURE_WHITE_BLOCK_OVERLAY], vec2(x, y), 0, box_slot_size / TILE_WIDTH, WHITE);
                EndScissorMode();
            }

        }
        x += box_slot_size;
    }
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

    inventory = (Inventory) {0};
    inventory.defense_items[DEFENSE_TYPE_1-DEFENSE_FIRST] = (DefenseItem){
        .is_unlocked=true, 
        .type=DEFENSE_TYPE_1, 
        .charging_cadence_seconds=4,
        .last_dispensed=GetTime() - 1.5,
        .wait_time_per_dispense_seconds=1.5,
        .projectile_type=PROJECTILE_TYPE_1,
        .damage=10,
    };
    inventory.defense_items[DEFENSE_TYPE_2-DEFENSE_FIRST] = (DefenseItem){
        .is_unlocked=true, 
        .type=DEFENSE_TYPE_2, 
        .charging_cadence_seconds=8,
        .last_dispensed=GetTime() - 10,
        .wait_time_per_dispense_seconds=10,
        .projectile_type=PROJECTILE_TYPE_2,
        .damage=45,
    };
    inventory.defense_items[DEFENSE_TYPE_3-DEFENSE_FIRST] = (DefenseItem){
        .is_unlocked=true, 
        .type=DEFENSE_TYPE_3, 
        .charging_cadence_seconds=10,
        .last_dispensed=GetTime() - 15,
        .wait_time_per_dispense_seconds=15,
        .projectile_type=PROJECTILE_TYPE_3,
        .damage=55,
    };
    inventory.defense_items_size = DEFENSE_COUNT - DEFENSE_FIRST;
}

void init_hud(void) {
    int TOTAL_SLOT_COUNT = 5;
    float hud_height = screen_height * 0.05;
    float box_slot_size = hud_height * 0.8;
    float clearing = (screen_width * 0.5) * 0.01;
    float hud_width = (clearing + box_slot_size) * TOTAL_SLOT_COUNT + clearing;
    hud = (HUD) {
        .total_slot_count = 5,
        .box_slot_size = box_slot_size,
        .bbox = {
            .x = (screen_width - hud_width) / 2.0,
            .y = screen_height * 0.02, // 2% margin
            .width = hud_width,
            .height = hud_height,
        },
        .bg_color = ColorAlpha(GRAY, 0.5),
        .clearing = clearing,
        .hovered_box_index = -1,
        .selected_box_index = -1,
    };
}

static inline bool colors_equal(Color a, Color b) {
    return (a.r == b.r) &&
           (a.g == b.g) &&
           (a.b == b.b) &&
           (a.a == b.a);
}

void init_grid(void) {
    grid = (Grid) {0};
    grid.width = 30;
    grid.height = 30;
    grid.cells = malloc(sizeof(GridCell) * grid.height * grid.width);

    Image image = LoadImage("./assets/Levels/lvl-30x30.png");
    Color* colors = LoadImageColors(image);
    Color COLOR_SAND = (Color){ 255, 125, 0, 255 };
    Color COLOR_WATER = (Color){ 0, 0, 255, 255 };
    Color COLOR_PAVEMENT = (Color){ 133, 133, 133, 255 };
    Color COLOR_GRASS = (Color){ 0, 255, 0, 255 };
    Color COLOR_EMPTY = (Color){ 255, 255, 255, 0 };
    Color COLOR_TARGET = (Color) {255, 255, 0, 255};
    
    for (uint16_t y = 0; y < grid.height; y++) {
        for (uint16_t x = 0; x < grid.width; x++) {
            Color color = colors[y * grid.width + x];
            GridCell* cell = &grid_cell_at(x, y);
            if (colors_equal(color, COLOR_SAND)) {
                cell->type = SAND;
            } else if (colors_equal(color, COLOR_WATER)) {
                cell->type = WATER;
            } else if (colors_equal(color, COLOR_PAVEMENT)) {
                cell->type = PAVEMENT;
            } else if (colors_equal(color, COLOR_GRASS)) {
                cell->type = GRASS;
            } else if (colors_equal(color, COLOR_EMPTY)) {
                cell->type = NONE;
            } else if (colors_equal(color, COLOR_TARGET)) {
                cell->type = TARGET;
            } else {
                DEBUG_PRINT("UNKNOWN COLOR! {%d, %d, %d, %d}\n", color.r, color.g, color.b, color.a);
                exit(EXIT_FAILURE);
            }
        }
    }

    UnloadImageColors(colors);
    UnloadImage(image);
}

int main(void){
    srand(time(NULL));

    SetTraceLogLevel(LOG_NONE);
    SetConfigFlags(FLAG_VSYNC_HINT);
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    SetConfigFlags(FLAG_FULLSCREEN_MODE);
    // SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    SetTargetFPS(30);

    init_grid();
    init();

    InitWindow(0, 0, "blockwave");

    screen_width = GetScreenWidth();
    screen_height = GetScreenHeight();

    //  2560 x 1440
    TILE_HEIGHT = ceilf((32.0 * screen_height) / 1440);
    TILE_WIDTH = 2 * TILE_HEIGHT;

    DEBUG_PRINT("[%.2f, %.2f]\n", TILE_WIDTH, TILE_HEIGHT);

    float iso_width = (grid.height + grid.width) * (TILE_WIDTH / 2.0);
    float iso_height = (grid.height + grid.width) * (TILE_HEIGHT / 2);
    HORIZONTAL_OFFSET = (screen_width - iso_width) / 2.0 + grid.height * TILE_WIDTH / 2.0;
    VERTICAL_OFFSET = (screen_height - iso_height) / 2.0;

    ALL_TEXTURES[TEXTURE_GROUND_GRASS] = loadTextureFromImageResized("Blocks/blocks_1.png", TILE_WIDTH, TILE_WIDTH);
    ALL_TEXTURES[TEXTURE_GROUND_GRASS_TREADED] = loadTextureFromImageResized("Blocks/blocks_1_treaded.png", TILE_WIDTH, TILE_WIDTH);
    ALL_TEXTURES[TEXTURE_GROUND_PAVEMENT] = loadTextureFromImageResized("Blocks/blocks_56.png", TILE_WIDTH, TILE_WIDTH);
    ALL_TEXTURES[TEXTURE_GROUND_PAVEMENT_TREADED] = loadTextureFromImageResized("Blocks/blocks_56_treaded.png", TILE_WIDTH, TILE_WIDTH);
    ALL_TEXTURES[TEXTURE_GROUND_SAND] = loadTextureFromImageResized("Blocks/blocks_32.png", TILE_WIDTH, TILE_WIDTH);
    ALL_TEXTURES[TEXTURE_GROUND_SAND_TREADED] = loadTextureFromImageResized("Blocks/blocks_32_treaded.png", TILE_WIDTH, TILE_WIDTH);
    ALL_TEXTURES[TEXTURE_GROUND_TARGET] = loadTextureFromImageResized("Blocks/blocks_100.png", TILE_WIDTH, TILE_WIDTH);
    ALL_TEXTURES[TEXTURE_GROUND_WATER] = loadTextureFromImageResized("Blocks/blocks_69.png", TILE_WIDTH, TILE_WIDTH);
    ALL_TEXTURES[TEXTURE_MOUSEOVER] = loadTextureFromImageResized("Blocks/blocks_99.png", TILE_WIDTH, TILE_WIDTH);
    ALL_TEXTURES[TEXTURE_WHITE_FULL_OVERLAY] = loadTextureFromImageResized("Blocks/overlay.png", TILE_WIDTH, TILE_WIDTH);
    ALL_TEXTURES[TEXTURE_WHITE_HALF_OVERLAY] = loadTextureFromImageResized("Blocks/half_overlay.png", TILE_WIDTH, TILE_WIDTH);
    ALL_TEXTURES[TEXTURE_ENEMY_TYPE_1] = loadTextureFromImageResized("Blocks/blocks_30.png", TILE_WIDTH, TILE_WIDTH);
    ALL_TEXTURES[TEXTURE_ENEMY_TYPE_2] = loadTextureFromImageResized("Blocks/blocks_31.png", TILE_WIDTH, TILE_WIDTH);
    ALL_TEXTURES[TEXTURE_DEFENDER_TYPE_1] = loadTextureFromImageResized("Blocks/blocks_24.png", TILE_WIDTH, TILE_WIDTH);
    ALL_TEXTURES[TEXTURE_DEFENDER_TYPE_2] = loadTextureFromImageResized("Blocks/blocks_58.png", TILE_WIDTH, TILE_WIDTH);
    ALL_TEXTURES[TEXTURE_DEFENDER_TYPE_3] = loadTextureFromImageResized("Blocks/blocks_14.png", TILE_WIDTH, TILE_WIDTH);
    ALL_TEXTURES[TEXTURE_PROJECTILE_1] = loadTextureFromImageResized("Custom/projectile_blue.png", TILE_WIDTH / 4, TILE_WIDTH / 4);
    ALL_TEXTURES[TEXTURE_PROJECTILE_2] = loadTextureFromImageResized("Custom/projectile_orange.png", TILE_WIDTH / 2, TILE_WIDTH / 2);
    ALL_TEXTURES[TEXTURE_PROJECTILE_3] = loadTextureFromImageResized("Custom/projectile_red.png", TILE_WIDTH / 2, TILE_WIDTH / 2);
    ALL_TEXTURES[TEXTURE_RED] = loadTextureFromImageResized("Blocks/blocks_96.png", TILE_WIDTH, TILE_WIDTH);

    Image image = GenImageColor(TILE_WIDTH, TILE_HEIGHT * 2, ColorAlpha(WHITE, 0.5));
    ALL_TEXTURES[TEXTURE_WHITE_BLOCK_OVERLAY] = LoadTextureFromImage(image);
    UnloadImage(image);

    GAME_OBJECT_TEXTURES[ENEMY_TYPE_1] = ALL_TEXTURES[TEXTURE_ENEMY_TYPE_1];
    GAME_OBJECT_TEXTURES[ENEMY_TYPE_2] = ALL_TEXTURES[TEXTURE_ENEMY_TYPE_2];
    GAME_OBJECT_TEXTURES[DEFENSE_TYPE_1] = ALL_TEXTURES[TEXTURE_DEFENDER_TYPE_1];
    GAME_OBJECT_TEXTURES[DEFENSE_TYPE_2] = ALL_TEXTURES[TEXTURE_DEFENDER_TYPE_2];
    GAME_OBJECT_TEXTURES[DEFENSE_TYPE_3] = ALL_TEXTURES[TEXTURE_DEFENDER_TYPE_3];
    GAME_OBJECT_TEXTURES[PROJECTILE_TYPE_1] = ALL_TEXTURES[TEXTURE_PROJECTILE_1];
    GAME_OBJECT_TEXTURES[PROJECTILE_TYPE_2] = ALL_TEXTURES[TEXTURE_PROJECTILE_2];
    GAME_OBJECT_TEXTURES[PROJECTILE_TYPE_3] = ALL_TEXTURES[TEXTURE_PROJECTILE_3];

    init_hud();

    while (!WindowShouldClose())
    {
        grab_user_input();
        process_user_input();
        update();

        BeginDrawing();
            ClearBackground(RAYWHITE);
            draw_game_elements();
            draw_hud();
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
        free(grid.cells);
        DEBUG_PRINT("freed grid cells\n");
        
        DEBUG_PRINT("unloading textures...\n");
        for(uint8_t i=0; i<TEXTURE_COUNT; i++) {
            UnloadTexture(ALL_TEXTURES[i]);
        }
        DEBUG_PRINT("unloading textures...done!\n");

    }

    CloseWindow();

    return 0;
}