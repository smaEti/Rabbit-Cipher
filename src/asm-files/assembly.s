.text
.global rabbit_key_setup_

rabbit_key_setup_:
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    sub sp, sp, #96

    // k0 ... k3 = key[NUM]
    // w2 = k0 , w3 = k1
    // w4 = k2 , w5 = k3
    ldr w2, [x1]
    ldr w3, [x1, #4]
    ldr w4, [x1, #8]
    ldr w5, [x1, #12]
    // ctx->x[NUM] = kNUM;
    str w2, [x0]
    str w3, [x0, #8]
    str w4, [x0, #16]
    str w5, [x0, #24]

    // ctx->x[1] = (k3 << 16) | (k2 >> 16);
    lsl w6, w5, #16
    lsr w7, w4, #16
    orr w6, w6, w7
    str w6, [x0, #4]

    // ctx->x[3] = (k0 << 16) | (k3 >> 16);
    lsl w6, w2, #16
    lsr w7, w5, #16
    orr w6, w6, w7
    str w6, [x0, #12]

    // ctx->x[5] = (k1 << 16) | (k0 >> 16);
    lsl w6, w3, #16
    lsr w7, w2, #16
    orr w6, w6, w7
    str w6, [x0, #20]

    // ctx->x[7] = (k2 << 16) | (k1 >> 16);
    lsl w6, w4, #16
    lsr w7, w3, #16
    orr w6, w6, w7
    str w6, [x0, #28]

    // ctx->c[0] = ROTL32(k2, 16);
    lsl w6, w4, #16
    lsr w7, w4, #16
    orr w6, w6, w7
    str w6, [x0, #32]

    // ctx->c[2] = ROTL32(k3, 16);
    lsl w6, w5, #16
    lsr w7, w5, #16
    orr w6, w6, w7
    str w6, [x0, #40]

    // ctx->c[4] = ROTL32(k0, 16);
    lsl w6, w2, #16
    lsr w7, w2, #16
    orr w6, w6, w7
    str w6, [x0, #48]

    // ctx->c[6] = ROTL32(k1, 16);
    lsl w6, w3, #16
    lsr w7, w3, #16
    orr w6, w6, w7
    str w6, [x0, #56]

    // w6 = 0x0000FFFF w7 = 0xFFFF0000
    movz w6, #0xFFFF
    movz w7, #0xFFFF, lsl #16
    
    //  ctx->c[1] = (k0 & 0xFFFF0000) | (k1 & 0x0000FFFF);
    and w8, w2, w7
    and w9, w3, w6
    orr w8, w8, w9
    str w8, [x0, #36]

    // ctx->c[3] = (k1 & 0xFFFF0000) | (k2 & 0x0000FFFF);
    and w8, w3, w7
    and w9, w4, w6
    orr w8, w8, w9
    str w8, [x0, #44]

    // ctx->c[5] = (k2 & 0xFFFF0000) | (k3 & 0x0000FFFF);
    and w8, w4, w7
    and w9, w5, w6
    orr w8, w8, w9
    str w8, [x0, #52]

    // ctx->c[7] = (k3 & 0xFFFF0000) | (k0 & 0x0000FFFF);
    and w8, w5, w7
    and w9, w2, w6
    orr w8, w8, w9
    str w8, [x0, #60]

    //ctx->carry = 0;
    mov w6, wzr           // or: mov w6, #0
    str w6, [x0, #64]
// =====================================================================ITS ALRIGHT TILL HERE
    movz w6, #0              // i = 0
loop_i:
    cmp w6, #4
    b.eq done_loop_i

    mov w1, #0              // j = 0
j1_loop:
    cmp w1, #8
    b.eq j1_done

    ldr w2, [x0, #64]       // ctx->carry
    movz w3, #0xD34D                // w3 = 0x0000D34D
    movk w3, #0x4D34, lsl #16       // w3 = 0x4D34D34D
    add w2, w2, w3          // w2 = carry + A

    lsl w4, w1, #2          // w4 = offset = j * 4
    add w4, w4, #32
    add x5, x0, w4, uxtw    // x5 = &ctx->c[j]
    ldr w3, [x5]            // prev_c = ctx->c[j]


    add w7, w3, w2          // new_c = prev_c + delta
    str w7, [x5]            // ctx->c[j] = new_c

    cmp w7, w3
    cset w2, lo             // carry = new_c < prev_c
    str w2, [x0, #64]       // ctx->carry = carry

    add w1, w1, #1
    b j1_loop
j1_done:

    mov w1, #0 // j = 0 - w1 = j
loop_j2:
    cmp w1, #8 
    b.eq j2_done

    lsl w2, w1, #2          // w2 = j * 4 (offset)
    
    add x11, x0, w2, uxtw
    ldr w3, [x11]       // w3 = x[j]

    add w5 , w2, #32        // c starts after 32 bytes which is 8 * 4
    add x12, x0, w5, uxtw
    ldr w4, [x12]        // w4 = c[j]

    add w3, w3, w4          // w3 = x[j] + c[j]

    umull x12, w3, w3     // x2 = (uint64_t)(u) * (uint64_t)(u)
    lsr x9, x12, #32      // x1 = sq >> 32
    eor w3, w2, w9       // w0 = (uint32_t)(sq ^ (sq >> 32))
    
    lsl w2, w2, #4
    add x11, sp, w2, uxtw
    str w3, [x11]        // g[j] = g_func(x[j] + c[j])

    add w1, w1, #1
    b loop_j2
j2_done:

//    mov w1, #0
//loop_j3:
//    cmp w1, #8
//    b.eq j3_done
//
//    lsl w2, w1, #2
//
//    add x11, sp, w2, uxtw
//    ldr w9, [x11]        // g[j]
//
//    // g[(j+7)%8]
//    add w3, w1, #7
//    and w3, w3, #7
//    lsl w3, w3, #2
//
//    add x11, sp, w3, uxtw
//    ldr w4, [x11]        // w4 = g[(j+7)%8]
//
//    // ROTL32(g[(j+7)%8], 16)
//    lsl w7, w4, #16
//    lsr w8, w4, #16
//    orr w4, w7, w8
//
//    // g[(j+6)%8]
//    add w3, w1, #6
//    and w3, w3, #7
//    lsl w3, w3, #2
//    
//    add x11, sp, w3, uxtw
//    ldr w5, [x11]        // w5 = g[(j+6)%8]
//
//    // ROTL32(g[(j + 6) % 8], 24)
//    lsl w7, w5, #24
//    lsr w8, w5, #8
//    orr w5, w7, w8
//
//    eor w3, w9, w4
//    eor w3, w5, w3
//
//    add x11, x10, w2, uxtw
//    str w3, [x11]      // ctx->x[j] = ...
//
//    add w1, w1, #1
//    b loop_j3
//j3_done:

    add w6, w6, #1
    b loop_i
done_loop_i:
    add sp, sp, #96
    ldp x29, x30, [sp], #16
    ret