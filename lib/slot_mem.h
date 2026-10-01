/* slot_mem.h — Átomo = Word_8 = 1 byte por slot.
 *
 * naturais.tex / inteiros.tex def:w8 thm:z:
 *   Word_8 = {0..255} = suporte (envelope). ℕ ≠ Word_8.
 *   ℤ = ℕ²/~ ; o inteiro É a classe [(a,b)]. Par ≠ classe (Lei 7).
 *   O par (a,b) ∈ Word_8² ocupa DOIS slots consecutivos — não um struct de 2 B.
 *   ~ testa-se em uint16 (cruz exacta); wrap/sat só na ESCRITA.
 *
 * Descer a álgebra ao metal: o disco guarda o átomo, não o par empacotado. */
#ifndef SLOT_MEM_H
#define SLOT_MEM_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>

#if 1
extern ssize_t pread(int, void *, size_t, off_t);
extern ssize_t pwrite(int, const void *, size_t, off_t);
#endif

typedef uint8_t SlotWord;           /* = Word_8 = F_8 (palavra8.h / naturais) */

#define SLOT_WORD_BYTES 2
#define WORD8_MAX       255u

static long slot_w8_wrap = 0;
static long slot_w8_sat  = 0;

static inline uint8_t slot_w8_wrap_u(unsigned long v){
    if(v > WORD8_MAX){ slot_w8_wrap++; return (uint8_t)(v & WORD8_MAX); }
    return (uint8_t)v;
}

static inline uint8_t slot_w8_sat_u(unsigned long v){
    if(v > WORD8_MAX){ slot_w8_sat++; return (uint8_t)WORD8_MAX; }
    return (uint8_t)v;
}

static inline uint8_t slot_w8_de_long(long v){
    if(v < 0 || (unsigned long)v > WORD8_MAX) slot_w8_wrap++;
    return (uint8_t)v;
}

static inline long slot_long_de_w8(uint8_t b){ return (long)b; }

/* ~ : (a,b)~(c,d) ⟺ a+d = b+c em uint16 — sem projectar (def:w8). */
static inline int slot_equiv4(uint8_t a, uint8_t b, uint8_t c, uint8_t d){
    return (uint16_t)a + (uint16_t)d == (uint16_t)b + (uint16_t)c;
}

static inline SlotWord slot_mem_le(int fd, unsigned slot){
    unsigned char buf[SLOT_WORD_BYTES] = { 0, 0 };
    if(fd >= 0)
        pread(fd, buf, sizeof buf, (off_t)slot * SLOT_WORD_BYTES);
    return buf[0];
}

static inline void slot_mem_grava(int fd, unsigned slot, SlotWord w){
    unsigned char buf[SLOT_WORD_BYTES] = { w, 0 };
    if(fd >= 0)
        pwrite(fd, buf, sizeof buf, (off_t)slot * SLOT_WORD_BYTES);
}

/* Par (a,b) = dois átomos: slot e slot+1. */
static inline void slot_mem_grava_par(int fd, unsigned slot, long a, long b){
    slot_mem_grava(fd, slot,     slot_w8_de_long(a));
    slot_mem_grava(fd, slot + 1, slot_w8_de_long(b));
}

static inline void slot_mem_le_par(int fd, unsigned slot, long *a, long *b){
    *a = slot_long_de_w8(slot_mem_le(fd, slot));
    *b = slot_long_de_w8(slot_mem_le(fd, slot + 1));
}

#endif
