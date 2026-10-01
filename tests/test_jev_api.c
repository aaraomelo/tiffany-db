/* test_jev_api.c — a PORTA, e não a matemática.
 *
 * O outro ficheiro de teste pergunta «os números são os mesmos que o oráculo
 * diz». Este pergunta coisas de diferente: de quem é a arena, se duas leituras
 * iguais dao o mesmo resultado, o que acontece quando a entrada está partida,
 * e se o estado de uma leitura manchada se verte para a seguinte.
 *
 * A RECUSA E O MOTIVO DESTE FICHEIRO. O motor original, quando uma projeção
 * caia fora de todos os blocos, usava o -1 como índice: escrevia por cima da
 * última fronteira de entrada, devolvia 0 — que aqui era sucesso — e não dizia
 * nada. Medido antes de mudar: fronteiras {10,20,0,1} passavam a {10,20,0,11},
 * sem segfault e sem aviso. A porta devolve JEV_ERR_CLASSE em vez disso.
 *
 * E aqui está o outro lado, que também é verdade e também tem de estar escrito:
 * a recusa acontece DEPOIS do servo escrever. O campo G sobrevive; as fronteiras
 * não. Por isso o ficheiro mede as duas coisas, e por isso a porta avisa para
 * quem chamou repor a cópia que guardou. Um teste que só verificasse «devolveu
 * 0» deixaria passar a ideia de que a arena ficou intacta, e não ficou.
 */
#include <stdio.h>
#include <string.h>

#include "jev_api.h"

static int feitas, falhas;

static void ok(const char *q, int c)
{
    feitas++;
    if (!c) { falhas++; printf("  FALHA  %s\n", q); }
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

int main(void)
{
    int *a = (int *)jev_arena();
    int G[128], x, p, i;
    static unsigned char guardado[JEV_ARENA_BYTES];
    static unsigned char primeira[JEV_ARENA_BYTES];
    static unsigned char segunda[JEV_ARENA_BYTES];

    /* ── a arena é do chamador, e é a mesma ──────────────────────────────── */
    ok("jev_arena() devolve um ponteiro", jev_arena() != NULL);
    ok("jev_arena() e estavel entre chamadas", jev_arena() == jev_arena());
    ok("a arena tem 65536 bytes", JEV_ARENA_BYTES == 65536);

    /* ── determinismo: a mesma entrada, duas vezes, os mesmos bytes ─────── */
    {
        int X = 32, cs[3] = { 2, 4, 5 }, B = X + 5, off;
        unsigned char buf[JEV_ARENA_BYTES];
        memset(jev_arena(), 0, JEV_ARENA_BYTES);
        gera_G(G, X, 1000, 111);
        for (x = 0; x < X; x++) a[x] = G[x];
        a[X] = 1000; a[X + 1] = 3;
        a[X + 2] = cs[0]; a[X + 3] = cs[1]; a[X + 4] = cs[2];
        off = B;
        for (p = 0; p < 3; p++) { int n = cs[p], j, v = 0;
            for (j = 0; j <= n; j++) { a[off++] = (int)(((long)j * X) / n); (void)v; } }
        memcpy(buf, jev_arena(), JEV_ARENA_BYTES);

        memcpy(jev_arena(), buf, JEV_ARENA_BYTES);
        ok("jev_marginal corre", jev_marginal(X) == JEV_OK);
        memcpy(primeira, jev_arena(), JEV_ARENA_BYTES);

        memcpy(jev_arena(), buf, JEV_ARENA_BYTES);
        ok("jev_marginal repete", jev_marginal(X) == JEV_OK);
        memcpy(segunda, jev_arena(), JEV_ARENA_BYTES);

        ok("os 65536 bytes são idênticos nas duas leituras",
           memcmp(primeira, segunda, JEV_ARENA_BYTES) == 0);

        /* e o chamador pode ISOLAR: guardar, correr, repor, correr */
        memcpy(jev_arena(), buf, JEV_ARENA_BYTES);
        memcpy(guardado, jev_arena(), JEV_ARENA_BYTES);
        ok("a entrada a guardar e a entrada original",
           memcmp(guardado, buf, JEV_ARENA_BYTES) == 0);
        ok("jev_marginal corre", jev_marginal(X) == JEV_OK);
        ok("jev_marginal alterou a arena",
           memcmp(guardado, jev_arena(), JEV_ARENA_BYTES) != 0);
        memcpy(jev_arena(), guardado, JEV_ARENA_BYTES);
        ok("a entrada reposta e a entrada original",
           memcmp(jev_arena(), buf, JEV_ARENA_BYTES) == 0);
        ok("e volta a correr bem depois de reposta", jev_marginal(X) == JEV_OK);
    }

    /* ── o mesmo campo G, duas leituras de outra forma ──────────────────── */
    {
        /* duas() tem um contrato próprio — duas tabelas de fronteiras, os
         * pesos, e a sua própria saída. Monta-se o seu, não se reaproveita o
         * de marginal(): são layouts diferentes para o mesmo campo. */
        int X = 32, m = 5, off = X + 2, k, I = 1000;
        int pesos[5] = { 0, 1, 2, 3, 4 };
        unsigned char buf[JEV_ARENA_BYTES];
        gera_G(G, X, I, 111);
        memset(jev_arena(), 0, JEV_ARENA_BYTES);
        for (x = 0; x < X; x++) a[x] = G[x];
        a[X] = I; a[X + 1] = m;
        a[off++] = 0; a[off++] = X / 2; a[off++] = X;      /* Noul, c=2 */
        for (k = 0; k <= m; k++) a[off++] = (int)(((long)k * X) / m);
        for (k = 0; k < m; k++) a[off++] = pesos[k];
        memcpy(buf, jev_arena(), JEV_ARENA_BYTES);
        memcpy(guardado, jev_arena(), JEV_ARENA_BYTES);
        ok("jev_duas corre com o contrato dele", jev_duas(X) == JEV_OK);
        ok("jev_duas alterou a arena", memcmp(guardado, jev_arena(), JEV_ARENA_BYTES) != 0);
        { int intacto = 1, som = 0;
          for (x = 0; x < X; x++) { if (a[x] != G[x]) intacto = 0; som += a[x]; }
          ok("jev_duas não recriou o campo G", intacto);
          /* OUT = off; o total recontado vive em OUT+m+4 e |I| em OUT+m+3 */
          ok("jev_duas devolveu o total de |I|", a[off + m + 4] == I && som == I);
          ok("jev_duas leu |I|", a[off + m + 3] == I); }
        memcpy(jev_arena(), guardado, JEV_ARENA_BYTES);
    }

    /* ── jev_proj: o -1 é RESPOSTA, não erro ────────────────────────────── */
    {
        int b[4], X = 16;
        /* a base de proj é um índice DA ARENA: as fronteiras têm de estar lá */
        b[0] = 0; b[1] = 4; b[2] = 10; b[3] = X;
        memset(jev_arena(), 0, JEV_ARENA_BYTES);
        for (i = 0; i < 4; i++) a[i] = b[i];
        ok("proj: 0 pertence a classe 0", jev_proj(0, 3, 0) == 0);
        ok("proj: 5 pertence a classe 1", jev_proj(0, 3, 5) == 1);
        ok("proj: 15 pertence a classe 2", jev_proj(0, 3, 15) == 2);
        ok("proj: 20 não pertence a nenhuma classe", jev_proj(0, 3, 20) == -1);
        ok("proj: -5 não pertence a nenhuma classe", jev_proj(0, 3, -5) == -1);
        ok("proj: um bloco so, com o valor dentro", jev_proj(0, 1, 7) == -1);
        /* pedir a projeção não pode manchar a leitura seguinte */
        memset(jev_arena(), 0, JEV_ARENA_BYTES);
        { int c2[3] = { 2, 2, 2 }, B = X + 5, off = B, n;
          for (x = 0; x < X; x++) a[x] = 1;
          a[X] = X; a[X + 1] = 3;
          a[X + 2] = c2[0]; a[X + 3] = c2[1]; a[X + 4] = c2[2];
          for (p = 0; p < 3; p++) { n = c2[p];
              for (i = 0; i <= n; i++) a[off++] = (int)(((long)i * X) / n); }
          ok("a leitura depois de um proj com -1 continua bem formada",
             jev_marginal(X) == JEV_OK); }
    }

    /* ── a recusa: projeção fora de classe ──────────────────────────────── */
    {
        /* X=16, d=2, c=[1,1]; a fronteira da dimensão 0 é [10,20) mas as
         * coordenadas são 0..15. Portanto x<10 fica fora de toda a classe, e o
         * -1 do original escrevia em D1-1, que é a última fronteira. */
        int X = 16, d = 2, C = X + 2 + d, B, off, D1, S;
        int cs[3] = { 1, 1, 1 }, cs2[3] = { 1, 1, 1 };
        char q[128];
        for (x = 0; x < X; x++) G[x] = 1;
        memset(jev_arena(), 0, JEV_ARENA_BYTES);
        for (x = 0; x < X; x++) a[x] = G[x];
        a[X] = X; a[X + 1] = d;
        a[X + 2] = cs[0]; a[X + 3] = cs[1];
        for (x = 0; x < X; x++) a[C + 0 * X + x] = x;
        for (x = 0; x < X; x++) a[C + 1 * X + x] = 0;
        B = C + d * X;
        off = B;
        a[off++] = 10; a[off++] = 20;      /* dim 0: so [10,20) */
        a[off++] = 0;  a[off++] = 1;       /* dim 1: [0,1)   */
        S = 2 + 2; D1 = B + S;

        memcpy(guardado, jev_arena(), JEV_ARENA_BYTES);
        ok("a entrada esta de facto partida: o motor tem de recusar",
           jev_marginal_d(X) == JEV_ERR_CLASSE);
        sprintf(q, "a fronteira de entrada foi sobrescrita (a[%d]=%d, era %d)",
                D1 - 1, a[D1 - 1], guardado[D1 - 1]);
        ok(q, a[D1 - 1] != guardado[D1 - 1]);
        { int intacto = 1;
          for (x = 0; x < X; x++) if (a[x] != G[x]) intacto = 0;
          ok("mas o campo G sobrevive intacto", intacto); }
        /* e a leitura seguinte não herda a mancha */
        memcpy(jev_arena(), guardado, JEV_ARENA_BYTES);
        a[X + 2] = cs2[0]; a[X + 3] = cs2[1];
        off = B;
        a[off++] = 0; a[off++] = X;
        a[off++] = 0; a[off++] = 1;
        for (x = 0; x < X; x++) a[C + 0 * X + x] = x;
        ok("depois de uma recusa, uma entrada bem formada corre bem",
           jev_marginal_d(X) == JEV_OK);
    }

    printf("jev_api: %d verificações, %d falhas\n", feitas, falhas);
    return falhas ? 1 : 0;
}
