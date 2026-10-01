/* pgcat.h — TRIO PG6: o catálogo mínimo, e ele é da SESSÃO e não do banco.
 *
 * Um cliente real (psql, um driver) não começa por consultar dados: começa por
 * perguntar QUEM É o servidor e em que estado está. Essas perguntas não tocam no
 * .mem nem na ISA — são sobre a ligação. Por isso o catálogo é uma FACHADA
 * ANTES do motor, e não mais um comando dentro dele:
 *
 *     sql_executa(sql) ─┬─ pgcat_responde(sql)  →  tratou?  devolve
 *                       └─ executa(sql)         →  o motor, o bytecode, o disco
 *
 * O que se trata aqui, e nada mais:
 *
 *     SELECT version()                a identidade do servidor
 *     SHOW <nome> | SHOW ALL          ler um parâmetro de sessão
 *     SET <nome> = <valor>            escrever um parâmetro de sessão
 *     SELECT current_database()       a base aberta
 *     SELECT current_schema()         o esquema corrente
 *     SELECT current_user | user      quem está ligado
 *     BEGIN | COMMIT | ROLLBACK       as tags que os drivers esperam
 *
 * E A IDA GUARDA A VOLTA: `SET x = v` seguido de `SHOW x` devolve v. Um
 * catálogo que aceitasse o SET e não o lesse de volta não seria catálogo — era
 * um sorvedouro. É essa a asserção que dá conteúdo a esta camada.
 *
 * SEM RAM DINÂMICA: os parâmetros vivem num arranjo fixo, decidido em compilação.
 */
#ifndef PGCAT_H
#define PGCAT_H

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <strings.h>
#include <dirent.h>
#include <unistd.h>
#include "sql_api.h"
#include "pgmsg.h"
#include "pgfunc.h"

#define PGCAT_MAX_PARAM   24
#define PGCAT_NOME        48
#define PGCAT_VALOR       64
#define PGCAT_VERSAO      "Tiffany-pgwire/0.1"

typedef struct {
    char nome[PGCAT_NOME];
    char valor[PGCAT_VALOR];
} PgParam;

static PgParam pgcat_par[PGCAT_MAX_PARAM] = {
    /* os que o handshake já anuncia — aqui para poderem ser lidos e escritos */
    { "server_version",  PGCAT_VERSAO },
    { "client_encoding", "UTF8"       },
    { "server_encoding", "UTF8"       },
    { "DateStyle",       "ISO, MDY"   },
    { "TimeZone",        "UTC"        },
    { "application_name","" },
    { "search_path",     "public"     },
    { "is_superuser",    "off"        },
};
static int  pgcat_n = 8;
static char pgcat_base[128] = "tiffany";
static char pgcat_user[64]  = "tiffany";

/* ── utilitários: comparação sem maiúsculas, e o corte dos brancos ─────────── */
static char baixa1_pgcat(char c){ return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }
static int pgcat_ieq(const char *a, const char *b){
    while(*a && *b){
        if(tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}
static const char *pgcat_pula(const char *s){
    while(*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    return s;
}
/* consome a palavra se ela lá estiver; devolve o ponteiro depois dela, ou NULL */
static const char *pgcat_pal(const char *s, const char *pal){
    size_t n = strlen(pal);
    s = pgcat_pula(s);
    if(strncasecmp(s, pal, n)) return NULL;
    if(s[n] && !strchr(" \t\r\n;(", s[n])) return NULL;
    return pgcat_pula(s + n);
}
/* copia o identificador (letras, dígitos, _) e devolve o resto */
static const char *pgcat_ident(const char *s, char *dst, int cap){
    int i = 0;
    s = pgcat_pula(s);
    while(*s && (isalnum((unsigned char)*s) || *s == '_' || *s == '.')){
        if(i + 1 < cap) dst[i++] = *s;
        s++;
    }
    dst[i] = 0;
    return s;
}
/* tira aspas simples e o ponto e vírgula final */
static void pgcat_limpa(char *v){
    int n = (int)strlen(v);
    while(n > 0 && (v[n-1] == ';' || v[n-1] == ' ' || v[n-1] == '\t')) v[--n] = 0;
    if(n >= 2 && ((v[0] == '\'' && v[n-1] == '\'') || (v[0] == '"' && v[n-1] == '"'))){
        memmove(v, v + 1, (size_t)(n - 2));
        v[n-2] = 0;
    }
}

/* o valor de um parâmetro de sessão, ou NULL --- para quem precisa de o LER e
 * não de o mostrar. É a mesma tabela que o SHOW usa, e por isso as duas
 * leituras não podem discordar. */
static const char *pgcat_valor(const char *nome);

static PgParam *pgcat_acha(const char *nome){
    for(int i = 0; i < pgcat_n; i++)
        if(pgcat_ieq(pgcat_par[i].nome, nome)) return &pgcat_par[i];
    return NULL;
}
static const char *pgcat_valor(const char *nome){
    PgParam *r = pgcat_acha(nome);
    return r ? r->valor : NULL;
}


/* ── as respostas, todas com a forma que o wire espera ─────────────────────── */
static void pgcat_uma(SqlOut *o, const char *col, const char *val, const char *tag){
    if(!o) return;
    memset(o, 0, sizeof *o);
    o->ok = 1; o->ncols = 1; o->nrows = 1;
    o->tipo[0] = SQL_TIPO_TEXT;          /* o catálogo devolve TEXTO, não int4 */
    snprintf(o->col[0], sizeof o->col[0], "%s", col);
    snprintf(o->cell[0][0], sizeof o->cell[0][0], "%s", val);
    snprintf(o->tag, sizeof o->tag, "%s", tag);
}
static void pgcat_so_tag(SqlOut *o, const char *tag){
    if(!o) return;
    memset(o, 0, sizeof *o);
    o->ok = 1; o->ncols = 0; o->nrows = 0;
    snprintf(o->tag, sizeof o->tag, "%s", tag);
}

/* ── o OID de uma tabela: estável, e derivado do NOME ────────────────────────
 *
 * O `\d cliente` precisa de um oid para depois pedir as colunas por ele. O
 * número tem de ser ESTÁVEL entre corridas — se mudasse, a segunda consulta do
 * psql não encontraria a tabela que a primeira lhe deu — e por isso sai do nome,
 * e não de um contador. Vive acima de 16384, a zona dos objectos de utilizador.
 */
static int pgcat_oid_de(const char *nome){
    unsigned h = 2166136261u;                  /* FNV-1a, e chega para isto */
    for(const char *p = nome; p && *p; p++){
        h ^= (unsigned char)baixa1_pgcat(*p);
        h *= 16777619u;
    }
    return (int)(16384u + (h % 40000u));
}

/* ── \dt e \d: RECONHECIMENTO DE PADRÃO, e convém dizer o que isto é ─────────
 *
 * O `\dt` do psql não pergunta «que tabelas há»: manda uma consulta com JOIN
 * sobre pg_class e pg_namespace, um CASE de oito ramos, duas funções de
 * catálogo e um operador de expressão regular. Implementar aquilo a sério é
 * implementar um pg_catalog e um motor que faça junções — e nenhum dos dois
 * existe aqui.
 *
 * O que se faz é outra coisa, e a distinção importa: RECONHECE-SE a consulta
 * pela sua assinatura e responde-se com as tabelas que o disco tem. É uma
 * camada de compatibilidade, não um catálogo, e a fronteira é essa: qualquer
 * consulta a pg_catalog que não seja esta continua a ser recusada, e é bom que
 * seja — recusar é honesto, responder ao acaso não.
 * ───────────────────────────────────────────────────────────────────────────── */
static char pgcat_dir[512]  = ".";
static char pgcat_pref[128] = "";

/* A PRIMEIRA consulta do `\d <tabela>`: pg_class com o nome dentro de um regex
 * `^(nome)$`. Reconhece-se pela assinatura e devolve-se oid, esquema e nome —
 * mas SÓ se a tabela existir de facto no disco: inventar um oid para uma tabela
 * que não há faria o psql pedir as colunas de coisa nenhuma. */
static int pgcat_e_procura_tabela(const char *sql, char *nome, int cap){
    const char *a, *b;
    if(!strstr(sql, "pg_catalog.pg_class")) return 0;
    if(!strstr(sql, "relname")) return 0;
    a = strstr(sql, "'^(");
    if(!a) return 0;
    a += 3;
    b = strstr(a, ")$'");
    if(!b || b - a >= cap) return 0;
    snprintf(nome, (size_t)(b - a + 1), "%s", a);
    return 1;
}

static int pgcat_e_lista_tabelas(const char *sql){
    /* a assinatura: as três coisas que só a consulta do \dt tem juntas */
    return strstr(sql, "pg_catalog.pg_class") != NULL
        && strstr(sql, "relkind")            != NULL
        && strstr(sql, "relname")            != NULL;
}

static int pgcat_lista_tabelas(SqlOut *out){
    DIR *d;
    struct dirent *e;
    int n = 0;
    size_t plen = strlen(pgcat_pref);
    if(!out) return 1;
    memset(out, 0, sizeof *out);
    out->ok = 1; out->ncols = 4;
    out->tipo[0] = out->tipo[1] = out->tipo[2] = out->tipo[3] = SQL_TIPO_TEXT;
    snprintf(out->col[0], sizeof out->col[0], "Schema");
    snprintf(out->col[1], sizeof out->col[1], "Name");
    snprintf(out->col[2], sizeof out->col[2], "Type");
    snprintf(out->col[3], sizeof out->col[3], "Owner");
    d = opendir(pgcat_dir);
    if(d){
        while((e = readdir(d)) != NULL && n < SQL_OUT_MAX_ROWS){
            const char *nm = e->d_name;
            size_t L = strlen(nm);
            if(plen == 0 || strncmp(nm, pgcat_pref, plen)) continue;
            if(L < plen + 4 || strcmp(nm + L - 4, ".mem")) continue;
            {
                size_t corpo = L - plen - 4;
                if(corpo == 0) continue;              /* a base sem tabela nomeada */
                snprintf(out->cell[n][0], SQL_OUT_CELL, "public");
                snprintf(out->cell[n][1], SQL_OUT_CELL, "%.*s", (int)corpo, nm + plen);
                snprintf(out->cell[n][2], SQL_OUT_CELL, "table");
                snprintf(out->cell[n][3], SQL_OUT_CELL, "%s", pgcat_user);
                n++;
            }
        }
        closedir(d);
    }
    /* ordenado por nome, como o ORDER BY 1,2 da consulta pede */
    for(int i = 0; i < n; i++) for(int j = i + 1; j < n; j++)
        if(strcmp(out->cell[i][1], out->cell[j][1]) > 0){
            char t[SQL_OUT_MAX_COLS][SQL_OUT_CELL];
            memcpy(t, out->cell[i], sizeof t);
            memcpy(out->cell[i], out->cell[j], sizeof t);
            memcpy(out->cell[j], t, sizeof t);
        }
    out->nrows = n;
    snprintf(out->tag, sizeof out->tag, "SELECT %d", n);
    return 1;
}

/* ── pg_type: uma TABELA de catálogo, e um SELECT pequeno por cima ───────────
 *
 * O `\dt` resolveu-se por reconhecimento de assinatura, e disse-se que era isso.
 * Aqui faz-se o contrário, porque se pode: `pg_type` É uma tabela — linhas e
 * colunas —, e o que falta é um SELECT que a saiba ler. Escreve-se esse SELECT,
 * pequeno e declarado:
 *
 *     SELECT <colunas|*> FROM [pg_catalog.]pg_type [WHERE <col> = <valor>]
 *
 * Sem junções, sem ORDER BY, sem funções. O que não couber nisto é RECUSADO —
 * e a recusa é a parte honesta: um catálogo que respondesse por aproximação
 * seria pior do que não existir, porque o cliente confia no que ele diz.
 *
 * E OS TIPOS SÃO OS QUE ESTE SERVIDOR SABE ANUNCIAR, nem mais um. Pôr aqui uma
 * lista longa copiada do Postgres seria descrever um servidor que não somos: o
 * motor guarda inteiros, e o catálogo declara texto. Os outros estão na tabela
 * porque um cliente pode perguntar pelo OID deles — não porque os produzamos.
 * ───────────────────────────────────────────────────────────────────────────── */
typedef struct {
    int oid;
    const char *typname;
    int typlen;           /* −1 = variável */
    const char *categoria;/* typcategory do Postgres: N numérico, S string, B booleano */
    int anunciado;        /* 1 = este servidor produz colunas deste tipo */
} PgTipo;

static const PgTipo pgcat_tipo_tab[] = {
    { PG_OID_BOOL,    "bool",     1, "B", 0 },
    { PG_OID_INT8,    "int8",     8, "N", 0 },
    { PG_OID_INT2,    "int2",     2, "N", 0 },
    { PG_OID_INT4,    "int4",     4, "N", 1 },   /* o motor */
    { PG_OID_TEXT,    "text",    -1, "S", 1 },   /* o catálogo */
    { PG_OID_OID,     "oid",      4, "N", 0 },
    { PG_OID_FLOAT8,  "float8",   8, "N", 0 },
    { PG_OID_VARCHAR, "varchar", -1, "S", 0 },
    { PG_OID_NUMERIC, "numeric", -1, "N", 0 },
    /* o vector do pgvector: texto no formato [a,b,c]. Este servidor PRODUZ
     * vetores (as funções abaixo), mas não os GUARDA — o motor guarda inteiros,
     * e dizer o contrário seria descrever um armazenamento que não existe. */
    { PG_OID_VECTOR,  "vector",  -1, "U", 1 },
};
#define PGCAT_NTIPOS ((int)(sizeof pgcat_tipo_tab / sizeof pgcat_tipo_tab[0]))

/* as colunas que se sabem servir; qualquer outra faz a consulta ser recusada */
static const char *pgcat_tipo_cols[] = {
    "oid", "typname", "typlen", "typcategory", "typtype", "typnamespace"
};
#define PGCAT_NTCOLS ((int)(sizeof pgcat_tipo_cols / sizeof pgcat_tipo_cols[0]))

static int pgcat_tipo_col_idx(const char *nome){
    for(int i = 0; i < PGCAT_NTCOLS; i++)
        if(pgcat_ieq(nome, pgcat_tipo_cols[i])) return i;
    return -1;
}
static void pgcat_tipo_valor(const PgTipo *t, int c, char *dst, int cap){
    switch(c){
        case 0: snprintf(dst, cap, "%d", t->oid);       break;
        case 1: snprintf(dst, cap, "%s", t->typname);   break;
        case 2: snprintf(dst, cap, "%d", t->typlen);    break;
        case 3: snprintf(dst, cap, "%s", t->categoria); break;
        case 4: snprintf(dst, cap, "b");                break;  /* typtype: base */
        case 5: snprintf(dst, cap, "11");               break;  /* pg_catalog   */
        default: dst[0] = 0;
    }
}
/* o tipo da COLUNA do catálogo — para o RowDescription não mentir outra vez */
static int pgcat_tipo_col_oid(int c){
    return (c == 0 || c == 2 || c == 5) ? SQL_TIPO_INT4 : SQL_TIPO_TEXT;
}

/* SELECT … FROM pg_type [WHERE col = valor]. Devolve 1 se tratou. */
static int pgcat_le_pg_type(const char *sql, SqlOut *out){
    const char *p, *q;
    char lista[256] = "", tab[64] = "", wcol[48] = "", wval[64] = "";
    int cols[SQL_OUT_MAX_COLS], nc = 0, tudo = 0, tem_where = 0;

    if((p = pgcat_pal(sql, "SELECT")) == NULL) return 0;
    /* a lista de colunas, até ao FROM */
    q = p;
    { int i = 0;
      while(*q && strncasecmp(q, "FROM", 4)){
          if(i + 1 < (int)sizeof lista) lista[i++] = *q;
          q++;
      }
      lista[i] = 0;
    }
    if(!*q) return 0;
    q = pgcat_pula(q + 4);
    q = pgcat_ident(q, tab, sizeof tab);
    if(!strncasecmp(tab, "pg_catalog.", 11)) memmove(tab, tab + 11, strlen(tab) - 10);
    if(!pgcat_ieq(tab, "pg_type")) return 0;          /* não é esta tabela: passa */

    /* DAQUI EM DIANTE A CONSULTA É NOSSA. O que não coubermos servir é ERRO COM
     * MENSAGEM — não se devolve ao motor, que responderia «zero colunas, zero
     * linhas» com sucesso, e um erro disfarçado de resultado vazio é o pior
     * desfecho para quem chama. */
    #define PGT_RECUSA(msg) do {                                            \
        if(out){ memset(out, 0, sizeof *out); out->ok = 0;                  \
                 snprintf(out->err, sizeof out->err,                        \
                          "pg_type: %s — este catálogo serve apenas "       \
                          "SELECT <colunas> FROM pg_type [WHERE <col> = <valor>]", (msg)); } \
        return 1;                                                           \
    } while(0)

    /* o WHERE, se houver — uma igualdade e mais nada */
    q = pgcat_pula(q);
    if(*q == ';') q++;
    q = pgcat_pula(q);
    if(*q){
        const char *w = pgcat_pal(q, "WHERE");
        if(!w) PGT_RECUSA("só se entende WHERE depois do nome da tabela");
        w = pgcat_ident(w, wcol, sizeof wcol);
        w = pgcat_pula(w);
        if(*w != '=') PGT_RECUSA("no WHERE só se entende uma igualdade");
        w = pgcat_pula(w + 1);
        snprintf(wval, sizeof wval, "%s", w);
        pgcat_limpa(wval);
        if(pgcat_tipo_col_idx(wcol) < 0) PGT_RECUSA("coluna desconhecida no WHERE");
        tem_where = 1;
    }

    /* as colunas pedidas */
    {
        const char *r = lista;
        char nome[48];
        while(*r){
            while(*r == ' ' || *r == ',' || *r == '\t' || *r == '\n') r++;
            if(!*r) break;
            if(*r == '*'){ tudo = 1; r++; continue; }
            { const char *ini = r;
              int i = 0;
              while(*r && *r != ',' && *r != ' ' && *r != '\t' && *r != '\n'){
                  if(i + 1 < (int)sizeof nome) nome[i++] = *r;
                  r++;
              }
              nome[i] = 0;
              if(!i){ (void)ini; continue; }
              if(!strncasecmp(nome, "t.", 2)) memmove(nome, nome + 2, strlen(nome) - 1);
              { int c = pgcat_tipo_col_idx(nome);
                if(c < 0) PGT_RECUSA("coluna que este catálogo não serve");
                if(nc < SQL_OUT_MAX_COLS) cols[nc++] = c;
              }
            }
        }
    }
    if(tudo){ nc = 0; for(int i = 0; i < PGCAT_NTCOLS && nc < SQL_OUT_MAX_COLS; i++) cols[nc++] = i; }
    if(nc == 0) PGT_RECUSA("nenhuma coluna reconhecida");

    if(out){
        int n = 0;
        memset(out, 0, sizeof *out);
        out->ok = 1; out->ncols = nc;
        for(int j = 0; j < nc; j++){
            snprintf(out->col[j], sizeof out->col[j], "%s", pgcat_tipo_cols[cols[j]]);
            out->tipo[j] = pgcat_tipo_col_oid(cols[j]);
        }
        for(int i = 0; i < PGCAT_NTIPOS && n < SQL_OUT_MAX_ROWS; i++){
            if(tem_where){
                char v[64];
                pgcat_tipo_valor(&pgcat_tipo_tab[i], pgcat_tipo_col_idx(wcol), v, sizeof v);
                if(strcmp(v, wval)) continue;
            }
            for(int j = 0; j < nc; j++)
                pgcat_tipo_valor(&pgcat_tipo_tab[i], cols[j], out->cell[n][j], SQL_OUT_CELL);
            n++;
        }
        out->nrows = n;
        snprintf(out->tag, sizeof out->tag, "SELECT %d", n);
    }
    #undef PGT_RECUSA
    return 1;
}

/* ── SELECT tiffany_xxx(args) e SELECT … FROM pg_proc ────────────────────── */
static int pgcat_chama_func(const char *sql, SqlOut *out){
    const char *p, *q;
    char nome[64], a1[PGF_ARG_MAX] = "", a2[PGF_ARG_MAX] = "", res[SQL_OUT_CELL];
    const PgFunc *f;
    int nargs = 0;

    if((p = pgcat_pal(sql, "SELECT")) == NULL) return 0;
    q = pgcat_ident(p, nome, sizeof nome);
    if(!strncasecmp(nome, "pg_catalog.", 11)) memmove(nome, nome + 11, strlen(nome) - 10);
    f = pgfunc_acha(nome);
    if(!f) return 0;                                  /* não é função nossa */

    #define PGF_RECUSA(msg) do {                                              \
        if(out){ memset(out, 0, sizeof *out); out->ok = 0;                    \
                 snprintf(out->err, sizeof out->err, "%s: %s", nome, (msg)); }\
        return 1;                                                             \
    } while(0)

    q = pgcat_pula(q);
    if(*q != '(') PGF_RECUSA("faltam os parênteses");
    q = pgcat_pula(q + 1);
    if(*q != ')'){
        int i = 0;
        while(*q && *q != ',' && *q != ')' && i + 1 < PGF_ARG_MAX) a1[i++] = *q++;
        a1[i] = 0; pgcat_limpa(a1); nargs = 1;
        q = pgcat_pula(q);
        if(*q == ','){
            q = pgcat_pula(q + 1); i = 0;
            while(*q && *q != ')' && i + 1 < PGF_ARG_MAX) a2[i++] = *q++;
            a2[i] = 0; pgcat_limpa(a2); nargs = 2;
        }
    }
    if(*q != ')') PGF_RECUSA("faltou fechar os parênteses");
    if(nargs != f->nargs) PGF_RECUSA("número de argumentos errado");

    res[0] = 0;
    if(f->especie == PGF_INTERNA){
        if(!strcasecmp(f->nome, "tiffany_versao")) snprintf(res, sizeof res, "%s", PGCAT_VERSAO);
        else if(!strcasecmp(f->nome, "tiffany_walsh")){
            if(!pgfunc_arg_limpo(a1) || !pgfunc_arg_limpo(a2))
                PGF_RECUSA("argumento com caracteres não permitidos");
            pgf_walsh(atoi(a1), atoi(a2), res, (int)sizeof res);
        }
        else if(!strcasecmp(f->nome, "tiffany_campo")){
            if(!pgfunc_arg_limpo(a1)) PGF_RECUSA("argumento com caracteres não permitidos");
            pgf_campo_recta(atoi(a1), res, (int)sizeof res);
        }
    }else{
        if(nargs && !pgfunc_arg_limpo(a1))
            PGF_RECUSA("argumento com caracteres não permitidos — a chamada é recusada,"
                       " e não saneada em silêncio");
        if(!pgf_corre_script(f, nargs ? a1 : NULL, res, (int)sizeof res))
            PGF_RECUSA("o script não devolveu nada");
    }
    if(!res[0]) PGF_RECUSA("sem resultado");
    if(out){
        memset(out, 0, sizeof *out);
        out->ok = 1; out->ncols = 1; out->nrows = 1;
        out->tipo[0] = (f->devolve == PG_OID_INT4) ? SQL_TIPO_INT4 : SQL_TIPO_TEXT;
        snprintf(out->col[0], sizeof out->col[0], "%s", f->nome);
        snprintf(out->cell[0][0], SQL_OUT_CELL, "%s", res);
        snprintf(out->tag, sizeof out->tag, "SELECT 1");
    }
    #undef PGF_RECUSA
    return 1;
}

/* pg_proc: o cliente tem de poder DESCOBRIR o que há, ou as funções são
 * segredo — e uma função que só quem escreveu conhece não é uma interface. */
static int pgcat_le_pg_proc(const char *sql, SqlOut *out){
    const char *p, *q;
    char tab[64];
    if((p = pgcat_pal(sql, "SELECT")) == NULL) return 0;
    q = strstr(p, "FROM");
    if(!q) return 0;
    q = pgcat_pula(q + 4);
    q = pgcat_ident(q, tab, sizeof tab);
    if(!strncasecmp(tab, "pg_catalog.", 11)) memmove(tab, tab + 11, strlen(tab) - 10);
    if(!pgcat_ieq(tab, "pg_proc")) return 0;
    if(out){
        int n = 0;
        memset(out, 0, sizeof *out);
        out->ok = 1; out->ncols = 4;
        out->tipo[0] = SQL_TIPO_TEXT; out->tipo[1] = SQL_TIPO_INT4;
        out->tipo[2] = SQL_TIPO_INT4; out->tipo[3] = SQL_TIPO_TEXT;
        snprintf(out->col[0], sizeof out->col[0], "proname");
        snprintf(out->col[1], sizeof out->col[1], "pronargs");
        snprintf(out->col[2], sizeof out->col[2], "prorettype");
        snprintf(out->col[3], sizeof out->col[3], "descricao");
        for(int i = 0; i < PGF_N && n < SQL_OUT_MAX_ROWS; i++){
            snprintf(out->cell[n][0], SQL_OUT_CELL, "%s", pgfunc_tab[i].nome);
            snprintf(out->cell[n][1], SQL_OUT_CELL, "%d", pgfunc_tab[i].nargs);
            snprintf(out->cell[n][2], SQL_OUT_CELL, "%d", pgfunc_tab[i].devolve);
            snprintf(out->cell[n][3], SQL_OUT_CELL, "%s", pgfunc_tab[i].doc);
            n++;
        }
        out->nrows = n;
        snprintf(out->tag, sizeof out->tag, "SELECT %d", n);
    }
    return 1;
}

/* Devolve 1 se ESTA camada tratou a consulta (e preencheu out); 0 se não é
 * dela — e então o motor corre, como se esta camada não existisse. */
static int pgcat_responde(const char *sql, SqlOut *out){
    const char *p;
    char nome[PGCAT_NOME], valor[PGCAT_VALOR];

    if(!sql) return 0;

    /* ── SHOW ───────────────────────────────────────────────────────────── */
    if((p = pgcat_pal(sql, "SHOW")) != NULL){
        p = pgcat_ident(p, nome, sizeof nome);
        if(pgcat_ieq(nome, "ALL")){
            if(out){
                memset(out, 0, sizeof *out);
                out->ok = 1; out->ncols = 2;
                out->tipo[0] = out->tipo[1] = SQL_TIPO_TEXT;
                snprintf(out->col[0], sizeof out->col[0], "name");
                snprintf(out->col[1], sizeof out->col[1], "setting");
                int n = pgcat_n < SQL_OUT_MAX_ROWS ? pgcat_n : SQL_OUT_MAX_ROWS;
                for(int i = 0; i < n; i++){
                    snprintf(out->cell[i][0], SQL_OUT_CELL, "%s", pgcat_par[i].nome);
                    snprintf(out->cell[i][1], SQL_OUT_CELL, "%s", pgcat_par[i].valor);
                }
                out->nrows = n;
                snprintf(out->tag, sizeof out->tag, "SHOW");
            }
            return 1;
        }
        {
            PgParam *q = pgcat_acha(nome);
            /* um parâmetro desconhecido é ERRO, e não uma linha vazia: o cliente
             * tem de saber a diferença entre «vale isto» e «não existe». */
            if(!q){
                if(out){
                    memset(out, 0, sizeof *out);
                    out->ok = 0;
                    snprintf(out->err, sizeof out->err,
                             "unrecognized configuration parameter \"%s\"", nome);
                }
                return 1;
            }
            pgcat_uma(out, q->nome, q->valor, "SHOW");
            return 1;
        }
    }

    /* ── SET nome = valor  |  SET nome TO valor ─────────────────────────── */
    if((p = pgcat_pal(sql, "SET")) != NULL){
        const char *q;
        p = pgcat_ident(p, nome, sizeof nome);
        p = pgcat_pula(p);
        if(*p == '=') p++;
        else if((q = pgcat_pal(p, "TO")) != NULL) p = q;
        else return 0;                       /* não é a nossa forma: passa */
        p = pgcat_pula(p);
        snprintf(valor, sizeof valor, "%s", p);
        pgcat_limpa(valor);
        {
            PgParam *r = pgcat_acha(nome);
            if(!r && pgcat_n < PGCAT_MAX_PARAM) r = &pgcat_par[pgcat_n++];
            if(r){
                snprintf(r->nome, sizeof r->nome, "%s", nome);
                snprintf(r->valor, sizeof r->valor, "%s", valor);
            }
        }
        pgcat_so_tag(out, "SET");
        return 1;
    }

    /* ── as transacções, e agora elas DESFAZEM ──────────────────────────
     *
     * Até aqui o ROLLBACK era recusado, porque devolver a tag sem desfazer era
     * mentir. Passou a desfazer, e o mecanismo é o do levantamento: ESCREVER É
     * QUOCIENTAR — o valor novo cola-se por cima do velho e a célula esquece
     * qual era —, e guardar o valor ANTERIOR é a coordenada que desdobra.
     * Desfaz-se lendo a pilha AO CONTRÁRIO, ou um endereço escrito duas vezes
     * ficaria com o valor do meio.
     *
     * E O TECTO CONTINUA DECLARADO: se a pilha encher, o ROLLBACK RECUSA em vez
     * de desfazer metade. Meia volta deixa a base num estado que nunca existiu,
     * e isso é pior do que não voltar. */
    if(pgcat_pal(sql, "BEGIN")){ sql_tx_abre(); pgcat_so_tag(out, "BEGIN"); return 1; }
    if(pgcat_pal(sql, "COMMIT")){ sql_tx_fecha(); pgcat_so_tag(out, "COMMIT"); return 1; }
    if(pgcat_pal(sql, "ROLLBACK")){
        if(sql_tx_desfaz()){ sql_tx_fecha(); pgcat_so_tag(out, "ROLLBACK"); return 1; }
        if(out){
            memset(out, 0, sizeof *out);
            out->ok = 0;
            snprintf(out->err, sizeof out->err,
                     "ROLLBACK recusado: a transaccao escreveu mais do que a pilha de"
                     " desfazer comporta. Nada foi desfeito — meia volta seria pior.");
        }
        sql_tx_fecha();
        return 1;
    }

    /* ── a procura de UMA tabela pelo nome (1.ª consulta do \d) ─────────── */
    {
        char alvo_nome[64];
        if(pgcat_e_procura_tabela(sql, alvo_nome, sizeof alvo_nome)){
            char cam[700];
            snprintf(cam, sizeof cam, "%s/%s%s.mem", pgcat_dir, pgcat_pref, alvo_nome);
            if(out){
                memset(out, 0, sizeof *out);
                out->ok = 1; out->ncols = 3;
                out->tipo[0] = SQL_TIPO_INT4;
                out->tipo[1] = out->tipo[2] = SQL_TIPO_TEXT;
                snprintf(out->col[0], sizeof out->col[0], "oid");
                snprintf(out->col[1], sizeof out->col[1], "nspname");
                snprintf(out->col[2], sizeof out->col[2], "relname");
                if(access(cam, F_OK) == 0){        /* só se existir mesmo */
                    snprintf(out->cell[0][0], SQL_OUT_CELL, "%d", pgcat_oid_de(alvo_nome));
                    snprintf(out->cell[0][1], SQL_OUT_CELL, "public");
                    snprintf(out->cell[0][2], SQL_OUT_CELL, "%s", alvo_nome);
                    out->nrows = 1;
                }else{
                    out->nrows = 0;                /* não há: zero linhas, sem erro */
                }
                snprintf(out->tag, sizeof out->tag, "SELECT %d", out->nrows);
            }
            return 1;
        }
    }

    /* ── pg_database: o `\l`. Há UMA base aberta, e é essa ───────────────
     * Não se inventa uma lista: este servidor serve a base com que arrancou, e
     * é ela que aparece. Nove colunas, lidas por posição pelo psql. */
    if(strstr(sql, "pg_catalog.pg_database") && strstr(sql, "datname")){
        if(out){
            const char *c[9] = { "Name", "Owner", "Encoding", "Locale Provider",
                                 "Collate", "Ctype", "ICU Locale", "ICU Rules",
                                 "Access privileges" };
            const char *v[9] = { pgcat_base, pgcat_user, "UTF8", "libc",
                                 "C", "C", "", "", "" };
            memset(out, 0, sizeof *out);
            out->ok = 1; out->ncols = 9; out->nrows = 1;
            for(int j = 0; j < 9; j++){
                snprintf(out->col[j], sizeof out->col[j], "%s", c[j]);
                out->tipo[j] = SQL_TIPO_TEXT;
                snprintf(out->cell[0][j], SQL_OUT_CELL, "%s", v[j]);
            }
            snprintf(out->tag, sizeof out->tag, "SELECT 1");
        }
        return 1;
    }

    /* ── as tabelas de catálogo que, aqui, estão VAZIAS ──────────────────
     *
     * Uma tabela desta casa não tem herança, nem índices, nem restrições, nem
     * regras, nem gatilhos, nem políticas. O `\d` pergunta por todas elas em
     * sequência, e a resposta certa é ZERO LINHAS — não é uma recusa.
     *
     * A distinção é a mesma do §W11 e vale a pena repeti-la: recusar diz «não
     * sei responder»; zero linhas diz «sei, e não há nada». Aqui SABE-SE: não
     * há mesmo. Recusar faria o psql tratar uma tabela perfeitamente descrita
     * como um erro. */
    {
        static const char *vazias[] = {
            "pg_catalog.pg_inherits", "pg_catalog.pg_index",
            "pg_catalog.pg_constraint", "pg_catalog.pg_rewrite",
            "pg_catalog.pg_trigger", "pg_catalog.pg_policy",
            "pg_catalog.pg_statistic_ext", "pg_catalog.pg_publication",
        };
        for(unsigned k = 0; k < sizeof vazias / sizeof vazias[0]; k++){
            if(strstr(sql, vazias[k])){
                if(out){
                    memset(out, 0, sizeof *out);
                    out->ok = 1; out->ncols = 1; out->nrows = 0;
                    out->tipo[0] = SQL_TIPO_TEXT;
                    snprintf(out->col[0], sizeof out->col[0], "%s", "?column?");
                    snprintf(out->tag, sizeof out->tag, "SELECT 0");
                }
                return 1;
            }
        }
    }

    /* ── a 3.ª consulta do \d: as COLUNAS, por oid da relação ────────────
     * Sete colunas, lidas por posição. O nome e o número vêm do MOTOR — o
     * catálogo não os inventa —, e o oid da relação resolve-se procurando entre
     * as tabelas do disco aquela cujo nome dá aquele oid. */
    if(strstr(sql, "pg_catalog.pg_attribute") && strstr(sql, "attrelid")){
        char alvo[64] = "";
        int oid_pedido = 0;
        { const char *a = strstr(sql, "attrelid = '");
          if(a) oid_pedido = atoi(a + 12); }
        if(oid_pedido){
            DIR *d = opendir(pgcat_dir);
            size_t plen = strlen(pgcat_pref);
            struct dirent *e;
            if(d){
                while((e = readdir(d)) != NULL){
                    const char *nm = e->d_name;
                    size_t L = strlen(nm);
                    if(plen == 0 || strncmp(nm, pgcat_pref, plen)) continue;
                    if(L < plen + 4 || strcmp(nm + L - 4, ".mem")) continue;
                    { char cand[64];
                      snprintf(cand, sizeof cand, "%.*s", (int)(L - plen - 4), nm + plen);
                      if(cand[0] && pgcat_oid_de(cand) == oid_pedido){
                          snprintf(alvo, sizeof alvo, "%s", cand); break; } }
                }
                closedir(d);
            }
        }
        if(out){
            char nomes[SQL_OUT_MAX_COLS][32];
            int n = alvo[0] ? sql_cols_de(alvo, nomes, SQL_OUT_MAX_COLS) : 0;
            memset(out, 0, sizeof *out);
            out->ok = 1; out->ncols = 7;
            { const char *c[7] = { "attname", "format_type", "pg_get_expr",
                                   "attnotnull", "attcollation", "attidentity",
                                   "attgenerated" };
              for(int j = 0; j < 7; j++){
                  snprintf(out->col[j], sizeof out->col[j], "%s", c[j]);
                  out->tipo[j] = SQL_TIPO_TEXT;
              } }
            for(int i = 0; i < n && i < SQL_OUT_MAX_ROWS; i++){
                snprintf(out->cell[i][0], SQL_OUT_CELL, "%s", nomes[i]);
                snprintf(out->cell[i][1], SQL_OUT_CELL, "integer");  /* o motor */
                out->cell[i][2][0] = 0;                              /* sem default */
                snprintf(out->cell[i][3], SQL_OUT_CELL, "f");        /* not null */
                out->cell[i][4][0] = 0;
                out->cell[i][5][0] = 0;
                out->cell[i][6][0] = 0;
            }
            out->nrows = n;
            snprintf(out->tag, sizeof out->tag, "SELECT %d", n);
        }
        return 1;
    }

    /* ── a 2.ª consulta do \d: as propriedades da relação, por oid ───────
     * Treze colunas, e o psql lê-as por POSIÇÃO — não por nome. Devolve-se o
     * que uma tabela desta casa é: sem índices, sem regras, sem gatilhos, sem
     * partições e sem tablespace. Dizer o contrário faria o psql pedir coisas
     * que não existem. */
    if(strstr(sql, "pg_catalog.pg_class") && strstr(sql, "relchecks")
       && strstr(sql, "relpersistence")){
        if(out){
            const char *v[13] = { "0", "r", "f", "f", "f", "f", "f",
                                  "f", "f", "", "0", "", "p" };
            const char *c[13] = { "relchecks", "relkind", "relhasindex",
                                  "relhasrules", "relhastriggers", "?column?",
                                  "?column?", "relhasoids", "relispartition",
                                  "?column?", "reltablespace", "?column?",
                                  "relpersistence" };
            memset(out, 0, sizeof *out);
            out->ok = 1;
            out->ncols = (13 > SQL_OUT_MAX_COLS) ? SQL_OUT_MAX_COLS : 13;
            for(int j = 0; j < out->ncols; j++){
                snprintf(out->col[j], sizeof out->col[j], "%s", c[j]);
                out->tipo[j] = (j == 0 || j == 10) ? SQL_TIPO_INT4 : SQL_TIPO_TEXT;
                snprintf(out->cell[0][j], SQL_OUT_CELL, "%s", v[j]);
            }
            out->nrows = 1;
            snprintf(out->tag, sizeof out->tag, "SELECT 1");
        }
        return 1;
    }

    /* ── a consulta do \dt, reconhecida pela assinatura ─────────────────── */
    if(pgcat_e_lista_tabelas(sql)) return pgcat_lista_tabelas(out);

    /* ── pg_type e pg_proc, lidos como tabelas ─────────────────────────── */
    if(pgcat_le_pg_type(sql, out)) return 1;
    if(pgcat_le_pg_proc(sql, out)) return 1;

    /* ── as funções da casa ─────────────────────────────────────────────── */
    if(pgcat_chama_func(sql, out)) return 1;

    /* ── SELECT <função de catálogo>() ──────────────────────────────────── */
    if((p = pgcat_pal(sql, "SELECT")) != NULL){
        const char *q = p;
        char alvo[PGCAT_NOME];
        q = pgcat_ident(q, alvo, sizeof alvo);
        /* pg_catalog.version() ≡ version() */
        if(!strncasecmp(alvo, "pg_catalog.", 11)) memmove(alvo, alvo + 11, strlen(alvo) - 10);
        q = pgcat_pula(q);
        {
            int tem_par = (*q == '(');
            if(tem_par){ q++; q = pgcat_pula(q); if(*q != ')') return 0; }
            if(pgcat_ieq(alvo, "version") && tem_par){
                pgcat_uma(out, "version", PGCAT_VERSAO, "SELECT 1"); return 1; }
            if(pgcat_ieq(alvo, "current_database") && tem_par){
                pgcat_uma(out, "current_database", pgcat_base, "SELECT 1"); return 1; }
            if(pgcat_ieq(alvo, "current_schema") && tem_par){
                pgcat_uma(out, "current_schema", "public", "SELECT 1"); return 1; }
            if(pgcat_ieq(alvo, "current_user") || pgcat_ieq(alvo, "user")
               || pgcat_ieq(alvo, "session_user")){
                if(!tem_par){ pgcat_uma(out, alvo, pgcat_user, "SELECT 1"); return 1; }
            }
        }
        return 0;                            /* SELECT normal: é do motor */
    }

    return 0;
}

/* a base aberta, para o current_database() responder o que é */
static void pgcat_base_nome(const char *nome){
    const char *b = nome ? nome : "tiffany";
    const char *s = strrchr(b, '/');
    snprintf(pgcat_base, sizeof pgcat_base, "%s", s ? s + 1 : b);
    /* o directório e o prefixo dos ficheiros de tabela: <base>__<nome>.mem */
    if(s){
        int nd = (int)(s - b);
        snprintf(pgcat_dir, sizeof pgcat_dir, "%.*s", nd ? nd : 1, nd ? b : "/");
    }else{
        snprintf(pgcat_dir, sizeof pgcat_dir, ".");
    }
    snprintf(pgcat_pref, sizeof pgcat_pref, "%s__", pgcat_base);
}

#endif
