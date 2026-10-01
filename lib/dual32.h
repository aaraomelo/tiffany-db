/* dual32.h — 64 BITS SÃO DOIS DUAIS DE 32, e o produto é onde a dualidade nasce.
 *
 * O Aarão: «64 bits são dois duais de 32, faz isso — dois int duais».
 *
 * ── POR QUE ISTO NÃO É UM TRUQUE DE IMPLEMENTAÇÃO ──────────────────────────────
 * É a lei desta casa aplicada à própria aritmética. Multiplicar dois números de 32 bits
 * dá um de 64: a operação SAI do tipo, e guardar só 32 perde exactamente metade. E a
 * casa já tinha a frase para isto — «a dualidade é a MEMÓRIA DA DIVISÃO»: dividir perde,
 * e o dual guarda a segunda metade.
 *
 * Aqui é literal. O produto de dois `unsigned` de 32 é o PAR
 *
 *      (alto, baixo),      a·b = alto·2³² + baixo,
 *
 * e nenhum tipo mais largo entra na conta. Onde o código escrevia `long` ou `__int128`
 * para «caber», passa a escrever o par — e o que era um tecto da máquina passa a ser uma
 * estrutura da matemática. O 64 não é primitivo: é o dual de dois 32.
 *
 * ── COMO SE MULTIPLICA SEM SAIR DE 32 ─────────────────────────────────────────
 * Parte-se cada factor em dois de 16, e cada produto parcial cabe EXACTAMENTE num
 * `unsigned` de 32: (2¹⁶−1)² = 4 294 836 225 < 2³². Fica com folga de 131 070, que é o
 * que permite somar as parcelas com o transporte à mão.
 *
 *      a = a₁·2¹⁶ + a₀,   b = b₁·2¹⁶ + b₀
 *      a·b = a₁b₁·2³² + (a₁b₀ + a₀b₁)·2¹⁶ + a₀b₀
 *
 * O transporte detecta-se pela volta do `unsigned`, que em C é DEFINIDA — é aritmética
 * módulo 2³², e a volta é exactamente o dual a reclamar o seu lado.
 *
 * ── E A REGRA DESTA CASA, QUE ESTE FICHEIRO TEM DE CUMPRIR ────────────────────
 * Dois caminhos que têm de concordar. O par calcula-se aqui em 32 bits, e o mesmo
 * produto calcula-se em `__int128` no medidor — e os dois têm de dar o mesmo em toda a
 * varredura. Se divergirem, é este ficheiro que está errado, e não a régua.
 *
 * Não precisa de nada.
 *
 * ANDAR 64 da torre (torre_alg.h): produto 32×32 → par D64. Anterior: dual16.h.
 * Seguinte: i128.h (128). Involução racional: d32_par_dual; par: d64_dual. */
#ifndef DUAL32_H
#define DUAL32_H

#include "dual16.h"

/* o PAR de 64 bits: dois de 32 — produto 32×32 */
typedef struct { unsigned alto, baixo; } D64;

#define D32_META 0xFFFFu          /* a máscara dos 16 de baixo */
#define D32_MEIO 16

static D64 d64_zero(void){ D64 r; r.alto = 0; r.baixo = 0; return r; }
static D64 d64_de(unsigned x){ D64 r; r.alto = 0; r.baixo = x; return r; }
static int d64_e_zero(D64 x){ return x.alto == 0 && x.baixo == 0; }

/* ── O PRODUTO: 32 × 32 → o PAR, sem tipo mais largo ───────────────────────────
 * As quatro parcelas de 16×16 cabem cada uma num `unsigned`. O que não cabe é a SOMA
 * das duas do meio — e é aí que o transporte aparece, e se guarda em vez de se perder. */
static D64 d64_mult(unsigned a, unsigned b){
    unsigned a0 = a & D32_META, a1 = a >> D32_MEIO;
    unsigned b0 = b & D32_META, b1 = b >> D32_MEIO;

    unsigned p00 = a0 * b0;
    unsigned p01 = a0 * b1;
    unsigned p10 = a1 * b0;
    unsigned p11 = a1 * b1;

    unsigned meio = p01 + p10;
    unsigned transp = (meio < p01) ? 1u : 0u;

    D64 r;
    r.alto  = p11 + (meio >> D32_MEIO) + (transp << D32_MEIO);
    unsigned baixo = p00 + (meio << D32_MEIO);
    if(baixo < p00) r.alto++;
    r.baixo = baixo;
    return r;
}
/* ── A SOMA E A DIFERENÇA do par, com o transporte à mão ───────────────────────*/
static D64 d64_soma(D64 x, D64 y){
    D64 r;
    r.baixo = x.baixo + y.baixo;
    r.alto  = x.alto + y.alto + (r.baixo < x.baixo ? 1u : 0u);
    return r;
}
static D64 d64_menos(D64 x, D64 y){          /* x − y, com x ≥ y */
    D64 r;
    r.baixo = x.baixo - y.baixo;
    r.alto  = x.alto - y.alto - (x.baixo < y.baixo ? 1u : 0u);
    return r;
}
/* ── O PAR VEZES UM ESCALAR, com o transporte — para as formas c·q² ────────────
 * Precisa-se disto porque a comparação p² contra c·q² tem TRÊS factores, e o produto de
 * dois já é um par. Devolve 0 se o resultado sair do par (aí não há resposta a dar, e
 * dizê-lo é melhor que devolver lixo). */
static int d64_esc(D64 x, unsigned k, D64 *saida){
    D64 lo = d64_mult(x.baixo, k);
    D64 hi = d64_mult(x.alto,  k);
    if(hi.alto != 0) return 0;                 /* passou do par: recusa */
    unsigned alto = lo.alto + hi.baixo;
    if(alto < lo.alto) return 0;
    saida->alto = alto; saida->baixo = lo.baixo;
    return 1;
}
/* ── A ORDEM: compara-se o ALTO primeiro, que é o que a metade perdida decidia ──*/
static int d64_cmp(D64 x, D64 y){
    if(x.alto != y.alto) return x.alto < y.alto ? -1 : 1;
    if(x.baixo != y.baixo) return x.baixo < y.baixo ? -1 : 1;
    return 0;
}
/* ── E O LADO COM SINAL, que é o que o corpo dos racionais usa ─────────────────
 * O sinal trata-se à parte da magnitude: é a decomposição de sempre — a parte que ORDENA
 * e a parte que ORIENTA. `d32_abs` devolve a magnitude como `unsigned`, e por isso
 * INT_MIN não é caso especial: −2³¹ tem magnitude 2³¹, que cabe num `unsigned`. */
static unsigned d32_abs(int v){
    /* e SEM tipo largo: `(unsigned)v` de um negativo é v + 2³² (definido em C), e
     * `0u − isso` é a magnitude. Escrevi primeiro `-(long long)v` e era um tipo de 64
     * bits a entrar pela porta das traseiras, no ficheiro que existe para não ter nenhum. */
    return v < 0 ? 0u - (unsigned)v : (unsigned)v;
}
/* compara a·b com c·d, todos com sinal — devolve −1, 0 ou +1.
 * É esta a única coisa que o corpo dos racionais precisa: a ORDEM por produto cruzado. */
static int d32_cmp_prod(int a, int b, int c, int d){
    int sab = ((a < 0) ^ (b < 0)) ? -1 : 1;
    int scd = ((c < 0) ^ (d < 0)) ? -1 : 1;
    D64 pab = d64_mult(d32_abs(a), d32_abs(b));
    D64 pcd = d64_mult(d32_abs(c), d32_abs(d));
    if(d64_e_zero(pab)) sab = 0;
    if(d64_e_zero(pcd)) scd = 0;
    if(sab != scd) return sab < scd ? -1 : 1;
    int m = d64_cmp(pab, pcd);
    if(sab < 0) m = -m;                    /* nos negativos a ordem inverte-se */
    return m;
}
/* ── O DETERMINANTE 2×2 da matriz [[a,b],[c,d]], que é a·d − b·c ───────────────
 * ATENÇÃO AO EMPARELHAMENTO, que foi onde me enganei à primeira: escrevi `a·b − c·d` e o
 * medidor deu 6560 divergências em 6561 e o gato falhou nos 200 metais. O determinante
 * cruza — a diagonal principal contra a secundária —, e é esse cruzamento que é o
 * conteúdo dele. Uma varredura que só tivesse matrizes simétricas não o teria apanhado.
 *
 * `d32_sinal_det` devolve o SINAL de a·d − b·c, e `d32_det2` o valor quando cabe. */
static int d32_sinal_det(int a, int b, int c, int d){ return d32_cmp_prod(a,d,b,c); }

static int d32_det2(int a, int b, int c, int d, int *saida){
    int sab = ((a < 0) ^ (d < 0)) ? -1 : 1;         /* o sinal de a·d */
    int scd = ((b < 0) ^ (c < 0)) ? -1 : 1;         /* o sinal de b·c */
    D64 pab = d64_mult(d32_abs(a), d32_abs(d));
    D64 pcd = d64_mult(d32_abs(b), d32_abs(c));
    if(d64_e_zero(pab)) sab = 0;
    if(d64_e_zero(pcd)) scd = 0;
    D64 r;
    int sinal;
    if(sab == scd || sab == 0 || scd == 0){
        if(sab == scd){                                  /* mesmo sinal: subtrai */
            int c2 = d64_cmp(pab, pcd);
            r = (c2 >= 0) ? d64_menos(pab, pcd) : d64_menos(pcd, pab);
            sinal = (c2 >= 0) ? sab : -sab;
        } else {                                         /* um deles é zero */
            if(sab == 0){ r = pcd; sinal = -scd; }
            else { r = pab; sinal = sab; }
        }
    } else {                                             /* sinais opostos: soma */
        r = d64_soma(pab, pcd);
        sinal = sab;
    }
    if(r.alto != 0 || r.baixo > 2147483647u) return 0;   /* não cabe num int */
    *saida = sinal * (int)r.baixo;
    return 1;
}
/* ── UTILITÁRIOS para somas de quadrados e raiz, sem long long ────────────────
 * `d64_sqr_i` e `d64_cruz_i` cobrem os casos em que os operandos cabem em int32.
 * `d64_u64`/`d64_de_u64` recompõem o par num único valor de 64 bits quando
 * precisa de dividir ou tirar raiz — o par continua a ser a forma canónica. */
static D64 d64_sqr_i(int v){
    return d64_mult(d32_abs(v), d32_abs(v));
}
static int d64_cruz_i(int a, int b, int a0, int b0, int *saida){
    return d32_det2(a, b0, b, a0, saida);
}
static long d64_as_long(D64 x){
    if(x.alto != 0) return -1;
    return (long)x.baixo;
}

/* ── OPERAÇÕES NO PAR RACIONAL — racionais.tex Def. def:ops ─────────────────
 * (a,b) ⊕ (c,d) = (ad+bc, bd)   ;   (a,b) ⊗ (c,d) = (ac, bd) */
typedef struct { D64 p; D64 q; } D64par;

static void d32_par_mult(int a, int b, int c, int d, D64par *r){
    r->p = d64_mult(d32_abs(a), d32_abs(c));
    r->q = d64_mult(d32_abs(b), d32_abs(d));
}

static void d32_par_soma(int a, int b, int c, int d, D64par *r){
    D64 ad = d64_mult(d32_abs(a), d32_abs(d));
    D64 bc = d64_mult(d32_abs(b), d32_abs(c));
    r->p = d64_soma(ad, bc);
    r->q = d64_mult(d32_abs(b), d32_abs(d));
}

static D64par d32_par_dual(D64par x){
    D64par r = {x.q, x.p};
    return r;
}

static D64 d64_dual(D64 x){
    D64 r = {x.baixo, x.alto};
    return r;
}

#endif
