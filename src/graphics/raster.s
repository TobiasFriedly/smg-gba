    .syntax unified
    .arm

    .equ FRAME_STRIDE, 240
    .equ FRAME_WIDTH, 120
    .equ FRAME_HEIGHT, 80
    .equ CLIP_NEAR_Z, 160
    .equ GUARD_SHIFT, 2

    .equ DE_NEXT, 0
    .equ DE_MODE, 2
    .equ DE_COLOUR, 3
    .equ DE_S, 4
    .equ DE_UV, 16
    .equ DE_TEX, 28

    .equ RM_FLAT, 0
    .equ RM_TEX, 1
    .equ RM_ALPHA, 2
    .equ RM_ADD, 3
    .equ RM_DARKEN, 4
    .equ RM_TINT, 5

    .equ MIP1_STEP, 1536

    .section .iwram.engine, "ax", %progbits

.macro RECIP v, e, t
    mov     \e, #0
    cmp     \v, #256
    blo     91f
    cmp     \v, #0x1000000
    movhs   \v, \v, lsr #16
    addhs   \e, \e, #16
    cmp     \v, #0x10000
    movhs   \v, \v, lsr #8
    addhs   \e, \e, #8
    cmp     \v, #0x1000
    movhs   \v, \v, lsr #4
    addhs   \e, \e, #4
    cmp     \v, #0x400
    movhs   \v, \v, lsr #2
    addhs   \e, \e, #2
    cmp     \v, #0x200
    movhs   \v, \v, lsr #1
    addhs   \e, \e, #1
    b       92f
91: cmp     \v, #16
    movlo   \v, \v, lsl #4
    sublo   \e, \e, #4
    cmp     \v, #64
    movlo   \v, \v, lsl #2
    sublo   \e, \e, #2
    cmp     \v, #128
    movlo   \v, \v, lsl #1
    sublo   \e, \e, #1
    mov     \v, \v, lsl #1
    sub     \e, \e, #1
92: ldr     \t, =engine_recip_table - 512
    add     \t, \t, \v, lsl #1
    ldrh    \v, [\t]
.endm

    .equ TRANSFORM_SCREEN, 0
    .equ TRANSFORM_POINT_LIST, 4
    .equ TRANSFORM_REMAINING, 8
    .equ TRANSFORM_INDEX, 12
    .equ TRANSFORM_SLOTS, 16
    .equ TRANSFORM_NEXT, 20
    .equ TRANSFORM_FRAME, 24

    .macro TRANSFORM_POINT
    add     r7, r5, r5, lsl #1
    add     r7, r1, r7, lsl #1
    ldrsh   r8, [r7]
    ldrsh   r9, [r7, #2]
    ldrsh   r10, [r7, #4]

    ldmia   r0, {r7, r11, r12, lr}
    mul     r3, r7, r8
    mla     r3, r11, r9, r3
    mla     r3, r12, r10, r3
    add     r3, lr, r3, asr #8
    add     r7, r0, #16
    ldmia   r7, {r7, r11, r12, lr}
    mul     r4, r7, r8
    mla     r4, r11, r9, r4
    mla     r4, r12, r10, r4
    add     r4, lr, r4, asr #8
    add     r7, r0, #32
    ldmia   r7, {r7, r11, r12, lr}
    mul     r5, r7, r8
    mla     r5, r11, r9, r5
    mla     r5, r12, r10, r5
    add     r5, lr, r5, asr #8
    add     r7, r2, r6, lsl #4
    stmia   r7, {r3, r4, r5}
    mov     r8, #0
    rsb     r11, r3, #0
    cmp     r11, r5
    orrgt   r8, r8, #2
    cmp     r3, r5
    orrgt   r8, r8, #1
    rsb     r11, r4, #0
    cmp     r11, r5
    orrgt   r8, r8, #4
    cmp     r4, r5
    orrgt   r8, r8, #8
    cmp     r5, #CLIP_NEAR_Z
    orrlt   r8, r8, #16
    mov     r11, r5, lsl #GUARD_SHIFT
    cmp     r3, r11
    orrgt   r8, r8, #16
    cmn     r3, r11
    orrlt   r8, r8, #16
    cmp     r4, r11
    orrgt   r8, r8, #16
    cmn     r4, r11
    orrlt   r8, r8, #16
    strb    r8, [r7, #12]
    tst     r8, #16
    bne     8f
    mov     r9, r5, lsr #2
    cmp     r9, #0x10000
    movhs   r9, #0xFF00
    orrhs   r9, r9, #0xFF
    ldr     r10, =lut_project
    ldr     r9, [r10, r9, lsl #2]
    mul     r10, r9, r3
    mov     r10, r10, asr #14
    mov     r11, r10, lsl #6
    sub     r10, r11, r10, lsl #2
    add     r10, r10, #15360
    mul     r11, r9, r4
    mov     r11, r11, asr #14
    mov     r12, r11, lsl #5
    add     r11, r12, r11, lsl #3
    add     r11, r11, #10240
    mov     r10, r10, asr #4
    mov     r10, r10, lsl #16
    mov     r10, r10, lsr #16
    mov     r11, r11, asr #4
    orr     r10, r10, r11, lsl #16
    ldr     r11, [sp, #TRANSFORM_SCREEN]
    str     r10, [r11, r6, lsl #2]
8:
    .endm

    .global engine_transform_points
    .type engine_transform_points, %function
engine_transform_points:
    push    {r4-r11, lr}
    sub     sp, sp, #TRANSFORM_FRAME
    ldr     r6, [r0, #68]
    ldr     r7, [r0, #72]
    str     r6, [sp, #TRANSFORM_SLOTS]
    str     r7, [sp, #TRANSFORM_NEXT]
    ldmia   r0, {r1-r5}
    add     r0, r0, #20
    str     r3, [sp, #TRANSFORM_SCREEN]
    str     r4, [sp, #TRANSFORM_POINT_LIST]
    str     r5, [sp, #TRANSFORM_REMAINING]
    cmp     r4, #0
    beq     2f
1:  ldr     r7, [sp, #TRANSFORM_REMAINING]
    subs    r7, r7, #1
    blt     9f
    str     r7, [sp, #TRANSFORM_REMAINING]
    ldr     r7, [sp, #TRANSFORM_POINT_LIST]
    ldrh    r5, [r7], #2
    str     r7, [sp, #TRANSFORM_POINT_LIST]
    mov     r6, r5
    ldr     r7, [sp, #TRANSFORM_SLOTS]
    cmp     r7, #0
    beq     3f
    add     r7, r7, r5, lsl #1
    ldrh    r6, [r7]
    ldr     r7, [sp, #TRANSFORM_NEXT]
    add     r7, r7, r6, lsl #1
    ldrsh   r6, [r7]
    add     r6, r6, r5
3:  TRANSFORM_POINT
    b       1b
2:  mov     r6, #0
4:  ldr     r7, [sp, #TRANSFORM_REMAINING]
    cmp     r6, r7
    bge     9f
    str     r6, [sp, #TRANSFORM_INDEX]
    mov     r5, r6
    TRANSFORM_POINT
    ldr     r6, [sp, #TRANSFORM_INDEX]
    add     r6, r6, #1
    b       4b
9:  add     sp, sp, #TRANSFORM_FRAME
    pop     {r4-r11, lr}
    bx      lr
    .pool
    .size engine_transform_points, . - engine_transform_points

    .equ RF_FILL, 0
    .equ RF_ROWB, 4
    .equ RF_PDY, 8
    .equ RF_END, 12
    .equ RF_PDX, 16
    .equ RF_LOOP, 20
    .equ RF_BOT, 24
    .equ RF_LONGL, 28
    .equ RF_RET, 32
    .equ RF_MASK, 36
    .equ RF_TEXELS, 40
    .equ RF_J0, 44
    .equ RF_A, 48
    .equ RF_B, 52
    .equ RF_C, 56
    .equ RF_UVA, 60
    .equ RF_UVB, 64
    .equ RF_UVC, 68
    .equ RF_TEX, 72
    .equ RF_MODE, 76
    .equ RF_JM, 80
    .equ RF_SECTION, 84
    .equ RF_KEEP, 88
    .equ RF_FB, 108
    .equ RF_D0, 112
    .equ RF_D1, 116
    .equ RF_D2, 120
    .equ RF_D3, 124
    .equ RF_SIZE, 128

.macro EDGE2 x, dx, vi, vj, row
    mov     r11, \vi, asr #16
    rsb     r12, r11, \vj, asr #16
    mov     \x, \vi, lsl #16
    mov     \x, \x, asr #16
    mov     \dx, \vj, lsl #16
    rsb     \dx, \x, \dx, asr #16
    sub     r11, r12, #16
    cmp     r11, #2032
    bhs     80f
    ldr     r12, [r8, r12, lsl #6]
    mul     r11, \dx, r12
    mov     \dx, r11, asr #3
    b       81f
80: mov     r11, \dx
    bl      edge_slope_slow2
    mov     \dx, r11
81: mov     r11, \vi, asr #16
    rsb     lr, r11, \row, lsl #4
    add     lr, lr, #8
    smull   r11, r12, \dx, lr
    mov     r11, r11, lsr #4
    orr     r11, r11, r12, lsl #28
    add     \x, r11, \x, lsl #12
    add     \x, \x, #0x7F00
    add     \x, \x, #0xFF
.endm

.macro COUNT_PIXELS
    str     r1, [sp, #-4]!
    ldr     r1, =g_raster_pixels
    ldr     r9, [r1]
    add     r9, r9, r3
    str     r9, [r1]
    ldr     r1, [sp], #4
.endm
.macro TEXEL rd, mask=r11
    and     \rd, r1, \mask
    orr     \rd, \rd, r1, lsr #24
    ldrb    \rd, [r12, \rd, lsr #3]
    add     r1, r1, r2
.endm
.macro SPAN_TEX mask
    tst     r0, #1
    beq     61f
    TEXEL   r10, \mask
    ldrb    r9, [r0, #-1]
    orr     r9, r9, r10, lsl #8
    strh    r9, [r0, #-1]
    add     r0, r0, #1
    subs    r3, r3, #1
    beq     69f
61: tst     r0, #2
    beq     62f
    cmp     r3, #2
    blt     65f
    TEXEL   r10, \mask
    TEXEL   r9, \mask
    orr     r10, r10, r9, lsl #8
    strh    r10, [r0], #2
    subs    r3, r3, #2
    beq     69f
62: subs    r3, r3, #4
    blt     64f
63: TEXEL   r10, \mask
    TEXEL   r9, \mask
    orr     r10, r10, r9, lsl #8
    TEXEL   r9, \mask
    orr     r10, r10, r9, lsl #16
    TEXEL   r9, \mask
    orr     r10, r10, r9, lsl #24
    str     r10, [r0], #4
    subs    r3, r3, #4
    bge     63b
64: adds    r3, r3, #4
    beq     69f
    cmp     r3, #2
    blt     65f
    TEXEL   r10, \mask
    TEXEL   r9, \mask
    orr     r10, r10, r9, lsl #8
    strh    r10, [r0], #2
    subs    r3, r3, #2
    beq     69f
65: TEXEL   r10, \mask
    ldrb    r9, [r0, #1]
    orr     r10, r10, r9, lsl #8
    strh    r10, [r0]
.endm

    .global engine_raster
    .type engine_raster, %function
engine_raster:
    push    {r4-r11, lr}
    sub     sp, sp, #RF_SIZE
    ldr     r1, =g_engine_fb
    ldr     r1, [r1]
    str     r1, [sp, #RF_FB]
    ldmia   r0, {r1-r8}
    mov     r1, r1, lsr #16
    str     r1, [sp, #RF_MODE]
    add     r0, sp, #RF_A
    stmia   r0, {r2-r8}
    bl      raster_face
    add     sp, sp, #RF_SIZE
    pop     {r4-r11, lr}
    bx      lr
    .size engine_raster, . - engine_raster

    .global engine_draw_chain
    .type engine_draw_chain, %function
engine_draw_chain:
    push    {r4-r11, lr}
    sub     sp, sp, #RF_SIZE
    ldr     r1, =g_engine_fb
    ldr     r1, [r1]
    str     r1, [sp, #RF_FB]
    mov     r9, r0
dc_next:
    mvn     r1, #0
    cmp     r9, r1, lsr #16
    beq     dc_done
    ldr     r0, =g_arena
    add     r0, r0, r9, lsl #5
    ldmia   r0, {r1-r8}
    mov     r9, r1, lsl #16
    mov     r9, r9, lsr #16
    str     r9, [sp, #RF_D0]
    mov     r1, r1, lsr #16
    str     r1, [sp, #RF_MODE]
    add     r0, sp, #RF_A
    stmia   r0, {r2-r8}
    bl      raster_face
    ldr     r9, [sp, #RF_D0]
    b       dc_next
dc_done:
    add     sp, sp, #RF_SIZE
    pop     {r4-r11, lr}
    bx      lr
    .pool
    .size engine_draw_chain, . - engine_draw_chain

    .equ FJ_POINTS, 0
    .equ FJ_SCREEN, 4
    .equ FJ_ORDER, 8
    .equ FJ_COUNT, 12
    .equ FJ_STEP, 16
    .equ FJ_TRIS, 20
    .equ FJ_TEXTURES, 24
    .equ FJ_TEXTURED, 28
    .equ FJ_FLAT_SMALL, 32
    .equ FJ_CLIPPED, 36

    .global engine_draw_figure_triangles
    .type engine_draw_figure_triangles, %function
engine_draw_figure_triangles:
    push    {r4-r11, lr}
    sub     sp, sp, #RF_SIZE
    ldr     r1, =g_engine_fb
    ldr     r1, [r1]
    str     r1, [sp, #RF_FB]
    str     r0, [sp, #RF_D0]
    ldr     r4, [r0, #FJ_ORDER]
    ldr     r5, [r0, #FJ_COUNT]

fg_reload:
    ldr     r0, [sp, #RF_D0]
    ldr     r8, [r0, #FJ_STEP]
    ldr     r9, [r0, #FJ_TRIS]
    ldr     r10, [r0, #FJ_POINTS]
    ldr     r11, [r0, #FJ_SCREEN]
fg_next:
    subs    r5, r5, #1
    blt     fg_done
    ldrh    r6, [r4]
    add     r4, r4, r8
    add     r1, r6, r6, lsl #1
    add     r1, r1, r6, lsl #3
    add     r7, r9, r1, lsl #1
    ldrh    r1, [r7, #0]
    ldrh    r2, [r7, #12]
    ldrh    r3, [r7, #6]
    add     r12, r10, r1, lsl #4
    ldrb    r0, [r12, #12]
    add     r12, r10, r2, lsl #4
    ldrb    lr, [r12, #12]
    and     r12, r0, lr
    orr     r0, r0, lr
    add     lr, r10, r3, lsl #4
    ldrb    lr, [lr, #12]
    and     r12, r12, lr
    orr     r0, r0, lr
    tst     r12, #15
    bne     fg_next
    tst     r0, #16
    bne     fg_clip
    ldr     r1, [r11, r1, lsl #2]
    ldr     r2, [r11, r2, lsl #2]
    ldr     r3, [r11, r3, lsl #2]

    mov     r0, r1, asr #16
    rsb     r12, r0, r3, asr #16
    rsb     lr, r0, r2, asr #16
    mov     r0, r1, lsl #16
    mov     r0, r0, asr #16
    mov     r6, r2, lsl #16
    rsb     r6, r0, r6, asr #16
    mul     r12, r6, r12
    mov     r6, r3, lsl #16
    rsb     r6, r0, r6, asr #16
    mul     lr, r6, lr
    subs    r12, r12, lr
    ble     fg_next
    str     r4, [sp, #RF_D2]
    str     r5, [sp, #RF_D1]
    ldr     r0, [sp, #RF_D0]
    add     r9, sp, #RF_A
    stmia   r9, {r1-r3}
    ldrh    r9, [r7, #20]
    ldrh    r10, [r7, #18]
    ldr     r11, [r0, #FJ_TEXTURES]
    add     r10, r11, r10, lsl #3
    str     r10, [sp, #RF_TEX]
    tst     r9, #8
    movne   r10, #RM_ALPHA
    bne     fg_textured
    ldr     r10, [r0, #FJ_TEXTURED]
    movs    r11, r9, lsr #8
    beq     fg_textured
    ldr     r12, [r0, #FJ_FLAT_SMALL]
    cmp     r12, #0
    beq     fg_textured

    mov     r12, r1, lsl #16
    mov     r8, r12
    cmp     r12, r2, lsl #16
    movgt   r12, r2, lsl #16
    cmp     r8, r2, lsl #16
    movlt   r8, r2, lsl #16
    cmp     r12, r3, lsl #16
    movgt   r12, r3, lsl #16
    cmp     r8, r3, lsl #16
    movlt   r8, r3, lsl #16
    sub     r8, r8, r12
    cmp     r8, #0x400000
    bgt     fg_textured
    mov     r12, r1, asr #16
    mov     r8, r12
    cmp     r12, r2, asr #16
    movgt   r12, r2, asr #16
    cmp     r8, r2, asr #16
    movlt   r8, r2, asr #16
    cmp     r12, r3, asr #16
    movgt   r12, r3, asr #16
    cmp     r8, r3, asr #16
    movlt   r8, r3, asr #16
    sub     r8, r8, r12
    cmp     r8, #0x30
    bgt     fg_textured
    mov     r11, r11, lsl #8
    str     r11, [sp, #RF_MODE]
    bl      raster_face
    ldr     r4, [sp, #RF_D2]
    ldr     r5, [sp, #RF_D1]
    b       fg_reload
fg_textured:
    str     r10, [sp, #RF_MODE]
    ldrh    r1, [r7, #2]
    ldrh    r2, [r7, #4]
    orr     r1, r1, r2, lsl #16
    ldrh    r2, [r7, #14]
    ldrh    r3, [r7, #16]
    orr     r2, r2, r3, lsl #16
    ldrh    r3, [r7, #8]
    ldrh    r8, [r7, #10]
    orr     r3, r3, r8, lsl #16
    add     r8, sp, #RF_UVA
    stmia   r8, {r1-r3}
    bl      raster_face
    ldr     r4, [sp, #RF_D2]
    ldr     r5, [sp, #RF_D1]
    b       fg_reload
fg_clip:
    str     r4, [sp, #RF_D2]
    str     r5, [sp, #RF_D1]
    mov     r0, r6
    ldr     r1, [sp, #RF_D0]
    ldr     r1, [r1, #FJ_CLIPPED]
    mov     lr, pc
    bx      r1
    ldr     r4, [sp, #RF_D2]
    ldr     r5, [sp, #RF_D1]
    b       fg_reload
fg_done:
    add     sp, sp, #RF_SIZE
    pop     {r4-r11, lr}
    bx      lr
    .size engine_draw_figure_triangles, . - engine_draw_figure_triangles

    .equ EJ_TRIS, 0
    .equ EJ_POINTS, 4
    .equ EJ_SCREEN, 8
    .equ EJ_TEXTURES, 12
    .equ EJ_AVERAGE, 16
    .equ EJ_CACHE, 20
    .equ EJ_HOME, 24
    .equ EJ_DELTA, 28
    .equ EJ_FAR, 40
    .equ EJ_CUT, 44
    .equ EJ_FLATBIN, 48
    .equ EJ_FLATSMALL, 52
    .equ EJ_WATERTEX, 32
    .equ EJ_STAGEPTS, 36
    .equ EJ_WATER, 56
    .equ EJ_SLOW, 60
    .equ EJ_WCX, 64
    .equ EJ_WCY, 68
    .equ EJ_WR2, 72
    .equ EJ_WMINZ, 76
    .equ EJ_WLEVEL, 80
    .equ EJ_UNDERWATER, 84
    .equ EJ_UNDERBINS, 88
    .equ SURF_ANIM_UV, 4
    .equ TRI_ALPHA, 8
    .equ ENGINE_ARENA_MAX, 2048
    .equ ES_JOB, 0
    .equ ES_END, 4
    .equ ES_FACE, 8
    .equ ES_FRAME, 32

    .global engine_emit_stage
    .type engine_emit_stage, %function
engine_emit_stage:
    push    {r4-r11, lr}
    sub     sp, sp, #ES_FRAME
    str     r0, [sp, #ES_JOB]
    add     r2, r1, r2, lsl #1
    str     r2, [sp, #ES_END]
    mov     r4, r1
es_next:
    ldr     r2, [sp, #ES_END]
    cmp     r4, r2
    bhs     es_done
    ldrh    r5, [r4], #2
    ldr     r11, [sp, #ES_JOB]
    ldr     r6, [r11, #EJ_TRIS]
    add     r0, r5, r5, lsl #1
    add     r6, r6, r0, lsl #3
    ldmia   r6, {r0-r3, r7, r8}
    add     r6, sp, #ES_FACE
    stmia   r6, {r0-r3, r7, r8}
    tst     r8, #SURF_ANIM_UV
    bne     es_slow
    mov     r0, r0, lsl #16
    mov     r1, r1, lsr #16
    mov     r2, r3, lsl #16
    mov     r3, r2, lsr #16
    mov     r2, r1
    mov     r1, r0, lsr #16
    ldr     r7, [r11, #EJ_HOME]
    cmp     r7, #0
    beq     1f
    ldr     r8, [r11, #EJ_DELTA]
    .irp r, r1, r2, r3
    add     r9, r7, \r, lsl #1
    ldrh    r9, [r9]
    add     r9, r8, r9, lsl #1
    ldrsh   r9, [r9]
    add     \r, \r, r9
    .endr
1:  ldr     r7, [r11, #EJ_POINTS]
    add     r8, r7, r1, lsl #4
    ldr     r12, [r8, #8]
    ldrb    r8, [r8, #12]
    add     r9, r7, r2, lsl #4
    ldr     lr, [r9, #8]
    ldrb    r9, [r9, #12]
    add     r10, r7, r3, lsl #4
    ldr     r7, [r10, #8]
    ldrb    r10, [r10, #12]
    and     r0, r8, r9
    and     r0, r0, r10
    tst     r0, #15
    bne     es_next
    orr     r0, r8, r9
    orr     r0, r0, r10
    tst     r0, #16
    bne     es_slow
    mov     r8, r12
    cmp     lr, r8
    movlt   r8, lr
    cmp     r7, r8
    movlt   r8, r7
    mov     r9, r12
    cmp     lr, r9
    movgt   r9, lr
    cmp     r7, r9
    movgt   r9, r7
    ldr     r0, [r11, #EJ_FAR]
    cmp     r0, #0
    bne     2f

    bic     r10, r8, r8, asr #31
    add     r10, r10, #768
    sub     r12, r9, r8
    cmp     r12, r10, lsl #2
    bgt     es_slow
2:  mov     r10, r9, lsr #6
    ldr     r12, =2047
    cmp     r10, r12
    movhi   r10, r12
    ldr     r12, =lut_zbin
    ldrb    r10, [r12, r10]
    ldr     r12, [r11, #EJ_CUT]
    cmp     r0, #0
    beq     3f
    cmp     r10, r12
    bhs     es_next
    b       4f
3:  cmp     r10, r12
    blo     es_next
4:  ldr     r7, [r11, #EJ_SCREEN]
    ldr     r1, [r7, r1, lsl #2]
    ldr     r2, [r7, r2, lsl #2]
    ldr     r3, [r7, r3, lsl #2]

    mov     r7, r1, asr #16
    rsb     r8, r7, r3, asr #16
    rsb     r9, r7, r2, asr #16
    mov     r7, r1, lsl #16
    mov     r7, r7, asr #16
    mov     r12, r2, lsl #16
    rsb     r12, r7, r12, asr #16
    mov     lr, r3, lsl #16
    rsb     lr, r7, lr, asr #16
    mul     r7, r8, r12
    mul     r8, lr, r9
    subs    r7, r7, r8
    ble     es_next
    ldr     r6, [sp, #ES_FACE + 16]
    mov     r7, r6, lsr #16
    bic     r7, r7, #0xFC00
    ldr     r0, [r11, #EJ_WATER]
    cmp     r0, #0
    bne     es_water
es_water_back:
    ldr     r8, [r11, #EJ_AVERAGE]
    ldrb    r8, [r8, r7]
    ldrb    r9, [sp, #ES_FACE + 20]
    tst     r9, #TRI_ALPHA
    movne   r12, #RM_ALPHA
    bne     5f
    mov     r12, #RM_TEX
    cmp     r8, #0
    beq     5f
    ldr     r0, [r11, #EJ_FAR]
    cmp     r0, #0
    movne   r12, #RM_FLAT
    bne     5f
    ldr     r0, [r11, #EJ_FLATBIN]
    cmp     r10, r0
    movlt   r12, #RM_FLAT
    blt     5f
    ldr     r0, [r11, #EJ_FLATSMALL]
    cmp     r0, #0
    beq     5f

    mov     r0, r1, lsl #16
    mov     r5, r0
    cmp     r0, r2, lsl #16
    movgt   r0, r2, lsl #16
    cmp     r5, r2, lsl #16
    movlt   r5, r2, lsl #16
    cmp     r0, r3, lsl #16
    movgt   r0, r3, lsl #16
    cmp     r5, r3, lsl #16
    movlt   r5, r3, lsl #16
    sub     r5, r5, r0
    cmp     r5, #0x400000
    bgt     5f
    mov     r0, r1, asr #16
    mov     r5, r0
    cmp     r0, r2, asr #16
    movgt   r0, r2, asr #16
    cmp     r5, r2, asr #16
    movlt   r5, r2, asr #16
    cmp     r0, r3, asr #16
    movgt   r0, r3, asr #16
    cmp     r5, r3, asr #16
    movlt   r5, r3, asr #16
    sub     r5, r5, r0
    cmp     r5, #0x30
    movle   r12, #RM_FLAT
5:  ldr     r0, [r11, #EJ_FAR]
    cmp     r0, #0
    beq     6f
    cmp     r12, #RM_FLAT
    bne     es_slow_face
6:  ldr     r0, [r11, #EJ_TEXTURES]
    add     r9, r0, r7, lsl #3
    ldr     r0, [r11, #EJ_CACHE]
    cmp     r0, #0
    beq     7f
    ldrb    r0, [r0, r7]
    cmp     r0, #0
    ldrne   r9, =g_texture_cache_reference - 8
    addne   r9, r9, r0, lsl #3
7:  ldr     r7, =g_arena_count
    ldr     r0, [r7]
    cmp     r0, #ENGINE_ARENA_MAX
    bhs     es_next
    add     r5, r0, #1
    str     r5, [r7]
    ldr     r7, =g_triangle_bin_heads
    ldrb    r5, [sp, #ES_FACE + 20]
    tst     r5, #1
    ldrne   r5, [r11, #EJ_UNDERBINS]
    addne   r7, r7, r5
    add     r7, r7, r10, lsl #1
    ldrh    r5, [r7]
    strh    r0, [r7]
    ldr     r7, =g_arena
    add     lr, r7, r0, lsl #5
    orr     r0, r5, r12, lsl #16
    orr     r0, r0, r8, lsl #24
    add     r10, sp, #ES_FACE
    ldmia   r10, {r5, r7, r8, r10, r12}
    mov     r5, r5, lsr #16
    orr     r5, r5, r7, lsl #16
    mov     r7, r8
    mov     r8, r10, lsr #16
    orr     r8, r8, r12, lsl #16
    stmia   lr, {r0-r3, r5, r7-r9}
    b       es_next

es_water:
    cmp     r0, #3
    bne     1f
    ldr     r0, [r11, #EJ_UNDERWATER]
    cmp     r0, #0
    bne     es_tint
    ldrb    r0, [sp, #ES_FACE + 20]
    tst     r0, #1
    bne     es_tint
    b       es_water_back
1:  ldr     lr, [r11, #EJ_STAGEPTS]
    ldrh    r5, [sp, #ES_FACE + 0]
    add     r5, r5, r5, lsl #1
    add     r5, lr, r5, lsl #1
    cmp     r0, #2
    bne     2f
    ldrsh   r8, [r5]
    ldr     r9, [r11, #EJ_WCX]
    sub     r8, r8, r9
    mul     r9, r8, r8
    mov     r9, r9, asr #8
    ldrsh   r8, [r5, #2]
    ldr     r12, [r11, #EJ_WCY]
    sub     r8, r8, r12
    mul     r12, r8, r8
    add     r9, r9, r12, asr #8
    ldr     r12, [r11, #EJ_WR2]
    cmp     r9, r12
    bgt     es_water_back
    ldrsh   r8, [r5, #4]
    ldr     r12, [r11, #EJ_WMINZ]
    cmp     r8, r12
    blt     es_water_back
2:  ldrsh   r8, [r5, #4]
    ldrh    r5, [sp, #ES_FACE + 6]
    add     r5, r5, r5, lsl #1
    add     r5, lr, r5, lsl #1
    ldrsh   r5, [r5, #4]
    cmp     r5, r8
    movgt   r8, r5
    ldrh    r5, [sp, #ES_FACE + 12]
    add     r5, r5, r5, lsl #1
    add     r5, lr, r5, lsl #1
    ldrsh   r5, [r5, #4]
    cmp     r5, r8
    movgt   r8, r5
    ldr     r5, [r11, #EJ_WLEVEL]
    ldr     r0, [r11, #EJ_UNDERWATER]
    cmp     r0, #0
    beq     3f
    cmp     r8, r5
    bge     es_tint
    b       es_water_back
3:  cmp     r8, r5
    blt     es_tint
    b       es_water_back
es_tint:
    ldr     r0, [r11, #EJ_WATERTEX]
    add     r0, r0, r7, lsl #1
    ldrh    r7, [r0]
    b       es_water_back
es_slow_face:
    ldrh    r5, [r4, #-2]
es_slow:
    mov     r0, r5
    ldr     r11, [sp, #ES_JOB]
    ldr     r1, [r11, #EJ_SLOW]
    mov     lr, pc
    bx      r1
    b       es_next
es_done:
    add     sp, sp, #ES_FRAME
    pop     {r4-r11, lr}
    bx      lr
    .pool
    .size engine_emit_stage, . - engine_emit_stage

raster_face:
    str     lr, [sp, #RF_RET]
    add     r0, sp, #RF_A
    ldmia   r0, {r1-r3}
    ldr     r12, =g_render_style
    ldrb    r12, [r12]
    cmp     r12, #0
    bne     rf_style
    mov     r7, r1, asr #16
    mov     r8, r2, asr #16
    mov     r9, r3, asr #16
    mov     r10, #0

    cmp     r8, r7
    bge     1f
    cmp     r8, r9
    bgt     2f
    mov     r12, r1
    mov     r1, r2
    mov     r2, r3
    mov     r3, r12
    mov     r12, r7
    mov     r7, r8
    mov     r8, r9
    mov     r9, r12
    mov     r10, #1
    b       3f
1:  cmp     r9, r7
    bge     3f
2:  mov     r12, r3
    mov     r3, r2
    mov     r2, r1
    mov     r1, r12
    mov     r12, r9
    mov     r9, r8
    mov     r8, r7
    mov     r7, r12
    mov     r10, #2
3:  stmia   r0, {r1-r3}

    cmp     r9, r8
    movgt   r11, r9
    movgt   r12, r8
    movgt   lr, #1
    movle   r11, r8
    movle   r12, r9
    movle   lr, #0
    str     lr, [sp, #RF_LONGL]
    add     r4, r7, #7
    mov     r4, r4, asr #4
    add     r11, r11, #7
    mov     r11, r11, asr #4
    add     r12, r12, #7
    mov     r12, r12, asr #4
    cmp     r4, #0
    movlt   r4, #0
    cmp     r11, #FRAME_HEIGHT
    movgt   r11, #FRAME_HEIGHT
    cmp     r4, r11
    bge     rf_done
    cmp     r12, r4
    movlt   r12, r4
    cmp     r12, r11
    movgt   r12, r11
    str     r4, [sp, #RF_J0]
    str     r12, [sp, #RF_JM]
    ldr     r0, [sp, #RF_FB]
    rsb     r5, r12, r12, lsl #4
    add     r5, r0, r5, lsl #4
    str     r5, [sp, #RF_END]
    rsb     r5, r11, r11, lsl #4
    add     r5, r0, r5, lsl #4
    str     r5, [sp, #RF_BOT]
    ldrb    r0, [sp, #RF_MODE]
    cmp     r0, #RM_FLAT
    cmpne   r0, #RM_TINT
    beq     rf_edges

    add     r0, sp, #RF_UVA
    ldmia   r0, {r4-r6}
    cmp     r10, #1
    moveq   r12, r4
    moveq   r4, r5
    moveq   r5, r6
    moveq   r6, r12
    cmp     r10, #2
    moveq   r12, r6
    moveq   r6, r5
    moveq   r5, r4
    moveq   r4, r12
    mov     r0, r1, lsl #16
    mov     r0, r0, asr #16
    mov     r8, r2, lsl #16
    rsb     r8, r0, r8, asr #16
    mov     r9, r3, lsl #16
    rsb     r9, r0, r9, asr #16
    rsb     r11, r7, r2, asr #16
    rsb     r12, r7, r3, asr #16
    mul     r10, r8, r12
    mul     r1, r11, r9
    subs    r10, r10, r1
    ble     tx_zero
    mov     r1, r4, lsl #16
    mov     r1, r1, asr #16
    mov     r2, r5, lsl #16
    rsb     r2, r1, r2, asr #16
    mov     r3, r6, lsl #16
    rsb     r3, r1, r3, asr #16
    mul     r1, r2, r12
    mul     lr, r3, r11
    sub     r1, r1, lr
    mul     lr, r8, r3
    mul     r3, r9, r2
    sub     lr, lr, r3
    mov     r3, r4, asr #16
    rsb     r2, r3, r5, asr #16
    rsb     r3, r3, r6, asr #16
    mul     r5, r2, r12
    mul     r6, r3, r11
    sub     r5, r5, r6
    mul     r6, r8, r3
    mul     r12, r9, r2
    sub     r6, r6, r12

    RECIP   r10, r11, r12
    add     r11, r11, #16
    cmp     r11, #32
    bge     tx_huge
tx_scaled:
    rsb     r12, r11, #32
    smull   r2, r3, r1, r10
    mov     r1, r2, lsr r11
    orr     r1, r1, r3, lsl r12
    smull   r2, r3, lr, r10
    mov     lr, r2, lsr r11
    orr     lr, lr, r3, lsl r12
    smull   r2, r3, r5, r10
    mov     r5, r2, lsr r11
    orr     r5, r5, r3, lsl r12
    smull   r2, r3, r6, r10
    mov     r6, r2, lsr r11
    orr     r6, r6, r3, lsl r12
    b       tx_have
tx_huge:
    sub     r12, r11, #31
    mov     r1, r1, asr r12
    mov     lr, lr, asr r12
    mov     r5, r5, asr r12
    mov     r6, r6, asr r12
    mov     r11, #31
    b       tx_scaled
tx_zero:
    mov     r1, #0
    mov     lr, #0
    mov     r5, #0
    mov     r6, #0
tx_have:

    cmp     r1, #0
    rsblt   r2, r1, #0
    movge   r2, r1
    cmp     r5, #0
    rsblt   r3, r5, #0
    movge   r3, r5
    cmp     r3, r2
    movgt   r2, r3
    ldr     r3, [sp, #RF_TEX]
    cmp     r2, #MIP1_STEP
    ldrlt   r2, [r3]
    ldrge   r2, [r3, #4]
    movlt   r3, #0x1F00
    movge   r3, #0x1E00
    str     r2, [sp, #RF_TEXELS]
    str     r3, [sp, #RF_MASK]

    rsb     r0, r0, #8
    ldr     r2, [sp, #RF_J0]
    mov     r2, r2, lsl #4
    add     r2, r2, #8
    sub     r7, r2, r7
    mov     r2, r4, lsl #16
    mov     r2, r2, asr #12
    mul     r3, r1, r0
    add     r2, r2, r3, asr #4
    mul     r3, lr, r7
    add     r2, r2, r3, asr #4
    mov     r3, r4, asr #16
    mov     r3, r3, lsl #4
    mul     r8, r5, r0
    add     r3, r3, r8, asr #4
    mul     r8, r6, r7
    add     r3, r3, r8, asr #4

    mov     r3, r3, lsl #19
    mov     r3, r3, lsr #19
    add     r3, r3, r2, lsl #19
    add     r3, r3, #0x40000
    str     r3, [sp, #RF_ROWB]
    add     r6, r6, lr, lsl #19
    str     r6, [sp, #RF_PDY]
    add     r5, r5, r1, lsl #19
    str     r5, [sp, #RF_PDX]

rf_edges:
    add     r0, sp, #RF_A
    ldmia   r0, {r1-r3}
    ldr     r10, [sp, #RF_LONGL]
    ldr     r9, [sp, #RF_J0]
    ldr     r8, =lut_delta
    cmp     r10, #0
    movne   r0, r3
    moveq   r0, r2
    EDGE2   r4, r5, r1, r0, r9
    ldr     r6, [sp, #RF_JM]
    cmp     r6, r9
    beq     1f
    mov     r6, #1
    str     r6, [sp, #RF_SECTION]
    cmp     r10, #0
    movne   r0, r2
    moveq   r0, r3
    EDGE2   r6, r7, r1, r0, r9
    b       2f
1:  mov     r0, #0
    str     r0, [sp, #RF_SECTION]
    ldr     r0, [sp, #RF_BOT]
    str     r0, [sp, #RF_END]
    cmp     r10, #0
    movne   r1, r2
    movne   r0, r3
    moveq   r1, r3
    moveq   r0, r2
    EDGE2   r6, r7, r1, r0, r9
2:
    cmp     r10, #0
    bne     3f
    mov     r0, r4
    mov     r4, r6
    mov     r6, r0
    mov     r0, r5
    mov     r5, r7
    mov     r7, r0
3:  ldr     r0, [sp, #RF_FB]
    rsb     r1, r9, r9, lsl #4
    add     r8, r0, r1, lsl #4
    ldrb    r0, [sp, #RF_MODE]
    cmp     r0, #RM_TEX
    beq     rf_tex
    cmp     r0, #RM_TINT
    beq     tn_start
    cmp     r0, #RM_FLAT
    bne     tc_start

    ldrb    r2, [sp, #RF_MODE + 1]
    orr     r2, r2, r2, lsl #8
    orr     r2, r2, r2, lsl #16
    adr     r0, fl_section
    str     r0, [sp, #RF_LOOP]
fl_section:
    ldr     r9, [sp, #RF_END]
    cmp     r8, r9
    bhs     rf_section2
fl_row:
    mov     r0, r4, asr #16
    cmp     r0, #0
    movlt   r0, #0
    mov     r3, r6, asr #16
    cmp     r3, #FRAME_WIDTH
    movgt   r3, #FRAME_WIDTH
    subs    r3, r3, r0
    ble     1f
    COUNT_PIXELS
    ldr     r9, [sp, #RF_END]
    add     r0, r8, r0
    bl      FillFlat
1:  add     r4, r4, r5
    add     r6, r6, r7
    add     r8, r8, #FRAME_STRIDE
    cmp     r8, r9
    blo     fl_row
    b       rf_section2

tn_start:
    adr     r0, tn_section
    str     r0, [sp, #RF_LOOP]
tn_section:
    ldr     r9, [sp, #RF_END]
    cmp     r8, r9
    bhs     rf_section2
tn_row:
    mov     r0, r4, asr #16
    cmp     r0, #0
    movlt   r0, #0
    mov     r3, r6, asr #16
    cmp     r3, #FRAME_WIDTH
    movgt   r3, #FRAME_WIDTH
    subs    r3, r3, r0
    ble     1f
    COUNT_PIXELS
    add     r0, r8, r0
    bl      FillTint
1:  add     r4, r4, r5
    add     r6, r6, r7
    add     r8, r8, #FRAME_STRIDE
    ldr     r9, [sp, #RF_END]
    cmp     r8, r9
    blo     tn_row
    b       rf_section2

rf_section2:
    ldr     r0, [sp, #RF_SECTION]
    cmp     r0, #0
    beq     rf_done
    mov     r0, #0
    str     r0, [sp, #RF_SECTION]
    ldr     r0, [sp, #RF_BOT]
    cmp     r8, r0
    bhs     rf_done
    str     r0, [sp, #RF_END]
    add     r0, sp, #RF_KEEP
    stmia   r0, {r2, r8, r11, r12, lr}
    ldr     r10, [sp, #RF_LONGL]
    ldr     r9, [sp, #RF_JM]
    ldr     r8, =lut_delta
    cmp     r10, #0
    ldrne   r1, [sp, #RF_B]
    ldrne   r0, [sp, #RF_C]
    ldreq   r1, [sp, #RF_C]
    ldreq   r0, [sp, #RF_B]
    cmp     r10, #0
    bne     1f
    EDGE2   r4, r5, r1, r0, r9
    b       2f
1:  EDGE2   r6, r7, r1, r0, r9
2:  add     r0, sp, #RF_KEEP
    ldmia   r0, {r2, r8, r11, r12, lr}
    ldr     pc, [sp, #RF_LOOP]
rf_done:
    ldr     pc, [sp, #RF_RET]

rf_style:
    ldrb    r12, [sp, #RF_MODE]
    mov     r0, r1
    mov     r1, r2
    mov     r2, r3
    mov     r3, r12
    ldr     r12, =engine_style_face
    mov     lr, pc
    bx      r12
    ldr     pc, [sp, #RF_RET]

rf_tex:
    ldr     r2, [sp, #RF_PDX]
    ldr     r11, [sp, #RF_PDY]
    ldr     r12, [sp, #RF_TEXELS]
    ldr     lr, [sp, #RF_ROWB]
    add     r0, sp, #RF_A
    ldmia   r0, {r0, r1, r3}
    mov     r0, r0, lsl #16
    mov     r1, r1, lsl #16
    mov     r3, r3, lsl #16
    cmp     r0, r1
    movgt   r9, r1
    movle   r9, r0
    movlt   r0, r1
    cmp     r3, r9
    movlt   r9, r3
    cmp     r3, r0
    movgt   r0, r3
    ldr     r1, [sp, #RF_MASK]
    cmp     r9, #16 << 16
    blt     1f
    cmp     r0, #(FRAME_WIDTH * 16 - 16) << 16
    bge     1f
    cmp     r1, #0x1F00
    beq     t0u_start
    b       t1u_start
1:  cmp     r1, #0x1F00
    bne     t1_start
t0_start:
    adr     r0, t0_section
    str     r0, [sp, #RF_LOOP]
t0_section:
    ldr     r9, [sp, #RF_END]
    cmp     r8, r9
    bhs     rf_section2
t0_row:
    mov     r0, r4, asr #16
    cmp     r0, #0
    movlt   r0, #0
    mov     r3, r6, asr #16
    cmp     r3, #FRAME_WIDTH
    movgt   r3, #FRAME_WIDTH
    subs    r3, r3, r0
    ble     69f
    COUNT_PIXELS
    mla     r1, r0, r2, lr
    add     r0, r8, r0
    SPAN_TEX #0x1F00
69: add     lr, lr, r11
    add     r4, r4, r5
    add     r6, r6, r7
    add     r8, r8, #FRAME_STRIDE
    ldr     r9, [sp, #RF_END]
    cmp     r8, r9
    blo     t0_row
    b       rf_section2
t1_start:
    adr     r0, t1_section
    str     r0, [sp, #RF_LOOP]
t1_section:
    ldr     r9, [sp, #RF_END]
    cmp     r8, r9
    bhs     rf_section2
t1_row:
    mov     r0, r4, asr #16
    cmp     r0, #0
    movlt   r0, #0
    mov     r3, r6, asr #16
    cmp     r3, #FRAME_WIDTH
    movgt   r3, #FRAME_WIDTH
    subs    r3, r3, r0
    ble     69f
    COUNT_PIXELS
    mla     r1, r0, r2, lr
    add     r0, r8, r0
    SPAN_TEX #0x1E00
69: add     lr, lr, r11
    add     r4, r4, r5
    add     r6, r6, r7
    add     r8, r8, #FRAME_STRIDE
    ldr     r9, [sp, #RF_END]
    cmp     r8, r9
    blo     t1_row
    b       rf_section2
t0u_start:
    adr     r0, t0u_section
    str     r0, [sp, #RF_LOOP]
t0u_section:
    ldr     r9, [sp, #RF_END]
    cmp     r8, r9
    bhs     rf_section2
t0u_row:
    mov     r0, r4, asr #16
    rsbs    r3, r0, r6, asr #16
    ble     69f
    COUNT_PIXELS
    mla     r1, r0, r2, lr
    add     r0, r8, r0
    SPAN_TEX #0x1F00
69: add     lr, lr, r11
    add     r4, r4, r5
    add     r6, r6, r7
    add     r8, r8, #FRAME_STRIDE
    ldr     r9, [sp, #RF_END]
    cmp     r8, r9
    blo     t0u_row
    b       rf_section2
t1u_start:
    adr     r0, t1u_section
    str     r0, [sp, #RF_LOOP]
t1u_section:
    ldr     r9, [sp, #RF_END]
    cmp     r8, r9
    bhs     rf_section2
t1u_row:
    mov     r0, r4, asr #16
    rsbs    r3, r0, r6, asr #16
    ble     69f
    COUNT_PIXELS
    mla     r1, r0, r2, lr
    add     r0, r8, r0
    SPAN_TEX #0x1E00
69: add     lr, lr, r11
    add     r4, r4, r5
    add     r6, r6, r7
    add     r8, r8, #FRAME_STRIDE
    ldr     r9, [sp, #RF_END]
    cmp     r8, r9
    blo     t1u_row
    b       rf_section2

tc_start:
    ldr     r1, =engine_fill_table
    ldr     r1, [r1, r0, lsl #2]
    str     r1, [sp, #RF_FILL]
    ldr     r2, [sp, #RF_PDX]
    ldr     r11, [sp, #RF_MASK]
    ldr     r12, [sp, #RF_TEXELS]
    adr     r0, tc_section
    str     r0, [sp, #RF_LOOP]
tc_section:
    ldr     r9, [sp, #RF_END]
    cmp     r8, r9
    bhs     rf_section2
tc_row:
    mov     r0, r4, asr #16
    cmp     r0, #0
    movlt   r0, #0
    mov     r3, r6, asr #16
    cmp     r3, #FRAME_WIDTH
    movgt   r3, #FRAME_WIDTH
    subs    r3, r3, r0
    ble     1f
    COUNT_PIXELS
    ldr     r1, [sp, #RF_ROWB]
    mla     r1, r0, r2, r1
    add     r0, r8, r0
    mov     lr, pc
    ldr     pc, [sp, #RF_FILL]
1:  ldr     r0, [sp, #RF_ROWB]
    ldr     r1, [sp, #RF_PDY]
    add     r0, r0, r1
    str     r0, [sp, #RF_ROWB]
    add     r4, r4, r5
    add     r6, r6, r7
    add     r8, r8, #FRAME_STRIDE
    ldr     r9, [sp, #RF_END]
    cmp     r8, r9
    blo     tc_row
    b       rf_section2
    .pool

edge_slope_slow2:
    cmp     r12, #0
    movle   r11, #0
    bxle    lr
    stmfd   sp!, {r4, r5, lr}
    RECIP   r12, r4, r5
    add     r4, r4, #8
    mul     r5, r12, r11
    mov     r11, r5, asr r4
    ldmfd   sp!, {r4, r5, lr}
    bx      lr
    .pool

    .global FillTexAlpha
FillTexAlpha:
    tst     r0, #1
    beq     1f
    TEXEL   r10
    cmp     r10, #0
    beq     0f
    ldrb    r9, [r0, #-1]
    orr     r9, r9, r10, lsl #8
    strh    r9, [r0, #-1]
0:  add     r0, r0, #1
    subs    r3, r3, #1
    bxeq    lr
1:  subs    r3, r3, #2
    blt     5f
2:  TEXEL   r10
    TEXEL   r9
    cmp     r10, #0
    beq     3f
    cmp     r9, #0
    orrne   r10, r10, r9, lsl #8
    strhne  r10, [r0]
    bne     4f
    ldrb    r9, [r0, #1]
    orr     r10, r10, r9, lsl #8
    strh    r10, [r0]
    b       4f
3:  cmp     r9, #0
    beq     4f
    ldrb    r10, [r0]
    orr     r10, r10, r9, lsl #8
    strh    r10, [r0]
4:  add     r0, r0, #2
    subs    r3, r3, #2
    bge     2b
5:  adds    r3, r3, #2
    bxeq    lr
    TEXEL   r10
    cmp     r10, #0
    bxeq    lr
    ldrb    r9, [r0, #1]
    orr     r10, r10, r9, lsl #8
    strh    r10, [r0]
    bx      lr

    .global FillTint
FillTint:
    ldr     r12, =g_shade_water
    tst     r0, #1
    beq     1f
    ldrh    r1, [r0, #-1]
    and     r2, r1, #0xFF
    ldrb    r1, [r12, r1, lsr #8]
    orr     r1, r2, r1, lsl #8
    strh    r1, [r0, #-1]
    add     r0, r0, #1
    subs    r3, r3, #1
    bxeq    lr
1:  tst     r0, #2
    beq     2f
    cmp     r3, #2
    blt     5f
    ldrh    r1, [r0]
    and     r2, r1, #0xFF
    ldrb    r2, [r12, r2]
    ldrb    r1, [r12, r1, lsr #8]
    orr     r1, r2, r1, lsl #8
    strh    r1, [r0], #2
    subs    r3, r3, #2
    bxeq    lr
2:  subs    r3, r3, #4
    blt     4f
3:  ldr     r1, [r0]
    and     r2, r1, #0xFF
    ldrb    r2, [r12, r2]
    and     r10, r1, #0xFF00
    ldrb    r10, [r12, r10, lsr #8]
    orr     r2, r2, r10, lsl #8
    and     r10, r1, #0xFF0000
    ldrb    r10, [r12, r10, lsr #16]
    orr     r2, r2, r10, lsl #16
    ldrb    r10, [r12, r1, lsr #24]
    orr     r2, r2, r10, lsl #24
    str     r2, [r0], #4
    subs    r3, r3, #4
    bge     3b
4:  adds    r3, r3, #4
    bxeq    lr
    cmp     r3, #2
    blt     5f
    ldrh    r1, [r0]
    and     r2, r1, #0xFF
    ldrb    r2, [r12, r2]
    ldrb    r1, [r12, r1, lsr #8]
    orr     r1, r2, r1, lsl #8
    strh    r1, [r0], #2
    subs    r3, r3, #2
    bxeq    lr
5:  ldrh    r1, [r0]
    and     r2, r1, #0xFF
    ldrb    r2, [r12, r2]
    bic     r1, r1, #0xFF
    orr     r1, r1, r2
    strh    r1, [r0]
    bx      lr

    .global FillDarken
FillDarken:
    stmfd   sp!, {r4, lr}
    ldr     r4, =shade_darken
1:  TEXEL   r10
    cmp     r10, #0
    beq     3f
    ldrb    r10, [r0]
    ldrb    r10, [r4, r10]
    tst     r0, #1
    beq     2f
    ldrb    r9, [r0, #-1]
    orr     r9, r9, r10, lsl #8
    strh    r9, [r0, #-1]
    b       3f
2:  ldrb    r9, [r0, #1]
    orr     r9, r10, r9, lsl #8
    strh    r9, [r0]
3:  add     r0, r0, #1
    subs    r3, r3, #1
    bne     1b
    ldmfd   sp!, {r4, lr}
    bx      lr
    .pool

    .global FillFlat
FillFlat:
    tst     r0, #1
    beq     1f
    ldrb    r1, [r0, #-1]
    orr     r1, r1, r2, lsl #8
    strh    r1, [r0, #-1]
    add     r0, r0, #1
    subs    r3, r3, #1
    bxeq    lr
1:  cmp     r3, #2
    blt     6f
    tst     r0, #2
    beq     2f
    strh    r2, [r0], #2
    sub     r3, r3, #2
2:  subs    r3, r3, #16
    blt     3f
20: str     r2, [r0], #4
    str     r2, [r0], #4
    str     r2, [r0], #4
    str     r2, [r0], #4
    subs    r3, r3, #16
    bge     20b
3:  adds    r3, r3, #16
    bxeq    lr
4:  subs    r3, r3, #4
    blt     5f
    str     r2, [r0], #4
    b       4b
5:  adds    r3, r3, #4
    bxeq    lr
    cmp     r3, #2
    blt     6f
    strh    r2, [r0], #2
    subs    r3, r3, #2
    bxeq    lr
6:  ldrb    r1, [r0, #1]
    and     r3, r2, #0xFF
    orr     r1, r3, r1, lsl #8
    strh    r1, [r0]
    bx      lr

    .section .text.engine_additive, "ax", %progbits
    .global FillAdditive
FillAdditive:
    stmfd   sp!, {r4, r5, lr}
    ldr     r4, =g_additive_rows
    ldr     r4, [r4]
    ldr     r5, =g_additive_table
    ldr     r5, [r5]
1:  TEXEL   r10
    add     r10, r4, r10, lsl #1
    ldrh    r10, [r10]
    ldrb    r9, [r0]
    add     r10, r9, r10, lsl #8
    ldrb    r10, [r5, r10]
    tst     r0, #1
    beq     2f
    ldrb    r9, [r0, #-1]
    orr     r9, r9, r10, lsl #8
    strh    r9, [r0, #-1]
    b       3f
2:  ldrb    r9, [r0, #1]
    orr     r9, r10, r9, lsl #8
    strh    r9, [r0]
3:  add     r0, r0, #1
    subs    r3, r3, #1
    bne     1b
    ldmfd   sp!, {r4, r5, lr}
    bx      lr
    .pool

    .section .rodata.engine, "a", %progbits
    .align 2
    .global engine_fill_table
engine_fill_table:
    .word   FillFlat, 0, FillTexAlpha, FillAdditive, FillDarken, 0
    .global engine_recip_table
engine_recip_table:
    .set m, 256
    .rept 256
    .if m == 256
    .hword 65535
    .else
    .hword (16777216 + m / 2) / m
    .endif
    .set m, m + 1
    .endr
