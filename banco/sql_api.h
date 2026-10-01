/* sql_api.h — porta C do motor SQL (banco/sql.c) para pgwire / apps.
 *
 * Não é Postgres. É a face estável: abrir base, executar, capturar linhas.
 * O bytecode ISA e o .mem continuam dentro de sql.c.
 */
#ifndef SQL_API_H
#define SQL_API_H

/* SESSENTA E QUATRO, e já foi oito e dezasseis — o mesmo erro três vezes, com
 * três números. O oito bastava para as tabelas desta casa; o dezasseis caiu no
 * `\d` do psql, que pede TREZE colunas por linha e as lê por POSIÇÃO; e o
 * dezasseis caiu outra vez no primeiro esquema de cliente a sério, onde onze
 * tabelas passam das dezasseis colunas e a maior tem vinte e nove.
 *
 * Este número é da JANELA que esta porta oferece, e não do objecto: a tabela
 * não tem tecto nenhum (o catálogo guarda o número de colunas num par, o corpo
 * vai a COL_MAX e a ordem da saída mora numa zona do .mem), e a resposta vive
 * no DISCO como tudo nesta casa — «a memória é o DISCO. Sem RAM» é a primeira
 * linha do sql.c. O que tem tamanho é esta struct, que é uma conveniência da
 * fronteira C para quem quer a resposta de uma vez. Enquanto ela for o único
 * caminho, o motor RECUSA o que não cabe em vez de truncar: uma linha truncada
 * não é uma linha curta, é uma linha em que a posição k passa a ser outra
 * coisa. Quem precisar de mais nomeia as colunas — e essas atravessam. */
/* QUANTAS LINHAS COMPORTA O MAPA DESTA MÁQUINA — e PERGUNTA-SE, não se escreve.
 * A teoria não tem tecto de linhas (`§sec:torre`: o que cresce é o objecto, não a
 * máquina); o MAPA DE SLOTS tem, e quem não cabe RECUSA. Escrevê-lo aqui seria o
 * número em dois sítios: o mapa mudaria e este mentiria. */
long sql_lin_tecto(void);
/* e o OUTRO eixo da mesma parede: a célula ocupa um slot por zona e o endereço é
 * i·ncols + j, pelo que o mapa limita o PRODUTO. Com uma coluna aperta o bitmap
 * das linhas; com oito, aperta este. */
long sql_cel_tecto(void);

/* QUANTAS VEZES SE PEDIU UM BIT FORA DO SEU BITMAP. Os três bitmaps da casa
 * atravessam todos o mesmo par de funções, pelo que a lei vive lá: quem sai não
 * escreve no vizinho, CONTA. Isto é um instrumento, não uma asserção --- diz
 * quantas vezes, e quem mede exige que seja zero. */
long sql_bits_fora(void);
void sql_bits_fora_zera(void);
int  sql_bit_fora_de_proposito(long i);   /* o gume do guarda: ver sql.c */

/* e o mesmo para as CÉLULAS. Estas não têm porta única --- o endereço i·ncols+j
 * forma-se em quarenta sítios ---, pelo que se lhes fez uma: `cel_ix`. É
 * INSTRUMENTO e não correcção: devolve o índice tal como veio e apenas conta
 * quando ele sai do tecto, porque truncar mandaria a escrita para outra célula,
 * que é corromper de outra maneira. A recusa está a montante, no INSERT. */
long sql_cels_fora(void);

/* e quantas vezes um RESTAURO de tabela não conseguiu voltar. Vinte sítios do
 * motor guardam o nome, mudam de tabela e voltam — e nos vinte o retorno era
 * deitado fora. Quando o restauro falha o `fmem` não se move: a sessão fica na
 * tabela errada e responde sobre outro objecto, sem erro nenhum. */
long sql_restauros_falhados(void);

/* e quantas chaves o percurso da árvore não pôde entregar. Onde o resultado são
 * linhas isto é visível --- a janela da porta C recusa; onde é um AGREGADO não
 * havia nada a cobri-lo, e o count(DISTINCT) respondia 64 a 65 classes. */
long sql_ord_perdidos(void);

/* as DUAS réguas da caixa de Pandora (`aranha thm:pandora`): a aritmética, que
 * diz quanto falta, e a profundidade da ultramétrica, que dá a partição. */
/* o resumo óptico do catálogo — contado UMA vez, lido pelo comando e pelo medidor */
void sql_optica_resumo(long *fora, long *prop, long *el, long *par, long *hip,
                       long *borda, long *viola);
/* o catálogo dos corpos com nome próprio, pela porta */
int  sql_corpo28_n(void);
const char *sql_corpo28(int i, long *B, long *C);
/* «a cifra para?» — |Δ| quadrado, o critério do cifra.h entre Euclides e PQa */
int  sql_cifra_para(long B, long C);
/* a raiz inteira da casa (o `raizi` do cifra.h), pela porta */
long sql_raizi(long n);
/* a cifra (traço, determinante, discriminante) de um corpo, pela porta */
void sql_corpo_cifra(long corpo, long parm, long *B, long *C, long *D);
int  sql_corpo_aureo(void);
int  sql_corpo_cristal(void);
/* os comparadores dos corpos, pela porta — a régua verdadeira, não uma cópia */
int  sql_au_cmp(long ua, long ub, long va, long vb, long m);
int  sql_cr_cmp(long ua, long ub, long va, long vb, long t);
long sql_esp_dist(long Di, long Dj);
int  sql_esp_prof(long Di, long Dj);

#define SQL_OUT_MAX_COLS 64
#define SQL_OUT_MAX_ROWS 64
#define SQL_OUT_CELL     64
#define SQL_TIPO_INT4    23
#define SQL_TIPO_TEXT    25
#define SQL_TIPO_INT8    20    /* o count() do Postgres é bigint, não int4 */

typedef struct {
    int ok;
    char err[160];
    char tag[80];                 /* CommandComplete: "SELECT 2", "INSERT 0 1", … */
    int ncols;
    int nrows;
    char col[SQL_OUT_MAX_COLS][32];
    /* O TIPO ACOMPANHA A COLUNA. Sem isto o wire anunciava tudo como int4 —
     * incluindo `SELECT version()`, que devolve texto —, e um cliente que
     * confiasse no OID convertia mal. O valor ia certo e o TIPO ia errado, que é
     * o defeito que nenhuma comparação de strings apanha. */
    int tipo[SQL_OUT_MAX_COLS];   /* PG_OID_INT4 (23) ou PG_OID_TEXT (25) */
    char cell[SQL_OUT_MAX_ROWS][SQL_OUT_MAX_COLS][SQL_OUT_CELL];
    /* A AUSÊNCIA ATRAVESSA O FIO. Uma célula vazia e uma célula ausente são
     * objectos diferentes — o `thm:bitunico` diz que a ausência é o dual, não
     * um valor —, e no protocolo isso escreve-se com o comprimento −1, não com
     * zero bytes. Sem esta máscara o motor sabia distingui-las e o wire não:
     * o valor ia certo e o NÍVEL ia errado. */
    unsigned char nulo[SQL_OUT_MAX_ROWS][SQL_OUT_MAX_COLS];
} SqlOut;

int  sql_abrir(const char *base);
void sql_fechar(void);
/* executa e, se out!=NULL, preenche tag/linhas (SELECT) ou só tag (DDL/DML) */
int  sql_executa(const char *sql, SqlOut *out);
/* os nomes das colunas de uma tabela — para o catálogo, que não os pode inventar */
int  sql_cols_de(const char *tabela, char nomes[][32], int cap);
/* A TRANSACÇÃO, e ela é o levantamento do `aranha.tex`: escrever é dobrar — o
 * valor novo cola-se por cima e a célula esquece qual era —, e guardar o
 * anterior é a coordenada que desdobra, com volta exacta. */
void sql_tx_abre(void);
int  sql_tx_desfaz(void);      /* 0 se a pilha encheu: então NÃO se desfaz nada */
void sql_tx_fecha(void);
int  sql_tx_cheia(void);
long sql_tx_escritas(void);
/* o campo G da transacção corrente, lido na pilha: |I|, |supp G| e max G */
/* a régua do percurso das escritas, e ela é a ULTRAMÉTRICA: a menor profundidade
 * do caminho (o maior salto), a profundidade de ponta a ponta, e se a absorção
 * do `cor:global` se verifica --- ver a nota no sql.c */
void sql_tx_ultra(long *prof_min, long *prof_ponta, long *absorve, long *passos);
void sql_tx_fibra(long *escritas, long *slots_distintos, long *maior_G,
                  long *soma_G);
/* o histograma de uma coluna — o campo G dos seus valores, para o passo
 * espectral: o tamanho de um join é (f*g)(0), a convolução na origem */
/* os passos da última varredura. O molde das linhas não ramifica — as quatro
 * faces correm num passo só —, pelo que este número depende do TAMANHO da
 * tabela e não do conteúdo dela. */
extern long sql_ultimos_passos;
/* os nós visitados na última descida da árvore — a PROFUNDIDADE do índice, que
 * não cresce com o tamanho da tabela. É esta a medida do índice, e não os
 * passos, que com ele são zero: zero não distingue não-correr de não-fazer. */
extern long sql_ultimos_nos;
/* o FNV do bytecode da última varredura: duas escritas que compilam para o
 * MESMO programa são o mesmo objecto, e não duas coisas com a mesma resposta */
extern long sql_ultimo_prog;
int  sql_histograma(const char *tabela, const char *coluna, long *hist, int n,
                    long *fora);

#endif
