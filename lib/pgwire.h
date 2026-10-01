/* pgwire.h — FEBE em buffer: builders/parsers (Trios PG1–PG2 — sem TCP).
 *
 * Encode/decode exacto + Simple Query → SqlOut. Listener = Trio PG3.
 * Referência: docs.postgresql.org Protocol → Message Formats / Message Flow.
 * Compilar com -Ilib -Ibanco (sql_api.h).
 */
#ifndef PGWIRE_H
#define PGWIRE_H

#include "pgmsg.h"
#include "sql_api.h"
#include <stddef.h>
#include <string.h>

#ifndef PGWIRE_CAP
#define PGWIRE_CAP 4096
#endif

typedef struct {
    uint8_t b[PGWIRE_CAP];
    int n;          /* bytes escritos */
    int erro;       /* 1 se estourou ou parse falhou */
} PgBuf;

static void pg_buf_limpa(PgBuf *w){ w->n = 0; w->erro = 0; }

static int pg_buf_cabe(PgBuf *w, int need){
    if(w->erro) return 0;
    if(w->n + need > PGWIRE_CAP){ w->erro = 1; return 0; }
    return 1;
}
static void pg_buf_u8(PgBuf *w, uint8_t v){
    if(!pg_buf_cabe(w, 1)) return;
    w->b[w->n++] = v;
}
static void pg_buf_i32(PgBuf *w, int32_t v){
    if(!pg_buf_cabe(w, 4)) return;
    pg_put_i32(w->b + w->n, v);
    w->n += 4;
}
static void pg_buf_bytes(PgBuf *w, const void *src, int n){
    if(n < 0 || !pg_buf_cabe(w, n)) return;
    if(n) memcpy(w->b + w->n, src, (size_t)n);
    w->n += n;
}
static void pg_buf_cstr(PgBuf *w, const char *s){
    int L = (int)strlen(s ? s : "");
    pg_buf_bytes(w, s ? s : "", L);
    pg_buf_u8(w, 0);
}

/* Marca início de mensagem com tipo; devolve offset do Int32(len) para fechar. */
static int pg_msg_begin(PgBuf *w, char tipo){
    int off;
    pg_buf_u8(w, (uint8_t)tipo);
    off = w->n;
    pg_buf_i32(w, 0);           /* placeholder: len = 4 + payload */
    return off;
}
/* len = (fim - off), inclui o próprio campo len (não inclui o Byte1 tipo). */
static void pg_msg_end(PgBuf *w, int len_off){
    if(w->erro || len_off < 0 || len_off + 4 > w->n){ w->erro = 1; return; }
    pg_put_i32(w->b + len_off, (int32_t)(w->n - len_off));
}

/* ── Backend: AuthenticationOk ───────────────────────────────────────────────
 * 'R' + Int32(8) + Int32(0) */
static void pg_put_auth_ok(PgBuf *w){
    int off = pg_msg_begin(w, PG_MSG_AUTH);
    pg_buf_i32(w, PG_AUTH_OK);
    pg_msg_end(w, off);
}

/* ── Backend: ParameterStatus(name, value) ─────────────────────────────────── */
static void pg_put_param_status(PgBuf *w, const char *nome, const char *valor){
    int off = pg_msg_begin(w, PG_MSG_PARAM_STATUS);
    pg_buf_cstr(w, nome);
    pg_buf_cstr(w, valor);
    pg_msg_end(w, off);
}

/* ── Backend: BackendKeyData (forma clássica 3.0: pid + key, 4+4) ────────────
 * 'K' + Int32(12) + Int32(pid) + Int32(key) */
static void pg_put_backend_key(PgBuf *w, int32_t pid, int32_t key){
    int off = pg_msg_begin(w, PG_MSG_BACKEND_KEY);
    pg_buf_i32(w, pid);
    pg_buf_i32(w, key);
    pg_msg_end(w, off);
}

static void pg_buf_i16(PgBuf *w, int16_t v){
    if(!pg_buf_cabe(w, 2)) return;
    pg_put_i16(w->b + w->n, v);
    w->n += 2;
}

/* ── Backend: ReadyForQuery(status) ──────────────────────────────────────────
 * 'Z' + Int32(5) + Byte1(I|T|E) */
static void pg_put_ready(PgBuf *w, char tx){
    int off = pg_msg_begin(w, PG_MSG_READY);
    pg_buf_u8(w, (uint8_t)tx);
    pg_msg_end(w, off);
}

/* ── Backend: ErrorResponse mínimo (S, V, C, M) ────────────────────────────── */
static void pg_put_error(PgBuf *w, const char *sqlstate, const char *msg){
    int off = pg_msg_begin(w, PG_MSG_ERROR);
    pg_buf_u8(w, PG_ERR_SEVERITY);   pg_buf_cstr(w, "ERROR");
    pg_buf_u8(w, PG_ERR_SEVERITY_V); pg_buf_cstr(w, "ERROR");
    pg_buf_u8(w, PG_ERR_SQLSTATE);   pg_buf_cstr(w, sqlstate ? sqlstate : "XX000");
    pg_buf_u8(w, PG_ERR_MESSAGE);    pg_buf_cstr(w, msg ? msg : "error");
    pg_buf_u8(w, 0);                 /* terminator */
    pg_msg_end(w, off);
}

/* ── Backend: CommandComplete(tag) ───────────────────────────────────────────
 * 'C' + Int32(len) + String */
static void pg_put_command_complete(PgBuf *w, const char *tag){
    int off = pg_msg_begin(w, PG_MSG_CMD_COMPLETE);
    pg_buf_cstr(w, tag ? tag : "");
    pg_msg_end(w, off);
}

/* ── Backend: RowDescription (texto; OID int4 por coluna) ────────────────────
 * 'T' + Int16(nfields) + por campo: name\0 + tableOID + attr + typeOID +
 * typelen + typmod + format */
static void pg_put_row_description_t(PgBuf *w, int ncols, char col[][32],
                                     const int *tipo){
    int off = pg_msg_begin(w, PG_MSG_ROW_DESC);
    int i;
    pg_buf_i16(w, (int16_t)ncols);
    for(i = 0; i < ncols; i++){
        int oid = (tipo && tipo[i]) ? tipo[i] : PG_OID_INT4;
        pg_buf_cstr(w, col[i]);
        pg_buf_i32(w, 0);                 /* table OID */
        pg_buf_i16(w, (int16_t)(i + 1));  /* attr number */
        pg_buf_i32(w, oid);
        /* typlen ANDA COM O OID: 4 para int4, −1 (variável) para text. Anunciar
         * 4 num texto é dizer ao cliente que lá cabem quatro bytes. */
        pg_buf_i16(w, (int16_t)(oid == PG_OID_TEXT ? -1 : 4));
        pg_buf_i32(w, -1);                /* typmod */
        pg_buf_i16(w, 0);                 /* text format */
    }
    pg_msg_end(w, off);
}
static void pg_put_row_description(PgBuf *w, int ncols, char col[][32]){
    pg_put_row_description_t(w, ncols, col, NULL);
}

/* ── Backend: DataRow (valores em texto) ───────────────────────────────────── */
static void pg_put_data_row_n(PgBuf *w, int ncols, char cell[][SQL_OUT_CELL],
                              const unsigned char *nulo){
    int off = pg_msg_begin(w, PG_MSG_DATA_ROW);
    int j;
    pg_buf_i16(w, (int16_t)ncols);
    for(j = 0; j < ncols; j++){
        const char *s = cell[j];
        int L = (int)strlen(s ? s : "");
        /* −1 é a AUSÊNCIA e 0 é a cadeia vazia: níveis diferentes, e o
         * protocolo tem as duas letras. */
        if(nulo && nulo[j]){ pg_buf_i32(w, -1); continue; }
        pg_buf_i32(w, L);
        if(L) pg_buf_bytes(w, s, L);
    }
    pg_msg_end(w, off);
}
static void pg_put_data_row(PgBuf *w, int ncols, char cell[][SQL_OUT_CELL]){
    pg_put_data_row_n(w, ncols, cell, NULL);
}

/* SqlOut → linhas (sem Ready) — Extended Query / Execute */
static void pg_reply_sql_rows(PgBuf *w, const SqlOut *o){
    int i;
    if(!o || !o->ok){
        pg_put_error(w, "42601", (o && o->err[0]) ? o->err : "query failed");
        return;
    }
    if(o->ncols > 0){
        pg_put_row_description_t(w, o->ncols, (char (*)[32])o->col, o->tipo);
        for(i = 0; i < o->nrows; i++)
            pg_put_data_row_n(w, o->ncols, (char (*)[SQL_OUT_CELL])o->cell[i],
                              o->nulo[i]);
    }
    pg_put_command_complete(w, o->tag[0] ? o->tag : "SELECT 0");
}

/* SqlOut → Simple Query (rows + Ready) */
static void pg_reply_sql(PgBuf *w, const SqlOut *o){
    pg_reply_sql_rows(w, o);
    pg_put_ready(w, PG_TX_IDLE);
}

/* ── Extended Query: builders frontend ─────────────────────────────────────── */
static void pg_put_parse(PgBuf *w, const char *stmt, const char *sql,
                         int nparams, const int32_t *oids){
    int off = pg_msg_begin(w, PG_MSG_PARSE);
    int i;
    pg_buf_cstr(w, stmt ? stmt : "");
    pg_buf_cstr(w, sql ? sql : "");
    pg_buf_i16(w, (int16_t)nparams);
    for(i = 0; i < nparams; i++)
        pg_buf_i32(w, oids ? oids[i] : PG_OID_INT4);
    pg_msg_end(w, off);
}

/* Bind com 1 parâmetro em texto (formato 0) — caso medido do Trio PG4 */
static void pg_put_bind_text1(PgBuf *w, const char *portal, const char *stmt,
                              const char *val){
    int off = pg_msg_begin(w, PG_MSG_BIND);
    int L = (int)strlen(val ? val : "");
    pg_buf_cstr(w, portal ? portal : "");
    pg_buf_cstr(w, stmt ? stmt : "");
    pg_buf_i16(w, 0);                 /* sem format codes → tudo texto */
    pg_buf_i16(w, 1);                 /* 1 parâmetro */
    pg_buf_i32(w, L);
    if(L) pg_buf_bytes(w, val, L);
    pg_buf_i16(w, 0);                 /* result formats: texto */
    pg_msg_end(w, off);
}

static void pg_put_execute(PgBuf *w, const char *portal, int32_t maxrows){
    int off = pg_msg_begin(w, PG_MSG_EXECUTE);
    pg_buf_cstr(w, portal ? portal : "");
    pg_buf_i32(w, maxrows);
    pg_msg_end(w, off);
}

static void pg_put_sync(PgBuf *w){
    int off = pg_msg_begin(w, PG_MSG_SYNC);
    pg_msg_end(w, off);
}

static void pg_put_describe(PgBuf *w, char alvo /* 'S'|'P' */, const char *nome){
    int off = pg_msg_begin(w, PG_MSG_DESCRIBE);
    pg_buf_u8(w, (uint8_t)alvo);
    pg_buf_cstr(w, nome ? nome : "");
    pg_msg_end(w, off);
}

static void pg_put_close_fe(PgBuf *w, char alvo, const char *nome){
    int off = pg_msg_begin(w, PG_MSG_CLOSE);
    pg_buf_u8(w, (uint8_t)alvo);
    pg_buf_cstr(w, nome ? nome : "");
    pg_msg_end(w, off);
}

/* ── Extended Query: builders backend ──────────────────────────────────────── */
static void pg_put_parse_complete(PgBuf *w){
    int off = pg_msg_begin(w, PG_MSG_PARSE_COMPLETE);
    pg_msg_end(w, off);
}
static void pg_put_bind_complete(PgBuf *w){
    int off = pg_msg_begin(w, PG_MSG_BIND_COMPLETE);
    pg_msg_end(w, off);
}
static void pg_put_close_complete(PgBuf *w){
    int off = pg_msg_begin(w, PG_MSG_CLOSE_COMPLETE);
    pg_msg_end(w, off);
}
static void pg_put_no_data(PgBuf *w){
    int off = pg_msg_begin(w, PG_MSG_NO_DATA);
    pg_msg_end(w, off);
}
static void pg_put_param_description(PgBuf *w, int nparams, const int32_t *oids){
    int off = pg_msg_begin(w, PG_MSG_PARAM_DESC);
    int i;
    pg_buf_i16(w, (int16_t)nparams);
    for(i = 0; i < nparams; i++)
        pg_buf_i32(w, oids ? oids[i] : PG_OID_INT4);
    pg_msg_end(w, off);
}

/* Substitui $1..$n pelo texto do parâmetro (RAM de sessão; sql.c não vê $n). */
static int pg_sql_subst(char *dst, int cap, const char *sql,
                        int nparams, char val[][64]){
    int di = 0, i = 0;
    if(!dst || cap < 2 || !sql) return 0;
    while(sql[i] && di < cap - 1){
        if(sql[i] == '$' && sql[i+1] >= '1' && sql[i+1] <= '9'){
            int idx = sql[i+1] - '1';
            const char *v;
            int L, j;
            if(idx >= nparams) return 0;
            v = val[idx];
            L = (int)strlen(v);
            if(di + L >= cap) return 0;
            for(j = 0; j < L; j++) dst[di++] = v[j];
            i += 2;
            continue;
        }
        dst[di++] = sql[i++];
    }
    if(di >= cap) return 0;
    dst[di] = 0;
    return 1;
}

/* ── Frontend: Terminate ─────────────────────────────────────────────────────
 * 'X' + Int32(4) */
static void pg_put_terminate(PgBuf *w){
    int off = pg_msg_begin(w, PG_MSG_TERMINATE);
    pg_msg_end(w, off);
}

/* ── Frontend: Query (Simple) ──────────────────────────────────────────────── */
static void pg_put_query(PgBuf *w, const char *sql){
    int off = pg_msg_begin(w, PG_MSG_QUERY);
    pg_buf_cstr(w, sql ? sql : "");
    pg_msg_end(w, off);
}

/* ── Frontend: StartupMessage (sem type-byte) ────────────────────────────────
 * Int32(len) + Int32(proto) + (key\0 val\0)* + \0 */
static void pg_put_startup(PgBuf *w, int32_t proto,
                           const char *user, const char *database){
    int off = w->n;
    pg_buf_i32(w, 0);                /* placeholder len */
    pg_buf_i32(w, proto);
    pg_buf_cstr(w, "user");
    pg_buf_cstr(w, user ? user : "");
    if(database && database[0]){
        pg_buf_cstr(w, "database");
        pg_buf_cstr(w, database);
    }
    pg_buf_u8(w, 0);                 /* end of params */
    if(!w->erro)
        pg_put_i32(w->b + off, (int32_t)(w->n - off));
}

/* ── Frontend: SSLRequest (sem type-byte) ────────────────────────────────────
 * Int32(8) + Int32(80877103) */
static void pg_put_ssl_request(PgBuf *w){
    pg_buf_i32(w, 8);
    pg_buf_i32(w, PG_SSL_REQUEST_CODE);
}

/* Resposta a SSLRequest: um byte 'N' (recusa) ou 'S' */
static void pg_put_ssl_reply(PgBuf *w, int aceita){
    pg_buf_u8(w, (uint8_t)(aceita ? 'S' : 'N'));
}

/* ── Parse: primeiros 8 bytes sem tipo → SSLRequest ou início de Startup ─── */
typedef struct {
    int e_ssl;              /* 1 se SSLRequest */
    int32_t len;
    int32_t proto_ou_codigo;
} PgHead8;

static int pg_peek_head8(const uint8_t *b, int n, PgHead8 *o){
    if(n < 8 || !o) return 0;
    o->len = pg_get_i32(b);
    o->proto_ou_codigo = pg_get_i32(b + 4);
    o->e_ssl = (o->len == 8 && o->proto_ou_codigo == PG_SSL_REQUEST_CODE);
    return 1;
}

/* StartupMessage → user/database (buffers do chamador) */
typedef struct {
    int32_t proto;
    char user[64];
    char database[64];
    int ok;
} PgStartup;

static int pg_parse_startup(const uint8_t *b, int n, PgStartup *o){
    int32_t len, proto;
    int i;
    if(!o || n < 8) return 0;
    memset(o, 0, sizeof *o);
    len = pg_get_i32(b);
    if(len < 8 || len > n) return 0;
    if(len == 8 && pg_get_i32(b + 4) == PG_SSL_REQUEST_CODE) return 0;
    proto = pg_get_i32(b + 4);
    if((proto >> 16) != 3) return 0;          /* só major 3 */
    o->proto = proto;
    i = 8;
    while(i < len){
        const char *key, *val;
        int klen, vlen;
        if(b[i] == 0){ i++; break; }          /* fim dos pares */
        key = (const char *)(b + i);
        klen = (int)strlen(key);
        i += klen + 1;
        if(i >= len) return 0;
        val = (const char *)(b + i);
        vlen = (int)strlen(val);
        i += vlen + 1;
        if(strcmp(key, "user") == 0){
            if(vlen >= (int)sizeof o->user) return 0;
            memcpy(o->user, val, (size_t)vlen + 1);
        }else if(strcmp(key, "database") == 0){
            if(vlen >= (int)sizeof o->database) return 0;
            memcpy(o->database, val, (size_t)vlen + 1);
        }
    }
    if(!o->user[0]) return 0;                 /* user obrigatório */
    if(!o->database[0]){
        /* default = user */
        memcpy(o->database, o->user, sizeof o->database);
    }
    o->ok = 1;
    return 1;
}

/* Handshake backend trust após Startup válido (sem TCP). */
static void pg_put_startup_ok(PgBuf *w, int32_t pid, int32_t key){
    pg_put_auth_ok(w);
    pg_put_param_status(w, "server_version", "Tiffany-pgwire/0.1");
    pg_put_param_status(w, "client_encoding", "UTF8");
    pg_put_param_status(w, "server_encoding", "UTF8");
    pg_put_backend_key(w, pid, key);
    pg_put_ready(w, PG_TX_IDLE);
}

/* Lê uma mensagem tipada: devolve tipo, aponta payload (após len), payload_n.
 * Consome a partir de *ioff. */
static int pg_read_typed(const uint8_t *b, int n, int *ioff,
                         char *tipo, const uint8_t **payload, int *payload_n){
    int off, len;
    if(!ioff || *ioff + 5 > n) return 0;
    off = *ioff;
    *tipo = (char)b[off];
    len = pg_get_i32(b + off + 1);
    if(len < 4 || off + 1 + len > n) return 0;
    *payload = b + off + 5;
    *payload_n = len - 4;
    *ioff = off + 1 + len;
    return 1;
}

#endif
