/* test_sql_hist_jev.c — a CAMADA SQL do JEV, de ponta a ponta.
 *
 * Os testes que já existiam (test_jev_oraculo.c, test_jev_api.c) provam a PORTA:
 * chamam jev_marginal/jev_escore/jev_duas/jev_marginal_d directamente contra a
 * arena. Nenhum deles passa por sql_hist_jev(), e é por isso que F9, F10 e F17
 * sobreviveram à prova de 96 casos: a porta estava certa, o invólucro não.
 *
 * Este ficheiro exercita o caminho completo
 *
 *     tabela SQL -> linhas vivas -> hist_local -> arena G -> jev_marginal
 *                -> D1 -> SqlOut
 *
 * e verifica os VALORES de gq, nunca apenas nrows.
 *
 * Contratos exercitados aqui: E1, E3 (limite do buffer e limite do suporte),
 * E5, E6, E7. E2 e E4 são reportados como NI com a razão.
 *
 * Não duplica a matemática: os valores esperados são escritos à mão a partir
 * de G(x)=|pi^-1(x)| e de q_b(x)=c_j sse b_j <= C_d(x) < b_{j+1}.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sql_api.h"
#include "jev_api.h"

static int falhas = 0;

static void check(const char *id, const char *desc, int cond, const char *evid){
    printf("%s | %s | %s | %s\n", cond ? "PASS" : "FAIL", id, desc, evid);
    if(!cond) falhas++;
}

static char evid[256];

/* ---- a arena é do chamador: cada chamada é isolada e verificada ---------- */

static unsigned char padrao = 0;          /* alterna, para provar que o
                                             resultado não depende do que
                                             estava na arena antes */
static int      arena_intacta = 1;
static int      n_chamadas     = 0;

static SqlOut   out;                      /* grande: em estático, não em stack */
static long     pi1 = 0, pi2 = 0;

static void sela(unsigned char *b){
    for(int i = 0; i < JEV_ARENA_BYTES; i++)
        b[i] = (unsigned char)(0xA5 ^ (i & 0xFF) ^ padrao);
}

/* Uma chamada completa, com a arena selada antes e comparada byte a byte
 * depois. O padrão vai para `antes` E PARA A ARENA REAL: a arena é um
 * `static unsigned char[65536]` zero-inicializado (banco/jev.c), e comparar
 * o fim da chamada com um padrão que a arena nunca teve seria um PASS
 * vazio que não prova o restore de nada. Toda a informação sobre
 * restauração de arena sai daqui. */
static int chama(const char *tab, const char *col, const char *spec){
    static unsigned char antes[JEV_ARENA_BYTES];
    padrao = (unsigned char)(padrao + 1);
    sela(antes);
    memcpy(jev_arena(), antes, JEV_ARENA_BYTES);
    memset(&out, 0, sizeof out);
    pi1 = 0; pi2 = 0;
    int r = sql_hist_jev(tab, col, &out, &pi1, &pi2, spec);
    if(memcmp(jev_arena(), antes, JEV_ARENA_BYTES) != 0) arena_intacta = 0;
    n_chamadas++;
    return r;
}

static int gqv(int j){ return atoi(out.cell[j][1]); }

/* ---- preparação da base -------------------------------------------------- */

static char base[512];

static int exec1(const char *sql){
    SqlOut o;
    memset(&o, 0, sizeof o);
    return sql_executa(sql, &o);
}

static int popula(const char *tab, const int *vals, int n){
    char buf[128];
    int maus = 0;
    for(int i = 0; i < n; i++){
        snprintf(buf, sizeof buf, "INSERT INTO %s VALUES (%d)", tab, vals[i]);
        if(exec1(buf) != 1) maus++;
    }
    return maus;
}

int main(void){
    const char *t = getenv("TEMP");
    if(!t || !*t) t = getenv("TMP");
    if(!t || !*t) t = ".";
#ifdef _WIN32
    snprintf(base, sizeof base, "%s\\tiffanydb_sqlhist_jev", t);
#else
    snprintf(base, sizeof base, "%s/tiffanydb_sqlhist_jev", t);
#endif

    /* sql_abrir faz ftruncate do .mem: a base e o catalogo sao (re)criados de
     * zero. Por isso tudo acontece numa unica sessao — criar e medir em
     * sessoes diferentes perde a tabela. */
    if(sql_abrir(base) != 1){
        printf("ERRO: sql_abrir(%s) != 1\n", base);
        return 2;
    }

    /* --- tabelas --------------------------------------------------------- */

    /* nominal: valores 0..9, |I|=10 */
    int v_nom[10] = {0,1,2,3,4,5,6,7,8,9};
    /* fronteira: so v=9=X-1 */
    int v_fro[1] = {9};
    /* F17: v == X */
    int v_eq[1] = {10};
    /* F17: v > X */
    int v_gt[1] = {50};
    /* E3 pelo limite do buffer: v >= 16384 */
    int v_alt[1] = {20000};

    struct { const char *nome; const char *tipo; const int *vals; int n; } tab[] = {
        { "nom",  "INTEIRO", v_nom, 10 },
        { "fron", "INTEIRO", v_fro,  1 },
        { "f17eq","INTEIRO", v_eq,   1 },
        { "f17gt","INTEIRO", v_gt,   1 },
        { "alto", "INTEIRO", v_alt,  1 },
        /* A coluna e TEXTO para E1 — mas a tabela TEM DE TER LINHAS: o teste
         * "E7: tabela vazia" (sql.c:16298) dispara ANTES de G1, e uma tabela
         * vazia testaria E7 e nao E1. */
        { "txt",  "TEXTO",   NULL,   0 },
        /* Duas colunas, e so uma e nomeada no INSERT: a outra nasce AUSENTE
         * (sql.c:3795, e S_PRES=0 em sql.c:4060). E o caminho para E2.
         * Sai do array porque sao DUAS colunas e o array so sabe criar uma. */
    };

    for(unsigned t2 = 0; t2 < sizeof tab / sizeof tab[0]; t2++){
        char ddl[160];
        snprintf(ddl, sizeof ddl, "CREATE TABLE %s (c %s)", tab[t2].nome, tab[t2].tipo);
        if(exec1(ddl) != 1){
            printf("ERRO: CREATE TABLE %s falhou\n", tab[t2].nome);
            return 2;
        }
        if(tab[t2].vals && popula(tab[t2].nome, tab[t2].vals, tab[t2].n) != 0){
            printf("ERRO: populacao de %s falhou\n", tab[t2].nome);
            return 2;
        }
    }
    if(exec1("CREATE TABLE lac (a INTEIRO, b INTEIRO)") != 1){
        printf("ERRO: CREATE TABLE lac falhou\n");
        return 2;
    }
    if(exec1("INSERT INTO txt (c) VALUES ('abc')") != 1){
        printf("ERRO: nao consegui popular a coluna TEXTO\n");
        return 2;
    }
    /* so a coluna `a` e nomeada: `b` fica de fora e nasce ausente */
    if(exec1("INSERT INTO lac (a) VALUES (5)") != 1){
        printf("ERRO: nao consegui criar a linha com coluna ausente\n");
        return 2;
    }

    /* spec canonica do caso nominal: X=10, k=3, B=(0,2,7,10) -> 00a3002007 */
    const char *S10K3 = "00a3002007";

    /* =====================================================================
     * T1 — CASO NOMINAL.  Gq = (2,5,3)
     *
     *   valores 0..9, |I| = 10
     *   b_0..b_3 = 0,2,7,10
     *   G_q(c_0) = |{0,1}|       = 2
     *   G_q(c_1) = |{2,3,4,5,6}| = 5
     *   G_q(c_2) = |{7,8,9}|     = 3
     *   soma = 10 = |I|
     * ===================================================================== */
    {
        int r = chama("nom", "c", S10K3);
        int ok = (r == 1) && (out.ok == 1) && (out.nrows == 3);
        snprintf(evid, sizeof evid,
                 "r=%d ok=%d nrows=%d", r, out.ok, out.nrows);
        check("T1a", "caso nominal aceita e devolve 3 linhas", ok, evid);

        snprintf(evid, sizeof evid, "gq=(%d,%d,%d) esperado (2,5,3)",
                 gqv(0), gqv(1), gqv(2));
        check("T1b", "gq = (2,5,3)", gqv(0)==2 && gqv(1)==5 && gqv(2)==3, evid);

        snprintf(evid, sizeof evid, "gq=(%d,%d,%d)", gqv(0), gqv(1), gqv(2));
        check("T1c", "gq NAO e identicamente zero (prova F9)",
              !(gqv(0)==0 && gqv(1)==0 && gqv(2)==0), evid);

        int soma = gqv(0) + gqv(1) + gqv(2);
        snprintf(evid, sizeof evid, "soma G_q=%d  |I|=%ld  i_total=%s",
                 soma, pi1, out.cell[0][2]);
        check("T1d", "soma_j G_q(c_j) = |I| = 10", soma == 10 && pi1 == 10, evid);
        /* A POSICAO de D1 nao e observavel por aqui: a SQL so devolve gq. E o
         * que o patch mexe em D1 prova-se contra a primitiva (harness H4),
         * onde D1 = X+k+10 = 23 e o antigo X+k+52 = 65 se ve. Aqui o que
         * fica e que os VALORES sobrevivem certos, o que ja e o contrato. */
    }

    /* =====================================================================
     * T2 — FRONTEIRA.  v = X-1 = 9 cai no ultimo intervalo valido [7,10)
     * ===================================================================== */
    {
        int r = chama("fron", "c", S10K3);
        snprintf(evid, sizeof evid, "r=%d ok=%d gq=(%d,%d,%d) esperado (0,0,1)",
                 r, out.ok, gqv(0), gqv(1), gqv(2));
        check("T2", "v = X-1 entra no ultimo intervalo",
              r == 1 && out.ok == 1 && gqv(0)==0 && gqv(1)==0 && gqv(2)==1, evid);
    }

    /* =====================================================================
     * T3/T4 — F17.  v >= X torna a realizacao invalida: pi: I -> X e aplicacao
     * total (redes/jev.tex def:realizacao) e C: X -> {0..|X|-1} e bijecao
     * (campos.tex def:suporte-coordenado). Rejeicao, nao truncamento.
     * ===================================================================== */
    {
        int r = chama("f17eq", "c", S10K3);
        snprintf(evid, sizeof evid, "r=%d ok=%d err=\"%s\"", r, out.ok, out.err);
        check("T3", "F17: v = X = 10 e rejeitado",
              r == 0 && out.ok == 0 && strstr(out.err, "E3") != NULL, evid);

        r = chama("f17gt", "c", S10K3);
        snprintf(evid, sizeof evid, "r=%d ok=%d err=\"%s\"", r, out.ok, out.err);
        check("T4", "F17: v = 50 > X e rejeitado na mesma classe",
              r == 0 && out.ok == 0 && strstr(out.err, "E3") != NULL, evid);
    }

    /* =====================================================================
     * T5..T11 — E1, E3 (buffer), E5, E6, E7
     * ===================================================================== */
    {
        int r = chama("alto", "c", S10K3);
        snprintf(evid, sizeof evid, "r=%d ok=%d err=\"%s\"", r, out.ok, out.err);
        check("T5", "E3: v = 20000 >= 16384 (limite do buffer) rejeitado",
              r == 0 && out.ok == 0 && strstr(out.err, "E3") != NULL, evid);
    }
    {
        int r = chama("txt", "c", S10K3);
        snprintf(evid, sizeof evid, "r=%d ok=%d err=\"%s\"", r, out.ok, out.err);
        check("T6", "E1: coluna TEXTO rejeitada (corpo != CORPO_INTEIRO)",
              r == 0 && out.ok == 0 && strstr(out.err, "E1") != NULL, evid);
    }
    {
        int r = chama("nao_existe", "c", S10K3);
        snprintf(evid, sizeof evid, "r=%d ok=%d err=\"%s\"", r, out.ok, out.err);
        check("T7", "E7: tabela inexistente rejeitada",
              r == 0 && out.ok == 0 && strstr(out.err, "E7") != NULL, evid);
    }
    {
        int r = chama("nom", "nao_existe", S10K3);
        snprintf(evid, sizeof evid, "r=%d ok=%d err=\"%s\"", r, out.ok, out.err);
        check("T8", "E7: coluna inexistente rejeitada",
              r == 0 && out.ok == 0 && strstr(out.err, "E7") != NULL, evid);
    }
    {
        /* X3 = "zzz" = 46655, k=1  ->  X+2k = 46657 > 16230 */
        int r = chama("nom", "c", "zzz1");
        snprintf(evid, sizeof evid, "r=%d ok=%d err=\"%s\"", r, out.ok, out.err);
        check("T9", "E5: X+2k > 16230 rejeitado",
              r == 0 && out.ok == 0 && strstr(out.err, "E5") != NULL, evid);
    }
    {
        /* B = (0,7,2,10): b1 > b2, nao crescente */
        int r = chama("nom", "c", "00a3007002");
        snprintf(evid, sizeof evid, "r=%d ok=%d err=\"%s\"", r, out.ok, out.err);
        check("T10", "E6: fronteiras nao crescentes rejeitadas",
              r == 0 && out.ok == 0 && strstr(out.err, "E6") != NULL, evid);
    }
    {
        /* B = (0,0,2,10): b1 = 0, fora de (0,X) */
        int r = chama("nom", "c", "00a3000002");
        snprintf(evid, sizeof evid, "r=%d ok=%d err=\"%s\"", r, out.ok, out.err);
        check("T11", "E6: b1 = 0 fora de (0,X) rejeitado",
              r == 0 && out.ok == 0 && strstr(out.err, "E6") != NULL, evid);
    }

    /* =====================================================================
     * T12 — E4 (k > 16).  ALCANCE, nao omissao.
     *
     * spec_decode() ja recusa k>16 antes de G4 ver o valor, logo a branch
     * "E4: k=%d" e codigo morto para k proveniente da spec. Este teste fixa o
     * comportamento real em vez de fingir que G4 e alcancavel.
     * ===================================================================== */
    {
        char spec17[64];
        int n = snprintf(spec17, sizeof spec17, "00a");
        spec17[n++] = 'h';                       /* k = 17 */
        for(int b = 0; b < 16; b++){ spec17[n++] = '0'; spec17[n++] = '0'; spec17[n++] = '1'; }
        spec17[n] = 0;
        int r = chama("nom", "c", spec17);
        snprintf(evid, sizeof evid,
                 "r=%d ok=%d err=\"%s\"  (spec_decode recusa k>16 antes de G4)",
                 r, out.ok, out.err);
        check("T12", "k = 17 rejeitado; G4 e inalcancavel via spec", r == 0 && out.ok == 0, evid);
    }

    /* =====================================================================
     * T13 — E2 (presenca incompleta).
     *
     * A tabela `lac` tem DUAS colunas e o INSERT nomeia SO `a`. A coluna que
     * ninguem nomeou nasce AUSENTE (sql.c:3795) e o INSERT escreve-lhe
     * S_PRES = 0 (sql.c:4060, `(j < nv && !nulo[j])`). A linha esta VIVA, o
     * G1 passa porque `b` e INTEIRO, e o G2 encontra a celula sem presenca.
     *
     * Nao se pode fazer isto em `nom`: um NULL ai acrescentaria uma linha a
     * |I| e a todos os testes de `nom` que viram a seguir.
     * ===================================================================== */
    {
        int r = chama("lac", "b", S10K3);
        snprintf(evid, sizeof evid, "r=%d ok=%d err=\"%s\"", r, out.ok, out.err);
        check("T13", "E2: coluna ausente numa linha viva rejeitada",
              r == 0 && out.ok == 0 && strstr(out.err, "E2") != NULL, evid);
    }

    /* =====================================================================
     * T14 — DETERMINISMO.  A mesma chamada com duas arenas seladas diferentes
     * tem de dar o mesmo gq: prova que o resultado vem de G e nao do residuo
     * que estava na arena.
     * ===================================================================== */
    {
        char g1[3][8], g2[3][8];
        chama("nom", "c", S10K3);
        for(int j = 0; j < 3; j++) snprintf(g1[j], 8, "%s", out.cell[j][1]);
        chama("nom", "c", S10K3);
        for(int j = 0; j < 3; j++) snprintf(g2[j], 8, "%s", out.cell[j][1]);
        snprintf(evid, sizeof evid, "1a=(%s,%s,%s) 2a=(%s,%s,%s)",
                 g1[0], g1[1], g1[2], g2[0], g2[1], g2[2]);
        check("T14", "duas arenas diferentes, mesmo gq",
              strcmp(g1[0],g2[0])==0 && strcmp(g1[1],g2[1])==0 && strcmp(g1[2],g2[2])==0,
              evid);
    }

    /* =====================================================================
     * T15 — ARENA.  Todas as N chamadas (sucesso e erro) deixaram a arena
     * byte a byte como estava.
     * ===================================================================== */
    {
        snprintf(evid, sizeof evid, "%d chamadas, arena intacta em todas: %s",
                 n_chamadas, arena_intacta ? "sim" : "NAO");
        check("T15", "arena restaurada em sucesso e em erro", arena_intacta, evid);
    }

    sql_fechar();

    printf("\nRESUMO: %d falhas\n", falhas);
    return falhas ? 1 : 0;
}
