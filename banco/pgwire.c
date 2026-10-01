/* pgwire.c — listener FEBE (Trio PG3). Sem Postgres instalado.
 *
 *   cc -O2 -std=c99 -w -Ilib -Ibanco -o /tmp/pgwire_srv banco/pgwire.c banco/sql.c -lm
 *   /tmp/pgwire_srv /tmp/reino          # tenta 5432; se ocupado, 55432
 *   /tmp/pgwire_srv /tmp/reino 55432
 *
 * sql continua interface_padrao; isto é entrada_apps=pgwire.
 */
#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "pgwire.h"
#include "sql_api.h"
#include "pgwire_sess.h"

#ifndef PGWIRE_RECV_CAP
#define PGWIRE_RECV_CAP 8192
#endif

/* ── I/O ─────────────────────────────────────────────────────────────────── */
int pgwire_send_all(int fd, const void *buf, int n){
    const uint8_t *p = (const uint8_t *)buf;
    int o = 0;
    while(o < n){
        ssize_t w = send(fd, p + o, (size_t)(n - o), 0);
        if(w <= 0) return -1;
        o += (int)w;
    }
    return 0;
}

int pgwire_recv_n(int fd, void *buf, int n){
    uint8_t *p = (uint8_t *)buf;
    int o = 0;
    while(o < n){
        ssize_t r = recv(fd, p + o, (size_t)(n - o), 0);
        if(r <= 0) return -1;
        o += (int)r;
    }
    return 0;
}

/* Tenta bind em 127.0.0.1:porto. Devolve listen-fd ou -1. */
static int pgwire_bind_porto(int porto){
    int fd, on = 1;
    struct sockaddr_in a;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if(fd < 0) return -1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons((uint16_t)porto);
    if(bind(fd, (struct sockaddr *)&a, sizeof a) < 0){
        close(fd); return -1;
    }
    if(listen(fd, 8) < 0){ close(fd); return -1; }
    return fd;
}

/* want<=0 → tenta 5432, senão 55432. *got recebe o porto efectivo. */
int pgwire_listen(int want, int *got){
    int fd;
    if(want > 0){
        fd = pgwire_bind_porto(want);
        if(fd >= 0 && got) *got = want;
        return fd;
    }
    fd = pgwire_bind_porto(5432);
    if(fd >= 0){ if(got) *got = 5432; return fd; }
    fd = pgwire_bind_porto(55432);
    if(fd >= 0){ if(got) *got = 55432; return fd; }
    return -1;
}

/* Uma sessão: SSL→N, Startup trust, Simple + Extended Query, Terminate. */
int pgwire_serve_conn(int cfd, int32_t pid, int32_t key){
    uint8_t head[8], body[PGWIRE_RECV_CAP];
    PgBuf out;
    PgStartup st;
    PgHead8 h;
    PgSess sess;
    int32_t len;

    pg_sess_limpa(&sess);

    if(pgwire_recv_n(cfd, head, 8) < 0) return -1;
    if(!pg_peek_head8(head, 8, &h)) return -1;

    if(h.e_ssl){
        uint8_t n = (uint8_t)'N';
        if(pgwire_send_all(cfd, &n, 1) < 0) return -1;
        if(pgwire_recv_n(cfd, head, 8) < 0) return -1;
        if(!pg_peek_head8(head, 8, &h) || h.e_ssl) return -1;
    }

    len = h.len;
    if(len < 8 || len > PGWIRE_RECV_CAP) return -1;
    memcpy(body, head, 8);
    if(len > 8 && pgwire_recv_n(cfd, body + 8, len - 8) < 0) return -1;
    if(!pg_parse_startup(body, len, &st) || !st.ok){
        pg_buf_limpa(&out);
        pg_put_error(&out, "28000", "startup invalido");
        pgwire_send_all(cfd, out.b, out.n);
        return -1;
    }

    pg_buf_limpa(&out);
    pg_put_startup_ok(&out, pid, key);
    if(out.erro || pgwire_send_all(cfd, out.b, out.n) < 0) return -1;

    for(;;){
        uint8_t hdr[5];
        char tipo;
        int32_t mlen, pay_n;
        int rc;

        if(pgwire_recv_n(cfd, hdr, 5) < 0) return -1;
        tipo = (char)hdr[0];
        mlen = pg_get_i32(hdr + 1);
        if(mlen < 4 || mlen - 4 > PGWIRE_RECV_CAP) return -1;
        pay_n = mlen - 4;
        if(pay_n > 0 && pgwire_recv_n(cfd, body, pay_n) < 0) return -1;
        if(pay_n >= 0 && pay_n < PGWIRE_RECV_CAP) body[pay_n] = 0;

        pg_buf_limpa(&out);
        rc = pg_sess_fe(&sess, tipo, body, pay_n, &out);
        if(rc == 0) return 0;
        if(rc < 0) return -1;
        if(out.n > 0 && (out.erro || pgwire_send_all(cfd, out.b, out.n) < 0))
            return -1;
    }
}

#ifndef PGWIRE_NO_MAIN
int main(int argc, char **argv){
    const char *base;
    int want = 0, got = 0, lfd, cfd;
    struct sockaddr_in peer;
    socklen_t plen = sizeof peer;

    if(argc < 2){
        fprintf(stderr, "uso: pgwire <base> [porto]\n"
                        "  porto omitido: tenta 5432; se ocupado, 55432\n");
        return 2;
    }
    base = argv[1];
    if(argc >= 3) want = atoi(argv[2]);

    signal(SIGPIPE, SIG_IGN);

    if(!sql_abrir(base)){
        fprintf(stderr, "pgwire: nao abriu base '%s'\n", base);
        return 2;
    }

    lfd = pgwire_listen(want, &got);
    if(lfd < 0){
        fprintf(stderr, "pgwire: bind falhou (want=%d): %s\n", want, strerror(errno));
        sql_fechar();
        return 2;
    }
    fprintf(stderr, "Tiffany-pgwire a escutar 127.0.0.1:%d  base=%s\n", got, base);
    fprintf(stderr, "entrada_apps=pgwire  (interface_padrao=sql)\n");

    for(;;){
        plen = sizeof peer;
        cfd = accept(lfd, (struct sockaddr *)&peer, &plen);
        if(cfd < 0){
            if(errno == EINTR) continue;
            break;
        }
        pgwire_serve_conn(cfd, (int32_t)getpid(), 0x54494646); /* 'TIFF' */
        close(cfd);
    }

    close(lfd);
    sql_fechar();
    return 0;
}
#endif
