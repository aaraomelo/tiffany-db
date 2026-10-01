/* inteiros.h — ℤ: E O QUE ELES ACRESCENTAM É A REVERSIBILIDADE.
 *
 * «Naturais eram o primeiro relógio; os inteiros acrescentam a REVERSIBILIDADE, porque
 * agora todo a tem uma folha (−a) que retorna ao zero. E depois divisibilidade/MDC/
 * Bézout transformam essa reversibilidade numa máquina de cortes.» (ordem do coordenador)
 *
 * É a linguagem desta casa dita de outro lado: o oposto é a VOLTA da soma, e `a+(−a)=0`
 * é a Lei 1 no grupo aditivo. Daí a construção por PARES — (a,b) ~ a−b, com
 * (a,b)~(c,d) ⟺ a+d = b+c —, que é o par dual a nascer: cada inteiro é uma CLASSE, e a
 * operação tem de estar BEM DEFINIDA, isto é, não pode depender do representante.
 *
 * E a regra do ficheiro para o «menos vezes menos»: «Não vale simplesmente dizer que dá
 * mais. PROVE.» Então prova-se, e a prova é uma cadeia de leis nomeadas — não uma tabela.
 */
#ifndef INTEIROS_H
#define INTEIROS_H

/* ── a construção por pares: (a,b) ~ a − b ─────────────────────────────────────── */
static int iz_equiv(long a, long b, long c, long d){ return a + d == b + c; }
static long iz_val(long a, long b){ return a - b; }          /* o representante */

/* ── o oposto: a folha que volta ao zero ───────────────────────────────────────── */
static long iz_oposto(long z){ return -z; }
static int iz_volta_zero(long z){ return z + iz_oposto(z) == 0; }

/* ── o módulo: a distância ao zero ─────────────────────────────────────────────── */
static long iz_mod(long a){ return a < 0 ? -a : a; }

/* ── a divisibilidade em ℤ: ∃k ∈ ℤ com b = ak ─────────────────────────────────── */
static int iz_div(long a, long b, long *k){
    if(a == 0) return b == 0;
    if(b % a) return 0;
    if(k) *k = b / a;
    return 1;
}
/* ── o gcd em ℤ (sempre ≥ 0) e o Bézout, com a testemunha ─────────────────────── */
static long iz_gcd(long a, long b, long *x, long *y){
    long x0 = 1, y0 = 0, x1 = 0, y1 = 1, sa = a < 0 ? -1 : 1, sb = b < 0 ? -1 : 1;
    long A = a < 0 ? -a : a, B = b < 0 ? -b : b;
    while(B){
        long q = A / B, t;
        t = A - q*B; A = B; B = t;
        t = x0 - q*x1; x0 = x1; x1 = t;
        t = y0 - q*y1; y0 = y1; y1 = t;
    }
    if(x) *x = x0 * sa;                     /* os sinais voltam ao lugar */
    if(y) *y = y0 * sb;
    return A;
}
/* ── a DIOFANTINA ax + by = c: tem solução ⟺ gcd(a,b) | c, e a solução EXIBE-SE ── */
static int iz_diofantina(long a, long b, long c, long *x, long *y){
    long x0, y0, g = iz_gcd(a, b, &x0, &y0);
    if(g == 0) return c == 0;
    if(c % g) return 0;                     /* o critério, e é ele que decide */
    long f = c / g;
    if(x) *x = x0 * f;
    if(y) *y = y0 * f;
    return 1;
}
/* ── a CONGRUÊNCIA: a ≡ b (mod n) ⟺ n | (a−b) ─────────────────────────────────── */
static int iz_cong(long a, long b, long n){ return n && (a - b) % n == 0; }
/* a potência modular pela DOBRA — «faça sem calcular 37^4 diretamente» */
static long iz_pot_mod(long b, long e, long n){
    long r = 1 % n;
    b %= n; if(b < 0) b += n;
    while(e > 0){
        if(e & 1) r = (r * b) % n;
        b = (b * b) % n;                    /* a dobra: o expoente parte-se ao meio */
        e >>= 1;
    }
    return (r % n + n) % n;
}
#endif
