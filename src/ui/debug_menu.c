enum {
    DEBUG_PAGE_HOME,
    DEBUG_PAGE_LEVELS,
    DEBUG_PAGE_MISSIONS,
    DEBUG_PAGE_POINTS,
    DEBUG_PAGE_OPTIONS,
    DEBUG_PAGE_POWERUPS,
    DEBUG_PAGE_PERF
};
typedef struct {
    int16_t selected, scroll;
    uint16_t last_direction;
    uint8_t page, level, repeat;
} DebugMenu;
static DebugMenu g_debug_menu EWRAM_BSS;

typedef struct {
    int16_t selected, scroll;
    uint16_t options;
    uint8_t page, level;
    int32_t stage, scenario, view, zoom;
} DebugMenuImageKey;
_Static_assert(sizeof(DebugMenuImageKey) == 24, "DebugMenuImageKey must have no padding");
static EWRAM_BSS struct {
    DebugMenuImageKey key;
    uint8_t pages;
} g_debug_menu_image;
static void debug_menu_invalidate(void){ g_debug_menu_image.pages = 0; }

static const char* const debug_home[] = {"Resume",
                                         "Choose level",
                                         "Jump to planet / point",
                                         "Restart mission",
                                         "Test options",
                                         "Restore health",
                                         "Add 100 Star Bits",
                                         "Power-ups",
                                         "Return to Observatory",
                                         "Performance"};
static const char* const debug_option_names[] = {"Ignore enemy damage",
                                                 "Unlimited air",
                                                 "Unlimited Bee flight",
                                                 "Star Bit magnet",
                                                 "Pull Star assistance",
                                                 "Pointer during flight",
                                                 "Music",
                                                 "Sound effects",
                                                 "Freeze enemies",
                                                 "Skip mission intro",
                                                 "Falling drops",
                                                 "Large bursts fall",
                                                 "Show FPS",
                                                 "Render detail",
                                                 "Camera distance",
                                                 "Render style",
                                                 "Line colour"};
static const uint16_t debug_option_bits[] = {
    DEBUG_NO_DAMAGE,      DEBUG_INFINITE_AIR,   DEBUG_INFINITE_BEE,  DEBUG_MAGNET,
    DEBUG_PULL_ASSIST,    DEBUG_FLIGHT_POINTER, DEBUG_MUSIC,         DEBUG_SOUND,
    DEBUG_FREEZE_ENEMIES, DEBUG_SKIP_INTRO,     DEBUG_FALLING_DROPS, DEBUG_LARGE_FALLING_DROPS,
    DEBUG_SHOW_FPS};
#define DEBUG_OPTION_TOGGLE_COUNT ((int)(sizeof debug_option_bits / sizeof debug_option_bits[0]))

static const char* const debug_powerup_names[] = {
    "Normal Mario", "Bee Mario", "Fire Mario", "Ice Mario", "Rainbow Star", "Life Mushroom", "1-Up Mushroom"};
static void debug_powerup(int row){
    switch(row){
    case 0:
        mario_power_end();
        bee_end();
        g_mario_star_power = 0;
        break;
    case 1:
        mario_power_end();
        if(!bee_active()){
            bee_start();
            audio_play_effect(SFX_BEE_MORPH, 4);
        }
        break;
    case 2:
    case 3: mario_power_start(row == 2 ? 2 : 3, -1); break;
    case 4:
        mario_power_end();
        bee_end();
        g_mario_star_power = 1200;
        audio_play_effect(SFX_RAINBOW_POWER_UP_2, 4);
        break;
    case 5: mario_lifeup_pickup(); break;
    case 6:
        g_game.lives++;
        audio_play_effect(SFX_ONEUP, 4);
        break;
    }
    g_game.scene = SCENE_PLAY;
}
static const char* const debug_perf_names[] = {"Frame rate",     "Frame time",    "Game logic",
                                               "Sky",            "Culling",       "Vertex transform",
                                               "Triangle setup", "Far scenery",   "Characters",
                                               "Level drawing",  "Other",         "Pixels drawn",
                                               "Overdraw"};
static const uint8_t debug_perf_steps[] = {PERF_LOGIC,     PERF_SKY,     PERF_CULL,     PERF_TRANSFORM,
                                           PERF_EMIT,      PERF_FAR,     PERF_FIGURES,  PERF_LEVEL,
                                           PERF_OTHER};
static char* debug_append(char* out, const char* s){
    while(*s) *out++ = *s++;
    *out = 0;
    return out;
}
static char* debug_append_number(char* out, uint32_t value){
    char rev[10];
    int n = 0;
    do {
        rev[n++] = (char)('0' + value % 10);
        value /= 10;
    } while(value && n < 10);
    while(n) *out++ = rev[--n];
    *out = 0;
    return out;
}
static char* debug_append_hundredths(char* out, uint32_t hundredths){
    out = debug_append_number(out, hundredths / 100);
    *out++ = '.';
    *out++ = (char)('0' + hundredths / 10 % 10);
    *out++ = (char)('0' + hundredths % 10);
    *out = 0;
    return out;
}
static void debug_perf_value(char* out, int row){
    uint32_t frame = g_perf_frame_us;
    if(!frame){
        debug_append(out, "-");
        return;
    }
    if(row == 0){
        debug_append(debug_append_hundredths(out, (100000000u + frame / 2) / frame), " fps");
    } else if(row == 1){
        debug_append(debug_append_hundredths(out, (frame + 5) / 10), " ms");
    } else if(row == 11){
        debug_append_number(out, g_perf_pixels);
    } else if(row == 12){
        debug_append(debug_append_hundredths(out, (g_perf_pixels * 100 + PERF_FRAME_PIXELS / 2) / PERF_FRAME_PIXELS),
                     "x");
    } else {
        uint32_t us = g_perf_us[debug_perf_steps[row - 2]];
        out = debug_append(debug_append_hundredths(out, (us + 5) / 10), " ms ");
        debug_append(debug_append_number(out, (us * 100 + frame / 2) / frame), "%");
    }
}
static const char* const debug_view_names[] = {"Full", "High", "Default", "Near", "Short"};
static const char* const debug_zoom_names[] = {"Farthest", "Far", "Medium", "Close"};
static const char* const debug_render_style_names[] = {"Normal", "Wireframe", "Hidden line", "Vertices"};
static const char* const debug_line_colour_names[] = {"Green", "White"};

static void debug_page(int page){
    debug_menu_invalidate();
    g_debug_menu.page = page;
    g_debug_menu.selected = g_debug_menu.scroll = 0;
    g_debug_menu.repeat = 0;
    g_debug_menu.last_direction = 0;
}
static void debug_menu_open(void){ debug_page(DEBUG_PAGE_HOME); }

static const char* debug_zone_name(int stage, int zone){
    for(unsigned i = 0; i < sizeof debug_zone_names / sizeof debug_zone_names[0]; i++){
        const DebugZoneName* n = &debug_zone_names[i];
        if(n->stage == stage && n->zone == zone) return n->name;
    }
    return "Planet";
}
static const char* debug_start_name(const StageStart* start){
    int zone = start->zone;
    if(zone == 0 && start->id > 0){
        const Stage* st = &g_game.stage;
        uint64_t best = (uint64_t)FX(1200) * FX(1200);
        v3 pos = V3(start->position[0], start->position[1], start->position[2]);
        for(unsigned i = 0; i < st->header->start_count; i++){
            const StageStart* other = &st->starts[i];
            if(!other->zone || !(other->scenarios & gravity_scenario_bit())) continue;
            uint64_t distance = v3_length_squared64(
                v3_sub(pos, V3(other->position[0], other->position[1], other->position[2])));
            if(distance < best){
                best = distance;
                zone = other->zone;
            }
        }
    }
    return debug_zone_name(g_game.stage_index, zone);
}
static const char* debug_object_name(int type){
    switch(type){
    case OBJ_LAUNCHSTAR: return "Launch Star";
    case OBJ_SLINGSTAR: return "Sling Star";
    case OBJ_GCAPTURETARGET: return "Pull Star";
    case OBJ_EARTHENPIPE: return "Warp pipe";
    case OBJ_POWERSTAR: return "Power Star";
    case OBJ_MARIOLAUNCHER: return "Mario Launcher";
    case OBJ_SHUTTERDOORE: return "Shutter Door";
    case OBJ_BOSSKAMECK2: return "Kamella";
    case OBJ_PHANTOMFLOATERA:
    case OBJ_PHANTOMFLOATERB:
    case OBJ_PHANTOMFLOATERC: return "Floating Crate";
    case OBJ_CANDLESTANDICEVOLCANO: return "Candlestand";
    case OBJ_POLESQUARE: return "Climbable Pole";
    case OBJ_DINOPACKUN: return "Dino Piranha";
    case OBJ_HAMMERHEADPACKUN: return "Piranha Plant";
    case OBJ_OTAKING: return "King Kaliente";
    case OBJ_BOSSSTINKBUG: return "Bugaboom";
    case OBJ_HONEYQUEEN: return "Queen Bee";
    case OBJ_WARPPOD: return "Warp pod";
    case OBJ_DOME: return "Dome entrance";
    default: return NULL;
    }
}

static int debug_point(int ordinal, int* start, int* object){
    const Stage* st = &g_game.stage;
    unsigned scenario = gravity_scenario_bit();
    int n = 0;
    if(start) *start = -1;
    if(object) *object = -1;
    for(unsigned i = 0; i < st->header->start_count; i++){
        if(!(st->starts[i].scenarios & scenario)) continue;
        if(n++ == ordinal){
            if(start) *start = (int)i;
            return n;
        }
    }
    for(unsigned i = 0; i < st->header->object_count; i++){
        const StageObject* o = &st->objects[i];
        if(!(o->scenarios & scenario) || !debug_object_name(o->type)) continue;
        if(n++ == ordinal){
            if(object) *object = (int)i;
            return n;
        }
    }
    return n;
}
static int debug_level(int ordinal){
    int n = 0;
    for(int i = 0; i < STAGE_COUNT; i++){
        const LevelHooks* level = level_hooks(i);
        if(level->galaxy_stage != i || (level->features & LEVEL_HUB_ROOM) || !stage_blobs[i]) continue;
        if(n++ == ordinal) return i;
    }
    return ordinal < 0 ? n : -1;
}
static int debug_menu_count(void){
    switch(g_debug_menu.page){
    case DEBUG_PAGE_LEVELS: return debug_level(-1);
    case DEBUG_PAGE_MISSIONS: {
        int galaxy = galaxy_for_stage(g_debug_menu.level);
        return galaxy >= 0 ? galaxies[galaxy].mission_count : 1;
    }
    case DEBUG_PAGE_POINTS: return debug_point(-1, NULL, NULL);
    case DEBUG_PAGE_OPTIONS: return sizeof debug_option_names / sizeof debug_option_names[0];
    case DEBUG_PAGE_POWERUPS: return sizeof debug_powerup_names / sizeof debug_powerup_names[0];
    case DEBUG_PAGE_PERF: return sizeof debug_perf_names / sizeof debug_perf_names[0];
    default: return sizeof debug_home / sizeof debug_home[0];
    }
}
static void debug_start_level(int stage, int mission){
    g_game.restart_id = -1;
    g_game.scene = SCENE_PLAY;
    game_load_stage(level_mission_stage(stage, mission), 0, stage == STAGE_OBSERVATORY ? 0 : mission);
    if(g_debug_options & DEBUG_SKIP_INTRO) scenario_intro_end();
}
static void debug_reload_level(int stage, int mission){
    g_game.restart_id = -1;
    g_game.scene = SCENE_PLAY;
    game_reload_keep_health(level_mission_stage(stage, mission), 0, stage == STAGE_OBSERVATORY ? 0 : mission);
    if(g_debug_options & DEBUG_SKIP_INTRO) scenario_intro_end();
}

static void debug_jump_point(int ordinal){
    int si, oi;
    debug_point(ordinal, &si, &oi);
    if(si < 0 && oi < 0) return;

    int stage = g_game.stage_index, mission = game_current_scenario();
    debug_reload_level(stage, mission);
    scenario_intro_end();
    flight_reset();
    talk_reset();
    pointer_reset();
    const Stage* st = &g_game.stage;
    v3 pos, up, front;
    int camera = -1;
    if(si >= 0){
        const StageStart* s = &st->starts[si];
        pos = V3(s->position[0], s->position[1], s->position[2]);
        up = v3_from_i16(s->up);
        front = v3_from_i16(s->front);
        camera = s->camera;
    } else {
        const StageObject* o = &st->objects[oi];
        up = v3_from_i16(o->up);
        front = v3_from_i16(o->front);
        pos = v3_add(V3(o->position[0], o->position[1], o->position[2]),
                     v3_add(v3_scale14(up, FX(220)), v3_scale14(front, FX(-400))));
        RayHit hit;
        if(collision_ray(v3_add(pos, v3_scale14(up, FX(600))), v3_sub(pos, v3_scale14(up, FX(2000))), &hit,
                         COL_DEATH) &&
           v3_dot14(hit.normal, up) > R14(.7))
            pos = v3_add(hit.point, v3_scale14(up, FX(12)));
    }
    mario_reset(pos, up, front);
    camera_snap_behind(&g_game.camera, &g_game.player);
    camera_galaxy_stage_start(camera);
    g_game.fade = g_game.fade_target = 0;
}

static void debug_menu_choose(int direction){
    debug_menu_invalidate();
    DebugMenu* m = &g_debug_menu;
    int row = m->selected;
    if(m->page == DEBUG_PAGE_PERF) return;
    if(m->page == DEBUG_PAGE_LEVELS){
        m->level = (uint8_t)debug_level(row);
        debug_page(DEBUG_PAGE_MISSIONS);
        return;
    }
    if(m->page == DEBUG_PAGE_MISSIONS){
        debug_start_level(m->level, row + 1);
        return;
    }
    if(m->page == DEBUG_PAGE_POINTS){
        debug_jump_point(row);
        return;
    }
    if(m->page == DEBUG_PAGE_POWERUPS){
        debug_powerup(row);
        return;
    }
    if(m->page == DEBUG_PAGE_OPTIONS){
        if(row < DEBUG_OPTION_TOGGLE_COUNT){
            g_debug_options ^= debug_option_bits[row];
            if(row == 6){
                if(g_debug_options & DEBUG_MUSIC) audio_play_stage_music(game_stage_music());
                else {
                    audio_stop_music();
                    audio_stop_sub_music();
                }
            }
            if(row == 7 && !(g_debug_options & DEBUG_SOUND)){
                audio_stop_effects();
            }
        } else if(row == DEBUG_OPTION_TOGGLE_COUNT)
            g_game.view_distance = (g_game.view_distance + direction + VIEW_MODE_COUNT) % VIEW_MODE_COUNT;
        else if(row == DEBUG_OPTION_TOGGLE_COUNT + 1)
            camera_set_zoom(&g_game.camera,
                            (g_game.camera_zoom + direction + CAMERA_ZOOM_COUNT) % CAMERA_ZOOM_COUNT);
        else if(row == DEBUG_OPTION_TOGGLE_COUNT + 2)
            g_render_style = (uint8_t)((g_render_style + direction + RENDER_STYLE_COUNT) % RENDER_STYLE_COUNT);
        else
            g_wire_line_colour ^= 1;
        return;
    }
    switch(row){
    case 0: g_game.scene = SCENE_PLAY; break;
    case 1: debug_page(DEBUG_PAGE_LEVELS); break;
    case 2: debug_page(DEBUG_PAGE_POINTS); break;
    case 3: debug_reload_level(g_game.stage_index, game_current_scenario()); break;
    case 4: debug_page(DEBUG_PAGE_OPTIONS); break;
    case 5:
        g_game.player.health = mario_lifeup_max();
        mario_swim_recover();
        break;
    case 6: g_game.star_bits = fx_min(g_game.star_bits + 100, 9999); break;
    case 7: debug_page(DEBUG_PAGE_POWERUPS); break;
    case 8: debug_start_level(STAGE_OBSERVATORY, 0); break;
    case 9: debug_page(DEBUG_PAGE_PERF); break;
    }
}
static void debug_menu_tick(const KeyState* keys){
    DebugMenu* m = &g_debug_menu;
    if(keys->pressed & KEY_START){
        g_game.scene = SCENE_PLAY;
        return;
    }
    if(keys->pressed & KEY_B){
        if(m->page == DEBUG_PAGE_HOME) g_game.scene = SCENE_PLAY;
        else debug_page(m->page == DEBUG_PAGE_MISSIONS ? DEBUG_PAGE_LEVELS : DEBUG_PAGE_HOME);
        return;
    }
    int count = debug_menu_count();
    if(!count) return;
    uint16_t direction = keys->held & (KEY_UP | KEY_DOWN);
    bool move = (keys->pressed & direction) != 0;
    if(direction && direction == m->last_direction){
        if(m->repeat < 36) m->repeat++;
        else if(++m->repeat >= 42){
            m->repeat = 36;
            move = true;
        }
    } else m->repeat = 0;
    m->last_direction = direction;
    if(move){
        m->selected = (m->selected + (direction & KEY_UP ? -1 : 1) + count) % count;
        audio_play_effect(SFX_MENU_MOVE, 1);
    }
    if(keys->pressed & (KEY_A | KEY_LEFT | KEY_RIGHT)){
        if((keys->pressed & KEY_A) || m->page == DEBUG_PAGE_OPTIONS){
            audio_play_effect(SFX_MENU_SELECT, 4);
            debug_menu_choose(keys->pressed & KEY_LEFT ? -1 : 1);
        }
    }
}

static void debug_pixel(uint16_t* fb, int x, int y, int color){
    if(x < 0 || x >= 240 || y < 0 || y >= 160) return;
    int i = y * 240 + x;
    uint16_t* p = fb + (i >> 1);
    *p = (uint16_t)((i & 1) ? ((*p & 255) | (color << 8)) : ((*p & 0xff00) | color));
}
static int debug_text(uint16_t* fb, int x, int y, const char* s, const uint8_t colors[4], int limit){
    for(; *s; s++){
        unsigned c = (unsigned char)*s;
        if(c < 32 || c > 126) c = '?';
        c -= 32;
        if(x + debug_font_advance[c] > limit) break;
        for(int gy = 0; gy < 11; gy++)
            for(int gx = 0; gx < debug_font_width[c]; gx++){
                int alpha = debug_font[c][gy][gx];
                if(alpha) debug_pixel(fb, x + gx, y + gy, colors[alpha]);
            }
        x += debug_font_advance[c];
    }
    return x;
}
static void debug_number(char* out, int value){
    char rev[8];
    int n = 0;
    do {
        rev[n++] = (char)('0' + value % 10);
        value /= 10;
    } while(value && n < 7);
    for(int i = 0; i < n; i++) out[i] = rev[n - 1 - i];
    out[n] = 0;
}
static void debug_menu_draw(uint8_t* page_bytes){
    uint16_t* framebuffer = (uint16_t*)page_bytes;
    DebugMenu* m = &g_debug_menu;
    int count = debug_menu_count();

    bool has_subtitle = m->page != DEBUG_PAGE_HOME;
    int visible = has_subtitle ? 8 : 9, top = has_subtitle ? 39 : 26;
    if(m->selected < m->scroll) m->scroll = m->selected;
    if(m->selected >= m->scroll + visible) m->scroll = m->selected - (visible - 1);
    DebugMenuImageKey key = {m->selected,       m->scroll,          g_debug_options, m->page,
                             m->level,          g_game.stage_index, g_game.scenario, g_game.view_distance,
                             g_game.camera_zoom};
    uint8_t page = framebuffer == (uint16_t*)0x06000000u ? 1 : framebuffer == (uint16_t*)0x0600a000u ? 2 : 0;
    if(memcmp(&key, &g_debug_menu_image.key, sizeof key)){
        g_debug_menu_image.key = key;
        debug_menu_invalidate();
    }
    if(page && (g_debug_menu_image.pages & page)){
        hud_begin();
        hud_end();
        hud_set_fade(0, false);
        return;
    }

    for(int y = 0; y < 160; y++) dma3_copy32(framebuffer + y * 120, debug_menu_background + y * 240, 240 / 4);
    uint8_t white[4] = {0, palette_nearest(10, 15, 20), palette_nearest(20, 25, 28),
                        palette_nearest(31, 31, 31)};
    uint8_t blue[4] = {0, palette_nearest(8, 16, 20), palette_nearest(12, 24, 29),
                       palette_nearest(19, 30, 31)};
    const char* title = m->page == DEBUG_PAGE_HOME       ? "Galaxy Test Menu"
                        : m->page == DEBUG_PAGE_LEVELS   ? "Choose Level"
                        : m->page == DEBUG_PAGE_MISSIONS ? debug_stage_names[m->level]
                        : m->page == DEBUG_PAGE_POINTS   ? "Planet / Progression Point"
                        : m->page == DEBUG_PAGE_POWERUPS ? "Power-ups"
                        : m->page == DEBUG_PAGE_PERF     ? "Performance"
                                                         : "Test Options";
    debug_text(framebuffer, 12, 7, title, white, 228);
    const char* subtitle = m->page == DEBUG_PAGE_HOME       ? NULL
                           : m->page == DEBUG_PAGE_POINTS   ? "Jump reloads the current mission"
                           : m->page == DEBUG_PAGE_POWERUPS ? "Applied as if collected"
                           : m->page == DEBUG_PAGE_PERF     ? "Average of the last 0.5 s of play"
                           : m->page == DEBUG_PAGE_MISSIONS
                               ? (debug_menu_count() > 1 ? "Choose a mission" : "Start this level")
                               : debug_stage_names[g_game.stage_index];
    if(subtitle) debug_text(framebuffer, 12, 21, subtitle, blue, 228);
    for(int row = m->scroll; row < count && row < m->scroll + visible; row++){
        int y = top + (row - m->scroll) * 13;
        const uint8_t* color = row == m->selected ? white : blue;
        if(row == m->selected) debug_text(framebuffer, 5, y, ">", white, 12);
        const char* label = NULL;
        char number[8];
        int suffix = -1;
        if(m->page == DEBUG_PAGE_HOME) label = debug_home[row];
        else if(m->page == DEBUG_PAGE_LEVELS) label = debug_stage_names[debug_level(row)];
        else if(m->page == DEBUG_PAGE_MISSIONS){
            label = debug_mission_names[m->level][row];
            if(!label) label = "Enter Observatory";
        } else if(m->page == DEBUG_PAGE_OPTIONS) label = debug_option_names[row];
        else if(m->page == DEBUG_PAGE_POWERUPS) label = debug_powerup_names[row];
        else if(m->page == DEBUG_PAGE_PERF) label = debug_perf_names[row];
        else {
            {
                int si, oi;
                debug_point(row, &si, &oi);
                if(si >= 0){
                    label = debug_start_name(&g_game.stage.starts[si]);
                    suffix = g_game.stage.starts[si].id;
                } else {
                    label = debug_object_name(g_game.stage.objects[oi].type);
                    suffix = g_game.stage.objects[oi].id;
                }
            }
        }
        int limit = m->page == DEBUG_PAGE_OPTIONS ? 175 : m->page == DEBUG_PAGE_PERF ? 130 : 202;
        int x = debug_text(framebuffer, 16, y, label ? label : "Point", color, limit);
        if(suffix >= 0){
            debug_number(number, suffix);
            debug_text(framebuffer, x + 4, y, number, color, 228);
        }
        if(m->page == DEBUG_PAGE_OPTIONS){
            const char* value = row < DEBUG_OPTION_TOGGLE_COUNT
                                    ? (g_debug_options & debug_option_bits[row] ? "ON" : "OFF")
                                : row == DEBUG_OPTION_TOGGLE_COUNT     ? debug_view_names[g_game.view_distance]
                                : row == DEBUG_OPTION_TOGGLE_COUNT + 1 ? debug_zoom_names[g_game.camera_zoom]
                                : row == DEBUG_OPTION_TOGGLE_COUNT + 2 ? debug_render_style_names[g_render_style]
                                                                       : debug_line_colour_names[g_wire_line_colour];
            debug_text(framebuffer, 176, y, value, white, 235);
        }
        if(m->page == DEBUG_PAGE_PERF){
            char value[24];
            debug_perf_value(value, row);
            debug_text(framebuffer, 132, y, value, white, 235);
        }
    }
    char number[8];
    debug_number(number, m->selected + 1);
    int x = debug_text(framebuffer, 12, 143, number, blue, 40);
    x = debug_text(framebuffer, x, 143, "/", blue, 47);
    debug_number(number, count);
    debug_text(framebuffer, x, 143, number, blue, 75);
    g_debug_menu_image.pages |= page;
    hud_begin();
    hud_end();
    hud_set_fade(0, false);
}
