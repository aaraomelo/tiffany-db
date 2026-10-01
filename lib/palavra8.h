/* palavra8.h — Word_8 = F_8 = {0…255}: operar DUAS palavras bit a bit, e subir a torre.
 *
 * naturais.tex: F_1→F_2→F_4→F_8 ≡ Word_8; coordenadas = bits (def:dobra);
 *               base e_k=2^k (thm:base); a+b=(a⊕b)+2(a∧b) (thm:transporte).
 * inteiros.tex def:w8: envelope; ~ em uint16; wrap/sat só na escrita.
 * umbit.h / binario.h / largura.h: as peças — aqui só a porta Word_8.
 *
 * Duas palavras a,b ∈ Word_8:
 *   ⊕ ∧          bit a bit (𝔽₂⁸ / neuronio)
 *   ⊗_F8         produto do corpo F_8 (bn_mul k=3)
 *   +_ℕ ×_ℕ      ponto fixo de (⊕,∧) — sobe a ℕ
 *   ×_w          largura.h: sobe w=8→16→32… (par alto/baixo)
 *
 * Slot = um átomo Word_8 (slot_mem.h). Par ℤ = dois slots (Lei 7). */
#ifndef PALAVRA8_H
#define PALAVRA8_H

#include <stdint.h>
#include "umbit.h"
#include "binario.h"
#include "largura.h"

typedef uint8_t Word8;                 /* F_8 ≡ Word_8 ≡ V8 */

/* QUANTOS BITS TEM O ÁTOMO --- e está aqui porque é aqui que ele é declarado.
 * O oito não é escolha de quem usa a palavra: é o nome do tipo. Quem precisar da
 * largura deriva daqui, e ninguém volta a escrevê-la. */
#define W8_BITS  ((unsigned)(sizeof(Word8) * 8u))

/* ── bit a bit (𝔽₂⁸): neuronio / umbit — qualquer par de palavras ───────────── */
static Word8 w8_xor(Word8 a, Word8 b){ return v_som(a, b); }          /* ⊕ */
static Word8 w8_and(Word8 a, Word8 b){ return (Word8)(a & b); }       /* ∧ */
static Word8 w8_opo(Word8 a){ return a; }                             /* −a=a em char 2 */
static B     w8_bit(Word8 a, int k){ return v_coord(a, k); }          /* e_k = 2^k */
static Word8 w8_poe_bit(Word8 a, int k, B v){
    Word8 m = (Word8)(1u << (k & 7));
    return v ? (Word8)(a | m) : (Word8)(a & (Word8)~m);
}

/* ── F_8: soma e produto do corpo (binario.h, andar k=3) ───────────────────── */
static void w8_torre(void){ if(!bn_erguido) bn_torre(); }
static Word8 w8_som_f8(Word8 a, Word8 b){ return (Word8)bn_som(a, b); }
static Word8 w8_mul_f8(Word8 a, Word8 b){
    w8_torre();
    return (Word8)bn_mul(3, a, b);
}
static Word8 w8_inv_f8(Word8 a){
    w8_torre();
    return (Word8)bn_inv(3, a);
}

/* ── ℕ no suporte Word_8: iteração (⊕,∧) até (a+b,0) — thm:transporte ─────── */
static unsigned long w8_soma_N(Word8 a, Word8 b){
    return bn_soma_nat((unsigned long)a, (unsigned long)b, 0);
}
static unsigned long w8_mult_N(Word8 a, Word8 b){
    return bn_mult_nat((unsigned long)a, (unsigned long)b);
}

/* ── subir largura: produto w×w → par (alto,baixo) — largura.h ───────────────
 * w=8: Word_8×Word_8 → 16 bits; w=16 → 32; w=32 → 64. Mesma dobra, parâmetro w. */
static LgPar w8_mult_larg(int w, uint64_t a, uint64_t b){
    return lg_mult(w, a & lg_masc(w), b & lg_masc(w));
}
static LgPar w8_soma_larg(int w, LgPar x, LgPar y){
    return lg_soma(w, x, y);
}

/* ── par Word_8² (Lei 7 / def:w8): dois átomos; ~ em uint16 sem projectar ───── */
typedef struct { Word8 a, b; } W8Par;
static W8Par w8_par(Word8 a, Word8 b){ W8Par p; p.a = a; p.b = b; return p; }
static int w8_par_equiv(W8Par x, W8Par y){
    return (uint16_t)x.a + (uint16_t)y.b == (uint16_t)x.b + (uint16_t)y.a;
}
/* ⊕ e ∧ componente a componente — bit a bit em cada coordenada do par */
static W8Par w8_par_xor(W8Par x, W8Par y){
    return w8_par(w8_xor(x.a, y.a), w8_xor(x.b, y.b));
}
static W8Par w8_par_and(W8Par x, W8Par y){
    return w8_par(w8_and(x.a, y.a), w8_and(x.b, y.b));
}

#endif
