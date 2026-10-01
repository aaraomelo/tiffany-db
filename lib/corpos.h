/* corpos.h — O TOOLKIT DOS CORPOS. A tríade ⊕ ⊗ ∏, uma por corpo.
 *
 * O mapa está em CORPOS_NA_ISA.md, trazido do catálogo: cada corpo tem a MESMA estrutura —
 * uma soma (Clifford), um produto (La Hire) e um operador (Pontryagin). Muda o que são, não
 * quantos são.
 *
 * A REGRA MUDOU, e a antiga estava errada. Eu escrevia: "um corpo só entra quando as três
 * operações estão implementadas e há medidor". Isso é uma LISTA com porteiro — e um porteiro é
 * juízo de valor disfarçado de curadoria. O toolkit não cura: VERIFICA.
 *
 * A ferramenta é `contrato.h`. O cliente declara o corpo que quiser — o dos unicórnios
 * coloridos, o das cores, o que for — nomeia as funções como lhe apetecer, e o verificador
 * devolve as cláusulas que passam. São QUATRO no contrato (chess/elementares/index.tex): uma
 * SOMA, uma MULTIPLICAÇÃO, uma DUALIDADE e um OPERADOR. Eu vinha dizendo três; faltava a dual.
 *
 * O que está abaixo não é a lista dos aprovados: são os que VÊM NA CAIXA, já medidos.
 *
 *   ÁUREO ℤ[φ]   ⊕ componente a componente   ⊗ o gato (a,b)↦(ma+b,a)   ∏ o deslocamento
 *                coroa.c, familia_real.c, normal_circulo.c
 *   RACIONAL ℚ   ⊕ Clifford cruzado          ⊗ La Hire componente      ∏ a classe (reduzir)
 *                racional_pg.c, rastro.c, tudo_ouro.c
 *   MÓRFICO      ⊕ XOR (deflexão D₁)         ⊗ AND (a erosão)          ∏ a adjunção δ⊣ε
 *                morfico.py — 36/36 certificadas
 *   MECÂNICO     ⊕ soma de matrizes          ⊗ produto de matrizes     ∏ a palavra em S,T
 *                mecanica.c, dual_cadeia.c — e a VOLTA: A_m⁻¹ = J·A_{−m}·J, inteira
 *   CRISTALINO   ⊕ componente a componente   ⊗ a borda ω² = tω − 1     ∏ ×ω: o ESQUILO
 *                cristalino.c — det +1, disc < 0, ordem 4 (Gauss) e 6 (Eisenstein)
 *
 * Os que faltam do mapa — fractal, criativo, eletromagnético, motor, telescópico,
 * conforme, entrópico, espaço-temporal, óptico, celeste, econômico, evolutivo, expansivo,
 * somático, geométrico, técnico, rotor, cósmico — estão no CORPOS_NA_ISA.md com a tríade
 * descrita, e entram aqui à medida que forem medidos. Ficam DITOS, não implementados: o
 * toolkit não promete o que não fecha.
 */
#ifndef CORPOS_H
#define CORPOS_H

#include "i128.h"

/* Isto é um cabeçalho de BIBLIOTECA: cada cliente usa as operações de que precisa, e as outras
 * ficam por usar sem que isso seja defeito. O aviso é do compilador a não saber disso. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

/* ---------------- o par que todos partilham: a Word da ISA ---------------- */
typedef struct { long a, b; } Par;

static long c_mdc(long x, long y){ if(x<0)x=-x; if(y<0)y=-y; while(y){ long t=x%y; x=y; y=t; } return x?x:1; }

/* ---------------- ÁUREO ℤ[φ]: a + bσ, com σ² = mσ + 1 ---------------- */
static Par au_soma(Par x, Par y){ Par r = { x.a+y.a, x.b+y.b }; return r; }
static Par au_prod(Par x, Par y, long m){            /* (a+bσ)(c+dσ) com a borda */
    Par r = { x.a*y.a + x.b*y.b, x.a*y.b + x.b*y.a + m*x.b*y.b }; return r; }
static Par au_op(Par x, long m){ Par r = { m*x.a + x.b, x.a }; return r; }   /* ×σ: o gato */
static long au_norma(Par x, long m){ return x.a*x.a + m*x.a*x.b - x.b*x.b; }

/* ---------------- RACIONAL ℚ: (num, den) ---------------- */
static Par ra_classe(Par x){
    if(x.b < 0){ x.a = -x.a; x.b = -x.b; }
    long g = c_mdc(x.a, x.b); x.a /= g; x.b /= g; return x; }
static Par ra_soma(Par x, Par y){ Par r = { x.a*y.b + x.b*y.a, x.b*y.b }; return ra_classe(r); }
static Par ra_prod(Par x, Par y){ Par r = { x.a*y.a, x.b*y.b }; return ra_classe(r); }
static int  ra_cmp(Par x, Par y){                    /* sem divisão: cruzado */
    x = ra_classe(x); y = ra_classe(y);
    long e = x.a*y.b, f = y.a*x.b; return (e>f) - (e<f); }

/* ---------------- MÓRFICO: conjuntos como máscaras de bits ---------------- */
static unsigned mo_soma(unsigned A, unsigned B){ return A ^ B; }   /* Clifford: a deflexão */
static unsigned mo_prod(unsigned A, unsigned B){ return A & B; }   /* La Hire: a EROSÃO   */
static unsigned mo_nao(unsigned A, unsigned topo){ return A ^ topo; }
/* a adjunção: dilatação por B (Minkowski em Z/n) e a erosão que lhe é adjunta */
static unsigned mo_dil(unsigned A, unsigned B, int n){
    unsigned r = 0;
    for(int i = 0; i < n; i++) if(A & (1u<<i))
        for(int j = 0; j < n; j++) if(B & (1u<<j)) r |= 1u << ((i+j) % n);
    return r; }
static unsigned mo_ero(unsigned A, unsigned B, int n){
    unsigned r = 0;
    for(int i = 0; i < n; i++){
        int cabe = 1;
        for(int j = 0; j < n; j++) if((B & (1u<<j)) && !(A & (1u << ((i+j)%n)))) cabe = 0;
        if(cabe) r |= 1u << i;
    }
    return r; }

/* ---------------- MECÂNICO: matrizes 2×2 sobre o par ---------------- */
typedef struct { long a, b, c, d; } Mat;
static Mat me_prod(Mat x, Mat y){
    Mat r = { x.a*y.a + x.b*y.c, x.a*y.b + x.b*y.d,
              x.c*y.a + x.d*y.c, x.c*y.b + x.d*y.d }; return r; }
static long me_det(Mat x){ return x.a*x.d - x.b*x.c; }
static Par me_ap(Mat m, Par v){ Par r = { m.a*v.a + m.b*v.b, m.c*v.a + m.d*v.b }; return r; }
static Mat me_rot(void){ Mat r = {0,-1,1,0}; return r; }          /* o esquilo    */
static Mat me_cis(long k){ Mat r = {1,k,0,1}; return r; }         /* cisalhamento */
static Mat me_gato(long m){ Mat r = {m,1,1,0}; return r; }        /* a cifra      */

/* A VOLTA, e ela é INTEIRA. O gato tem det = −1, logo a inversa não sai do anel — e não é uma
 * segunda máquina: é a ANTÍPODA (m ↦ −m) conjugada pela TROCA. Medido em dual_cadeia.c:
 *
 *     J = [[0,1],[1,0]]        a troca: det −1, J² = I, involução de ordem 2
 *     A_m⁻¹ = J · A_{−m} · J = [[0,1],[1,−m]]
 *
 * Um estica por σ, o outro contrai por 1/σ, e o produto é 1 exato — que é o det = −1 lido de
 * outro jeito. É por isso que a volta não aproxima: ela desfaz. */
static Mat me_troca(void){ Mat r = {0,1,1,0}; return r; }         /* J, a involução */
static Mat me_antigato(long m){ Mat r = {0,1,1,-m}; return r; }   /* A_m⁻¹, inteira  */
static Mat me_inv(Mat x){                                          /* qualquer det ±1 */
    long D = me_det(x);                                            /* adj/det, sem sair de ℤ */
    Mat r = { x.d/D, -x.b/D, -x.c/D, x.a/D }; return r; }

/* ---------------- CRISTALINO ℤ[ω]: a + bω, com ω² = tω − 1 ----------------
 * O lado que FALTAVA. O áureo é o gato: det −1, discriminante m²+4 > 0, hiperbólico, ordem
 * infinita — estica. O cristalino é o ESQUILO: det +1, discriminante t²−4 < 0, elíptico, ordem
 * FINITA — gira e volta. São os dois lados do mesmo par, e o toolkit só tinha um.
 *
 *     t = 0   Gauss ℤ[i]        ω² = −1       N = a² + b²        ω de ordem 4
 *     t = 1   Eisenstein ℤ[ω]   ω² = ω − 1    N = a² + ab + b²   ω de ordem 6 — o Φ₆ do trono
 *
 * A restrição cristalográfica é o preço, e é exato: só passam ordens {1,2,3,4,6}, e o primeiro
 * traço proibido é o áureo. O cristal proíbe exatamente o preenchedor ótimo. */
static Par cr_soma(Par x, Par y){ Par r = { x.a+y.a, x.b+y.b }; return r; }
static Par cr_prod(Par x, Par y, long t){         /* (a+bω)(c+dω) com a borda ω² = tω−1 */
    Par r = { x.a*y.a - x.b*y.b, x.a*y.b + x.b*y.a + t*x.b*y.b }; return r; }
static Par cr_op(Par x, long t){ Par r = { -x.b, x.a + t*x.b }; return r; }   /* ×ω: o esquilo */
static Par cr_conj(Par x, long t){ Par r = { x.a + t*x.b, -x.b }; return r; } /* ω ↦ ω' = t−ω  */
static long cr_norma(Par x, long t){ return x.a*x.a + t*x.a*x.b + x.b*x.b; }
static Mat cr_mat(long t){ Mat r = {0,-1,1,t}; return r; }        /* o esquilo, em matriz */

/* ---------------- AS SETAS DO CATÁLOGO: o que liga um corpo a outro ----------------
 * De chess/elementares/catalogo_isomorfismos.py: "quase todo corpo é o mesmo CORPO-MÃE vestido
 * por uma RÉGUA diferente", e há só QUATRO transformações-tipo — as arestas do grafo:
 *
 *   P  Pontryagin  exp/log      soma ↔ produto        χ(u+v) = χ(u)·χ(v)
 *   W  Wick        t ↦ i·t      euclidiano ↔ lorentz  cos²+sin² ↦ cosh²−sinh²
 *   ν  nu          −rev         corpo ↔ dual          ν∘ν = id
 *   L  Legendre    T → 0        pleno ↔ sombra        max = lim T·log Σe^{·/T}
 *
 * Duas delas são EXATAS em inteiros e entram aqui. W é o sinal do último termo da borda —
 * σ² = mσ + 1 vira ω² = mω − 1 — e é exatamente a seta que leva o GATO ao ESQUILO. ν é a
 * antípoda m ↦ −m, a que já apareceu na inversa. P e L ficam DITAS: P mede-se no corpo finito
 * (gerador.c) e L não é isomorfismo — é limite, e perde o inverso. */
static Mat ar_wick(long m){ Mat r = {m,-1,1,0}; return r; }   /* W(A_m): det +1, disc m²−4 */
static long ar_nu(long m){ return -m; }                        /* ν: a antípoda            */

/* ---- A COMPARAÇÃO DENTRO DO CORPO QUADRÁTICO: e ela DEPENDE DA CLASSE ----
 *
 * Comparar a+bσ com c+dσ não é uma operação só: é duas, e o discriminante decide qual.
 *
 *   Δ > 0  HIPERBÓLICO  σ é REAL, o corpo mergulha em ℝ e é ORDENÁVEL. Compara-se direto, e
 *                       exatamente: 2(x+yσ) = P + y√Δ com P = 2x+ym, e o sinal decide-se em
 *                       inteiros comparando P² com y²Δ. Sem raiz, sem float.
 *   Δ < 0  ELÍPTICO     σ é COMPLEXO. O corpo NÃO é ordenável — não existe ordem compatível
 *                       com as operações. Então compara-se pela NORMA, que é definida positiva
 *                       (cristalino.c §X4) e dá pré-ordem total honesta.
 *   Δ = 0  PARABÓLICO   degenerado: a norma é um quadrado perfeito e não separa.
 *
 * É por isso que o caminho do átomo não podia fazer isto genericamente: a comparação não é a
 * mesma operação nos dois lados. */
static int au_sinal(long x, long y, long m){          /* sinal de x + y·σ, exato em ℤ */
    long D = m*m + 4, P = 2*x + y*m;
    if(y == 0) return (P > 0) - (P < 0);
    if(y > 0 && P >= 0) return (P || y) ? 1 : 0;
    if(y < 0 && P <= 0) return (P || y) ? -1 : 0;
    /* sinais opostos: decide-se comparando os quadrados — e nenhum lado é negativo */
    I128 a2 = i128_smul(P, P);
    I128 b2 = i128_smul_i128(i128_smul(y, y), D);
    if(y > 0) return i128_cmp(b2, a2);
    return i128_cmp(a2, b2);
}
static int au_cmp(Par u, Par v, long m){              /* u < v ? no áureo (Δ>0) */
    return au_sinal(u.a - v.a, u.b - v.b, m); }
static int cr_cmp(Par u, Par v, long t){              /* no cristalino (Δ<0): pela NORMA */
    long nu = cr_norma(u,t), nv = cr_norma(v,t);
    return (nu > nv) - (nu < nv); }

/* ---- A CIFRA DO CONTÍNUO: é a mesma em toda parte, e é a da FAMÍLIA REAL, em ouro ----
 *
 * O Aarão: "a cifra de todo espaço contínuo é a mesma — a cifra da família real, em ouro."
 *
 * E é: a fração contínua. Todo racional tem expansão FINITA [a₀; a₁, …, a_k] (é Euclides), e a
 * expansão É UMA PALAVRA nos metais — o convergente sai do produto A_{a₀}·A_{a₁}···A_{a_k}
 * aplicado a (1,0). A família real são as expansões PERIÓDICAS: σ_m = [m; m, m, …], e o ouro
 * φ = [1;1,1,…] é a unidade — o metal 1, e o gerador da ISA.
 *
 * Então não há uma cifra por corpo: há UMA, e ela serve todo o contínuo. Generaliza sozinha. */
static int cf_cifra(Par x, long *a, int max){          /* x = num/den → [a₀; a₁, …]; devolve k */
    long p = x.a, q = x.b; int k = 0;
    if(q < 0){ p = -p; q = -q; }
    while(q != 0 && k < max){
        long d = p / q, r = p - d*q;
        if(r < 0){ d -= 1; r += q; }                   /* piso, também nos negativos */
        a[k++] = d; p = q; q = r;
    }
    return k;
}
static Par cf_decifra(const long *a, int k){           /* a palavra de volta ao racional */
    long n = 1, d = 0;                                 /* convergente por A_{a_i}, de trás */
    for(int i = k-1; i >= 0; i--){ long t = n; n = a[i]*n + d; d = t; }
    Par r = { n, d }; return r;
}
static Mat cf_palavra(const long *a, int k){           /* a MESMA cifra, em matriz */
    Mat P = {1,0,0,1};
    for(int i = 0; i < k; i++) P = me_prod(P, me_gato(a[i]));
    return P;
}

#pragma GCC diagnostic pop
#endif
