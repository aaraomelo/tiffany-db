/* ═══════════════════════════════════════════════════════════════════════════
 * lib/incidencia.h — S, ζ, μ: o deslocamento gera a soma e a inversão
 *
 * Um operador e o trio inteiro sai dele:
 *
 *     S    a subdiagonal de uns          S_ij = [i = j+1]
 *     ζ    a série de S                  ζ = Σ_{j≥0} S^j   (a triangular de uns)
 *     μ    a diferença finita            μ = 1 − S
 *     μζ = ζμ = id                       (aranha thm:shift)
 *
 * A série é FINITA porque S é nilpotente, e o trio vive na face Δ = 0 --- a
 * mesma nilpotência do corpo exterior. É por isso que o inverso sai sem fração.
 *
 * Tudo em inteiros, e sem alocar: o cliente dá o vector e o tamanho.
 * Medido em `tests/incidencia.c` e `tests/pgwire.c` §W153.
 * ═══════════════════════════════════════════════════════════════════════════ */
#ifndef INCIDENCIA_H
#define INCIDENCIA_H

#define IN_MAX 64

/* ── ζ: a acumulação, (ζa)(t) = Σ_{u≤t} a(u) ─────────────────────────────
 * É a convolução pela triangular de uns, feita num passo por índice. */
static void in_zeta(const long *a, long *out, int n){
    long acc = 0;
    for(int i = 0; i < n; i++){ acc += a[i]; out[i] = acc; }
}

/* ── μ: a diferença finita, (μa)(t) = a(t) − a(t−1) ─────────────────────
 * É 1 − S, e desfaz ζ exactamente --- sem divisão, sem resto. */
static void in_mu(const long *a, long *out, int n){
    for(int i = n - 1; i > 0; i--) out[i] = a[i] - a[i-1];
    if(n > 0) out[0] = a[0];
}

/* ── S: o deslocamento, (Sa)(t) = a(t−1) ────────────────────────────────── */
static void in_desloca(const long *a, long *out, int n){
    for(int i = n - 1; i > 0; i--) out[i] = a[i-1];
    if(n > 0) out[0] = 0;
}

/* ── O grau de nilpotência: S^k = 0 exactamente em k = n ─────────────────
 * É ele que faz a série de ζ terminar, e devolve-se para o cliente saber
 * quantos termos a série tem. */
static int in_grau_nilpotente(int n){ return n; }

/* ── A VOLTA, verificada: μ(ζa) = a e ζ(μa) = a ──────────────────────────
 * Devolve 1 se a volta é exacta em todo o vector. Não é decoração: é a lei
 * que garante ao cliente que acumular e diferenciar não perde nada. */
static int in_volta_exacta(const long *a, int n){
    long z[IN_MAX], v[IN_MAX];
    if(n > IN_MAX) return 0;
    in_zeta(a, z, n);  in_mu(z, v, n);
    for(int i = 0; i < n; i++) if(v[i] != a[i]) return 0;
    in_mu(a, z, n);    in_zeta(z, v, n);
    for(int i = 0; i < n; i++) if(v[i] != a[i]) return 0;
    return 1;
}

/* ── E O GUME, que o cliente pode chamar: com o deslocamento CÍCLICO a série
 * não fecha. Devolve 1 se o cíclico volta à identidade em n passos --- e nesse
 * caso 1 − S é singular, pelo que não há inversa. */
static int in_ciclico_nao_fecha(int n){
    /* S_cic^n = I: a soma de cada linha de 1 − S_cic é zero, logo é singular */
    (void)n;
    return 1;
}


/* ═══════════════════════════════════════════════════════════════════════════
 * A MESMA ÁLGEBRA, LIDA AO LONGO DA ORDEM — para o motor.
 *
 * O que está acima é o trio como OPERADOR: S a subdiagonal, ζ = ΣS^j, μ = 1−S,
 * e a nilpotência a garantir que a série é finita. O que está abaixo é o mesmo
 * trio lido como PERCURSO, que é a forma de que o `banco/sql.c` precisa: o
 * `count` do `GROUP BY` é a convolução com ζ (`thm:zetamu`: G_t(x) = (a_x*ζ)(t)),
 * e a volta é a diferença finita.
 *
 *     in_zeta / in_mu     o operador, sobre `int n`   (o de cima)
 *     inc_zeta / inc_mu   o percurso,  sobre `long n` (o de baixo)
 *
 * NÃO SÃO DUAS ÁLGEBRAS: são a mesma, e é por isso que vivem no mesmo ficheiro.
 * O `tests/escada.c` mede a leitura de operador; o `tests/zetamu.c` mede a de
 * percurso e o par a fechar (ζ∘μ = μ∘ζ = id, resíduo 0). O prefixo distingue a
 * leitura, e o guarda de inclusão é um só.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ── ζ: acumula o prefixo. b(t) = Σ_{u≤t} a(u). A convolução com ζ na cadeia. */
static void inc_zeta(const long *a, long *b, long n){
    long s = 0;
    for(long t = 0; t < n; t++){ s += a[t]; b[t] = s; }
}

/* ── μ: a diferença finita. a(t) = b(t) − b(t−1), com a(0) = b(0). ζ⁻¹, exacto. */
static void inc_mu(const long *b, long *a, long n){
    long ant = 0;
    for(long t = 0; t < n; t++){ a[t] = b[t] - ant; ant = b[t]; }
}

/* ── o núcleo μ(u,t) da ordem total, tal como o thm:mu o dá ────────────────────
 * É a definição pontual, para quem quiser a matriz em vez do operador: vale 1 na
 * diagonal, −1 logo abaixo, e zero em tudo o resto. `inc_mu` é isto aplicado —
 * e é a mesma subdiagonal S do trio lá de cima, com o sinal da cláusula. */
static long inc_mu_nucleo(long u, long t){
    if(u == t)     return  1;
    if(u == t - 1) return -1;
    return 0;
}

/* ── O G COMO ζ, E A SUA VOLTA ─────────────────────────────────────────────────
 * Dada a realização π (π[t] = o valor visitado no passo t) e um alvo x, o
 * indicador é a_x[t] = [π[t] = x], e o campo acumulado é G_t(x) = (a_x * ζ)(t):
 * quantas vezes x foi visto até t. É o G do `thm:mult`, lido ao longo da ordem.
 *
 * `inc_G` enche a família acumulada {G_t(x)}; `inc_mu` sobre ela devolve a_x —
 * a trajetória de x, recuperada da contagem sem se ter guardado a trajetória. */
static void inc_indicador(const long *pi, long n, long x, long *a){
    for(long t = 0; t < n; t++) a[t] = (pi[t] == x);
}
static void inc_G(const long *pi, long n, long x, long *G){
    long s = 0;
    for(long t = 0; t < n; t++){ if(pi[t] == x) s++; G[t] = s; }
}

/* ── O LEVANTAMENTO NA FIBRA (thm:zetamu (3)) ─────────────────────────────────
 * k(i) = G_i(π(i)) é a contagem acumulada da PRÓPRIA célula ao longo da fibra:
 * no r-ésimo encontro vale r. A sua diferença finita vale 1 em cada visita —
 * é o G̃ = 1 do levantamento, dito na linguagem da incidência. */
static void inc_levanta(const long *pi, long n, long *k){
    long cont[256];
    for(int v = 0; v < 256; v++) cont[v] = 0;
    for(long t = 0; t < n; t++){
        long v = pi[t] & 255;
        k[t] = ++cont[v];
    }
}

#endif /* INCIDENCIA_H */
