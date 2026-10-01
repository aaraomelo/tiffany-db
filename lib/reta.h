/* reta.h — AS OPERAÇÕES DA RECTA GEOMÉTRICA, TODAS INTEIRAS. Uma casa para elas.
 *
 * O Aarão: «vamos centralizar as operações matemáticas; cria essa biblioteca de funções
 * matemáticas da recta geométrica, tudo inteiro — deve facilitar essa centralização.»
 *
 * E o levantamento diz porquê. No repositório há 237 nomes de função definidos em mais
 * de um ficheiro, e os campeões são exactamente as operações desta recta:
 *
 *      28×  mdc          20×  mul          12×  norma        11×  pot
 *      26×  ok           19×  q            12×  mmc           9×  primo
 *
 * Vinte e oito cópias do máximo divisor comum — e três delas dentro da própria `lib/`,
 * com nomes diferentes: `al_mdc` (algebra.h), `iz_gcd` (inteiros.h) e `pl_mdc` (poli.h).
 * Não é desleixo de quem escreveu: é o que acontece quando não há um sítio.
 *
 * ── O QUE ESTE CABEÇALHO É, E O QUE NÃO É ────────────────────────────────────────────
 *
 * NÃO é a 238.ª cópia. Onde já existe uma canónica, ela é INCLUÍDA e apontada — e o que
 * fica aqui é só o que não tinha casa, mais o mapa de quem é quem. Reescrever o que já
 * está medido seria trocar uma duplicação por duas.
 *
 * ── O MAPA: onde vive cada operação, e qual é a canónica ─────────────────────────────
 *
 *   mdc, mmc, Bézout           `inteiros.h`  iz_gcd, iz_diofantina        ← A CANÓNICA
 *   racionais p/q              `racionais.h` qz, qz_soma, qz_mult, qz_menor
 *   polinómios em ℤ[x]         `poli.h`      pz_mul (convolução), pz_div_exata,
 *                                            pol_sturm_reais, pz_beta_pisot
 *   matrizes, posto, núcleo    `linear.h`    mat_posto, mat_nucleo, mat_inversa
 *   a régua e os convergentes  `aritmetica.h` nt_fc, nt_convergentes
 *   o corpo dual σ, σ†         `corpos.h`, `dual32.h`
 *
 * E o que ESTA casa acrescenta, porque não estava em lado nenhum e nasceu medido nesta
 * migração — cada uma com o medidor onde foi provada:
 *
 *   rt_det_bareiss   o determinante EXACTO de ordem qualquer, sem divisão inexacta
 *   rt_det_mod       o mesmo em 𝔽ₚ, para quando os intermédios não cabem
 *   rt_inv_mod       o inverso em 𝔽ₚ por Fermat (7 cópias no repo antes disto)
 *   rt_ipow          a potência inteira (5 cópias antes disto)
 *   rt_orbita        a órbita de ∞ — os convergentes, [p:q] ⟼ [m·p+q : p]
 *   rt_reverte       a REVERSÃO dos coeficientes: o dual do polinómio
 *   rt_hurwitz_est   o regime pelos coeficientes: Routh–Hurwitz, sem calcular raiz
 *
 * ── A REGRA DA CASA, e ela vale aqui como em todo o lado ─────────────────────────────
 *
 * Tudo inteiro. Onde aparece uma divisão, ou ela é EXACTA (e prova-se) ou o resultado é
 * um par (numerador, denominador) e a comparação faz-se por produto cruzado. Não há
 * limiar nenhum neste ficheiro: «é zero» é uma pergunta sobre o número, e «é menor que
 * a régua que escolhi» é uma pergunta sobre a régua.
 */
#ifndef RETA_H
#define RETA_H

#include <stdint.h>
#include "inteiros.h"      /* iz_gcd, iz_diofantina — a canónica do mdc */

#ifndef RT_MAX
#define RT_MAX 64          /* a ordem máxima das matrizes aqui — e ela VERIFICA-SE */
#endif

/* ═══════════════════════════════════════════════════════════════════════════════════
 * A OPERAÇÃO — e tudo o que se segue sai dela.
 *
 * Há UMA operação, e escreve-se numa linha: a ⋆ b. O que dela sai são duas LEITURAS,
 * separadas por trocar a ordem — e o que as separa é o espelho τ(a,b) = (b,a):
 *
 *      Dir(a,b)  = ½(a⋆b + b⋆a)      a leitura que NÃO muda        o directo
 *      Cruz(a,b) = ½(a⋆b − b⋆a)      a leitura que TROCA DE SINAL  o cruzado
 *
 * Que sejam DUAS é dedução e não escolha: τ² = id obriga o polinómio mínimo a dividir
 * (t−1)(t+1), de raízes distintas — logo τ é diagonalizável com espectro {+1,−1}, e
 * Dir e Cruz são os PROJECTORES nos dois espaços próprios, (id ± τ)/2. Em característica
 * 2 os dois colapsam, e é a única hipótese que isto usa.
 *
 * E DAQUI SAI O RESTO DESTE CABEÇALHO, que é a razão de estar no topo:
 *
 *      o produto interno       é  Dir              a métrica, a diagonal
 *      a norma                 é  Dir(a,a)
 *      o produto vectorial     é  Cruz em ℝ³       a área, fora da diagonal
 *      o determinante 2×2      é  Cruz             a mesma área, lida na matriz
 *      o simétrico/antissim.   é  Dir/Cruz de matrizes
 *      o SINAL                 é  a mesma dobra em ℤ, com o mesmo espectro {+1,−1}
 *
 * Não são seis operações que por acaso se parecem: são leituras de uma só, e é por isso
 * que vivem no mesmo sítio. Tudo em DOBRO, para não dividir por dois — o factor 2 é o
 * preço de não ter vírgula, e paga-se uma vez.
 * ═══════════════════════════════════════════════════════════════════════════════════ */

/* as duas leituras, EM DOBRO: 2·Dir e 2·Cruz de dois vectores de dimensão n. O directo
 * é um escalar (a métrica); o cruzado é a matriz antissimétrica a_i b_j − a_j b_i. */
static long rt_dir(const long *a, const long *b, int n){
    long s = 0;
    for(int i = 0; i < n; i++) s += a[i]*b[i];
    return s;                                    /* Dir(a,b) = ⟨a,b⟩ */
}
static void rt_cruz(const long *a, const long *b, int n, long *C){
    for(int i = 0; i < n; i++) for(int j = 0; j < n; j++)
        C[i*n + j] = a[i]*b[j] - a[j]*b[i];      /* Cruz(a,b) = a∧b */
}
/* e a norma é o directo consigo próprio — não é uma operação nova */
static long rt_norma(const long *a, int n){ return rt_dir(a, a, n); }

/* Fase B/D: Dir e Cruz em int32 — acumulação int64; long mantém vectores grandes */
static int32_t rt_dir_i32(const int32_t *a, const int32_t *b, int n){
    int64_t s = 0;
    for(int i = 0; i < n; i++) s += (int64_t)a[i]*b[i];
    return (int32_t)s;
}
static void rt_cruz_i32(const int32_t *a, const int32_t *b, int n, int32_t *C){
    for(int i = 0; i < n; i++) for(int j = 0; j < n; j++)
        C[i*n + j] = (int32_t)((int64_t)a[i]*b[j] - (int64_t)a[j]*b[i]);
}
static int32_t rt_norma_i32(const int32_t *a, int n){ return rt_dir_i32(a, a, n); }

/* ── A POTÊNCIA INTEIRA ──────────────────────────────────────────────────────────────
 * Cinco cópias no repositório antes disto. Sem vírgula e sem `pow`. */
static int32_t rt_ipow(int32_t base, int e){
    int32_t r = 1;
    while(e-- > 0) r *= base;
    return r;
}

/* ── O INVERSO EM 𝔽ₚ, POR FERMAT ────────────────────────────────────────────────────
 * a^(p−2) mod p. Sete cópias antes disto. Nenhuma divisão.
 * Mantém `long`: P pode ser 2³¹−1 (estado_caos, milenio_trial). */
static long rt_inv_mod(long a, long p){
    long r = 1, e = p - 2;
    a = ((a % p) + p) % p;
    while(e){ if(e & 1) r = r*a % p; a = a*a % p; e >>= 1; }
    return r;
}

/* ── O DETERMINANTE EXACTO, DE ORDEM QUALQUER — BAREISS ──────────────────────────────
 * A eliminação de Gauss divide, e por isso pede vírgula e um limiar para o pivô. A de
 * Bareiss divide pelo pivô ANTERIOR, e essa divisão é exacta em ℤ — é o teorema. Fica
 * tudo inteiro, e o limiar do pivô desaparece: `== 0` é a pergunta certa.
 *
 * O CUIDADO, que custou uma asserção falhada no `matricial.c` §M11: os intermédios de
 * Bareiss são MENORES da matriz, e crescem muito mais que o resultado. Numa 5×5 com
 * entradas da ordem de 10^10 chegam a 10^42 e o `long` enrola calado — o determinante
 * final cabia (10^14) e saía errado à mesma. Quando as entradas forem grandes, usar
 * `rt_det_mod` em vários primos.
 *
 * A matriz é dada em G com passo `passo`, e é CONSUMIDA. */
/* O RESTO DA DIVISÃO DE BAREISS vigia-se aqui, e não em cada chamador. A divisão pelo
 * pivô anterior é EXACTA por teorema — os intermédios são menores da matriz —, logo um
 * resto não nulo não é imprecisão: é defeito, e conta-se. O `geral.c` tinha esta vigia
 * escrita à mão dentro do seu laço; agora quem chama a lib herda-a de graça. */
static long rt_bareiss_resto = 0;

static long rt_det_bareiss(long *G, int n, int passo){
    long ant = 1;
    int sinal = 1;
    for(int k = 0; k < n - 1; k++){
        if(G[k*passo + k] == 0){                       /* troca por uma linha com pivô */
            int t = -1;
            for(int i = k + 1; i < n; i++) if(G[i*passo + k]){ t = i; break; }
            if(t < 0) return 0;                        /* coluna nula: o det É zero */
            for(int j = 0; j < n; j++){
                long v = G[k*passo + j];
                G[k*passo + j] = G[t*passo + j];
                G[t*passo + j] = v;
            }
            sinal = -sinal;
        }
        for(int i = k + 1; i < n; i++)
            for(int j = k + 1; j < n; j++)
            {
                long num = G[i*passo + j]*G[k*passo + k] - G[i*passo + k]*G[k*passo + j];
                if(num % ant != 0) rt_bareiss_resto++;      /* não pode acontecer */
                G[i*passo + j] = num / ant;
            }
        ant = G[k*passo + k];
    }
    return sinal*G[(n-1)*passo + (n-1)];
}

/* ── O MESMO DETERMINANTE EM 𝔽ₚ ──────────────────────────────────────────────────────
 * Para quando os intermédios de Bareiss não cabem: em 𝔽ₚ nada cresce. */
static long rt_det_mod(long *G, int n, int passo, long p){
    long d = 1;
    for(int k = 0; k < n; k++){
        int piv = -1;
        for(int i = k; i < n; i++) if(((G[i*passo+k] % p) + p) % p){ piv = i; break; }
        if(piv < 0) return 0;
        if(piv != k){
            for(int j = 0; j < n; j++){
                long t = G[k*passo+j]; G[k*passo+j] = G[piv*passo+j]; G[piv*passo+j] = t;
            }
            d = (p - d) % p;
        }
        long a = ((G[k*passo+k] % p) + p) % p;
        d = d*a % p;
        long iv = rt_inv_mod(a, p);
        for(int i = k+1; i < n; i++){
            long f = ((G[i*passo+k] % p) + p) % p * iv % p;
            if(!f) continue;
            for(int j = k; j < n; j++)
                G[i*passo+j] = (((G[i*passo+j] - f*G[k*passo+j]) % p) + p) % p;
        }
    }
    return d;
}

/* ── A ÓRBITA DE ∞: OS CONVERGENTES ──────────────────────────────────────────────────
 * O paper (prop:orbita) diz que a acção é a matriz vezes o vector, «sem uma divisão»:
 *
 *      [p:q] ⟼ [m·p + q : p],   a partir de [1:0] = ∞
 *
 * e o que sai é exactamente a recorrência dos convergentes, com |det| = 1 em todos os
 * passos. É assim que o ponto fixo se MOSTRA — porque ele não cabe no andar (é isso o
 * corte), e um decimal truncado não é o objecto: é outro número. */
static void rt_orbita(long m, int k, long *p, long *q){
    long P = 1, Q = 0;                       /* ∞ = [1:0], pela Lei 0 */
    for(int t = 0; t < k; t++){ long np = m*P + Q; Q = P; P = np; }
    *p = P; *q = Q;
}

/* Fase B: órbita/dobra/forma/reversão em int32 — convergentes no envelope medido */
static int rt_orbita_i32(int32_t m, int k, int32_t *p, int32_t *q){
    int64_t P = 1, Q = 0;
    for(int t = 0; t < k; t++){ int64_t np = (int64_t)m*P + Q; Q = P; P = np; }
    if(P != (int32_t)P || Q != (int32_t)Q) return 0;
    *p = (int32_t)P; *q = (int32_t)Q;
    return 1;
}
static void rt_dobra_i32(int32_t m, int32_t *p, int32_t *q){
    int32_t np = *q, nq = *p - m * (*q);
    *p = np; *q = nq;
}
static int32_t rt_forma_i32(int32_t p, int32_t q, int32_t m){
    return (int32_t)((int64_t)p*p - (int64_t)m*p*q - (int64_t)q*q);
}
static int rt_no_cone_i32(int32_t m, int32_t p, int32_t q){
    return q > 0 && p > m*q && p < (m+1)*q;
}
static void rt_reverte_i32(const int32_t *a, int n, int32_t *r){
    for(int k = 0; k <= n; k++) r[k] = a[n-k];
}

/* ── O TEOREMA DO OPERADOR, realizado: a DOBRA e o ACUMULADOR ────────────────────────
 *
 * `thm:operador` do universal.tex: a indução, a meta-indução e o acumulador não são três
 * peças — são o operador, o seu espelho, e o rasto que eles deixam.
 *
 *   INDUÇÃO       [p:q] ⟼ [q : p+mq]     é a rt_orbita acima      (λ⁺, sobe)
 *   META-INDUÇÃO  [p:q] ⟼ [q : p−mq]     é a DOBRA, aqui          (λ⁻, lê)
 *   ACUMULADOR    a PALAVRA                é a RtCf                (o rasto)
 *
 * A dobra troca o SINAL da forma F(p,q) = p² − mpq − q², e aplicada duas vezes devolve-o:
 * é a involução de espectro {+1,−1} do espelho, a mesma que reparte a operação em Dir e
 * Cruz. Devolve o novo par em (*p, *q). */
static void rt_dobra(long m, long *p, long *q){
    long np = *q, nq = *p - m * (*q);
    *p = np; *q = nq;
}

/* e o que a dobra faz à forma, dito como predicado: F(dobra(p,q)) = −F(p,q).
 * Devolve 1 se a identidade vale — e ela vale sempre, em ℤ, por álgebra. */
static long rt_forma(long p, long q, long m);            /* declarada abaixo */
static int rt_dobra_inverte(long m, long p, long q){
    long F = rt_forma(p, q, m), P = p, Q = q;
    rt_dobra(m, &P, &Q);
    return rt_forma(P, Q, m) == -F;
}
static int rt_dobra_inverte_i32(int32_t m, int32_t p, int32_t q){
    int32_t F = rt_forma_i32(p, q, m), P = p, Q = q;
    rt_dobra_i32(m, &P, &Q);
    return rt_forma_i32(P, Q, m) == -F;
}

/* O CONE onde a dobra ENCOLHE o denominador: 0 < p − m·q < q, isto é m·q < p < (m+1)·q.
 * É o cone dos DOIS lados — sem o tecto, o encolhimento não é promessa nenhuma, e foi
 * assim que a asserção do §R12b caiu à primeira. */
static int rt_no_cone(long m, long p, long q){ return q > 0 && p > m*q && p < (m+1)*q; }

/* ── A REVERSÃO DOS COEFICIENTES: O DUAL DO POLINÓMIO ────────────────────────────────
 * p*(x) = xⁿ·p(1/x) — os coeficientes ao contrário. É ela que troca DENTRO por FORA no
 * disco, e é por isso que contar UM zero basta em vez de contar n−1 (Rouché no dual,
 * `matricial.c` §M17). E é ela que faz do RECÍPROCO do ouro a raiz do polinómio
 * revertido, que é a autodualidade do `solar.c`. */
static void rt_reverte(const long *a, int n, long *r){
    for(int k = 0; k <= n; k++) r[k] = a[n-k];
}

/* ── O REGIME PELOS COEFICIENTES: ROUTH–HURWITZ ──────────────────────────────────────
 * Todas as raízes com Re < 0 ⟺ os menores principais da matriz de Hurwitz são todos
 * positivos. Sai dos COEFICIENTES, e não precisa de calcular raiz nenhuma.
 *
 * O QUE ELE NÃO DECIDE, e diz-se: com todos os menores nulos, ele não distingue a BORDA
 * (raízes no eixo imaginário) do CAOS. Para isso é preciso o segundo passo — procurar um
 * factor x² + c com c > 0 por divisão exacta —, e é o `geral.c` §G4 que os junta.
 *
 * a[] vai do maior grau ao menor, com n = grau. Devolve 1 se estável. */
static int rt_hurwitz_est(const long *a, int n){
    for(int m = 1; m <= n; m++){
        long H[RT_MAX*RT_MAX];
        for(int i = 0; i < m; i++) for(int j = 0; j < m; j++){
            int idx = n - 2*(i+1) + (j+1);
            H[i*m + j] = (idx >= 0 && idx <= n) ? a[n-idx] : 0;
        }
        if(rt_det_bareiss(H, m, m) <= 0) return 0;
    }
    return 1;
}

/* ── O TECTO VERIFICA-SE, e não se documenta ─────────────────────────────────────────
 * Um `#define` que ninguém testa é documentação, não limite — e já pôs esta casa a não
 * terminar uma vez. Quem usa `rt_det_bareiss` com n > RT_MAX escreve fora do array; esta
 * função diz onde está a fronteira, e o medidor que a chama prova que ela é respeitada. */
static int rt_cabe(int n){ return n > 0 && n <= RT_MAX; }

/* ═══════════════════════════════════════════════════════════════════════════════════
 * AS TÉCNICAS DE ALTO NÍVEL — as que os ficheiros repetem inline, e quantas vezes.
 *
 * O levantamento por PADRÃO DE CÓDIGO (e não por nome de função, que já estava feito)
 * diz onde a casa se repete a si própria:
 *
 *      40×  a recorrência  u = m·u₁ + u₀        ← a régua, e é a mais copiada de todas
 *      14×  a convolução   c[i+j] += a[i]·b[j]
 *      14×  o cruzado      a[1]b[2] − a[2]b[1]
 *      12×  a exponenciação modular
 *      10×  a eliminação com pivô
 *       6×  a forma        p² − m·p·q − q²
 *       5×  a companheira  C[i][i−1] = 1
 *       4×  o traço da potência  Tr(Cᵏ)
 *
 * Estas são as PEÇAS da recta, não utilitários: a recorrência é a régua, a convolução é
 * o produto de polinómios, o cruzado é a metade que inverte, a forma é o que o ponto
 * fixo anularia. Cada uma escrita uma vez, aqui, e medida em `tests/reta.c`.
 * ═══════════════════════════════════════════════════════════════════════════════════ */

/* ── A RECORRÊNCIA: a régua, e a peça mais copiada do repositório ────────────────────
 * u_{k+1} = m·u_k + u_{k−1}, que é a acção da companheira A_m = [[m,1],[1,0]] e é a
 * mesma coisa que a órbita de ∞ (rt_orbita) vista do lado da sucessão. Preenche `u` com
 * n termos a partir de (u0, u1). */
static void rt_recorre(long m, long u0, long u1, long *u, int n){
    if(n > 0) u[0] = u0;
    if(n > 1) u[1] = u1;
    for(int k = 2; k < n; k++) u[k] = m*u[k-1] + u[k-2];
}

/* ── A CONVOLUÇÃO: multiplicar polinómios É convolver os coeficientes ────────────────
 * «a forma aditiva da multiplicação vista pela Transformada Universal». r tem de ter
 * espaço para na+nb−1 coeficientes, e é zerado aqui. */
static void rt_conv(const long *a, int na, const long *b, int nb, long *r){
    for(int k = 0; k < na + nb - 1; k++) r[k] = 0;
    for(int i = 0; i < na; i++) for(int j = 0; j < nb; j++) r[i+j] += a[i]*b[j];
}

/* ── O CRUZADO EM ℝ³: um CASO de rt_cruz, não uma operação nova ──────────────────────
 * Em ℝ³ a matriz antissimétrica Cruz(a,b) tem três entradas independentes, e o vector
 * (c₀,c₁,c₂) é a leitura delas — é o mesmo objecto com outra roupa, e é por isso que o
 * produto vectorial só existe em três dimensões: é onde n(n−1)/2 = n. */
static void rt_cruz3(const long *a, const long *b, long *c){
    c[0] = a[1]*b[2] - a[2]*b[1];
    c[1] = a[2]*b[0] - a[0]*b[2];
    c[2] = a[0]*b[1] - a[1]*b[0];
}
static void rt_cruz3_i32(const int32_t *a, const int32_t *b, int32_t *c){
    c[0] = (int32_t)((int64_t)a[1]*b[2] - (int64_t)a[2]*b[1]);
    c[1] = (int32_t)((int64_t)a[2]*b[0] - (int64_t)a[0]*b[2]);
    c[2] = (int32_t)((int64_t)a[0]*b[1] - (int64_t)a[1]*b[0]);
}

/* ── A MESMA DECOMPOSIÇÃO, AGORA EM MATRIZES ─────────────────────────────────────────
 * Dir e Cruz aplicados a uma matriz em vez de a um par de vectores — o espelho é o
 * mesmo, τ(M) = Mᵀ, e os projectores são os mesmos (id ± τ)/2. Devolvidas
 * EM DOBRO, para não dividir: 2S = M + Mᵀ e 2A = M − Mᵀ. A decomposição existe quando 2
 * é invertível — em característica 2 as duas colapsam (medido em operacao.c §O10). */
static void rt_dir_cruz(const long *M, int n, long *S2, long *A2){
    for(int i = 0; i < n; i++) for(int j = 0; j < n; j++){
        S2[i*n+j] = M[i*n+j] + M[j*n+i];
        A2[i*n+j] = M[i*n+j] - M[j*n+i];
    }
}
static void rt_dir_cruz_i32(const int32_t *M, int n, int32_t *S2, int32_t *A2){
    for(int i = 0; i < n; i++) for(int j = 0; j < n; j++){
        S2[i*n+j] = M[i*n+j] + M[j*n+i];
        A2[i*n+j] = M[i*n+j] - M[j*n+i];
    }
}

/* ── A EXPONENCIAÇÃO MODULAR ─────────────────────────────────────────────────────────
 * a^e mod p, por quadrados. Doze cópias no repositório antes disto. */
static long rt_pot_mod(long a, long e, long p){
    long r = 1;
    a = ((a % p) + p) % p;
    while(e > 0){ if(e & 1) r = r*a % p; a = a*a % p; e >>= 1; }
    return r;
}

/* ── A COMPANHEIRA, E O TRAÇO DAS SUAS POTÊNCIAS ─────────────────────────────────────
 * A companheira de xⁿ = c₁xⁿ⁻¹ + … + cₙ, com a acção nas LINHAS: v_{k+1} = v_k·C. E o
 * traço de Cᵏ É a soma das potências das raízes — o Tr(σᵏ) de Newton, sem avaliar raiz
 * nenhuma. Escrevi-a transposta à primeira, e a asserção deu 91 de 160: o índice sobe
 * na COLUNA, e não na linha. */
static void rt_companheira(const long *c, int n, long *C){
    for(int i = 0; i < n*n; i++) C[i] = 0;
    for(int i = 0; i + 1 < n; i++) C[i*n + (i+1)] = 1;
    for(int j = 0; j < n; j++) C[(n-1)*n + j] = c[j];
}
static void rt_tracos(const long *C, int n, long *tr, int k){
    long P[RT_MAX*RT_MAX], N[RT_MAX*RT_MAX];
    for(int i = 0; i < n*n; i++) P[i] = 0;
    for(int i = 0; i < n; i++) P[i*n+i] = 1;
    for(int t = 0; t < k; t++){
        long s = 0;
        for(int i = 0; i < n; i++) s += P[i*n+i];
        tr[t] = s;
        for(int i = 0; i < n*n; i++) N[i] = 0;
        for(int i = 0; i < n; i++) for(int j = 0; j < n; j++)
            for(int l = 0; l < n; l++) N[i*n+j] += P[i*n+l]*C[l*n+j];
        for(int i = 0; i < n*n; i++) P[i] = N[i];
    }
}

/* Fase B: companheira/tracos com coeficientes int32 — acumulação int64 nos traços */
static void rt_companheira_i32(const int32_t *c, int n, int32_t *C){
    for(int i = 0; i < n*n; i++) C[i] = 0;
    for(int i = 0; i + 1 < n; i++) C[i*n + (i+1)] = 1;
    for(int j = 0; j < n; j++) C[(n-1)*n + j] = c[j];
}
static void rt_tracos_i32(const int32_t *C, int n, int64_t *tr, int k){
    int64_t P[RT_MAX*RT_MAX], N[RT_MAX*RT_MAX];
    for(int i = 0; i < n*n; i++) P[i] = 0;
    for(int i = 0; i < n; i++) P[i*n+i] = 1;
    for(int t = 0; t < k; t++){
        int64_t s = 0;
        for(int i = 0; i < n; i++) s += P[i*n+i];
        tr[t] = s;
        for(int i = 0; i < n*n; i++) N[i] = 0;
        for(int i = 0; i < n; i++) for(int j = 0; j < n; j++)
            for(int l = 0; l < n; l++) N[i*n+j] += P[i*n+l]*C[l*n+j];
        for(int i = 0; i < n*n; i++) P[i] = N[i];
    }
}

/* ── A FORMA QUE O PONTO FIXO ANULARIA ───────────────────────────────────────────────
 * p² − m·p·q − q². Nos convergentes ela vale ±1 e NUNCA zero, e é isso o corte: o ponto
 * fixo pediria a forma = 0, e a descida mostra que não tem solução em ℤ. Seis cópias. */
static long rt_forma(long p, long q, long m){ return p*p - m*p*q - q*q; }

/* ── O PRODUTO DE KRONECKER ──────────────────────────────────────────────────────────
 * A operação ⊗ do corpo de corpos: (A⊗B) tem ordem a·b, e det(A⊗B) = det(A)^b·det(B)^a.
 * K tem de ter espaço para (a·b)², e o passo dele é `pk`. */
static void rt_kron(const long *A, int a, const long *B, int b, long *K, int pk){
    for(int i = 0; i < a; i++) for(int j = 0; j < a; j++)
    for(int k = 0; k < b; k++) for(int l = 0; l < b; l++)
        K[(i*b+k)*pk + (j*b+l)] = A[i*a+j]*B[k*b+l];
}

/* ═══════════════════════════════════════════════════════════════════════════════════
 * O TEOREMA DO PONTO FIXO, E A LEI 8 — as três peças que o fecham.
 *
 * `thm:corte-ponto-fixo` do Corpo Universal, e a Lei 8 que lhe dá o chão. O enunciado:
 *
 *   (i)   a órbita de ∞ é [p:q] ⟼ [m·p+q : p], sem uma divisão — e o ponto de partida
 *         só existe pela Lei 0, porque num corpo sem ela «começar no infinito» não quer
 *         dizer nada
 *   (ii)  o ponto fixo é o VECTOR PRÓPRIO: x² = m·x + 1, com soma tr = m e produto
 *         det = −1, os dois INTEIROS
 *   (iii) e o corte é a FALTA dele. Completando o quadrado,
 *
 *              4(p² − m·p·q − q²) = (2p − m·q)² − D·q²,   D = m² + 4
 *
 *         logo o ponto fixo vive em ℙ¹(ℚ) ⟺ D é QUADRADO PERFEITO. E aí decide-se em
 *         duas linhas para todo m ≥ 1: m² < D < (m+2)², o único quadrado possível no
 *         meio é (m+1)², e D = (m+1)² pede 2m = 3, que não tem solução em ℤ.
 *         É o PASSO, e não a lista.
 *   (iv)  e a FACE FINITA diz o contrário: em 𝔽ₚ a mesma equação tem raiz exactamente
 *         quando D é resíduo quadrático — aí o ponto fixo ESTÁ no andar, a órbita cai
 *         nele, e não há corte nenhum a fazer. É a Lei 8: o anel onde o grupo é finito
 *         e as órbitas fecham, e onde tudo se verifica EXAUSTIVAMENTE.
 *   (v)   e há VOLTA, que é a metade dual: det A_m = −1 torna a inversa INTEIRA, e a
 *         acção dela é [p:q] ⟼ [q : p − m·q] — letra por letra, a descida de (iii).
 * ═══════════════════════════════════════════════════════════════════════════════════ */

/* ── (iii) O PONTO FIXO EM ℙ¹(ℚ): decide-se pelo DISCRIMINANTE ───────────────────────
 * Devolve 1 se D = m² + 4 é quadrado perfeito — isto é, se o ponto fixo cabe no andar.
 * Para m ≥ 1 devolve sempre 0, e a razão é o intervalo, não uma varredura. */
static int rt_fixo_racional(int32_t m){
    int64_t D = (int64_t)m*m + 4;
    int32_t r = 0;
    while((int64_t)r*r < D) r++;
    return (int64_t)r*r == D;
}
static int32_t rt_fixo_candidato(int32_t m){
    int64_t D = (int64_t)m*m + 4;
    int64_t lo = (int64_t)m*m, hi = (int64_t)(m+2)*(m+2);
    return (D > lo && D < hi) ? (int32_t)((m+1)*(m+1)) : 0;
}

/* ── (iv) A LEI 8: em 𝔽ₚ o ponto fixo EXISTE quando D é resíduo quadrático ───────────
 * O critério de Euler: a é resíduo quadrático mod p ⟺ a^((p−1)/2) ≡ 1. Devolve 1 se
 * o ponto fixo cai no andar finito, 0 se não — e é o contrário exacto do caso racional,
 * que é o que torna o corte ESTRUTURAL e não um defeito de ℚ. */
static int rt_fixo_em_fp(long m, long p){
    long D = ((m*m + 4) % p + p) % p;
    if(D == 0) return 1;                                  /* raiz dupla: o ponto está lá */
    return rt_pot_mod(D, (p-1)/2, p) == 1;
}

/* ── (v) A VOLTA: a órbita para trás, e ela é INTEIRA ────────────────────────────────
 * [p:q] ⟼ [q : p − m·q], que é A_m⁻¹ = [[0,1],[1,−m]] — inteira porque det A_m = −1.
 * A ida cresce como σᵏ e a volta desce pelo mesmo caminho, até ao ∞ exacto. */
static void rt_volta(long m, long p, long q, long *np, long *nq){
    *np = q; *nq = p - m*q;
}

/* ── O SINAL É UMA DOBRA, E O MDC VIVE DO LADO QUE ELA NÃO MOVE ──────────────────────
 * O `universal.tex` trata isto, e não como caso particular: `cor:cadeia` diz que
 *
 *      ℤ  =  ℕ  +  o SINAL
 *
 * e a Lei 1 diz o que o sinal é — 1† = −1, a Möbius INVOLUTIVA de traço 0, ν(x) = −1/x.
 * É a MESMA dobra que reparte a operação em Dir e Cruz: uma involução, com espectro
 * {+1, −1}, e portanto com dois lados — o que ela FIXA e o que ela INVERTE.
 *
 *      x  =  |x| · sinal(x)          o que fica  ·  o que inverte
 *
 * E o mdc é uma função da CLASSE, não do representante: a divisibilidade não vê o sinal,
 * porque d | a ⟺ d | (−a). Logo o mdc vive do lado FIXO da dobra.
 *
 * ── E É ISSO QUE EXPLICA AS 28 CÓPIAS ───────────────────────────────────────────────
 * Vinte não tratam o sinal e oito tratam, e divergem em 3280 dos 6561 pares de
 * [−40,40]² — mdc(−40,−40) dá −40 numas e 40 noutras. Escrevi primeiro que eram «duas
 * respostas para a mesma pergunta», e não é: são DOIS REPRESENTANTES DA MESMA CLASSE.
 * As que não tratam devolvem o representante com o sinal de `a`, porque em C o resto
 * herda o sinal do dividendo; as que tratam devolvem o representante NÃO NEGATIVO.
 *
 * A canónica é o não negativo, e a razão não é gosto: é que «MÁXIMO» divisor comum pede
 * uma ordem, e na ordem dos inteiros um divisor negativo nunca é o maior. O
 * representante certo é o do lado que a dobra fixa. */

/* a DOBRA do sinal, explícita: x ↦ (|x|, s) com s ∈ {+1,−1,0}, e a volta x = |x|·s.
 * É a decomposição de ℤ em ℕ × {±1} que a `cor:cadeia` enuncia, e ν∘ν = id é o que a
 * torna uma dobra e não uma projecção — nada se perde, e a volta devolve. */
static int32_t rt_sinal(int32_t x){ return x > 0 ? 1 : (x < 0 ? -1 : 0); }
static int32_t rt_modulo(int32_t x){ return x < 0 ? -x : x; }

/* Fase B: mdc/inverso em int32 — coordenadas do envelope E₁₆; long mantém vectores grandes */
static int32_t rt_mdc_i32(int32_t a, int32_t b){
    if(a < 0) a = -a;
    if(b < 0) b = -b;
    while(b){ int32_t t = a % b; a = b; b = t; }
    return a;
}
static int32_t rt_inv_mod_i32(int32_t a, int32_t p){
    int64_t r = 1, e = (int64_t)p - 2;
    a = (int32_t)(((a % p) + p) % p);
    int64_t base = a;
    while(e){
        if(e & 1) r = r * base % p;
        base = base * base % p;
        e >>= 1;
    }
    return (int32_t)r;
}

static long rt_mdc(long a, long b){
    if(a < 0) a = -a;
    if(b < 0) b = -b;
    while(b){ long t = a % b; a = b; b = t; }
    return a;
}

/* e o mínimo múltiplo comum, pelo par: a·b = mdc·mmc. Divide-se PRIMEIRO para o produto
 * não estourar — é a mesma razão por que a soma de racionais cancela em cruz antes de
 * multiplicar. */
static long rt_mmc(long a, long b){
    if(!a || !b) return 0;
    long g = rt_mdc(a, b);
    if(a < 0) a = -a;
    if(b < 0) b = -b;
    return (a / g) * b;
}

/* ═══════════════════════════════════════════════════════════════════════════════════
 * A NORMALIZAÇÃO, E O QUE MAIS SE REPETE — o levantamento por padrão, segunda volta.
 *
 *      25×  o produto interno  Σ a[i]·b[i]        ← já era rt_dir, e 25 reescrevem-no
 *      23×  o módulo positivo  ((x%p)+p)%p
 *      15×  a transposta       M[j][i]
 *      11×  a norma²           Σ x[i]·x[i]        ← já era rt_norma
 *      11×  a potência de matriz  P ← P·C
 *       9×  a NORMALIZAÇÃO    v/‖v‖, v/z
 *       5×  a troca           t=a; a=b; b=t
 *       4×  o traço           Σ M[i][i]
 *       4×  a identidade      (i==j) ? 1 : 0
 *
 * ── E NORMALIZAR NÃO É DIVIDIR ──────────────────────────────────────────────────────
 *
 * É a peça que mais custa, e a que mais engana. Dividir por ‖v‖ traz uma RAIZ, a raiz
 * traz a vírgula, e a vírgula traz o limiar — foi assim que o `forca.c` acabou a medir
 * cos²+sin²=1 com uma régua de quinze casas, quando a identidade que ele queria era
 * Lagrange, e Lagrange é inteira.
 *
 * Em ℤ normalizar é GUARDAR O PAR. O cosseno ao quadrado entre dois vectores é
 *
 *      cos²(a,b)  =  ⟨a,b⟩² / (‖a‖²·‖b‖²)
 *
 * um racional EXACTO — numerador e denominador inteiros, sem uma raiz —, e as
 * comparações entre direcções fazem-se por produto cruzado. A raiz só apareceria se
 * alguém pedisse o cosseno em vez do seu quadrado, e ninguém precisa: as perguntas
 * («são paralelos?», «são perpendiculares?», «qual é mais alinhado?») respondem-se
 * todas no quadrado.
 * ═══════════════════════════════════════════════════════════════════════════════════ */

/* o cos² como PAR (numerador, denominador) — a normalização sem dividir e sem raiz */
static void rt_cos2(const long *a, const long *b, int n, long *num, long *den){
    long d = rt_dir(a, b, n);
    *num = d*d;
    *den = rt_norma(a, n) * rt_norma(b, n);
}

/* e as três perguntas que a normalização servia, respondidas no quadrado:
 *   paralelos      ⟺ cos² = 1  ⟺  ⟨a,b⟩² = ‖a‖²‖b‖²   (Lagrange: o cruzado é nulo)
 *   perpendiculares⟺ cos² = 0  ⟺  ⟨a,b⟩ = 0
 *   mais alinhado  ⟺ compara-se cos² por PRODUTO CRUZADO, sem dividir */
static int rt_paralelos(const long *a, const long *b, int n){
    long nu, de; rt_cos2(a, b, n, &nu, &de);
    return nu == de;
}
static int rt_perp(const long *a, const long *b, int n){ return rt_dir(a, b, n) == 0; }
static int rt_mais_alinhado(const long *a, const long *b, const long *c, const long *d, int n){
    long n1, d1, n2, d2;
    rt_cos2(a, b, n, &n1, &d1);
    rt_cos2(c, d, n, &n2, &d2);
    return n1*d2 > n2*d1;                     /* cruzado, e sem uma divisão */
}

/* ── O MÓDULO POSITIVO: 23 cópias ────────────────────────────────────────────────────
 * Em C o resto herda o sinal do dividendo, logo −3 % 7 dá −3 e não 4. O representante
 * canónico em ℤ/pℤ é o não negativo — é a mesma dobra do sinal, um andar acima. */
static int32_t rt_mod(int32_t x, int32_t p){ return (int32_t)(((x % p) + p) % p); }

/* ── A TRANSPOSTA: 15 cópias — e ela É o espelho τ ───────────────────────────────────
 * τ(M) = Mᵀ é a involução que reparte Dir e Cruz nas matrizes. Não é um utilitário: é
 * a dobra, escrita como operação. */
static void rt_transpoe(const long *M, int n, long *T){
    for(int i = 0; i < n; i++) for(int j = 0; j < n; j++) T[i*n + j] = M[j*n + i];
}

/* ── O PRODUTO E A POTÊNCIA DE MATRIZES: 11 cópias ───────────────────────────────────*/
static void rt_mul_mat(const long *A, const long *B, int n, long *C){
    for(int i = 0; i < n; i++) for(int j = 0; j < n; j++){
        long s = 0;
        for(int k = 0; k < n; k++) s += A[i*n + k]*B[k*n + j];
        C[i*n + j] = s;
    }
}
/* (o parâmetro chamava-se `I`, e `I` é a unidade imaginária de <complex.h>: qualquer
 *  ficheiro que inclua os dois deixava de compilar. Um nome de uma letra num header é um
 *  nome que colide.) */
static void rt_identidade(long *Id, int n){
    for(int i = 0; i < n; i++) for(int j = 0; j < n; j++) Id[i*n + j] = (i == j);
}
/* A POTÊNCIA DE UMA MATRIZ, por multiplicação repetida — e o corpo do passo não menciona
 * k, que é o que dá o ∀k pela `thm:meta-inducao`. É o gerador do semigrupo na realização
 * EXACTA: A^{a+b} = A^a·A^b é o morfismo (ℕ,+) → (matrizes,×), medido em ℤ e sem uma
 * exponencial. O chamador garante n ≤ RT_MAX. */
static void rt_pot_mat(const long *A, int n, int k, long *R){
    rt_identidade(R, n);
    long T[RT_MAX*RT_MAX];
    for(int t = 0; t < k; t++){
        rt_mul_mat(R, A, n, T);
        for(int i = 0; i < n*n; i++) R[i] = T[i];
    }
}

static long rt_traco(const long *M, int n){
    long s = 0;
    for(int i = 0; i < n; i++) s += M[i*n + i];
    return s;
}

/* ═══════════════════════════════════════════════════════════════════════════════════
 * ℤ[√D] — O CORPO ONDE A RAIZ NÃO SE FORMA.
 *
 * O ponto fixo da família metálica é σ = (m + √D)/2 com D = m² + 4, e ele é irracional
 * sempre que D não é quadrado perfeito (`thm:fixo-dual`). Formá-lo em vírgula é a coisa
 * errada a fazer: o que se quer dele são somas, produtos e potências, e essas fazem-se
 * no anel ℤ[√D] sem a raiz aparecer uma única vez —
 *
 *      (a₁ + b₁√D)(a₂ + b₂√D) = (a₁a₂ + D·b₁b₂) + (a₁b₂ + a₂b₁)√D
 *
 * — porque √D·√D é D, um INTEIRO, e é aí que a raiz se cancela contra si própria. O par
 * (a,b) é a coordenada, e a conjugação é a DOBRA: b ↦ −b, com o mesmo espectro {+1,−1}
 * da Lei 1. A norma N(a+b√D) = a² − D·b² é o produto do elemento pelo seu dual, e é ela
 * que vale ±1 nas unidades — a condição do `thm:cruzado-potencia`.
 *
 * ── E É AQUI QUE O 2 ENTRA ──────────────────────────────────────────────────────────
 * σ tem um meio: 2σ = m + √D. Por isso guarda-se 2σ e não σ, e as potências saem como
 * (2σ)ᵏ = A_k + B_k√D com A e B inteiros; o traço, que é σᵏ + σ'ᵏ, lê-se daí por
 *
 *      σᵏ + σ'ᵏ = ((2σ)ᵏ + (2σ')ᵏ) / 2ᵏ = 2A_k / 2ᵏ = A_k / 2^{k−1}
 *
 * e essa divisão é EXACTA — o que se verifica, e não se assume.
 *
 * Para coeficientes racionais e escala m genérica em ℚ(m√D), ver lib/qmd.h
 * (thm:serie-quadratica: conj, norma, inversão, qmd_verifica_rapida).
 * ═══════════════════════════════════════════════════════════════════════════════════ */

/* o produto em ℤ[√D]: (a₁+b₁√D)(a₂+b₂√D) */
static void rt_zd_mul(long a1, long b1, long a2, long b2, long D, long *a, long *b){
    *a = a1*a2 + D*b1*b2;
    *b = a1*b2 + a2*b1;
}
/* a potência k-ésima, por multiplicação repetida — e o passo não menciona k */
static void rt_zd_pot(long a0, long b0, long D, int k, long *a, long *b){
    long ra = 1, rb = 0;                       /* o 1 do anel */
    for(int t = 0; t < k; t++){
        long na, nb;
        rt_zd_mul(ra, rb, a0, b0, D, &na, &nb);
        ra = na; rb = nb;
    }
    *a = ra; *b = rb;
}
/* a NORMA, que é o elemento vezes o seu dual: (a+b√D)(a−b√D) = a² − D·b². Nas unidades
 * vale ±1, e é essa a condição que faz o cruzado atravessar a órbita. */
static long rt_zd_norma(long a, long b, long D){ return a*a - D*b*b; }

/* O TRAÇO σᵏ + σ'ᵏ, EM INTEIROS E SEM RAIZ. Guarda-se 2σ = m + √D, eleva-se a k, e o
 * resultado é A_k/2^{k−1} — divisão exacta, e devolve-se 0 se ela não for (o que não pode
 * acontecer, e por isso se verifica). D = m² + 4 é assumido. */
static long rt_traco_metalico(long m, int k){
    if(k == 0) return 2;
    long D = m*m + 4, A, B;
    rt_zd_pot(m, 1, D, k, &A, &B);             /* (2σ)ᵏ = A + B√D */
    long den = 1;
    for(int t = 1; t < k; t++) den *= 2;       /* 2^{k−1} */
    if(A % den != 0) return 0;                 /* a divisão TEM de ser exacta */
    return A / den;
}

/* ═══════════════════════════════════════════════════════════════════════════════════
 * O CRUZADO, O BIVECTOR E A ORDEM SEM RAIZ — `thm:cruzado-potencia`.
 *
 * O teorema diz que o cruzado se transforma pelo determinante:
 *
 *      Cruz(Au, Av) = det(A)·Cruz(u,v)      e logo      (det A)^k na potência
 *      |det A| = 1  ⟹  |Cruz| INVARIANTE em toda a órbita
 *
 * e daqui a raiz sai de cena, porque a raiz é a INVERSA da potência e o cruzado não vê
 * nem uma nem outra. As perguntas métricas respondem-se no par (Dir, Cruz), ambos
 * inteiros, e o par fecha por LAGRANGE:
 *
 *      Dir(u,v)² + ‖u∧v‖² = Dir(u,u)·Dir(v,v)
 *
 * que é cos² + sin² = 1 sem nunca formar nem o cosseno nem o seno.
 *
 * ── E ESTAS SÃO AS CÓPIAS QUE ISTO RECOLHE ──────────────────────────────────────────
 *
 *      298  sqrt/hypot em 85 ficheiros      — quase todos a decidir uma ORDEM
 *       79  produto interno em laço, 39     — é rt_dir, e já cá estava
 *       33  o determinante 2×2 à mão, 19    — `a*d − b*c` e `u[0]*v[1] − u[1]*v[0]`
 *       14  norma ao quadrado em laço, 9    — é rt_norma
 *
 * A FRONTEIRA, dita: onde o valor da raiz é o RESULTADO — um comprimento a entregar ao
 * cliente —, ela faz-se no fim e fora do núcleo, como todo o resto da representação.
 * O que estas funções tiram são as raízes que apareciam para responder a uma pergunta
 * que nunca foi sobre o comprimento.
 * ═══════════════════════════════════════════════════════════════════════════════════ */

/* O CRUZADO EM ℤ²: a entrada independente de Cruz, e É o determinante da matriz que os
 * dois vectores formam. Uma coisa, dois nomes — e é a área. */
static long rt_cruz2(const long *u, const long *v){
    return (long)((int64_t)u[0]*v[1] - (int64_t)u[1]*v[0]);
}
static int32_t rt_cruz2_i32(const int32_t *u, const int32_t *v){
    return (int32_t)((int64_t)u[0]*v[1] - (int64_t)u[1]*v[0]);
}

/* o mesmo, com as quatro entradas soltas: o `a*d − b*c` que aparece 23 vezes no repo */
static long rt_det2(long a, long b, long c, long d){
    return (long)((int64_t)a*d - (int64_t)b*c);
}

/* aplicar uma matriz n×n a um vector, e o caso 2×2 que é o da órbita */
static void rt_aplica(const long *M, const long *v, int n, long *r){
    for(int i = 0; i < n; i++){
        long s = 0;
        for(int j = 0; j < n; j++) s += M[i*n + j] * v[j];
        r[i] = s;
    }
}

/* ‖u∧v‖² PELA FORMA FECHADA: Dir(u,u)·Dir(v,v) − Dir(u,v)². É o que se usa; existe em
 * TODA a dimensão (ao contrário do produto vectorial, que só vive em 3 e 7 por Hurwitz),
 * e não se constrói a matriz n×n para um número que sai em três produtos. */
static long rt_bivetor2(const long *a, const long *b, int n){
    long aa = rt_dir(a, a, n), bb = rt_dir(b, b, n), ab = rt_dir(a, b, n);
    return aa*bb - ab*ab;
}

/* e PELA SOMA DAS COMPONENTES: Σ_{i<j} (aᵢbⱼ − aⱼbᵢ)², a norma de Frobenius do bivector.
 * É a SEGUNDA ROTA, e existe por uma razão precisa: sem ela, verificar Lagrange contra
 * `rt_bivetor2` seria comparar a definição consigo própria — foi esse o defeito que o
 * §S3 do semantico.c tinha, com um limiar de 1e-12 por cima a dar-lhe cara de medição.
 * Custa O(n²) e a outra custa O(n): usa-se para MEDIR, não para calcular. */
static long rt_bivetor_soma(const long *a, const long *b, int n){
    long s = 0;
    for(int i = 0; i < n; i++)
        for(int j = i + 1; j < n; j++){
            long c = a[i]*b[j] - a[j]*b[i];
            s += c*c;
        }
    return s;
}

/* LAGRANGE: devolve 1 se as duas rotas fecham. A identidade é HOMOGÉNEA de grau 4, logo
 * escalar os dois vectores por um factor comum não a move — é isso que permite medi-la
 * exacta sobre dados que chegaram em vírgula, depois de escalados a inteiros. */
static int rt_lagrange(const long *a, const long *b, int n){
    return rt_bivetor_soma(a, b, n) == rt_bivetor2(a, b, n);
}

/* A ORDEM SEM RAIZ: −1 se ‖a‖ < ‖b‖, +1 se maior, 0 se iguais. Não é uma aproximação da
 * comparação de normas: É ela, porque x ↦ x² é monótona nos não negativos e a pergunta
 * nunca foi sobre o comprimento — era sobre a ORDEM. Aqui estão as 298 raízes. */
static int rt_ordem_norma(const long *a, const long *b, int n){
    long na = rt_dir(a, a, n), nb = rt_dir(b, b, n);
    return na < nb ? -1 : (na > nb ? 1 : 0);
}

/* e a mesma pergunta sobre dois escalares não negativos, que é onde o sqrt aparecia para
 * comparar: |x| < |y| ⟺ x² < y², e os quadrados são inteiros */
static int rt_ordem_abs(long x, long y){
    long ax = x < 0 ? -x : x, ay = y < 0 ? -y : y;
    return ax < ay ? -1 : (ax > ay ? 1 : 0);
}

/* E O QUADRADO PERFEITO, que é a única pergunta em que a raiz é mesmo a resposta: existe
 * r inteiro com r² = x? Por busca binária em inteiros, sem uma única operação de vírgula.
 * Devolve 1 e escreve r, ou 0 se x não é quadrado (ou é negativo). É o que decide o
 * `rt_fixo_candidato`: D = m²+4 ser quadrado perfeito é o ponto fixo cair no racional. */
static int rt_raiz_exacta(long x, long *r){
    if(x < 0) return 0;
    if(x < 2){ if(r) *r = x; return 1; }
    long lo = 1, hi = 3037000499L;
    while(lo < hi){
        long mid = lo + (hi - lo + 1)/2;
        if(mid <= x / mid) lo = mid; else hi = mid - 1;
    }
    if(lo*lo != x) return 0;
    if(r) *r = lo;
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════════════════════
 * A ENTREGA — o número sai em FRACÇÃO CONTÍNUA, e o cliente reconstrói o que quiser.
 *
 * O Aarão: «essa representação vai até ao fim e entrega assim mesmo em long int, daí ele
 * pode reconstruir depois em qualquer representação».
 *
 * É o `def:real` e o `prop:alfabeto` levados à saída. Um decimal escrito NÃO é um número
 * aproximado: «3.14159» é 314159/100000, um racional EXACTO, e o que o perde é convertê-lo
 * a `double` — 0,1 não é 1/10 em base dois, e nunca foi. A fracção contínua desse racional
 * é uma palavra de inteiros, sai por Euclides, e reconstrói o racional de volta sem perder
 * um bit. O que se entrega são longs; o que o cliente faz com eles é dele.
 *
 *      texto  ──►  p/q exacto  ──►  [a₀; a₁, …, aₙ]  ──►  p/q  ──►  a representação que quiser
 *
 * A fracção contínua propriamente já vive em `aritmetica.h` — `nt_fc`, `nt_convergentes`,
 * `nt_cmp`, `nt_mediante`, sobre ℕ e com a saturação contada à parte. Aqui está só o que
 * falta para a ponte: a leitura do TEXTO, o sinal, e a reconstrução.
 *
 * E O SINAL VAI À PARTE, que não é preguiça: é a Lei 1. ℤ = ℕ + o SINAL, e o sinal é a
 * DOBRA de espectro {+1,−1} — a mesma involução que reparte Dir e Cruz um andar acima.
 * A fracção contínua é a palavra sobre ℕ; o sinal é a dobra que a leva a ℤ.
 * ═══════════════════════════════════════════════════════════════════════════════════ */

#define RT_CF_MAX 48

typedef struct { int sinal; long a[RT_CF_MAX]; int n; int saturou; } RtCf;
/* Representação canónica da palavra: slots consecutivos no disco — ver rt_cf_slot.h */

/* «−3.14159» ⟼ sinal −1, p = 314159, q = 100000. EXACTO, e sem uma divisão de reais.
 * Devolve 1 se leu, 0 se o texto não é um decimal ou se os inteiros não chegam.
 * O expoente («1e3») também entra, e multiplica p ou q por 10 tantas vezes. */
static int rt_le_decimal_end(const char *s, int *sinal, long *p, long *q, const char **end){
    int i = 0;
    while(s[i] == ' ' || s[i] == '\t') i++;
    *sinal = 1;
    if(s[i] == '-'){ *sinal = -1; i++; }
    else if(s[i] == '+') i++;
    long v = 0, d = 1;
    int digitos = 0;
    while(s[i] >= '0' && s[i] <= '9'){
        if(v > 922337203685477580L) return 0;              /* não cabe: diz-se, não se finge */
        v = v*10 + (s[i]-'0'); i++; digitos++;
    }
    if(s[i] == '.'){
        i++;
        while(s[i] >= '0' && s[i] <= '9'){
            if(v > 922337203685477580L || d > 922337203685477580L) return 0;
            v = v*10 + (s[i]-'0'); d *= 10; i++; digitos++;
        }
    }
    if(!digitos) return 0;
    if(s[i] == 'e' || s[i] == 'E'){
        i++;
        int es = 1;
        if(s[i] == '-'){ es = -1; i++; }
        else if(s[i] == '+') i++;
        int e = 0;
        while(s[i] >= '0' && s[i] <= '9'){ e = e*10 + (s[i]-'0'); i++; if(e > 30) return 0; }
        for(int k = 0; k < e; k++){
            if(es > 0){ if(v > 922337203685477580L) return 0; v *= 10; }
            else       { if(d > 922337203685477580L) return 0; d *= 10; }
        }
    }
    *p = v; *q = d;
    if(end) *end = s + i;
    return 1;
}
static int rt_le_decimal(const char *s, int *sinal, long *p, long *q){
    return rt_le_decimal_end(s, sinal, p, q, NULL);
}

/* p/q ⟼ a palavra [a₀; a₁, …, aₙ]. É EUCLIDES — a mesma descida do mdc, lida noutra coluna.
 * O sinal entra como está e não se mistura com os quocientes. */
static void rt_cf_de(int sinal, long p, long q, RtCf *c){
    c->sinal = (p == 0) ? 1 : sinal;
    c->n = 0; c->saturou = 0;
    long P = p < 0 ? -p : p, Q = q < 0 ? -q : q;
    while(Q != 0){
        if(c->n >= RT_CF_MAX){ c->saturou = 1; return; }    /* conta-se, não se corta calado */
        c->a[c->n++] = P / Q;
        long r = P % Q;
        P = Q; Q = r;
    }
}

/* e a VOLTA: da palavra ao racional, pela recorrência dos convergentes lida de trás para a
 * frente. Devolve 1, ou 0 se um produto não coubesse. */
/* ── O TEOREMA DO OPERADOR COMO LEITOR/ESCRITOR ──────────────────────────────────────
 *
 * `thm:operador`: a indução, a meta-indução e o acumulador são um só. E sobre o acumulador
 * as duas metades têm um nome operacional — uma ESCREVE, a outra LÊ:
 *
 *      rt_op_escreve   a INDUÇÃO   (λ⁺)   acrescenta um quociente à palavra
 *      rt_op_le        a META-INDUÇÃO (λ⁻) lê o convergente que a palavra já tem
 *
 * e o par fecha: escrever k termos e ler dá exactamente o k-ésimo convergente, que é o
 * mesmo que a órbita de ∞ produz em k passos (rt_orbita). São as duas metades do MESMO
 * operador — não duas funções que por acaso concordam.
 *
 * O escritor recusa quando a palavra está cheia, e diz-se: RT_CF_MAX é o tecto do array,
 * e um tecto que não se verifica é documentação e não limite. */
static int rt_op_escreve(RtCf *w, long a){
    if(w->n >= RT_CF_MAX){ w->saturou = 1; return 0; }
    w->a[w->n++] = a;
    return 1;
}

/* O LEITOR: o convergente de ordem k, pela recorrência p_k = a_k·p_{k−1} + p_{k−2}. Lê
 * apenas os primeiros k+1 termos — é a leitura PARCIAL, que é o que faz dela a meta-
 * indução: ela mede o passo sem precisar do fim da palavra. */
static int rt_op_le(const RtCf *w, int k, long *p, long *q){
    if(k < 0 || k >= w->n) return 0;
    long p0 = 1, q0 = 0, p1 = w->a[0], q1 = 1;
    for(int i = 1; i <= k; i++){
        long np = w->a[i]*p1 + p0, nq = w->a[i]*q1 + q0;
        p0 = p1; q0 = q1; p1 = np; q1 = nq;
    }
    if(p) *p = p1;
    if(q) *q = q1;
    return 1;
}

static int rt_cf_para(const RtCf *c, long *p, long *q){
    if(c->n == 0){ *p = 0; *q = 1; return 1; }
    long P = 1, Q = 0;                                      /* ∞ = [1:0], como em rt_orbita */
    for(int k = c->n - 1; k >= 0; k--){
        long ak = c->a[k];
        if(ak != 0 && P > 4611686018427387903L / (ak > 1 ? ak : 1)) return 0;
        long np = ak*P + Q;
        Q = P; P = np;
    }
    *p = c->sinal * P; *q = Q;
    return 1;
}

/* A RECONSTRUÇÃO DO CLIENTE: de p/q sai o decimal com quantas casas ele pedir, por DIVISÃO
 * LONGA em inteiros — o resto é sempre menor que q, logo nada transborda e nada arredonda
 * por acaso. Escreve em `d` e devolve o número de bytes. Isto não é «converter a double e
 * imprimir»: é o cliente a ler a palavra na base que lhe apetecer. */
static int rt_escreve_decimal(int sinal, long p, long q, int casas, char *d, int max){
    int k = 0;
    if(q == 0){ if(max > 3){ d[0]='n'; d[1]='a'; d[2]='n'; d[3]=0; } return 3; }
    long P = p < 0 ? -p : p;
    if(sinal < 0 && (P != 0) && k < max-1) d[k++] = '-';
    long inteiro = P / q, r = P % q;
    char buf[24]; int nb = 0;
    if(inteiro == 0) buf[nb++] = '0';
    while(inteiro > 0){ buf[nb++] = (char)('0' + inteiro % 10); inteiro /= 10; }
    while(nb > 0 && k < max-1) d[k++] = buf[--nb];
    if(casas > 0 && k < max-1){
        d[k++] = '.';
        for(int i = 0; i < casas && k < max-1; i++){
            r *= 10;
            d[k++] = (char)('0' + r / q);
            r %= q;
        }
    }
    d[k] = 0;
    return k;
}

/* ═══════════════════════════════════════════════════════════════════════════════════
 * NORMALIZAR É ESCOLHER A UNIDADE — e depois tudo é inteiro.
 *
 * O Aarão: «normalizar e operar em inteiros». As duas metades são uma só operação, e a
 * ordem importa: primeiro escolhe-se a unidade em que os dados são inteiros, e só depois
 * se opera. Feito assim, nenhuma conta precisa de vírgula.
 *
 * A UNIDADE É O DENOMINADOR COMUM. Um dado escrito «0,6» é 6/10; «1,25» é 125/100. O
 * denominador comum de um conjunto é o MMC dos denominadores, e multiplicar por ele leva
 * todos a ℤ de uma vez. Não é aproximar — é MUDAR DE RÉGUA, e a régua nova é exacta.
 *
 * ── E POR QUE É QUE ISTO NÃO ESTRAGA A MEDIDA ───────────────────────────────────────
 *
 * Porque as perguntas que esta casa faz são HOMOGÉNEAS. Escalar todos os dados por um
 * factor comum λ multiplica os dois lados de:
 *
 *      uma COMPARAÇÃO           a < b          por λ            — não muda
 *      uma RAZÃO                a/b            por λ/λ = 1      — não muda
 *      LAGRANGE                 Dir² + Cruz² = N(u)N(v)   por λ⁴ — não muda
 *      o CRUZADO na órbita      Cruz(Au,Av) = det(A)Cruz(u,v)   — não muda
 *
 * e o que MUDA é só o valor absoluto, que é a representação e sai no fim (`cor:entrega`).
 * Onde a pergunta NÃO é homogénea — uma constante física com dimensão, um limiar
 * absoluto — a escala tem de entrar na conta, e isso diz-se em vez de se escalar calado.
 *
 * ── OS QUATRO CASOS QUE ISTO RECOLHE ────────────────────────────────────────────────
 *
 *      microfluidica   metros escritos como 100e-6   →  unidade de 10 µm, tudo inteiro
 *      hopfield        W/N em cada uso                →  N-vezes: «W = N·w, sem dividir»
 *      semantico       embeddings de um modelo        →  milésimos, e Lagrange é grau 4
 *      gerador_analog  1 mH e 1 nF                    →  1000 µH e 1000 pF
 *
 * Em todos, o double era a régua errada — não o objecto.
 * ═══════════════════════════════════════════════════════════════════════════════════ */

/* O DENOMINADOR COMUM de n textos decimais: lê cada um como p/q exacto e devolve o MMC
 * dos q. Devolve 0 se algum texto não é decimal ou se o MMC não cabe — e o zero é a
 * metade que faz disto uma medição: quando a unidade não existe em ℤ, diz-se. */
static long rt_unidade_comum(const char **textos, int n){
    long u = 1;
    for(int i = 0; i < n; i++){
        int sg; long p, q;
        if(!rt_le_decimal(textos[i], &sg, &p, &q)) return 0;
        long g = rt_mdc(u, q);
        if(g == 0) return 0;
        if(u > 4000000000000000000L / (q/g)) return 0;      /* o MMC não cabe */
        u = (u / g) * q;
    }
    return u;
}

/* e a CONVERSÃO: com a unidade escolhida, cada texto vira um INTEIRO. Devolve quantos
 * converteu; se algum não couber, pára e devolve o índice — não trunca calado. */
static int rt_para_unidade(const char **textos, int n, long unidade, long *saida){
    for(int i = 0; i < n; i++){
        int sg; long p, q;
        if(!rt_le_decimal(textos[i], &sg, &p, &q)) return i;
        if(q == 0 || unidade % q != 0) return i;             /* a unidade tem de servir */
        long f = unidade / q;
        if(p != 0 && rt_modulo(p) > 4000000000000000000L / f) return i;
        saida[i] = sg * p * f;
    }
    return n;
}

/* E A ESCALA DE UM VECTOR JÁ EM ℤ: divide todas as coordenadas pelo mdc delas, que é a
 * forma mínima — o mesmo ponto da recta projectiva com o menor par possível. Devolve o
 * factor retirado. É `[p:q]` a menos de escala, da `def:lei0`, aplicado a n coordenadas. */
static long rt_reduz_vector(long *v, int n){
    long g = 0;
    for(int i = 0; i < n; i++) g = rt_mdc(g, v[i]);
    if(g <= 1) return g ? g : 1;
    for(int i = 0; i < n; i++) v[i] /= g;
    return g;
}

/* ═══════════════════════════════════════════════════════════════════════════════════
 * AS INVERSAS — e a armadilha que elas trazem consigo.
 *
 * Toda operação com fibra tem volta (`project-corpos-a-escada-fecha`), e esta casa está
 * cheia de pares: a raiz e a potência, o log e a exponencial, o acos e o cosseno, a
 * fracção contínua e os convergentes, a conjugação consigo própria. Reuni-los aqui é
 * útil por uma razão que não é a comodidade:
 *
 *      MEDIR f(f⁻¹(x)) = x NÃO MEDE NADA.
 *
 * É a definição do par relida. E não é um erro teórico — foi encontrado três vezes num
 * dia, sempre disfarçado por conversões que o tornavam ilegível:
 *
 *      octeto.c   ok(|acos(ip/(n0·n1))·180/π − acos(−1/3)·180/π| < 1e-9)
 *                 e ip/(n0·n1) É −1/3: dois acos e duas conversões a mascarar x = x
 *      octeto.c   ok(|acos(cos(2π/3))·180/π − 120| < 1e-9)
 *                 que é acos∘cos, a função e a sua inversa
 *      gerador    difz = |√(L/C) − √((L/2)/(C/2))|
 *                 e (L/2)/(C/2) É L/C
 *
 * A regra que daí sai: quando os dois lados de uma comparação passam pela MESMA inversa,
 * ela cancela-se e o que fica é a igualdade de dentro. Ou se mede a igualdade de dentro
 * — que costuma ser INTEIRA —, ou não se mede nada.
 *
 * ── E AS INVERSAS EXACTAS SÃO INTEIRAS ──────────────────────────────────────────────
 * Onde a fibra existe em ℤ, a inversa não precisa de vírgula: a raiz k-ésima acha-se por
 * busca binária, o logaritmo discreto por multiplicação repetida, e as duas RECUSAM
 * quando não há fibra — que é a metade que as torna medições e não adivinhas.
 *
 *   PAR                       DIRECTA              INVERSA            onde
 *   potência ↔ raiz           rt_ipow              rt_raiz_k          aqui
 *   quadrado ↔ raiz           x*x                  rt_raiz_exacta     aqui
 *   base ↔ expoente           rt_ipow              rt_log_int         aqui
 *   p/q ↔ palavra             rt_cf_para           rt_cf_de           aqui
 *   a+b√D ↔ a−b√D             rt_zd_conj           rt_zd_conj         (involução)
 *   ×a mod p ↔ ×a⁻¹ mod p     rt_pot_mod           rt_inv_mod         aqui
 *   M ↔ M⁻¹ com |det|=1       rt_mul_mat           rt_inversa2        (= rt_adjunta2/det)
 *   [p:q] ↔ [q:p] em ℙ¹        a TROCA              a propria          (involucao, Lei 0)
 *   coeficientes ↔ reversão   rt_reverte           rt_reverte         (involução)
 *   M ↔ Mᵀ                    rt_transpoe          rt_transpoe        (involução)
 *
 * As três involuções da coluna da direita são o mesmo objecto que a Lei 1: espectro
 * {+1,−1}, e f∘f = id. Medir f∘f = id NELAS também não mede — o que mede é f ≠ id.
 * ═══════════════════════════════════════════════════════════════════════════════════ */

/* A CONJUGAÇÃO EM ℤ[√D]: a dobra b ↦ −b, e é a sua própria inversa. */
static void rt_zd_conj(long a, long b, long *ca, long *cb){ *ca = a; *cb = -b; }

/* A RAIZ k-ÉSIMA EXACTA: existe r inteiro com rᵏ = x? Busca binária, sem vírgula.
 * Devolve 1 e escreve r, ou 0 se não existe — e o 0 é metade do valor desta função. */
static int rt_raiz_k(long x, int k, long *r){
    if(k <= 0) return 0;
    if(k == 1){ if(r) *r = x; return 1; }
    if(x < 0){                                  /* ímpar tem raiz negativa; par não tem */
        if(k % 2 == 0) return 0;
        long rr;
        if(!rt_raiz_k(-x, k, &rr)) return 0;
        if(r) *r = -rr;
        return 1;
    }
    if(x < 2){ if(r) *r = x; return 1; }
    long lo = 1, hi = x;
    while(lo < hi){
        long mid = lo + (hi - lo + 1)/2;
        /* mid^k > x ? — com corte, para não transbordar ao elevar */
        long v = 1; int passou = 0;
        for(int t = 0; t < k; t++){
            if(mid != 0 && v > x / mid){ passou = 1; break; }
            v *= mid;
        }
        if(passou || v > x) hi = mid - 1; else lo = mid;
    }
    long v = 1;
    for(int t = 0; t < k; t++) v *= lo;
    if(v != x) return 0;
    if(r) *r = lo;
    return 1;
}

/* ── O TRIO DE b^k = n, E ELE ESTÁ FECHADO ──────────────────────────────────────────
 *
 * O Aarão: «a exponencial está na lib e a inversa também, daí tem logaritmo».
 *
 * E está: uma equação com três letras tem TRÊS perguntas, e a lib tem as três — cada uma
 * a inversa da potência por um lado diferente, e nenhuma a precisar de vírgula:
 *
 *      dado b, k  →  n     rt_ipow        a POTÊNCIA          (a ida)
 *      dado n, k  →  b     rt_raiz_k      a inversa pela BASE
 *      dado n, b  →  k     rt_log_int     a inversa pelo EXPOENTE — o LOGARITMO
 *
 * As duas inversas são PARCIAIS, e é isso que as torna medidas: devolvem 0 quando a
 * resposta não existe em ℤ, e esse 0 é metade do valor delas. `log(n)/log(b)` nunca
 * devolve 0 — devolve sempre um número, e é preciso uma régua para decidir se ele é
 * inteiro. Aqui a pergunta responde-se sozinha.
 *
 * E é por isso que `log(4^N)/log(3^N)` sai de cena: os N cancelam-se, a razão é a mesma
 * para todo N, e o que a pergunta queria — «4^N é potência de 3?» — é rt_log_int a
 * devolver 0. Ver o koch.c e o furos.c, onde as duas versões conviveram. */

/* O LOGARITMO INTEIRO: existe k com bᵏ = n? É a inversa da potência do lado do EXPOENTE,
 * e é o que substitui `log(n)/log(b)` quando a pergunta é sobre inteiros — sem dois
 * logaritmos, sem uma divisão, e sem a régua que a divisão de logaritmos obriga a
 * escolher. Devolve 1 e escreve k, ou 0 se n não é potência de b. */
static int rt_log_int(long n, long b, int *k){
    if(b < 2 || n < 1) return 0;
    long v = 1; int e = 0;
    while(v < n){
        if(v > n / b) return 0;                 /* o próximo passo já excede: não é potência */
        v *= b; e++;
    }
    if(v != n) return 0;
    if(k) *k = e;
    return 1;
}

/* ── O DUAL, E A INVERSÃO QUE SAI DELE ───────────────────────────────────────
 *
 * `thm:derivacao-primitivas` do Corpo Universal: as cinco operações — Soma, Multiplicação,
 * Divisão, Dual, Inversão — NÃO são independentes. O dual emparelha com cada uma:
 *
 *      M + M†  = tr M · I        o CENTRO
 *      M · M†  = det M · I       a MEMBRANA
 *      M⁻¹     = M† / det M      a INVERSÃO É A DIVISÃO DO DUAL
 *
 * «Cada operação, emparelhada com o dual, dá a sua parceira, e a parceira não entra na
 * lista.» Pela mesma conta a subtracção é a soma do dual — a − b = a ⊕ b† — e é por isso
 * que são CINCO e não sete.
 *
 * Por isso a inversa NÃO se escreve aqui: escreve-se o DUAL, e a inversa sai dele. Eu
 * tinha escrito `Inv[0] = M[3]/d` à mão, que é a adjunta sem lhe chamar o nome — e uma
 * operação escrita à mão onde havia uma derivação é a lista a crescer sem razão.
 *
 * E NA RECTA PROJECTIVA A INVERSÃO NEM DIVISÃO É (`def:lei0` do geométrico): é a TROCA
 * [p:q] ↦ [q:p], sem teste e sem ramo, e daí 0† = ∞. As duas leituras concordam — a troca
 * S = [[0,1],[1,0]] tem det = −1 e é a sua própria adjunta, logo S⁻¹ = −S, que em ℙ¹ é o
 * mesmo ponto a menos de escala. */

/* O DUAL de uma 2×2: a adjunta. É ele a primitiva; tudo o resto sai daqui. */
static void rt_adjunta2(const long *M, long *A){
    A[0] =  M[3]; A[1] = -M[1];
    A[2] = -M[2]; A[3] =  M[0];
}

/* A INVERSA, DERIVADA: M⁻¹ = M†/det M. Com |det| = 1 ela é INTEIRA — a condição da
 * unidade do `thm:cruzado-potencia`, e é por isso que a órbita metálica volta sem sair
 * de ℤ. Devolve 1, ou 0 quando |det| ≠ 1: aí a inversa existe, mas não neste corpo. */
static int rt_inversa2(const long *M, long *Inv){
    long d = M[0]*M[3] - M[1]*M[2];
    if(d != 1 && d != -1) return 0;
    rt_adjunta2(M, Inv);                        /* o DUAL */
    for(int i = 0; i < 4; i++) Inv[i] /= d;     /* dividido pelo determinante */
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════════════════════
 * A INDUÇÃO E A META-INDUÇÃO — a recursão formalizada, e sem casos especiais.
 *
 * `def:inducao` e `thm:meta-inducao` do Corpo Universal. O par é o de sempre:
 *
 *      INDUÇÃO       o passo SOBE: base em 0, e P(n) ⟹ P(n+1)      projecta (λ⁺)
 *      DESCIDA       o dual: nega-se a tese — «há um PRIMEIRO andar N onde P falha» —
 *                    e mostra-se que esse N não existe                    lê (λ⁻)
 *      META-INDUÇÃO  o passo SOBRE os passos: mede a indução e valida o que ela
 *                    conservou
 *
 * As duas primeiras usam o mesmo facto e nada mais: ℕ é bem ordenado. São duais no
 * sentido desta casa — a involução é inverter a ordem, e ν∘ν = id: a indução diz o que
 * HÁ no andar seguinte, a descida diz o que NÃO HÁ em andar nenhum, e é a mesma frase.
 *
 * ── E O QUE A META-INDUÇÃO MEDE É O PASSO ───────────────────────────────────────────
 *
 * Se o passo C(n) → C(n+1) é uma construção cujo corpo NÃO MENCIONA n, então ∀n C(n)
 * segue da FORMA do passo. O resultado não é C(0), …, C(N):
 *
 *      ∀ n ≥ 0,  C(n)
 *
 * e a ausência de tecto é consequência da forma da construção, não de uma varredura.
 *
 *      «Uma tabela de andares prova os andares da tabela.»
 *
 * É a frase que esta sessão andou a aprender à sua custa: a cláusula do supremo estava
 * verificada numa janela de quarenta valores com a asserção a dizer «todos», e o
 * conserto não foi alargar a janela — foi medir o PASSO e deixar a indução dar o resto.
 *
 * ── E A SATURAÇÃO NÃO É UM RESULTADO ────────────────────────────────────────────────
 *
 *      falha de representação  ≠  contra-exemplo matemático
 *
 * Se a realização finita transborda ao k-ésimo passo, mediu-se o tamanho do tipo e não a
 * lei. A regra que daí sai é obrigatória: quando a primeira realização atinge o limite,
 * verifica-se numa SEGUNDA independente, e a saturação conta-se em lugar SEPARADO dos
 * defeitos. (É por isso que `rt_det_mod` existe ao lado de `rt_det_bareiss`.)
 * ═══════════════════════════════════════════════════════════════════════════════════ */

/* a INDUÇÃO: verifica o PASSO P(n) ⟹ P(n+1) em toda a gama, e a base. Devolve 1 se o
 * passo nunca falha; se falhar, escreve em `onde` o primeiro n em que falhou.
 * O passo recebe n e um contexto opaco — o corpo dele é que não pode mencionar n para
 * a meta-indução valer, e isso é do chamador, não desta função. */
static int rt_induz(int (*base)(void*), int (*passo)(long, void*),
                    long ate, void *ctx, long *onde){
    if(onde) *onde = -1;
    if(base && !base(ctx)){ if(onde) *onde = 0; return 0; }
    for(long n = 0; n < ate; n++)
        if(!passo(n, ctx)){ if(onde) *onde = n; return 0; }
    return 1;
}

/* a DESCIDA: o dual. Procura o PRIMEIRO andar onde P falha e devolve-o, ou −1 se não
 * existe. É a mesma varredura da indução lida ao contrário — e o que a torna prova é
 * voltar VAZIA quando o passo é verdadeiro, com um controlo que mostra que ela sabe
 * achar quando há. */
static long rt_desce(int (*P)(long, void*), long ate, void *ctx){
    for(long n = 0; n <= ate; n++) if(!P(n, ctx)) return n;
    return -1;
}


/* ── O REAL COMO CAMINHO: a peça que tira a vírgula de vez ────────────────────────────
 *
 * O universal.tex, §sec:supremo: «o habitante É o caminho — não há valor por trás à
 * espera de ser aproximado, logo não há erro a acumular». Um real não se representa: as
 * suas DECISÕES é que se guardam, e cada uma é um bit.
 *
 *      m_k := max{ m ∈ ℤ : m/2^k < x para algum x ∈ S },   m_{k+1} ∈ {2m_k, 2m_k+1}
 *
 * A construção vivia inline no tests/supremo.c e é geral: qualquer real definido por um
 * predicado INTEIRO — «existe x em S acima de m/2^k?» — constrói-se assim, e o resultado
 * é um par (m, 2^k) sem uma vírgula. O supremo de {r² < 2} sai 1482910/2^20.
 *
 * E é este o caminho para os andares seguintes: a sequência de bits das decisões é, em
 * blocos de oito, exactamente um byte — o corpo256.h lê-o pelas suas oito coordenadas
 * (cor:bitdecide). */
static long rt_caminho_sup(int (*serve)(long m, long pot, void *ctx), int k,
                           void *ctx, long *pot_out){
    long m = 0, pot = 1;
    while(serve(m + 1, 1, ctx)) m++;                  /* m_0, subindo do zero */
    for(int i = 1; i <= k; i++){
        m = 2*m; pot *= 2;                            /* desce um nível da árvore */
        if(serve(m + 1, pot, ctx)) m++;               /* o filho direito ainda serve? */
    }
    if(pot_out) *pot_out = pot;
    return m;
}

/* O CORTE SEM RAIZ — thm:corte do universal.tex. Para D = m²+4 e σ = (m+√D)/2:
 *
 *      a/b < σ  ⟺  2a − mb < b√D
 *
 * e como o lado direito é positivo, parte-se em dois SEM PERDA e sem formar a raiz:
 * ou 2a − mb é negativo (e acabou), ou é não negativo e (2a−mb)² < b²D. Devolve 1 se
 * a/b < σ. Exige b ≥ 1. */
/* A COMPARAÇÃO GERAL: p/q  <  (A + B·√D)/E ?   Sem formar a raiz, e é a mesma partição
 * do thm:corte um passo mais geral. Escreve-se
 *
 *      p·E − q·A  <  q·B·√D
 *
 * e decide-se pelo SINAL dos dois lados antes de elevar ao quadrado, porque elevar troca
 * a ordem nos negativos — é aí que uma comparação «óbvia» se estraga:
 *
 *      esq < 0 e dir ≥ 0   →  sim, e acabou
 *      esq ≥ 0 e dir < 0   →  não
 *      ambos ≥ 0           →  esq² < dir²·D
 *      ambos < 0           →  esq² > dir²·D      (a ordem inverte-se)
 *
 * Exige q > 0 e E > 0. Devolve 1 se p/q é menor. */
static int rt_menor_que_zd(long p, long q, long A, long B, long E, long D){
    long esq = p*E - q*A, dir = q*B;
    if(esq < 0 && dir >= 0) return 1;
    if(esq >= 0 && dir < 0) return 0;
    if(esq >= 0) return esq*esq < dir*dir*D;
    return esq*esq > dir*dir*D;
}

static int rt_menor_que_sigma(long a, long b, long m, long D){
    long e = 2*a - m*b;
    if(e < 0) return 1;
    return e*e < b*b*D;
}

/* ═══════════════════════════════════════════════════════════════════════════════════
 * A CODIFICAÇÃO, PROMOVIDA — e a verificação da volta, AUTOMÁTICA
 *
 * O pipe da entrega é `textos → unidade (MMC) → inteiros → operar → PALAVRA → cliente`, e
 * a palavra é a fracção contínua. Mas ela não é a única codificação exacta do racional: o
 * SHIFT de Cantor — os dígitos numa base, com pré-período e período — codifica o mesmo
 * objecto, e o `thm:cantor-julia` diz o que as separa (uma gasta o denominador e termina,
 * a outra não gasta e cicla).
 *
 * As duas ficam aqui, com a MESMA assinatura, e por cima delas fica o que interessa:
 *
 *      UMA CODIFICAÇÃO SÓ É UMA CODIFICAÇÃO SE A VOLTA FECHAR.
 *
 * `rt_volta_fecha` varre os racionais reduzidos de um domínio, manda cada um pela ida e
 * volta, e conta. Não é um teste escrito à mão para uma codificação: é a condição que
 * QUALQUER codificação tem de cumprir, aplicada por máquina. Acrescentar uma codificação
 * nova é escrever uma função com esta assinatura e passá-la aqui.
 * ═══════════════════════════════════════════════════════════════════════════════════ */

#define RT_COD_MAX 64

/* O SHIFT DE CANTOR: os dígitos de p/q na base b. O estado é o RESTO, e ele vive em
 * {0,…,q−1} — finito, logo a sequência de restos REPETE, e é isso que dá o período. */
typedef struct {
    long base;
    long d[RT_COD_MAX];      /* os dígitos, pré-período seguido do período */
    int  n;                  /* quantos ao todo                            */
    int  pre;                /* d[0 .. pre−1] não repetem                  */
    int  per;                /* d[pre .. pre+per−1] repetem para sempre    */
    long inteiro;            /* a parte antes da vírgula                   */
    int  saturou;            /* o que não coube, e conta-se                */
} RtCod;

static int rt_cod_shift(long p, long q, long base, RtCod *c){
    if(q <= 0 || base < 2) return 0;
    c->base = base; c->n = 0; c->pre = 0; c->per = 0; c->saturou = 0;
    c->inteiro = p / q;
    long r = p % q;
    if(r < 0){ r += q; c->inteiro -= 1; }
    long visto_r[RT_COD_MAX]; int nv = 0;
    while(r != 0){
        /* já vimos este resto? então o ciclo fecha aqui */
        for(int i = 0; i < nv; i++) if(visto_r[i] == r){
            c->pre = i; c->per = nv - i; c->n = nv; return 1;
        }
        if(nv >= RT_COD_MAX){ c->saturou = 1; c->n = nv; c->pre = nv; c->per = 0; return 0; }
        visto_r[nv] = r;
        if(r > 4611686018427387903L / base){ c->saturou = 1; return 0; }
        long t = r * base;
        c->d[nv] = t / q;
        r = t % q;
        nv++;
    }
    c->n = nv; c->pre = nv; c->per = 0;      /* terminou: dígitos finitos, sem período */
    return 1;
}

/* A VOLTA: dos dígitos ao racional, exacta e inteira. A parte não periódica vale
 * D_pre/base^pre; a periódica vale D_per/((base^per − 1)·base^pre). Devolve 0 se algum
 * produto não couber — e é isso, e não um limiar, o que decide se a volta se pode fazer. */
static int rt_desc_shift(const RtCod *c, long *p, long *q){
    if(c->saturou) return 0;
    long base = c->base;
    long bp = 1;                                    /* base^pre  */
    for(int i = 0; i < c->pre; i++){
        if(bp > 4611686018427387903L / base) return 0;
        bp *= base;
    }
    long dpre = 0;                                  /* o número formado pelos pré-dígitos */
    for(int i = 0; i < c->pre; i++){
        if(dpre > 4611686018427387903L / base) return 0;
        dpre = dpre*base + c->d[i];
    }
    if(c->per == 0){                                /* finito: p/q = inteiro + dpre/bp */
        long num = c->inteiro * bp + dpre;
        *p = num; *q = bp;
        return 1;
    }
    long bq = 1;                                    /* base^per */
    for(int i = 0; i < c->per; i++){
        if(bq > 4611686018427387903L / base) return 0;
        bq *= base;
    }
    long dper = 0;
    for(int i = 0; i < c->per; i++){
        if(dper > 4611686018427387903L / base) return 0;
        dper = dper*base + c->d[c->pre + i];
    }
    /* p/q = inteiro + dpre/bp + dper/((bq−1)·bp) */
    long den1 = bq - 1;
    if(bp != 0 && den1 > 4611686018427387903L / bp) return 0;
    long den = bp * den1;
    long num = c->inteiro;
    if(num != 0 && (num > 4611686018427387903L / den || num < -4611686018427387903L / den))
        return 0;
    num = num * den + dpre * den1 + dper;
    *p = num; *q = den;
    return 1;
}

/* ── A VERIFICAÇÃO AUTOMÁTICA ────────────────────────────────────────────────────────
 * Uma ida-e-volta devolve 1 e o par reconstruído, ou 0 se não coube. `rt_volta_fecha`
 * varre os racionais REDUZIDOS com denominador até qmax, chama-a, e compara em ℙ¹ (por
 * produto cruzado, sem dividir). Devolve quantos fecharam; escreve em `*total` quantos
 * foram tentados e em `*fora` quantos não couberam — que se contam à parte, porque não
 * caber não é falhar. */
typedef int (*RtIdaVolta)(long p, long q, long *rp, long *rq);

static long rt_volta_fecha(RtIdaVolta f, long qmax, long *total, long *fora){
    long ok = 0, tot = 0, nc = 0;
    for(long q = 1; q <= qmax; q++)
        for(long p = 0; p <= q; p++){
            if(rt_mdc(p, q) != 1 && !(p == 0 && q == 1)) continue;
            tot++;
            long rp = 0, rq = 0;
            if(!f(p, q, &rp, &rq)){ nc++; continue; }
            if(rq != 0 && p*rq == rp*q) ok++;        /* igualdade em ℙ¹, sem dividir */
        }
    if(total) *total = tot;
    if(fora)  *fora  = nc;
    return ok;
}

/* as três codificações da casa, com a MESMA assinatura — é isso que as torna comparáveis */
static int rt_iv_palavra(long p, long q, long *rp, long *rq){
    RtCf w; rt_cf_de(1, p, q, &w);
    if(w.saturou) return 0;
    return rt_cf_para(&w, rp, rq);
}
static int rt_iv_shift(long p, long q, long base, long *rp, long *rq){
    RtCod c;
    if(!rt_cod_shift(p, q, base, &c)) return 0;
    return rt_desc_shift(&c, rp, rq);
}
static int rt_iv_shift2 (long p, long q, long *rp, long *rq){ return rt_iv_shift(p,q, 2,rp,rq); }
static int rt_iv_shift10(long p, long q, long *rp, long *rq){ return rt_iv_shift(p,q,10,rp,rq); }

/* ═══════════════════════════════════════════════════════════════════════════════════
 * A ORDEM DE UM OPERADOR — e é ela que identifica a lei
 *
 * O `thm:unificacao` diz que Cantor, Julia, Viviani e o trial não são quatro objectos
 * parecidos: são quatro operadores de ORDEM FINITA, e a ordem de cada um é o fecho da sua
 * lei no catálogo (2, 3, 4, 8). A peça que mede isso vivia num medidor; passa para aqui,
 * porque é dela que o PIPE precisa.
 *
 * NO PIPE ela responde à pergunta que decide a codificação: `textos → unidade → inteiros →
 * operar → PALAVRA → cliente` — e a palavra ou FECHA ou CICLA. Qual dos dois, e ao fim de
 * quantos passos, é exactamente a ORDEM do operador que a escreve:
 *
 *      a Möbius   gasta o denominador   →  ordem finita no racional, e a palavra FECHA
 *      o shift    não o gasta           →  período = ord_q(base), e a palavra CICLA
 *
 * `rt_ordem` mede a primeira; `rt_periodo_shift` mede a segunda. E as duas devolvem 0
 * quando não fecham dentro do tecto — que é o que distingue «tem ordem finita» de «ainda
 * não vi voltar», e não se pode confundir com ordem 0.
 * ═══════════════════════════════════════════════════════════════════════════════════ */

/* um passo actua sobre um estado de n inteiros, em-lugar */
typedef void (*RtPasso)(long *estado, int n);

/* A ORDEM: aplica-se o passo até o estado voltar ao inicial, e devolve-se o PRIMEIRO k.
 * A minimalidade é a tese: τ⁶ = id também é verdade e não faz do trial ordem 6. */
static int rt_ordem(RtPasso f, const long *e0, int n, int tecto){
    long e[16], ini[16];
    if(n > 16) return 0;
    for(int i = 0; i < n; i++){ e[i] = e0[i]; ini[i] = e0[i]; }
    for(int k = 1; k <= tecto; k++){
        f(e, n);
        int igual = 1;
        for(int i = 0; i < n; i++) if(e[i] != ini[i]) igual = 0;
        if(igual) return k;
    }
    return 0;                                  /* não fechou dentro do tecto — e diz-se */
}

/* O PERÍODO DO SHIFT de p/q na base b: o estado é o RESTO, e ele vive em {0,…,q−1}. Com
 * mdc(b,q) = 1 o shift é invertível e o período é a ordem de b em (ℤ/q)*; sem isso o
 * denominador encolhe primeiro (thm:cantor-julia), e o que se devolve é o período do que
 * sobra. Devolve 0 se o racional for inteiro (resto zero: não há palavra a ciclar).
 *
 * E A REDUÇÃO PELO MDC É UMA GUARDA REDUNDANTE — dito, porque foi medido. Apontei-lhe um
 * gume (tirar a redução) e ele NÃO mordeu, nem depois de a varredura passar a incluir
 * fracções não reduzidas. A razão é estrutural: o shift sobre os múltiplos de g em ℤ/q é
 * isomorfo ao shift em ℤ/(q/g), logo o período é o mesmo com ou sem reduzir. Fica porque
 * torna o estado canónico — não porque mude a resposta. Uma mutação que sobrevive é ou um
 * gap meu ou uma guarda que está certa, e esta é do segundo tipo. */
static int rt_periodo_shift(long p, long q, long base, int tecto){
    if(q <= 0 || base < 2) return 0;
    long g = rt_mdc(p < 0 ? -p : p, q); if(g < 1) g = 1;
    long qr = q / g, pr = (p < 0 ? -p : p) / g;
    long r0 = pr % qr;
    if(r0 == 0) return 0;
    long r = r0;
    for(int k = 1; k <= tecto; k++){
        r = (r * base) % qr;
        if(r == r0) return k;
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════════
 * O CICLO UNIVERSAL, PROMOVIDO — descodifica, opera, inverte, codifica
 *
 * O `thm:universal` diz que tudo cabe num ciclo, e que o que o faz FECHAR é |det| = 1.
 * Estava escrito num medidor; passa para aqui, porque é a arquitectura do pipe e não uma
 * medida sobre ele. Quem converte um medidor deixa de o reescrever: chama a peça.
 *
 *      DESCODIFICA   o texto é um racional exacto        rt_le_decimal / rt_unidade_comum
 *      OPERA         [p:q] ↦ T·[p:q], sem dividir        rt_opera
 *      INVERTE       T⁻¹ = adj(T)/det T, INTEIRA         rt_inverte   ← exige |det| = 1
 *      CODIFICA      a palavra ou os dígitos             rt_cf_de / rt_cod_shift
 *
 * O TORO é a condição, não um passo: sem |det T| = 1 a inversa não é inteira e o ciclo
 * não fecha — `rt_inverte` RECUSA, e recusar é dizer, não truncar.
 * ═══════════════════════════════════════════════════════════════════════════════════ */

typedef struct { int16_t T[4]; } RtOp;     /* SL(2,ℤ): entradas em E₁₆; (p,q) ainda long */

static int32_t rt_op_det_i32(const RtOp *o){
    return (int32_t)o->T[0]*o->T[3] - (int32_t)o->T[1]*o->T[2];
}

static long rt_op_det(const RtOp *o){
    return rt_op_det_i32(o);
}

/* o operador serve ao ciclo exactamente quando |det| = 1 — é a condição do toro */
static int rt_op_valido(const RtOp *o){ int32_t d = rt_op_det_i32(o); return d == 1 || d == -1; }

/* OPERA: em ℙ¹, sem dividir — coordenadas (p,q) em long; produtos via int32 implícito */
static void rt_opera(const RtOp *o, long p, long q, long *rp, long *rq){
    long a = (long)o->T[0]*p + (long)o->T[1]*q;
    long b = (long)o->T[2]*p + (long)o->T[3]*q;
    *rp = a; *rq = b;
}

/* INVERTE: a metade dual. adj(T) = [d −b; −c a], e dividir por ±1 é multiplicar por ±1 —
 * é isso que a torna INTEIRA. Devolve 0 quando o operador não serve. */
static int rt_inverte(const RtOp *o, long p, long q, long *rp, long *rq){
    long d = rt_op_det(o);
    if(d != 1 && d != -1) return 0;
    RtOp inv = {{
        (int16_t)(o->T[3]/d), (int16_t)(-o->T[1]/d),
        (int16_t)(-o->T[2]/d), (int16_t)(o->T[0]/d)
    }};
    rt_opera(&inv, p, q, rp, rq);
    return 1;
}

/* OPERA/INVERTE em int32 — coordenadas pequenas do ciclo (Fase B/D); entradas T em int16 */
static void rt_opera_i32(const RtOp *o, int32_t p, int32_t q, int32_t *rp, int32_t *rq){
    *rp = (int32_t)o->T[0]*p + (int32_t)o->T[1]*q;
    *rq = (int32_t)o->T[2]*p + (int32_t)o->T[3]*q;
}
static int rt_inverte_i32(const RtOp *o, int32_t p, int32_t q, int32_t *rp, int32_t *rq){
    int32_t d = rt_op_det_i32(o);
    if(d != 1 && d != -1) return 0;
    RtOp inv = {{
        (int16_t)(o->T[3]/d), (int16_t)(-o->T[1]/d),
        (int16_t)(-o->T[2]/d), (int16_t)(o->T[0]/d)
    }};
    rt_opera_i32(&inv, p, q, rp, rq);
    return 1;
}

/* a ORDEM do operador em int32 — mesma semântica que rt_ordem_* (Fase B) */
static int rt_ordem_ponto_i32(const RtOp *o, int32_t p, int32_t q, int tecto){
    int32_t P = p, Q = q;
    for(int k = 1; k <= tecto; k++){
        int32_t np, nq; rt_opera_i32(o, P, Q, &np, &nq); P = np; Q = nq;
        if(Q != 0 && (int64_t)p*Q == (int64_t)P*q) return k;
        if(P == 0 && Q == 0) return 0;
    }
    return 0;
}
static int rt_ordem_vector_i32(const RtOp *o, int32_t p, int32_t q, int tecto){
    int32_t P = p, Q = q;
    for(int k = 1; k <= tecto; k++){
        int32_t np, nq; rt_opera_i32(o, P, Q, &np, &nq); P = np; Q = nq;
        if(P == p && Q == q) return k;
    }
    return 0;
}

/* OPERA/INVERTE em int16 — coordenadas em E₁₆; acumula int32 (Fase D) */
static void rt_opera_i16(const RtOp *o, int16_t p, int16_t q, int16_t *rp, int16_t *rq){
    *rp = (int16_t)((int32_t)o->T[0]*p + (int32_t)o->T[1]*q);
    *rq = (int16_t)((int32_t)o->T[2]*p + (int32_t)o->T[3]*q);
}
static int rt_inverte_i16(const RtOp *o, int16_t p, int16_t q, int16_t *rp, int16_t *rq){
    int32_t d = rt_op_det_i32(o);
    if(d != 1 && d != -1) return 0;
    RtOp inv = {{
        (int16_t)(o->T[3]/d), (int16_t)(-o->T[1]/d),
        (int16_t)(-o->T[2]/d), (int16_t)(o->T[0]/d)
    }};
    rt_opera_i16(&inv, p, q, rp, rq);
    return 1;
}
static int rt_ordem_ponto_i16(const RtOp *o, int16_t p, int16_t q, int tecto){
    int16_t P = p, Q = q;
    for(int k = 1; k <= tecto; k++){
        int16_t np, nq; rt_opera_i16(o, P, Q, &np, &nq); P = np; Q = nq;
        if(Q != 0 && (int32_t)p*Q == (int32_t)P*q) return k;
        if(P == 0 && Q == 0) return 0;
    }
    return 0;
}
static int rt_ordem_vector_i16(const RtOp *o, int16_t p, int16_t q, int tecto){
    int16_t P = p, Q = q;
    for(int k = 1; k <= tecto; k++){
        int16_t np, nq; rt_opera_i16(o, P, Q, &np, &nq); P = np; Q = nq;
        if(P == p && Q == q) return k;
    }
    return 0;
}

/* a ORDEM do operador, e são DUAS: no PONTO (ℙ¹, onde v e −v são o mesmo) e no VECTOR
 * (ℤ²). Viviani tem 2 e 4 — e é essa razão que é o recobrimento duplo. Devolvem 0 quando
 * não fecham dentro do tecto, que é o caso do gato. */
static int rt_ordem_ponto(const RtOp *o, long p, long q, int tecto){
    long P = p, Q = q;
    for(int k = 1; k <= tecto; k++){
        long np, nq; rt_opera(o, P, Q, &np, &nq); P = np; Q = nq;
        if(Q != 0 && p*Q == P*q) return k;
        if(P == 0 && Q == 0) return 0;
    }
    return 0;
}
static int rt_ordem_vector(const RtOp *o, long p, long q, int tecto){
    long P = p, Q = q;
    for(int k = 1; k <= tecto; k++){
        long np, nq; rt_opera(o, P, Q, &np, &nq); P = np; Q = nq;
        if(P == p && Q == q) return k;
    }
    return 0;
}

/* O CICLO INTEIRO: opera, inverte, e passa pela codificação escolhida. Devolve 1 e o par
 * de volta; 0 se o operador não serve ou se a codificação não representa. A comparação
 * com o original faz-se em ℙ¹ por produto cruzado — quem chama é que a faz, porque é ela
 * a tese e não o ciclo. */
/* ── O ALGORITMO UNIVERSAL, nos quatro passos ────────────────────────────────────────
 *
 * Ele corre no ESPAÇO da def:espaco — V = Q ⊕ Q·m√D, dim 2 — e no seu DUAL, cuja régua é
 * m† = −1/(mD). São QUATRO passos e não cinco: não há verificação, porque não há nada a
 * verificar — com |det| = 1 a adjunta é inteira e a volta é exacta POR CONSTRUÇÃO.
 *
 *      DESCODIFICA   o texto entra como (p,q) EXACTO      → coordenadas em V
 *      OPERA         a acção de T, |det T| = 1            → produto ponto a ponto no dual
 *      INVERTE       adj(T)/det T, inteira                → a deconvolução, pela NORMA
 *      CODIFICA      palavra (FC) ou dígitos (Cantor)     → sai
 *
 * e FECHA: descodifica(w) = (p,q). O que o faz fechar é |det| = 1 — a conservação e a
 * volta são a mesma frase. E o sentido é o par cone/espiral: o CONE desce em passo
 * discreto, a ESPIRAL sobe recompondo, e o produto das duas réguas é conservado. */
static int rt_ciclo(const RtOp *o, RtIdaVolta cod, long p, long q, long *rp, long *rq){
    long a, b, c, d;
    if(!rt_op_valido(o)) return 0;
    rt_opera(o, p, q, &a, &b);
    if(!rt_inverte(o, a, b, &c, &d)) return 0;
    if(d == 0) return 0;                          /* a órbita passou por ∞ */
    /* NÃO SE REDUZ. O primeiro passo da transformada já normaliza: a avaliação é linear
     * e o que se usa é a RAZÃO das folhas, onde o factor cancela — medido em
     * `transformada_universal.c` §T9, e §T10: quem normaliza é a NORMA, que é o produto
     * das folhas, e um mdc por cima não acrescenta nada. Medido também pelo lado de fora:
     * com esta redução neutralizada, os 71 medidores que usam esta lib passam todos. */
    long ri = c, rj = d;
    if(rj < 0){ ri = -ri; rj = -rj; }
    if(ri < 0) return 0;                          /* as codificações são de p/q >= 0 */
    if(!cod) { *rp = ri; *rq = rj; return 1; }
    return cod(ri, rj, rp, rq);
}

/* ciclo universal em int32 — opera/inverte via rt_opera_i32; codificação ainda long (FC) */
static int rt_ciclo_i32(const RtOp *o, RtIdaVolta cod, int32_t p, int32_t q,
                        int32_t *rp, int32_t *rq){
    int32_t a, b, c, d;
    if(!rt_op_valido(o)) return 0;
    rt_opera_i32(o, p, q, &a, &b);
    if(!rt_inverte_i32(o, a, b, &c, &d)) return 0;
    if(d == 0) return 0;
    int32_t ri = c, rj = d;
    if(rj < 0){ ri = -ri; rj = -rj; }
    if(ri < 0) return 0;
    if(!cod){
        if(rp) *rp = ri;
        if(rq) *rq = rj;
        return 1;
    }
    long lr, lq;
    if(!cod((long)ri, (long)rj, &lr, &lq)) return 0;
    if(lr != (int32_t)lr || lq != (int32_t)lq) return 0;
    if(rp) *rp = (int32_t)lr;
    if(rq) *rq = (int32_t)lq;
    return 1;
}

/* ciclo universal em int16 — opera/inverte via rt_opera_i16 */
static int rt_ciclo_i16(const RtOp *o, RtIdaVolta cod, int16_t p, int16_t q,
                        int16_t *rp, int16_t *rq){
    int16_t a, b, c, d;
    if(!rt_op_valido(o)) return 0;
    rt_opera_i16(o, p, q, &a, &b);
    if(!rt_inverte_i16(o, a, b, &c, &d)) return 0;
    if(d == 0) return 0;
    int16_t ri = c, rj = d;
    if(rj < 0){ ri = (int16_t)-ri; rj = (int16_t)-rj; }
    if(ri < 0) return 0;
    if(!cod){
        if(rp) *rp = ri;
        if(rq) *rq = rj;
        return 1;
    }
    long lr, lq;
    if(!cod((long)ri, (long)rj, &lr, &lq)) return 0;
    if(lr != (int16_t)lr || lq != (int16_t)lq) return 0;
    if(rp) *rp = (int16_t)lr;
    if(rq) *rq = (int16_t)lq;
    return 1;
}

/* ── DIR e CRUZ dos operadores, e o que CRUZ mede ────────────────────────────────────
 *
 * A lib já tem `rt_dir` e `rt_cruz` — as duas leituras sobre VECTORES. Estas são sobre
 * OPERADORES, que é outro objecto, e por isso levam o sufixo: o objecto aqui é o par [p:q]
 * de ℙ¹ e a operação é a acção de MATRIZES (def:lei0). As duas leituras do §sec:operacao
 * lêem-se ali:
 *
 *      Dir(A,B)  = ½(AB + BA)      a leitura que NÃO MUDA ao trocar a ordem
 *      Cruz(A,B) = ½(AB − BA)      a que TROCA DE SINAL
 *
 * Guardam-se em DOBRO, para não dividir — `rt_dir_op` e `rt_cruz_op` devolvem AB±BA.
 *
 * E o que CRUZ mede é a obstrução à DIAGONALIZAÇÃO SIMULTÂNEA (cor:dir-cruz-folhas):
 * com o determinante fixo, o traço determina as folhas, e
 *
 *      Cruz = 0  ⟺  mesmo traço  ⟺  MESMAS FOLHAS  ⟺  uma avaliação diagonaliza os DOIS
 *
 * É esta a condição que autoriza o passo OPERA do ciclo a fazer-se na transformada: a
 * convolução só vira produto ponto a ponto quando o par tem espectro comum. */
static void rt_dir_op(const RtOp *A, const RtOp *B, long *R){
    long AB[4], BA[4];
    AB[0]=(long)A->T[0]*B->T[0]+(long)A->T[1]*B->T[2]; AB[1]=(long)A->T[0]*B->T[1]+(long)A->T[1]*B->T[3];
    AB[2]=(long)A->T[2]*B->T[0]+(long)A->T[3]*B->T[2]; AB[3]=(long)A->T[2]*B->T[1]+(long)A->T[3]*B->T[3];
    BA[0]=(long)B->T[0]*A->T[0]+(long)B->T[1]*A->T[2]; BA[1]=(long)B->T[0]*A->T[1]+(long)B->T[1]*A->T[3];
    BA[2]=(long)B->T[2]*A->T[0]+(long)B->T[3]*A->T[2]; BA[3]=(long)B->T[2]*A->T[1]+(long)B->T[3]*A->T[3];
    for(int i = 0; i < 4; i++) R[i] = AB[i] + BA[i];
}
static void rt_cruz_op(const RtOp *A, const RtOp *B, long *R){
    long AB[4], BA[4];
    AB[0]=(long)A->T[0]*B->T[0]+(long)A->T[1]*B->T[2]; AB[1]=(long)A->T[0]*B->T[1]+(long)A->T[1]*B->T[3];
    AB[2]=(long)A->T[2]*B->T[0]+(long)A->T[3]*B->T[2]; AB[3]=(long)A->T[2]*B->T[1]+(long)A->T[3]*B->T[3];
    BA[0]=(long)B->T[0]*A->T[0]+(long)B->T[1]*A->T[2]; BA[1]=(long)B->T[0]*A->T[1]+(long)B->T[1]*A->T[3];
    BA[2]=(long)B->T[2]*A->T[0]+(long)B->T[3]*A->T[2]; BA[3]=(long)B->T[2]*A->T[1]+(long)B->T[3]*A->T[3];
    for(int i = 0; i < 4; i++) R[i] = AB[i] - BA[i];
}

/* o par tem ESPECTRO COMUM? — isto é, uma avaliação diagonaliza os dois. Devolve 1 quando
 * Cruz é nula, que é a condição que o passo OPERA precisa. */
static int rt_espectro_comum(const RtOp *A, const RtOp *B){
    long C[4]; rt_cruz_op(A, B, C);
    return C[0] == 0 && C[1] == 0 && C[2] == 0 && C[3] == 0;
}

/* ── A DOURADA DISCRETA: a dourada NA BORDA (thm:dourada-discreta) ───────────────────
 *
 * A dourada é Mellin — os caracteres do grupo MULTIPLICATIVO são as POTÊNCIAS, e dilatar
 * vira transladar. O que a torna DISCRETA é a BORDA: com σ² = mσ + 1, a folha σ é uma
 * unidade e em F_p tem ORDEM MULTIPLICATIVA finita N, logo os caracteres são as N
 * potências σ^k. Daí:
 *
 *      G(x)_k = Σ_j x_j · σ^{jk}        e        x_j = N⁻¹ Σ_k G(x)_k · σ^{−jk}
 *
 * e a volta corre pela RÉGUA DUAL, porque σσ† = −1 dá σ⁻¹ = −σ†. O factor é N e NÃO √N:
 * a directa junta, a inversa divide por N, e não há metades a repartir.
 *
 * Nada aqui usa raízes da unidade — esse é o lado ADITIVO da dualidade, e o nosso é o
 * MULTIPLICATIVO (as folhas são recíprocas, não estão no círculo). */

/* a folha da borda σ² = mσ + 1 em F_p, ou −1 se ela lá não vive */
static long rt_folha_borda(long m, long p){
    for(long x = 1; x < p; x++)
        if((((x*x - m*x - 1) % p) + p) % p == 0) return x;
    return -1;
}
/* a ORDEM multiplicativa de σ — VERIFICADA por iteração, não suposta; 0 se não fechar */
static long rt_ordem_mult(long sg, long p){
    if(sg <= 0 || sg >= p) return 0;
    long e = 1;
    for(long k = 1; k < p; k++){ e = e * sg % p; if(e == 1) return k; }
    return 0;
}
/* a DOURADA DISCRETA directa: X[k] = Σ_j x[j]·σ^{jk} em F_p */
static void rt_dourada(const long *x, long n, long sg, long p, long *X){
    for(long k = 0; k < n; k++){
        long acc = 0, w = 1, wk = 1;
        for(long q = 0; q < k; q++) wk = wk * sg % p;
        for(long j = 0; j < n; j++){ acc = (acc + x[j] % p * w) % p; w = w * wk % p; }
        X[k] = (acc + p) % p;
    }
}
/* e a INVERSA, pela outra folha: σ⁻¹ = −σ†. Devolve 0 se N não for invertível em F_p. */
static int rt_dourada_inv(const long *X, long n, long sg, long p, long *x){
    long inv_sg = rt_inv_mod(sg, p);          /* a lib já tinha o inverso por Fermat */
    long invN   = rt_inv_mod(n % p, p);
    if(!inv_sg || !invN) return 0;
    for(long j = 0; j < n; j++){
        long acc = 0, w = 1, wj = 1;
        for(long q = 0; q < j; q++) wj = wj * inv_sg % p;
        for(long k = 0; k < n; k++){ acc = (acc + X[k] % p * w) % p; w = w * wj % p; }
        x[j] = acc * invN % p;
    }
    return 1;
}

#endif /* RETA_H */
