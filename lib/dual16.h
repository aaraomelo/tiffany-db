/* dual16.h — 32 BITS SÃO DOIS DUAIS DE 16, e o produto é onde a dualidade nasce.
 *
 * O par (alto, baixo) materializa exactamente os 32 bits de um produto 16×16 —
 * a mesma lei que `dual32.h`, um degrau abaixo. A soma e a multiplicação dos
 * pares seguem `racionais.tex` Def.~\ref{def:ops}:
 *
 *      (a,b) ⊕ (c,d) = (ad+bc, bd)
 *      (a,b) ⊗ (c,d) = (ac, bd)
 *
 * A comparação por produto cruzado lê o `alto` como signed int16 primeiro,
 * depois o `baixo` como unsigned — `racionais.tex` §Q16.
 *
 * ANDAR 32 da torre (torre_alg.h): produto 16×16 → par D32. Próximo andar: dual32.h.
 * Involução no par racional: ι(p,q)=(q,p) — d16_par_dual. */
#ifndef DUAL16_H
#define DUAL16_H

#include <stdint.h>

typedef struct { uint16_t alto, baixo; } D32;

#define D16_META 0xFFu
#define D16_MEIO 8

static D32 d32_zero(void){ D32 r = {0, 0}; return r; }
static D32 d32_de(uint16_t x){ D32 r = {0, x}; return r; }
static int d32_e_zero(D32 x){ return x.alto == 0 && x.baixo == 0; }

static uint16_t d16_abs_u(int16_t v){
    return v < 0 ? (uint16_t)(0u - (uint16_t)v) : (uint16_t)v;
}

static int32_t d32_to_i32(D32 x){
    return (int32_t)((uint32_t)x.baixo | ((uint32_t)x.alto << 16));
}

static D32 d32_from_i32(int32_t v){
    D32 r;
    r.baixo = (uint16_t)v;
    r.alto = (uint16_t)(v >> 16);
    return r;
}

/* produto unsigned 16×16 → par D32, sem tipo mais largo que uint16 nas parcelas */
static D32 d16_mult_u(uint16_t a, uint16_t b){
    uint16_t a0 = (uint16_t)(a & D16_META), a1 = (uint16_t)(a >> D16_MEIO);
    uint16_t b0 = (uint16_t)(b & D16_META), b1 = (uint16_t)(b >> D16_MEIO);

    uint16_t p00 = (uint16_t)(a0 * b0);
    uint16_t p01 = (uint16_t)(a0 * b1);
    uint16_t p10 = (uint16_t)(a1 * b0);
    uint16_t p11 = (uint16_t)(a1 * b1);

    uint16_t meio = (uint16_t)(p01 + p10);
    uint16_t transp = (meio < p01) ? 1u : 0u;

    D32 r;
    r.alto  = (uint16_t)(p11 + (meio >> D16_MEIO) + (transp << D16_MEIO));
    uint16_t baixo = (uint16_t)(p00 + (meio << D16_MEIO));
    if(baixo < p00) r.alto++;
    r.baixo = baixo;
    return r;
}

static D32 d16_mult(int16_t a, int16_t b){
    D32 m = d16_mult_u(d16_abs_u(a), d16_abs_u(b));
    if(((a < 0) ^ (b < 0)) && !d32_e_zero(m))
        return d32_from_i32(-d32_to_i32(m));
    return m;
}

/* soma e diferença do par como int32 em complemento de dois */
static D32 d16_soma(D32 x, D32 y){
    return d32_from_i32(d32_to_i32(x) + d32_to_i32(y));
}

static D32 d16_menos(D32 x, D32 y){
    return d32_from_i32(d32_to_i32(x) - d32_to_i32(y));
}

/* ordem: alto signed, baixo unsigned — racionais.tex */
static int d16_cmp(D32 x, D32 y){
    int32_t a = d32_to_i32(x), b = d32_to_i32(y);
    return a < b ? -1 : (a > b ? 1 : 0);
}

static int d16_cmp_prod(int16_t a, int16_t b, int16_t c, int16_t d){
    return d16_cmp(d16_mult(a, b), d16_mult(c, d));
}

/* ── OPERAÇÕES NO PAR RACIONAL — racionais.tex Def. def:ops ─────────────────
 * Devolve 0 se algum numerador/denominador do par resultado transbordar o
 * envelope (alto ≠ 0 num produto que devia caber em 16 bits). */
typedef struct { D32 p; D32 q; } D32par;

static int d16_par_mult(int16_t a, int16_t b, int16_t c, int16_t d, D32par *r){
    r->p = d16_mult(a, c);
    r->q = d16_mult(b, d);
    return 1;
}

static int d16_par_soma(int16_t a, int16_t b, int16_t c, int16_t d, D32par *r){
    D32 ad = d16_mult(a, d);
    D32 bc = d16_mult(b, c);
    r->p = d16_soma(ad, bc);
    r->q = d16_mult(b, d);
    return 1;
}

static int d16_sinal_det(int16_t a, int16_t b, int16_t c, int16_t d){
    return d16_cmp_prod(a, d, b, c);
}

/* involução ι: (p,q)↦(q,p) — inverso multiplicativo, racionais.tex thm:swap-inverso */
static D32par d16_par_dual(D32par x){
    D32par r = {x.q, x.p};
    return r;
}

/* dual do produto: troca as metades — memória da divisão (dual32.h mesma lei) */
static D32 d32_dual(D32 x){
    D32 r = {x.baixo, x.alto};
    return r;
}

#endif
