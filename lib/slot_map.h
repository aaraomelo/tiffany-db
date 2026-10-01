/* slot_map.h — Regiões fixas: 1 byte = 1 slot (Word_8).
 *
 * FC: meta em DOIS slots (sinal, n); depois cada a_k em DOIS slots (Word_8², a Word
 * da ISA); flag no fim. O termo é um PAR de átomos porque um quociente não cabe num
 * átomo — e o que não cabe no par acende `saturou` em vez de enrolar. */
#ifndef SLOT_MAP_H
#define SLOT_MAP_H

#include "slot_mem.h"

#ifndef RT_CF_MAX
#define RT_CF_MAX 48
#endif

#define S_CF         2048u
/* sinal + n + RT_CF_MAX termos de DOIS átomos + flag */
#define S_CF_STRIDE  (2u * RT_CF_MAX + 3u)
#define S_CF_WORDS   8u
#define S_CF_END     (S_CF + S_CF_WORDS * S_CF_STRIDE)

#define S_CF_MEM_BYTES  ((unsigned long)S_CF_END * (unsigned long)SLOT_WORD_BYTES)

static inline unsigned cf_slot_base(unsigned word_ix){
    return (unsigned)(S_CF + word_ix * S_CF_STRIDE);
}

#endif
