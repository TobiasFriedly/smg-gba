__attribute__((noinline)) static void scene_clear_words(void* destination, const volatile uint32_t* zero,
                                                        unsigned words){
    uint32_t* output = destination;
    for(unsigned at = 0; at < words; at += 64){
        unsigned count = words - at;
        if(count > 64) count = 64;
        dma3_fill32(output + at, zero, count);
    }
}

__attribute__((noinline)) static void scene_reset_point_marks(void){
    volatile uint32_t zero = 0;
    scene_clear_words(g_point_marks, &zero, (sizeof g_point_marks + 3) / 4);
    scene_clear_words(g_far_point_marks, &zero, (sizeof g_far_point_marks + 3) / 4);
    scene_clear_words(g_leaf_stamps, &zero, sizeof g_leaf_stamps / 4);
    for(int i = 0; i < MAX_POINTS; i++) g_frame_points[i].mark = 0;
}

__attribute__((noinline)) static void scene_far_pass(const CameraView* view, v3 camera_position, int axis,
                                                     const fx* cam_axis, int cut_bin){
    g_far_base = g_point_count;
    if(g_far_base + (int)g_far_stage.header->point_count > MAX_POINTS) return;
    FramePoint* points = g_frame_points + g_far_base;
    int far_leaves = collect_visible_chunks(&g_far_stage, view, camera_position, 0, cut_bin);
    far_leaves = parts_filter_leaves(g_far_stage.header, g_visible_leaves, far_leaves);
    if(!far_leaves) return;
    int far_points, far_used;
    int far_tris = collect_visible_triangles(
        &g_far_stage, far_leaves, axis, -cam_axis[axis], camera_position, &far_points, &far_used,
        g_game.stage.header->point_home ? g_point_marks : g_far_point_marks, MAX_POINTS);
    if(!far_tris) return;
    TransformJob far_job =
        geometry_transform_job(g_far_stage.points, points, g_screen_positions + g_far_base, g_point_list,
                               far_points, &view->to_screen, g_far_stage.point_shift);
    engine_transform_points(&far_job);
    const uint16_t* water = stage_water_table(g_far_stage.header);
    EmitJob job = {g_far_stage.triangles,
                   points,
                   g_screen_positions + g_far_base,
                   g_far_stage.textures,
                   g_far_texture_average,
                   NULL,
                   NULL,
                   NULL,
                   water,
                   g_far_stage.points,
                   1,
                   cut_bin,
                   BIN_COUNT,
                   0,
                   water && g_game.stage.header->has_water == 2 ? 3 : 0,
                   NULL};
    job.under_bins = g_oceanbowl_split ? BIN_COUNT * 2 : 0;
    emit_stage_triangles(&job, g_visible_triangles, far_tris);
}

__attribute__((noinline)) static bool view_sphere_visible(const CameraView* view, v3 centre, fx radius){
    v3 p = xform_point(&view->to_screen, centre);
    const m3* m = &view->to_screen.m;
    for(int axis = 0; axis < 2; axis++)
        for(int sign = -1; sign <= 1; sign += 2){
            v3 n = v3_sub(v3_scale(m->r[axis], sign * FX_ONE), m->r[2]);
            if(sign * (&p.x)[axis] - p.z > fx_mul(radius, v3_length_exact(n)) + 1) return false;
        }
    return p.z >= -fx_mul(radius, v3_length_exact(m->r[2]));
}

static const FigureOps* scene_figure_ops(const Figure* figure){
    uintptr_t offset = (uintptr_t)figure - (uintptr_t)&g_game.actors[0].figure;
    if(offset < sizeof g_game.actors){
        const Actor* owner = (const Actor*)((const uint8_t*)figure - offsetof(Actor, figure));
        return actor_kinds[owner->type].figure_ops;
    }
    for(unsigned i = 0; i < sizeof extra_figure_kinds / sizeof *extra_figure_kinds; i++)
        if(figure->model == (const FigureHeader*)extra_figure_kinds[i].model)
            return extra_figure_kinds[i].ops;
    return NULL;
}

__attribute__((noinline)) static void scene_draw_figure(uint8_t* framebuffer, Figure* figure,
                                                        const CameraView* view){
    const FigureOps* ops = scene_figure_ops(figure);
    if(ops && ops->draw && ops->draw(framebuffer, figure, view)) return;
    draw_figure(framebuffer, figure, view);
}

__attribute__((noinline)) static fx scene_player_sort_depth(const xform* view, const Player* player){
    v3 centre = v3_add(player->figure.position, v3_scale14(player->up, FXC(0.4f)));
    return xform_point(view, centre).z;
}

__attribute__((noinline)) static bool scene_figure_view_bounds(const Figure* f, const CameraView* view){
    const FigureOps* ops = scene_figure_ops(f);
    return ops && ops->view_bounds && ops->view_bounds(f, view);
}

__attribute__((noinline)) static bool scene_wide_figure_visible(
    const Figure* f, const CameraView* view, v3 position, fx depth){
    const FigureOps* ops = scene_figure_ops(f);
    return ops && ops->wide_visible && ops->wide_visible(f, view, position, depth);
}

__attribute__((noinline)) static bool scene_figure_both_sides(const Figure* f){
    const FigureOps* ops = scene_figure_ops(f);
    return ops && ops->both_sides && ops->both_sides(f);
}

__attribute__((noinline)) static void draw_scene_bins_water(
    uint8_t* framebuffer, const CameraView* view, int first_bin){
    for(int side = 0; side < 2; side++){
        bool below = g_game.camera_underwater ? side == 1 : side == 0;
        const uint16_t* heads = g_triangle_bin_heads + (below ? BIN_COUNT : 0);
        g_oceanbowl.side = below;
        for(int bin = first_bin; bin < BIN_COUNT; bin++){
            for(Figure* figure = g_figure_bin_heads[bin]; figure; figure = figure->next){
                if((figure->position.z < 0) != below && !scene_figure_both_sides(figure)) continue;
                perf_lap(PERF_LEVEL);
                if(figure->has_shadow) draw_shadow(figure, view);
                scene_draw_figure(framebuffer, figure, view);
                perf_lap(PERF_FIGURES);
            }
            if(heads[bin] != BIN_END) engine_draw_chain(heads[bin]);
        }
        if(side == 0) oceanbowl_draw_surface();
    }
}

IWRAM_FN static void draw_scene_bins(uint8_t* framebuffer, Game* game, const CameraView* view,
                                     int first_bin){
    if(g_oceanbowl_split){
        draw_scene_bins_water(framebuffer, view, first_bin);
        return;
    }

    int last_bin = BIN_COUNT - 1;
    while(last_bin >= first_bin && g_triangle_bin_heads[last_bin] == BIN_END && !g_figure_bin_heads[last_bin])
        last_bin--;
    while(first_bin < last_bin && g_triangle_bin_heads[first_bin] == BIN_END &&
          !g_figure_bin_heads[first_bin])
        first_bin++;
    for(int bin = first_bin; bin <= last_bin; bin++){
        for(Figure* figure = g_figure_bin_heads[bin]; figure; figure = figure->next){
            perf_lap(PERF_LEVEL);
            if(figure->has_shadow) draw_shadow(figure, view);
            scene_draw_figure(framebuffer, figure, view);
            perf_lap(PERF_FIGURES);
        }
        if(g_triangle_bin_heads[bin] != BIN_END) engine_draw_chain(g_triangle_bin_heads[bin]);
    }
}

__attribute__((noinline)) static void draw_scene(uint8_t* framebuffer, Game* game){
    const Stage* stage = &game->stage;
    CameraView view;
    perf_lap(PERF_OTHER);
    camera_build_view(&game->camera, &view);
    v3 camera_position = game->camera.position;

    if(++g_point_stamp == 0) g_point_stamp = 1;
    if(g_point_stamp == 1) scene_reset_point_marks();

    const ViewMode* mode = view_mode();
    int cut_bin = BIN_COUNT - (fx_mul(BIN_COUNT << 8, mode->near) >> 8);
    g_farthest_visible_bin = cut_bin;
    bool far_pass = mode->far && g_has_far_stage;
    g_flat_small = mode->flat_small;

    int leaf_count = collect_visible_chunks(stage, &view, camera_position, g_farthest_visible_bin, 0);
    leaf_count = parts_filter_leaves(stage->header, g_visible_leaves, leaf_count);
    leaf_count = occluder_filter_leaves(g_visible_leaves, leaf_count, camera_position);

    int axis = view_nearest_axis(view.direction), point_count, pool_used;
    fx cam_axis[6] = {-camera_position.x, camera_position.x,  -camera_position.y,
                      camera_position.y,  -camera_position.z, camera_position.z};

    const StageHeader* sh = stage->header;
    g_far_reserve = g_has_far_stage ? (int)g_far_stage.header->point_count + 1 : 0;
    const uint16_t* home = sh->point_home ? (const uint16_t*)((const uint8_t*)sh + sh->point_home) : NULL;
    int triangle_count = collect_visible_triangles(
        stage, leaf_count, axis, -cam_axis[axis], camera_position, &point_count, &pool_used, g_point_marks,
        home ? MAX_POINTS - POINTS_SPLIT_RESERVE - FAR_POINTS_RESERVE : MAX_POINTS);
    perf_lap(PERF_CULL);
    TransformJob transform_job =
        geometry_transform_job(stage->points, g_frame_points, g_screen_positions, g_point_list, point_count,
                               &view.to_screen, stage->point_shift);
    transform_job.home = home;
    transform_job.delta = g_leaf_delta;
    engine_transform_points(&transform_job);
    perf_lap(PERF_TRANSFORM);

    for(int i = 0; i < BIN_COUNT; i++){
        g_triangle_bin_heads[i] = BIN_END;
        g_triangle_bin_heads[BIN_COUNT + i] = BIN_END;
        g_figure_bin_heads[i] = NULL;
    }
    oceanbowl_begin();
    g_point_count = home ? pool_used : (int)sh->point_count;
    g_arena_count = 0;
    g_engine_fb = framebuffer;
    int flat_bin = mode->flat_depth == 0x7FFFFFFF ? -1 : depth_bin_for_z(mode->flat_depth);
    {
        const uint16_t* water = stage_water_table(sh);
        EmitJob job = {stage->triangles,
                       g_frame_points,
                       g_screen_positions,
                       stage->textures,
                       g_flat_small || flat_bin >= 0 ? g_texture_average : g_no_average,
                       g_texture_cache_slot,
                       home,
                       g_leaf_delta,
                       water,
                       stage->points,
                       0,
                       g_farthest_visible_bin,
                       flat_bin,
                       g_flat_small,
                       !water               ? 0
                       : sh->has_water == 2 ? 3
                       : sh->has_water      ? (sh->has_water_region ? 2 : 1)
                                            : 0,
                       NULL};
        job.under_bins = g_oceanbowl_split ? BIN_COUNT * 2 : 0;
        emit_stage_triangles(&job, g_visible_triangles, triangle_count);
    }

    perf_lap(PERF_EMIT);
    if(far_pass){
        scene_far_pass(&view, camera_position, axis, cam_axis, cut_bin);
    }

    perf_lap(PERF_FAR);
    oceanbowl_queue(&view);

    for(int i = 0; i < game->figure_count; i++){
        Figure* figure = game->figures[i];
        fx depth;
        if(figure == &game->player.figure) depth = scene_player_sort_depth(&view.to_screen, &game->player);
        else {
            v3 view_position = xform_point(&view.to_screen, v3_add(figure->position, V3(0, 0, FXC(0.4f))));
            if((((view_clip_mask(view_position) & CLIP_SIDES) &&
                 !scene_figure_view_bounds(figure, &view)) ||
                view_position.z > mode->figure_depth) &&
               !scene_wide_figure_visible(figure, &view, view_position, mode->figure_depth))
                continue;
            depth = view_position.z;
        }
        int bin = depth_bin_for_z(depth);
        figure->next = g_figure_bin_heads[bin];
        g_figure_bin_heads[bin] = figure;
    }

    perf_lap(PERF_EMIT);
    draw_scene_bins(framebuffer, game, &view, far_pass ? 0 : g_farthest_visible_bin);
    perf_lap(PERF_LEVEL);
}
