#define MAX_FIGURES 40

enum { HINT_NONE, HINT_LAUNCH, HINT_ENTER, HINT_TALK };

typedef enum {
    SCENE_TITLE,
    SCENE_PLAY,
    SCENE_STAR_GET,
    SCENE_MISSION_SELECT,
    SCENE_GAME_OVER,
    SCENE_PAUSE,
    SCENE_GALAXY_SELECT,
    SCENE_GALAXY_MAP,
} SceneId;

typedef struct {
    int stage_index;
    int scenario;
    Stage stage;
    fx time;
    uint32_t tick;
    uint32_t tick_accumulator;
    Camera camera;
    bool camera_underwater;
    bool camera_orbit_held;
    Player player;
    Actor actors[MAX_ACTORS];
    int actor_count;
    Figure* figures[MAX_FIGURES];
    int figure_count;
    int view_distance;
    int camera_zoom;

    SceneId scene;
    int scene_timer;
    int fade;
    int fade_target;

    int coins, star_bits, lives, purple_coins;
    int hint;
    KeyState keys;
    int selected_galaxy, selected_mission;
    int stars_total;
    int stage_tick;
    uint32_t stars_collected[8];
    int restart_id, restart_zone;
    int stage_index_loaded, scenario_loaded;
} Game;

enum {
    DEBUG_NO_DAMAGE = 1,
    DEBUG_INFINITE_AIR = 2,
    DEBUG_INFINITE_BEE = 4,
    DEBUG_MAGNET = 8,
    DEBUG_PULL_ASSIST = 16,
    DEBUG_FLIGHT_POINTER = 32,
    DEBUG_MUSIC = 64,
    DEBUG_SOUND = 128,
    DEBUG_FREEZE_ENEMIES = 256,
    DEBUG_SKIP_INTRO = 512,
    DEBUG_FALLING_DROPS = 1024,
    DEBUG_LARGE_FALLING_DROPS = 2048,
    DEBUG_SHOW_FPS = 4096
};

typedef struct {
    fx near;
    bool far;
    fx flat_depth;
    int figure_lod;
    bool flat_small;
    int actors;
    fx actor_range;
    fx figure_depth;
    int actors_extra;
    int actor_budget;
} ViewMode;

#define VIEW_MODE_DEFAULT 2
#define VIEW_MAX_ACTORS   24
#define VIEW_MODE_COUNT   5
#define CAMERA_ZOOM_COUNT 4

static Game g_game EWRAM_BSS;
static uint16_t g_debug_options EWRAM_BSS;
static const fx camera_zooms[CAMERA_ZOOM_COUNT];

static inline const ViewMode* view_mode(void);
