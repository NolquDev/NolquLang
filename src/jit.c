/*
 * jit.c — Nolqu Template JIT Compiler
 *
 * x86-64 template JIT for simple numeric for-range loops.
 *
 * Generated code layout (System V AMD64 ABI calling convention):
 *
 *   void jit_loop(double* i_ptr,   ← rdi
 *                 double  stop,    ← xmm0
 *                 double  step,    ← xmm1
 *                 double* n0_ptr,  ← rsi    (first body var, if any)
 *                 double  delta0,  ← xmm2   (first body delta, if any)
 *                 double* n1_ptr,  ← rdx    (second body var, if any)
 *                 double  delta1,  ← xmm3   ...)
 *
 * For 0 body vars (pure counter loop), only rdi/xmm0/xmm1 are used.
 * For N body vars, N pointer registers and N xmm delta registers are used.
 *
 * Integer arg registers: rdi, rsi, rdx, rcx, r8, r9  (6 max)
 * XMM arg registers:     xmm0..xmm7                  (8 max)
 *
 * This gives us: i_ptr(rdi) + stop(xmm0) + step(xmm1) = 3 fixed regs,
 * leaving 5 int regs (rsi,rdx,rcx,r8,r9) and 6 xmm regs (xmm2..xmm7)
 * for body vars — so up to 5 body variables are natively supported.
 * (NQ_JIT_MAX_BODY_VARS = 8 but only 5 use native calling convention;
 *  above 5 we fall back to interpreted for safety.)
 */

#include "jit.h"
#include <string.h>
#include <unistd.h>

/* ─────────────────────────────────────────────────────────────────────
 * x86-64 JIT path
 * ───────────────────────────────────────────────────────────────────── */
#if defined(__x86_64__) && (defined(__linux__) || defined(__APPLE__))

#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>

/* Emit helpers — write bytes into a buffer and advance pointer */
static void emit_byte(uint8_t** p, uint8_t b)      { *(*p)++ = b; }
static void emit2(uint8_t** p, uint8_t a, uint8_t b) { emit_byte(p,a); emit_byte(p,b); }
static void emit4(uint8_t** p, uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    emit_byte(p,a); emit_byte(p,b); emit_byte(p,c); emit_byte(p,d);
}

/*
 * Emit: movsd xmm_dst, [reg_base + offset]
 *
 * reg_base: rdi=7, rsi=6, rdx=2, rcx=1, r8=8 (needs REX.B), r9=9 (needs REX.B)
 * xmm_dst: 0..7
 *
 * Encoding:
 *   F2 [REX] 0F 10 ModRM [SIB] [disp]
 *   ModRM = mod(2b) | reg(3b) | r/m(3b)
 *   mod=01: [reg+disp8],  mod=10: [reg+disp32],  mod=00: [reg] (no disp)
 */
static void emit_movsd_load(uint8_t** p, int xmm_dst, int reg_base, long offset) {
    /* Encoding: F2 [REX] 0F 10 ModRM [disp]
     * Mandatory prefix F2 MUST come before REX. */
    emit_byte(p, 0xF2);
    int need_rex = (reg_base >= 8) || (xmm_dst >= 8);
    if (need_rex) {
        uint8_t rex = 0x40;
        if (xmm_dst >= 8)  rex |= 0x04; /* REX.R */
        if (reg_base >= 8) rex |= 0x01; /* REX.B */
        emit_byte(p, rex);
    }
    emit2(p, 0x0F, 0x10); /* MOVSD load opcode */
    int base = reg_base & 7;
    int dst  = xmm_dst  & 7;
    if (offset == 0 && base != 5) { /* mod=00 */
        emit_byte(p, (0 << 6) | (dst << 3) | base);
    } else if (offset >= -128 && offset <= 127) { /* mod=01, disp8 */
        emit_byte(p, (1 << 6) | (dst << 3) | base);
        emit_byte(p, (uint8_t)(int8_t)offset);
    } else { /* mod=10, disp32 */
        emit_byte(p, (2 << 6) | (dst << 3) | base);
        uint32_t d = (uint32_t)(int32_t)offset;
        emit_byte(p, d & 0xFF); emit_byte(p, (d>>8)&0xFF);
        emit_byte(p, (d>>16)&0xFF); emit_byte(p, (d>>24)&0xFF);
    }
}

/*
 * Emit: movsd [reg_base + offset], xmm_src
 * Same encoding as load but opcode is 0x11.
 */
static void emit_movsd_store(uint8_t** p, int reg_base, long offset, int xmm_src) {
    /* Encoding: F2 [REX] 0F 11 ModRM [disp] */
    emit_byte(p, 0xF2);
    int need_rex = (reg_base >= 8) || (xmm_src >= 8);
    if (need_rex) {
        uint8_t rex = 0x40;
        if (xmm_src >= 8)  rex |= 0x04;
        if (reg_base >= 8) rex |= 0x01;
        emit_byte(p, rex);
    }
    emit2(p, 0x0F, 0x11); /* MOVSD store opcode */
    int base = reg_base & 7;
    int src  = xmm_src  & 7;
    if (offset == 0 && base != 5) {
        emit_byte(p, (0 << 6) | (src << 3) | base);
    } else if (offset >= -128 && offset <= 127) {
        emit_byte(p, (1 << 6) | (src << 3) | base);
        emit_byte(p, (uint8_t)(int8_t)offset);
    } else {
        emit_byte(p, (2 << 6) | (src << 3) | base);
        uint32_t d = (uint32_t)(int32_t)offset;
        emit_byte(p, d & 0xFF); emit_byte(p, (d>>8)&0xFF);
        emit_byte(p, (d>>16)&0xFF); emit_byte(p, (d>>24)&0xFF);
    }
}

/* Emit: addsd xmm_dst, xmm_src */
static void emit_addsd(uint8_t** p, int xmm_dst, int xmm_src) {
    /* Encoding: F2 [REX] 0F 58 ModRM */
    emit_byte(p, 0xF2);
    int need_rex = (xmm_dst >= 8) || (xmm_src >= 8);
    if (need_rex) {
        uint8_t rex = 0x40;
        if (xmm_dst >= 8) rex |= 0x04; /* REX.R */
        if (xmm_src >= 8) rex |= 0x01; /* REX.B */
        emit_byte(p, rex);
    }
    emit_byte(p, 0x0F);
    emit_byte(p, 0x58);
    emit_byte(p, (uint8_t)(((xmm_dst & 7) << 3) | 0xC0 | (xmm_src & 7)));
}

/* Emit: mulsd xmm_dst, xmm_src */
static void emit_mulsd(uint8_t** p, int xmm_dst, int xmm_src) {
    /* Encoding: F2 [REX] 0F 59 ModRM */
    emit_byte(p, 0xF2);
    int need_rex = (xmm_dst >= 8) || (xmm_src >= 8);
    if (need_rex) {
        uint8_t rex = 0x40;
        if (xmm_dst >= 8) rex |= 0x04;
        if (xmm_src >= 8) rex |= 0x01;
        emit_byte(p, rex);
    }
    emit_byte(p, 0x0F);
    emit_byte(p, 0x59);
    emit_byte(p, (uint8_t)(((xmm_dst & 7) << 3) | 0xC0 | (xmm_src & 7)));
}

/* Emit: ucomisd xmm_a, xmm_b */
static void emit_ucomisd(uint8_t** p, int xmm_a, int xmm_b) {
    emit4(p, 0x66, 0x0F, 0x2E,
          (uint8_t)(((xmm_a & 7) << 3) | 0xC0 | (xmm_b & 7)));
}

/*
 * Register assignments for JIT function call:
 *
 *   xmm_regs[0] = xmm0 = stop (passed as arg, not a scratch reg)
 *   xmm_regs[1] = xmm1 = step
 *   xmm_regs[2] = xmm2 = delta0  (first body var delta)
 *   xmm_regs[3] = xmm3 = delta1  ...
 *   ...
 *
 *   Scratch regs for loop state:
 *   xmm_i     = xmm5  (loop counter i, loaded from *i_ptr)
 *   xmm_n[k]  = xmm6, xmm7, ... (accumulator values)
 *
 *   Pointer regs (base registers):
 *   rdi = i_ptr
 *   rsi = n0_ptr
 *   rdx = n1_ptr
 *   rcx = n2_ptr
 *   r8  = n3_ptr
 *   r9  = n4_ptr
 *
 * Intel reg encoding: rax=0, rcx=1, rdx=2, rbx=3, rsp=4, rbp=5, rsi=6, rdi=7
 *   r8=8, r9=9 (need REX.B)
 */

/* Map body var index to base register (pointer args) */
static int ptr_reg_for_var(int k) {
    /* k=0→rsi(6), k=1→rdx(2), k=2→rcx(1), k=3→r8(8), k=4→r9(9) */
    static const int regs[] = {6, 2, 1, 8, 9};
    return (k < 5) ? regs[k] : -1;
}

/* Map body var index to xmm register (delta args) */
static int delta_xmm_for_var(int k) {
    return k + 2; /* xmm2, xmm3, xmm4, xmm5, ... */
}

/* Scratch xmm for loop counter (i) and body accumulators */
#define XMM_I     7   /* xmm7 = loop counter */
#define XMM_N0    6   /* xmm6 = first accumulator */
/* For >1 body vars: xmm5, xmm4, xmm3... overlap with deltas.
 * To be safe, only use scratch regs that don't overlap arg regs.
 * delta args: xmm2, xmm3, xmm4, xmm5 (for vars 0,1,2,3)
 * So for accumulators we need non-overlapping regs.
 * Use: xmm8, xmm9, ... (need REX prefix) for additional accumulators.
 * For simplicity in a5: fully support up to 3 body vars using scratch regs
 * xmm7(i), xmm8(n0), xmm9(n1), xmm10(n2) — all need REX.
 */
static int accum_xmm_for_var(int k) {
    return 8 + k; /* xmm8, xmm9, xmm10... (needs REX prefix in encoding) */
}

bool nq_jit_run_loop(JITLoopSpec* spec) {
    /* Only support positive step (i < stop direction) for now */
    if (spec->step <= 0.0) return false;

    int n_vars = spec->n_vars;
    if (n_vars < 0 || n_vars > 5) return false;

    /* Allocate executable memory — one page is more than enough */
    size_t page = (size_t)getpagesize();
    uint8_t* buf = (uint8_t*)mmap(NULL, page,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (buf == MAP_FAILED) return false;

    uint8_t* p = buf;

    /*
     * Emit function prologue (none needed — no callee-saved regs used).
     *
     * Load loop state from pointers into scratch xmm registers:
     *   xmm7 ← *i_ptr   (loop counter)
     *   xmm8 ← *n0_ptr  (accumulator 0)
     *   xmm9 ← *n1_ptr  (accumulator 1)
     *   ...
     *
     * Pointer registers (per calling convention):
     *   rdi = i_ptr
     *   rsi = n0_ptr  (if n_vars >= 1)
     *   rdx = n1_ptr  (if n_vars >= 2)
     *   ...
     */

    /* Load counter: movsd xmm7, [rdi] */
    emit_movsd_load(&p, XMM_I, 7 /*rdi*/, 0);

    /* Load accumulators */
    for (int k = 0; k < n_vars; k++) {
        int base = ptr_reg_for_var(k);
        if (base < 0) return false; /* shouldn't happen given n_vars <= 5 */
        emit_movsd_load(&p, accum_xmm_for_var(k), base, 0);
    }

    /*
     * Loop top — record position for back-edge.
     *
     *   ucomisd xmm7, xmm0   ; compare i < stop
     *   jae done             ; if i >= stop, exit
     *   addsd xmm7, xmm1     ; i += step
     *   addsd xmm8, xmm2     ; n0 += delta0  (if n_vars >= 1)
     *   addsd xmm9, xmm3     ; n1 += delta1  (if n_vars >= 2)
     *   ...
     *   jmp loop_top
     * done:
     */
    uint8_t* loop_top = p;

    /* ucomisd xmm7, xmm0 — compare counter vs stop */
    emit_ucomisd(&p, XMM_I, 0 /*xmm0=stop*/);

    /* jae done — placeholder, patch after we know the offset */
    uint8_t* jae_ip = p;  /* address of the jae instruction */
    emit2(&p, 0x73, 0x00);  /* jae rel8 = 0 (placeholder) */

    /* addsd xmm7, xmm1 — i += step */
    emit_addsd(&p, XMM_I, 1 /*xmm1=step*/);

    /* addsd/mulsd xmmN, xmm(k+2) — accumulators */
    for (int k = 0; k < n_vars; k++) {
        if (spec->ops[k] == JIT_OP_MUL)
            emit_mulsd(&p, accum_xmm_for_var(k), delta_xmm_for_var(k));
        else
            emit_addsd(&p, accum_xmm_for_var(k), delta_xmm_for_var(k));
    }

    /* jmp loop_top — short backward jump */
    int jmp_back = (int)(loop_top - (p + 2)); /* rel8 from next ip */
    if (jmp_back < -128) {
        /* Loop body too large for short jump — use near jump */
        emit_byte(&p, 0xE9);
        int32_t rel32 = (int32_t)(loop_top - (p + 4));
        uint8_t* r = (uint8_t*)&rel32;
        emit_byte(&p, r[0]); emit_byte(&p, r[1]);
        emit_byte(&p, r[2]); emit_byte(&p, r[3]);
    } else {
        emit2(&p, 0xEB, (uint8_t)(int8_t)jmp_back);
    }

    /* Patch jae offset */
    uint8_t* done_ptr = p;
    int jae_off = (int)(done_ptr - (jae_ip + 2));
    if (jae_off > 127) { mmap(buf, page, 0, 0, -1, 0); return false; }
    jae_ip[1] = (uint8_t)(int8_t)jae_off;

    /* Store results back */
    emit_movsd_store(&p, 7 /*rdi*/, 0, XMM_I);
    for (int k = 0; k < n_vars; k++) {
        int base = ptr_reg_for_var(k);
        emit_movsd_store(&p, base, 0, accum_xmm_for_var(k));
    }

    /* ret */
    emit_byte(&p, 0xC3);

    /* Make executable */
    if (mprotect(buf, page, PROT_READ | PROT_EXEC) != 0) {
        munmap(buf, page); return false;
    }

    /*
     * Build argument list and call the generated function.
     *
     * The C compiler will place args into the right registers
     * for us via the calling convention. We use a typed function
     * pointer that matches the generated code's expected ABI.
     *
     * For 0..5 body vars we use overloaded call patterns via
     * a simple switch — ugly but avoids va_args and is safe.
     */
    typedef void (*F0)(double* i, double stop, double step);
    typedef void (*F1)(double* i, double stop, double step,
                       double* n0, double d0);
    typedef void (*F2)(double* i, double stop, double step,
                       double* n0, double d0, double* n1, double d1);
    typedef void (*F3)(double* i, double stop, double step,
                       double* n0, double d0, double* n1, double d1,
                       double* n2, double d2);

    double* vp[5];
    for (int k = 0; k < n_vars; k++) vp[k] = spec->var_ptrs[k];

    switch (n_vars) {
        case 0:
            ((F0)(void*)buf)(spec->i_ptr, spec->stop, spec->step);
            break;
        case 1:
            ((F1)(void*)buf)(spec->i_ptr, spec->stop, spec->step,
                             vp[0], spec->deltas[0]);
            break;
        case 2:
            ((F2)(void*)buf)(spec->i_ptr, spec->stop, spec->step,
                             vp[0], spec->deltas[0],
                             vp[1], spec->deltas[1]);
            break;
        case 3:
            ((F3)(void*)buf)(spec->i_ptr, spec->stop, spec->step,
                             vp[0], spec->deltas[0],
                             vp[1], spec->deltas[1],
                             vp[2], spec->deltas[2]);
            break;
        default:
            munmap(buf, page); return false;
    }

    munmap(buf, page);
    return true;
}

#else /* ─── Non-x86-64 fallback ─────────────────────────────────────── */

bool nq_jit_run_loop(JITLoopSpec* spec) {
    /*
     * No JIT available on this platform.
     * Execute the loop as a C tight-loop (no bytecode dispatch overhead,
     * since we have direct double* pointers to the slot values).
     */
    if (spec->step > 0) {
        while (*spec->i_ptr < spec->stop) {
            *spec->i_ptr += spec->step;
            for (int k = 0; k < spec->n_vars; k++)
                *spec->var_ptrs[k] += spec->deltas[k];
        }
    } else if (spec->step < 0) {
        while (*spec->i_ptr > spec->stop) {
            *spec->i_ptr += spec->step;
            for (int k = 0; k < spec->n_vars; k++)
                *spec->var_ptrs[k] += spec->deltas[k];
        }
    }
    return true;
}

#endif
