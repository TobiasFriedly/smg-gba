_Static_assert(sizeof(DrawEntry) == 32, "raster.s assumes 32-byte draw entries");

#define ENGINE_ARENA_MAX 2048
DrawEntry g_arena[ENGINE_ARENA_MAX] EWRAM_BSS;
int g_arena_count;
uint8_t* g_engine_fb;

int32_t g_flat_small = 1;
static const uint8_t g_no_average[STAGE_MAX_TEXTURES] = {0};

IWRAM_CODE static inline int32_t screen_area(uint32_t a, uint32_t b, uint32_t c){
    int32_t ax = screen_x(a), ay = screen_y(a), bx = screen_x(b), by = screen_y(b), cx = screen_x(c),
            cy = screen_y(c);
    return (ay - cy) * (ax - bx) - (ax - cx) * (ay - by);
}

IWRAM_CODE static inline bool screen_rowless(uint32_t a, uint32_t b, uint32_t c){
    int32_t y0 = screen_y(a), y1 = screen_y(b), y2 = screen_y(c);
    int32_t lo = y0 < y1 ? y0 : y1, hi = y0 < y1 ? y1 : y0;
    if(y2 < lo) lo = y2;
    if(y2 > hi) hi = y2;
    return ((lo + 7) >> 4) >= ((hi + 7) >> 4) || hi < -7 || lo >= FRAME_HEIGHT * 16 - 8;
}

IWRAM_CODE static inline bool screen_small(uint32_t a, uint32_t b, uint32_t c){
    int32_t x0 = screen_x(a), x1 = screen_x(b), x2 = screen_x(c);
    int32_t lo = x0 < x1 ? x0 : x1, hi = x0 < x1 ? x1 : x0;
    if(x2 < lo) lo = x2;
    if(x2 > hi) hi = x2;
    if(hi - lo > 4 * 16) return false;
    int32_t y0 = screen_y(a), y1 = screen_y(b), y2 = screen_y(c);
    lo = y0 < y1 ? y0 : y1;
    hi = y0 < y1 ? y1 : y0;
    if(y2 < lo) lo = y2;
    if(y2 > hi) hi = y2;
    return hi - lo <= 3 * 16;
}

static int g_engine_bin_offset;

IWRAM_CODE static void engine_push(int bin, uint32_t sa, uint32_t sb, uint32_t sc, uint32_t uva, uint32_t uvb,
                                   uint32_t uvc, unsigned mode, unsigned colour,
                                   const TextureReference* texture){
    int n = g_arena_count;
    if(n >= ENGINE_ARENA_MAX) return;
    g_arena_count = n + 1;
    DrawEntry* e = &g_arena[n];
    bin += g_engine_bin_offset;
    e->next = g_triangle_bin_heads[bin];
    g_triangle_bin_heads[bin] = (uint16_t)n;
    e->mode = (uint8_t)mode;
    e->colour = (uint8_t)colour;
    e->s[0] = sa;
    e->s[1] = sb;
    e->s[2] = sc;
    e->uv[0] = uva;
    e->uv[1] = uvb;
    e->uv[2] = uvc;
    e->texture = texture;
}

IWRAM_CODE static void engine_out(int bin, uint32_t sa, uint32_t sb, uint32_t sc, uint32_t uva, uint32_t uvb,
                                  uint32_t uvc, unsigned mode, unsigned colour,
                                  const TextureReference* texture){
    if(bin >= 0){
        engine_push(bin, sa, sb, sc, uva, uvb, uvc, mode, colour, texture);
        return;
    }
    DrawEntry e = {0xFFFF, (uint8_t)mode, (uint8_t)colour, {sa, sb, sc}, {uva, uvb, uvc}, texture};
    engine_raster(&e);
}

IWRAM_FN static int32_t engine_ratio16(uint32_t num, uint32_t den){
    if(!den) return 0;
    int e = 0;
    uint32_t m = den;
    while(m >= 512){
        m >>= 1;
        e++;
    }
    while(m < 256){
        m <<= 1;
        e--;
    }
    uint64_t r = (uint64_t)num * engine_recip_table[m - 256];
    int sh = 8 + e;
    int32_t t = sh >= 0 ? (int32_t)(r >> sh) : (int32_t)(r << -sh);
    return t > 65536 ? 65536 : t;
}

IWRAM_FN static int32_t clip_distance(const v3* p, int k){
    switch(k){
    case 0: return p->z - CLIP_NEAR_Z;
    case 1: return (p->z << 2) - p->x;
    case 2: return (p->z << 2) + p->x;
    case 3: return (p->z << 2) - p->y;
    default: return (p->z << 2) + p->y;
    }
}

IWRAM_FN static void clip_lerp(ClipVertex* c, const ClipVertex* s, const ClipVertex* o, int32_t t, int k){
    const int32_t* sp = &s->p.x;
    const int32_t* op = &o->p.x;
    int32_t* cp = &c->p.x;
    for(int i = 0; i < 5; i++) cp[i] = sp[i] + (int32_t)(((int64_t)(op[i] - sp[i]) * t) >> 16);
    if(k == 0 && c->p.z < CLIP_NEAR_Z) c->p.z = CLIP_NEAR_Z;
}
_Static_assert(sizeof(ClipVertex) == 20 && offsetof(ClipVertex, v) == 16,
               "clip_lerp walks a ClipVertex as 5 words");

IWRAM_FN static int clip_polygon(ClipVertex* out, const ClipVertex* in, int n, int k){
    int m = 0;
    const ClipVertex* a = &in[n - 1];
    int32_t da = clip_distance(&a->p, k);
    for(int i = 0; i < n; i++){
        const ClipVertex* b = &in[i];
        int32_t db = clip_distance(&b->p, k);

        if(da >= 0 && db < 0)
            clip_lerp(&out[m++], a, b, engine_ratio16((uint32_t)da, (uint32_t)(da - db)), k);
        else if(da < 0 && db >= 0)
            clip_lerp(&out[m++], b, a, engine_ratio16((uint32_t)db, (uint32_t)(db - da)), k);
        if(db >= 0) out[m++] = *b;
        a = b;
        da = db;
    }
    return m;
}

static inline uint32_t pack_uv(int32_t u, int32_t v){ return ((uint32_t)u & 0xFFFF) | (uint32_t)v << 16; }

IWRAM_FN static void engine_emit_clipped(int bin, const ClipVertex* tri, unsigned mode, unsigned colour,
                                         const TextureReference* texture){
    ClipVertex buf[2][10];
    int n = 3;
    for(int i = 0; i < 3; i++) buf[0][i] = tri[i];
    int cur = 0;
    for(int k = 0; k < 5 && n >= 3; k++){
        bool out = false;
        for(int i = 0; i < n; i++)
            if(clip_distance(&buf[cur][i].p, k) < 0){
                out = true;
                break;
            }
        if(!out) continue;
        n = clip_polygon(buf[cur ^ 1], buf[cur], n, k);
        cur ^= 1;
    }
    if(n < 3) return;
    uint32_t s[10], uv[10];
    for(int i = 0; i < n; i++){
        s[i] = screen_pack_position(project_to_screen(buf[cur][i].p));
        uv[i] = pack_uv(buf[cur][i].u, buf[cur][i].v);
    }
    int32_t area = 0;
    for(int i = 1; i + 1 < n; i++) area += screen_area(s[0], s[i], s[i + 1]);
    if(area <= 0) return;
    for(int i = 1; i + 1 < n; i++){
        if(screen_area(s[0], s[i], s[i + 1]) <= 0 || screen_rowless(s[0], s[i], s[i + 1])) continue;
        engine_out(bin, s[0], s[i], s[i + 1], uv[0], uv[i], uv[i + 1], mode, colour, texture);
    }
}

static inline const uint16_t* stage_water_table(const StageHeader* h){
    return h->texture_water ? (const uint16_t*)((const uint8_t*)h + h->texture_water) : NULL;
}

_Static_assert(offsetof(EmitJob, far) == 40 && offsetof(EmitJob, slow) == 60 &&
                   offsetof(EmitJob, camera_underwater) == 84 && offsetof(EmitJob, under_bins) == 88,
               "raster.s assumes the EmitJob layout");

static inline int emit_slot(const EmitJob* j, int point){
    return j->home ? point + j->delta[j->home[point]] : point;
}

__attribute__((noinline)) static bool emit_through_water(const EmitJob* j, const StageTriangle* st){
    const StageHeader* h = g_game.stage.header;
    if(j->water == 3) return g_game.camera_underwater || (st->flags & TRI_WATER_TINT);
    const int16_t* a = j->stage_points[st->v[0].point];
    if(j->water == 2){
        int32_t dx = a[0] - h->water_centre[0], dy = a[1] - h->water_centre[1];
        if(((dx * dx) >> 8) + ((dy * dy) >> 8) > h->water_radius_squared || a[2] < h->water_min_z)
            return false;
    }
    int32_t max_z = a[2], z;
    if((z = j->stage_points[st->v[1].point][2]) > max_z) max_z = z;
    if((z = j->stage_points[st->v[2].point][2]) > max_z) max_z = z;
    return g_game.camera_underwater ? max_z >= h->water_level : max_z < h->water_level;
}

__attribute__((noinline)) static uint32_t emit_scroll_uv(const StageTriangle* st){
    const StageHeader* h = g_game.stage.header;
    const int16_t* speed =
        (const int16_t*)((const uint8_t*)h + h->texture_scroll) + 2 * stage_triangle_texture(st);
    int16_t du = (int16_t)((fx_mul(speed[0], g_game.time) % (32 * FX_ONE)) >> 4);
    int16_t dv = (int16_t)((fx_mul(speed[1], g_game.time) % (32 * FX_ONE)) >> 4);
    return pack_uv(du, dv);
}

static inline int32_t min3(int32_t a, int32_t b, int32_t c){
    int32_t m = a < b ? a : b;
    return m < c ? m : c;
}
static inline int32_t max3(int32_t a, int32_t b, int32_t c){
    int32_t m = a > b ? a : b;
    return m > c ? m : c;
}

IWRAM_CODE static inline bool emit_needs_subdivision(fx min_z, fx max_z){
    return max_z - min_z > fx_mul(fx_max(min_z, 0) + FXC(3.0f), MAX_TRIANGLE_DEPTH_SPAN);
}

typedef struct {
    v3 p;
    int32_t u, v;
    uint32_t clip;
    uint32_t s;
} SplitVertex;

IWRAM_FN static void emit_split_vertex(SplitVertex* o, v3 p, int32_t u, int32_t v){
    o->p = p;
    o->u = u;
    o->v = v;
    o->clip = view_clip_mask(p);
    o->s = o->clip & CLIP_GUARD ? 0 : screen_pack_position(project_to_screen(p));
}

IWRAM_FN static void emit_face(const EmitJob* j, int bin, const SplitVertex* a, const SplitVertex* b,
                               const SplitVertex* c, bool alpha, unsigned average,
                               const TextureReference* texture){
    if(a->clip & b->clip & c->clip & CLIP_SIDES) return;
    bool flat_far = !alpha && average && (j->far || bin < j->flat_bin);
    unsigned mode = alpha ? RM_ALPHA : RM_TEX;
    if((a->clip | b->clip | c->clip) & CLIP_GUARD){
        ClipVertex t[3] = {{a->p, a->u, a->v}, {b->p, b->u, b->v}, {c->p, c->u, c->v}};
        engine_emit_clipped(bin, t, flat_far ? RM_FLAT : mode, average, texture);
        return;
    }
    if(screen_area(a->s, b->s, c->s) <= 0 || screen_rowless(a->s, b->s, c->s)) return;
    if(!alpha && average && (flat_far || (j->flat_small && screen_small(a->s, b->s, c->s)))) mode = RM_FLAT;
    engine_push(bin, a->s, b->s, c->s, pack_uv(a->u, a->v), pack_uv(b->u, b->v), pack_uv(c->u, c->v), mode,
                average, texture);
}

IWRAM_FN static void emit_subdivided(const EmitJob* j, SplitVertex tri[3], bool alpha, unsigned average,
                                     const TextureReference* texture){
    SplitVertex stack[16][3];
    int top = 0;
    for(int i = 0; i < 3; i++) stack[0][i] = tri[i];
    top = 1;
    while(top){
        SplitVertex* t = stack[--top];
        SplitVertex a = t[0], b = t[1], c = t[2];
        if(a.clip & b.clip & c.clip & CLIP_SIDES) continue;
        bool split = false;
        for(int rot = 0; rot < 3 && !split; rot++){
            if(emit_needs_subdivision(fx_min(a.p.z, b.p.z), fx_max(a.p.z, b.p.z)) &&
               emit_needs_subdivision(min3(a.p.z, b.p.z, c.p.z), max3(a.p.z, b.p.z, c.p.z)) &&
               top + 2 <= 16){
                SplitVertex m;
                emit_split_vertex(&m, V3((a.p.x + b.p.x) >> 1, (a.p.y + b.p.y) >> 1, (a.p.z + b.p.z) >> 1),
                                  (int16_t)((int16_t)(a.u + b.u) >> 1), (int16_t)((int16_t)(a.v + b.v) >> 1));
                SplitVertex* p = stack[top++];
                p[0] = c;
                p[1] = a;
                p[2] = m;
                p = stack[top++];
                p[0] = b;
                p[1] = c;
                p[2] = m;
                split = true;
            } else {
                SplitVertex r = a;
                a = b;
                b = c;
                c = r;
            }
        }
        if(split) continue;
        int bin = depth_bin_for_z(max3(a.p.z, b.p.z, c.p.z));
        if(bin < j->cut_bin) bin = j->cut_bin;
        emit_face(j, bin, &a, &b, &c, alpha, average, texture);
    }
}

__attribute__((noinline)) static void emit_stage_face_bins(const EmitJob* j, int face){
    const StageTriangle* st = &j->tris[face];
    const FramePoint* points = j->points;
    int ia = emit_slot(j, st->v[0].point), ib = emit_slot(j, st->v[1].point),
        ic = emit_slot(j, st->v[2].point);
    const FramePoint* pa = &points[ia];
    const FramePoint* pb = &points[ib];
    const FramePoint* pc = &points[ic];
    unsigned ca = pa->clip, cb = pb->clip, cc = pc->clip;
    if(ca & cb & cc & CLIP_SIDES) return;
    fx min_z = min3(pa->position.z, pb->position.z, pc->position.z);
    fx max_z = max3(pa->position.z, pb->position.z, pc->position.z);
    bool deep = !j->far && emit_needs_subdivision(min_z, max_z);
    int bin = depth_bin_for_z(max_z);
    if(!deep && (j->far ? bin >= j->cut_bin : bin < j->cut_bin)) return;

    unsigned texture = stage_triangle_texture(st);
    if(j->water && j->water_textures && emit_through_water(j, st)) texture = j->water_textures[texture];
    bool alpha = st->flags & TRI_ALPHA;
    unsigned average = j->average[texture];
    const TextureReference* source = &j->textures[texture];
    if(j->cache_slots){
        unsigned slot = j->cache_slots[texture];
        if(slot) source = &g_texture_cache_reference[slot - 1];
    } else if(j->far && (g_far_stage.header->surface_features & STAGE_PACKED_FAR_TEXTURES)){
        const TextureReference* raw = stage_far_raw_texture(texture);
        if(!raw && (alpha || !average)) return;
        source = raw;
    }
    uint32_t uva = pack_uv(st->v[0].u, st->v[0].v), uvb = pack_uv(st->v[1].u, st->v[1].v),
             uvc = pack_uv(st->v[2].u, st->v[2].v);
    if(st->flags & SURF_ANIM_UV){
        uint32_t d = emit_scroll_uv(st);
        uva = pack_uv((int16_t)(uva + d), (int16_t)((uva >> 16) + (d >> 16)));
        uvb = pack_uv((int16_t)(uvb + d), (int16_t)((uvb >> 16) + (d >> 16)));
        uvc = pack_uv((int16_t)(uvc + d), (int16_t)((uvc >> 16) + (d >> 16)));
    }
    if(deep || ((ca | cb | cc) & CLIP_GUARD)){
        SplitVertex v[3];
        const FramePoint* fp[3] = {pa, pb, pc};
        const uint32_t uv[3] = {uva, uvb, uvc};
        const int slots[3] = {ia, ib, ic};
        for(int i = 0; i < 3; i++){
            v[i].p = fp[i]->position;
            v[i].u = (int16_t)uv[i];
            v[i].v = (int16_t)(uv[i] >> 16);
            v[i].clip = fp[i]->clip;
            v[i].s = j->screen[slots[i]];
        }
        if(deep) emit_subdivided(j, v, alpha, average, source);
        else emit_face(j, bin, &v[0], &v[1], &v[2], alpha, average, source);
        return;
    }
    uint32_t sa = j->screen[ia], sb = j->screen[ib], sc = j->screen[ic];
    if(screen_area(sa, sb, sc) <= 0 || screen_rowless(sa, sb, sc)) return;
    unsigned mode = alpha ? RM_ALPHA : RM_TEX;
    if(!alpha && average && (j->far || bin < j->flat_bin || (j->flat_small && screen_small(sa, sb, sc))))
        mode = RM_FLAT;
    engine_push(bin, sa, sb, sc, uva, uvb, uvc, mode, average, source);
}
static void emit_stage_face(const EmitJob* j, int face){
    if(j->under_bins && (j->tris[face].flags & TRI_WATER_TINT)){
        g_engine_bin_offset = BIN_COUNT;
        emit_stage_face_bins(j, face);
        g_engine_bin_offset = 0;
    } else emit_stage_face_bins(j, face);
}

static const EmitJob* g_emit_job;
static void emit_stage_slow(int face){ emit_stage_face(g_emit_job, face); }

static void emit_stage_triangles(EmitJob* j, const uint16_t* visible, int count){
    const StageHeader* h = g_game.stage.header;
    j->slow = emit_stage_slow;
    j->water_cx = h->water_centre[0];
    j->water_cy = h->water_centre[1];
    j->water_r2 = h->water_radius_squared;
    j->water_min_z = h->water_min_z;
    j->water_level = h->water_level;
    j->camera_underwater = g_game.camera_underwater;
    g_emit_job = j;
    engine_emit_stage(j, visible, count);
}

static const FigureTriangle* g_figure_draw_triangles;
static const TextureReference* g_figure_draw_textures;
EWRAM_BSS const uint16_t* g_additive_rows;
EWRAM_BSS const uint8_t* g_additive_table;

static void engine_figure_clipped(const FramePoint* points, int face){
    const FigureTriangle* tri = &g_figure_draw_triangles[face];
    int ia = tri->v[0].point, ib = tri->v[2].point, ic = tri->v[1].point;
    bool alpha = tri->flags & TRI_ALPHA;
    ClipVertex t[3] = {{points[ia].position, tri->v[0].u, tri->v[0].v},
                       {points[ib].position, tri->v[2].u, tri->v[2].v},
                       {points[ic].position, tri->v[1].u, tri->v[1].v}};
    engine_emit_clipped(-1, t,
                        alpha             ? RM_ALPHA
                        : g_additive_rows ? RM_ADD
                                          : RM_TEX,
                        0, &g_figure_draw_textures[tri->texture]);
}

static const TextureReference g_shadow_texture = {
    {(uint32_t)shadow_texture.mip[0], (uint32_t)shadow_texture.mip[0]}};
__attribute__((noinline)) static void draw_shadow(const Figure* figure, const CameraView* view){
    const fx radius = FXC(0.2f);
    v3 up = figure == &g_game.player.figure ? g_game.player.up : V3(0, 0, N14_ONE);
    v3 t1 = v3_tangent14(view->view.r[0].x || view->view.r[0].y || view->view.r[0].z
                             ? V3(view->view.r[0].x << 6, view->view.r[0].y << 6, view->view.r[0].z << 6)
                             : V3(N14_ONE, 0, 0),
                         up, V3(N14_ONE, 0, 0));
    v3 t2 = v3_cross14(up, t1);
    v3 g = v3_add(figure->shadow, v3_scale14(up, 2));
    v3 e1 = v3_scale14(t1, radius), e2 = v3_scale14(t2, radius);
    v3 corner[4] = {v3_sub(v3_sub(g, e1), e2), v3_sub(v3_add(g, e1), e2), v3_add(v3_add(g, e1), e2),
                    v3_add(v3_sub(g, e1), e2)};
    static const int16_t uvs[4][2] = {{0, 0}, {512, 0}, {512, 512}, {0, 512}};
    ClipVertex q[4];
    uint32_t clip_and = CLIP_SIDES | CLIP_GUARD, clip_or = 0;
    for(int i = 0; i < 4; i++){
        q[i].p = xform_point(&view->to_screen, corner[i]);
        q[i].u = uvs[i][0];
        q[i].v = uvs[i][1];
        uint32_t c = view_clip_mask(q[i].p);
        clip_and &= c;
        clip_or |= c;
    }
    if(clip_and & CLIP_SIDES) return;
    for(int half = 0; half < 2; half++){
        ClipVertex t[3] = {q[0], q[1 + half], q[2 + half]};

        if(!(clip_or & CLIP_GUARD)){
            uint32_t s0 = screen_pack_position(project_to_screen(t[0].p));
            uint32_t s1 = screen_pack_position(project_to_screen(t[1].p));
            uint32_t s2 = screen_pack_position(project_to_screen(t[2].p));
            if(screen_area(s0, s1, s2) < 0){
                ClipVertex x = t[1];
                t[1] = t[2];
                t[2] = x;
            }
        } else {
            v3 n = v3_cross14(v3_sub(t[1].p, t[0].p), v3_sub(t[2].p, t[0].p));
            if(v3_dot14(n, t[0].p) > 0){
                ClipVertex x = t[1];
                t[1] = t[2];
                t[2] = x;
            }
        }
        engine_emit_clipped(-1, t, RM_DARKEN, 0, &g_shadow_texture);
    }
}

uint8_t g_render_style;
static uint8_t g_wire_line_colour;
static uint8_t g_wire_colour, g_wire_background;

static void render_style_begin(uint8_t* framebuffer){
    static const uint8_t line_rgb[][3] = {{4, 31, 8}, {31, 31, 31}};
    const uint8_t* rgb = line_rgb[g_wire_line_colour];
    g_wire_colour = palette_nearest(rgb[0], rgb[1], rgb[2]);
    g_wire_background = palette_nearest(0, 0, 0);
    volatile uint32_t fill = g_wire_background * 0x01010101u;
    for(int y = 0; y < FRAME_HEIGHT; y++) dma3_fill32(framebuffer + y * FRAME_STRIDE, &fill, FRAME_WIDTH / 4);
}

static inline int wire_outcode(int x, int y){
    return (x < 0) | (x >= FRAME_WIDTH) << 1 | (y < 0) << 2 | (y >= FRAME_HEIGHT) << 3;
}

static void wire_line(uint8_t* fb, int x0, int y0, int x1, int y1){
    int c0 = wire_outcode(x0, y0), c1 = wire_outcode(x1, y1);
    while(c0 | c1){
        if(c0 & c1) return;
        int c = c0 ? c0 : c1, x, y;
        if(c & 8){
            x = x0 + (x1 - x0) * (FRAME_HEIGHT - 1 - y0) / (y1 - y0);
            y = FRAME_HEIGHT - 1;
        } else if(c & 4){
            x = x0 + (x1 - x0) * -y0 / (y1 - y0);
            y = 0;
        } else if(c & 2){
            y = y0 + (y1 - y0) * (FRAME_WIDTH - 1 - x0) / (x1 - x0);
            x = FRAME_WIDTH - 1;
        } else {
            y = y0 + (y1 - y0) * -x0 / (x1 - x0);
            x = 0;
        }
        if(c == c0){
            x0 = x;
            y0 = y;
            c0 = wire_outcode(x0, y0);
        } else {
            x1 = x;
            y1 = y;
            c1 = wire_outcode(x1, y1);
        }
    }
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for(;;){
        frame_put(fb, x0, y0, g_wire_colour);
        if(x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if(e2 >= dy){
            err += dy;
            x0 += sx;
        }
        if(e2 <= dx){
            err += dx;
            y0 += sy;
        }
    }
}

static inline void wire_point(uint8_t* fb, int x, int y){
    if((unsigned)x < FRAME_WIDTH && (unsigned)y < FRAME_HEIGHT) frame_put(fb, x, y, g_wire_colour);
}

void engine_style_face(uint32_t sa, uint32_t sb, uint32_t sc, unsigned mode){
    if(mode == RM_ADD || mode == RM_DARKEN || mode == RM_TINT) return;
    uint8_t* fb = g_engine_fb;
    int ax = screen_x(sa) >> 4, ay = screen_y(sa) >> 4;
    int bx = screen_x(sb) >> 4, by = screen_y(sb) >> 4;
    int cx = screen_x(sc) >> 4, cy = screen_y(sc) >> 4;
    if(g_render_style == RENDER_VERTICES){
        wire_point(fb, ax, ay);
        wire_point(fb, bx, by);
        wire_point(fb, cx, cy);
        return;
    }
    if(g_render_style == RENDER_HIDDEN_LINE){
        DrawEntry e = {0xFFFF, RM_FLAT, g_wire_background, {sa, sb, sc}, {0, 0, 0}, NULL};
        g_render_style = RENDER_NORMAL;
        engine_raster(&e);
        g_render_style = RENDER_HIDDEN_LINE;
    }
    wire_line(fb, ax, ay, bx, by);
    wire_line(fb, bx, by, cx, cy);
    wire_line(fb, cx, cy, ax, ay);
}
