/* word_isa.h — Word da ISA = Word_8². Sem long C.
 *
 * Lei 7 / inteiros def:w8: par (total,e) = dois átomos.
 * GOLD = ×σ com σ²=σ+1. Transporte = thm:transporte (não “carry”).
 * ∞+1=−1 (Möbius §M8). Envelope = Word_8; coef. que crescem sobem a torre. */
#ifndef WORD_ISA_H
#define WORD_ISA_H

#include "palavra8.h"

typedef struct { Word8 total, e; } Word;

static inline Word word_isa(Word8 t, Word8 e){
    Word w; w.total = t; w.e = e; return w;
}
/* visão assinada do envelope (int8) — I/O local, não largura semântica */
static inline int word_isa_ti(Word w){ return (int)(int8_t)w.total; }
static inline int word_isa_ei(Word w){ return (int)(int8_t)w.e; }
static inline Word word_isa_de_i(int t, int e){
    return word_isa((Word8)(int8_t)t, (Word8)(int8_t)e);
}

#define WORD_ISA_ATOMS 2u

/* e a largura da Word deriva da do átomo: a Word diz quantos átomos tem, o
 * `palavra8.h` diz quantos bits tem o átomo, e nenhum número se repete. */
#define WORD_ISA_BITS  (WORD_ISA_ATOMS * W8_BITS)

#endif
