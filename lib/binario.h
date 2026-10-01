/* binario.h — A TORRE BINÁRIA: F_{2w} = F_w + F_w*, de UM BIT até ao BYTE.
 *
 * O Aarão: «constrói os naturais a partir do binário, parte de 𝔽 e daí tem a relação
 * F_n = F_{n-1} + F_{n-1}*; a dualidade aí aplica as leis até chegar em F_8, e F_8 fica ~ ℕ».
 *
 * O `inteiros.tex` duplica ℕ e quocienta (ℕ²/∼ = ℤ). Aqui duplica-se o CORPO e NÃO se
 * quocienta: F_{2w} = F_w ⊕ σF_w com σ² = σ + λ. O mesmo molde dual (Lei 4), um degrau
 * abaixo do chão do outro paper — em vez de descer de ℕ, sobe-se até ele.
 *
 * ── A CONVENÇÃO, ANTES DO PRIMEIRO USO (naturais.tex, e o revisor tinha razão) ──
 * F_w := GF(2^w): o índice conta BITS, nunca elementos. Logo F_1 = 𝔽₂ e |F_8| = 256, e a
 * torre é F_1 → F_2 → F_4 → F_8. Aqui no código o índice do andar é `k` = log2 da largura:
 *
 *      andar k  ↔  F_w  com  w = bn_larg(k) = 2^k,   |F_w| = bn_card(k) = 2^{2^k}
 *
 * k = 0,1,2,3   ↔   F_1, F_2, F_4, F_8   ↔   larguras 1, 2, 4, 8   ↔   2, 4, 16, 256.
 *
 * A largura DOBRA a cada andar: 1 → 2 → 4 → 8 bits, quatro andares, |F| = 2, 4, 16, 256.
 * E o conjunto subjacente do andar k É {0, …, 2^{2^k}−1} pela IDENTIDADE — não por uma
 * codificação escolhida: o bit j do inteiro é a coordenada j na base {1, σ, τ, στ, …}.
 * Por isso F_8 tem por suporte exactamente W_8 = {0,…,255} do `inteiros.tex` §sec:palavra8,
 * e os andares de baixo são os PREFIXOS de W_8 — encaixe medido, não declarado.
 *
 * A RÉGUA NÃO SE ESCREVE À MÃO: bn_ergue() procura o menor λ do andar de baixo que torna
 * σ²+σ+λ irredutível. Nenhuma constante mágica entra por cima (nem o 0x1B do AES, que dá
 * um corpo ISOMORFO sobre o MESMO suporte {0,…,255} — o que ele não tem é a FILTRAÇÃO:
 * ali {0,…,15} não é subcorpo, 2⊗8 = 16 sai. §NB8).
 *
 * A régua λ_k = 2^{2^k−1} = Φ/2 (Φ a potência de Fermat que abre o andar) é a do NIM de
 * Conway: com ela, o produto desta torre É o nim-produto (naturais.tex thm:nim, por indução
 * sobre as duas regras de Conway; medidas em tests/naturais.c §NB10).
 *
 * O que a torre dá: o conjunto e a reversibilidade (soma involutiva, todo não-nulo com
 * inversa). O que ela NÃO dá: o TRANSPORTE — e é ele a diferença entre ⊕ e +. Ver
 * bn_soma_nat: a soma de ℕ é o ponto fixo do par (⊕, ∧), as duas primitivas de 𝔽₂.
 */
#ifndef BINARIO_H
#define BINARIO_H
#include <stdint.h>

/* ── OS ANDARES: k conta as DOBRAS, w = 2^k é a largura em bits ───────────────── */
#define BN_ANDARES 4                              /* k = 0,1,2,3 → 1,2,4,8 bits */
static unsigned bn_larg(int k){ return 1u << k; }             /* 1, 2, 4, 8      */
static unsigned bn_card(int k){ return 1u << (1u << k); }     /* 2, 4, 16, 256   */
static unsigned bn_masc(int k){ return bn_card(k) - 1u; }     /* 1, 3, 15, 255   */

/* λ_k: a régua com que se ergue do andar k para o k+1. DERIVADA por bn_ergue. */
static unsigned bn_lam[BN_ANDARES] = {0,0,0,0};
static int bn_erguido = 0;

/* ── A SOMA é a mesma em TODO andar: o XOR — e é a sua própria inversa ────────── */
static unsigned bn_som(unsigned a, unsigned b){ return a ^ b; }
static unsigned bn_opo(unsigned a){ return a; }               /* −a = a (char 2) */

/* ── O PRODUTO: a dobra. (a0+a1σ)(b0+b1σ) = (a0b0 + λa1b1) + (a0b1+a1b0+a1b1)σ ──
 * Com Karatsuba em 𝔽₂: a0b1+a1b0 = (a0+a1)(b0+b1) + a0b0 + a1b1, logo o alto é
 * c ⊕ p0 (o p1 cancela contra o +a1b1). Três produtos por andar, nenhum widen. */
static unsigned bn_mul(int k, unsigned a, unsigned b){
    if(k == 0) return (a & b) & 1u;                           /* 𝔽₂: o AND       */
    unsigned h = bn_larg(k-1), m = bn_masc(k-1);
    unsigned a0 = a & m, a1 = (a >> h) & m;
    unsigned b0 = b & m, b1 = (b >> h) & m;
    unsigned p0 = bn_mul(k-1, a0, b0);
    unsigned p1 = bn_mul(k-1, a1, b1);
    unsigned c  = bn_mul(k-1, a0 ^ a1, b0 ^ b1);
    unsigned baixo = p0 ^ bn_mul(k-1, p1, bn_lam[k-1]);
    unsigned alto  = c ^ p0;
    return (alto << h) | baixo;
}

/* ── O DUAL: as DUAS raízes de σ²+σ+λ são σ e σ+1, logo (a0+a1σ)* = (a0+a1) + a1σ.
 * É o Frobenius do andar (x* = x^{|F_{k-1}|}) e é involução — Lei 1 e Lei 2. */
static unsigned bn_conj(int k, unsigned x){
    if(k == 0) return x;                                      /* 𝔽₂: o dual degenera */
    unsigned h = bn_larg(k-1), m = bn_masc(k-1);
    unsigned a0 = x & m, a1 = (x >> h) & m;
    return (a1 << h) | (a0 ^ a1);
}
/* traço e norma DESCEM um andar: T(x) = x + x* = a1,  N(x) = x·x* ∈ F_{k-1} */
static unsigned bn_traco(int k, unsigned x){ return bn_som(x, bn_conj(k,x)) >> bn_larg(k-1); }
static unsigned bn_norma(int k, unsigned x){ return bn_mul(k, x, bn_conj(k,x)); }

/* ── O FROBENIUS do corpo todo: φ(x) = x², e φ^w = id no andar de largura w ───── */
static unsigned bn_frob(int k, unsigned x){ return bn_mul(k, x, x); }

/* ── A INVERSA: x⁻¹ = x^{|F|−2} (Fermat), por quadrados e produtos ───────────── */
static unsigned bn_pot(int k, unsigned x, unsigned e){
    unsigned r = 1;
    while(e){ if(e & 1) r = bn_mul(k, r, x); x = bn_mul(k, x, x); e >>= 1; }
    return r;
}
static unsigned bn_inv(int k, unsigned x){ return x ? bn_pot(k, x, bn_card(k) - 2u) : 0u; }

/* ── A RÉGUA DERIVA-SE: σ²+σ+λ é irredutível sobre F_k ⟺ y²+y ≠ λ para todo y.
 * Devolve o MENOR λ que serve, ou 0 se nenhum (não acontece: metade dos λ serve). */
static unsigned bn_ergue(int k){
    unsigned n = bn_card(k);
    for(unsigned lam = 1; lam < n; lam++){
        int raiz = 0;
        for(unsigned y = 0; y < n; y++) if(bn_som(bn_mul(k,y,y), y) == lam){ raiz = 1; break; }
        if(!raiz) return lam;
    }
    return 0;
}
/* ergue a torre inteira, de baixo para cima — cada andar usa o andar já erguido */
static void bn_torre(void){
    for(int k = 0; k + 1 < BN_ANDARES; k++) bn_lam[k] = bn_ergue(k);
    bn_erguido = 1;
}
/* quantas raízes tem σ²+σ+λ em F_k: 0, 1 ou 2 — o TRIAL (Lei 3) */
static int bn_raizes(int k, unsigned lam){
    int r = 0;
    for(unsigned y = 0; y < bn_card(k); y++) if(bn_som(bn_mul(k,y,y), y) == lam) r++;
    return r;
}

/* ══ E AGORA ℕ: o que a torre NÃO tem é o TRANSPORTE ═══════════════════════════
 * a + b = (a ⊕ b) + 2(a ∧ b): a soma de 𝔽₂ e o produto de 𝔽₂ (deslocado) são as duas
 * metades da soma de ℕ. A iteração (a,b) ↦ (a⊕b, 2(a∧b)) converge em ≤ n+1 passos ao par
 * (a+b, 0) — o transporte a ser propagado, com as duas ÚNICAS primitivas de um bit e sem
 * tabela nem memória. (Os pontos fixos da iteração são os pares (x,0).) */
static unsigned long bn_soma_nat(unsigned long a, unsigned long b, int *passos){
    int n = 0;
    while(b){ unsigned long t = (a & b) << 1; a ^= b; b = t; n++; }
    if(passos) *passos = n;
    return a;
}
/* o produto de ℕ pela mesma via: deslocar e somar COM transporte */
static unsigned long bn_mult_nat(unsigned long a, unsigned long b){
    unsigned long r = 0;
    while(b){ if(b & 1) r = bn_soma_nat(r, a, 0); a <<= 1; b >>= 1; }
    return r;
}
/* o produto SEM transporte: o de 𝔽₂[x], sem redução — o carry-less */
static unsigned long bn_clmul(unsigned long a, unsigned long b){
    unsigned long r = 0;
    while(b){ if(b & 1) r ^= a; a <<= 1; b >>= 1; }
    return r;
}
/* o sucessor de Peano escrito só em bits: S(n) = n + 1 pelo mesmo ponto fixo */
static unsigned long bn_S(unsigned long n){ return bn_soma_nat(n, 1, 0); }

/* ── A OUTRA RÉGUA, para o gume: o produto do AES (x⁸+x⁴+x³+x+1, sem torre) ──── */
#define BN_AES 0x1Bu
static unsigned bn_mul_aes(unsigned a, unsigned b){
    unsigned r = 0;
    for(int i = 0; i < 8; i++){
        if(b & 1) r ^= a;
        int alto = a & 0x80;
        a = (a << 1) & 0xFF;
        if(alto) a ^= BN_AES;
        b >>= 1;
    }
    return r;
}
#endif
