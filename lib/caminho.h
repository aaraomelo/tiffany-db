/* caminho.h — A DESCIDA. O endereço de um valor é o seu CAMINHO, e o formato é só a roupa.
 *
 * Nasceu do analisador do stratum, que eu tinha escrito a contar símbolos e que partiu duas vezes
 * — no array da merkle branch e numa aspa escapada. Contar símbolos onde há ESTRUTURA é o erro; a
 * estrutura endereça-se descendo, um termo por nível, que é o que a cifra faz.
 *
 * E o que muda de formato para formato NÃO é a descida: é como cada um MARCA o nível.
 *
 *     JSON       o nível é o parêntese          [ { } ]
 *     YAML       o nível é a INDENTAÇÃO         os espaços à esquerda
 *     Markdown   o nível é a contagem de #      # ## ###
 *
 * A lei é a mesma nos três, e é a do tesseracto: M_k = M_{k-1}·A_1, o nível k carrega o k-1.
 * Trocar de formato é trocar quem lê a marca — não a descida.
 */
#ifndef CAMINHO_H
#define CAMINHO_H
#include <string.h>
#include <stdlib.h>
#include "cifra.h"

/* A TRADUÇÃO É DO HIPERCORPO, E NÃO DE VARREDURA DE STRING.
 *
 * Eu escrevi um analisador que contava aspas e ele partiu duas vezes: primeiro com o array da
 * merkle branch (que desloca as posições conforme o número de ramos), depois com uma ASPA
 * ESCAPADA — JSON válido, e devolvia zeros. Remendar terceira vez seria esperar a quarta.
 *
 * JSON é recursão auto-similar: um nível contém cópias de si, que é a lei do tesseracto
 * (`M_k = M_{k-1}A_1`, o nível k carrega o k-1). E a posição de um campo NÃO é uma contagem de
 * símbolos: é o seu CAMINHO — params, depois o 5.o — e caminho é o que a cifra endereça. Logo
 * não se varre: DESCE-SE, um nível por termo, e o aninhamento fica certo por construção porque
 * o aninhamento É a descida.
 *
 * Uma string lê-se num sítio só, e é lá que o escape se respeita — não espalhado por um scanner. */
/* A FONTE. Ler deixa de ser "andar num ponteiro" e passa a ser PEDIR O SIMBOLO k a alguem —
 * memoria ou banco, e a descida nao sabe qual. E o que tira os buffers do meio: enquanto o
 * analisador exigir bytes contiguos, tem de haver um array algures a segurar o objeto.
 *
 *   fn(ctx, k) devolve o simbolo k, ou -1 se acabou. So isso.
 *
 * Com isto a mesma descida serve uma linha em memoria e uma linha em slots, e o objeto nunca
 * precisa de existir inteiro em lado nenhum. */
typedef struct { int (*sim)(const void*, long); const void *ctx; } Fonte;
static int fonte_str(const void *c, long k){
    const char *s = (const char*)c;
    for(long i = 0; i < k; i++) if(!s[i]) return -1;
    return s[k] ? (unsigned char)s[k] : -1;
}
static Fonte fonte_de(const char *s){ Fonte f = { fonte_str, s }; return f; }
#define SIM(f,k) ((f)->sim((f)->ctx, (k)))
/* A descida, sobre a fonte: os mesmos passos, sem ponteiro. */
static long f_fim_string(const Fonte *f, long p){       /* p e o indice do " de abertura */
    p++;
    for(;;){
        int c = SIM(f, p);
        if(c < 0) return -1;
        if(c == '\\' && SIM(f, p+1) >= 0) p += 2;
        else if(c == '"') return p;
        else p++;
    }
}
static long f_fim_valor(const Fonte *f, long p){
    int c = SIM(f, p);
    if(c == '"'){ long e = f_fim_string(f, p); return e < 0 ? -1 : e + 1; }
    if(c == '[' || c == '{'){
        int ab = c, fe = (ab == '[') ? ']' : '}';
        int prof = 0;
        for(;;){
            c = SIM(f, p);
            if(c < 0) return -1;
            if(c == '"'){ long e = f_fim_string(f, p); if(e < 0) return -1; p = e + 1; continue; }
            if(c == ab) prof++;
            else if(c == fe){ prof--; if(!prof) return p + 1; }
            p++;
        }
    }
    for(;;){ c = SIM(f, p); if(c < 0 || c == ',' || c == ']' || c == '}') return p; p++; }
}
static long f_desce(const Fonte *f, long p, int k){
    int c = SIM(f, p);
    if(c != '[' && c != '{') return -1;
    p++;
    for(int n = 0;; n++){
        while((c = SIM(f, p)) == ' ' || c == '\t') p++;
        if(c < 0 || c == ']' || c == '}') return -1;
        long fim = f_fim_valor(f, p);
        if(fim < 0) return -1;
        if(n == k) return p;
        p = fim;
        while((c = SIM(f, p)) == ' ' || c == '\t') p++;
        if(c == ',') p++;
    }
}
/* Achar uma agulha na fonte, sem a fonte existir contigua. E o strstr sem o array por baixo. */
static long f_acha(const Fonte *f, const char *agulha){
    for(long p = 0;; p++){
        if(SIM(f, p) < 0) return -1;
        long k = 0;
        while(agulha[k] && SIM(f, p + k) == (unsigned char)agulha[k]) k++;
        if(!agulha[k]) return p;
    }
}
/* O texto entre aspas que comeca em p (que aponta ao "), copiado para out ate lim. E a UNICA
 * copia que sobra, e e do CAMPO — nao do objeto. */
static long f_str(const Fonte *f, long p, char *out, size_t lim){
    long e = f_fim_string(f, p);
    if(e < 0) return -1;
    long n = e - p - 1;
    size_t m = (size_t)n < lim - 1 ? (size_t)n : lim - 1;
    for(size_t k = 0; k < m; k++) out[k] = (char)SIM(f, p + 1 + (long)k);
    out[m] = 0;
    return n;
}
static const char *js_fim_string(const char *p){        /* p aponta ao " de abertura */
    p++;
    while(*p){
        if(*p == '\\' && p[1]) p += 2;                  /* o escape: dois símbolos, um valor */
        else if(*p == '"') return p;
        else p++;
    }
    return NULL;
}
static const char *js_fim_valor(const char *p){         /* o fim do valor que começa em p */
    if(*p == '"'){ const char *e = js_fim_string(p); return e ? e + 1 : NULL; }
    if(*p == '[' || *p == '{'){
        char ab = *p, fe = (ab == '[') ? ']' : '}';
        int prof = 0;
        while(*p){
            if(*p == '"'){ const char *e = js_fim_string(p); if(!e) return NULL; p = e + 1; continue; }
            if(*p == ab) prof++;
            else if(*p == fe){ prof--; if(!prof) return p + 1; }
            p++;
        }
        return NULL;
    }
    while(*p && *p != ',' && *p != ']' && *p != '}') p++;
    return p;
}
/* Desce um nível: devolve o k-ésimo elemento do container que começa em p. */
static const char *js_desce(const char *p, int k){
    if(*p != '[' && *p != '{') return NULL;
    p++;
    for(int n = 0; *p; n++){
        while(*p == ' ' || *p == '\t') p++;
        if(*p == ']' || *p == '}') return NULL;
        const char *fim = js_fim_valor(p);
        if(!fim) return NULL;
        if(n == k) return p;
        p = fim;
        while(*p == ' ' || *p == '\t') p++;
        if(*p == ',') p++;
    }
    return NULL;
}
/* O CAMINHO: um termo por nível, como a cifra. Devolve a string do fim, sem as aspas. */
static const char *js_caminho(const char *raiz, const int *cam, int n, size_t *len){
    const char *p = raiz;
    for(int i = 0; i < n; i++){
        p = js_desce(p, cam[i]);
        if(!p) return NULL;
    }
    if(*p != '"') return NULL;
    const char *e = js_fim_string(p);
    if(!e) return NULL;
    *len = (size_t)(e - p - 1);
    return p + 1;
}

/* ---- CADA FORMATO É UM ELEMENTO DO HIPERCORPO, E A SUA ASSINATURA É A RÉGUA ----
 *
 * Eu tinha escrito três descidas. São UMA. O formato não é implementação: é ELEMENTO, e o que o
 * distingue é a ASSINATURA — como ele marca o nível. Exatamente como um corpo é (B,C) e o resto
 * sai daí, um formato é (marca, largura, por linha) e a descida sai daí.
 *
 *   assinatura         marca  largura  por linha
 *   JSON               [ {      -        não      o nível é o parêntese, e aninha
 *   YAML               espaço   2        sim      o nível é a indentação
 *   Markdown           #        1        sim      o nível é a contagem de cardinais
 *
 * Uma só descida os lê aos três. Se amanhã entrar outro formato, entra uma LINHA de assinatura,
 * não uma função — que é o que ser elemento quer dizer. */
/* A ASSINATURA É SEMPRE SÓ UMA: A CIFRA. Eu tinha escrito um struct com marca, largura e por
 * linha — três campos meus, e uma "assinatura" por formato. Não há três: há a cifra, e cada
 * formato é um ELEMENTO com a sua, na mesma coordenada dos textos, dos números e dos 31 corpos.
 *
 * A cifra de um formato é a do seu GERADOR — a marca com que ele escreve o nível — símbolo a
 * símbolo, como 'ouro' se cifrou, mais a largura do passo:
 *
 *   json  '['  ->  [60; 0]     largura 0: o nível não é prefixo de linha, aninha
 *   yaml  ' '  ->  [ 1; 2]     dois espaços por nível
 *   md    '#'  ->  [ 4; 1]     um cardinal por nível
 *
 * Dois termos, e é tudo. Um formato novo é uma cifra nova, não uma função nova — e a distância
 * entre JSON e YAML passa a ser uma pergunta com resposta, na régua de sempre. */
/* UM FORMATO É UM CORPO, e como qualquer corpo ele é (razão, sinal) — e a cifra sai do
 * cifra_geral, o mesmo que encodou os 31. Não há codificador para formatos.
 *
 *   a RAZÃO   quantos símbolos por nível
 *   o SINAL   se a marca FECHA (-1: o parêntese abre e fecha, as duas direções cancelam-se)
 *             ou só se acumula (+1: a indentação e o cardinal não têm marca de fecho)
 *
 *   json  razão 1  sinal -1     [ ... ]  fecha
 *   yaml  razão 2  sinal +1     dois espaços, e nada fecha
 *   md    razão 1  sinal +1     um cardinal, e nada fecha
 */
typedef struct { const char *nome; char marca; long razao, sinal; } Assinatura;
static const Assinatura FORMATOS[] = {
    { "json", '[', 1, -1 },
    { "yaml", ' ', 2, +1 },
    { "md",   '#', 1, +1 },
};
#define AS_MARCA(a)   ((a)->marca)
#define AS_LARGURA(a) ((a)->razao)
#define AS_LINHA(a)   ((a)->sinal > 0)
/* o nível de uma linha, pela assinatura: contar a marca e dividir pela largura */
static int as_nivel(const Assinatura *a, const char *l){
    int n = 0;
    while(l[n] == AS_MARCA(a)) n++;
    return (int)(n / AS_LARGURA(a));
}
/* A DESCIDA, UMA SÓ. Para o JSON delega no aninhamento (que é a marca dele); para os outros
 * conta a marca por linha. É o mesmo passo, com a assinatura a dizer onde olhar. */
static const char *desce(const Assinatura *a, const char *p, int nivel, int k, size_t *len){
    if(!AS_LINHA(a)){                                  /* JSON: a marca é o parêntese */
        const char *e = js_desce(p, k);
        if(!e) return NULL;
        const char *f = js_fim_valor(e);
        *len = f ? (size_t)(f - e) : 0;
        return e;
    }
    int n = 0;
    while(*p){
        const char *fim = strchr(p, 10);
        size_t m = fim ? (size_t)(fim - p) : strlen(p);
        int v = as_nivel(a, p);
        if(m && v == nivel){
            if(n == k){ *len = m; return p; }
            n++;
        } else if(m && v && v < nivel && n) return NULL;   /* v && : linha sem marca nao acaba
                                                            o nivel — foi o que eu perdi ao
                                                            unificar, e o Markdown partiu */
        if(!fim) break;
        p = fim + 1;
    }
    return NULL;
}
#endif
