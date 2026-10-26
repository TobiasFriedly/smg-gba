#define REG_WAITCNT  REG16(0x04000204)
#define REG_MEMCTRL  REG32(0x04000800)

#define DMA_32        0x04000000u
#define DMA_SRC_FIXED 0x01000000u

static inline void dma3_copy32(void* dst, const void* src, uint32_t words){
    REG_DMA3SAD = (uint32_t)src;
    REG_DMA3DAD = (uint32_t)dst;
    REG_DMA3CNT = DMA_ENABLE | DMA_32 | words;
}

static inline void dma3_fill32(void* dst, const volatile uint32_t* value, uint32_t words){
    REG_DMA3SAD = (uint32_t)value;
    REG_DMA3DAD = (uint32_t)dst;
    REG_DMA3CNT = DMA_ENABLE | DMA_32 | DMA_SRC_FIXED | words;
}

static void memory_set_fast_waitstates(void){
    static volatile uint32_t probe EWRAM_BSS;
    REG_MEMCTRL = 1 << 5 | 14 << 24;
    probe = 1;
    if(probe != 1) REG_MEMCTRL = 1 << 5 | 13 << 24;
    // ROM wait states stay at crt0's 3/1 (0x4317): 2/1 hangs the cartridge bus on 3DS (open_agb_firm).
}
