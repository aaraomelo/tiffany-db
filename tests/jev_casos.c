/* jev_casos.c — os 96 casos E1–E7, gerados. Porta de linha a linha de gen.mjs.
 *
 * No laboratório (tiffany, fora deste repositório) gen.mjs gerava estes casos
 * em Node e chamava pelo marginal.wasm ORIGINAL; o resultado ficou em
 * wasm_out.bin. Aqui só se monta a ENTRADA, na arena da biblioteca, e a
 * comparação com o WASM é feita contra o golden, que é a saída daquele WASM.
 *
 * ── O LCG ──────────────────────────────────────────────────────────────────────
 * rnd() em JS era
 *     x = (Math.imul(x, 1664525) + 1013904223) >>> 0;   return x / 4294967296
 * e o Math.floor(rnd()*X) escolhia a casa. Em C a aritmética uint32 faz o
 * mesmo embrulho módulo 2^32 que o >>> 0, e a divisão por 2^32 é exacta em
 * double, portanto os dois lados escolhem a MESMA casa, sempre.
 */
#include "jev_casos.h"
#include "jev_api.h"

#include <stdio.h>
#include <string.h>

static unsigned int lcg_estado;

static double lcg(void)
{
    lcg_estado = lcg_estado * 1664525u + 1013904223u;
    return (double)lcg_estado / 4294967296.0;
}

/* A arena da biblioteca, como o motor a vê. */
#define A ((int *)jev_arena())

static void zera(void) { memset(jev_arena(), 0, JEV_ARENA_B); }

static void gera_G(int X, int I, unsigned int seed)
{
    int *a = A, t, x;
    lcg_estado = seed;
    for (t = 0; t < I; t++) {
        x = (int)(lcg() * X);
        a[x]++;
    }
}

/* As fronteiras de c blocos: floor(j·X/c). */
static void bounds(int X, int c, int *b)
{
    int j;
    for (j = 0; j <= c; j++) b[j] = j * X / c;
}

/* As três fronteiras do Noul: 0, metade, X. */
static void noulB(int X, int *b) { b[0] = 0; b[1] = X / 2; b[2] = X; }

int jev_casos_total(void) { return 96; }

/* ─── as tabelas que o gen.mjs tinha dentro dos ciclos ─────────────────────── */

static const struct { int X, I, c0, c1, c2; } E1E2[6] = {
    { 16, 64,   2, 4, 5 },
    { 16, 31,   2, 4, 1 },
    {  8, 300,  5, 2, 4 },
    { 64, 4444, 3, 3, 3 },
    { 32, 0,    2, 2, 1 },
    { 32, 1,    4, 2, 2 }
};

/* E4, primeira leva: G escrito à mão, fracções 7/3, 11/4, 0/5 e 5/3. */
static const struct { int I, esp[4]; int g[9]; } E4MANO[4] = {
    { 3, {  7, 3, 0, 0 }, { 0, 1, 0, 0, 0, 1, 1, 0, 0 } },
    { 4, { 11, 4, 0, 0 }, { 0, 0, 1, 1, 0, 0, 0, 1, 1 } },
    { 5, {  0, 5, 0, 0 }, { 5, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { 6, {  5, 3, 0, 0 }, { 1, 2, 0, 0, 2, 0, 0, 0, 1 } }
};

/* E7b: as duas formas do produto cartesiano. */
static const struct { int B0, B1; } E7B[3] = { { 16, 4 }, { 4, 16 }, { 8, 8 } };

/* Os três (X,I) que E3, E5 e E7a percorrem, pela mesma ordem. */
static const struct { int X, I; } XIS[3] = { { 16, 64 }, { 8, 300 }, { 64, 4444 } };

/* ─── o percurso ────────────────────────────────────────────────────────────── */

/* O gen.mjs guardava a entrada ANTES de chamar o motor, porque a chamada
 * escreve na arena que é a mesma. Aqui é igual: a entrada de cada caso é
 * copiada para cá, e só depois se executa o resíduo que o ensaio deixou. */
static unsigned char entrada_guardada[JEV_ARENA_B];

/* Fecha o caso: aponta para a entrada guardada e avança o cursor. Usar
 * SO_GUARDADO=1 logo DEPOIS de guardar à mão (E5 duas, que deixa resíduo e
 * por isso não pode ser guardado depois de duas correr). */
#define FECHA(SO_GUARDADO)                                                   \
    do {                                                                    \
        if (!(SO_GUARDADO))                                                  \
            memcpy(entrada_guardada, jev_arena(), JEV_ARENA_B);             \
        cs.arena = entrada_guardada;                                         \
        *cursor = idx + 1;                                                   \
        return &cs;                                                          \
    } while (0)

const jev_caso *jev_casoSeguinte(int *cursor)
{
    static jev_caso cs;
    int idx = *cursor;
    int *a = A;
    int X, I, k, j, off;

    if (idx >= jev_casos_total()) return NULL;
    memset(&cs, 0, sizeof cs);
    cs.arena = jev_arena();

    /* ---- E1+E2 : marginais (fn 0), índices 0..29 ---- */
    if (idx < 30) {
        static const int SEM[5] = { 111, 222, 333, 777, 999 };
        int cc = idx / 5, si = idx % 5;
        int cq[3], nReal = 0, bs[3][66];
        X = E1E2[cc].X; I = E1E2[cc].I;
        cq[0] = E1E2[cc].c0; cq[1] = E1E2[cc].c1; cq[2] = E1E2[cc].c2;
        for (k = 0; k < 3; k++) { bounds(X, cq[k], bs[k]); if (cq[k] > 1) nReal++; }

        zera();
        gera_G(X, I, (unsigned int)SEM[si]);
        a[X] = I; a[X + 1] = nReal;
        a[X + 2] = cq[0]; a[X + 3] = cq[1]; a[X + 4] = cq[2];
        off = X + 5;
        for (k = 0; k < 3; k++) for (j = 0; j <= cq[k]; j++) a[off++] = bs[k][j];
        cs.fn = JEV_MARGINAL; cs.X = X;
        snprintf(cs.nome, sizeof cs.nome, "E1E2 X%d I%d c%d.%d.%d s%d",
                 X, I, cq[0], cq[1], cq[2], SEM[si]);
        FECHA(0);
    }

    /* ---- E3 : três leituras sobre o MESMO G (fn 0), índices 30..37 ---- */
    if (idx < 38) {
        /* Flat, na mesma ordem dos dois ciclos aninhados do gen.mjs: cinco de
         * X=16, dois de X=8, um de X=64. */
        static const struct { int X, I, seed; } E3[8] = {
            { 16, 64,   111 }, { 16, 64,   222 }, { 16, 64,   333 },
            { 16, 64,   777 }, { 16, 64,   999 },
            {  8, 300,  111 }, {  8, 300,  333 },
            { 64, 4444, 111 }
        };
        int cq[3], bs[3][66];
        X = E3[idx - 30].X; I = E3[idx - 30].I;
        cq[0] = 2; cq[1] = 4; cq[2] = 5;
        for (k = 0; k < 3; k++) bounds(X, cq[k], bs[k]);

        zera();
        gera_G(X, I, (unsigned int)E3[idx - 30].seed);
        a[X] = I; a[X + 1] = 3;
        a[X + 2] = cq[0]; a[X + 3] = cq[1]; a[X + 4] = cq[2];
        off = X + 5;
        for (k = 0; k < 3; k++) for (j = 0; j <= cq[k]; j++) a[off++] = bs[k][j];
        cs.fn = JEV_MARGINAL; cs.X = X;
        snprintf(cs.nome, sizeof cs.nome, "E3 X%d I%d s%d", X, I, E3[idx - 30].seed);
        FECHA(0);
    }

    /* ---- E4 : escore (fn 1), índices 38..50 ---- */
    if (idx < 51) {
        static const int S[5] = { 0, 1, 2, 3, 4 };
        static const int BS[6] = { 0, 1, 3, 5, 7, 9 };
        int m = 5;
        if (idx < 42) {
            int q = idx - 38;
            X = 9; I = E4MANO[q].I;
            zera();
            for (j = 0; j < X; j++) a[j] = E4MANO[q].g[j];
            a[X] = I; a[X + 1] = m;
            off = X + 2;
            for (j = 0; j <= m; j++) a[off++] = BS[j];
            for (j = 0; j < m; j++) a[off++] = S[j];
            snprintf(cs.nome, sizeof cs.nome, "E4 %d/%d", E4MANO[q].esp[0], E4MANO[q].esp[1]);
        } else {
            static const int SEM[3] = { 111, 333, 777 };
            int q = idx - 42, xi = q / 3, si = q % 3;
            int b[16];
            X = XIS[xi].X; I = XIS[xi].I;
            zera();
            gera_G(X, I, (unsigned int)SEM[si]);
            a[X] = I; a[X + 1] = m;
            bounds(X, m, b);
            off = X + 2;
            for (j = 0; j <= m; j++) a[off++] = b[j];
            for (j = 0; j < m; j++) a[off++] = S[j];
            snprintf(cs.nome, sizeof cs.nome, "E4 X%d I%d s%d", X, I, SEM[si]);
        }
        cs.fn = JEV_ESCORE; cs.X = X;
        FECHA(0);
    }

    /* ---- E5 : duas (fn 2) e as leituras isoladas, índices 51..77 ---- */
    if (idx < 78) {
        static const int SEM[3] = { 111, 222, 777 };
        static const int S[5] = { 0, 1, 2, 3, 4 };
        int q = idx - 51, xi = q / 9, sub = q % 9;
        int si = sub / 3, qual = sub % 3;
        int m = 5, bn[3], bs[16];
        X = XIS[xi].X; I = XIS[xi].I;
        noulB(X, bn); bounds(X, m, bs);

        if (qual == 0) {
            /* duas: Noul e Score na MESMA varredura */
            zera();
            gera_G(X, I, (unsigned int)SEM[si]);
            a[X] = I; a[X + 1] = m;
            off = X + 2;
            for (j = 0; j < 3; j++) a[off++] = bn[j];
            for (j = 0; j <= m; j++) a[off++] = bs[j];
            for (j = 0; j < m; j++) a[off++] = S[j];
            cs.fn = JEV_DUAS; cs.X = X;
            snprintf(cs.nome, sizeof cs.nome, "E5 X%d I%d s%d", X, I, SEM[si]);
            /* Este caso é o único que escreve a arena ANTES de devolver: o
             * ensaio chamava duas aqui, e os dois casos seguintes eram
             * montados sobre o que ela deixou. Sem esta chamada a entrada
             * deles não é a mesma que o laboratório viu. A entrada deste
             * caso guarda-se primeiro, para a chamada a não a contaminar. */
            memcpy(entrada_guardada, jev_arena(), JEV_ARENA_B);
            cs.arena = entrada_guardada;
            *cursor = idx + 1;
            jev_duas(X);
            return &cs;
        } else if (qual == 1) {
            /* o Noul isolado, pela marginal. NÃO limpa a arena. */
            a[X + 1] = 1; a[X + 2] = 2; a[X + 3] = 1; a[X + 4] = 1;
            off = X + 5;
            for (j = 0; j < 3; j++) a[off++] = bn[j];
            a[off++] = 0; a[off++] = X;
            a[off++] = 0; a[off++] = X;
            cs.fn = JEV_MARGINAL; cs.X = X;
            snprintf(cs.nome, sizeof cs.nome, "E5isoMarg X%d I%d s%d", X, I, SEM[si]);
        } else {
            /* o Score isolado, pelo escore */
            a[X + 1] = m;
            off = X + 2;
            for (j = 0; j <= m; j++) a[off++] = bs[j];
            for (j = 0; j < m; j++) a[off++] = S[j];
            cs.fn = JEV_ESCORE; cs.X = X;
            snprintf(cs.nome, sizeof cs.nome, "E5isoEsc X%d I%d s%d", X, I, SEM[si]);
        }
        FECHA(0);
    }

    /* ---- E7 : a ponte dimensional (fn 3), índices 78..95 ---- */
    {
        static const int SEMA[3] = { 111, 222, 777 };
        static const int SEMB[3] = { 111, 333, 999 };
        int cq[2], d = 2, C;

        /* montaD, palavra a palavra do gen.mjs */
        #define MONTA_D(K0, K1, b0, b1)                                        \
            do {                                                               \
                C = X + 2 + d;                                                 \
                for (k = 0; k < d; k++) a[X + 2 + k] = cq[k];                  \
                for (k = 0; k < d; k++)                                        \
                    for (j = 0; j < X; j++) a[C + k * X + j] = (k == 0) ? (K0)[j] : (K1)[j]; \
                off = C + d * X;                                              \
                for (k = 0; k < d; k++)                                        \
                    for (j = 0; j <= cq[k]; j++) a[off++] = (k == 0) ? (b0)[j] : (b1)[j]; \
                a[X + 1] = d;                                                  \
            } while (0)

        if (idx < 87) {
            int xi = (idx - 78) / 3, si = (idx - 78) % 3;
            int K0[64], K1[64];
            int b0[8], b1[8];
            X = XIS[xi].X; I = XIS[xi].I;
            cq[0] = 2; cq[1] = 1;
            noulB(X, b0); b1[0] = 0; b1[1] = 1;
            for (j = 0; j < X; j++) { K0[j] = j; K1[j] = 0; }
            zera();
            gera_G(X, I, (unsigned int)SEMA[si]);
            a[X] = I;
            MONTA_D(K0, K1, b0, b1);
            snprintf(cs.nome, sizeof cs.nome, "E7a X%d I%d s%d", X, I, SEMA[si]);
        } else {
            int e = idx - 87, xi = e / 3, si = e % 3;
            int B0 = E7B[xi].B0, B1 = E7B[xi].B1;
            int K0[256], K1[256];
            int b0[8], b1[8];
            X = B0 * B1; I = 500;
            cq[0] = 4; cq[1] = 2;
            bounds(B0, cq[0], b0); bounds(B1, cq[1], b1);
            {
                int bb0 = 0, bb1 = 0;
                for (j = 0; j < X; j++) {
                    K0[j] = bb0; K1[j] = bb1;
                    bb0++;
                    if (bb0 == B0) { bb0 = 0; bb1++; }
                }
            }
            zera();
            gera_G(X, I, (unsigned int)SEMB[si]);
            a[X] = I;
            MONTA_D(K0, K1, b0, b1);
            snprintf(cs.nome, sizeof cs.nome, "E7b B%dx%d s%d", B0, B1, SEMB[si]);
        }
        cs.fn = JEV_MARGINAL_D; cs.X = X;
        FECHA(0);
    }
    #undef MONTA_D
}
