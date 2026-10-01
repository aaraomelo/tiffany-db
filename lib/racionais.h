/* racionais.h — ℚ: A REVERSIBILIDADE DA MULTIPLICAÇÃO NÃO NULA.
 *
 *  ordem do coordenador põe a escada e o que cada andar acrescenta:
 *
 *     ℕ: + ×        ℤ: + × −  (reversibilidade da SOMA)
 *     ℚ: + × − ÷    (reversibilidade da MULTIPLICAÇÃO não nula)
 *
 * e a cadeia: construção → oposto → inverso.
 *
 * O gume do andar é dito na língua desta casa, e não podia ser melhor:
 *
 *     «divisão por zero não é uma aproximação ruim; é uma operação SEM FIBRA.»
 *
 * A fibra é a divisão das cinco operações — dado o produto e um fator, achar o outro.
 * Com o fator zero não há fibra nenhuma: nenhum x cumpre 0·x = 3, e TODOS cumprem
 * 0·x = 0. Não é um valor difícil de achar: é a ausência da fibra, e diz-se.
 *
 * O racional é uma CLASSE, não o par que calhou escrito: (a,b) ~ (c,d) ⟺ ad = bc. Por
 * isso cada operação tem de estar BEM DEFINIDA — trocar o representante não pode mudar
 * o resultado —, e é isso que aqui se mede. */
/* ═══ O RACIONAL — ENVELOPE E₁₆ COM PROMOÇÃO NO HOT PATH ══════════════════════
 * Histórico (Teorema do Gato / racionais.tex §Q16):
 *   1. REDUZ-SE ANTES DE MULTIPLICAR.
 *   2. O que não cabe em E₁₆ CONTA-SE (`qz_saturou`).
 *   3. (20/08) saturo→promove: em vez de devolver ±32767, guarda-se o exacto em
 *      int64 (andar acima do E₁₆). QzP/I128 continua o testemunho fora do int64.
 *
 * Igualdade/ordem: produto cruzado exacto via i128 (64×64), não d16 truncado. */
#ifndef RACIONAIS_H
#define RACIONAIS_H

#include "dual16.h"
#include "dual32.h"
#include "i128.h"

typedef struct { int64_t p, q; } Qz;         /* p/q; cabe em E₁₆ ou promovido a int64 */
typedef struct { I128 p, q; } QzP;           /* par I128 — fora do int64 */

static long qz_saturou = 0;    /* saiu de E₁₆ (promoveu) — contado À PARTE dos defeitos */
/* ── E O OUTRO CONTADOR, QUE NÃO É O MESMO ────────────────────────────────────
 * `qz_saturou` conta a PROMOÇÃO: o valor saiu de E₁₆ e continua EXACTO no andar de
 * cima. `qz_perdeu` conta a PERDA: nem no int64 coube, o valor foi DESCARTADO e o
 * que se lê (0/1) não é o número. São eventos diferentes e por isso são gavetas
 * diferentes — quem quer saber «este valor ainda é o valor?» pergunta a este.
 *
 * Antes de 20/08 os dois coincidiam (sair de E₁₆ era truncar), e por isso quem
 * vigiava a honestidade de uma sucessão lia `qz_saturou`. Depois da promoção
 * deixaram de coincidir, e essa leitura passou a dizer o contrário do que mede. */
static long qz_perdeu = 0;

/* ── A PERGUNTA DO TEOREMA DO GATO, FEITA AO PRÓPRIO TIPO ──────────────────────
 * Com -DQZ_MEDE regista-se a maior magnitude que passa por um racional. Agora ela é um
 * complemento e não o instrumento principal: o instrumento é o `qz_saturou`, que vê o
 * número ANTES de ele enrolar. */
#ifdef QZ_MEDE
#include <stdio.h>
#include <stdlib.h>
static long qz_max_pq = 0;
static int qz_reg = 0;
static void qz_rodape(void){
    fprintf(stderr, "#QZMAX %ld %ld\n", qz_max_pq, qz_saturou);
}
static void qz_ve(long v){
    long a = v < 0 ? -v : v;
    if(!qz_reg){ qz_reg = 1; atexit(qz_rodape); }
    if(a > qz_max_pq) qz_max_pq = a;
}
#else
#define qz_ve(v)   ((void)0)
#endif

static long qz_mdc(long a, long b){
    if(a < 0) a = -a;
    if(b < 0) b = -b;
    while(b){ long t = a % b; a = b; b = t; }
    return a ? a : 1;
}
static int64_t qz_abs64(int64_t v){ return v < 0 ? -v : v; }
/* cabe em E₁₆? — detector do Gato (promove quando falso; já não trunca) */
static int qz_cabe(long v){ return v <= 32767L && v >= -32767L; }

/* forma REDUZIDA; saturo→promove: fora de E₁₆ conta e GUARDA o exacto em int64 */
static Qz qz(long p, long q){
    Qz r;
    if(q < 0){ p = -p; q = -q; }
    long g = qz_mdc(p, q);
    p /= g; q /= g;
    if(p == 0) q = 1;
    if(!qz_cabe(p) || !qz_cabe(q))
        qz_saturou++;                    /* saiu de E₁₆: promove — não devolve tecto */
    qz_ve(p); qz_ve(q);
    r.p = p; r.q = q;
    return r;
}
/* ── A ORDEM E A IGUALDADE: produto cruzado exacto (i128), nunca d16 truncado ─ */
static int qz_igual(Qz a, Qz b){
    return i128_cmp(i128_smul(a.p, b.q), i128_smul(b.p, a.q)) == 0;
}
static int qz_menor(Qz a, Qz b){
    /* a/b < c/d com b,d > 0 após qz(); sinal no numerador */
    int64_t bq = a.q, dq = b.q;
    if(bq < 0){ a.p = -a.p; bq = -bq; }
    if(dq < 0){ b.p = -b.p; dq = -dq; }
    return i128_cmp(i128_smul(a.p, dq), i128_smul(b.p, bq)) < 0;
}

/* ── A SOMA: cancela em cruz; intermédios em i128; resultado via qz() ───────── */
static Qz qz_soma(Qz a, Qz b){
    long g = qz_mdc((long)qz_abs64(a.q), (long)qz_abs64(b.q));
    long bq = (long)(b.q / g), aq = (long)(a.q / g);
    I128 n1 = i128_smul(a.p, bq);
    I128 n2 = i128_smul(b.p, aq);
    I128 den = i128_smul(a.q, bq);
    I128 num = i128_add(n1, n2);
    if(!i128_fits_i64(num) || !i128_fits_i64(den)){
        qz_saturou++; qz_perdeu++;       /* fora do int64: PERDEU-SE, e diz-se */
        return qz(0, 1);
    }
    return qz(i128_to_i64(num), i128_to_i64(den));
}
/* ── O PRODUTO: cancela em cruz, dos DOIS lados ─────────────────────────────── */
static Qz qz_mult(Qz a, Qz b){
    long g1 = qz_mdc((long)a.p, (long)b.q), g2 = qz_mdc((long)b.p, (long)a.q);
    int64_t ap = a.p / g1, bq = b.q / g1;
    int64_t bp = b.p / g2, aq = a.q / g2;
    I128 np = i128_smul(ap, bp), nq = i128_smul(aq, bq);
    if(!i128_fits_i64(np) || !i128_fits_i64(nq)){
        qz_saturou++; qz_perdeu++;
        return qz(0, 1);
    }
    return qz(i128_to_i64(np), i128_to_i64(nq));
}
static Qz qz_oposto(Qz a){ Qz r; r.p = -a.p; r.q = a.q; return r; }

static int qz_inverso(Qz a, Qz *r){
    if(a.p == 0) return 0;                     /* 0⁻¹ NÃO EXISTE — sem fibra */
    *r = qz(a.q, a.p);
    return 1;
}
static int qz_divide(Qz a, Qz b, Qz *r){
    Qz i;
    if(!qz_inverso(b, &i)) return 0;
    *r = qz_mult(a, i);
    return 1;
}
static Qz qz_medio(Qz a, Qz b){
    I128 n = i128_add(i128_smul(a.p, b.q), i128_smul(b.p, a.q));
    I128 den = i128_smul_i128(i128_from_i64(2LL * a.q), b.q);
    if(!i128_fits_i64(n) || !i128_fits_i64(den)){ qz_saturou++; qz_perdeu++; return qz(0,1); }
    int64_t num = i128_to_i64(n), d = i128_to_i64(den);
    long g = qz_mdc(num < 0 ? -num : num, d < 0 ? -d : d);
    return qz((long)(num / g), (long)(d / g));
}
static long qz_arquimediano(Qz a){
    long n = a.p < 0 ? -(long)a.p : (long)a.p;
    return n / (long)a.q + 1;
}
static int qz_dist_menor(Qz a, Qz b, Qz eps){
    if(eps.p <= 0 || eps.q <= 0) return 0;
    I128 x = i128_smul(a.p, b.q);
    I128 y = i128_smul(b.p, a.q);
    I128 n = i128_cmp(x, y) >= 0 ? i128_sub(x, y) : i128_sub(y, x);
    if(i128_negativo(n)) n = i128_neg(n);
    I128 esq = i128_smul_i128(n, qz_abs64(eps.q));
    I128 d = i128_smul(qz_abs64(a.q), qz_abs64(b.q));
    I128 dir = i128_smul_i128(d, qz_abs64(eps.p));
    return i128_cmp(esq, dir) < 0;
}
static int qz_cmp_quad(Qz x, long c, int *bom){
    if(bom) *bom = 1;
    if(c < 0) return 1;
    I128 e = i128_smul(x.p, x.p);
    I128 q2 = i128_smul(x.q, x.q);
    I128 d = i128_smul_i128(q2, c);
    return i128_cmp(e, d);
}
static int qz_dist_menor_ig(Qz a, Qz b, Qz eps){
    if(eps.p <= 0 || eps.q <= 0) return 0;
    I128 x = i128_smul(a.p, b.q);
    I128 y = i128_smul(b.p, a.q);
    I128 n = i128_cmp(x, y) >= 0 ? i128_sub(x, y) : i128_sub(y, x);
    if(i128_negativo(n)) n = i128_neg(n);
    I128 esq = i128_smul_i128(n, qz_abs64(eps.q));
    I128 d = i128_smul(qz_abs64(a.q), qz_abs64(b.q));
    I128 dir = i128_smul_i128(d, qz_abs64(eps.p));
    return i128_cmp(esq, dir) <= 0;
}
static Qz qz_de_inteiro(long n){ return qz(n, 1); }

/* ── PROMOÇÃO I128: testemunho fora do int64; qz() já promove E₁₆→int64 ───────
 * qz_prom_sat = ainda fora de E₁₆ (detector). Após a troca, qz() concorda com a
 * classe exacta sempre que o promovido cabe em int64. */
static int qz_prom_sat(QzP w){
    if(!i128_fits_i64(w.p) || !i128_fits_i64(w.q)) return 1;
    long p = i128_to_i64(w.p), q = i128_to_i64(w.q);
    return !qz_cabe(p) || !qz_cabe(q);
}
static QzP qz_prom(long p, long q){
    QzP r;
    if(q < 0){ p = -p; q = -q; }
    long g = qz_mdc(p, q);
    p /= g; q /= g;
    if(p == 0) q = 1;
    r.p = i128_from_i64(p);
    r.q = i128_from_i64(q);
    return r;
}
static QzP qz_prom_de(Qz a){
    QzP r;
    r.p = i128_from_i64((int64_t)a.p);
    r.q = i128_from_i64((int64_t)a.q);
    return r;
}
static int qz_prom_igual(QzP x, QzP y){
    if(!i128_fits_i64(x.p) || !i128_fits_i64(x.q) ||
       !i128_fits_i64(y.p) || !i128_fits_i64(y.q)) return 0;
    return i128_cmp(i128_smul(i128_to_i64(x.p), i128_to_i64(y.q)),
                      i128_smul(i128_to_i64(y.p), i128_to_i64(x.q))) == 0;
}
static Qz qz_prom_estreita(QzP w){ return qz(i128_to_i64(w.p), i128_to_i64(w.q)); }
static int qz_prom_estreito_bate(Qz r, QzP ex){
    /* após saturo→promove: qz() guarda a classe sempre que cabe em int64 */
    if(!i128_fits_i64(ex.p) || !i128_fits_i64(ex.q)) return 0;
    return qz_igual(r, qz(i128_to_i64(ex.p), i128_to_i64(ex.q)));
}
static QzP qz_prom_soma(Qz a, Qz b){
    long g = qz_mdc((long)qz_abs64(a.q), (long)qz_abs64(b.q));
    long bq = (long)(b.q / g), aq = (long)(a.q / g);
    I128 n1 = i128_smul(a.p, bq), n2 = i128_smul(b.p, aq);
    I128 den = i128_smul(a.q, bq);
    I128 num = i128_add(n1, n2);
    if(!i128_fits_i64(num) || !i128_fits_i64(den))
        return qz_prom(0, 1);
    return qz_prom(i128_to_i64(num), i128_to_i64(den));
}
static QzP qz_prom_soma_pp(QzP a, QzP b){
    I128 n1 = i128_smul_i128(a.p, i128_to_i64(b.q));
    I128 n2 = i128_smul_i128(b.p, i128_to_i64(a.q));
    I128 den = i128_smul_i128(a.q, i128_to_i64(b.q));
    return qz_prom(i128_to_i64(i128_add(n1, n2)), i128_to_i64(den));
}
static QzP qz_prom_mult(Qz a, Qz b){
    long g1 = qz_mdc((long)a.p, (long)b.q), g2 = qz_mdc((long)b.p, (long)a.q);
    int64_t ap = a.p / g1, bq = b.q / g1;
    int64_t bp = b.p / g2, aq = a.q / g2;
    I128 np = i128_smul(ap, bp), nq = i128_smul(aq, bq);
    if(!i128_fits_i64(np) || !i128_fits_i64(nq)) return qz_prom(0, 1);
    return qz_prom(i128_to_i64(np), i128_to_i64(nq));
}
static QzP qz_prom_mult_pp(QzP a, QzP b){
    return qz_prom(i128_to_i64(i128_smul_i128(a.p, i128_to_i64(b.p))),
                 i128_to_i64(i128_smul_i128(a.q, i128_to_i64(b.q))));
}

/* ── QzX: estreito (= qz promovido) + testemunho I128; saturo = saiu de E₁₆ ── */
typedef struct { Qz estreito; QzP promovido; int saturo; } QzX;

static QzX qz_x(long p, long q){
    QzX r;
    r.promovido = qz_prom(p, q);
    r.saturo = qz_prom_sat(r.promovido);
    r.estreito = qz(p, q);
    return r;
}
static QzX qz_x_de(Qz a){
    QzX r;
    r.promovido = qz_prom_de(a);
    r.saturo = qz_prom_sat(r.promovido);
    r.estreito = a;
    return r;
}
static QzX qz_x_soma(Qz a, Qz b){
    QzX r;
    r.promovido = qz_prom_soma(a, b);
    r.saturo = qz_prom_sat(r.promovido);
    r.estreito = qz_soma(a, b);
    return r;
}
static QzX qz_x_mult(Qz a, Qz b){
    QzX r;
    r.promovido = qz_prom_mult(a, b);
    r.saturo = qz_prom_sat(r.promovido);
    r.estreito = qz_mult(a, b);
    return r;
}
static int qz_x_divide(Qz a, Qz b, QzX *r){
    Qz i;
    if(!qz_inverso(b, &i)) return 0;
    if(r) *r = qz_x_mult(a, i);
    return 1;
}
static int qz_x_igual(QzX a, QzX b){
    return qz_prom_igual(a.promovido, b.promovido);
}
#endif
