/* test_jev_oráculo.c — o JEV contra um oráculo escrito em C, sem o motor.
 *
 * Este ficheiro é a prova que fica no produto, e por isso NÃO sabe nada do
 * tiffany, nem do marginal.c, nem do marginal.wasm. Tudo o que ele sabe está
 * escrito duas vezes: uma no motor, em C, e outra aqui, em C, escritas por mão
 * uma a partir da outra. Se as duas concordarem, o motor está certo — não
 * porque repete o mesmo código, mas porque repete o mesmo TEOREMA por caminhos
 * independentes.
 *
 * A realização de G é um LCG fixo (a=1664525, c=1013904223), o mesmo dos
 * experimentos do bloco E1-E7. E o mesmo e por uma razão: os números são
 * reproduzíveis por qualquer um, e a série já foi usada para selar o WASM, de
 * modo que os casos daqui e os de lá são os mesmos casos.
 *
 * O QUE CADA BLOCO PROVA
 *   E1  o campo de contagem conserva o total: Σ G(x) = |I|
 *   E2  as marginais da conjunta igualam as marginais directas (teorema das
 *       marginais) e ambas batem com o oráculo
 *   E3  três leituras tipadas sobre o mesmo G: cada uma é uma marginal
 *   E4  o escore fraccionário como par racional EXACTO (N, |I|), sem divisão
 *   E5  Noul e Score numa varredura só, e o campo G não é recriado
 *   E7  a ponte com o motor dimensional: a mesma coisa lida como coordenadas
 *       de X_d, e com o residuo 0 a leitura 2D devolve a 1D
 */
#include <stdio.h>
#include <string.h>

#include "jev_api.h"

static int feitas, falhas;

static void ok(const char *q, int c)
{
    feitas++;
    if (!c) falhas++;
    if (!c) printf("  FALHA  %s\n", q);
}

/* o oráculo: as mesmas formulae, escritas outra vez */
static int cla(const int *b, int c, int x)
{
    int i;
    for (i = 0; i < c; i++) if (x >= b[i] && x < b[i + 1]) return i;
    return -1;
}
static void bounds(int X, int c, int *b)
{
    int j;
    for (j = 0; j <= c; j++) b[j] = (int)(((long)j * (long)X) / (long)c);
}
static unsigned int st;
static double lcg(void)
{
    st = st * 1664525u + 1013904223u;
    return (double)st / 4294967296.0;
}
static void gera_G(int *G, int X, int I, unsigned int seed)
{
    int t, x;
    for (x = 0; x < X; x++) G[x] = 0;
    st = seed;
    for (t = 0; t < I; t++) { x = (int)(lcg() * (double)X); if (x >= X) x = X - 1; G[x]++; }
}
static long oracleE4(int X, const int *G, int m, const int *bs, const int *s)
{
    int x, y; long N = 0;
    for (x = 0; x < X; x++) { y = cla(bs, m, x); N += (long)s[y] * (long)G[x]; }
    return N;
}

int main(void)
{
    int *a = (int *)jev_arena();
    int G[512], bs[3][33], bsc[3][33];
    int c, p, i, x;

    /* ── E1 + E2 : as marginais da conjunta igualam as directas ────────────── */
    {
        static const int CASOS[][4] = {
            { 16, 64, 2, 4 }, { 16, 31, 2, 4 }, {  8, 300, 5, 2 },
            { 64, 4444, 3, 3 }, { 32, 0, 2, 2 }, { 32, 1, 4, 2 }
        };
        static const unsigned int SEMENTES[] = { 111, 222, 333, 777, 999 };
        int nc = (int)(sizeof CASOS / sizeof CASOS[0]);
        for (c = 0; c < nc; c++) {
            int X = CASOS[c][0], I = CASOS[c][1];
            int c0 = CASOS[c][2], c1 = CASOS[c][3], c2 = 5;
            int cs[3], B, S, D1, J, PROD, D2, OUT;
            cs[0] = c0; cs[1] = c1; cs[2] = c2;
            bounds(X, c0, bs[0]); bounds(X, c1, bs[1]); bounds(X, c2, bs[2]);
            for (i = 0; i < (int)(sizeof SEMENTES / sizeof SEMENTES[0]); i++) {
                unsigned int seed = SEMENTES[i];
                int joint[512], D1o[3][16], D2o[3][16];
                char id[96];
                sprintf(id, "E1E2 X%d I%d c%d.%d.%d s%u", X, I, c0, c1, c2, seed);
                gera_G(G, X, I, seed);
                for (p = 0; p < 3; p++) for (x = 0; x < 16; x++) { D1o[p][x] = 0; D2o[p][x] = 0; }
                PROD = c0 * c1 * c2;
                for (x = 0; x < PROD; x++) joint[x] = 0;
                for (x = 0; x < X; x++) {
                    int y[3], g = G[x];
                    y[0] = cla(bs[0], c0, x); y[1] = cla(bs[1], c1, x); y[2] = cla(bs[2], c2, x);
                    joint[y[2] + c2 * (y[1] + c1 * y[0])] += g;
                    D1o[0][y[0]] += g; D1o[1][y[1]] += g; D1o[2][y[2]] += g;
                }
                { int k0 = 0, k1 = 0, k2 = 0;
                  for (x = 0; x < PROD; x++) {
                      int v = joint[x];
                      D2o[0][k0] += v; D2o[1][k1] += v; D2o[2][k2] += v;
                      k2++; if (k2 == c2) { k2 = 0; k1++; if (k1 == c1) { k1 = 0; k0++; } }
                  } }
                memset(jev_arena(), 0, JEV_ARENA_BYTES);
                for (x = 0; x < X; x++) a[x] = G[x];
                a[X] = I; a[X + 1] = 2;
                a[X + 2] = c0; a[X + 3] = c1; a[X + 4] = c2;
                B = X + 5; { int off = B;
                    for (p = 0; p < 3; p++) for (i = 0; i <= cs[p]; i++) a[off++] = bs[p][i]; }
                S = c0 + c1 + c2 + 3; D1 = B + S; J = D1 + 48; D2 = J + PROD; OUT = D2 + 48;
                ok("porta aceita", jev_marginal(X) == JEV_OK);
                { char q[160];
                  sprintf(q, "%s: total=%d == |I|=%d", id, a[OUT], I);
                  ok(q, a[OUT] == I && a[OUT + 1] == I);
                  for (p = 0; p < 3; p++) {
                      int soma = 0, igual = 1;
                      if (cs[p] <= 1) continue;
                      for (i = 0; i < cs[p]; i++) soma += a[D1 + 16 * p + i];
                      sprintf(q, "%s: soma_y D1[%d]=%d == %d", id, p, soma, I);
                      ok(q, soma == I);
                      for (i = 0; i < 16; i++) {
                          if (a[D1 + 16 * p + i] != D1o[p][i]) igual = 0;
                          if (a[D2 + 16 * p + i] != D2o[p][i]) igual = 0;
                          if (a[D1 + 16 * p + i] != a[D2 + 16 * p + i]) igual = 0;
                      }
                      sprintf(q, "%s: D1[%d]==D2[%d]==oráculo", id, p, p);
                      ok(q, igual);
                  } }
            }
        }
    }

    /* ── E3 : três leituras sobre o mesmo G ───────────────────────────────── */
    {
        static const int CASOS[][3] = { { 16, 64, 5 }, { 8, 300, 2 }, { 64, 4444, 1 } };
        static const unsigned int SEM3[][5] = { { 111, 222, 333, 777, 999 }, { 111, 333, 0, 0, 0 }, { 111, 0, 0, 0, 0 } };
        int cs[3] = { 2, 4, 5 }, B, S, D1, J, D2, OUT, PROD = 40;
        for (c = 0; c < 3; c++) {
            int X = CASOS[c][0], I = CASOS[c][1], ns = CASOS[c][2];
            bs[0][0] = 0; bs[0][1] = X / 2; bs[0][2] = X;
            bounds(X, 4, bs[1]); bounds(X, 5, bs[2]);
            for (i = 0; i < ns; i++) {
                unsigned int seed = SEM3[c][i];
                int rows[3][16], D2r[3][16];
                char id[96], q[160];
                sprintf(id, "E3 X%d I%d s%u", X, I, seed);
                gera_G(G, X, I, seed);
                memset(jev_arena(), 0, JEV_ARENA_BYTES);
                for (x = 0; x < X; x++) a[x] = G[x];
                a[X] = I; a[X + 1] = 3;
                a[X + 2] = cs[0]; a[X + 3] = cs[1]; a[X + 4] = cs[2];
                B = X + 5; { int off = B;
                    for (p = 0; p < 3; p++) for (i = 0; i <= cs[p]; i++) a[off++] = bs[p][i]; }
                S = cs[0] + cs[1] + cs[2] + 3; D1 = B + S; J = D1 + 48; D2 = J + PROD; OUT = D2 + 48;
                ok("porta aceita", jev_marginal(X) == JEV_OK);
                for (p = 0; p < 3; p++) for (x = 0; x < 16; x++) { rows[p][x] = a[D1 + 16 * p + x]; D2r[p][x] = a[D2 + 16 * p + x]; }
                for (p = 0; p < 3; p++) {
                    int soma = 0;
                    for (i = 0; i < cs[p]; i++) soma += rows[p][i];
                    sprintf(q, "%s: leitura %d normaliza para |I|=%d (%d)", id, p, I, soma);
                    ok(q, soma == I);
                    sprintf(q, "%s: leitura %d e a marginal da conjunta", id, p);
                    ok(q, memcmp(rows[p], D2r[p], sizeof rows[0]) == 0);
                }
                sprintf(q, "%s: o mesmo G sobreviveu as três leituras", id);
                ok(q, a[OUT] == I && a[OUT + 1] == I);
                sprintf(q, "%s: Noul e Score são leituras diferentes", id);
                ok(q, memcmp(rows[0], rows[2], 4 * sizeof(int)) != 0);
            }
        }
    }

    /* ── E4 : o escore fraccionário como par racional exacto ───────────────── */
    {
        int m = 5, sb5[5] = { 0, 1, 2, 3, 4 }, bs5[6] = { 0, 1, 3, 5, 7, 9 };
        static const int EXPL[][8] = {
            { 9, 3, 1, 0, 1, 0, 0, 0 }, { 9, 4, 1, 1, 1, 1, 0, 0 },
            { 9, 5, 0, 0, 0, 0, 0, 0 }, { 9, 6, 0, 0, 0, 0, 0, 0 }
        };
        for (c = 0; c < 4; c++) {
            int X = EXPL[c][0], I = EXPL[c][1], off, N, D, k;
            long Noc; char q[128];
            memset(jev_arena(), 0, JEV_ARENA_BYTES);
            for (x = 0; x < X; x++) a[x] = 0;
            /* os quatro casos explicitos: um perfil de G escrito a mão */
            static const int PROF[4][9] = {
                { 0, 0, 0, 0, 0, 1, 0, 1, 1 },
                { 0, 0, 1, 0, 1, 0, 0, 1, 1 },
                { 5, 0, 0, 0, 0, 0, 0, 0, 0 },
                { 1, 0, 0, 1, 1, 2, 2, 0, 1 }
            };
            for (x = 0; x < X; x++) a[x] = PROF[c][x];
            a[X] = I; a[X + 1] = m;
            off = X + 2;
            for (k = 0; k <= m; k++) a[off++] = bs5[k];
            for (k = 0; k < m; k++) a[off++] = sb5[k];
            N = off; D = off + 1;
            Noc = oracleE4(X, PROF[c], m, bs5, sb5);
            ok("porta aceita", jev_escore(X) == JEV_OK);
            sprintf(q, "E4 perfil %d: N=%d == oráculo %ld (sem divisão no motor)", c, a[N], Noc);
            ok(q, a[N] == (int)Noc);
            sprintf(q, "E4 perfil %d: D=%d == |I|=%d", c, a[D], I);
            ok(q, a[D] == I);
            sprintf(q, "E4 perfil %d: m=%d lido", c, a[N + 2]);
            ok(q, a[N + 2] == m);
        }
        /* robustez: LCG sobre o mesmo escore, com fronteiras derivadas de X */
        {
            static const int XS[][2] = { { 16, 64 }, { 8, 300 }, { 64, 4444 } };
            for (c = 0; c < 3; c++) for (i = 0; i < 3; i++) {
                int X = XS[c][0], I = XS[c][1], off, k;
                unsigned int seed = (unsigned int)(i == 0 ? 111 : i == 1 ? 333 : 777);
                char q[128];
                gera_G(G, X, I, seed);
                bounds(X, m, bs5);
                memset(jev_arena(), 0, JEV_ARENA_BYTES);
                for (x = 0; x < X; x++) a[x] = G[x];
                a[X] = I; a[X + 1] = m;
                off = X + 2;
                for (k = 0; k <= m; k++) a[off++] = bs5[k];
                for (k = 0; k < m; k++) a[off++] = sb5[k];
                ok("porta aceita", jev_escore(X) == JEV_OK);
                sprintf(q, "E4 X%d I%d s%u: par (N,D) exacto", X, I, seed);
                ok(q, a[off] == (int)oracleE4(X, G, m, bs5, sb5) && a[off + 1] == I);
            }
        }
    }

    /* ── E5 : Noul e Score numa varredura só, sem recriar o campo ──────────── */
    {
        static const int XS[][2] = { { 16, 64 }, { 8, 300 }, { 64, 4444 } };
        int m = 5, sb5[5] = { 0, 1, 2, 3, 4 }, bn[3], bs5[6];
        for (c = 0; c < 3; c++) for (i = 0; i < 3; i++) {
            int X = XS[c][0], I = XS[c][1], off, k, n0, n1, somS = 0, tot, N, D;
            unsigned int seed = (unsigned int)(i == 0 ? 111 : i == 1 ? 222 : 777);
            char q[128];
            /* Noul lido com so duas classes, {1,0}. Um array de verdade: fazer
             * (const int *)"\1\0\0\0\0" lia 8 bytes de um literal de 6. */
            int sb2[2] = { 1, 0 };
            gera_G(G, X, I, seed);
            bn[0] = 0; bn[1] = X / 2; bn[2] = X;
            bounds(X, m, bs5);
            memset(jev_arena(), 0, JEV_ARENA_BYTES);
            for (x = 0; x < X; x++) a[x] = G[x];
            a[X] = I; a[X + 1] = m;
            off = X + 2;
            for (k = 0; k < 3; k++) a[off++] = bn[k];
            for (k = 0; k <= m; k++) a[off++] = bs5[k];
            for (k = 0; k < m; k++) a[off++] = sb5[k];
            ok("porta aceita", jev_duas(X) == JEV_OK);
            n0 = a[off]; n1 = a[off + 1];
            for (k = 0; k < m; k++) somS += a[off + 2 + k];
            tot = a[off + m + 4]; N = a[off + m + 2]; D = a[off + m + 3];
            sprintf(q, "E5 X%d I%d s%u: uma varredura, total=%d == |I|=%d", X, I, seed, tot, I);
            ok(q, tot == I);
            sprintf(q, "E5 X%d I%d s%u: Noul conserva (%d,%d)", X, I, seed, n0, n1);
            ok(q, n0 + n1 == tot && n0 == oracleE4(X, G, 2, bn, sb2));
            sprintf(q, "E5 X%d I%d s%u: Score conserva (%d)", X, I, seed, somS);
            ok(q, somS == tot);
            sprintf(q, "E5 X%d I%d s%u: (N,D) = (%d,%d) exacto", X, I, seed, N, D);
            ok(q, N == (int)oracleE4(X, G, m, bs5, sb5) && D == I);
            { int intacto = 1;
              for (x = 0; x < X; x++) if (a[x] != G[x]) intacto = 0;
              sprintf(q, "E5 X%d I%d s%u: o campo G não foi recriado", X, I, seed);
              ok(q, intacto && a[X] == I); }
        }
    }

    /* ── E7 : a ponte com o motor dimensional ──────────────────────────────── */
    {
        /* (a) o residuo 0: a coordenada nova vale 0, e a leitura 2D devolve a 1D */
        static const int XS[][2] = { { 16, 64 }, { 8, 300 }, { 64, 4444 } };
        for (c = 0; c < 3; c++) for (i = 0; i < 3; i++) {
            int X = XS[c][0], I = XS[c][1], cs[3], C, B, S, D1, J, D2, OUT, k;
            unsigned int seed = (unsigned int)(i == 0 ? 111 : i == 1 ? 222 : 777);
            char q[128]; int d1[16], d2[16], d1b[16];
            cs[0] = 2; cs[1] = 1; cs[2] = 1;
            gera_G(G, X, I, seed);
            memset(jev_arena(), 0, JEV_ARENA_BYTES);
            for (x = 0; x < X; x++) a[x] = G[x];
            a[X] = I; C = X + 2 + 2;
            a[X + 1] = 2; a[X + 2] = cs[0]; a[X + 3] = cs[1];
            for (x = 0; x < X; x++) a[C + 0 * X + x] = x;
            for (x = 0; x < X; x++) a[C + 1 * X + x] = 0;
            B = C + 2 * X; bsc[0][0] = 0; bsc[0][1] = X / 2; bsc[0][2] = X;
            bsc[1][0] = 0; bsc[1][1] = 1;
            { int off = B;
              for (p = 0; p < 2; p++) for (k = 0; k <= cs[p]; k++) a[off++] = bsc[p][k]; }
            S = 3 + 2; D1 = B + S; J = D1 + 32; D2 = J + 2; OUT = D2 + 32;
            ok("porta aceita", jev_marginal_d(X) == JEV_OK);
            for (k = 0; k < 16; k++) { d1[k] = a[D1 + k]; d2[k] = a[D2 + k]; d1b[k] = a[D1 + 16 + k]; }
            sprintf(q, "E7a X%d I%d s%u: conserva o total (%d)", X, I, seed, a[OUT]);
            ok(q, a[OUT] == I && a[OUT + 1] == I);
            sprintf(q, "E7a X%d I%d s%u: a marginal da 2D devolve a 1D", X, I, seed);
            ok(q, memcmp(d1, d2, 2 * sizeof(int)) == 0);
            sprintf(q, "E7a X%d I%d s%u: a coordenada nova so tem massa no residuo", X, I, seed);
            ok(q, d1b[0] == I && d1b[1] == 0);
            { int intacto = 1;
              for (x = 0; x < X; x++) if (a[x] != G[x]) intacto = 0;
              sprintf(q, "E7a X%d I%d s%u: o campo G não foi recriado", X, I, seed);
              ok(q, intacto); }
        }
        /* (b) 2D genuino sobre X_2 = B0 x B1, e o retrocesso para a leitura 1D */
        {
            static const int BS[][2] = { { 16, 4 }, { 4, 16 }, { 8, 8 } };
            for (c = 0; c < 3; c++) for (i = 0; i < 3; i++) {
                int B0 = BS[c][0], B1 = BS[c][1], X = B0 * B1, I = 500;
                int cs[3], C, B, S, D1, J, D2, OUT, PROD = 8, k;
                unsigned int seed = (unsigned int)(i == 0 ? 111 : i == 1 ? 333 : 999);
                char q[128]; int D1o[2][16], D2o[2][16], joint[8];
                cs[0] = 4; cs[1] = 2; cs[2] = 1;
                bounds(B0, 4, bsc[0]); bounds(B1, 2, bsc[1]);
                gera_G(G, X, I, seed);
                memset(jev_arena(), 0, JEV_ARENA_BYTES);
                for (x = 0; x < X; x++) a[x] = G[x];
                a[X] = I; C = X + 2 + 2;
                a[X + 1] = 2; a[X + 2] = cs[0]; a[X + 3] = cs[1];
                { int b0 = 0, b1 = 0;
                  for (x = 0; x < X; x++) {
                      a[C + 0 * X + x] = b0; a[C + 1 * X + x] = b1;
                      b0++; if (b0 == B0) { b0 = 0; b1++; } } }
                B = C + 2 * X; { int off = B;
                  for (p = 0; p < 2; p++) for (k = 0; k <= cs[p]; k++) a[off++] = bsc[p][k]; }
                S = 5 + 3; D1 = B + S; J = D1 + 32; D2 = J + PROD; OUT = D2 + 32;
                /* o oráculo em X_2 */
                for (p = 0; p < 2; p++) for (k = 0; k < 16; k++) { D1o[p][k] = 0; D2o[p][k] = 0; }
                for (x = 0; x < PROD; x++) joint[x] = 0;
                for (x = 0; x < X; x++) {
                    int y0 = cla(bsc[0], 4, a[C + 0 * X + x]);
                    int y1 = cla(bsc[1], 2, a[C + 1 * X + x]);
                    joint[y1 + 2 * y0] += G[x];
                    D1o[0][y0] += G[x]; D1o[1][y1] += G[x];
                }
                { int k0 = 0, k1 = 0;
                  for (x = 0; x < PROD; x++) {
                      D2o[0][k0] += joint[x]; D2o[1][k1] += joint[x];
                      k1++; if (k1 == 2) { k1 = 0; k0++; } } }
                ok("porta aceita", jev_marginal_d(X) == JEV_OK);
                sprintf(q, "E7b %dx%d s%u: conserva o total (%d)", B0, B1, seed, a[OUT]);
                ok(q, a[OUT] == I && a[OUT + 1] == I);
                for (p = 0; p < 2; p++) {
                    sprintf(q, "E7b %dx%d s%u: D1[%d]==D2[%d]==oráculo", B0, B1, seed, p, p);
                    ok(q, memcmp(&a[D1 + 16 * p], D1o[p], 16 * sizeof(int)) == 0
             && memcmp(&a[D2 + 16 * p], D2o[p], 16 * sizeof(int)) == 0
             && memcmp(&a[D1 + 16 * p], &a[D2 + 16 * p], 16 * sizeof(int)) == 0);
                }
            }
        }
    }

    printf("jev_oráculo: %d verificações, %d falhas\n", feitas, falhas);
    return falhas ? 1 : 0;
}
