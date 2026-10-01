/* pgwire_sess.h — estado Extended Query (statement/portal em RAM de sessão). */
#ifndef PGWIRE_SESS_H
#define PGWIRE_SESS_H

#include "pgwire.h"
#include "sql_api.h"

#define PG_SESS_NAME  64
#define PG_SESS_SQL   512
#define PG_SESS_PARAM 8
#define PG_SESS_PVAL  64

typedef struct {
    int vivo;
    char nome[PG_SESS_NAME];
    char sql[PG_SESS_SQL];
    int nparams;
    int32_t oids[PG_SESS_PARAM];
} PgStmt;

typedef struct {
    int vivo;
    char nome[PG_SESS_NAME];
    char sql[PG_SESS_SQL];   /* após Bind ($n substituído) */
} PgPortal;

typedef struct {
    PgStmt stmt_u;            /* unnamed "" */
    PgStmt stmt_n;            /* um nomeado */
    PgPortal portal_u;
    PgPortal portal_n;
    int precisa_sync;         /* 1 após erro no extended */
} PgSess;

static void pg_sess_limpa(PgSess *s){ memset(s, 0, sizeof *s); }

static PgStmt *pg_stmt_slot(PgSess *s, const char *nome){
    if(!nome || !nome[0]) return &s->stmt_u;
    if(s->stmt_n.vivo && strcmp(s->stmt_n.nome, nome) == 0) return &s->stmt_n;
    return &s->stmt_n; /* sobrescreve o nomeado */
}
static PgStmt *pg_stmt_acha(PgSess *s, const char *nome){
    if(!nome || !nome[0]) return s->stmt_u.vivo ? &s->stmt_u : NULL;
    if(s->stmt_n.vivo && strcmp(s->stmt_n.nome, nome) == 0) return &s->stmt_n;
    return NULL;
}
static PgPortal *pg_portal_slot(PgSess *s, const char *nome){
    if(!nome || !nome[0]) return &s->portal_u;
    return &s->portal_n;
}
static PgPortal *pg_portal_acha(PgSess *s, const char *nome){
    if(!nome || !nome[0]) return s->portal_u.vivo ? &s->portal_u : NULL;
    if(s->portal_n.vivo && strcmp(s->portal_n.nome, nome) == 0) return &s->portal_n;
    return NULL;
}

/* Lê cstr em *i; devolve ponteiro interno a pay. */
static const char *pg_cstr_at(const uint8_t *pay, int pn, int *i){
    const char *s;
    if(*i >= pn) return NULL;
    s = (const char *)(pay + *i);
    *i += (int)strlen(s) + 1;
    if(*i > pn) return NULL;
    return s;
}

/* Despacho Extended (+ Query/Terminate). Acumula resposta em out.
 * Devolve: 1=ok continue, 0=Terminate, -1=erro fatal I/O lógico. */
static int pg_sess_fe(PgSess *sess, char tipo, const uint8_t *pay, int pn, PgBuf *out){
    if(tipo == PG_MSG_TERMINATE) return 0;

    if(tipo == PG_MSG_QUERY){
        SqlOut so;
        memset(&so, 0, sizeof so);
        sql_executa((const char *)pay, &so);
        pg_reply_sql(out, &so);
        sess->precisa_sync = 0;
        return 1;
    }

    if(tipo == PG_MSG_SYNC){
        pg_put_ready(out, sess->precisa_sync ? PG_TX_ERROR : PG_TX_IDLE);
        sess->precisa_sync = 0;
        return 1;
    }

    if(tipo == PG_MSG_FLUSH) return 1; /* nada a enviar */

    if(sess->precisa_sync){
        /* ignora até Sync (fluxo PG após erro no extended) */
        return 1;
    }

    if(tipo == PG_MSG_PARSE){
        int i = 0, k;
        const char *nome, *sql;
        int16_t np;
        PgStmt *st;
        nome = pg_cstr_at(pay, pn, &i);
        sql = pg_cstr_at(pay, pn, &i);
        if(!nome || !sql || i + 2 > pn){
            pg_put_error(out, "08P01", "Parse malformado");
            sess->precisa_sync = 1;
            return 1;
        }
        np = pg_get_i16(pay + i); i += 2;
        if(np < 0 || np > PG_SESS_PARAM || i + 4 * np > pn){
            pg_put_error(out, "08P01", "Parse params");
            sess->precisa_sync = 1;
            return 1;
        }
        if(strlen(sql) >= PG_SESS_SQL){
            pg_put_error(out, "54000", "query demasiado longa");
            sess->precisa_sync = 1;
            return 1;
        }
        st = pg_stmt_slot(sess, nome);
        memset(st, 0, sizeof *st);
        st->vivo = 1;
        snprintf(st->nome, sizeof st->nome, "%s", nome);
        snprintf(st->sql, sizeof st->sql, "%s", sql);
        st->nparams = np;
        for(k = 0; k < np; k++){
            st->oids[k] = pg_get_i32(pay + i); i += 4;
        }
        pg_put_parse_complete(out);
        return 1;
    }

    if(tipo == PG_MSG_BIND){
        int i = 0, k;
        const char *portal, *stmt;
        int16_t nfmt, np, nres;
        char vals[PG_SESS_PARAM][PG_SESS_PVAL];
        PgStmt *st;
        PgPortal *po;
        portal = pg_cstr_at(pay, pn, &i);
        stmt = pg_cstr_at(pay, pn, &i);
        if(!portal || !stmt || i + 2 > pn){
            pg_put_error(out, "08P01", "Bind malformado");
            sess->precisa_sync = 1;
            return 1;
        }
        nfmt = pg_get_i16(pay + i); i += 2;
        if(nfmt < 0 || i + 2 * nfmt > pn){
            pg_put_error(out, "08P01", "Bind formats");
            sess->precisa_sync = 1;
            return 1;
        }
        i += 2 * nfmt; /* ignoramos: só texto/int textual */
        if(i + 2 > pn){ pg_put_error(out, "08P01", "Bind nparams"); sess->precisa_sync = 1; return 1; }
        np = pg_get_i16(pay + i); i += 2;
        if(np < 0 || np > PG_SESS_PARAM){
            pg_put_error(out, "08P01", "Bind nparams");
            sess->precisa_sync = 1;
            return 1;
        }
        memset(vals, 0, sizeof vals);
        for(k = 0; k < np; k++){
            int32_t L;
            if(i + 4 > pn){ pg_put_error(out, "08P01", "Bind val"); sess->precisa_sync = 1; return 1; }
            L = pg_get_i32(pay + i); i += 4;
            if(L < 0){ /* NULL → "0" fraco; recusamos */
                pg_put_error(out, "22004", "NULL param nao suportado");
                sess->precisa_sync = 1;
                return 1;
            }
            if(i + L > pn || L >= PG_SESS_PVAL){
                pg_put_error(out, "08P01", "Bind val len");
                sess->precisa_sync = 1;
                return 1;
            }
            if(L == 4 && nfmt == 1){
                /* int4 binário */
                int32_t v = pg_get_i32(pay + i);
                snprintf(vals[k], sizeof vals[k], "%d", (int)v);
            }else{
                memcpy(vals[k], pay + i, (size_t)L);
                vals[k][L] = 0;
            }
            i += L;
        }
        if(i + 2 > pn){ pg_put_error(out, "08P01", "Bind nres"); sess->precisa_sync = 1; return 1; }
        nres = pg_get_i16(pay + i); i += 2;
        if(nres < 0 || i + 2 * nres > pn){
            pg_put_error(out, "08P01", "Bind result formats");
            sess->precisa_sync = 1;
            return 1;
        }
        st = pg_stmt_acha(sess, stmt);
        if(!st){
            pg_put_error(out, "26000", "statement inexistente");
            sess->precisa_sync = 1;
            return 1;
        }
        if(np != st->nparams && st->nparams != 0){
            /* permite Parse com nparams e Bind a bater */
            if(np != st->nparams){
                pg_put_error(out, "08P01", "Bind arity");
                sess->precisa_sync = 1;
                return 1;
            }
        }
        po = pg_portal_slot(sess, portal);
        memset(po, 0, sizeof *po);
        if(!pg_sql_subst(po->sql, sizeof po->sql, st->sql, np, vals)){
            pg_put_error(out, "08P01", "subst $n falhou");
            sess->precisa_sync = 1;
            return 1;
        }
        po->vivo = 1;
        snprintf(po->nome, sizeof po->nome, "%s", portal);
        pg_put_bind_complete(out);
        return 1;
    }

    if(tipo == PG_MSG_EXECUTE){
        int i = 0;
        const char *portal;
        PgPortal *po;
        SqlOut so;
        portal = pg_cstr_at(pay, pn, &i);
        if(!portal){
            pg_put_error(out, "08P01", "Execute malformado");
            sess->precisa_sync = 1;
            return 1;
        }
        /* maxrows ignorado (0 = todas) */
        po = pg_portal_acha(sess, portal);
        if(!po){
            pg_put_error(out, "34000", "portal inexistente");
            sess->precisa_sync = 1;
            return 1;
        }
        memset(&so, 0, sizeof so);
        sql_executa(po->sql, &so);
        if(!so.ok) sess->precisa_sync = 1;
        pg_reply_sql_rows(out, &so);
        return 1;
    }

    if(tipo == PG_MSG_DESCRIBE){
        char alvo;
        const char *nome;
        int i = 0;
        if(pn < 1){ pg_put_error(out, "08P01", "Describe"); sess->precisa_sync = 1; return 1; }
        alvo = (char)pay[0]; i = 1;
        nome = pg_cstr_at(pay, pn, &i);
        if(!nome){ pg_put_error(out, "08P01", "Describe nome"); sess->precisa_sync = 1; return 1; }
        if(alvo == 'S'){
            PgStmt *st = pg_stmt_acha(sess, nome);
            if(!st){ pg_put_error(out, "26000", "statement"); sess->precisa_sync = 1; return 1; }
            pg_put_param_description(out, st->nparams, st->oids);
            pg_put_no_data(out); /* colunas só após Execute neste Trio */
            return 1;
        }
        if(alvo == 'P'){
            PgPortal *po = pg_portal_acha(sess, nome);
            if(!po){ pg_put_error(out, "34000", "portal"); sess->precisa_sync = 1; return 1; }
            pg_put_no_data(out);
            return 1;
        }
        pg_put_error(out, "08P01", "Describe alvo");
        sess->precisa_sync = 1;
        return 1;
    }

    if(tipo == 'C'){ /* Close frontend (mesmo byte que CmdComplete backend) */
        char alvo;
        const char *nome;
        int i = 0;
        if(pn < 1){ pg_put_error(out, "08P01", "Close"); sess->precisa_sync = 1; return 1; }
        alvo = (char)pay[0]; i = 1;
        nome = pg_cstr_at(pay, pn, &i);
        if(!nome){ pg_put_error(out, "08P01", "Close nome"); sess->precisa_sync = 1; return 1; }
        if(alvo == 'S'){
            PgStmt *st = pg_stmt_acha(sess, nome);
            if(st) st->vivo = 0;
        }else if(alvo == 'P'){
            PgPortal *po = pg_portal_acha(sess, nome);
            if(po) po->vivo = 0;
        }
        pg_put_close_complete(out);
        return 1;
    }

    pg_put_error(out, "0A000", "mensagem FEBE nao suportada");
    if(tipo == PG_MSG_QUERY){ /* unreachable */ }
    else sess->precisa_sync = 1;
    /* Simple Query path already handled; unknown → Ready só se não extended.
     * Para manter clientes simples: se parece extended, espera Sync. */
    return 1;
}

#endif
