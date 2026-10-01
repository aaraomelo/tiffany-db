/* unidade.h — cada asserção vira um TESTE UNITÁRIO endereçável.
 *
 * Os medidores já eram unitários por dentro. O que faltava não era o teste — era o ENDEREÇO:
 * a bateria só sabia dizer "o medidor passou", e um programa que faz vinte afirmações é grosso
 * demais para servir de unidade.
 *
 * Há dois idiomas no repositório, e este arquivo cobre os dois sem reescrever a lógica de
 * nenhum:
 *
 *   ok("o que se afirma", condição)      — 29 medidores. O endereço é a própria frase.
 *   VD(condição, "msg")                   — os outros, que já escreviam
 *                                             printf(… , cond ? "FALHA" : "a mensagem")
 *                                           e agora escrevem printf(… , VD(cond, "a mensagem")).
 *                                           A string devolvida é a MESMA, a saída humana não muda,
 *                                           e o rótulo da unidade é a própria mensagem — que já
 *                                           dizia em português o que se afirmava.
 *
 * As linhas #UNIT saem TODAS no fim, por atexit, e não no meio do texto: quem lê continua a ler
 * o relatório, e quem conta lê o rodapé. Um só lugar, dois leitores.
 *
 * E é isto que liga a bateria à transformada. A MEMBRANA é o vetor das asserções — uma entrada
 * por teste, 0 quando passa. Por transformada.c §U4, x = 0 ⟺ ‖Fx‖² = 0: o selo é a norma, zero
 * exatamente quando tudo passa, e imune a cancelamento por ser soma de quadrados. Por §U5 o
 * Dirac LOCALIZA — antes o selo apontava um programa, agora aponta uma afirmação.
 *
 * A finura serve para saber ONDE olhar quando algo se mexe. Não para rodar mais.
 */
#ifndef UNIDADE_H
#define UNIDADE_H
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int falhas = 0;
static int unidades = 0;

/* O RODAPÉ NÃO SE GUARDA: ESCREVE-SE QUANDO ACONTECE.
 *
 * Isto tinha `uni_rot[1024]` e `uni_est[1024]` — 12 KB por medidor, para segurar
 * asserções que só seriam impressas no fim. Com 289 medidores, eram 3 173 KB: 39% de
 * toda a RAM estática que restava no sistema, num ficheiro só, replicada 289 vezes.
 *
 * E o teto de 1024 era outra coisa que se pagava sem se usar: um medidor tem dezenas de
 * asserções, não mil.
 *
 * Agora cada asserção vai para um ficheiro assim que acontece, e o rodapé despeja-o no
 * fim. Zero vetores, sem teto, e o rodapé sai igual ao que saía — é a mesma linha
 * `#UNIT ok/falha` na mesma ordem. É a máquina sem memória aplicada ao instrumento que
 * a mede. */
static FILE *uni_f = NULL;
static char  uni_cam[64];
static int   uni_n = 0, uni_registado = 0;

static void uni_rodape(void){
    if(!uni_f) return;
    fflush(uni_f);
    FILE *r = fopen(uni_cam, "r");
    if(r){
        char linha[1024];
        while(fgets(linha, sizeof linha, r)) fputs(linha, stdout);
        fclose(r);
    }
    fclose(uni_f); uni_f = NULL;
    unlink(uni_cam);
}
static void uni_poe(const char *rot, int passou){
    unidades++;
    if(!passou) falhas++;
    if(!uni_registado){
        snprintf(uni_cam, sizeof uni_cam, "/tmp/uni_%d.txt", (int)getpid());
        uni_f = fopen(uni_cam, "w+");
        atexit(uni_rodape);
        uni_registado = 1;
    }
    if(uni_f) fprintf(uni_f, "#UNIT %s  %s\n", passou ? "ok   " : "falha", rot);
    uni_n++;
}

/* SALTOU — a terceira palavra, e ela faltava.
 *
 * Havia duas: `ok` e `falha`. Mas um medidor tem um terceiro estado — NAO MEDIU, e por uma
 * razao de FORA: a fonte nao esta instalada, o modelo .gguf nao esta no disco, o multicast
 * nao respondeu. Esses ramos imprimiam um aviso e nao emitiam unidade nenhuma, e o total
 * da bateria MUDAVA com a maquina.
 *
 * Isso nao e uma imprecisao: o total e o detector de eu ter escrito por cima de um medidor
 * — foi ele que salvou o tests/potencia.c. Um total que muda sozinho cega-o. Medido no
 * gitb.c: `git pack-refs` levava-o de 10 para 9 unidades, e a bateria de 501 para 500.
 *
 * Com `saltou`, a unidade EXISTE e diz-se do que precisava. O total passa a ser
 * ok + falha + saltado, que e o mesmo em qualquer maquina, e o que varia fica CONTADO e
 * com motivo — que e a licao do medidor que nunca mediu: a atestacao guarda o resultado, e
 * o motivo tem de ir junto. */
static long saltadas = 0;
static void saltou(const char *r, const char *porque){
    printf("      %-58s — NAO MEDIDO: %s\n", r, porque);
    saltadas++;
    if(!uni_registado){
        snprintf(uni_cam, sizeof uni_cam, "/tmp/uni_%d.txt", (int)getpid());
        uni_f = fopen(uni_cam, "w+");
        atexit(uni_rodape);
        uni_registado = 1;
    }
    if(uni_f) fprintf(uni_f, "#UNIT salta  %s  [%s]\n", r, porque);
    uni_n++;
}

/* o idioma dos 29: a afirmação em português É o endereço */
static void ok(const char *r, int c){
    printf("      %-58s %s\n", r, c ? "sim ✓" : "NÃO ✗");
    uni_poe(r, c);
}

/* o idioma dos outros: devolve a MESMA string de antes, e regista pelo caminho.
 *
 * A troca é textual e reversível — `cond ? "FALHA" : "a msg"` vira `VD(cond, "a msg")`. E o
 * rótulo da unidade é a PRÓPRIA mensagem de sucesso, que já dizia em português o que estava a
 * ser afirmado: não foi preciso inventar nome para teste nenhum, eles já tinham. */
static const char *uni_vd(int falhou, const char *bom){
    uni_poe(bom, !falhou);
    return falhou ? "FALHA" : bom;
}
#define VD(cond, bom) uni_vd((cond), (bom))


/* CONCLUI — o resumo de uma secção, e NÃO uma unidade.
 *
 * Estas linhas eram `ok("...", 1)`: a constante disfarçada, e havia 66 delas no repositório.
 * Mas o defeito não era afirmarem falso — era serem `ok()`. Elas RESUMEM o que as asserções
 * acima mediram, e um resumo não é uma medida: contadas como unidade, inflacionavam a bateria
 * com frases. Marcadas com [~], a contagem passa a ser só do que mede.
 */
static void conclui(const char *q){ printf("  [~] %s\n", q); }

#endif
