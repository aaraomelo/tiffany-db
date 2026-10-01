#ifndef TIFFANY_PARSE_FICHEIRO_H
#define TIFFANY_PARSE_FICHEIRO_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Parser de ficheiros da casa → nodo U autossimilar.
 * JSON é canónico. claim/erg/sql/tex/wasm são emissões (MOVE de formato).
 * Não é Parte; não homogeneíza Alonzo com U. */

typedef struct {
    char kind[24];
    char id[64];
    int sentido;          /* 0 completa; -1 emite; +1 absorve */
    char formato[16];
    char estatuto[24];
    char evidencia[256];
    char proibicao[256];
    char fonte[256];
    char faz[64];
    char move[64];
    long p, q, r;
    int law;
    char object[64];
    char step[128];
    char back[128];
    char measure[128];
    char invariant[128];
    char mutate[128];
    char classify[64];
} SchemaNodo;

static void schema_zera(SchemaNodo *n){
    memset(n, 0, sizeof *n);
    n->sentido = 0;
    n->law = -1;
    snprintf(n->formato, sizeof n->formato, "json");
    snprintf(n->estatuto, sizeof n->estatuto, "nao localizada");
}

static int schema_sufixo(const char *path, const char *ext){
    size_t np, ne;
    if(!path || !ext) return 0;
    np = strlen(path); ne = strlen(ext);
    if(np < ne) return 0;
    return strcmp(path + np - ne, ext) == 0;
}

static int schema_detecta(const char *path, const unsigned char *buf, long n, char *fmt, size_t cap){
    if(!fmt || cap < 8) return 0;
    if(n >= 4 && buf[0] == 0 && buf[1] == 'a' && buf[2] == 's' && buf[3] == 'm'){
        snprintf(fmt, cap, "wasm"); return 1;
    }
    if(n >= 8 && memcmp(buf, "implante", 8) == 0){
        snprintf(fmt, cap, "implante"); return 1;
    }
    {
        const char *p = (const char *)buf;
        while(*p && isspace((unsigned char)*p)) p++;
        if(!strncmp(p, "claim", 5) && (p[5] == 0 || isspace((unsigned char)p[5]))){
            snprintf(fmt, cap, "claim"); return 1;
        }
        if(*p == '{'){
            if(n > 20 && (strstr((const char *)buf, "\"linguagens\"") || strstr((const char *)buf, "\"corpos\""))){
                snprintf(fmt, cap, "manifesto"); return 1;
            }
            snprintf(fmt, cap, "json"); return 1;
        }
    }
    if(schema_sufixo(path, ".claim")) { snprintf(fmt, cap, "claim"); return 1; }
    if(schema_sufixo(path, ".erg")) { snprintf(fmt, cap, "erg"); return 1; }
    if(schema_sufixo(path, ".fita") || schema_sufixo(path, ".fita.bin")){
        snprintf(fmt, cap, "fita"); return 1;
    }
    if(schema_sufixo(path, ".wasm")) { snprintf(fmt, cap, "wasm"); return 1; }
    if(schema_sufixo(path, ".tex")) { snprintf(fmt, cap, "tex"); return 1; }
    if(schema_sufixo(path, ".json")) { snprintf(fmt, cap, "json"); return 1; }
    if(n > 8 && (strstr((const char *)buf, "\nLOAD ") || strstr((const char *)buf, "; wasm"))){
        snprintf(fmt, cap, "erg"); return 1;
    }
    snprintf(fmt, cap, "json");
    return 0;
}

static void schema_copia(char *dst, size_t cap, const char *src){
    size_t i = 0;
    if(!dst || cap < 1) return;
    if(!src){ dst[0] = 0; return; }
    while(src[i] && i + 1 < cap){ dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static void schema_json_esc(FILE *out, const char *s){
    if(!s){ fputs("\"\"", out); return; }
    fputc('"', out);
    for(; *s; s++){
        unsigned char c = (unsigned char)*s;
        if(c == '"' || c == '\\'){ fputc('\\', out); fputc(c, out); }
        else if(c == '\n') fputs("\\n", out);
        else if(c == '\r') fputs("\\r", out);
        else if(c == '\t') fputs("\\t", out);
        else if(c < 32) fprintf(out, "\\u%04x", c);
        else fputc(c, out);
    }
    fputc('"', out);
}

static int schema_parse_claim(const char *src, SchemaNodo *out){
    char linha[512];
    const char *p;
    int viu_claim = 0, viu_end = 0;
    if(!src || !out) return 0;
    schema_zera(out);
    snprintf(out->kind, sizeof out->kind, "claim");
    snprintf(out->formato, sizeof out->formato, "claim");
    snprintf(out->estatuto, sizeof out->estatuto, "realizado");
    p = src;
    while(*p){
        int i = 0;
        while(*p && *p != '\n' && i < (int)sizeof linha - 1) linha[i++] = *p++;
        linha[i] = 0;
        if(*p == '\n') p++;
        {
            char *a = linha;
            size_t n;
            while(*a && isspace((unsigned char)*a)) a++;
            if(a != linha) memmove(linha, a, strlen(a) + 1);
            n = strlen(linha);
            while(n > 0 && isspace((unsigned char)linha[n - 1])) linha[--n] = 0;
        }
        if(linha[0] == 0 || linha[0] == '#') continue;
        if(!strncmp(linha, "claim", 5) && (linha[5] == 0 || isspace((unsigned char)linha[5]))){
            const char *v = linha + 5;
            while(*v && isspace((unsigned char)*v)) v++;
            schema_copia(out->id, sizeof out->id, v);
            viu_claim = 1; continue;
        }
        if(!strncmp(linha, "end", 3) && (linha[3] == 0 || isspace((unsigned char)linha[3]))){
            viu_end = 1; break;
        }
        if(!strncmp(linha, "law", 3) && isspace((unsigned char)linha[3])){
            out->law = (int)strtol(linha + 3, 0, 10); continue;
        }
        if(!strncmp(linha, "object", 6) && isspace((unsigned char)linha[6])){
            const char *v = linha + 6; while(*v && isspace((unsigned char)*v)) v++;
            schema_copia(out->object, sizeof out->object, v); continue;
        }
        if(!strncmp(linha, "step", 4) && isspace((unsigned char)linha[4])){
            const char *v = linha + 4; while(*v && isspace((unsigned char)*v)) v++;
            schema_copia(out->step, sizeof out->step, v); continue;
        }
        if(!strncmp(linha, "back", 4) && isspace((unsigned char)linha[4])){
            const char *v = linha + 4; while(*v && isspace((unsigned char)*v)) v++;
            schema_copia(out->back, sizeof out->back, v); continue;
        }
        if(!strncmp(linha, "measure", 7) && isspace((unsigned char)linha[7])){
            const char *v = linha + 7; while(*v && isspace((unsigned char)*v)) v++;
            schema_copia(out->measure, sizeof out->measure, v); continue;
        }
        if(!strncmp(linha, "invariant", 9) && isspace((unsigned char)linha[9])){
            const char *v = linha + 9; while(*v && isspace((unsigned char)*v)) v++;
            schema_copia(out->invariant, sizeof out->invariant, v); continue;
        }
        if(!strncmp(linha, "mutate", 6) && isspace((unsigned char)linha[6])){
            const char *v = linha + 6; while(*v && isspace((unsigned char)*v)) v++;
            schema_copia(out->mutate, sizeof out->mutate, v); continue;
        }
        if(!strncmp(linha, "classify", 8) && isspace((unsigned char)linha[8])){
            const char *v = linha + 8; while(*v && isspace((unsigned char)*v)) v++;
            schema_copia(out->classify, sizeof out->classify, v); continue;
        }
        if(!strncmp(linha, "residual", 8)) return 0;
    }
    if(!viu_claim || !viu_end || !out->id[0] || out->law < 0) return 0;
    if(!out->step[0] || !out->back[0] || !out->measure[0]) return 0;
    return 1;
}

static void schema_preenche_u(SchemaNodo *n){
    schema_zera(n);
    snprintf(n->kind, sizeof n->kind, "U");
    snprintf(n->id, sizeof n->id, "U");
    n->sentido = 0;
    snprintf(n->formato, sizeof n->formato, "json");
    snprintf(n->estatuto, sizeof n->estatuto, "gramatica");
    snprintf(n->evidencia, sizeof n->evidencia, "corpo_universal.tex univ:def:U");
    snprintf(n->proibicao, sizeof n->proibicao,
             "U != catalogo != INGEST; nao e Parte; Star(U)=D");
}

static int schema_parse_nodo(const char *path, const char *buf, long n, SchemaNodo *out){
    char fmt[16];
    if(!out) return 0;
    schema_zera(out);
    schema_detecta(path, (const unsigned char *)(buf ? buf : ""), n, fmt, sizeof fmt);
    schema_copia(out->formato, sizeof out->formato, fmt);
    schema_copia(out->fonte, sizeof out->fonte, path ? path : "");
    if(!strcmp(fmt, "claim")){
        if(!schema_parse_claim(buf, out)) return 0;
        schema_copia(out->fonte, sizeof out->fonte, path ? path : "");
        return 1;
    }
    if(!strcmp(fmt, "wasm")){
        snprintf(out->kind, sizeof out->kind, "ficheiro");
        snprintf(out->id, sizeof out->id, "wasm");
        snprintf(out->estatuto, sizeof out->estatuto, "realizado");
        snprintf(out->evidencia, sizeof out->evidencia, "magic \\0asm");
        return 1;
    }
    if(!strcmp(fmt, "erg")){
        snprintf(out->kind, sizeof out->kind, "ficheiro");
        snprintf(out->id, sizeof out->id, "erg");
        snprintf(out->estatuto, sizeof out->estatuto, "realizado");
        snprintf(out->evidencia, sizeof out->evidencia, "ERG-64");
        return 1;
    }
    if(!strcmp(fmt, "fita")){
        snprintf(out->kind, sizeof out->kind, "ficheiro");
        snprintf(out->id, sizeof out->id, "fita");
        snprintf(out->estatuto, sizeof out->estatuto, "realizado");
        return 1;
    }
    if(!strcmp(fmt, "tex")){
        snprintf(out->kind, sizeof out->kind, "ficha");
        snprintf(out->id, sizeof out->id, "tex");
        snprintf(out->estatuto, sizeof out->estatuto, "nao localizada");
        snprintf(out->evidencia, sizeof out->evidencia, "fichaingestao no catalogo; nao se relê I0");
        return 1;
    }
    if(!strcmp(fmt, "implante")){
        snprintf(out->kind, sizeof out->kind, "ficheiro");
        snprintf(out->id, sizeof out->id, "implante");
        snprintf(out->estatuto, sizeof out->estatuto, "realizado");
        return 1;
    }
    if(!strcmp(fmt, "manifesto")){
        snprintf(out->kind, sizeof out->kind, "U");
        snprintf(out->id, sizeof out->id, "U");
        snprintf(out->estatuto, sizeof out->estatuto, "gramatica");
        snprintf(out->evidencia, sizeof out->evidencia, "manifesto.linguagens realizam U; nao sao Partes");
        snprintf(out->proibicao, sizeof out->proibicao, "lingua != palco != corpo");
        return 1;
    }
    if(!strcmp(fmt, "json") && buf && strstr(buf, "\"kind\"")){
        const char *k = strstr(buf, "\"kind\"");
        if(k){
            char kind[24] = "";
            sscanf(k, "\"kind\": \"%23[^\"]\"", kind);
            if(kind[0]) schema_copia(out->kind, sizeof out->kind, kind);
        }
        {
            const char *i = strstr(buf, "\"id\"");
            if(i) sscanf(i, "\"id\": \"%63[^\"]\"", out->id);
        }
        if(!out->kind[0]) snprintf(out->kind, sizeof out->kind, "ficheiro");
        if(!out->id[0]) snprintf(out->id, sizeof out->id, "json");
        snprintf(out->estatuto, sizeof out->estatuto, "realizado");
        return 1;
    }
    snprintf(out->kind, sizeof out->kind, "ficheiro");
    snprintf(out->id, sizeof out->id, "ficheiro");
    return 1;
}

static void schema_emit_json(const SchemaNodo *n, FILE *out){
    fputc('{', out);
    fputs("\"kind\":", out); schema_json_esc(out, n->kind);
    fputs(",\"id\":", out); schema_json_esc(out, n->id);
    fprintf(out, ",\"sentido\":%d", n->sentido);
    fputs(",\"formato\":", out); schema_json_esc(out, n->formato);
    if(n->estatuto[0]){ fputs(",\"estatuto\":", out); schema_json_esc(out, n->estatuto); }
    if(n->evidencia[0]){ fputs(",\"evidencia\":", out); schema_json_esc(out, n->evidencia); }
    if(n->proibicao[0]){ fputs(",\"proibicao\":", out); schema_json_esc(out, n->proibicao); }
    if(n->fonte[0]){ fputs(",\"fonte\":", out); schema_json_esc(out, n->fonte); }
    if(n->faz[0]){ fputs(",\"faz\":", out); schema_json_esc(out, n->faz); }
    if(n->move[0]){ fputs(",\"move\":", out); schema_json_esc(out, n->move); }
    if(n->p || n->q || n->r) fprintf(out, ",\"p\":%ld,\"q\":%ld,\"r\":%ld", n->p, n->q, n->r);
    if(n->law >= 0) fprintf(out, ",\"law\":%d", n->law);
    if(n->object[0]){ fputs(",\"object\":", out); schema_json_esc(out, n->object); }
    if(n->step[0]){ fputs(",\"step\":", out); schema_json_esc(out, n->step); }
    if(n->back[0]){ fputs(",\"back\":", out); schema_json_esc(out, n->back); }
    if(n->measure[0]){ fputs(",\"measure\":", out); schema_json_esc(out, n->measure); }
    if(n->invariant[0]){ fputs(",\"invariant\":", out); schema_json_esc(out, n->invariant); }
    if(n->mutate[0]){ fputs(",\"mutate\":", out); schema_json_esc(out, n->mutate); }
    if(n->classify[0]){ fputs(",\"classify\":", out); schema_json_esc(out, n->classify); }
    if(n->sentido == 0){
        fputs(",\"faces\":{\"menos\":{", out);
        fputs("\"kind\":\"metade\",\"id\":", out); schema_json_esc(out, n->id);
        fprintf(out, ",\"sentido\":-1,\"formato\":\"json\"},\"mais\":{");
        fputs("\"kind\":\"metade\",\"id\":", out); schema_json_esc(out, n->id);
        fprintf(out, ",\"sentido\":1,\"formato\":\"json\"}}");
    }
    fputc('}', out);
}

static int schema_emit(const SchemaNodo *n, const char *formato, FILE *out){
    const char *as = (formato && *formato) ? formato : "json";
    if(!n || !out) return 0;
    if(!strcmp(as, "json")){
        schema_emit_json(n, out);
        fputc('\n', out);
        return 1;
    }
    if(!strcmp(as, "claim")){
        if(strcmp(n->kind, "claim") != 0) return 0;
        fprintf(out,
            "claim %s\nlaw %d\nobject %s\nstep %s\nback %s\nmeasure %s\n"
            "invariant %s\nmutate %s\nclassify %s\nend\n",
            n->id, n->law, n->object, n->step, n->back, n->measure,
            n->invariant, n->mutate, n->classify);
        return 1;
    }
    if(!strcmp(as, "sql")){
        fprintf(out, "INSERT TEXTO 'schema/%s|%s|%d|%s'\n",
                n->id, n->kind, n->sentido, n->formato);
        return 1;
    }
    if(!strcmp(as, "tex")){
        fprintf(out, "\\ficharow{%s}{%s sentido %d formato %s}\n",
                n->id, n->kind, n->sentido, n->formato);
        return 1;
    }
    /* formato pedido que o nodo não emite: metade ausente, não N/A */
    return 0;
}

static int schema_emit_lingua_json(FILE *out, const char *nome, const char *faz,
                                  long p, long q, long r, const char *move){
    SchemaNodo n;
    schema_zera(&n);
    snprintf(n.kind, sizeof n.kind, "lingua");
    schema_copia(n.id, sizeof n.id, nome);
    n.sentido = 0;
    snprintf(n.formato, sizeof n.formato, "json");
    snprintf(n.estatuto, sizeof n.estatuto, "realizado");
    schema_copia(n.faz, sizeof n.faz, faz);
    schema_copia(n.move, sizeof n.move, move);
    n.p = p; n.q = q; n.r = r;
    snprintf(n.proibicao, sizeof n.proibicao, "lingua != Parte; js != node");
    schema_emit_json(&n, out);
    return 1;
}

static int schema_emit_manifesto(const char *buf, long st, int sentido, FILE *out){
    int nling = 0;
    fputs("{\"kind\":\"U\",\"id\":\"U\",\"sentido\":", out);
    fprintf(out, "%d", sentido);
    fputs(",\"formato\":\"json\",\"star\":\"D\",\"estatuto\":\"gramatica\"", out);
    fputs(",\"proibicao\":\"lingua != palco != corpo; Star(U)=D\"", out);
    fputs(",\"filhos\":[", out);
    for(char *q = (char *)buf; (q = strstr(q, "\"nome\"")); q++){
        char nome[64] = "", faz[64] = "", move[64] = "";
        long pp = 0, qq = 0, rr = 0;
        if(sscanf(q, "\"nome\": \"%63[^\"]\"", nome) != 1) continue;
        if(!strcmp(nome, "Algebra") || !strcmp(nome, "Fractal") || !strcmp(nome, "Alonzo"))
            continue;
        {
            char *bl = q, *fim = q + 800;
            if(fim > buf + st) fim = (char *)buf + st;
            for(char *t = bl; t < fim; t++){
                if(!faz[0] && sscanf(t, "\"faz\": \"%63[^\"]\"", faz) == 1) continue;
                if(!pp && sscanf(t, "\"p\": %ld", &pp) == 1) continue;
                if(!qq && sscanf(t, "\"q\": %ld", &qq) == 1) continue;
                if(!rr && sscanf(t, "\"r\": %ld", &rr) == 1) continue;
                if(!move[0] && sscanf(t, "\"move\": \"%63[^\"]\"", move) == 1) continue;
            }
        }
        if(!nome[0] || !faz[0]) continue;
        if(nling) fputc(',', out);
        schema_emit_lingua_json(out, nome, faz, pp, qq, rr, move);
        nling++;
        if(nling >= 19) break;
    }
    fputs("]}\n", out);
    return nling > 0;
}

static char *schema_le_ficheiro(const char *path, long *out_n){
    FILE *f;
    long st;
    char *buf;
    if(out_n) *out_n = 0;
    f = fopen(path, "rb");
    if(!f) return NULL;
    if(fseek(f, 0, SEEK_END) != 0){ fclose(f); return NULL; }
    st = ftell(f);
    if(st < 0 || st > (4 << 20)){ fclose(f); return NULL; }
    rewind(f);
    buf = (char *)malloc((size_t)st + 1);
    if(!buf){ fclose(f); return NULL; }
    if((long)fread(buf, 1, (size_t)st, f) != st){ free(buf); fclose(f); return NULL; }
    buf[st] = 0;
    fclose(f);
    if(out_n) *out_n = st;
    return buf;
}

/* Lê o ficheiro, detecta o formato da casa, emite no formato pedido (json canónico). */
static int schema_parse_ficheiro(const char *path, const char *as, int sentido, FILE *out){
    long n = 0;
    char fmt[16];
    SchemaNodo nod;
    char *buf = schema_le_ficheiro(path, &n);
    int ok;
    if(!buf) return 0;
    schema_detecta(path, (const unsigned char *)buf, n, fmt, sizeof fmt);
    if(!as || !as[0] || !strcmp(as, "json")) as = "json";
    if(!strcmp(fmt, "manifesto") && !strcmp(as, "json")){
        ok = schema_emit_manifesto(buf, n, sentido, out);
        free(buf);
        return ok;
    }
    if(!schema_parse_nodo(path, buf, n, &nod)){ free(buf); return 0; }
    nod.sentido = sentido;
    if(sentido != 0) snprintf(nod.kind, sizeof nod.kind, "metade");
    ok = schema_emit(&nod, as, out);
    free(buf);
    return ok;
}

#endif
