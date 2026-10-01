/* stratum.h — O POOL COMO BACKEND. O banco lê o job de um slot; não sabe que houve TCP.
 *
 * Mesma fronteira do canal: acima de S_POOL, LOAD vai buscar um campo do job corrente e STORE
 * emite a share. O protocolo fica atrás destas funções, e é por isso que ele é indistinguível —
 * trocar stratum por outra coisa é trocar aqui dentro, e nem uma linha de SQL muda.
 *
 * O JSON do stratum é de linha e de forma conhecida; não é preciso analisador geral, e um
 * analisador geral seria peça a mais. Lê-se o campo pela posição, que é o que o protocolo define.
 */
#ifndef STRATUM_H
#define STRATUM_H
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include "banda.h"
#include "caminho.h"

typedef struct {
    int fd;
    char extranonce1[32]; int en1_len;
    int  en2_size;
    char job_id[64];
    unsigned char prevhash[32], merkle_raiz[32];
    unsigned char cb1[512], cb2[512]; int n1, n2;      /* o coinbase, em duas metades */
    unsigned char ramos[16*32]; int n_ramos;           /* a merkle branch */
    unsigned versao, nbits, ntime;
    int tem_job;
    char buf[8192]; size_t nbuf;
    int id;
} Pool;

static int hexval(int c){
    return (c>='0'&&c<='9')?c-'0':(c>='a'&&c<='f')?c-'a'+10:(c>='A'&&c<='F')?c-'A'+10:-1;
}
static int st_hex(const char *s, size_t n, unsigned char *out){
    if(n % 2) return 0;
    for(size_t i = 0; i < n; i += 2){
        int a = hexval(s[i]), b = hexval(s[i+1]);
        if(a < 0 || b < 0) return 0;
        out[i/2] = (unsigned char)(a*16 + b);
    }
    return 1;
}
/* A descida vem do caminho.h — uma so, e serve JSON, YAML e Markdown. */
/* o k-ésimo parâmetro do mining.notify, pelo caminho */
static const char *st_param(const char *linha, int k, size_t *len){
    const char *p = strstr(linha, "\"params\"");
    if(!p) return NULL;
    while(*p && *p != '[') p++;
    if(!*p) return NULL;
    int cam[1] = { k };
    return js_caminho(p, cam, 1, len);
}
static int st_envia(Pool *P, const char *s){
    size_t n = strlen(s);
    return write(P->fd, s, n) == (ssize_t)n;
}
static int st_liga(Pool *P, const char *host, int porta, const char *user){
    memset(P, 0, sizeof *P);
    P->id = 1;
    struct addrinfo dica, *res = NULL;
    char sp[16]; snprintf(sp, sizeof sp, "%d", porta);
    memset(&dica, 0, sizeof dica);
    dica.ai_family = AF_INET; dica.ai_socktype = SOCK_STREAM;
    if(getaddrinfo(host, sp, &dica, &res) || !res) return 0;
    P->fd = socket(res->ai_family, res->ai_socktype, 0);
    if(P->fd < 0 || connect(P->fd, res->ai_addr, res->ai_addrlen) < 0){ freeaddrinfo(res); return 0; }
    freeaddrinfo(res);
    int um = 1; setsockopt(P->fd, IPPROTO_TCP, TCP_NODELAY, &um, sizeof um);
    /* TEMPO-LIMITE. Sem ele o st_linha fica preso no read a espera de mais uma linha e nunca
     * devolve — o worker ligava, e ficava ali para sempre. E o RELOGIO do laco: com 200 ms ele
     * le o que chegou, volta a martelar, e volta a olhar. */
    struct timeval to = { 0, 200000 };
    setsockopt(P->fd, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof to);
    char m[512];
    snprintf(m, sizeof m, "{\"id\":%d,\"method\":\"mining.subscribe\",\"params\":[]}\n", P->id++);
    if(!st_envia(P, m)) return 0;
    snprintf(m, sizeof m,
             "{\"id\":%d,\"method\":\"mining.authorize\",\"params\":[\"%s\",\"x\"]}\n", P->id++, user);
    return st_envia(P, m);
}
/* uma linha do pool, se houver. Não bloqueia mais do que o socket bloquear. */
static int st_linha(Pool *P, char *saida, size_t max){
    char *nl;
    while(!(nl = memchr(P->buf, '\n', P->nbuf))){
        if(P->nbuf + 1 >= sizeof P->buf){ P->nbuf = 0; return 0; }
        ssize_t g = read(P->fd, P->buf + P->nbuf, sizeof P->buf - P->nbuf - 1);
        if(g <= 0) return 0;
        P->nbuf += (size_t)g;
    }
    size_t n = (size_t)(nl - P->buf);
    if(n >= max) n = max - 1;
    memcpy(saida, P->buf, n); saida[n] = 0;
    memmove(P->buf, nl + 1, P->nbuf - n - 1);
    P->nbuf -= n + 1;
    return 1;
}
/* Trata o que o pool disser. mining.notify enche o job; subscribe enche o extranonce. */
/* O st_trata SOBRE UMA FONTE. Deixa de exigir a linha contigua: pede simbolos, e quem os da pode
 * ser memoria ou o banco. O strstr vira f_acha, o st_param vira descida, e o unico array que
 * sobra e o do CAMPO que se copia — nao o do objeto. */
static void st_trata_fonte(Pool *P, const Fonte *f){
    long m = f_acha(f, "\"method\":\"mining.notify\"");
    if(m < 0) m = f_acha(f, "\"method\": \"mining.notify\"");
    long pr = f_acha(f, "\"params\"");
    if(m >= 0 && pr >= 0){
        while(SIM(f, pr) >= 0 && SIM(f, pr) != '[') pr++;
        char v[600];
        long a;
        if((a = f_desce(f, pr, 0)) >= 0) f_str(f, a, P->job_id, sizeof P->job_id);
        if((a = f_desce(f, pr, 1)) >= 0 && f_str(f, a, v, sizeof v) == 64) st_hex(v, 64, P->prevhash);
        if((a = f_desce(f, pr, 2)) >= 0){ long n = f_str(f, a, v, sizeof v);
            if(n > 0 && n/2 <= 512){ st_hex(v, (size_t)n, P->cb1); P->n1 = (int)(n/2); } }
        if((a = f_desce(f, pr, 3)) >= 0){ long n = f_str(f, a, v, sizeof v);
            if(n > 0 && n/2 <= 512){ st_hex(v, (size_t)n, P->cb2); P->n2 = (int)(n/2); } }
        if((a = f_desce(f, pr, 4)) >= 0){
            P->n_ramos = 0;
            for(int k = 0; k < 16; k++){
                long r = f_desce(f, a, k);
                if(r < 0 || f_str(f, r, v, sizeof v) != 64) break;
                st_hex(v, 64, P->ramos + 32*k); P->n_ramos++;
            }
        }
        for(int k = 5; k <= 7; k++){
            if((a = f_desce(f, pr, k)) >= 0 && f_str(f, a, v, sizeof v) == 8){
                unsigned char b4[4]; st_hex(v, 8, b4);
                unsigned x = ((unsigned)b4[0]<<24)|((unsigned)b4[1]<<16)|((unsigned)b4[2]<<8)|b4[3];
                if(k == 5) P->versao = x; else if(k == 6) P->nbits = x; else P->ntime = x;
            }
        }
        P->tem_job = 1;
        return;
    }
    /* a resposta do subscribe: o extranonce1 e o tamanho do extranonce2 */
    if(!P->en1_len && f_acha(f, "\"result\"") >= 0){
        long p = f_acha(f, "\"result\"");
        long ult = -1, q = p;
        for(;;){ int c = SIM(f, q); if(c < 0) break; if(c == '"') ult = q; q++; }
        (void)ult;
        /* a ultima string antes do numero: percorre-se do fim para tras */
        long fim = q, ini = -1, cnt = 0;
        for(long i = fim - 1; i > p; i--){
            if(SIM(f, i) == '"'){ cnt++; if(cnt == 2){ ini = i; break; } }
        }
        if(ini > 0){ char v[64]; long n = f_str(f, ini, v, sizeof v);
                     if(n > 0 && (size_t)n < sizeof P->extranonce1){
                         memcpy(P->extranonce1, v, (size_t)n);
                         P->extranonce1[n] = 0; P->en1_len = (int)n; } }
        long r = ini > 0 ? ini : p; int val = 0;
        for(;;){ int c = SIM(f, r); if(c < 0) break;
                 if(c >= '0' && c <= '9'){ val = val*10 + (c - '0'); if(val > 9999) break; }
                 else if(val) break;
                 r++; }
        P->en2_size = (val > 0 && val <= 32) ? val : 4;
    }
}
static void st_trata(Pool *P, const char *l){
    /* O METODO, e nao a string solta. A resposta do subscribe CONTEM "mining.notify" (o ckpool
     * devolve [["mining.set_difficulty",..],["mining.notify",..]]), e com strstr eu analisava-a
     * como se fosse um job: tem_job=1 com id vazio, e o worker a martelar um cabecalho que nunca
     * foi montado. Procura-se o METODO. */
    if(strstr(l, "\"method\":\"mining.notify\"") || strstr(l, "\"method\": \"mining.notify\"")){
        size_t n;
        const char *v;
        if((v = st_param(l, 0, &n)) && n < sizeof P->job_id){ memcpy(P->job_id, v, n); P->job_id[n]=0; }
        if((v = st_param(l, 1, &n)) && n == 64) st_hex(v, n, P->prevhash);
        if((v = st_param(l, 2, &n)) && n/2 <= 512){ st_hex(v, n, P->cb1); P->n1 = (int)(n/2); }
        if((v = st_param(l, 3, &n)) && n/2 <= 512){ st_hex(v, n, P->cb2); P->n2 = (int)(n/2); }
        /* a merkle branch e um ARRAY: desce-se ao 4.o parametro e le-se cada ramo */
        { const char *pr = strstr(l, "\"params\"");
          if(pr){ while(*pr && *pr != '[') pr++;
                  const char *arr = js_desce(pr, 4);
                  P->n_ramos = 0;
                  if(arr && *arr == '['){
                      for(int k = 0; k < 16; k++){
                          int c[1] = { k }; size_t m;
                          const char *r = js_caminho(arr, c, 1, &m);
                          if(!r || m != 64) break;
                          st_hex(r, m, P->ramos + 32*k);
                          P->n_ramos++;
                      }
                  } } }
        if((v = st_param(l, 5, &n)) && n == 8){ unsigned char b[4]; st_hex(v,n,b);
            P->versao = ((unsigned)b[0]<<24)|((unsigned)b[1]<<16)|((unsigned)b[2]<<8)|b[3]; }
        if((v = st_param(l, 6, &n)) && n == 8){ unsigned char b[4]; st_hex(v,n,b);
            P->nbits = ((unsigned)b[0]<<24)|((unsigned)b[1]<<16)|((unsigned)b[2]<<8)|b[3]; }
        if((v = st_param(l, 7, &n)) && n == 8){ unsigned char b[4]; st_hex(v,n,b);
            P->ntime = ((unsigned)b[0]<<24)|((unsigned)b[1]<<16)|((unsigned)b[2]<<8)|b[3]; }
        P->tem_job = 1;
    } else if(strstr(l, "\"result\"") && !P->en1_len){
        /* a resposta do subscribe traz o extranonce1 e o tamanho do extranonce2 */
        const char *p = strrchr(l, '"');
        if(p){ const char *ini = p; while(ini > l && *(ini-1) != '"') ini--;
               size_t n = (size_t)(p - ini);
               if(n && n < sizeof P->extranonce1){ memcpy(P->extranonce1, ini, n);
                                                   P->extranonce1[n]=0; P->en1_len=(int)n; }
               /* O TAMANHO DO EXTRANONCE2, que vem a seguir ao extranonce1 no result. Estava
                * declarado e NUNCA ATRIBUIDO — ficava 0, e o coinbase saia SEM o extranonce2, o
                * que da merkle errada e share invalida em job real. O genese nao apanha isto
                * porque la nao ha extranonce nenhum: passava por saudavel.
                *
                * E vem LIMITADO. E numero de fora, e numero de fora sem limite e o buraco por
                * onde um pool hostil mandava escrever slots sem fim. */
               { const char *q = p; while(*q && (*q < '0' || *q > '9')) q++;
                 int v = 0; while(*q >= '0' && *q <= '9'){ v = v*10 + (*q++ - '0'); if(v > 9999) break; }
                 P->en2_size = (v > 0 && v <= 32) ? v : 4; } }
    }
}
/* A MERKLE saiu daqui: nao e conta, e DOBRA, e quem dobra e a maquina (sql.c, OP_FOLD). */
/* A SHARE de volta: STORE no slot de share vira mining.submit. */
static int st_submete(Pool *P, const char *user, unsigned nonce){
    char m[512];
    snprintf(m, sizeof m,
        "{\"id\":%d,\"method\":\"mining.submit\",\"params\":"
        "[\"%s\",\"%s\",\"00000000\",\"%08x\",\"%08x\"]}\n",
        P->id++, user, P->job_id, P->ntime, nonce);
    return st_envia(P, m);
}
#endif
