enum { RM_FLAT, RM_TEX, RM_ALPHA, RM_ADD, RM_DARKEN, RM_TINT };
enum { RENDER_NORMAL, RENDER_WIREFRAME, RENDER_HIDDEN_LINE, RENDER_VERTICES, RENDER_STYLE_COUNT };

typedef struct {
    uint16_t next;
    uint8_t mode, colour;
    uint32_t s[3];
    uint32_t uv[3];
    const TextureReference* texture;
} DrawEntry;

typedef struct {
    v3 p;
    int32_t u, v;
} ClipVertex;

extern const uint16_t engine_recip_table[256];

typedef struct {
    const StageTriangle* tris;
    const FramePoint* points;
    const uint32_t* screen;
    const TextureReference* textures;
    const uint8_t* average;
    const uint8_t* cache_slots;
    const uint16_t* home;
    const int16_t* delta;
    const uint16_t* water_textures;
    const int16_t (*stage_points)[3];
    int32_t far;
    int32_t cut_bin;
    int32_t flat_bin;
    int32_t flat_small;
    int32_t water;
    void (*slow)(int face);
    int32_t water_cx, water_cy, water_r2, water_min_z, water_level, camera_underwater;
    int32_t under_bins;
} EmitJob;

typedef struct {
    const FramePoint* points;
    const uint32_t* screen;
    const uint16_t* order;
    int32_t count;
    int32_t step;
    const FigureTriangle* tris;
    const TextureReference* textures;
    int32_t textured;
    int32_t flat_small;
    void (*clipped)(int face);
} FigureRasterJob;

int g_arena_count;
uint8_t* g_engine_fb;
int32_t g_flat_small;
static const uint8_t g_no_average[STAGE_MAX_TEXTURES];
static const FigureTriangle* g_figure_draw_triangles;
static const TextureReference* g_figure_draw_textures;
EWRAM_BSS const uint16_t* g_additive_rows;
EWRAM_BSS const uint8_t* g_additive_table;

void engine_raster(const DrawEntry* e);
void engine_draw_chain(unsigned head);
IWRAM_CODE static inline int32_t screen_area(uint32_t a, uint32_t b, uint32_t c);
IWRAM_CODE static void engine_out(int bin, uint32_t sa, uint32_t sb, uint32_t sc, uint32_t uva, uint32_t uvb,
                                  uint32_t uvc, unsigned mode, unsigned colour,
                                  const TextureReference* texture);
IWRAM_FN static void engine_emit_clipped(int bin, const ClipVertex* tri, unsigned mode, unsigned colour,
                                         const TextureReference* texture);
static inline const uint16_t* stage_water_table(const StageHeader* h);
void engine_emit_stage(const EmitJob* job, const uint16_t* visible, int count);
static void emit_stage_triangles(EmitJob* j, const uint16_t* visible, int count);
void engine_draw_figure_triangles(const FigureRasterJob* job);
static void engine_figure_clipped(const FramePoint* points, int face);
__attribute__((noinline)) static void draw_shadow(const Figure* figure, const CameraView* view);
