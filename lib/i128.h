/* i128.h — CENTO E VINTE E OITO BITS EM DOIS DE SESSENTA E QUATRO, sem __int128.
 *
 * ANDAR 128 da torre (torre_alg.h): par (hi, lo) = produto exacto 64×64 quando
 * i128_fits_i64 falha nos operandos. Anterior: dual32.h (D64). Trava nativa:
 * tests/dual32.c (__int128 de referência).
 *
 * O par (hi, lo) é complemento de dois assinado: valor = hi·2⁶⁴ + lo.
 * Só entra aritmética de 64 bits — a mesma lei que dual32.h, um degrau acima. */
#ifndef I128_H
#define I128_H

#include <stdint.h>

typedef struct { int64_t hi; uint64_t lo; } I128;

static I128 i128_zero(void){ I128 r = {0, 0}; return r; }

static I128 i128_from_i64(int64_t x){
    I128 r;
    r.lo = (uint64_t)x;
    r.hi = (x < 0) ? -1 : 0;
    return r;
}

static int i128_negativo(I128 x){ return x.hi < 0; }

static I128 i128_neg(I128 x){
    I128 r;
    r.lo = 0u - x.lo;
    r.hi = -x.hi - (x.lo != 0 ? 1 : 0);
    return r;
}

static I128 i128_abs(I128 x){ return i128_negativo(x) ? i128_neg(x) : x; }

static int i128_cmp(I128 a, I128 b){
    if(a.hi != b.hi) return a.hi < b.hi ? -1 : 1;
    if(a.lo != b.lo) return a.lo < b.lo ? -1 : 1;
    return 0;
}

static I128 i128_add(I128 a, I128 b){
    I128 r;
    r.lo = a.lo + b.lo;
    r.hi = a.hi + b.hi + (r.lo < a.lo ? 1 : 0);
    return r;
}

static I128 i128_sub(I128 a, I128 b){
    I128 r;
    r.lo = a.lo - b.lo;
    r.hi = a.hi - b.hi - (a.lo < b.lo ? 1 : 0);
    return r;
}

static void umul64(uint64_t a, uint64_t b, uint64_t *hi, uint64_t *lo){
    uint64_t a0 = a & 0xFFFFFFFFu, a1 = a >> 32;
    uint64_t b0 = b & 0xFFFFFFFFu, b1 = b >> 32;
    uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
    uint64_t mid = p01 + p10;
    uint64_t carry = (mid < p01) ? (1ull << 32) : 0;
    *lo = p00 + (mid << 32);
    if(*lo < p00) carry += 1ull << 32;
    *hi = p11 + (mid >> 32) + (carry >> 32);
}

static I128 i128_smul(int64_t a, int64_t b){
    int neg = ((a < 0) ^ (b < 0)) ? 1 : 0;
    uint64_t ua, ub;
    if(a == INT64_MIN) ua = (uint64_t)1 << 63;
    else ua = (a < 0) ? (uint64_t)(-a) : (uint64_t)a;
    if(b == INT64_MIN) ub = (uint64_t)1 << 63;
    else ub = (b < 0) ? (uint64_t)(-b) : (uint64_t)b;
    uint64_t phi, plo;
    umul64(ua, ub, &phi, &plo);
    I128 r;
    r.lo = plo;
    r.hi = (int64_t)phi;
    if(neg) r = i128_neg(r);
    return r;
}

static I128 i128_shr1(I128 x){
    I128 r;
    r.lo = (x.lo >> 1) | ((uint64_t)(x.hi & 1) << 63);
    r.hi = (int64_t)((uint64_t)x.hi >> 1);
    return r;
}

static int i128_fits_i64(I128 x){
    if(x.hi == 0) return x.lo <= (uint64_t)INT64_MAX;
    if(x.hi == -1) return x.lo > (uint64_t)INT64_MAX;
    return 0;
}

static I128 i128_smul_i128(I128 a, int64_t b){
    if(b == 0 || (a.hi == 0 && a.lo == 0)) return i128_zero();
    if(i128_fits_i64(a)) return i128_smul((int64_t)a.lo, b);
    int neg = i128_negativo(a) ^ (b < 0);
    I128 ua = i128_abs(a);
    uint64_t ub = (b < 0) ? (uint64_t)(-(int64_t)b) : (uint64_t)b;
    I128 acc = i128_zero();
    I128 cur = ua;
    while(ub){
        if(ub & 1) acc = i128_add(acc, cur);
        cur = i128_add(cur, cur);
        ub >>= 1;
    }
    return neg ? i128_neg(acc) : acc;
}

static I128 i128_mul(I128 a, I128 b){
    if(i128_fits_i64(a) && i128_fits_i64(b))
        return i128_smul((int64_t)a.lo, (int64_t)b.lo);
    int neg = (i128_negativo(a) ^ i128_negativo(b)) ? 1 : 0;
    I128 ua = i128_abs(a), ub = i128_abs(b);
    I128 acc = i128_zero();
    I128 cur = ua;
    while(ub.hi != 0 || ub.lo != 0){
        if(ub.lo & 1) acc = i128_add(acc, cur);
        cur = i128_add(cur, cur);
        ub = i128_shr1(ub);
    }
    return neg ? i128_neg(acc) : acc;
}

static I128 i128_div_i64(I128 a, int64_t b){
    if(b == 0) return i128_zero();
    int neg = (i128_negativo(a) ^ (b < 0)) ? 1 : 0;
    I128 ua = i128_abs(a);
    uint64_t ub = (b < 0) ? (uint64_t)(-(int64_t)b) : (uint64_t)b;
    /* divisão longa: ua / ub */
    I128 q = i128_zero();
    I128 r = i128_zero();
    for(int i = 127; i >= 0; i--){
        int bit = (i >= 64) ? ((ua.hi >> (i - 64)) & 1) : ((ua.lo >> i) & 1);
        r = i128_add(r, r);
        if(bit){
            r.lo |= 1;
            if(r.lo == 0) r.hi++;
        }
        if(i128_cmp(r, i128_from_i64((int64_t)ub)) >= 0){
            r = i128_sub(r, i128_from_i64((int64_t)ub));
            if(i >= 64) q.hi |= (int64_t)1 << (i - 64);
            else q.lo |= (uint64_t)1 << i;
        }
    }
    return neg ? i128_neg(q) : q;
}

static int64_t i128_to_i64(I128 x){
    if(x.hi != 0 && x.hi != -1) return 0; /* não cabe */
    return (int64_t)x.lo;
}

static int i128_is_zero(I128 x){ return x.hi == 0 && x.lo == 0; }

static I128 i128_div(I128 a, I128 b){
    if(i128_is_zero(b)) return i128_zero();
    int neg = (i128_negativo(a) ^ i128_negativo(b)) ? 1 : 0;
    I128 ua = i128_abs(a), ub = i128_abs(b);
    I128 q = i128_zero(), r = i128_zero();
    for(int i = 127; i >= 0; i--){
        r = i128_add(r, r);
        int bit = (i >= 64) ? (int)((ua.hi >> (i - 64)) & 1) : (int)((ua.lo >> i) & 1);
        if(bit){ r.lo |= 1; if(r.lo == 0) r.hi++; }
        if(i128_cmp(r, ub) >= 0){
            r = i128_sub(r, ub);
            if(i >= 64) q.hi |= (int64_t)1 << (i - 64);
            else q.lo |= (uint64_t)1 << i;
        }
    }
    return neg ? i128_neg(q) : q;
}

static I128 i128_mod(I128 a, I128 b){
    if(i128_is_zero(b)) return a;
    return i128_sub(a, i128_mul(i128_div(a, b), b));
}

static I128 i128_gcd(I128 a, I128 b){
    a = i128_abs(a); b = i128_abs(b);
    while(!i128_is_zero(b)){ I128 t = i128_mod(a, b); a = b; b = t; }
    return i128_is_zero(a) ? i128_from_i64(1) : a;
}

/* ── O DUAL DESTE ANDAR — a troca das metades, ν∘ν = id ──────────────────────
 * `torre_alg.h` fixa QUATRO operações estruturais por andar: soma, produto,
 * dual e inversão/fibra. Este andar tinha soma e produto e não tinha DUAL — era
 * o único da torre sem ele (E₁₆ tem o swap ι, D32 tem `d32_dual`, D64 tem
 * `d64_dual`, Hip tem `hip_conj`), e o medidor `torre_alg.c` §T1 media a
 * involução em QUATRO sítios onde a torre tem cinco.
 *
 * É a mesma troca alto/baixo dos andares de baixo, um andar acima: o par é
 * (hi, lo) e o dual troca-os. Aplicado duas vezes devolve — que é o que faz
 * dele uma involução e não uma permutação qualquer. */
static I128 i128_dual(I128 x){
    I128 r;
    r.hi = (int64_t)x.lo;
    r.lo = (uint64_t)x.hi;
    return r;
}

static I128 i128_shl(I128 a, int sh){
    if(sh <= 0) return a;
    if(sh >= 128) return i128_zero();
    if(sh >= 64){
        I128 r;
        r.lo = 0;
        r.hi = (int64_t)a.lo << (sh - 64);
        return r;
    }
    I128 r;
    r.lo = a.lo << sh;
    r.hi = ((int64_t)a.lo >> (64 - sh)) | (a.hi << sh);
    return r;
}

#endif
