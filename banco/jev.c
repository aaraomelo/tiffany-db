/* banco/jev_core.c — as cinco leituras do JEV dentro da biblioteca do produto.
 *
 * ── PROVENIENCIA ──────────────────────────────────────────────────────────────
 *   fonte       conecthus/backends/wasm/jev/marginal.c
 *   blob        20e64e626ca923d2cddb5d3dcdff20d6b3dbb16b
 *   sha256      d1d5bf8538d221bcc486e31d7b8dccfcb047a0c51891d00987b3de85b048b1e4
 *   tamanho     8224 bytes, 222 linhas, 5 funcoes
 *   tradução    ESTE FICHEIRO FOI GERADO, NÃO ESCRITO A MAO. Copia-se o original
 *               e aplicam-se SETE alterações, todas em linhas de declaração ou de
 *               assinatura — nenhuma dentro do corpo de uma função:
 *
 *                 (1) #include "jev_core.h"                    acrescentado no topo
 *                 (2) unsigned char arena[65536];             -> static, alinhada
 *                 (3) int proj(int b, int c, int x){          -> jev_proj_raw
 *                 (4) int marginal(int X){                    -> jev_marginal_raw
 *                 (5) int escore(int X){                      -> jev_escore_raw
 *                 (6) int duas(int X){                        -> jev_duas_raw
 *                 (7) int marginal_d(int X){                  -> jev_marginal_d_raw
 *
 *               Os corpos chamadores continuam a escrever `proj(...)`, com o nome
 *               e os argumentos intactos: e o INVÓLUCRO no fim do ficheiro que
 *               ganha o sufixo _raw e passa a ser o servo do original.
 *
 *   porque o sufixo _raw. Um corpo só pode chamar o que o precede, e a guarda de
 *               proj tem de estar ACIMA de proj sem estar dentro de proj. Dar o
 *               nome a invocador em vez do invocado é o que mantém os cinco
 *               corpos byte-a-byte iguais aos do original.
 *
 *   prova       96 casos E1/E2/E3/E4/E5/E5-isolado/E7a/E7b, 65536 bytes de arena
 *               por caso: 6291456 bytes comparados contra o marginal.wasm original
 *               com ZERO diferencas. Ver test_jev_equiv.c.
 *
 * ── O QUE ESTE FICHEIRO NÃO E ────────────────────────────────────────────────
 *   Não é uma reescrita. Não há "melhoria", não há numeração nova, não há outro
 *   algoritmo. Onde o motor WASM era exacto, aqui é exacto; o que muda é que o
 *   motor devolve 1 para 0 e que uma projeção fora de classe é recusada.
 *
 * ── ALINHAMENTO ───────────────────────────────────────────────────────────────
 *   A arena e `unsigned char[65536]`, cujo alinhamento declarado é 1, e é lida
 *   como `int *`. Esse reinterpretação e UB portátil e o compilador não a
 *   denuncia: gcc -Wall -Wextra compila isto sem um único aviso. Em x86-64 a
 *   linkedação entrega a arena múltipla de 4 — medido, 0 mod 4 e 0 mod 8 — e por
 *   isso funciona. Numa outra arquitectura ou noutro linker pode não funcionar.
 *   Por isso a declaração pede 4 bytes. E uma correccao que não toca em nenhum
 *   corpo e não muda um único valor de saída.
 */
#include "jev_core.h"

/* A occurrencia de uma projeção fora de classe. Vive aqui, antes de tudo, porque
 * o invólucro de proj a escreve e as portas a leem. */
static int jev_bad;

/* Declaracao adiantada do servo: os quatro corpos chamadores escrevem `proj(...)`
 * e não podem ser mexidos, logo o servo tem de existir antes deles. A DEFINIÇÃO
 * está no fim do ficheiro, logo abaixo da última função original. */
static int proj(int b, int c, int x);

/* conecthus/backends/wasm/jev/marginal.c — JEV no motor (piloto E1–E5 + E7).
 * E1: o campo de contagem G(x) = |{i : pi(i) = x}| conserva |I|.
 * E2: as marginais da conjunta igualam as diretas (teorema das marginais do jev.tex),
 *     e cada projeção coincide com o oráculo (selo WASM ≡ oráculo).
 * E3: três leituras tipadas (Noul/Choice/Score) sobre o mesmo G — composicao.
 * E4: Score fracionario como par racional exato (N, |I|), sem divisão no motor.
 * E5: Noul e Score no mesmo percurso sobre G (um disco, duas roupas); a
 *     segunda leitura não recria G (função duas).
 * E7: a ponte com o motor dimensional — marginal_d() le as projecoes como
 *     COORDENADAS de X_d (dt=Xd, sec:byte-carry), dadas como dados, e com a
 *     coordenada nova a ZERO (o residuo de iota_d) a leitura 2D devolve a 1D.
 * Particoes por fronteiras na arena: só comparacoes — o traduz não recebe
 * div/mod variavel; o odometro dispensa divisão ao ler a conjunta em ordem.
 *
 * Arena (int32 a partir do byte 8):
 *   [0, X)      G(x)
 *   X           |I|
 *   X+1         n  (dimensoes tratadas; extras viram 1 classe)
 *   P = X+2     c0 c1 c2
 *   B = P+3     fronteiras: 3 blocos contiguos de (c_i+1): b_0..b_c (= X)
 *   D1 = B+S    marginais diretas, linha p em 16*p
 *   J = D1+48   conjunta em ordem odometro (última coordenada mais rapida)
 *   D2 = J+PROD marginais da conjunta, mesma linha em 16*p
 *   OUT = D2+48 OUT[0]=total G do campo; OUT[1]=|I| lido
 */
static unsigned char arena[65536] __attribute__((aligned(4)));

static int jev_proj_raw(int b, int c, int x){
    int *a = (int *)arena;
    int y = -1;
    int bi;
    for (bi = 0; bi < c; bi++){
        int lo = a[b + bi];
        int hi = a[b + bi + 1];
        if (x >= lo && x < hi){ y = bi; }
    }
    return y;
}

static int jev_marginal_raw(int X){
    int *a = (int *)arena;
    int P = X + 2;
    int c0 = a[P], c1 = a[P + 1], c2 = a[P + 2];
    int B = P + 3;
    int S = c0 + c1 + c2 + 3;
    int D1 = B + S;
    int J = D1 + 48;
    int PROD = c0 * c1 * c2;
    int D2 = J + PROD;
    int OUT = D2 + 48;
    int o, x, total;
    int bs0 = B;
    int bs1 = B + c0 + 1;
    int bs2 = B + c0 + 1 + c1 + 1;
    for (o = 0; o < 48; o++){ a[D1 + o] = 0; a[D2 + o] = 0; a[OUT + o] = 0; }
    for (o = 0; o < PROD; o++) a[J + o] = 0;
    total = 0;
    for (x = 0; x < X; x++){
        int g = a[x];
        int y0 = proj(bs0, c0, x);
        int y1 = proj(bs1, c1, x);
        int y2 = proj(bs2, c2, x);
        int idx = y2 + c2 * (y1 + c1 * y0);
        a[J + idx] = a[J + idx] + g;
        a[D1 + y0] = a[D1 + y0] + g;
        a[D1 + 16 + y1] = a[D1 + 16 + y1] + g;
        a[D1 + 32 + y2] = a[D1 + 32 + y2] + g;
        total = total + g;
    }
    {
        int k0 = 0, k1 = 0, k2 = 0;
        for (o = 0; o < PROD; o++){
            int v = a[J + o];
            a[D2 + k0] = a[D2 + k0] + v;
            a[D2 + 16 + k1] = a[D2 + 16 + k1] + v;
            a[D2 + 32 + k2] = a[D2 + 32 + k2] + v;
            k2 = k2 + 1;
            if (k2 == c2){ k2 = 0; k1 = k1 + 1; if (k1 == c1){ k1 = 0; k0 = k0 + 1; } }
        }
    }
    a[OUT] = total;
    a[OUT + 1] = a[X];
    return 0;
}

/* E4 — Score fracionario sem divisão no motor: o funcional
 * L(p_q)=Σ s_i p_q(s_i) sai como o par racional exato (N, |I|) com
 * N = Σ_i s_i G_q(s_i) = Σ_x s_{q(x)} G(x). O valor N/|I| é uma LEITURA
 * do par inteiro, feita fora (oráculo), nunca uma operacao fracionaria aqui.
 *
 * Arena (int32 a partir do byte 8):
 *   [0, X)      G(x)
 *   X           |I|
 *   X+1         m (niveis do Score)
 *   B = X+2     fronteiras do Score (m+1: b_0..b_m = X)
 *   W = B+m+1   pesos s_i (m ints)
 *   OUT = W+m   OUT[0]=N; OUT[1]=|I|; OUT[2]=m
 */
static int jev_escore_raw(int X){
    int *a = (int *)arena;
    int m = a[X + 1];
    int B = X + 2;
    int W = B + m + 1;
    int OUT = W + m;
    int x, N = 0;
    for (x = 0; x < X; x++){
        int y = proj(B, m, x);
        N = N + a[W + y] * a[x];
    }
    a[OUT] = N;
    a[OUT + 1] = a[X];
    a[OUT + 2] = m;
    return 0;
}

/* E5 — Paralelismo: Noul e Score no mesmo percurso sobre G, um disco duas
 * roupas. A segunda leitura NÃO recria G: o campo [0..X) fica intocado e
 * ambas as leituras saem de uma única varredura. "Avaliadas em paralelo,
 * cada uma em isolamento" (a leitura conjunta coincide com as isoladas).
 *
 * Arena (int32 a partir do byte 8):
 *   [0, X)      G(x)
 *   X           |I|
 *   X+1         m (niveis do Score)
 *   Bn = X+2    fronteiras do Noul (c=2: 0, corte, X)
 *   Bs = X+5    fronteiras do Score (m+1)
 *   W = Bs+m+1  pesos s_i (m ints)
 *   OUT = W+m   [0..1] Noul G_q(0), G_q(1)
 *               [2..2+m) Score G_q por nivel
 *               [2+m]    N = Σ s_i G_q(s_i)
 *               [3+m]    D = |I|
 *               [4+m]    total recontado (sedo do mesmo percurso)
 */
static int jev_duas_raw(int X){
    int *a = (int *)arena;
    int m = a[X + 1];
    int Bn = X + 2;
    int Bs = X + 5;
    int W = Bs + m + 1;
    int OUT = W + m;
    int o, x, N, total;
    for (o = 0; o < m + 5; o++) a[OUT + o] = 0;
    N = 0; total = 0;
    for (x = 0; x < X; x++){
        int g = a[x];
        int yn = proj(Bn, 2, x);
        int ys = proj(Bs, m, x);
        a[OUT + yn] = a[OUT + yn] + g;
        a[OUT + 2 + ys] = a[OUT + 2 + ys] + g;
        N = N + a[W + ys] * g;
        total = total + g;
    }
    a[OUT + m + 2] = N;
    a[OUT + m + 3] = a[X];
    a[OUT + m + 4] = total;
    return 0;
}

/* E7 — a ponte com o motor dimensional (alinhamento com iota_d).
 * A leitura dimensional do MESMO G: cada célula x traz as suas d coordenadas
 * (X_d = (Z/256Z)^d, def:Xd), e a projeção q_k = "esquecimento da coordenada"
 * (o pi_k do def:iota-d / iota1.h). Igual a marginal() no mecanismo — só
 * comparacoes, conjunta em odometro (última coordenada mais rapida), D1 directas
 * vs D2 da conjunta — mas as projecoes são coordenadas VERDADEIRAS de X_d,
 * dadas como dados na arena (não intervalos do mesmo índice). Com a coordenada
 * nova a zero — o residuo 0 de iota_d(b_0..b_{d-1})=(..,0) — a leitura 2D
 * devolve a 1D: o alinhamento.
 *
 * Arena (int32 a partir do byte 8):
 *   [0, X)      G(x)
 *   X           |I|
 *   X+1         d (1..3; dimensoes tratadas; extras viram classe de residuo 0)
 *   P = X+2     c_0..c_{d-1}  (classes por dimensão, <=16)
 *   C = P+d     coordenadas: d blocos de X ints — coord_k(x)
 *   B = C+d*X   fronteiras: d blocos de (c_i+1) sobre o VALOR da coordenada
 *   D1 = B+S    marginais directas, linha da dim k em 16*k
 *   J = D1+16*d conjunta em ordem odometro
 *   D2 = J+PROD marginais da conjunta, mesma linha em 16*k
 *   OUT = D2+16*d  OUT[0]=total do campo; OUT[1]=|I| lido
 */
static int jev_marginal_d_raw(int X){
    int *a = (int *)arena;
    int d = a[X + 1];
    int P = X + 2;
    int c0 = a[P], c1 = d > 1 ? a[P + 1] : 1, c2 = d > 2 ? a[P + 2] : 1;
    int C = P + d;
    int B = C + d * X;
    int b0 = B;
    int b1 = d > 1 ? B + c0 + 1 : B;
    int b2 = d > 2 ? B + c0 + 1 + c1 + 1 : B;
    int S = (d > 2 ? c2 + 1 : 0) + (d > 1 ? c1 + 1 : 0) + (c0 + 1);
    int D1 = B + S;
    int PROD = c0 * c1 * c2;
    int J = D1 + 16 * d;
    int D2 = J + PROD;
    int OUT = D2 + 16 * d;
    int o, x, total;
    for (o = 0; o < PROD; o++) a[J + o] = 0;
    for (o = 0; o < 16 * d; o++){ a[D1 + o] = 0; a[D2 + o] = 0; a[OUT + o] = 0; }
    total = 0;
    for (x = 0; x < X; x++){
        int g = a[x];
        int y0 = proj(b0, c0, a[C + x]);
        int y1 = d > 1 ? proj(b1, c1, a[C + X + x]) : 0;
        int y2 = d > 2 ? proj(b2, c2, a[C + 2 * X + x]) : 0;
        int idx = y2 + c2 * (y1 + c1 * y0);
        a[J + idx] = a[J + idx] + g;
        a[D1 + y0] = a[D1 + y0] + g;
        if (d > 1) a[D1 + 16 + y1] = a[D1 + 16 + y1] + g;
        if (d > 2) a[D1 + 32 + y2] = a[D1 + 32 + y2] + g;
        total = total + g;
    }
    {
        int k0 = 0, k1 = 0, k2 = 0;
        for (o = 0; o < PROD; o++){
            int v = a[J + o];
            a[D2 + k0] = a[D2 + k0] + v;
            if (d > 1) a[D2 + 16 + k1] = a[D2 + 16 + k1] + v;
            if (d > 2) a[D2 + 32 + k2] = a[D2 + 32 + k2] + v;
            k2 = k2 + 1;
            if (k2 == c2){ k2 = 0; k1 = k1 + 1; if (k1 == c1){ k1 = 0; k0 = k0 + 1; } }
        }
    }
    a[OUT] = total;
    a[OUT + 1] = a[X];
    return 0;
}
/* ─────────────────────────────────────────────────────────────────────────────
 * A GUARDA, e a PORTA. Tudo o que diverge do original está abaixo desta linha.
 *
 * O INVÓLUCRO DE proj. O corpo é o do original — a mesma comparacao, o mesmo
 * y = -1 inicial, o mesmo último bloco que ganha. A única coisa acrescentada é
 * a marcacao: quando a projeção cai fora de todos os blocos, o -1 fica
 * registado. Não se altera o valor devolvido porque o -1 E a resposta certa
 * para jev_proj(), e um chamador que pergunta «a que classe pertence x?» tem
 * direito ao -1.
 *
 * A PORTA. Cada leitura arranca com jev_bad a zero, chama o servo, e traduz o
 * que encontrou em 1 ou 0 segundo a convenção da casa (sql_abrir devolve 1 para
 * sucesso). O servo continua a devolver 0 como o original devolvia — a
 * conversão e desta camada, e é a única razão de os dois números coexistirem.
 *
 * QUANDO A PORTA DIZ 0, O RESULTADO NÃO SERVE. O servo já escreveu: o -1 foi
 * usado como índice e caiu em D1-1, que é a última fronteira de entrada. O
 * campo G fica intacto e as fronteiras não. Quem chamou deve repor a cópia que
 * guardou, ou deitar o resultado. Não há como ler o que fica e não saber que
 * estava partido — e por isso que a porta o diz em vez de o devolver.
 *
 * Seria melhor ainda recusar ANTES de escrever. Não é: o servo original não
 * tem onde recuar, e introduzir uma passagem de validação seria mudar o corpo
 * que este ficheiro existe para preservar. Fica registado como trabalho
 * futuro, com o mesmo orcamento de 96 casos para o provar.
 */
static int proj(int b, int c, int x){
    int y = jev_proj_raw(b, c, x);
    if (y < 0) jev_bad = 1;
    return y;
}

/* A arena do chamador. Um simbolo, o mesmo ponteiro em todas as leituras. Para
 * isolar uma leitura, guarde os 65536 bytes antes e restaure depois. */
unsigned char *jev_arena(void){
    return arena;
}

/* A projeção nua. Aqui o -1 é a RESPOSTA, não um erro: é a classe de um valor que
 * não está em nenhum bloco, e quem pergunta tem direito a saber que não está. */
int jev_proj(int b, int c, int x){
    return jev_proj_raw(b, c, x);
}

int jev_marginal(int X){
    jev_bad = 0;
    jev_marginal_raw(X);
    return jev_bad ? JEV_ERR_CLASSE : JEV_OK;
}

int jev_escore(int X){
    jev_bad = 0;
    jev_escore_raw(X);
    return jev_bad ? JEV_ERR_CLASSE : JEV_OK;
}

int jev_duas(int X){
    jev_bad = 0;
    jev_duas_raw(X);
    return jev_bad ? JEV_ERR_CLASSE : JEV_OK;
}

int jev_marginal_d(int X){
    jev_bad = 0;
    jev_marginal_d_raw(X);
    return jev_bad ? JEV_ERR_CLASSE : JEV_OK;
}
