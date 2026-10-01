/* jev_casos.h — os 96 casos E1–E7, gerados, sem nenhum ficheiro de entrada.
 *
 * Não há blob de casos no repositório: os 96 casos são uma FUNÇÃO de números
 * pequenos e de uma semente. O que está congelado no repositório é a SAÍDA
 * (tests/jev_golden.bin), tirada do marginal.wasm original.
 *
 * Este ficheiro é uma porta de linha a linha de gen.mjs, que no laboratório
 * gerava os mesmos casos e chamava pelo WASM. A ordem dos 96 é a mesma e não
 * pode mudar: o golden é uma concatenação pela ordem, e trocar dois casos
 * troca dois blocos de 65536 bytes sem dar erro nenhum.
 *
 * ── PORQUE TRABALHA NA ARENA DA BIBLIOTECA ────────────────────────────────────
 * Em E5, a leitura isolada NÃO limpa a arena antes de se montar: parte do que a
 * chamada anterior (duas) deixou lá. Para essa entrada ser a mesma que o
 * laboratório viu, é preciso executar duas a sério. Logo o gerador não pode
 * montar em um buffer seu: monta em jev_arena(), que é a arena que a leitura
 * de facto escreve. E o ensaio gen.mjs fazia exatamente isto — partilhava a
 * arena e só guardava cópias.
 */
#ifndef JEV_CASOS_H
#define JEV_CASOS_H

#define JEV_ARENA_B   65536
#define JEV_CELULAS   (JEV_ARENA_B / 4)

/* O que se chama, por caso. A ordem dos valores é a do gen.mjs e não se mexe:
 * o golden está indexado por esta numeração. */
enum {
    JEV_MARGINAL   = 0,
    JEV_ESCORE     = 1,
    JEV_DUAS       = 2,
    JEV_MARGINAL_D = 3
};

typedef struct {
    char     nome[64];   /* só para o diagnóstico; o golden não depende disto */
    int      fn;         /* o enum acima */
    int      X;          /* |I|, como o servo o recebe */
    unsigned char *arena;/* a ENTRADA deste caso, em jev_arena() */
} jev_caso;

/* Quantos casos existem. 96. */
int jev_casos_total(void);

/* Percorre os 96 por ordem, na ordem do golden. `*cursor` começa a 0 e avança
 * uma unidade por chamada; devolver NULL no fim.
 *
 * A arena que cada caso deixa escrita é a de jev_arena(), e é REESCRITA no caso
 * seguinte. Copie-a se precisar dela depois de avançar. */
const jev_caso *jev_casoSeguinte(int *cursor);

#endif
