/* pgmsg.h — FEBE: constantes e inteiros big-endian (referência Postgres Protocol).
 *
 * NÃO é o servidor Postgres. Só o mapa de bits do Frontend/Backend Protocol v3,
 * lido da documentação oficial (Message Formats). Realização Tiffany.
 *
 * Frame habitual: Byte1(tipo) + Int32(len incluindo o próprio len) + payload.
 * Excepção: StartupMessage e SSLRequest — sem Byte1 de tipo.
 */
#ifndef PGMSG_H
#define PGMSG_H

#include <stdint.h>
#include <string.h>

/* type-bytes (ASCII) — backend (B) e frontend (F) usados no Trio PG1+ */
#define PG_MSG_AUTH          'R'   /* B Authentication* */
#define PG_MSG_BACKEND_KEY   'K'   /* B BackendKeyData */
#define PG_MSG_ERROR         'E'   /* B ErrorResponse */
#define PG_MSG_PARAM_STATUS  'S'   /* B ParameterStatus */
#define PG_MSG_READY         'Z'   /* B ReadyForQuery */
#define PG_MSG_QUERY         'Q'   /* F Query (Simple) */
#define PG_MSG_TERMINATE     'X'   /* F Terminate */
#define PG_MSG_ROW_DESC      'T'   /* B RowDescription */
#define PG_MSG_DATA_ROW      'D'   /* B DataRow */
#define PG_MSG_CMD_COMPLETE  'C'   /* B CommandComplete */

/* Extended Query (Trio PG4) */
#define PG_MSG_PARSE         'P'   /* F Parse */
#define PG_MSG_BIND          'B'   /* F Bind */
#define PG_MSG_EXECUTE       'E'   /* F Execute */
#define PG_MSG_DESCRIBE      'D'   /* F Describe */
#define PG_MSG_CLOSE         'C'   /* F Close  — colide com CmdComplete no mesmo byte;
                                    * distingue-se pelo sentido F vs B no fluxo */
#define PG_MSG_SYNC          'S'   /* F Sync — colide com ParamStatus; sentido F vs B */
#define PG_MSG_FLUSH         'H'   /* F Flush */
#define PG_MSG_PARSE_COMPLETE '1'  /* B ParseComplete */
#define PG_MSG_BIND_COMPLETE  '2'  /* B BindComplete */
#define PG_MSG_CLOSE_COMPLETE '3'  /* B CloseComplete */
#define PG_MSG_NO_DATA        'n'  /* B NoData */
#define PG_MSG_PARAM_DESC     't'  /* B ParameterDescription */

/* OIDs mínimos (pg_type) — texto FEBE; int4 para colunas numéricas do metal */
/* Os OIDs de tipo. São os NÚMEROS DO POSTGRES, e não uma numeração nossa: um
 * cliente que receba 23 espera int4, e é essa a única razão de eles existirem.
 * A lista é curta de propósito — só o que este servidor sabe ANUNCIAR. */
#define PG_OID_BOOL          16
#define PG_OID_INT8          20
#define PG_OID_INT2          21
#define PG_OID_INT4          23
#define PG_OID_TEXT          25
#define PG_OID_OID           26
#define PG_OID_FLOAT8       701
#define PG_OID_VARCHAR     1043
#define PG_OID_NUMERIC     1700
/* O `vector` do pgvector NÃO tem OID fixo no Postgres — é uma extensão, e o
 * número sai do catálogo da instância. Os clientes descobrem-no perguntando
 * `SELECT oid FROM pg_type WHERE typname = 'vector'`, e é por isso que este
 * servidor tem de o servir. O número aqui é o desta casa, e vive na zona dos
 * OIDs de utilizador (>= 16384) precisamente para não colidir com os fixos. */
#define PG_OID_VECTOR     16385

/* ReadyForQuery — estado de transacção */
#define PG_TX_IDLE           'I'
#define PG_TX_BLOCK          'T'
#define PG_TX_ERROR          'E'

/* Protocolo 3.0 = (3<<16)|0 ; docs recentes também citam 3.2 */
#define PG_PROTO_3_0         196608
#define PG_PROTO_3_2         196610

/* SSLRequest: len=8, código 80877103 */
#define PG_SSL_REQUEST_CODE  80877103

/* AuthenticationOk: método 0 */
#define PG_AUTH_OK           0

/* ErrorResponse field codes (subset §54.8) */
#define PG_ERR_SEVERITY      'S'
#define PG_ERR_SEVERITY_V    'V'
#define PG_ERR_SQLSTATE      'C'
#define PG_ERR_MESSAGE       'M'

/* ── big-endian Int16 / Int32 (rede) ──────────────────────────────────────── */
static void pg_put_i16(uint8_t *p, int16_t v){
    uint16_t u = (uint16_t)v;
    p[0] = (uint8_t)(u >> 8);
    p[1] = (uint8_t)(u);
}
static void pg_put_i32(uint8_t *p, int32_t v){
    uint32_t u = (uint32_t)v;
    p[0] = (uint8_t)(u >> 24);
    p[1] = (uint8_t)(u >> 16);
    p[2] = (uint8_t)(u >> 8);
    p[3] = (uint8_t)(u);
}
static int16_t pg_get_i16(const uint8_t *p){
    return (int16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}
static int32_t pg_get_i32(const uint8_t *p){
    return (int32_t)(((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                     ((uint32_t)p[2] << 8)  | (uint32_t)p[3]);
}

#endif
