#define REG_TM3CNT_L REG16(0x0400010C)
#define REG_TM3CNT_H REG16(0x0400010E)
#define TM_FREQ_64   0x0001
#define PERF_TICKS_PER_SECOND 262144u
#define PERF_FRAME_PIXELS     (FRAME_WIDTH * FRAME_HEIGHT)

enum {
    PERF_LOGIC,
    PERF_SKY,
    PERF_CULL,
    PERF_TRANSFORM,
    PERF_EMIT,
    PERF_FAR,
    PERF_FIGURES,
    PERF_LEVEL,
    PERF_OTHER,
    PERF_COUNT
};

uint32_t g_raster_pixels;
static uint16_t g_perf_mark;
static EWRAM_BSS uint32_t g_perf_ticks[PERF_COUNT];
static EWRAM_BSS uint32_t g_perf_window_frames, g_perf_window_pixels;
static EWRAM_BSS uint32_t g_perf_us[PERF_COUNT];
static EWRAM_BSS uint32_t g_perf_frame_us, g_perf_pixels;

static void perf_init(void){
    REG_TM3CNT_H = 0;
    REG_TM3CNT_L = 0;
    REG_TM3CNT_H = TM_ENABLE | TM_FREQ_64;
    g_perf_mark = REG_TM3CNT_L;
}

static inline void perf_lap(int step){
    uint16_t now = REG_TM3CNT_L;
    g_perf_ticks[step] += (uint16_t)(now - g_perf_mark);
    g_perf_mark = now;
}

static void perf_reset_window(void){
    for(int i = 0; i < PERF_COUNT; i++) g_perf_ticks[i] = 0;
    g_perf_window_frames = g_perf_window_pixels = 0;
}

static uint32_t perf_ticks_to_us(uint32_t ticks, uint32_t frames){
    return (uint32_t)(((uint64_t)ticks * 1000000u + (uint64_t)PERF_TICKS_PER_SECOND * frames / 2) /
                      ((uint64_t)PERF_TICKS_PER_SECOND * frames));
}

static void perf_frame_end(bool gameplay){
    perf_lap(PERF_OTHER);
    uint32_t pixels = g_raster_pixels;
    g_raster_pixels = 0;
    if(!gameplay){
        perf_reset_window();
        return;
    }
    g_perf_window_frames++;
    g_perf_window_pixels += pixels;
    uint32_t total = 0;
    for(int i = 0; i < PERF_COUNT; i++) total += g_perf_ticks[i];
    if(total < PERF_TICKS_PER_SECOND / 2) return;
    uint32_t frames = g_perf_window_frames;
    for(int i = 0; i < PERF_COUNT; i++) g_perf_us[i] = perf_ticks_to_us(g_perf_ticks[i], frames);
    g_perf_frame_us = perf_ticks_to_us(total, frames);
    g_perf_pixels = (g_perf_window_pixels + frames / 2) / frames;
    perf_reset_window();
}
