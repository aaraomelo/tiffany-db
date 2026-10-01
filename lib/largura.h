/* largura.h — UMA LEI PARA TODA A ESCADA: o produto w×w → par de w bits, com w PARÂMETRO.
 *
 * O `dual16.h` faz 16×16 → par de 16. O `dual32.h` faz 32×32 → par de 32. Postos lado a
 * lado, são o MESMO texto com as larguras trocadas: quatro parcelas de meia largura, a
 * soma do meio a transbordar, o transporte lido pela volta do `unsigned`. O próprio
 * cabeçalho do `dual16.h` diz «a mesma lei que dual32.h, um degrau abaixo».
 *
 * E o `binario.h` mostrou, um andar mais abaixo, que a lei se escreve UMA vez e o andar é
 * argumento. Este ficheiro põe isso à prova na escada de larguras — sem tocar nos headers:
 *
 *      lg_mult(w, a, b)      w ∈ {1, 2, 4, 8, 16, 32}      SEIS andares, um corpo
 *
 * ── O QUE AQUI SE AFIRMA, E O QUE NÃO ─────────────────────────────────────────────
 * NÃO se afirma que os headers estão errados nem que devem sair. Afirma-se o que se pode
 * medir: os dois caminhos dão o MESMO par, bit a bit, e portanto a especialização por
 * andar é evitável. Remover é decisão do coordenador, e vem depois da equivalência —
 * caminho novo primeiro, equivalência medida, remoção por último.
 *
 * ── O VEÍCULO E O PARÂMETRO, que não se confundem ─────────────────────────────────
 * O tipo da máquina (`uint64_t`) é o VEÍCULO; `w` é o PARÂMETRO. Toda a aritmética é
 * mod 2^w EXPLÍCITO — cada passo mascara —, e o transporte lê-se por comparação, como
 * nos headers. Por isso o tecto é w ≤ 32: o par ocupa 2w ≤ 64 bits do veículo. Acima
 * disso o veículo teria de ser ele próprio um par, que é o andar do `i128.h`.
 *
 * ── E O TRANSPORTE, OUTRA VEZ ─────────────────────────────────────────────────────
 * `lg_mult_f2` é a MESMA dobra com o XOR no lugar da soma: dá o produto de 𝔽₂[x] sem
 * redução (o carry-less). A diferença entre a aritmética de ℕ e a do corpo binário, no
 * produto, é exactamente o transporte — que é o `naturais.tex thm:transporte` um andar
 * acima, agora no produto e não na soma.
 */
#ifndef LARGURA_H
#define LARGURA_H
#include <stdint.h>

#define LG_MAX 32                      /* 2w ≤ 64: o par tem de caber no veículo */

typedef struct { uint64_t alto, baixo; } LgPar;      /* valor = alto·2^w + baixo */

static uint64_t lg_masc(int w){
    return (w >= 64) ? ~(uint64_t)0 : (((uint64_t)1 << w) - 1);
}
/* o par de w bits lido como valor de 2w bits — só onde 2w cabe no veículo */
static uint64_t lg_val(LgPar p, int w){ return (p.alto << w) | p.baixo; }

/* ── O PRODUTO: parte em metades, desce, e compõe com o transporte à mão ──────────
 * (a1·2^h + a0)(b1·2^h + b0) = p11·2^w + (p01+p10)·2^h + p00,  h = w/2.
 * Cada parcela cabe em w bits porque (2^h−1)² < 2^w. O que NÃO cabe é a soma do meio,
 * e é aí que o transporte aparece — guardado, não perdido. */
static LgPar lg_mult(int w, uint64_t a, uint64_t b){
    if(w == 1){ LgPar r; r.alto = 0; r.baixo = a & b & 1u; return r; }
    int h = w / 2;
    uint64_t mh = lg_masc(h), mw = lg_masc(w);
    uint64_t a0 = a & mh, a1 = (a >> h) & mh;
    uint64_t b0 = b & mh, b1 = (b >> h) & mh;

    uint64_t p00 = lg_val(lg_mult(h, a0, b0), h);
    uint64_t p01 = lg_val(lg_mult(h, a0, b1), h);
    uint64_t p10 = lg_val(lg_mult(h, a1, b0), h);
    uint64_t p11 = lg_val(lg_mult(h, a1, b1), h);

    uint64_t meio   = (p01 + p10) & mw;
    uint64_t transp = (meio < p01) ? 1u : 0u;        /* a volta É o transporte */

    LgPar r;
    r.alto = (p11 + (meio >> h) + (transp << h)) & mw;
    uint64_t baixo = (p00 + ((meio << h) & mw)) & mw;
    if(baixo < p00) r.alto = (r.alto + 1) & mw;      /* o segundo transporte */
    r.baixo = baixo;
    return r;
}

/* ── A SOMA do par, com o transporte à mão — a mesma lei que d64_soma ───────────── */
static LgPar lg_soma(int w, LgPar x, LgPar y){
    uint64_t mw = lg_masc(w);
    LgPar r;
    r.baixo = (x.baixo + y.baixo) & mw;
    r.alto  = (x.alto + y.alto + (r.baixo < x.baixo ? 1u : 0u)) & mw;
    return r;
}
/* ── O DUAL do par: trocar as metades — a memória da divisão (d32_dual/d64_dual) ── */
static LgPar lg_dual(LgPar x){ LgPar r; r.alto = x.baixo; r.baixo = x.alto; return r; }

/* ── A MESMA DOBRA SEM TRANSPORTE: o produto de 𝔽₂[x], sem redução ───────────────
 * Devolve o valor de 2w bits (não o par): em 𝔽₂ não há transporte a guardar. */
static uint64_t lg_mult_f2(int w, uint64_t a, uint64_t b){
    if(w == 1) return a & b & 1u;
    int h = w / 2;
    uint64_t mh = lg_masc(h);
    uint64_t a0 = a & mh, a1 = (a >> h) & mh;
    uint64_t b0 = b & mh, b1 = (b >> h) & mh;
    uint64_t p00 = lg_mult_f2(h, a0, b0);
    uint64_t p01 = lg_mult_f2(h, a0, b1);
    uint64_t p10 = lg_mult_f2(h, a1, b0);
    uint64_t p11 = lg_mult_f2(h, a1, b1);
    return (p11 << (2*h)) ^ ((p01 ^ p10) << h) ^ p00;   /* ⊕ no lugar de + */
}

/* ── amostragem DETERMINÍSTICA: a semente está escrita, não vem do relógio ──────── */
static uint64_t lg_prox(uint64_t *s){
    *s = *s * 6364136223846793005ULL + 1442695040888963407ULL;
    return *s >> 11;
}
#endif
