static const GalaxyInfo galaxies[GALAXY_COUNT] = {
    [GALAXY_HONEYHIVE] = {HUD_ART_T_HONEYHIVE_NAME,
                          STAGE_HONEYHIVE,
                          6,
                          {HUD_ART_T_HONEYHIVE_M1, HUD_ART_T_HONEYHIVE_M2, HUD_ART_T_HONEYHIVE_M3,
                           HUD_ART_T_HONEYHIVE_M4, HUD_ART_T_HONEYHIVE_M5, HUD_ART_T_HONEYHIVE_M6},
                          {1, 2, 3, 4, 5, 6},
                          {0, 0, 0, 0, 0, 0},
                          HUD_ART_T_HONEYHIVE_PLATE},
    [GALAXY_GOODEGG] = {HUD_ART_T_GOODEGG_NAME,
                        STAGE_GOODEGG,
                        6,
                        {HUD_ART_T_GOODEGG_M1, HUD_ART_T_GOODEGG_M2, HUD_ART_T_GOODEGG_M3,
                         HUD_ART_T_GOODEGG_M4, HUD_ART_T_GOODEGG_M5, HUD_ART_T_GOODEGG_M6},
                        {1, 2, 3, 4, 5, 6},
                        {0, 0, 0, 0, 0, 0},
                        HUD_ART_T_GOODEGG_PLATE},
    [GALAXY_BEACHBOWL] = {HUD_ART_T_BEACHBOWL_NAME,
                          STAGE_BEACHBOWL,
                          6,
                          {HUD_ART_T_BEACHBOWL_M1, HUD_ART_T_BEACHBOWL_M2, HUD_ART_T_BEACHBOWL_M3,
                           HUD_ART_T_BEACHBOWL_M4, HUD_ART_T_BEACHBOWL_M5, HUD_ART_T_BEACHBOWL_M6},
                          {1, 2, 3, 4, 5, 6},
                          {0, 0, 0, 0, 0, 0},
                          HUD_ART_T_BEACHBOWL_PLATE},
    [GALAXY_FREEZEFLAME] = {HUD_ART_T_FREEZEFLAME_NAME,
                            STAGE_FREEZEFLAME,
                            6,
                            {HUD_ART_T_FREEZEFLAME_M1, HUD_ART_T_FREEZEFLAME_M2, HUD_ART_T_FREEZEFLAME_M3,
                             HUD_ART_T_FREEZEFLAME_M4, HUD_ART_T_FREEZEFLAME_M5, HUD_ART_T_FREEZEFLAME_M6},
                            {1, 2, 3, 4, 5, 6},
                            {0, 0, 0, 0, 0, 0},
                            HUD_ART_T_FREEZEFLAME_PLATE},
    [GALAXY_DEEPDARK] = {HUD_ART_T_DEEPDARK_NAME,
                         STAGE_DEEPDARK,
                         6,
                         {HUD_ART_T_DEEPDARK_M1, HUD_ART_T_DEEPDARK_M2, HUD_ART_T_DEEPDARK_M3,
                          HUD_ART_T_DEEPDARK_M4, HUD_ART_T_DEEPDARK_M5, HUD_ART_T_DEEPDARK_M6},
                         {1, 2, 3, 4, 5, 6},
                         {0, 0, 0, 0, 0, 0},
                         HUD_ART_T_DEEPDARK_PLATE},
    [GALAXY_GUSTYGARDEN] = {HUD_ART_T_GUSTYGARDEN_NAME,
                            STAGE_GUSTYGARDEN,
                            6,
                            {HUD_ART_T_GUSTYGARDEN_M1, HUD_ART_T_GUSTYGARDEN_M2, HUD_ART_T_GUSTYGARDEN_M3,
                             HUD_ART_T_GUSTYGARDEN_M4, HUD_ART_T_GUSTYGARDEN_M5, HUD_ART_T_GUSTYGARDEN_M6},
                            {1, 2, 3, 4, 5, 6},
                            {0, 0, 0, 0, 0, 0},
                            HUD_ART_T_GUSTYGARDEN_PLATE},
    [GALAXY_BUOYBASE] = {HUD_ART_T_BUOYBASE_NAME,
                         STAGE_BUOYBASE,
                         2,
                         {HUD_ART_T_BUOYBASE_M1, HUD_ART_T_BUOYBASE_M2},
                         {1, 2},
                         {0, 0},
                         HUD_ART_T_BUOYBASE_PLATE},
};
_Static_assert(sizeof(g_game.stars_collected) / sizeof(g_game.stars_collected[0]) >= GALAXY_COUNT,
               "every galaxy needs a progress slot");

static int galaxy_for_stage(int stage){
    stage = level_hooks(stage)->galaxy_stage;
    for(int i = 0; i < GALAXY_COUNT; i++)
        if(galaxies[i].stage == stage) return i;
    return -1;
}

static bool game_star_collected(int stage, int star_id){
    int g = galaxy_for_stage(stage);
    if(g < 0 || star_id <= 0) return false;
    return g_game.stars_collected[g] >> star_id & 1;
}

static int stage_find_start(const Stage* stage, int id, int scenario, int zone){
    uint8_t bit = scenario > 0 ? (uint8_t)(1 << (scenario - 1)) : 1;
    int found = -1;
    for(uint32_t i = 0; i < stage->header->start_count; i++){
        const StageStart* s = &stage->starts[i];
        if(s->id != id || !(s->scenarios & bit)) continue;
        if(s->zone == zone) return (int)i;
        if(found < 0) found = (int)i;
    }
    return found < 0 ? 0 : found;
}

static int game_stage_music(void){
    const LevelHooks* level = level_current();
    return level->music ? level->music() : g_game.stage.header->music;
}

static int game_current_scenario(void){ return g_game.scenario_loaded; }

static int game_placed_scenario(int stage, int selected){
    const LevelHooks* level = level_hooks(stage);
    if(level->hidden_mission && selected == level->hidden_mission) return level->hidden_scenario;
    return selected;
}

static bool g_frame_had_load;
static int g_fps;
static void game_load_stage(int index, int start, int scenario){
    if((unsigned)index >= STAGE_COUNT || !stage_blobs[index]) return;
    g_frame_had_load = true;
    audio_clear_effect_queue();
    Game* game = &g_game;
    int selected = scenario;
    scenario = game_placed_scenario(index, selected);
    game->stage_index = index;
    level_enter(index);
    game->scenario = scenario;
    game->selected_mission = selected - 1;
    stage_load_blob(&game->stage, stage_blobs[index]);
    stage_load_far(index);

    bool restart =
        game->restart_id >= 0 && index == game->stage_index_loaded && selected == game->scenario_loaded;
    if(!restart) game->restart_id = -1;
    const StageStart* s = &game->stage.starts[stage_find_start(
        &game->stage, restart ? game->restart_id : start, scenario, restart ? game->restart_zone : 0)];
    game->stage_index_loaded = index;
    game->scenario_loaded = selected;

    game->player.health = mario_lifeup_dark() ? 1 : 3;
    v3 pos = V3(s->position[0], s->position[1], s->position[2]);
    v3 up = v3_from_i16(s->up), front = v3_from_i16(s->front);

    game->time = 0;
    game->stage_tick = 0;
    game->purple_coins = 0;
    level_load_before_player();
    mario_reset(pos, up, front);
    mario_lifeup_reset();
    switches_reset();
    areas_reset();
    actors_load(&game->stage, scenario);
    camera_snap_behind(&game->camera, &game->player);
    camera_galaxy_stage_start(s->camera);
    hud_reset_counters(game);
    audio_play_stage_music(game_stage_music());
    game->fade = 16;
    game->fade_target = 0;
    talk_reset();
    pointer_reset();
    if(!restart) scenario_intro_start(index, scenario);
    else scenario_intro_end();
}

static void game_reload_keep_health(int index, int start, int scenario){
    int health = g_game.player.health;
    game_load_stage(index, start, scenario);
    if(health > 0){
        g_game.player.health = fx_clamp(health, 1, 6);
        mario_lifeup_reset();
    }
}

static void game_init(void){
    Game* game = &g_game;
    g_debug_options = DEBUG_MAGNET | DEBUG_PULL_ASSIST | DEBUG_FLIGHT_POINTER | DEBUG_MUSIC | DEBUG_SOUND |
                      DEBUG_FALLING_DROPS;
    game->view_distance = VIEW_MODE_DEFAULT;
    game->lives = 4;
    game->player.health = 3;
    game->restart_id = -1;
    audio_init();
    hud_init();
    game_load_stage(STAGE_OBSERVATORY, 0, 0);

    game->scene = SCENE_PLAY;
}

static void game_open_missions(int galaxy){
    Game* game = &g_game;
    if(galaxy < 0 || galaxy >= GALAXY_COUNT) return;
    game->scene = SCENE_MISSION_SELECT;
    game->selected_galaxy = galaxy;
    game->selected_mission = 0;
    for(int i = 0; i < galaxies[galaxy].mission_count; i++)
        if(!(game->stars_collected[galaxy] >> galaxies[galaxy].star_ids[i] & 1)){
            game->selected_mission = i;
            break;
        }
    audio_play_effect(SFX_MENU_OPEN, 4);
    scenario_select_open(galaxy);
}

static void game_collect_star(int star_id){
    Game* game = &g_game;
    int g = galaxy_for_stage(game->stage_index);
    if(g >= 0 && star_id > 0 && !(game->stars_collected[g] >> star_id & 1)){
        game->stars_collected[g] |= 1u << star_id;
        game->stars_total++;
    }
    game->scene = SCENE_STAR_GET;
    game->scene_timer = 0;
    mario_set_action(MA_STAR_GET);
    star_get_start(star_id);
    audio_stop_music();
    audio_play_effect(SFX_STAR_GET, 6);
}

static void play_tick(const KeyState* keys){
    Game* game = &g_game;
    Player* p = &game->player;
    game->stage_tick++;
    if(g_debug_options & DEBUG_INFINITE_AIR) g_swim.oxygen = 3600;
    if(g_debug_options & DEBUG_INFINITE_BEE) g_bee.fly_time = BEE_AIR_WALK_TIME;
    mario_lifeup_meter_tick();
    hud_tick(game, keys);
    if(cutscene_tick(keys)) return;
    level_before_player(keys);
    pointer_tick();
    talk_tick(keys);
    if(p->action == MA_STAR_LAUNCH) flight_tick();
    else if(!cutscene_blocks(BLOCK_PLAYER)) mario_tick(keys);
    if(!g_mario_held && level_pull_back(p->figure.position)) mario_pull_back_start();
    actors_tick();
    areas_tick();
    if(p->action == MA_DEATH && p->action_timer++ > 100){
        game->scene = SCENE_GAME_OVER;
        game->scene_timer = 0;
    }
}

static void game_over_tick(const KeyState* keys){
    Game* game = &g_game;
    game->scene_timer++;
    game->fade_target = 16;
    if(game->scene_timer > 40){
        game->player.health = 3;
        bool game_over = game->lives <= 0;
        if(!game_over) game->lives--;
        if(game_over){
            game->lives = 4;
            game->scene = SCENE_TITLE;
            game_load_stage(STAGE_OBSERVATORY, 0, 0);
        } else {
            level_miss();
            game->scene = SCENE_PLAY;
            game_load_stage(game->stage_index, 0, game_current_scenario());
        }
    }
}

static void title_tick(const KeyState* keys){
    Game* game = &g_game;
    game->scene_timer++;
    if(keys->pressed & (KEY_START | KEY_A)){
        game->scene = SCENE_PLAY;
        audio_play_effect(SFX_MENU_SELECT, 4);
    }
}

static void game_tick(const KeyState* keys){
    Game* game = &g_game;
    Player* p = &game->player;
    game->tick++;
    game->hint = HINT_NONE;
    game_scene(game->scene)->tick(keys);

    if(game_scene(game->scene)->animates_player){
        mario_anim_tick();
        if(!cutscene_blocks(BLOCK_MARIO_ANIM) &&
           (p->action == MA_WAIT || p->action == MA_SQUAT || p->action == MA_SQUAT_SLIDE))
            mario_choose_ground_anim();
        if(p->action != MA_STAR_LAUNCH){
            p->figure.local_yaw = 0;
            p->figure.local_pitch = 0;
        }
    }
    star_get_wipe_tick();
    if(game->fade < game->fade_target) game->fade++;
    else if(game->fade > game->fade_target) game->fade--;
}

static void play_draw_hud(void){
    Game* game = &g_game;
    const Player* p = &game->player;
    if(g_debug_options & DEBUG_SHOW_FPS) hud_digits(HUD_STAR_X, HUD_STAR_Y + 18, g_fps);
    if(cutscene_draw_hud()) return;
    pointer_draw();
    talk_draw();
    if(!cutscene_blocks(BLOCK_HUD)){
        hud_draw_counters(game);
        hud_draw_fly_meter(game);
        if(!actors_banner_active()) hud_draw_air_meter(game);
    }
    missions_draw_hud();
    level_draw_hud();
    actors_draw_hud();

    int g = galaxy_for_stage(game->stage_index);
    if(g >= 0 && game->scenario > 0 && scenario_intro_title_visible(game->stage_tick))
        hud_draw_stage_title(galaxies[g].name_art, galaxies[g].mission_arts[game_current_scenario() - 1]);
    if(p->action == MA_DEATH && p->action_timer > 40) hud_draw_too_bad(p->action_timer - 40);
}

static void game_over_draw_hud(void){ hud_draw_too_bad(100); }

static void game_draw_hud(void){
    hud_begin();
    const GameScene* scene = game_scene(g_game.scene);
    if(scene->draw_hud) scene->draw_hud();
    hud_end();
}

static bool camera_manual_allowed(void){
    const Player* p = &g_game.player;
    return g_game.scene == SCENE_PLAY && p->action != MA_DEATH && p->action != MA_STAR_GET &&
           !cutscene_blocks(BLOCK_CAMERA_INPUT);
}

static void title_camera(fx dt, const KeyState* keys, uint16_t held, uint16_t pressed){
    Game* game = &g_game;
    game->camera.heading =
        v3_rotate_axis14(game->camera.heading, game->camera.smooth_up, fx_mul(dt, FXC(0.25f)));
    KeyState none = {0};
    camera_update(dt, &none);
}

static void follow_camera(fx dt, const KeyState* keys, uint16_t held, uint16_t pressed){
    Game* game = &g_game;
    bool manual_allowed = camera_manual_allowed();
    camera_manual_prepare(held, pressed, manual_allowed);
    if(!(game->scene == SCENE_STAR_GET && star_get_camera(&game->camera))) camera_update(dt, keys);
    level_after_camera();
    scenario_intro_camera();
    talk_camera();
    actors_camera();
    level_after_actor_cameras();
    camera_manual_update(held, manual_allowed);
}

static const GameScene title_scene = {.tick = title_tick, .camera = title_camera};

static const GameScene play_scene = {
    .tick = play_tick,
    .camera = follow_camera,
    .draw_hud = play_draw_hud,
    .animates_player = true,
};

static const GameScene star_get_scene = {
    .tick = star_get_tick,
    .camera = follow_camera,
    .draw_hud = star_get_draw_hud,
    .animates_player = true,
};

static const GameScene game_over_scene = {.tick = game_over_tick, .draw_hud = game_over_draw_hud};

static const GameScene pause_scene = {
    .tick = debug_menu_tick,
    .draw = debug_menu_draw,
    .release = debug_menu_invalidate,
    .full_res = true,
};

__attribute__((noinline)) static void game_update_frame(fx dt, KeyState keys){
    Game* game = &g_game;
    game->time += dt;
    uint16_t camera_held = keys.held, camera_pressed = keys.pressed;
    pointer_filter_keys(&keys);
    game->keys = keys;
    if(game->scene == SCENE_PLAY && (keys.pressed & KEY_START) && game->player.action != MA_DEATH &&
       !cutscene_blocks(BLOCK_PAUSE)){
        game->scene = SCENE_PAUSE;
        debug_menu_open();
        audio_play_effect(SFX_MENU_OPEN, 4);
        keys.pressed &= ~KEY_START;
    }

    if(game->scene == SCENE_PLAY && (keys.pressed & KEY_SELECT)){
        if(keys.held & KEY_L){
            game->view_distance = (game->view_distance + 1) % VIEW_MODE_COUNT;
        } else if(keys.held & KEY_R){
            camera_set_zoom(&game->camera, (game->camera_zoom + 1) % CAMERA_ZOOM_COUNT);
        } else {
            int next = (game->stage_index + 1) % STAGE_COUNT;
            while(level_hooks(next)->features & LEVEL_HUB_ROOM) next = (next + 1) % STAGE_COUNT;
            game->scene = SCENE_PLAY;
            game_load_stage(next, 0, next == STAGE_OBSERVATORY ? 0 : 1);
        }
        keys.pressed &= ~KEY_SELECT;
    }

    game->tick_accumulator += (uint32_t)dt * 60;
    int ticks = 0;

    static uint16_t s_unticked_pressed;
    KeyState tick_keys = keys;
    tick_keys.pressed |= s_unticked_pressed;

    const int max_ticks = 8;
    while(game->tick_accumulator >= 256 && ticks < max_ticks){
        game->tick_accumulator -= 256;
        game->keys = tick_keys;
        game_tick(&tick_keys);
        tick_keys.pressed = 0;
        ticks++;
    }
    s_unticked_pressed = ticks ? 0 : tick_keys.pressed;
    if(game->tick_accumulator >= 256) game->tick_accumulator = 0;

    game->camera_underwater = water_query(game->camera.position, game->player.gravity_up, 0);
    const GameScene* scene = game_scene(game->scene);
    if(scene->camera) scene->camera(dt, &keys, camera_held, camera_pressed);
    mario_update_pose();
    if(game->player.action == MA_STAR_LAUNCH) flight_apply_pose(&game->player.figure);
    actors_player_pose(&game->player.figure);
    level_after_player_pose();
    scene = game_scene(game->scene);
    if(scene->pose_player) scene->pose_player(&game->player.figure);
    figure_update_transform(&game->player.figure);
}

__attribute__((noinline)) static void game_collect_scene_figures(void){
    Game* game = &g_game;
    CameraView view;
    camera_build_view(&game->camera, &view);
    game->figure_count = 0;
    if(!game->player.figure.hidden) game->figures[game->figure_count++] = &game->player.figure;
    actors_collect_figures(&view);
    star_get_add_figures();
    level_add_figures();
}

__attribute__((noinline)) static void game_add_player_attachment(Figure* attachment){
    if(level_player_figure(attachment)) return;
    actors_player_figure(attachment);
}

__attribute__((noinline)) static void game_draw_frame(uint8_t* framebuffer){
    icestep_cache_end_logic();
    Game* game = &g_game;
    const GameScene* scene = game_scene(game->scene);
    game_scenes_release(scene);
    if(g_display_was_full_res && !scene->full_res) stage_restore_texture_cache();
    g_display_was_full_res = scene->full_res;
    if(scene->full_res){
        scene->draw(framebuffer);
        return;
    }
    if(scene->draw){
        scene->draw(framebuffer);
        game_draw_hud();
        scenario_select_set_fade(game);
        return;
    }
    perf_lap(PERF_OTHER);
    if(g_render_style) render_style_begin(framebuffer);
    else draw_sky(framebuffer, game->stage.sky, game->camera.orientation.x, game->camera.orientation.y);
    perf_lap(PERF_SKY);

    game_collect_scene_figures();
    Figure player_attachment;
    game_add_player_attachment(&player_attachment);
    Figure freeze_part;
    mario_freeze_add_figure(&freeze_part);
    draw_scene(framebuffer, game);
    scenario_intro_draw_frame(framebuffer);
    wipe_update();
    game_draw_hud();
    scenario_select_set_fade(game);
}

static void game_run_frame(bool page, const KeyState* keys, fx dt){
    game_update_frame(dt, *keys);
    perf_lap(PERF_LOGIC);

    g_display_full_res = game_scene(g_game.scene)->full_res;
    game_draw_frame(display_page(page));
}

IWRAM_CODE void vblank_service(void){
    audio_vblank();
    hud_vblank();
    wipe_vblank();
}

int main(void){
    memory_set_fast_waitstates();
    REG_DISPSTAT = DSTAT_VBL_IRQ;
    REG_IE = IRQ_VBLANK;
    REG_IME = 1;
    frame_timer_restart();
    perf_init();
    g_input_latch_enabled = 1;

    game_init();
    for(int i = 0; i < 256; i++) BG_PALETTE[i] = game_palette[i];

    fx dt = FXC(1.0f / 30.0f);

    uint32_t frame_timer_remainder = 0;
    uint32_t fps_ticks = 0, fps_frames = 0;
    bool page = true;
    for(;;){
        uint16_t now;
        page = !page;

        KeyState keys = *(KeyState*)&g_keys;
        game_run_frame(page, &keys, dt);

        now = input_read_keys();
        REG_IME = 0;
        input_finish_frame((KeyState*)&g_keys, now, dt);
        REG_IME = 1;

        display_show_page(page);
        wipe_after_flip();

        uint16_t ticks = REG_TM2CNT_L;
        frame_timer_restart();
        uint32_t elapsed = ticks + frame_timer_remainder;
        frame_timer_remainder = elapsed & 63;
        dt = fx_min(elapsed >> 6, FXC(1.0f / 8.0f));
        fps_ticks += ticks;
        fps_frames++;
        if(fps_ticks >= 8192){
            g_fps = (int)((fps_frames * 16384 + fps_ticks / 2) / fps_ticks);
            fps_ticks = fps_frames = 0;
        }
        if(g_frame_had_load){
            dt = 5;
            frame_timer_remainder = 0;
        }
        if(g_frame_had_load || g_game.scene != SCENE_PLAY) fps_ticks = fps_frames = 0;
        perf_frame_end(g_game.scene == SCENE_PLAY && !g_frame_had_load);
        g_frame_had_load = false;
    }
}
