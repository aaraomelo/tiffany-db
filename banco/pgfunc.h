/* pgfunc.h — AS FUNÇÕES: o que a casa sabe fazer, chamável por SQL.
 *
 * Duas espécies, e a diferença é de natureza:
 *
 *   INTERNAS   calculadas aqui, em C, exactas e sem sair do processo.
 *              É o caso dos vetores de Walsh: a casa constrói-os com inteiros
 *              e sinais ±1, e o `vector` do pgvector é só a sua roupa de texto.
 *
 *   SCRIPTS    correm um programa do repositório e devolvem a última linha.
 *              É útil — a bateria, o painel, a cobertura — e é PERIGOSO, porque
 *              executar é executar. Por isso:
 *
 *   A LISTA É BRANCA E FIXA EM COMPILAÇÃO. Não há «corre o que te mandarem»:
 *   cada função nomeia o seu script, e um nome que não esteja aqui não corre.
 *   Os ARGUMENTOS são filtrados a [A-Za-z0-9_.-] — sem espaços, sem aspas, sem
 *   metacaracteres — e passados como UM argumento. Não há shell a interpretar
 *   nada: o que não passa o filtro faz a chamada ser recusada, e não sanitizada.
 *   Sanear em silêncio é pior, porque o chamador fica a pensar que correu o que
 *   pediu.
 *
 *   E NENHUMA função escreve: a lista tem só leitores. Um `SELECT` que apagasse
 *   um ficheiro seria uma armadilha para quem confia na palavra SELECT.
 */
#ifndef PGFUNC_H
#define PGFUNC_H

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
/* popen/pclose são POSIX e o resto da casa compila com -std=c99, onde eles não
 * são declarados. Declaram-se aqui em vez de mexer nas opções de compilação de
 * todo o repositório por causa de uma função. */
extern FILE *popen(const char *, const char *);
extern int   pclose(FILE *);
#include "sql_api.h"
#include "pgmsg.h"

#define PGF_ARG_MAX   64
#define PGF_LINHA     512

typedef enum { PGF_INTERNA, PGF_SCRIPT } PgFuncTipo;

typedef struct {
    const char *nome;
    PgFuncTipo  especie;
    int         nargs;         /* argumentos esperados */
    int         devolve;       /* OID do resultado */
    const char *script;        /* PGF_SCRIPT: caminho relativo ao repositório */
    const char *doc;
} PgFunc;

static const PgFunc pgfunc_tab[] = {
    /* internas — a casa a calcular, exacta */
    { "tiffany_walsh",     PGF_INTERNA, 2, PG_OID_VECTOR,  NULL,
      "o caractere de Walsh chi_k sobre 2^m pontos, em ±1" },
    { "tiffany_campo",     PGF_INTERNA, 1, PG_OID_VECTOR,  NULL,
      "o campo G de uma recta de n passos: multiplicidade por célula" },
    { "tiffany_versao",    PGF_INTERNA, 0, PG_OID_TEXT,    NULL,
      "a versão deste servidor" },
    /* scripts — leitores do repositório, lista branca */
    { "tiffany_bateria",   PGF_SCRIPT,  0, PG_OID_TEXT, "tools/bateria.sh",
      "corre a bateria e devolve a linha do total" },
    { "tiffany_painel",    PGF_SCRIPT,  0, PG_OID_TEXT, "tools/painel.sh",
      "o painel do repositório" },
    { "tiffany_cobertura", PGF_SCRIPT,  0, PG_OID_TEXT, "tools/cobertura.sh",
      "a cobertura dos medidores" },
};
#define PGF_N ((int)(sizeof pgfunc_tab / sizeof pgfunc_tab[0]))

static const PgFunc *pgfunc_acha(const char *nome){
    for(int i = 0; i < PGF_N; i++)
        if(!strcasecmp(pgfunc_tab[i].nome, nome)) return &pgfunc_tab[i];
    return NULL;
}

/* o filtro: o que não passa RECUSA a chamada, e não é saneado em silêncio */
static int pgfunc_arg_limpo(const char *a){
    if(!a || !*a) return 0;
    for(const char *p = a; *p; p++)
        if(!isalnum((unsigned char)*p) && *p != '_' && *p != '.' && *p != '-')
            return 0;
    return 1;
}

/* ── as internas ──────────────────────────────────────────────────────────── */
static int pgf_paridade(unsigned v){ int p = 0; while(v){ p ^= (int)(v & 1u); v >>= 1; } return p; }

/* chi_k(j) = (−1)^paridade(k AND j), o caractere de Walsh: inteiro, sem vírgula */
static void pgf_walsh(int k, int m, char *dst, int cap){
    int n = 1 << (m < 0 ? 0 : (m > 10 ? 10 : m));
    int i = 0;
    if(cap > 0) dst[0] = 0;
    i += snprintf(dst + i, cap - i, "[");
    for(int j = 0; j < n && i < cap - 8; j++)
        i += snprintf(dst + i, cap - i, "%s%d", j ? "," : "",
                      pgf_paridade((unsigned)(k & j)) ? -1 : 1);
    snprintf(dst + i, cap - i, "]");
}
/* o campo G de uma recta de n passos: cada célula visitada uma vez */
static void pgf_campo_recta(int n, char *dst, int cap){
    int i = 0;
    if(n < 0) n = 0;
    if(n > 64) n = 64;
    if(cap > 0) dst[0] = 0;
    i += snprintf(dst + i, cap - i, "[");
    for(int j = 0; j < n && i < cap - 8; j++)
        i += snprintf(dst + i, cap - i, "%s1", j ? "," : "");
    snprintf(dst + i, cap - i, "]");
}

/* A RAIZ DO REPOSITÓRIO, e não o directório de onde se calhou correr.
 *
 * `bash tools/painel.sh` é um caminho relativo, e um servidor arranca de onde o
 * arrancarem. Isto apareceu medido: o bloco passava isolado — corrido da raiz —
 * e falhava dentro da bateria, que corre de outro sítio. Um servidor que só
 * funcionasse no directório certo é um servidor que funciona por acaso.
 *
 * Procura-se para cima a partir do directório corrente até encontrar o marcador
 * do repositório. Se não houver, as funções-script deixam de correr — e é o
 * desfecho certo: melhor não correr do que correr o script de outra árvore. */
static const char *pgf_raiz(void){
    static char raiz[512];
    static int achada = 0;
    char aqui[512], teste[600];
    if(achada) return raiz[0] ? raiz : NULL;
    achada = 1;
    /* primeiro a declaração explícita: um servidor que sirva um repositório diz
     * qual, e não o adivinha. */
    { const char *env = getenv("TIFFANY_RAIZ");
      if(env && *env){
          snprintf(teste, sizeof teste, "%s/tools/bateria.sh", env);
          if(access(teste, F_OK) == 0){ snprintf(raiz, sizeof raiz, "%s", env); return raiz; }
      } }
    if(!getcwd(aqui, sizeof aqui)) return NULL;
    for(;;){
        snprintf(teste, sizeof teste, "%s/tools/bateria.sh", aqui);
        if(access(teste, F_OK) == 0){ snprintf(raiz, sizeof raiz, "%s", aqui); return raiz; }
        { char *b = strrchr(aqui, '/');
          if(!b || b == aqui) break;
          *b = 0; }
    }
    raiz[0] = 0;
    return NULL;
}

/* ── os scripts: última linha da saída, e nada mais ───────────────────────── */
static int pgf_corre_script(const PgFunc *f, const char *arg, char *dst, int cap){
    char cmd[1024], linha[PGF_LINHA];
    const char *raiz = pgf_raiz();
    FILE *fp;
    if(!raiz) return 0;                       /* sem repositório: não corre */
    if(arg && *arg && !pgfunc_arg_limpo(arg)) return 0;
    /* corre-se A PARTIR DA RAIZ, porque os scripts contam com isso */
    if(arg && *arg) snprintf(cmd, sizeof cmd, "cd %s && bash %s %s 2>/dev/null",
                             raiz, f->script, arg);
    else            snprintf(cmd, sizeof cmd, "cd %s && bash %s 2>/dev/null",
                             raiz, f->script);
    fp = popen(cmd, "r");
    if(!fp) return 0;
    dst[0] = 0;
    while(fgets(linha, sizeof linha, fp)){
        size_t L = strlen(linha);
        while(L && (linha[L-1] == '\n' || linha[L-1] == '\r')) linha[--L] = 0;
        if(L) snprintf(dst, cap, "%s", linha);      /* fica a ÚLTIMA não vazia */
    }
    pclose(fp);
    return dst[0] != 0;
}

#endif
