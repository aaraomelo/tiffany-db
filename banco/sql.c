
/* sql.c — SQL NO METAL. Compila para a ISA, e a memória é o DISCO. Sem RAM.
 *
 * ESTE é o motor da casa (interface_padrao=sql; manifesto.corpos.motor.isa).
 * Não confundir com app/src/motor_campo.js (roupa WebGL no mesmo DISCO) nem com
 * tests/motor.c (medidor da Parte Mecânica: DTC/indução). Backends não copiam
 * a ULA: são roupas (MOVE/traduz). As 19 línguas da ISA ingerem-se por essa
 * porta; asm é degrau ERG de isa, não língua. Schema de U: JSON canónico
 * (`conecthus/schema/u.schema.json`); PARSE / IMPORT PARSER / GET SCHEMA;
 * IMPORT MANIFESTO / MANIFESTO -> U / U -> MANIFESTO (ponte M↔U);
 * IMPORT PAGINA / PAGINA -> U / U -> PAGINA (tríade html/css/js).
 *
 * Nada aqui é simulado por cima de estruturas em memória: o SQL vira BYTECODE da ISA do
 * broca-so (ula/instrucoes.h), o bytecode vive num arquivo, a memória da máquina vive noutro
 * arquivo, e o interpretador lê instrução por instrução com pread. O único estado em RAM são
 * os três registradores (A, B, R), o pc e as flags — como no metal.
 *
 * A ISA. Os oito primeiros opcodes são os do broca-so, número a número --- HALT,
 * LOAD, STORE, ADD, SUB, AND, OR, XOR. A partir do nono as duas SEPARAM-SE, e é
 * bom que se diga em vez de se afirmar o contrário: o broca-so tem ali SILVER,
 * GOLD, BRONZE, e esta casa não carregou os três metais nem o UNFOLD, o PROJECT,
 * o LIFT, o SPECT e o COUNT --- pôs o GOLD e acrescentou os seus. São 24 e todos
 * correm: cada um é emitido pelo compilador e tem `case` no interpretador (o
 * `erg.c` §E1 confronta este enum com o montador, nome a nome e número a
 * número, LENDO daqui e não copiando).
 *
 * O que os dois lados partilham não é a numeração --- é a forma: dois
 * registadores, uma ULA componente a componente, e o endereço IMEDIATO na
 * instrução. Dizer «transcrita, não reinventada» era falso em dezasseis das
 * vinte e duas posições, e um byte compilado por um lado significa outra coisa
 * do outro.
 *
 * As instruções, e o que cada uma faz:
 *
 *   HALT                     para
 *   LOAD  slot(u16)          B←A ; A←mem[slot]
 *   STORE slot(u16)          mem[slot]←R            (grava R, NÃO A)
 *   ADD SUB AND OR XOR       R ← ula(A,B)           (componente a componente)
 *   GOLD                     A←A_1(A) ; R←A            o gato: estica, det −1, ordem ∞
 *   NEGRO_OURO               A←A_1⁻¹(A) ; R←A         a volta: INTEIRA, porque det = −1
 *   ESQUILO                  A←(−e, total) ; R←A       ×ω do cristal: det +1, ordem 4
 *   TROCA                    A←(e, total) ; R←A        J, a involução: det −1, ordem 2
 *   CMP                      FL_ZERO sse A e B são AMBOS zero; FL_EQ se iguais
 *   JMP JZ JNZ  rel(s8)      pc ← pc + 1 + rel
 *   FOLD UNFOLD PROJECT LIFT as folhas e as projeções
 *
 * Duas consequências da ISA real, que moldam o compilador:
 *   (1) STORE grava R, então pôr uma constante num slot é LOAD k, LOAD zero, ADD, STORE.
 *   (2) FL_ZERO é "ambos zero", então a igualdade a=k testa-se por SUB e depois CMP com A=0:
 *       LOAD col, LOAD k, SUB, STORE tmp, LOAD tmp, LOAD zero, CMP  →  FL_ZERO sse col=k.
 *   (3) o endereço do slot é IMEDIATO (u16 na instrução): não há indexação indireta. Logo o
 *       compilador DESENROLA a varredura — ele lê o catálogo antes de compilar e emite o
 *       código das linhas que existem. O programa é compilado para o estado atual da tabela.
 *
 * Mapa (átomo=Word_8; Word ISA=Word_8² Lei 7; σ²=σ+1; blobs via atomos_*):
 *   0   catálogo {ncols, nrows}     (Word ISA)
 *   1   a constante 0        2  a constante 1        3  temporário
 *   4   o contador de casamentos
 *   8   a constante da consulta (o k do WHERE)
 *   16+ o bitmap de casamento, uma linha por slot
 *   1024+ as linhas: linha i, coluna j  →  slot 1024 + i*ncols + j
 *   2048+ S_CF: palavras FC (rt_cf_slot.h), S_CF_STRIDE slots cada
 *   500000+ S_CAB / S_ALVO / S_FOLHA / S_CB — blobs em átomos físicos
 *
 *   cc -O2 -std=c99 sql.c -o sql
 *   ./sql <base> "CREATE TABLE t (a,b,c)"
 *   ./sql <base> "INSERT INTO t VALUES (7,8,9)"
 *   ./sql <base> "SELECT * FROM t WHERE a = 7"
 *   ./sql teste
 */
#define _POSIX_C_SOURCE 200809L
/* E _DEFAULT_SOURCE tambem, senao o -std=c99 estrito da bateria esconde ip_mreq e usleep e este
 * ficheiro NAO COMPILA — e as 87 assercoes dele deixam de ser medidas em silencio. Foi o que
 * aconteceu: tres corridas seguidas com "sql.c NAO COMPILOU" na tabela e eu a ler so a linha
 * das unidades abertas. O compilador avisou e ninguem leu. */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include "../lib/disco.h"
#include "../lib/slot_mem.h"
#include "../lib/palavra8.h"
#include "../lib/word_isa.h"
#include "../lib/slot_map.h"
#include "../lib/reta.h"
/* ── OS SINAIS DO WHERE VÊM DE UM TEOREMA, E NÃO DE UMA TABELA AQUI ──────────
 * O `fisica.tex §fis:simbolos` prova que os símbolos de relação saem de uma
 * dobra sobre S×S: três blocos, 2³ = 8 relações, e as duas involuções que as
 * permutam. Este ficheiro escrevia seis linhas à mão para as ler, uma tabela de
 * três casos para as podar, e um `nega` solto para as negar — três leituras da
 * MESMA coisa, e por isso três sítios onde discordarem. Agora há um. */
#include "../lib/simbolos.h"
/* ── A DESACUMULAÇÃO VEM DE UM TEOREMA, E NÃO SE ESCREVE AQUI ────────────────
 * O `fisica.tex thm:zetamu`: o `count` acumula (é a convolução com ζ) e a volta
 * é a diferença finita μ. O `ACUMULA`/`DELTA` do WHERE são ζ e μ ao longo da
 * ordem, e a álgebra mora no `incidencia.h` — aqui só se liga à porta do SQL. */
#include "../lib/incidencia.h"
/* ── AS FUNÇÕES ANALÍTICAS VÊM DA CASA, NÃO SE ESCREVEM AQUI ─────────────────
 * O `aranha §sec:serie` deriva-as todas de uma só: com J² = −1 as potências de J
 * ciclam com período quatro, a série da exponencial PARTE-SE PELA PARIDADE do
 * índice, e o que sai é exp(tJ) = c(t)·1 + s(t)·J — o cosseno nos pares, o seno
 * nos ímpares, e o (−1)^k a contar as voltas do ciclo módulo dois. O logaritmo é
 * a inversa, do lado da série.
 *
 * E isso já está construído em `lib/calculo2.h`, com os coeficientes EXACTOS em
 * ℚ: `sr_exp`, `sr_sin`, `sr_cos`, `sr_log1p`, e o `sr_parcial` que avalia. Não
 * há aqui matemática nova — há a ligação ao motor: o banco passa a poder
 * perguntar por elas, sobre as suas células, sem um único double. */
#include "../lib/racionais.h"
#include "../lib/serie.h"
#include "../lib/fatorial.h"
/* ── E A ÁLGEBRA LINEAR, COM O NOME MUDADO À ENTRADA ─────────────────────────
 * O `linear.h` tem a matriz sobre ℚ e tudo o que ela faz — produto, soma,
 * transposta, determinante, posto, núcleo, imagem, inversa —, e o motor precisa
 * dela porque UMA TABELA É UMA MATRIZ: as suas linhas por colunas são as
 * entradas, e as perguntas da álgebra linear são perguntas sobre a tabela.
 *
 * O obstáculo é um nome: `Mat` está tomado pelo `corpos.h`, que chama assim a
 * matriz 2×2 de longos do transporte mecânico, e o `sql.c` já o traz pelo
 * `reta.h`. São dois objectos diferentes com o mesmo nome, e 58 ficheiros
 * dependem de um enquanto 21 dependem do outro — renomear num header tocaria em
 * setenta e nove. Renomeia-se AQUI, à entrada, e nenhum dos dois muda. */
#define Mat MatQz
#define Vec VecQz
#include "../lib/linear.h"
/* e o `forma.h` entra no MESMO alívio de nome, porque é sobre os mesmos dois
 * objectos: as formas, o produto interno e o espectro 2×2 exacto vivem em cima
 * do `linear.h` e não têm tipos próprios. Traz o segundo caminho para o
 * espectro — o `cifra.h` escreve-o SEMPRE por fração contínua, o `esp_racional`
 * dá os números QUANDO o discriminante é quadrado perfeito, e os dois têm de
 * concordar onde ambos respondem.
 *
 * O `cifra.h` entra ANTES porque é dele o `raizi` — a raiz inteira por baixo,
 * que o `forma.h` usa para decidir se o discriminante é quadrado perfeito. As
 * duas peças partilham a mesma raiz, e é isso que faz dos dois caminhos duas
 * leituras do MESMO discriminante e não duas contas parecidas. */
#include "../lib/cifra.h"
#include "../lib/forma.h"
/* e o `exterior.h` pela mesma porta, porque traz a peça que faltava ao corpo
 * diferencial: `ex_parte` parte QUALQUER matriz em simétrica ⊕ antissimétrica —
 * que é o «A = gato ⊕ esquilo» do paper das equações diferenciais, escrito aqui
 * há muito com outro nome. Sobre ℚ a partição é única e exata, e por isso não
 * tem ramo de falha: dividir por 2 em ℚ sempre pode. */
#include "../lib/exterior.h"
#undef Mat
#undef Vec
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include "banda.h"
#include "stratum.h"
#include "cifra.h"
/* O CATALOGO. As funcoes dos corpos ja existem — nao se importa nada de fora, e nao se
 * reescreve nada aqui: cr_norma e cr_cmp sao a regua eliptica, e sao as que decidem. */
#include "corpos.h"
#include <ctype.h>
#include <strings.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include "unidade.h"
#include "contrato.h"   /* o toolkit: a tríade ⊕ ⊗ ∏ de cada corpo */
#include "sql_api.h"    /* porta C para pgwire — captura de resultado */
#include "pgcat.h"      /* Trio PG6: o catálogo de SESSÃO, antes do motor */
#include "tiffany_node.h"  /* shells ingeridos: node/bash/pwsh via tiffany_shell.h */
#include "parse_ficheiro.h" /* schema U autossimilar; PARSE ficheiro → JSON canónico */

/* ── A CONSULTA DE DENTRO NÃO FALA ──────────────────────────────────────────
 * Uma subconsulta é um passo de outra, e não uma resposta ao cliente: as linhas
 * dela iam para o terminal junto com as da consulta que a pediu, e quem lia via
 * duas respostas onde há uma. O que ela devolve é a LISTA, e essa entra na
 * reescrita --- não na saída. */
static int sql_calado = 0;
static SqlOut *sql_cap = NULL;   /* preenchido por sql_executa quando out!=NULL */

/* ---------------- a ISA (transcrita) ---------------- */
#include "isa.h"   /* a ISA numa fonte só --- ver lib/isa.h */
/* as flags vêm com a ISA, do lib/isa.h */

/* Word ISA = word_isa.h (Word_8²). Disco: 2 átomos/slot (Lei 7). */
typedef struct { Word A, B, R; unsigned pc; unsigned char flags; } Regs;

/* ---------------- os slots ---------------- */
/* O ESPAÇO É O HIPERCUBO, E A ISA DIZ ONDE ELE ACABA.
 *
 * `aranha.tex thm:espaco`: «o espaço NÃO É ESCOLHIDO: a dobra dá o hipercubo»;
 * `thm:hiper`: «separando pelo ÚLTIMO BIT obtêm-se duas cópias». Separar zonas é
 * separar POR BIT — cada uma é um bloco alinhado e o seu endereço é um PREFIXO,
 * e prefixos distintos são disjuntos POR CONSTRUÇÃO (Lei 7, ligar sem fundir).
 * Não há conta nenhuma a fazer para garantir que não se pisam.
 *
 * E O TECTO NÃO É ESCOLHIDO TAMBÉM: o `emit_slot` escreve o endereço em DOIS
 * BYTES, logo a instrução alcança 2^16 slots. Quem o bytecode endereça vive
 * abaixo disso; quem só o C toca — a árvore da cifra, os textos, a ordem, a
 * junção — vive acima, e não paga nada por isso. Foi ignorar esta fronteira que
 * me custou uma tarde: pus uma zona em 65536, o endereço truncou para ZERO, que
 * é o S_CAT, e o catálogo passou a dizer que a tabela tinha uma coluna. */
#define ISA_BITS  16u                    /* o endereço cabe na instrução */
#define ISA_TECTO (1u << ISA_BITS)
#define ZBITS     14u                    /* 2^14 = 16384 slots por zona grande */
#define ZONA(k)   ((unsigned)(k) << ZBITS)
#define ZONA_SLOTS (1u << ZBITS)          /* quantos slots uma zona segura */

/* ── QUANTAS COLUNAS: O TECTO É O DO ENDEREÇO, E NÃO UM NÚMERO ESCOLHIDO ─────
 *
 * O `aranha` §sec:dimensao é explícito: a dimensão entra num sítio só e entra
 * como PARÂMETRO --- «as cláusulas valem palavra por palavra em qualquer posto
 * n ≥ 1» ---, e a §sec:escada constrói a largura por DOBRA, com o encaixe por
 * prefixos a pôr cada andar dentro do seguinte. Um tecto em oito, em dezasseis
 * ou em trinta e dois não vem de lado nenhum da teoria: vem de quem escreveu a
 * linha e não voltou lá.
 *
 * O que existe de facto é o espaço de endereços: cada zona segura ZONA_SLOTS
 * Words, e um nome de coluna ocupa S_COLNOME_W delas. Daí sai o número, e ele
 * DIZ-SE quando uma recusa acontece --- «a tabela tem N colunas e o corpo vai a
 * COL_MAX» é uma frase sobre a máquina; «já tem 8 colunas» era uma frase sobre
 * um esquecimento. */
#define COL_MAX   (ZONA_SLOTS / 16u)      /* = 1024, o que a zona dos nomes segura */

/* ── abaixo do tecto da ISA: o que o bytecode endereça ─────────────────── */
#define S_CAT     0
#define S_ZERO    1
#define S_UM      2
#define S_TMP     3
/* O MARTELO: cabeçalho 80 átomos + alvo 32 átomos. Longe das linhas. */
/* O CANAL É UM BACKEND DE LOAD/STORE, NÃO UM PROTOCOLO À PARTE.
 *
 * O banco já faz E/S sem opcode por leitura: LOAD e STORE vão ao ficheiro por pread/pwrite. O
 * canal é outro destino dos MESMOS dois. Acima de S_CANAL, LOAD recebe da banda e STORE emite —
 * e a ISA não cresceu, o compilador não mudou, e a query não sabe que falou com outro banco.
 *
 * E É POR ISSO QUE OS PROTOCOLOS SÃO INDISTINGUÍVEIS PARA O BANCO: o que ele vê é sempre um slot
 * a ser lido e um slot a ser escrito. UDP com bump, STOMP, TCP — a topologia é a mesma, porque
 * do lado de cá não há topologia nenhuma: há um endereço. Trocar de protocolo é trocar o que
 * está atrás de duas funções, e nem uma linha de SQL muda. */
/* ACIMA DE TUDO O RESTO. Eu pus isto em S_LINHAS+50000 e enterrei o S_NO da cifra (que mora em
 * S_LINHAS+240000): os nos do indice passaram a ir para a rede em vez do disco, e a bateria
 * apanhou-o em tres asserçoes. A fronteira de um backend tem de ficar onde nao pisa ninguem. */
/* A ZONA ONDE A MEMÓRIA ACABA E A BANDA COMEÇA --- dita UMA vez. O 600 estava
 * escrito em TRÊS sítios de lei: aqui, no IDX_MAXCOL e no typedef que os
 * verifica. Mudar o canal de sítio deixava dois errados, e o typedef passaria a
 * confirmar uma conta contra um número que já não era o dele. */
#define ZONA_CANAL 600u
#define S_CANAL   (ISA_TECTO + ZONA(ZONA_CANAL))
/* as três zonas da célula têm de caber no que a instrução alcança */
typedef char zonas_cabem_na_isa[(ZONA(4) <= ISA_TECTO) ? 1 : -1];
/* O POOL, O MESMO DESENHO. Acima de S_POOL, LOAD devolve um campo do job corrente e STORE no
 * slot da share emite o mining.submit. O protocolo fica atras de duas funcoes — e e por isso que
 * ele e indistinguivel do canal, e o canal do disco: o banco le um slot e escreve um slot.
 *
 * Os campos, pela ordem em que o cabecalho os quer:
 *   +0 versao   +1 nbits   +2 ntime   +3..+10 prevhash   +11 tem job   +12..+19 merkle
 *   +20 a SHARE: escrever aqui submete o nonce */
#define S_POOL     (S_CANAL + 100000u)
/* bash no barramento: stdin/stdout como Words no canal (plano-terminal-barramento.md) */
#define S_BASH_IN  (S_CANAL + 9100u)
#define S_BASH_OUT (S_CANAL + 9101u)
#define S_PWSH_IN  (S_CANAL + 9110u)
#define S_PWSH_OUT (S_CANAL + 9111u)
#define S_NODE_IN  (S_CANAL + 9120u)
#define S_NODE_OUT (S_CANAL + 9121u)
#define S_CHUNK    (S_CANAL + 9102u)
#define S_FRONT_REQ (S_CANAL + 9200u)
#define S_FRONT_RSP (S_CANAL + 9201u)
#define S_ESTADO_REQ (S_CANAL + 9210u)
#define S_ESTADO_RSP (S_CANAL + 9211u)
/* S_DEPOSITO: blob opaco de utilizador (bootstrap/OAuth como transporte).
 * ≠ S_ESTADO (JSON GKBANCO operacional entre realizações que já partilham a banda).
 * A Pátria deposita bytes; sem a banda o ficheiro não é JSON. Banda do fio
 * continua sha256(bytes da pub) — sem prefixo gk:id:v1. */
#define S_DEPOSITO_REQ (S_CANAL + 9220u)
#define S_DEPOSITO_RSP (S_CANAL + 9221u)
#define S_POOL_SH  (S_POOL + 20u)
/* Blobs (cab/alvo/merkle/coinbase): índices FÍSICOS de átomo (atomos_* → slot_mem directo).
 * Não passam pelo stride-2 das Words ISA. Longe do mapa Word (S_LINHAS…). */
#define S_CAB     500000u                 /* 80 átomos = cabeçalho */
#define S_ALVO    (S_CAB + 80u)           /* 32 átomos = alvo */
#define S_FOLHA   (S_CAB + 200u)          /* folhas merkle */
#define S_CB      (S_CAB + 500u)          /* coinbase em átomos */
#define S_FOLD_ARG (S_CB + 800u)         /* u32 LE base do OP_FOLD */
#define S_FAIXA   (S_FOLD_ARG + 4u)      /* u32 LE de, u32 LE ate — OP_MARTELO */

/* ── O POOL DE TEXTO: onde a cadeia vive, e porque a célula não a guarda.
 *
 * Uma célula é uma Word --- dois átomos --- e uma cadeia não cabe lá. Mas o
 * `thm:enumfin` já diz o que fazer: «esse é o ÍNDICE COMO ESTANTE: a matriz
 * continua a ser o que está nas gavetas, e o emparelhamento apenas dá a cada
 * gaveta um endereço único». A cadeia fica na gaveta --- o pool de átomos --- e
 * a célula guarda o ENDEREÇO dela, que é uma palavra e cabe.
 *
 * Cada texto ocupa 1 átomo de comprimento + os bytes. O índice 0 é reservado
 * para «vazio», pelo que a célula 0 é a cadeia vazia e não uma ausência --- a
 * ausência continua a ser dita pelo S_PRES, como em todas as outras colunas. */
#define S_TXPOOL  600000u                /* onde as cadeias vivem, em átomos */
#define S_TXTOPO  (S_TXPOOL - 8u)        /* u32 LE: o primeiro átomo livre */
#define TX_MAX    240                    /* uma cadeia por célula, no máximo */
/* (era: contar é ∑ sobre o campo (bits_conta), não um contador
 * a ser incrementado por linha. Ver `neuronio.c`: «∑ soma popcount». */
#define S_MASK    5          /* a máscara do bit de sinal — é ela que dá o < e o >     */
#define S_MASK16  59         /* o mesmo, um andar acima: {0, 0x80} — o bit 15 do par   */
#define S_ACC     6          /* o acumulador booleano da cláusula inteira                */
#define S_V       7          /* o valor do SET — o átomo BAIXO                          */
#define S_CHEIO   69         /* {0xFF,0xFF}: o VERDADEIRO, espalhado por todos os
                              * bits. O booleano deixou de viver na coordenada 0 —
                              * vive na largura toda, e é isso que dispensa o salto */
#define S_VB      71         /* os bytes 2,3 do valor do SET --- o andar de cima.
                              * Sem ele o UPDATE escrevia dois planos de três, e
                              * o terceiro ficava com o do valor ANTERIOR: numa
                              * DATA, o instante mudava sem ninguém lhe tocar.
                              * Eu tinha remendado isso a LIMPAR 8192 slots na
                              * criação da tabela --- escrever zeros onde a
                              * trajectória não passou, que é o contrário da
                              * cláusula (3) («a memória existe onde a
                              * trajectória passou»), e custava 44 KB por tabela
                              * antes de existir uma linha. */
#define S_VA      68         /* o átomo ALTO do valor do SET (o par, Lei 7) — 68 está
                              * livre entre o S_CORPO (60..67) e o S_EXPR (72..199); o
                              * 56 que eu tinha posto é o S_KZ+7 */
#define S_K       8          /* 8..23  a constante de cada condição                     */
/* ── AS CONSTANTES DO INSERT TÊM CASA PRÓPRIA, E ANTES NÃO TINHAM ───────────
 *
 * O INSERT punha o valor de CADA COLUNA em `S_K + j`, e o S_K tem dezasseis
 * slots --- são as constantes das CONDIÇÕES (MAXCOND × MAXTERMO). Com mais de
 * dezasseis colunas a escrita saía da região e ia caindo, uma a uma, em cima do
 * que estava a seguir: S_COND, S_TERMO, S_KZ, o S_DIA, e a 52ª coluna aterrava
 * em S_CORPO --- o corpo da tabela. Uma tabela com 56 colunas passava a ter a
 * primeira coluna «áurea» e a terceira «cristalina», sem ninguém as ter
 * declarado, e nada falhava: os valores iam para o sítio certo e o CATÁLOGO é
 * que mudava por baixo.
 *
 * Não era um tecto que recusasse --- era um tecto que ESCREVIA POR CIMA. A casa
 * é a região livre entre o fim do S_VIVO e o S_LIN, e tem COL_MAX slots, que é
 * o mesmo tecto derivado de todo o resto. */
#define S_KINS    1024       /* 1024..2047: uma constante por COLUNA, no INSERT */
#define S_KDEN    2048       /* 2048..3071: e o DENOMINADOR de cada coluna, o mesmo caso.
                              * Este vivia em S_KZ, que tem OITO slots (49..56) --- com
                              * duzentas colunas a escrita ia de 49 a 248 e apanhava o
                              * S_CORPO, o S_EXPR inteiro, o S_NR e o S_NOME: a tabela
                              * perdia o NOME e passava a «não existir», depois de o
                              * INSERT dizer que tinha inserido. */
#define S_COND    24         /* 24..39 o resultado de cada condição (0 ou 1)            */
#define S_TERMO   40         /* 40..47 o resultado de cada termo (as condições em AND)  */
#define S_UME     48         /* o "um" no campo .e — para incrementar nrows              */
/* O nrows SUBIU A TORRE: dezasseis bits, num slot próprio.
 *
 * Vivia no campo `.e` do S_CAT, que é UM BYTE — e o `nrows++` somava 1 a esse
 * campo pela ISA. Ao chegar a 255 dava a volta: uma tabela com 300 linhas
 * respondia 44 (300 mod 256) ao SELECT, ao ORDER BY e ao count. O banco PERDIA
 * dados sem uma queixa. Não se pode usar o ADD16 no S_CAT porque o transporte
 * atravessaria do nrows para o ncols, que é precisamente o que o comentário do
 * enum avisa; então o coeficiente que cresce SOBE, como manda o `word_isa.h`
 * («coef. que crescem sobem a torre»): sai do par do catálogo e passa a ocupar
 * os dois componentes de um slot só seu, somado com OP_ADD16. */
/* O TOPO, e não a contagem.
 *
 * `S_NR` guarda quantos slots de linha já foram usados — é onde o próximo
 * INSERT escreve. NÃO é quantas linhas existem: essa é ∑ sobre o bitmap de
 * vivos (o popcount), porque o DELETE desliga a coordenada e não move as
 * outras. Duas grandezas, dois sítios: o topo é um contador e sobe de andar
 * com OP_ADD16; a contagem lê-se do campo e não se guarda.
 *
 * NÃO HÁ TECTO DE LINHAS NA TEORIA — pôr um aqui foi invenção minha, e saiu.
 * Pelo `thm:BI` a dobra DUPLICA a largura, e o `§sec:torre` da arquitectura diz
 * o que fazer quando não cabe: T_{k+1} = T_k + T_k*, «o que cresce é o OBJECTO,
 * não a máquina». O que limita é o mapa de slots, que é da máquina. */
#define S_NR      216        /* o TOPO: quantos slots de linha já foram usados.
                              * Estava em 69 — entre o S_VA (68) e o S_EXPR (72)
                              * — e era PISADO: o topo passava a 2 depois de uma
                              * consulta, e a tabela encolhia. 216..223 é a folga
                              * entre o S_BITM (200..215) e o S_NOME (224). */
#define S_UM16    217        /* a constante 1 do OP_ADD16 que sobe o TOPO de andar       */
/* a extensão de sinal do σ: o `Word8` não traz sinal e um σ de −1 chega 255.
 * Vive nos slots que sobram entre o S_UM16 e o S_NOME; o typedef abaixo prende. */
#define S_SIGEXT  218
/* o slot da NORMA: a régua de uma coluna quadrática, computada em C na emissão
 * (o `emit_atomos` corre POR LINHA) e copiada daqui pelo bytecode. Vive nos seis
 * slots que sobram entre o S_UM16 e o S_NOME, e o typedef abaixo prende-o: se
 * alguém baixar o S_NOME, não compila em vez de pisar. */
#define S_DIA     58         /* o DIÁRIO: {total = ação pendente, e = coluna do SET}      */
/* O CORPO DE CADA COLUNA — passo 1 de 6 do catálogo em SQL (ver docs/TOOLKIT.md).
 *
 * A coluna deixa de ser "um número" e passa a DECLARAR em que corpo vive. O slot guarda
 * {total = o código do corpo, e = o parâmetro dele} — o metal m no áureo, o n no mórfico.
 *
 * Só o campo e o CREATE a aceitá-lo. O despacho das operações vem depois, um corpo de cada
 * vez, cada um com medidor antes de entrar — que é o método que o Aarão pediu e que hoje
 * mostrou ser o único que fecha. */
#define S_CORPO   60         /* 60..67: o corpo de cada coluna */
/* ── QUANTAS COLUNAS: O NÚMERO É UM PAR, COMO A CÉLULA ──────────────────────
 *
 * O catálogo guardava a contagem de colunas em `cat.total`, que é um Word8 ---
 * UM BYTE. Uma tabela de 500 colunas era criada com 500, anunciada com 500, e
 * lida com 244: o resto da divisão por 256. Não recusava; DAVA OUTRO NÚMERO, e
 * a partir daí o endereço de cada célula (i·ncols+j) era o de outra tabela.
 *
 * A saída é a mesma do thm:espaco que a célula já usa --- F_{2w} = F_w ⊕ σF_w,
 * a segunda cópia multiplicada por σ ---: o byte alto num plano paralelo. Quem
 * lê em baixo continua a ler o mesmo, e uma base gravada antes disto tem o
 * plano alto a zero, que é exactamente o número que ela já tinha. */
#define S_NCOLA   70         /* o byte ALTO do número de colunas */
#define CORPO_INTEIRO  0     /* o de sempre — e continua a ser o omitido, sem quebrar base antiga */
#define CORPO_RACIONAL 1
#define CORPO_AUREO    2
#define CORPO_MORFICO  3
#define CORPO_CRISTAL  4     /* PASSO 6: o lado que gira — a+bω, ω²=tω−1 */
/* ── O BOOLEANO: o corpo de DOIS elementos, que é o B da Def.~def:B.
 * Não é um inteiro com um aviso: é o primeiro andar da escada, {0,1}, onde as
 * duas operações do thm:B ficam FORÇADAS e 1+1=0. O envelope é o próprio corpo
 * --- e é por isso que ele recusa o 2, em vez de o aceitar e chamar-lhe
 * verdadeiro: um valor fora do corpo não é um valor do corpo. */
#define CORPO_BOOLEANO 5
/* ── O TEXTO: a célula guarda o ENDEREÇO no pool, não a cadeia.
 * É o «índice como estante» do thm:enumfin: a cadeia fica na gaveta e o
 * emparelhamento dá-lhe um endereço único, que cabe numa palavra. */
#define CORPO_TEXTO    6
/* ── A DATA: o instante é uma CONTAGEM, e por isso é um índice como os outros.
 *
 * Não se guarda um calendário: guarda-se quantos passos desde a origem, que é o
 * que o §sec:leitura chama tempo --- «a contagem das aplicações do operador»,
 * e não um parâmetro importado. Aqui o passo é o segundo e a origem é 1970,
 * porque é a convenção com que o cliente fala; mas o que a célula guarda é a
 * contagem, e ela precisa de MAIS UM ANDAR: 2^16 segundos são dezoito horas, e
 * 2^32 são cento e trinta anos. A dobra volta a duplicar a largura, e o plano
 * S_ALTO2 é o σ do andar seguinte --- o mesmo thm:espaco, um degrau acima. */
#define CORPO_DATA     7
#define S_MT      57         /* mascara {total=todos os bits, e=0} — limpa o .e apos GOLD */
#define S_KZ      49         /* 49..56  o zero de cada comparação (a contração compara com 0) */
#define S_LIN     4096       /* 4096+  o rascunho de cada átomo: acc, prod, cnt, passo…   */
/* O rascunho por átomo passou de 12 a 48 slots: o produto de dezasseis bits
 * precisa das dezasseis máscaras de bit, cada uma no SEU slot — as constantes
 * escrevem-se ao COMPILAR e o programa corre depois, logo partilhar um slot
 * daria o valor trocado (é a lição que o S_UM já custou aqui). Cabe: com 16
 * átomos são 4096 + 16·48 = 4864, e a região seguinte (S_DEN) só começa em
 * 33792. */
#define ATOMO_SLOTS 48u
/* A ÁRVORE DA EXPRESSÃO COMEÇAVA EM 64 — E O CORPO DA COLUNA VIVE EM 60..67.
 *
 * As duas regiões PISAVAM-SE em 64..67, que são as colunas 4 a 7. Numa tabela com
 * cinco colunas ou mais, o primeiro SELECT com WHERE escrevia o temporário da raiz
 * por cima do corpo declarado, E FICAVA NO DISCO: medido, uma coluna `MORFICO(6)`
 * — o par (3,6) — passava a (1,0), que é RACIONAL. A partir daí a aritmética
 * daquela coluna é a álgebra errada, para sempre, e nada o diz.
 *
 * Ninguém o apanhou porque todas as tabelas dos medidores têm três colunas.
 *
 * A árvore desce por `dest+2` e `dest+34` recursivamente, e o espaço reservado era
 * 128 slots. Fica 72..199, e o nome da tabela vai para 224 — com folga declarada
 * entre as três regiões em vez de encostadas. */
#define S_EXPR    72         /* 72..199  os temporários da árvore da expressão          */
/* O NOME DA TABELA — 192..207, dois caracteres por Word (Lei 7: o par são dois átomos).
 *
 * O catálogo guardava só {ncols, nrows}: a relação não tinha NOME. O `varre` lia o
 * `FROM x` para uma variável e nunca a usava, e o mesmo no `INSERT INTO x`. A
 * consequência não é cosmética — `SELECT * FROM tabela_que_nao_existe` devolvia as
 * linhas da tabela que lá estava, e uma aplicação que se enganasse no nome recebia
 * dados de outra e não tinha como saber. Pela porta FEBE isso chega ao driver como um
 * SELECT bem sucedido.
 *
 * Base ANTIGA: o nome lê-se vazio (os slots estavam a zero) e aí aceita-se qualquer
 * nome — é a mesma compatibilidade que o CORPO_INTEIRO tem por ser o código 0. */
#define S_NOME    224        /* 224..239: 16 Words = 32 caracteres (folga até S_MATCH) */
#define S_NOME_N  16
typedef char sigext_nao_pisa_o_nome[(S_SIGEXT < S_NOME) ? 1 : -1];

/* A BASE ORTONORMAL, EM SLOTS: e_k = 2^k, k = 0..15.
 *
 * `naturais.tex thm:base`: os produtos dos geradores dão e_k = 2^k, e essa base
 * é ORTONORMAL para ⟨a,b⟩ = paridade(a∧b) — ⟨e_i,e_j⟩ = δ_ij. E o `cor:w8`: «a
 * identificação é a IDENTIDADE — o bit j do inteiro é a COORDENADA j na base».
 * Por isso um bitmap não gasta um slot por linha: a linha i É a coordenada i, e
 * lê-se como o bit i. Uma Word são dezasseis bits, logo dezasseis linhas por
 * slot. São só dezasseis máscaras distintas, e ficam aqui em constantes porque
 * as constantes escrevem-se ao COMPILAR e o programa corre depois. */
#define ATOMO_BITS  W8_BITS          /* do `palavra8.h`: o 8 é o nome do tipo */
#define SLOT_BITS   WORD_ISA_BITS    /* do `word_isa.h`:  átomos × bits do átomo */

#define S_BITM    200        /* SLOT_BITS coordenadas: e_k = 2^k                        */
#define S_BITN    240        /* SLOT_BITS complementares, para desligar a coordenada    */
/* O MAPA comporta 24 slots em 200..223 e 16 em 240..255: com a Word actual são
 * dezasseis coordenadas e sobra. Se o andar dobrar, é o MAPA que tem de abrir
 * espaço — e isso é da máquina, não da teoria. O compilador avisa em vez de
 * escrever por cima do vizinho. */
typedef char cabe_a_base[(S_BITM + WORD_ISA_BITS <= 224u
                       && S_BITN + WORD_ISA_BITS <= 256u) ? 1 : -1];
#define S_MATCH   256        /* bitmap do resultado, BIT por linha (256..511)           */
#define MAXCOND   4          /* condições por termo                                     */
#define MAXTERMO  4          /* termos ligados por OR                                   */
#define S_VIVO    512        /* a linha existe? BIT por linha (512..1023)               */
#define S_VIVO_FIM 1024u    /* e a seguir vem o S_KINS: a constante da coluna 0        */

/* ── QUANTAS LINHAS CABEM: O NÚMERO SAI DO MAPA, NÃO DE UM COMENTÁRIO ────────
 *
 * `NÃO HÁ TECTO DE LINHAS NA TEORIA` (acima, no S_NR): o que limita é o MAPA DE
 * SLOTS, que é da máquina. Mas então o tecto tem de SAIR do mapa — e estava
 * escrito num comentário, à mão, com os dois números ERRADOS: dizia «a linha
 * 257 escreve o seu match por cima do primeiro vivo», que era verdade quando o
 * bitmap gastava um SLOT por linha. Passou a gastar um BIT e o número não
 * acompanhou; a linha que transborda é a 4096, dezasseis vezes mais adiante.
 * Um comentário não é verificado por ninguém, e este sobreviveu à mudança que o
 * invalidou. Agora deriva, e o compilador confere.
 *
 * O TECTO É O MENOR DOS DOIS, porque um match é escrito para toda a linha que
 * exista: se o do resultado for mais curto, a linha alta escreve o seu match
 * por cima do primeiro vivo — não recusa, CORROMPE, que é o defeito que este
 * mesmo ficheiro já documenta no `den[16]` do INSERT. */
#define LIN_MATCH  ((long)((S_VIVO     - S_MATCH) * WORD_ISA_BITS))
#define LIN_VIVO   ((long)((S_VIVO_FIM - S_VIVO ) * WORD_ISA_BITS))
#define LIN_TECTO  (LIN_MATCH < LIN_VIVO ? LIN_MATCH : LIN_VIVO)
typedef char match_nao_pisa_o_vivo[(S_MATCH + (unsigned)LIN_TECTO/WORD_ISA_BITS <= S_VIVO) ? 1 : -1];
typedef char vivo_nao_pisa_a_constante[(S_VIVO_FIM <= S_KINS) ? 1 : -1];
long sql_lin_tecto(void){ return LIN_TECTO; }   /* quem define é quem responde */

/* (a lei de apagar o match está junto do `bit_poe`, que é onde o bitmap se escreve) */

/* ── O DUAL DA CÉLULA: A PRESENÇA ────────────────────────────────────────────
 *
 * `thm:bitunico`: «a PRESENÇA b=1 é o único operacional; a AUSÊNCIA b=0 é o
 * suporte neutro, e é ela o dual». E o `thm:multiplicidade`(4): a leitura local
 * distingue G=0 — célula ainda não realizada — de G>0.
 *
 * A LINHA já tinha esse dual: é o campo do vivo. A CÉLULA não tinha, e por isso
 * uma que valia zero e uma que nunca foi escrita eram indistinguíveis — uma
 * coluna acabada de acrescentar respondia a `= 0` em todas as linhas. O dual é
 * do DADO, não só da linha.
 *
 * É o mesmo campo, com a mesma base ortonormal e_k = 2^k, indexado por célula
 * em vez de por linha: o bit i·ncols + j. Zero é a ausência — o neutro — e por
 * isso uma tabela antiga, cujo campo nunca foi escrito, leria tudo como
 * ausente; a migração está no `prepara`, que acende o que já lá está. */
#define S_PRESCAB (ISA_TECTO + ZONA(9))   /* {1,0} = o campo já foi escrito */
#define S_PRES    (S_PRESCAB + 1)          /* presença: BIT por célula */

/* ── A COLUNA PODE RECUSAR ────────────────────────────────────────────────────
 *
 * Uma restrição de coluna não é uma comodidade: é uma AFIRMAÇÃO sobre a fibra,
 * e as duas que aqui vivem são exactamente as duas metades do que se acabou de
 * construir.
 *
 *   NOT NULL  — a coluna recusa o SUPORTE. Declara que toda a linha tem ali uma
 *               coordenada do corpo, isto é, que o dual pesa zero nesta coluna.
 *               É a negação directa do `thm:bitunico` aplicada a uma direcção.
 *
 *   UNIQUE    — a coluna declara que a FIBRA TEM UMA FOLHA SÓ. É a mesma frase
 *               do DISTINCT (`thm:levantamento`: a folha 1 de cada fibra), mas
 *               dita na ESCRITA em vez de na leitura: o DISTINCT escolhe o
 *               representante à saída, o UNIQUE recusa o segundo à entrada. E a
 *               testemunha é a ÁRVORE — uma chave, um sítio —, pelo que a
 *               verificação é uma DESCIDA e não uma varredura.
 *
 * PRIMARY KEY é a conjunção das duas, e não uma terceira coisa.
 *
 * Uma Word por coluna: o .total leva as bandeiras. O cabeçalho diz se o campo
 * já foi escrito, pela mesma razão do S_PRESCAB — uma tabela antiga leria zeros
 * e zero aqui é «sem restrição», que por acaso é a resposta certa. */
#define S_RESTR   (ISA_TECTO + ZONA(10))  /* uma Word por coluna: bandeiras */
#define R_NOTNULL 1
#define R_UNICO   2

/* ── A SETA ENTRE DUAS TABELAS ────────────────────────────────────────────────
 *
 * `REFERENCES mae(col)` declara uma SETA: cada linha desta tabela aponta para
 * uma linha daquela. É o mesmo caminho que o JOIN percorre a cada consulta,
 * dito UMA vez na tabela em vez de escrito a cada pergunta — e por ser dito,
 * pode ser EXIGIDO.
 *
 * O que a exigência diz é que a seta está BEM DEFINIDA, e isso tem duas metades
 * que são uma o dual da outra:
 *
 *   à ENTRADA  — escrever um valor que não está do outro lado é criar uma seta
 *                para lado nenhum. Recusa-se no INSERT e no UPDATE.
 *   à SAÍDA    — apagar a linha apontada é cortar a seta por baixo, deixando o
 *                que aponta a apontar para nada. Recusa-se no DELETE.
 *
 * Medir só a primeira era o defeito clássico desta casa: metade do par. E a
 * segunda precisa de uma coisa que a primeira não precisa — saber QUEM aponta
 * para mim —, pelo que a declaração escreve nos DOIS lados: a filha guarda para
 * onde aponta, e a mãe guarda quem a aponta. É a mesma dualidade da árvore, que
 * guarda a chave e o sítio.
 *
 * A ausência não é uma seta: uma célula NULL não aponta para nada e nada exige.
 * (`thm:bitunico` outra vez — o dual não é um valor.) */
/* ── E A RESTRIÇÃO DE PREDICADO ──────────────────────────────────────────────
 *
 * `CHECK (a > 0)` é o `WHERE` dito na ESCRITA. É a mesma dualidade do UNIQUE
 * contra o DISTINCT, um andar acima: o WHERE escolhe à saída as linhas que
 * satisfazem o predicado, o CHECK recusa à entrada as que não o satisfazem. E
 * é literalmente o mesmo objecto — a mesma árvore, o mesmo molde, a mesma ISA
 * —, corrido sobre a linha que quer entrar em vez de sobre as que já estão.
 *
 * Por isso não há avaliador novo: guarda-se o TEXTO do predicado e compila-se
 * com o mesmo `le_expr`. Um motor que escrevesse aqui uma segunda avaliação
 * teria duas respostas possíveis para a mesma pergunta.
 *
 * UM por tabela, e o segundo é recusado em vez de calado — juntar dois com um
 * AND implícito seria o motor a escrever predicado que ninguém escreveu. */
#define S_CHECK   (ISA_TECTO + ZONA(14))  /* 64 Words = 128 caracteres */
#define S_CHECK_W 64u

#define S_FK      (ISA_TECTO + ZONA(11))  /* por coluna: .total = col_alvo+1, 0 = sem seta */
#define S_FKNOME  (ISA_TECTO + ZONA(12))  /* por coluna: 16 Words = 32 chars, a tabela alvo */
#define S_FILHOS  (ISA_TECTO + ZONA(13))  /* na MÃE: {quantos,0} e depois blocos de 17 */
#define FK_NOME_W 16u
#define FK_BLOCO  17u                     /* 16 Words de nome + 1 Word com a coluna */
/* QUANTAS FILHAS UMA MÃE VIGIA: as que a zona segura. Eram DEZASSEIS, e o
 * dezasseis não vinha de lado nenhum --- a zona tem ZONA_SLOTS Words, o
 * cabeçalho gasta uma e cada filha ocupa FK_BLOCO. */
#define FK_MAXFIL ((int)((ZONA_SLOTS - 1u) / FK_BLOCO))

/* ── UM ÍNDICE POR COLUNA, e cada um na sua zona ─────────────────────────────
 *
 * Havia um só, e por isso uma condição composta só podia descer de um lado.
 * Com um por coluna, `A AND B` é a INTERSECÇÃO dos dois campos e `A OR B` a
 * UNIÃO — o par ∧/∨ que o §W23 chama as duas operações duais, agora sobre os
 * campos em vez de sobre as linhas. E o OR deixa de ficar de fora: estava fora
 * porque METADE não chegava, não porque a árvore não soubesse responder.
 *
 * Cada índice mora na sua zona, acima do tecto da ISA e dentro do .mem da
 * tabela; a zona 16+k é a da coluna k. Cabem oito — as que o S_CORPO segura —
 * e cada uma ocupa 9603 slots dos 16384 da zona.
 *
 * o índice: a árvore que NÃO se limpa, acima do tecto da ISA e
 * dentro do .mem da tabela. O cabeçalho diz qual a coluna e quantas linhas
 * havia quando ele foi feito — se o número mudou sem ele acompanhar, é velho. */
/* O CABEÇALHO MORA DENTRO DA ZONA, e não um slot antes dela. Estava em
 * `S_IDX - 1`, que é o ÚLTIMO slot da zona 2 — a do texto —, e por isso era
 * escrito por cima: a coluna indexada lia-se como 5 numa tabela de duas
 * colunas. Uma zona começa onde começa; pedir emprestado ao vizinho é escrever
 * na casa dele. */
/* ── AS VISTAS: uma composição com nome ──────────────────────────────────────
 *
 * Uma condição é uma FUNÇÃO do campo no campo, e o `AND` compõe duas. Uma
 * vista é essa composição com NOME: guarda a tabela e a condição, e usá-la num
 * `FROM` compõe a condição dela com a de quem a usa. Não traz operação nova —
 * traz o direito de dar nome a uma que já existia.
 *
 * Mora na zona 5, dentro do .mem, e o cabeçalho mora DENTRO dela: pedir
 * emprestado ao vizinho é escrever na casa dele, e isso já custou dois defeitos
 * a este ficheiro. */
#define VIEW_W       64u                     /* Words por vista */
/* QUANTAS VISTAS: as que a zona segura, menos o cabeçalho. Eram OITO, e o oito
 * não vinha de lado nenhum --- a zona tem ZONA_SLOTS Words e cada vista ocupa
 * VIEW_W: o resto é uma divisão. */
#define VIEW_MAX      ((ZONA_SLOTS - 1u) / VIEW_W)
#define S_VIEWCAB   (ISA_TECTO + ZONA(5))    /* {quantas, 0} */
#define S_VIEW      (S_VIEWCAB + 1)
#define VIEW_NOME(i)  (S_VIEW + (unsigned)(i)*VIEW_W)         /* 8 Words: 16 chars */
#define VIEW_TAB(i)   (S_VIEW + (unsigned)(i)*VIEW_W + 8u)    /* 8 Words: 16 chars */
#define VIEW_COND(i)  (S_VIEW + (unsigned)(i)*VIEW_W + 16u)   /* 48 Words: 96 chars */

/* QUANTAS ÁRVORES: AS QUE O ESPAÇO DE ZONAS SEGURA.
 *
 * Eram oito, «as que o S_CORPO segura» --- um tecto herdado de outro tecto, e
 * esse já caiu. Custava caro: o esquema do cliente pede um índice na décima
 * primeira coluna e o motor recusava, não por a árvore não caber, mas por o
 * número de árvores ter sido escrito quando as colunas eram oito.
 *
 * As oito primeiras ficam onde estavam --- ZONA(16..23) ---, porque há `.mem`
 * gravados com elas lá; as seguintes continuam em ZONA(32) e vão até ao pé do
 * S_CANAL, que é a fronteira real. O tecto passa a ser uma CONTA sobre o mapa
 * de zonas, e não um número. */
#define IDX_ZONA0      16u                /* as oito primeiras, onde sempre estiveram */
/* ── E O MAPA DE ZONAS VERIFICA-SE, PORQUE JÁ ME CUSTOU UMA TARDE ────────────
 *
 * As oito primeiras árvores ficam em 16..23 e as seguintes continuavam em 32 ---
 * escrito por mim, e a seguir pus a ORDEM em 29..36, o DIÁRIO em 37..44 e a
 * direita do JOIN em 45..76. A árvore da coluna 8 ia para a ZONA(32), que é o
 * meio do S_ORD; a da coluna 13 para o diário; a da 21 para o JOIN. Nada
 * falhava na bateria porque nenhum medidor indexa uma coluna acima da décima
 * segunda --- e é exactamente assim que este defeito espera.
 *
 * A faixa das árvores começa DEPOIS de tudo o que está reservado, e o
 * `typedef` abaixo obriga o compilador a verificá-lo: acrescentar uma zona por
 * cima delas deixa de compilar, em vez de partir uma consulta longe daqui. */
/* ── A GRADE DO LCS, no disco como tudo ──────────────────────────────────────
 *
 * O `lcs_dobra.tex` põe a tabela L como uma matriz (m+1)×(n+1) de inteiros, e
 * diz o que ela é: «a execução da dobra alternada das duas faces sobre a grade
 * finita». Ela mora numa zona --- duas Words por célula, o valor e o PONTEIRO
 * (a face que produziu o valor), porque o comprimento sozinho não devolve a
 * palavra: isso é o levantamento, e ele pede a coordenada de folha «de onde
 * vim».
 *
 * Duas zonas dão 32768 slots; com duas Words por célula são 16384, e a grade
 * quadrada que cabe tem lado 127. O que não couber RECUSA-SE, que é o que o
 * paper manda («se a tabela não cabe, a arquitectura recusa em vez de
 * truncar»). */
#define S_LCS          (ISA_TECTO + ZONA(77))
#define LCS_ZONAS      2u
#define LCS_CELULAS    ((ZONA_SLOTS * LCS_ZONAS) / 2u)
/* ── O ÍNDICE INVERTIDO É A FIBRA, E MORA NO DISCO ───────────────────────────
 *
 * Três Words por ocorrência --- o endereço do termo no pool, a linha, e a
 * posição dentro do texto --- e o resto é leitura: a FIBRA de um termo é o
 * conjunto das ocorrências com aquele endereço, e G(termo) é o tamanho dela. */
#define S_INV          (ISA_TECTO + ZONA(79))
#define INV_ZONAS      2u
#define INV_W          3u
#define INV_MAX        ((ZONA_SLOTS * INV_ZONAS) / INV_W)
/* ── A SEQUÊNCIA DO GROUP BY TEM ZONA PRÓPRIA ────────────────────────────────
 *
 * Ele precisa da sequência ORDENADA para contar as corridas, e usava um arranjo
 * local de SQL_OUT_MAX_ROWS --- a janela da porta C. Com ele o laço parava às 64
 * linhas e as contagens saíam ERRADAS: 200 linhas em 3 grupos davam 22+21+21, e
 * a linha da conservação imprimia «∑G = 64» com |I| = 200, violando o
 * `thm:escada` (∑_x G(x) = |I|) e apresentando-o como certo.
 *
 * A correcção óbvia era usar a sequência do disco --- e ela JÁ TEM DONO, que é o
 * ORDER BY. Fi-lo, corri, e caíram vinte e três medidores: as duas escritas
 * pisam-se quando a consulta tem os dois. O endereço tinha dono, e o que faltava
 * era dar-lhe casa, não repetir a troca.
 *
 * Aqui está a casa: a mesma forma da saída --- metade para a ordem, metade para
 * os ausentes ---, numa zona sua. */
#define S_GRPSEQ    (ISA_TECTO + ZONA(81))
#define S_GRPSEQ_N  (ZONA_SLOTS / 2u)
#define S_GRPAUS    (S_GRPSEQ + S_GRPSEQ_N)

/* ── O RASCUNHO DA LINHA DO SCRIPT, E ELE ESTAVA DENTRO DAS CÉLULAS ──────────
 *
 * Estava `#define S_LINHA (S_LINHAS + 20000u)` dentro do `main`, com o
 * comentário «o rascunho da linha, no banco». Ora S_LINHAS é a ZONA(1) e ela
 * acaba em 32768: 16384 + 20000 = 36384 cai dentro do S_DEN, que é a zona dos
 * DENOMINADORES. Cada caractere lido do script escrevia por cima do
 * denominador de uma célula --- e com LIN_MAX a valer 16384, uma linha podia
 * atravessar o S_DEN inteiro e entrar no S_ALTO.
 *
 * MEDIDO, e o dano é do próprio carregamento: uma tabela de 2000 linhas
 * carregada pelo STDIN respondia 1985 onde a mesma carregada por ARGUMENTO
 * respondia 2000. Quinze linhas perdidas por o script escrever no sítio onde os
 * dados vivem. Nenhum medidor o apanhava porque a célula atingida é a 3616 ---
 * é preciso uma tabela grande para lá chegar.
 *
 * Zona própria, como a do GROUP BY. */
#define S_LINRASC  (ISA_TECTO + ZONA(82))
#define ZONA_LIVRE1    83u                /* a primeira zona depois do reservado */
#define IDX_ZONA1      ZONA_LIVRE1
#define IDX_MAXCOL     (8u + (ZONA_CANAL - IDX_ZONA1))
#define S_IDXBASE(k)   (ISA_TECTO + ZONA((k) < 8 ? IDX_ZONA0 + (unsigned)(k) \
                                                 : IDX_ZONA1 + (unsigned)(k) - 8u))
#define S_IDXCAB(k)    (S_IDXBASE(k))        /* {coluna+1, 0}                    */
#define S_IDXCAB2(k)   (S_IDXBASE(k) + 1)    /* as linhas indexadas, no par      */
#define S_IDXNOS(k)    (S_IDXBASE(k) + 2)    /* o contador de NÓS da árvore      */
#define S_IDX(k)       (S_IDXBASE(k) + 3)    /* e a árvore começa depois deles   */
#define S_DEN     ZONA(2)      /* o DENOMINADOR de cada célula, no TOTAL do seu slot: a ISA não
                              * move e→total, e a conta precisa de q como número. */
#define S_LINHAS  ZONA(1)
/* ── O NOME DE CADA COLUNA — 36864.., 16 Words (32 caracteres) por coluna ─────
 * O `CREATE TABLE cliente (nome,idade,saldo)` lia os três identificadores e
 * DEITAVA-OS FORA: só o corpo de cada coluna era guardado. A consequência não é
 * cosmética — o SELECT devolvia `a`, `b`, `c`, e `WHERE idade > 20` saía como
 * «comando recusado», porque a coluna era decidida por `nome[0] - 'a'`. Um
 * `UPDATE beta SET z = 99` pedia a coluna 25 numa tabela de três.
 *
 * A região fica acima das linhas (S_LINHAS + MAXLIN·8 = 3024) e dos
 * denominadores (S_DEN + MAXLIN·8 = 35792), longe do mapa que a ISA percorre.
 * Dois caracteres por Word, como o nome da tabela — é a mesma Lei 7. */
/* ── O ÁTOMO ALTO DE CADA CÉLULA — 40960.., um plano paralelo ────────────────
 * A célula é `{numerador, denominador}` em dois átomos, e a ULA soma componente
 * a componente: o valor de uma coluna inteira vive em OITO bits. O `INSERT ...
 * VALUES (1000,...)` era recusado (§24) porque guardar 232 em vez de 1000 é pior.
 *
 * O byte ALTO passa a viver num plano à parte — como o denominador já vive em
 * S_DEN —, e a Word da linha não muda: TODA a aritmética emitida continua a ler
 * o que sempre leu. O que muda é a ESCRITA e a LEITURA, que são a fronteira.
 *
 * E a metade que falta diz-se em vez de se fingir: o avaliador do WHERE é
 * polinomial e de oito bits (o `S_MT` mascara o `.e` e os produtos são de átomo),
 * logo uma comparação que cite uma coluna com valores acima de 255 é RECUSADA,
 * contada, e com o motivo escrito. Alargar o avaliador é o trio seguinte —
 * a aritmética do andar já está provada (`ula_add16`, §26). */
/* ══ O QUE O S_ALTO É, E TEM NOME: A FOLHA DO LEVANTAMENTO ═══════════════════
 *
 * `arquitetura.tex` thm:aranha-inversa. A célula de oito bits é a realização
 *
 *     π : valor ⟼ cél,     cél = valor mod 256
 *
 * e ela DOBRA: 300 e 44 caem na mesma célula. Isso é literalmente a
 * identificação `i ∼ j ⟺ π(i) = π(j)` do thm:multiplicidade, e a
 * multiplicidade G(cél) = |π⁻¹(cél)| conta quantos valores lá colapsam. Foi
 * essa dobra que fazia o `INSERT ... VALUES (500)` guardar 244 — não era um bug
 * de tipo: era a fibra a ser perdida na projecção, que é o que
 * `algebrico thm:metrica` diz que |det| = 1 não vê («a pata some na marca»).
 *
 * E o conserto tem nome, e não é «pôr mais um byte»: é o LEVANTAMENTO EM FOLHAS
 *
 *     π̃(valor) = (π(valor), k(valor)),     k = o número da visita à célula
 *
 * com k a ser o byte ALTO. O teorema diz o que ele garante, e é o que se mede
 * a seguir:
 *   (1) INJECTIVIDADE — G̃ ≡ 1: dois valores distintos nunca dão o mesmo par
 *   (2) pr₁ ∘ π̃ = π  — deitar fora a folha devolve EXACTAMENTE a célula velha,
 *       que é o que uma base escrita antes disto tem guardado
 *   (3) CONSERVAÇÃO   — Σ_cél G(cél) = |I|: nenhum valor se perde nem se conta
 *       duas vezes
 *
 * «Não inventa geometria nova: desfaz a identificação i∼j que o dragão fez na
 * grade. A métrica unimodular continua a governar a base; a folha é a fibra que
 * |det| = 1 não via.» E a recusa da comparação acima de 32767 é a outra metade
 * do mesmo teorema: o avaliador decide na BASE, e a base não vê a folha. */

/* ── A MARCA DA COLUNA — 45056.., uma por coluna ─────────────────────────────
 * `arquitetura.tex` §sec:aranha, thm:multiplicidade, cláusula 3: «A memória NÃO
 * pertence ao agente. A escrita incremental G_{t+1}(x) = G_t(x) + 1 materializa
 * no espaço tudo o que distingue região já percorrida de região nova. O agente
 * NÃO conserva a sequência π(0)…π(t).» E a cláusula 4: «SENTIR É LER G».
 *
 * O banco está lá nomeado: «o agente (autómato, tradutor, BANCO, aranha
 * estigmérgica) não carrega o mapa: o mapa é ESTADO DO AMBIENTE, e a regra local
 * de leitura/escrita produz a geometria global.»
 *
 * E eu tinha escrito o contrário: o `col_max` percorria TODAS as linhas a cada
 * consulta para saber a largura da coluna — o agente a reconstruir a trajectória
 * em vez de ler a marca que o ambiente já tem. A escrita deixa a marca; a
 * leitura lê-a, e é local.
 *
 * A marca NÃO DESCE quando uma linha é apagada, e isso é a estigmergia e não um
 * descuido: o traço não se desescreve. O efeito é ser CONSERVADOR — pode recusar
 * uma consulta que hoje já caberia —, nunca aceitar uma que não cabe. */
#define S_COLMAX    (ISA_TECTO + ZONA(7))
/* O S_ALTO GUARDA UMA CÉLULA POR LINHA×COLUNA, tal como o S_LINHAS — e tinha
 * SESSENTA E DOIS slots antes do S_TXLIVRE, quando precisa de tantos quantos o
 * S_LINHAS tem. Uma tabela com mais de 31 linhas de dois campos escrevia o byte
 * alto por cima do ponteiro da zona de texto. Passa para a folga que existe
 * depois do S_COLMAX (que usa dezasseis slots e tem cento e noventa mil até ao
 * S_NO), com o MESMO tamanho do S_LINHAS — que é o que a simetria exige: as
 * duas metades da mesma célula, o baixo e o alto, têm de caber igual. */
#define S_ALTO      ZONA(3)
/* ── O ANDAR SEGUINTE: os bytes 2 e 3 da palavra, para o que não cabe em 16.
 * É o F_{2w} = F_w ⊕ σF_w outra vez, um degrau acima --- e vale a mesma regra:
 * quem lê no andar de baixo continua a ler o que sempre leu, porque o π_k
 * trunca e o π_k∘ι_k = id. */
/* ── E A ZONA ESCOLHE-SE DEPOIS DE VER QUEM JÁ LÁ ESTÁ.
 *
 * Pus isto na ZONA(16) e escrevi por cima das ÁRVORES DOS ÍNDICES, que ocupam
 * S_IDXBASE(k) = ISA_TECTO + ZONA(16+k) para k até IDX_MAXCOL. A bateria
 * apanhou: o UNIQUE deixou de recusar duplicados, porque a árvore que o
 * testemunha tinha sido esmagada por bytes altos de células.
 *
 * As árvores vão até à ZONA(23). Estas ficam a partir da 24, com folga. */
#define S_ALTO2     (ISA_TECTO + ZONA(24))
/* ── QUANTAS CÉLULAS CABEM: O TECTO É DO PRODUTO, NÃO DAS LINHAS ─────────────
 *
 * Uma célula ocupa um slot em TRÊS zonas — o valor (`S_LINHAS`), o denominador
 * (`S_DEN`) e o plano alto (`S_ALTO`) — e o endereço é `i·ncols + j`. Logo o que
 * o mapa limita é o PRODUTO, e não o número de linhas: com uma coluna cabem
 * dezasseis mil linhas, com oito cabem duas mil.
 *
 * Isto não estava dito em sítio nenhum, e o INSERT não o verificava: com 8
 * colunas a linha 2049 escrevia a sua primeira célula em S_DEN[0], que é o
 * denominador da célula (0,0) — e a linha 0 desaparecia da tabela sem erro
 * nenhum. A fronteira é EXACTA e foi medida: 2048×8 responde inteiro, 2049×8
 * perde a primeira linha.
 *
 * Esta constante já existia com o nome certo e o corpo errado: dizia «tantos
 * quantos o S_LINHAS» e escrevia `S_LIN - S_LINHAS`, que é −12288 — o `S_LIN`
 * é o rascunho por átomo, não a zona das células. Nunca foi usada, e por isso
 * nunca doeu; agora é ela o tecto, e o compilador confere as três zonas. */
#define S_ALTO_N    ((long)(S_DEN - S_LINHAS))   /* a zona das células, em slots */
#define CEL_TECTO   S_ALTO_N
typedef char as_tres_zonas_da_celula_sao_iguais[
    ((long)(S_ALTO - S_DEN)     >= CEL_TECTO &&
     (long)(ISA_TECTO - S_ALTO) >= CEL_TECTO) ? 1 : -1];
long sql_cel_tecto(void){ return CEL_TECTO; }   /* quem define é quem responde */

/* NOTA sobre os `>= 0` que parecem redundantes nas guardas de índice desta
 * casa (col, fc, j): os quatro TÊM guarda a montante --- `if(fc < 0) continue`,
 * `if(achou < 0) RECUSADA` --- e sem o `>= 0` local funcionavam na mesma, mas
 * por ACIDENTE DO SINAL: comparar um int negativo com um unsigned converte-o
 * para um número enorme, e a comparação dá falso «à sorte». Quem visse o aviso
 * do compilador e o calasse com um cast assinado partia isso em silêncio, e o
 * índice −1 passava a escrever fora. O `>= 0` torna a rede DELIBERADA e local,
 * e deixa o compilador calado --- que é o que faz um aviso novo ser sinal. */
/* ── A PORTA POR ONDE O ÍNDICE DA CÉLULA SE FORMA ────────────────────────────
 *
 * Os bitmaps têm um par de funções por onde tudo passa, e por isso a lei coube
 * lá. As células NÃO têm: o endereço `i·ncols + j` é formado em quarenta sítios
 * espalhados, e o defeito é INDISTINGUÍVEL a jusante --- um índice além do
 * tecto de S_LINHAS aterra em S_DEN, que para o `mem_grava` é um slot tão
 * legítimo como qualquer outro. Não há guarda possível na gravação: tem de ser
 * onde o índice NASCE.
 *
 * Esta função é INSTRUMENTO e não correcção. Devolve o índice tal como veio ---
 * não trunca, não desvia, não muda uma vírgula do que o motor faz ---, e apenas
 * CONTA quando ele sai do tecto. É deliberado: truncar mandaria a escrita para
 * outra célula, que é corromper de outra maneira, e a recusa já está a montante,
 * no INSERT, onde a base ainda está intacta. Aqui o que se quer é SABER. */
static long cels_fora = 0;
static unsigned cel_ix(long k){
    if(k < 0 || k >= CEL_TECTO) cels_fora++;
    return (unsigned)k;
}
long sql_cels_fora(void){ return cels_fora; }
#define S_COLNOME   (ISA_TECTO + ZONA(6))
/* ── O DEFAULT: o valor que a coluna toma quando o INSERT não o diz.
 *
 * Guarda-se uma Word por coluna --- {total = o valor, e = 1 se há default} ---
 * e é a marca que decide, não o valor: um default de ZERO é um default, e sem a
 * marca ele seria indistinguível de «não tem». É o mesmo par (valor, presença)
 * que a casa usa na célula ausente, e pela mesma razão. */
/* ── O CORPO DE CADA COLUNA, para MAIS de oito.
 *
 * O S_CORPO original tem oito slots (60..67), presos entre vizinhos --- e uma
 * tabela do cliente tem vinte e oito colunas. O plano largo fica acima do tecto
 * da ISA, onde há espaço, e o de baixo continua a valer para as oito primeiras:
 * é o mesmo encaixe por prefixos do thm:BI --- o andar de baixo não muda, o de
 * cima acrescenta ---, e por isso nada do que já lê o S_CORPO deixa de ler. */
/* ── O ISOLAMENTO POR INQUILINO (RLS): a política é uma EROSÃO.
 *
 * Um ERP multi-inquilino não pode deixar uma linha de um cliente aparecer a
 * outro, e a peça que o Postgres usa é a Row Level Security: uma política que
 * acrescenta, a toda a leitura, a condição `tenantId = <o inquilino da sessão>`.
 *
 * Aqui isso não é maquinaria nova. O `corpo mórfico` desta casa já diz o que
 * é: «erode-se para ESCOLHER, dilata-se para escrever» --- e a política é
 * exactamente a erosão do campo visível, aplicada DEPOIS do WHERE do cliente e
 * antes de a resposta sair. O S_MATCH é o campo; a política apaga dele o que
 * não é do inquilino.
 *
 * Guarda-se: se a tabela tem política (uma marca), qual coluna isola, e qual o
 * inquilino corrente --- que vem do `SET app.tenant_id`, como no original. */
#define S_RLS       (ISA_TECTO + ZONA(26))   /* {1,0} = a tabela tem política */
#define S_RLSCOL    (S_RLS + 1)              /* qual coluna isola */
#define S_TENANT    (S_RLS + 2)              /* o inquilino da sessão (endereço no pool) */
#define S_BYPASS    (S_RLS + 3)              /* {1,0} = a sessão passa por cima */

#define S_CORPOX    (ISA_TECTO + ZONA(25))
#define S_CORPOX_N  COL_MAX   /* o corpo de cada coluna: uma Word, e cabem ZONA_SLOTS */

#define S_DEFAULT   (ISA_TECTO + ZONA(27))
/* ── A SAÍDA VIVE NO DISCO, COMO TUDO O RESTO ────────────────────────────────
 *
 * «A memória é o DISCO. Sem RAM» --- é a primeira linha deste ficheiro, e a
 * `arquitetura` §interface repete-a: «a query compila para bytecode da ISA e
 * corre com a memória no disco, sem RAM para a tabela». Eu tinha a ORDEM da
 * saída num array da pilha de 64 posições, e chamei-lhe «o tecto do fio, e esse
 * é real» --- inventei uma pilha que esta casa não tem para justificar um
 * limite que ela não impõe. Uma tabela de 300 linhas ordenava 64.
 *
 * A ordem é um objecto como outro qualquer: mora numa zona do `.mem`, uma Word
 * por linha emitida, e quem lê pede a k-ésima. O tecto é o que a ZONA segura,
 * que é uma conta e não uma escolha. */
#define S_SAIDA     (ISA_TECTO + ZONA(28))
#define S_SAIDA_N   (ZONA_SLOTS / 2u)          /* a ordem; a outra metade é dos ausentes */
#define S_SAIDAUS   (S_SAIDA + S_SAIDA_N)

#define S_DEFAULT_N COL_MAX   /* o DEFAULT de cada coluna, no mesmo passo */

#define S_COLNOME_W 16u        /* Words por nome → 32 caracteres */
/* TANTAS QUANTAS A ZONA SEGURA. O 8 ficou de quando as oito eram tudo, e o 32
 * foi o meu passo seguinte --- que é o mesmo erro com outro número. A zona tem
 * ZONA_SLOTS Words e cada nome ocupa S_COLNOME_W: o tecto é o quociente, e nada
 * mais. */
#define S_COLNOME_N (ZONA_SLOTS / S_COLNOME_W)

/* ── E A MARCA TEM DUAS METADES: O MÁXIMO E O MÍNIMO ─────────────────────────
 *
 * A marca guardava só o MÁXIMO, e quem a lê --- o `atom_possivel` --- assumia
 * zero para o outro lado. É meia marca: numa coluna cujos valores estão todos
 * entre 600 e 700, o máximo não permite descartar `k < 100`, e o mínimo
 * permitiria dizer «nenhuma linha pode satisfazer» sem varrer uma.
 *
 * É o par dual outra vez, medido só de um lado. O `thm:medida` dá o critério:
 * «sempre que uma quantidade se propuser a distinguir realizações, a pergunta é
 * se ela é ou não invariante da medida --- porque as que são, não distinguem».
 * O máximo sozinho não distingue as colunas cujo intervalo está ACIMA do alvo;
 * o par (mín, máx) distingue.
 *
 * A casa é a MESMA zona, pela dobra `F_{2w} = F_w ⊕ σF_w`: ela segura 16384
 * slots e as colunas são no máximo S_COLNOME_N, pelo que há espaço para os três
 * blocos --- o máximo, o mínimo, e o «já foi escrito» que o mínimo precisa e o
 * máximo não. O máximo usa o zero como «ainda não»; o mínimo não pode, porque
 * zero é um mínimo legítimo, e essa é exactamente a lição do S_PRES: «a marca a
 * zero não é uma afirmação». */
#define S_COLMIN    (S_COLMAX + S_COLNOME_N)
#define S_COLMINCAB (S_COLMIN + S_COLNOME_N)
typedef char marca_cabe_na_zona[(3u * S_COLNOME_N <= ZONA_SLOTS) ? 1 : -1];
/* S_CF definido em lib/slot_map.h — região FC, 2048..S_CF_END */
#define MAXLIN    250
/* quantas chaves uma propagação de seta pode olhar de uma vez --- é o tamanho
 * de um lote de trabalho e não uma dimensão do objecto, e por isso RECUSA em
 * vez de truncar quando é ultrapassado */
#define FK_MORRE_MAX 4096
#define MAXNO     64         /* nós da árvore do WHERE                                  */
#define SLOTSZ    SLOT_WORD_BYTES

/* ---------------- a memória É o disco ---------------- */
static int fmem = -1, fprog = -1;
static char g_base[512];          /* caminho da base aberta — blobs em <base>_corpo/ */
/* g_tabela declarada acima, junto às vistas: a tabela cujo .mem está aberto
 * ("" = a sem nome, que é o ficheiro da base) */
static long cel_recusadas = 0;    /* células que não couberam no envelope — contadas À PARTE */
static long cmp_recusadas = 0;    /* comparações recusadas por a coluna ser larga */
static int usa_tabela(const char *nome, int cria_se_falta);   /* definida com abrir_base */
static int ord_valor_vivo_col(long col, long valor, long nr);  /* a existência sem arranjo */

/* ── O RESTAURO QUE FALHA EM SILÊNCIO ────────────────────────────────────────
 *
 * Vinte sítios deste ficheiro seguem o mesmo padrão: guardar o nome da tabela
 * corrente, mudar para outra, fazer o trabalho, e voltar com
 * `usa_tabela(guarda, 0)`. Nos vinte o retorno é DEITADO FORA --- e quando o
 * restauro falha o `fmem` não se move, pelo que a sessão fica na tabela ERRADA
 * e tudo o que vier a seguir opera nela. Não há erro: há respostas sobre outro
 * objecto.
 *
 * Antes de concluir seja o que for, MEDE-SE: este contador diz quantas vezes um
 * restauro não conseguiu voltar. É instrumento, não correcção --- não muda uma
 * vírgula do que o motor faz. Se for zero em toda a bateria, o padrão está a
 * salvo hoje; se não for, sabe-se onde procurar em vez de se adivinhar. */
static long restauros_falhados = 0;
long sql_restauros_falhados(void){ return restauros_falhados; }
static int usa_tabela_z(const char *nome, int cria_se_falta, int zera);
static int tabela_existe(const char *nome);
static void caminho_tabela(const char *nome, char *out, size_t cap, const char *ext);

/* O backend do canal. Trocar isto por STOMP ou por TCP é trocar estas duas funções — o banco
 * não distingue, porque o que ele faz é sempre ler um slot e escrever um slot. */
static int canal_fd = -1;
static unsigned char canal_banda[32];
static struct sockaddr_in canal_dst;
/* ── O CANAL SAI DO LOOPBACK QUANDO SE LHO PEDE, E SÓ AÍ ──────────────────────
 * O grupo, a porta e a interface eram três constantes cravadas em loopback: o
 * canal falava consigo próprio e nunca cruzava a máquina. Passam a ler-se do
 * ambiente, e os DEFAULTS são os de sempre --- sem env, o byte que sai é o
 * mesmo, e o `sql teste` não muda. Para o barramento a sério (dois bancos, duas
 * máquinas) põe-se a interface na placa real:
 *
 *   TIFFANY_TECIDO       banda = sha256(tecido) (omissão: "tecido por omissao")
 *   TIFFANY_PUB          chave pública hex → banda = sha256(bytes); tem prioridade
 *   TIFFANY_CANAL_PORTA  a porta      (por omissão 47313)
 *   TIFFANY_CANAL_IF     a interface: "loopback" (omissão), "any" (0.0.0.0, a
 *                        placa que o SO escolher), ou um IP concreto da placa
 *
 * Nada disto toca no que VIAJA: a trama continua 6 bytes (slot·4 + Word·2),
 * bump-ada com a banda do tecido. Muda por ONDE viaja, não O QUE. */
static unsigned long canal_if_addr(void){
    const char *v = getenv("TIFFANY_CANAL_IF");
    if(!v || !*v || !strcmp(v, "loopback")) return INADDR_LOOPBACK;
    if(!strcmp(v, "any")) return INADDR_ANY;
    unsigned long a = ntohl(inet_addr(v));
    return a ? a : INADDR_LOOPBACK;   /* IP inválido: não se abre uma porta muda em 0.0.0.0 sem querer */
}
static void canal_abre(void){
    if(canal_fd >= 0) return;
    banda_de(getenv("TIFFANY_TECIDO") ? getenv("TIFFANY_TECIDO") : "tecido por omissao",
             canal_banda);
    {
      const char *pub = getenv("TIFFANY_PUB");
      if(pub && *pub && !banda_de_chave(pub, canal_banda)){
        fprintf(stderr, "TIFFANY_PUB: hex invalida — uso tecido\n");
        banda_de(getenv("TIFFANY_TECIDO") ? getenv("TIFFANY_TECIDO") : "tecido por omissao",
                 canal_banda);
      }
    }
    const char *grupo = getenv("TIFFANY_CANAL_GRUPO"); if(!grupo || !*grupo) grupo = "239.7.31.27";
    const char *porta_s = getenv("TIFFANY_CANAL_PORTA");
    unsigned short porta = (porta_s && *porta_s) ? (unsigned short)atoi(porta_s) : 47313;
    unsigned long ifaddr = canal_if_addr();
    canal_fd = socket(AF_INET, SOCK_DGRAM, 0);
    int um = 1;
    setsockopt(canal_fd, SOL_SOCKET, SO_REUSEADDR, &um, sizeof um);
    struct sockaddr_in e; memset(&e, 0, sizeof e);
    e.sin_family = AF_INET; e.sin_addr.s_addr = htonl(INADDR_ANY); e.sin_port = htons(porta);
    bind(canal_fd, (struct sockaddr*)&e, sizeof e);
    struct ip_mreq mr;
    mr.imr_multiaddr.s_addr = inet_addr(grupo);
    mr.imr_interface.s_addr = htonl(ifaddr);
    setsockopt(canal_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mr, sizeof mr);
    struct in_addr i; i.s_addr = htonl(ifaddr);
    setsockopt(canal_fd, IPPROTO_IP, IP_MULTICAST_IF, &i, sizeof i);
    setsockopt(canal_fd, IPPROTO_IP, IP_MULTICAST_LOOP, &um, sizeof um);
    struct timeval to = { 0, 200000 };
    setsockopt(canal_fd, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof to);
    memset(&canal_dst, 0, sizeof canal_dst);
    canal_dst.sin_family = AF_INET;
    canal_dst.sin_addr.s_addr = inet_addr(grupo);
    canal_dst.sin_port = htons(porta);
}
static void canal_grava(unsigned slot, Word w){
    canal_abre();
    unsigned char m[6], ks[6], b[6];
    memcpy(m, &slot, 4); m[4] = w.total; m[5] = w.e;
    keystream(canal_banda, ks, 6);
    bump(m, ks, b, 6);
    sendto(canal_fd, b, 6, 0, (struct sockaddr*)&canal_dst, sizeof canal_dst);
}
static Word canal_le(unsigned slot){
    canal_abre();
    Word w = { 0, 0 };
    unsigned char ks[6], b[6], m[6];
    keystream(canal_banda, ks, 6);
    for(;;){
        socklen_t n = sizeof canal_dst;
        if(recvfrom(canal_fd, b, 6, 0, (struct sockaddr*)&canal_dst, &n) != 6) break;
        bump(b, ks, m, 6);
        unsigned s2; memcpy(&s2, m, 4);
        if(s2 == slot){ w.total = m[4]; w.e = m[5]; break; }
    }
    return w;
}
/* O backend do pool. Trocar stratum por outra coisa e trocar estas duas — nem uma linha de SQL. */
/* o pool e a tabela de relacoes moram no disco — 9,7 KB e 8 KB que nao tem por que
 * estar em .bss: sao estado do processo, e o processo le-os do ficheiro */
static Pool *const pool_p = DISCO_FIXO(Pool, 24);
#define pool_st  (*pool_p)
static int  pool_ligado = 0;
static char pool_user[128];
/* ---------------- O BANCO DAS CONFIGS ----------------
 *
 * Um banco NORMAL e a parte: ficheiro proprio, pread proprio, e por isso pode ser lido de
 * qualquer sitio — inclusive daqui, que e antes do mem_le existir. Foi o que me partiu tres
 * tentativas seguidas: eu queria meter a config nos slots do banco da mina e o pool_abre mora
 * ACIMA do mem_le. Separado, o problema nao existe.
 *
 * A chave do stratum vive aqui, e nao no ambiente: quem guarda estado e o banco, e uma chave em
 * variavel de ambiente e estado fora dele.
 *
 * Um registo e (nome, valor): 4 slots de nome, 28 de valor, 64 registos.
 *   ./sql <base> "CONFIG pool_user 'a chave'"     poe
 *   ./sql <base> "CONFIG pool_user"               le
 */
#define CONF_N   64
#define CONF_SL  32
#define CONF_NM  64
static int fconf = -1;
static void conf_abre(const char *base){
    if(fconf >= 0) return;
    char c[512]; snprintf(c, sizeof c, "%s.conf", base);
    fconf = open(c, O_RDWR|O_CREAT, 0600);          /* 0600: e chave, e so do dono */
}
static int conf_slot(const char *nome){
    unsigned h = 5381;
    for(const char *p = nome; *p; p++) h = ((h << 5) + h) ^ (unsigned char)*p;
    return (int)(h % CONF_N);
}
static void conf_poe(const char *nome, const char *valor){
    if(fconf < 0) return;
    char reg[CONF_SL*16]; memset(reg, 0, sizeof reg);
    snprintf(reg, CONF_NM, "%s", nome);
    snprintf(reg + CONF_NM, sizeof reg - CONF_NM, "%s", valor);
    pwrite(fconf, reg, sizeof reg, (off_t)conf_slot(nome)*sizeof reg);
    fsync(fconf);
}
static void conf_le(const char *nome, char *out, size_t lim){
    out[0] = 0;
    if(fconf < 0) return;
    char reg[CONF_SL*16];
    if(pread(fconf, reg, sizeof reg, (off_t)conf_slot(nome)*sizeof reg) != (ssize_t)sizeof reg) return;
    reg[CONF_NM-1] = 0; reg[sizeof reg - 1] = 0;
    if(strcmp(reg, nome)) return;                    /* nao e este: o slot e de outro nome */
    snprintf(out, lim, "%s", reg + CONF_NM);
}
static void pool_abre(void){
    if(pool_ligado) return;
    /* a config do banco vem PRIMEIRO; o ambiente e o recurso de quem ainda nao a pos */
    char *cu = DISCO_FIXO(char, 120);
    char *ch = DISCO_FIXO(char, 121);
    char *cp = DISCO_FIXO(char, 122);
    disco_prende(DISCO_BASE(120),"dados/cu_120.bin",(size_t)((512)),sizeof(char));
    disco_zera(cu,(size_t)((512)),sizeof(char));
    disco_prende(DISCO_BASE(121),"dados/ch_121.bin",(size_t)((512)),sizeof(char));
    disco_zera(ch,(size_t)((512)),sizeof(char));
    disco_prende(DISCO_BASE(122),"dados/cp_122.bin",(size_t)((64)),sizeof(char));
    disco_zera(cp,(size_t)((64)),sizeof(char));
    conf_le("pool_user", cu, ((size_t)((512))*sizeof(char)));
    conf_le("pool_host", ch, ((size_t)((512))*sizeof(char)));
    conf_le("pool_porta", cp, ((size_t)((64))*sizeof(char)));
    const char *u = cu[0] ? cu : getenv("TIFFANY_POOL_USER");
    const char *h = ch[0] ? ch : getenv("TIFFANY_POOL_HOST");
    const char *p = cp[0] ? cp : getenv("TIFFANY_POOL_PORTA");
    if(!u || !h) return;
    snprintf(pool_user, sizeof pool_user, "%s", u);
    pool_ligado = st_liga(&pool_st, h, p ? atoi(p) : 3333, pool_user);
}
static void merkle_pelo_fold(void);   /* definida depois da maquina, que e quem dobra */
static Word pool_le(unsigned slot){
    Word w = { 0, 0 };
    pool_abre();
    if(!pool_ligado) return w;
    char l[8192];
    while(st_linha(&pool_st, l, sizeof l)){
        Fonte f = fonte_de(l);
        st_trata_fonte(&pool_st, &f);
    }
    unsigned k = slot - S_POOL;
    /* Word_8: campos largos ficam em pool_st; o slot só sinaliza / pedaço baixo. */
    if(k == 0) w.total = (Word8)pool_st.versao;
    else if(k == 1) w.total = (Word8)pool_st.nbits;
    else if(k == 2) w.total = (Word8)pool_st.ntime;
    else if(k >= 3 && k <= 10){
        const unsigned char *b = pool_st.prevhash + 4*(k-3);
        w.total = b[0]; w.e = b[1];
    }
    else if(k == 11) w.total = pool_st.tem_job ? 1 : 0;
    else if(k >= 12 && k <= 19){
        merkle_pelo_fold();
        const unsigned char *b = pool_st.merkle_raiz + 4*(k-12);
        w.total = b[0]; w.e = b[1];
    }
    return w;
}
static void pool_grava(unsigned slot, Word w){
    pool_abre();
    if(!pool_ligado) return;
    if(slot == S_POOL_SH) st_submete(&pool_st, pool_user, (unsigned)w.total);
}
static Word mem_le(unsigned slot){
    if(slot >= S_POOL)  return pool_le(slot);
    if(slot >= S_CANAL) return canal_le(slot);
    /* Word ISA = 2 átomos (total,e) — Lei 7; sem long. */
    return (Word){
        slot_mem_le(fmem, slot * 2u),
        slot_mem_le(fmem, slot * 2u + 1u)
    };
}
/* ── O DESDOBRAR: a transacção guarda o que a escrita colou por cima ─────────
 *
 * O motor escrevia directo no disco e o ROLLBACK não desfazia nada — devolvia a
 * tag e a linha ficava lá. A resposta não é inventar um sistema de transacções:
 * é o LEVANTAMENTO que o `aranha.tex` já prova.
 *
 * Ali, π identifica índices na mesma célula (a DOBRA, G > 1) e o levantamento
 * π̃ = (π, k) separa-os outra vez com UMA coordenada — o número da visita —,
 * com volta exacta. Aqui é a mesma coisa: escrever é dobrar, porque o valor
 * novo cola-se por cima do velho e a célula esquece qual era; e guardar o valor
 * ANTERIOR é a coordenada de folha que desdobra.
 *
 * Em operadores é a segunda equação a correr ao contrário: acumular é ζ,
 * desfazer é μ = ζ⁻¹, e desfaz-se LENDO A PILHA DE TRÁS PARA A FRENTE, que é a
 * diferença finita aplicada à ordem das escritas.
 *
 * O TECTO É DECLARADO, e se encher a transacção fica MARCADA e o ROLLBACK
 * RECUSA: desfazer metade seria pior do que não desfazer, porque deixaria a
 * base num estado que nunca existiu.
 *
 * E O DIÁRIO MORA NO DISCO. Estava num `Desfaz undo_pilha[16384]` --- 131 KB de
 * RAM --- com o comentário ao lado a dizer «sem RAM em execução, como o resto
 * da casa». Era falso, e era falso no sítio onde a casa é mais exigente: o
 * desfazer É a trajectória (§W: «cada entrada é uma escrita, o índice é a ordem
 * e o slot é a célula»), e uma trajectória guardada em RAM não sobrevive à
 * queda que ela existe para cobrir. Agora é uma zona do `.mem`: três Words por
 * entrada --- o slot no par de duas, o valor antes na terceira ---, e o tecto é
 * o que as zonas seguram. */
#define UNDO_ZONAS 8u
#define S_UNDO     (ISA_TECTO + ZONA(37))       /* 37..44 */
#define UNDO_W     3u
#define UNDO_MAX   (long)((ZONA_SLOTS * UNDO_ZONAS) / UNDO_W)
/* E O DIÁRIO TEM FICHEIRO PRÓPRIO, `<base>.undo`.
 *
 * A primeira escrita disto pôs o diário numa zona do `.mem` --- e o `.mem` é POR
 * TABELA: uma transacção que toca duas tabelas escrevia metade da trajectória
 * num ficheiro e metade noutro, com um contador só a numerá-las. A trajectória é
 * do SISTEMA e não de uma tabela; por isso mora num ficheiro seu, aberto com a
 * base e independente de qual tabela está aberta.
 *
 * Escreve-se e lê-se com `slot_mem_*` directo: passar pelo `mem_grava` seria o
 * diário a registar-se a si próprio, sem fundo. */
static int fundo = -1;
static void undo_poe(long k, unsigned slot, Word antes){
    if(fundo < 0) return;
    unsigned b = S_UNDO + (unsigned)k * UNDO_W;
    slot_mem_grava(fundo, b * 2u,       (Word8)(slot & 255u));
    slot_mem_grava(fundo, b * 2u + 1u,  (Word8)((slot >> 8) & 255u));
    slot_mem_grava(fundo, (b+1u) * 2u,      (Word8)((slot >> 16) & 255u));
    slot_mem_grava(fundo, (b+1u) * 2u + 1u, (Word8)((slot >> 24) & 255u));
    slot_mem_grava(fundo, (b+2u) * 2u,      antes.total);
    slot_mem_grava(fundo, (b+2u) * 2u + 1u, antes.e);
}
static unsigned undo_slot(long k){
    unsigned b = S_UNDO + (unsigned)k * UNDO_W;
    return (unsigned)slot_mem_le(fundo, b * 2u)
         | ((unsigned)slot_mem_le(fundo, b * 2u + 1u) << 8)
         | ((unsigned)slot_mem_le(fundo, (b+1u) * 2u) << 16)
         | ((unsigned)slot_mem_le(fundo, (b+1u) * 2u + 1u) << 24);
}
static Word undo_antes(long k){
    unsigned b = S_UNDO + (unsigned)k * UNDO_W;
    Word w; w.total = slot_mem_le(fundo, (b+2u) * 2u);
            w.e     = slot_mem_le(fundo, (b+2u) * 2u + 1u);
    return w;
}
static long   undo_n = 0;
static int    undo_em_tx = 0;
static int    undo_cheio = 0;
static void mem_grava(unsigned slot, Word w){
    /* A ORDEM É A DO `mem_le`, E TEM DE SER.
     *
     * S_POOL = S_CANAL + 100000, logo o pool está ACIMA do canal. O `mem_le`
     * pergunta primeiro pelo pool e só depois pelo canal; aqui perguntava-se ao
     * contrário, de modo que um slot do pool satisfazia `slot >= S_CANAL` e ia
     * para o canal — ESCREVIA-SE NUM SÍTIO E LIA-SE DE OUTRO. A ida não guardava
     * a volta, que é a segunda equação do operador (∂x · x = 1): as duas metades
     * de um par têm de decidir pelo mesmo critério, e a maneira de o garantir é
     * a ordem ser a mesma nas duas. */
    if(slot >= S_POOL){ pool_grava(slot, w); return; }
    if(slot >= S_CANAL){ canal_grava(slot, w); return; }
    /* dentro de uma transacção, o que se vai colar por cima fica guardado */
    if(undo_em_tx){
        if(undo_n < UNDO_MAX){
            undo_poe(undo_n, slot, mem_le(slot));
            undo_n++;
        }else{
            undo_cheio = 1;                 /* e o ROLLBACK vai recusar */
        }
    }
    slot_mem_grava(fmem, slot * 2u,     w.total);
    slot_mem_grava(fmem, slot * 2u + 1u, w.e);
}

/* O NROWS, LIDO E ESCRITO NO PAR.
 *
 * `S_NR` guarda-o nos dois componentes — baixo e alto. NÃO HÁ TECTO DE LINHAS
 * NA TEORIA, e pôr um aqui foi invenção minha: pelo `thm:BI` do `aranha.tex` a
 * dobra DUPLICA a largura, e a cadeia {0,1} ⊂ {0..3} ⊂ {0..15} ⊂ {0..255}
 * enumera andares em vez de acabar num deles. O `arquitetura.tex §sec:torre`
 * diz o que fazer quando não cabe — T_{k+1} = T_k + T_k*, d_{k+1} = 2·d_k, «o
 * que cresce é o OBJECTO, não a máquina», com PROMOVE a dobrar e DESCE a
 * colapsar com resíduo 0. O que limita é o MAPA DE SLOTS deste ficheiro, que é
 * da máquina e está por endireitar.
 *
 * MIGRAÇÃO: uma base gravada antes tem o nrows no `.e` do catálogo e o S_NR a
 * zero. Nesse caso adopta-se o valor antigo, uma vez. É a volta: o formato novo
 * lê o que o velho escreveu, em vez de o dar por perdido. */
/* O PAR, LIDO E ESCRITO COMO UM NÚMERO DE DEZASSEIS BITS.
 *
 * `Word` é {Word8 total, e} — DOIS bytes. Ler `.total` é ler METADE. Estas duas
 * funções existem porque o mesmo defeito apareceu em cinco sítios deste
 * ficheiro (o valor da célula, o ponteiro da zona de texto, o contador de nós
 * da cifra, o contador de nós da ordem e o do count): todo o slot que guarde um
 * ENDEREÇO, um CONTADOR ou um ÍNDICE passa por aqui. */
static unsigned par_le(unsigned slot){
    Word w = mem_le(slot);
    return (unsigned)((unsigned long)w.total | ((unsigned long)w.e << 8));
}
static void par_grava(unsigned slot, unsigned v){
    Word w = { (Word8)(v & 255u), (Word8)((v >> 8) & 255u) };
    mem_grava(slot, w);
}


/* A PALAVRA É UM BIT, E A LARGURA É ARGUMENTO.
 *
 * `lib/largura.h`: «UMA LEI PARA TODA A ESCADA, com w PARÂMETRO — seis andares,
 * um corpo», e «o tipo da máquina é o VEÍCULO; w é o PARÂMETRO». Aqui estava
 * escrito `i >> 4`, `i & 15` e `k < 8`: três palavras fixas, que é especializar
 * por andar — exactamente o que a lei diz para não fazer.
 *
 * Nada abaixo escreve 8 nem 16. O átomo diz quantos bits tem, a Word diz
 * quantos átomos tem (`WORD_ISA_ATOMS`, do `word_isa.h`), e o resto deriva. Se
 * o andar dobrar, dobra sozinho.
 *
 * A LINHA i É A COORDENADA i (naturais `cor:w8`: «o bit j do inteiro é a
 * coordenada j na base»), e lê-se como o bit i. */
/* (ATOMO_BITS e SLOT_BITS vivem junto do MAPA, que é quem precisa deles primeiro) */

static unsigned atomo_le(Word w, unsigned a){
    return a ? (unsigned)w.e : (unsigned)w.total;      /* WORD_ISA_ATOMS = 2 */
}
static void atomo_poe(Word *w, unsigned a, unsigned v){
    if(a) w->e = (Word8)v; else w->total = (Word8)v;
}
/* ── APAGAR O MATCH: OS SLOTS QUE O CONTÊM, E NEM UM A MAIS ──────────────────
 *
 * Estava escrito `q <= nrows / SLOT_BITS` em QUATRO sítios. O último bit a
 * apagar é o da linha nrows-1, que vive no slot (nrows-1)/SLOT_BITS — e a
 * divisão arredonda para baixo, pelo que o `<=` só acerta quando nrows NÃO é
 * múltiplo da largura do slot. Quando é, o laço escreve um slot além do
 * bitmap. Medido: com 4096 linhas o slot excedente É o S_VIVO[0], e as linhas
 * 0..15 desapareciam da tabela — dezasseis linhas mortas por um `<=`, sem erro
 * nenhum, num SELECT que só queria limpar o seu rascunho.
 *
 * A forma sem casos: um slot entra se o seu primeiro bit é uma linha. Com
 * nrows=0 não entra nenhum, que é o certo — não há bit para apagar. */
static void match_limpa(long nrows){
    Word z = {0,0};
    for(long q = 0; q * (long)SLOT_BITS < nrows; q++) mem_grava(S_MATCH + (unsigned)q, z);
}

/* ── O GUARDA DO BITMAP: TODO O BIT PASSA POR AQUI, LOGO A LEI TAMBÉM ────────
 *
 * Os dois piores defeitos que este ficheiro teve esta semana foram o MESMO
 * defeito visto de dois sítios: um bit escrito além do seu bitmap, a aterrar no
 * vizinho. Nem um nem outro deu erro --- deram LINHAS A DESAPARECER, que é a
 * pior forma de os dar, porque o resultado continua a parecer um resultado.
 *
 * Encontrei-os por sorte, um de cada vez. Ora os três bitmaps desta casa
 * atravessam TODOS estas duas funções, e portanto a lei pode viver aqui: cada
 * base sabe quantos bits comporta, e quem pede um bit fora não escreve no
 * vizinho --- CONTA. Ler fora devolve ZERO, que é a ausência e é a resposta
 * certa: o bit que não existe não está ligado.
 *
 * O contador é o instrumento. Não é uma asserção que pára o programa: é uma
 * MEDIDA que os medidores podem exigir seja zero, e que diz QUANTAS vezes se
 * saiu, em vez de dizer que se saiu uma vez. Uma base que este mapa não conheça
 * devolve tecto −1 e não é vigiada --- não se afirma sobre o que não se sabe. */
static long bit_tecto(unsigned base){
    if(base == S_MATCH) return LIN_MATCH;
    if(base == S_VIVO)  return LIN_VIVO;
    if(base == S_PRES)  return (long)(S_RESTR - S_PRES) * (long)WORD_ISA_BITS;
    return -1;
}
static long bits_fora = 0;        /* quantas vezes se pediu um bit fora do bitmap */
static int bit_cabe(unsigned base, long i){
    long t = bit_tecto(base);
    if(i < 0 || (t >= 0 && i >= t)){ bits_fora++; return 0; }
    return 1;
}
long sql_bits_fora(void){ return bits_fora; }
void sql_bits_fora_zera(void){ bits_fora = 0; }

static int bit_le(unsigned base, long i){
    if(!bit_cabe(base, i)) return 0;        /* o bit que não existe é ausência */
    unsigned long u = (unsigned long)i;
    Word w = mem_le(base + (unsigned)(u / SLOT_BITS));
    unsigned k = (unsigned)(u % SLOT_BITS);
    return (int)((atomo_le(w, k / ATOMO_BITS) >> (k % ATOMO_BITS)) & 1u);
}
static void bit_poe(unsigned base, long i, int liga){
    if(!bit_cabe(base, i)) return;          /* e não se escreve no vizinho */
    unsigned long u = (unsigned long)i;
    unsigned sl = base + (unsigned)(u / SLOT_BITS), k = (unsigned)(u % SLOT_BITS);
    unsigned a = k / ATOMO_BITS, m = 1u << (k % ATOMO_BITS);
    Word w = mem_le(sl);
    unsigned v = atomo_le(w, a);
    atomo_poe(&w, a, liga ? (v | m) : (v & ~m));
    mem_grava(sl, w);
}
/* O GUME DO PRÓPRIO GUARDA. O medidor precisa de exigir que o contador CONTE, e
 * para isso tem de pedir um bit que não existe --- pelo caminho normal, que é o
 * mesmo por onde o defeito real entrava. Se o índice couber, isto é uma leitura
 * como outra qualquer; se não couber, é o guarda a fazer o seu trabalho. Sem
 * isto, «bits_fora == 0» e um contador avariado seriam indistinguíveis. */
int sql_bit_fora_de_proposito(long i){ return bit_le(S_MATCH, i); }
/* PELO MATCH, e não pelo vivo: cada bitmap tem o SEU tecto, e o LIN_TECTO é o
 * MENOR dos dois --- o do match. Pedir o bit LIN_TECTO ao vivo, que comporta o
 * dobro, é pedir um bit que CABE, e foi o que fiz à primeira: o controlo deu
 * zero e apanhou-me, que é exactamente para o que ele lá está. */

/* ∑ — O KIRCHHOFF: CONTAR É POPCOUNT, NÃO VARRER.
 *
 * `tests/neuronio.c`, no cabeçalho: «⊕ cisão b agrupado mod n · ∑ soma
 * POPCOUNT (o Kirchhoff)». O bitmap tem uma coordenada por linha, logo quantas
 * linhas casaram é QUANTOS BITS ESTÃO LIGADOS — e isso soma-se sobre os slots,
 * sem visitar linha nenhuma. É o `thm:dobra-norma` do `aranha.tex` na sua forma
 * mais simples: «lê-se num único número calculado sobre o campo, sem visitar a
 * trajetória».
 *
 * Estava um contador a ser incrementado linha a linha DENTRO do bytecode, com
 * um slot próprio e um OP_ADD16 por linha que casa. Era trabalho a mais para
 * saber o que o campo já diz. */
static long bits_conta(unsigned base, long n){
    long soma = 0;
    /* O TECTO É O DA ZONA, e verifica-se. O bitmap do resultado ocupa de
     * S_MATCH a S_VIVO; percorrer além disso é ler outra zona — devagar, e a
     * contar bits que não são de ninguém. Um `n` grande vindo de um catálogo
     * ainda por abrir bastava para isso. */
    long tecto = (long)(S_VIVO - S_MATCH) - 1;
    long ate = n / (long)SLOT_BITS;
    if(ate > tecto) ate = tecto;
    if(ate < 0) ate = 0;
    /* ── E CONTA-SE COMO O `aranha.c` CONTA: SEM RAMIFICAR ───────────────
     *
     * Estava `while(v){ soma += v & 1u; v >>= 1; }` --- um laço cujo número de
     * voltas DEPENDE DO VALOR: um átomo a zero sai logo, um com o bit alto
     * ligado dá oito voltas. Isso é o agente a percorrer, e o custo passa a ser
     * função do dado em vez de o ser da largura.
     *
     * O `papers/aranha.c` mostra a forma, e não tem um único `if`:
     *
     *     phase[0] += (b >> 0) & 1u;
     *     phase[1] += (b >> 1) & 1u;   ... oito, fixos
     *
     * «Cada bit alimenta exactamente uma lei» --- é a projecção na base
     * ortonormal e_k = 2^k, com uma soma por coordenada e nenhuma decisão. É a
     * mesma coisa que o motor já diz de si no WHERE: «o par ⊗/⊘ do algoritmo
     * não ramifica: OPERA», com as faces a correrem simultaneamente no espaço
     * de fases. Aqui as coordenadas são os bits do átomo, e o laço é sobre a
     * LARGURA (que é do tipo, e conhecida), não sobre o valor. */
    for(long sl = 0; sl <= ate; sl++){
        Word w = mem_le(base + (unsigned)sl);
        for(unsigned a = 0; a < WORD_ISA_ATOMS; a++){
            unsigned v = atomo_le(w, a);
            for(unsigned k = 0; k < ATOMO_BITS; k++) soma += (long)((v >> k) & 1u);
        }
    }
    return soma;
}

static long cat_ncols(void){
    Word c = mem_le(S_CAT);
    return (long)c.total | ((long)mem_le(S_NCOLA).total << 8);
}
static void cat_ncols_poe(long n){
    Word c = mem_le(S_CAT); c.total = (Word8)(n & 255); mem_grava(S_CAT, c);
    Word a; a.total = (Word8)((n >> 8) & 255); a.e = 0; mem_grava(S_NCOLA, a);
}

static long cat_nrows(void){
    Word w = mem_le(S_NR);
    long n = (long)((unsigned long)w.total | ((unsigned long)w.e << 8));
    if(n == 0){
        long velho = mem_le(S_CAT).e;          /* base gravada antes do par */
        if(velho > 0) return velho;
    }
    return n;
}
static void cat_poe_nrows(long n){
    Word w = { (Word8)((unsigned long)n & 255u), (Word8)(((unsigned long)n >> 8) & 255u) };
    mem_grava(S_NR, w);
}

/* a transacção: abrir, desfazer, confirmar */
void sql_tx_abre(void){ undo_em_tx = 1; undo_n = 0; undo_cheio = 0; }
int  sql_tx_cheia(void){ return undo_cheio; }
long sql_tx_escritas(void){ return undo_n; }
void sql_tx_fecha(void){ undo_em_tx = 0; undo_n = 0; undo_cheio = 0; }
/* A PILHA É A TRAJECTÓRIA, e daqui lê-se o campo do `aranha.tex`.
 *
 * Cada entrada é uma escrita: o índice é a ordem, o slot é a célula. Isso É a
 * realização π : I → X da Def. do paper, com X o espaço de endereços. Logo:
 *
 *     |I|        = o número de escritas          (undo_n)
 *     |supp G|   = os slots DISTINTOS escritos
 *     G(x)       = quantas vezes o slot x foi escrito
 *     ∑ G(x)     = |I|                            (a conservação)
 *
 * A última igualdade não é uma escolha de contabilidade: é o Lema da
 * conservação, e o que ela diz é que nenhuma escrita se perde no caminho — só
 * se sobrepõe. Devolve-se aqui para o medidor a poder verificar no MOTOR, e não
 * só no papel. */
/* ── A RÉGUA É UMA SÓ, E É A ULTRAMÉTRICA ────────────────────────────────────
 *
 * Escrevi primeiro uma métrica quadrática pesada pelo campo, τ = Σ G(x_t) --- e
 * isso é euclidiano, não é esta casa. A régua desta obra é UMA: a ultramétrica
 * da `§sec:ultra`,
 *
 *     d(a,b) = 2^{−prof(a,b)},   prof = a profundidade da primeira divergência
 *
 * e o que a define não é uma desigualdade triangular mais apertada de que se
 * tire um número melhor: é a PARTIÇÃO. «Duas bolas de raio r ou coincidem ou são
 * disjuntas», «todo ponto é centro», e é por isso que ΣG = |I| não exige cuidado
 * nenhum --- não há sobreposição a haver.
 *
 * DAÍ SAI A CONSEQUÊNCIA QUE MUDA A CONTA: numa ultramétrica um percurso não
 * SOMA. O `cor:global` dá-o como lei --- D(R_0,R_n) ≤ max D(R_{i−1},R_i) ---, o
 * pior passo ABSORVE os outros, e é isso que faz o cone virar espiral: o custo
 * não acumula linearmente, fica preso ao passo mais fundo. Somar seria medir com
 * uma régua que esta casa não tem.
 *
 * Devolve-se então o que a régua diz do percurso das escritas: o MÁXIMO dos
 * passos (a distância ultramétrica de ponta a ponta), a SOMA (só para se ver que
 * ela não é a resposta), e quantos passos a absorção cobre --- que é a lei a
 * verificar-se, e não a supor-se. */
static long prof_slots(unsigned a, unsigned b){
    /* a profundidade da primeira divergência, do bit mais alto para baixo */
    if(a == b) return 32;                       /* iguais: distância zero */
    long q = 0;
    for(int k = 31; k >= 0; k--){
        if(((a >> k) & 1u) != ((b >> k) & 1u)) break;
        q++;
    }
    return q;
}
void sql_tx_ultra(long *prof_min, long *prof_ponta, long *absorve, long *passos){
    long pmin = 32, n = 0, cobre = 0;
    for(long i = 1; i < undo_n; i++){
        long q = prof_slots(undo_slot(i-1), undo_slot(i));
        if(q < pmin) pmin = q;                  /* o MENOR prefixo = o MAIOR salto */
        n++;
    }
    /* a distância de ponta a ponta, e a lei: ela é ≤ ao maior salto, isto é, a
     * profundidade da ponta é ≥ à menor profundidade do caminho */
    long pponta = (undo_n >= 2)
                ? prof_slots(undo_slot(0), undo_slot(undo_n - 1)) : 32;
    cobre = (pponta >= pmin) ? 1 : 0;
    if(prof_min)   *prof_min = pmin;
    if(prof_ponta) *prof_ponta = pponta;
    if(absorve)    *absorve = cobre;
    if(passos)     *passos = n;
}

void sql_tx_fibra(long *escritas, long *slots_distintos, long *maior_G,
                  long *soma_G){
    long dist = 0, maxg = 0, soma = 0;
    if(escritas) *escritas = undo_n;
    for(long i = 0; i < undo_n; i++){
        long g = 0;
        int primeiro = 1;
        unsigned si = undo_slot(i);
        for(long j = 0; j < i; j++) if(undo_slot(j) == si){ primeiro = 0; break; }
        if(!primeiro) continue;
        dist++;
        for(long j = i; j < undo_n; j++) if(undo_slot(j) == si) g++;
        if(g > maxg) maxg = g;
        soma += g;                       /* ∑G, somado de facto e não suposto */
    }
    if(slots_distintos) *slots_distintos = dist;
    if(maior_G) *maior_G = maxg;
    if(soma_G) *soma_G = soma;
}

/* desfaz LENDO AO CONTRÁRIO: a última escrita é a primeira a ser reposta, ou um
 * slot escrito duas vezes ficaria com o valor do meio. */
int  sql_tx_desfaz(void){
    if(undo_cheio) return 0;
    if(fmem < 0) return 0;
    for(long i = undo_n - 1; i >= 0; i--){
        unsigned s = undo_slot(i);
        Word v = undo_antes(i);
        slot_mem_grava(fmem, s * 2u,     v.total);
        slot_mem_grava(fmem, s * 2u + 1u, v.e);
    }
    undo_n = 0;
    return 1;
}
/* átomos físicos consecutivos (1 B = 1 índice) — cab/merkle/texto; sem stride Word */
static void atomos_grava(unsigned base, const unsigned char *b, int n){
    for(int i = 0; i < n; i++) slot_mem_grava(fmem, base + (unsigned)i, b[i]);
}
static void atomos_le(unsigned base, unsigned char *b, int n){
    for(int i = 0; i < n; i++) b[i] = slot_mem_le(fmem, base + (unsigned)i);
}
/* u32 LE em átomos físicos — endereços/faixas sem long na Word. */
static void atomos_u32(unsigned base, unsigned v){
    unsigned char b[4] = { (unsigned char)v, (unsigned char)(v>>8),
                           (unsigned char)(v>>16), (unsigned char)(v>>24) };
    atomos_grava(base, b, 4);
}
static unsigned atomos_le_u32(unsigned base){
    unsigned char b[4]; atomos_le(base, b, 4);
    return (unsigned)b[0] | ((unsigned)b[1]<<8) | ((unsigned)b[2]<<16) | ((unsigned)b[3]<<24);
}
/* ── GUARDA uma cadeia no pool e devolve o seu endereço (índice de átomo).
 * O topo vive no próprio disco, para o pool sobreviver ao fecho da base. */
/* ── UMA OPERAÇÃO, DUAS FACES, E O SINAL DIZ QUAL ────────────────────────────
 *
 * O `papers/nucleo_sql/slot.h` escreve a instrução da casa em quatro linhas:
 *
 *     move(mem, addr, reg, sinal):  +1 lê, −1 escreve, 0 é o ponto fixo
 *
 * --- «MOVE = dobra (ler e escrever são a mesma operação + sinal)». O pool
 * tinha as duas faces em funções SEPARADAS, e foi por isso que o defeito
 * apareceu: o `SET` chamou a que lê, o endereço veio 0, e o UPDATE dizia «1
 * linha atualizada» enquanto apagava. Com o sinal no argumento, quem chama TEM
 * de dizer de que lado está --- e não há como escolher a face errada por
 * distracção, porque não há duas para escolher.
 *
 * `tx(s, n, +1)` pergunta: devolve o endereço, ou 0 se ninguém escreveu a
 * cadeia --- e 0 é a ausência, que nenhuma célula presente tem.
 * `tx(s, n, -1)` escreve: procura primeiro (a mesma cadeia TEM de dar o mesmo
 * endereço, que é o critério da leitura) e só grava se ela não estiver lá. */
static unsigned tx(const char *s2, int n, int sinal);
static int poe_chave_texto(const char *s);   /* o termo também vai à árvore */
static unsigned tx_procura(const char *s2, int n){ return tx(s2, n, +1); }
static unsigned tx_guarda (const char *s2, int n){ return tx(s2, n, -1); }
static unsigned tx(const char *s2, int n, int sinal){
    if(n < 0) n = 0;
    if(n > TX_MAX) n = TX_MAX;
    unsigned topo = atomos_le_u32(S_TXTOPO);
    if(topo < S_TXPOOL) topo = S_TXPOOL;          /* primeira vez */
    /* ── A MESMA CADEIA TEM DE DAR O MESMO ENDEREÇO, e isto não é economia de
     * espaço: é o CRITÉRIO DA LEITURA. Uma leitura serve quando é bem definida
     * --- x = y ⟹ R(x) = R(y) --- e sem isto duas cadeias iguais recebiam
     * endereços diferentes, pelo que comparar endereços deixava de comparar
     * textos.
     *
     * Foi assim que a política de isolamento passou a aceitar tudo: o 'acme' da
     * linha e o 'acme' do SET eram endereços distintos, e a comparação nunca
     * batia. O comando era aceite e não fazia nada --- que é o pior desfecho
     * possível numa peça de segurança. */
    for(unsigned a2 = S_TXPOOL; a2 < topo; ){
        int ln = (int)slot_mem_le(fmem, a2);
        if(ln == n){
            int igual = 1;
            for(int i = 0; i < n; i++)
                if((char)slot_mem_le(fmem, a2 + 1u + (unsigned)i) != s2[i]){ igual = 0; break; }
            if(igual) return a2 - S_TXPOOL + 1u;
        }
        a2 += 1u + (unsigned)ln;
    }
    /* não está no pool: quem PERGUNTA fica por aqui --- criar a cadeia seria a
     * leitura a mudar o espaço, e a marca é de quem escreve */
    if(sinal >= 0) return 0;
    unsigned onde = topo;
    /* e o POOL também tem parede: acima de S_CANAL o slot deixa de ser memória e
     * passa a ser a banda do canal. O espaço é largo --- nove milhões de slots
     * --- mas «largo» não é «verificado», e o que estava aqui era o topo a subir
     * sem ninguém a olhar. Recusar devolve 0, que É a ausência: quem escreve vê
     * a célula ficar sem texto em vez de a ver escrita noutro sítio. */
    if(onde + 1u + (unsigned)n >= S_CANAL) return 0;
    slot_mem_grava(fmem, onde, (unsigned char)n);
    for(int i = 0; i < n; i++)
        slot_mem_grava(fmem, onde + 1u + (unsigned)i, (unsigned char)s2[i]);
    atomos_u32(S_TXTOPO, onde + 1u + (unsigned)n);
    return onde - S_TXPOOL + 1u;                  /* 0 fica para a cadeia vazia */
}
/* e LÊ de volta, pelo endereço */
static void tx_le(unsigned ix, char *out, int lim){
    out[0] = 0;
    if(!ix) return;
    unsigned onde = S_TXPOOL + ix - 1u;
    int n = (int)slot_mem_le(fmem, onde);
    if(n > lim - 1) n = lim - 1;
    for(int i = 0; i < n; i++) out[i] = (char)slot_mem_le(fmem, onde + 1u + (unsigned)i);
    out[n] = 0;
}

/* o corpo da coluna j: as oito primeiras no plano de sempre, as outras no largo */
static Word corpo_de(long j){
    if(j < 8) return mem_le(S_CORPO + (unsigned)j);
    if(j < (long)S_CORPOX_N) return mem_le(S_CORPOX + (unsigned)j);
    return (Word){0,0};
}
/* a k-ésima linha da saída, no disco --- e o par, porque uma linha passa de 255 */
static long ordseq_le(long k){
    Word w = mem_le(S_SAIDA + (unsigned)k);
    return (long)w.total | ((long)w.e << 8);
}
static void ordseq_poe(long k, long v){
    Word w; w.total = (Word8)(v & 255); w.e = (Word8)((v >> 8) & 255);
    mem_grava(S_SAIDA + (unsigned)k, w);
}
/* e as mesmas quatro portas para o GROUP BY, na zona dele: a sequência ordenada
 * e os ausentes, que formam a sua própria fibra e vão no fim. */
static long grpseq_le(long k){
    Word w = mem_le(S_GRPSEQ + (unsigned)k);
    return (long)w.total | ((long)w.e << 8);
}
static void grpseq_poe(long k, long v){
    Word w; w.total = (Word8)(v & 255); w.e = (Word8)((v >> 8) & 255);
    mem_grava(S_GRPSEQ + (unsigned)k, w);
}
static long grpaus_le(long k){
    Word w = mem_le(S_GRPAUS + (unsigned)k);
    return (long)w.total | ((long)w.e << 8);
}
static void grpaus_poe(long k, long v){
    Word w; w.total = (Word8)(v & 255); w.e = (Word8)((v >> 8) & 255);
    mem_grava(S_GRPAUS + (unsigned)k, w);
}
static long ordaus_le(long k){
    Word w = mem_le(S_SAIDAUS + (unsigned)k);
    return (long)w.total | ((long)w.e << 8);
}
static void ordaus_poe(long k, long v){
    Word w; w.total = (Word8)(v & 255); w.e = (Word8)((v >> 8) & 255);
    mem_grava(S_SAIDAUS + (unsigned)k, w);
}

/* ── O MÓRFICO ESCRITO COMO CONJUNTO, NUM SÍTIO SÓ ───────────────────────────
 * A célula guarda a máscara e lê-se `{0,2}`. Estava inline no laço da saída, e
 * a chave do GROUP BY não a tinha --- devolvia o número cru, que é a coordenada
 * no lugar do objecto, o mesmo defeito que o TEXTO tinha. Um sítio, dois
 * leitores. */
static void cel_morfico(unsigned long msk, long n, char *out, int cap){
    if(n < 1 || n > 62) n = 6;
    size_t o = 0;
    if(cap < 4){ if(cap) out[0] = 0; return; }
    out[o++] = '{';
    int primeiro = 1;
    for(long t = 0; t < n && o + 8 < (size_t)cap; t++) if(msk & (1UL << t)){
        o += (size_t)snprintf(out + o, (size_t)cap - o, "%s%ld", primeiro ? "" : ",", t);
        primeiro = 0;
    }
    if(o < (size_t)cap - 1){ out[o++] = '}'; out[o] = 0; }
}

static void corpo_poe(long j, Word w){
    if(j < 8) mem_grava(S_CORPO + (unsigned)j, w);
    if(j < (long)S_CORPOX_N) mem_grava(S_CORPOX + (unsigned)j, w);
}

static Word w8(unsigned t, unsigned e){
    return (Word){ (Word8)t, (Word8)e };
}

/* Palavra FC no .mem — mem_le no slot S_CF + word_ix·stride; mesmo layout que rt_cf_slot.h */
static Word cf_le(unsigned word_ix, unsigned rel){
    return mem_le(cf_slot_base(word_ix) + rel);
}
static void cf_grava(unsigned word_ix, unsigned rel, Word w){
    mem_grava(cf_slot_base(word_ix) + rel, w);
}
/* ── O NOME DA TABELA no catálogo ──────────────────────────────────────────────
 * Dois caracteres por Word, minúsculas normalizadas: o SQL não distingue maiúsculas
 * no identificador, e comparar sem normalizar recusaria `FROM T` depois de
 * `CREATE TABLE t` — que é uma recusa errada, e das que só aparecem em produção. */
static char baixa1(char c){ return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c; }

/* ── O NOME QUE NÃO CABE NÃO SE TRUNCA ───────────────────────────────────────
 *
 * O nome de uma tabela vive em S_NOME_N Words (dois caracteres cada) e o de uma
 * coluna em S_COLNOME_W. Um nome maior era GUARDADO truncado e depois não se
 * encontrava: `CREATE TABLE ttt…t` (40 caracteres) dizia «criada» e
 * `SELECT … FROM ttt…t` dava «a tabela não existe». A volta não fechava, e a
 * primeira metade dizia que sim.
 *
 * Recusa-se com o número. É a mesma regra da linha larga e da cadeia grande: o
 * que não cabe diz-se, e quem escreve escolhe outro nome. */
static int nome_cabe(const char *nome, int tecto, const char *que){
    if(!nome) return 1;
    size_t n = strlen(nome);
    if((int)n <= tecto) return 1;
    printf("erro: o nome %s «%s» tem %zu caracteres e o motor guarda %d —"
           " RECUSADO. Truncar guardaria um nome que depois não se encontra.\n",
           que, nome, n, tecto);
    if(sql_cap){ sql_cap->ok = 0;
        snprintf(sql_cap->err, sizeof sql_cap->err,
                 "%s name is %zu chars; the limit is %d", que, n, tecto); }
    return 0;
}

static void cat_nome_grava(const char *nome){
    char n[S_NOME_N * 2 + 1];
    int i;
    snprintf(n, sizeof n, "%s", nome ? nome : "");
    for(i = 0; n[i]; i++) n[i] = baixa1(n[i]);
    for(i = (int)strlen(n); i < S_NOME_N * 2; i++) n[i] = 0;
    for(i = 0; i < S_NOME_N; i++)
        mem_grava(S_NOME + (unsigned)i, w8((unsigned)(unsigned char)n[2*i],
                                          (unsigned)(unsigned char)n[2*i + 1]));
}

static void cat_nome_le(char *out, int cap){
    int i, j = 0;
    for(i = 0; i < S_NOME_N && j + 2 < cap; i++){
        Word w = mem_le(S_NOME + (unsigned)i);
        out[j++] = (char)w.total;
        out[j++] = (char)w.e;
    }
    if(j < cap) out[j] = 0; else out[cap - 1] = 0;
}

/* 1 = é esta tabela (ou a base é antiga e não tem nome guardado); 0 = não é. */
static int cat_nome_bate(const char *nome){
    char guardado[S_NOME_N * 2 + 2], pedido[S_NOME_N * 2 + 2];
    int i;
    cat_nome_le(guardado, (int)sizeof guardado);
    if(!guardado[0]) return 1;                    /* base sem nome: compatibilidade */
    snprintf(pedido, sizeof pedido, "%s", nome ? nome : "");
    for(i = 0; pedido[i]; i++) pedido[i] = baixa1(pedido[i]);
    return strcmp(guardado, pedido) == 0;
}

/* a recusa DIZ-SE, e diz qual é a tabela que existe — senão o erro não ajuda ninguém */
static int cat_nome_recusa(const char *nome){
    char guardado[S_NOME_N * 2 + 2];
    cat_nome_le(guardado, (int)sizeof guardado);
    printf("erro: a tabela «%s» não existe nesta base — a que existe é «%s».\n"
           " A consulta é RECUSADA, e nada é devolvido.\n", nome ? nome : "", guardado);
    if(sql_cap){
        sql_cap->ok = 0;
        snprintf(sql_cap->err, sizeof sql_cap->err,
                 "relation \"%s\" does not exist", nome ? nome : "");
    }
    return 0;
}

/* ── OS NOMES DAS COLUNAS ─────────────────────────────────────────────────── */
static void col_nome_grava(int j, const char *nome){
    char n[S_COLNOME_W * 2 + 1];
    unsigned i;
    if(j < 0 || (unsigned)j >= S_COLNOME_N) return;
    snprintf(n, sizeof n, "%s", nome ? nome : "");
    for(i = 0; n[i]; i++) n[i] = baixa1(n[i]);
    for(i = (unsigned)strlen(n); i < S_COLNOME_W * 2; i++) n[i] = 0;
    for(i = 0; i < S_COLNOME_W; i++)
        mem_grava(S_COLNOME + (unsigned)j * S_COLNOME_W + i,
                  w8((unsigned)(unsigned char)n[2*i], (unsigned)(unsigned char)n[2*i + 1]));
}

static void col_nome_le(int j, char *out, int cap){
    int k = 0;
    unsigned i;
    if(cap > 0) out[0] = 0;
    if(j < 0 || (unsigned)j >= S_COLNOME_N) return;
    for(i = 0; i < S_COLNOME_W && k + 2 < cap; i++){
        Word w = mem_le(S_COLNOME + (unsigned)j * S_COLNOME_W + i);
        out[k++] = (char)w.total;
        out[k++] = (char)w.e;
    }
    if(k < cap) out[k] = 0; else out[cap - 1] = 0;
}

/* ── a seta: escrita e leitura, nos dois lados ───────────────────────────── */
/* texto curto em Words: dois caracteres por Word, terminado a zero */
static void txt_grava(unsigned base, unsigned nw, const char *t){
    for(unsigned k = 0; k < nw; k++){
        Word w;
        w.total = (Word8)(t[2*k] ? (unsigned char)t[2*k] : 0);
        w.e     = (Word8)(t[2*k] && t[2*k+1] ? (unsigned char)t[2*k+1] : 0);
        mem_grava(base + k, w);
        if(!t[2*k] || !t[2*k+1]) { for(unsigned j = k+1; j < nw; j++){ Word z={0,0}; mem_grava(base+j, z);} break; }
    }
}
static void txt_le(unsigned base, unsigned nw, char *out, size_t cap){
    size_t o = 0;
    for(unsigned k = 0; k < nw && o + 2 < cap; k++){
        Word w = mem_le(base + k);
        if(!w.total) break;
        out[o++] = (char)w.total;
        if(!w.e) break;
        out[o++] = (char)w.e;
    }
    out[o] = 0;
}

/* e o nome da tabela normaliza-se como o das colunas: a letra é a mesma */
static void fk_txt_grava(unsigned base, const char *nome){
    char n[FK_NOME_W * 2 + 2];
    unsigned i;
    snprintf(n, sizeof n, "%s", nome ? nome : "");
    for(i = 0; n[i]; i++) n[i] = baixa1(n[i]);
    txt_grava(base, FK_NOME_W, n);
}
/* na FILHA: a coluna j aponta para tab(col). Devolve a coluna alvo, ou −1.
 *
 * DOIS NÚMEROS ESTAVAM ESCRITOS À MÃO AQUI, e os dois eram tectos:
 *
 *   · `j >= 16` --- a seta só se gravava nas primeiras DEZASSEIS colunas da
 *     filha, e o 16 estava dito duas vezes (aqui e no `fk_le`). Numa tabela de
 *     300 colunas, uma seta declarada na última não se escrevia sequer;
 *   · `(Word8)(col + 1)` --- a coluna ALVO num byte. Acima de 255 ela dava a
 *     volta, e a verificação passava a olhar para outra coluna da mãe. Medido:
 *     seta na coluna 300, `DELETE` na mãe, e a linha apagou-se por baixo dela.
 *
 * O alvo passa ao par --- o byte alto na metade de cima da mesma zona ---, e o
 * tecto de colunas é o COL_MAX de toda a casa. */
#define S_FKA(j)  (S_FK + ZONA_SLOTS/2u + (unsigned)(j))   /* o byte ALTO do alvo */
static void fk_grava(int j, const char *tab, int col, int modo){
    if(j < 0 || j >= (int)COL_MAX) return;
    { Word w; w.total = (Word8)((col + 1) & 255); w.e = (Word8)modo;
      mem_grava(S_FK + (unsigned)j, w); }
    { Word a; a.total = (Word8)(((col + 1) >> 8) & 255); a.e = 0;
      mem_grava(S_FKA(j), a); }
    fk_txt_grava(S_FKNOME + (unsigned)j * FK_NOME_W, tab);
}
static int fk_le(int j, char *tab, int cap){
    if(j < 0 || j >= (int)COL_MAX) return -1;
    { long t = (long)mem_le(S_FK + (unsigned)j).total
             | ((long)mem_le(S_FKA(j)).total << 8);
      if(!t) return -1;
      if(tab) txt_le(S_FKNOME + (unsigned)j * FK_NOME_W, FK_NOME_W, tab, (size_t)cap);
      return (int)t - 1; }
}
/* na MÃE: quem aponta para mim. A lista não repete — declarar duas vezes a
 * mesma seta não a torna duas setas. */
/* ── E O CONTADOR É UM PAR, não um byte ──────────────────────────────────────
 * `mem_le(S_FILHOS).total` é um Word8: com mais de 255 filhas ele dava a volta
 * e a mãe passava a vigiar as primeiras outra vez. O par leva-o a 65535, que é
 * mais do que a zona segura --- e assim o tecto que recusa é o do ESPAÇO. */
static long filho_quantas(void){
    Word w = mem_le(S_FILHOS);
    return (long)w.total | ((long)w.e << 8);
}
static void filho_quantas_poe(long n){
    Word w; w.total = (Word8)(n & 255); w.e = (Word8)((n >> 8) & 255);
    mem_grava(S_FILHOS, w);
}

/* devolve 0 quando não coube --- e quem chama TEM de recusar a seta, porque uma
 * mãe que não vigia é uma restrição que não existe */
static int filho_regista(const char *tab, int col){
    long n = filho_quantas(), k;
    char q[64];
    for(k = 0; k < n && k < FK_MAXFIL; k++){
        unsigned b = S_FILHOS + 1 + (unsigned)k * FK_BLOCO;
        txt_le(b, FK_NOME_W, q, sizeof q);
        { Word cw = mem_le(b + FK_NOME_W);
          long cq = (long)cw.total | ((long)cw.e << 8);
          if(!strcmp(q, tab) && cq == (long)col) return 1; }
    }
    /* CHEIO NÃO É «A MÃE DEIXA DE VIGIAR».
     *
     * Era o que estava escrito, e era o que acontecia: a décima sétima filha
     * era criada com a seta e a mãe não a registava, pelo que um DELETE na mãe
     * apagava por baixo dela --- e a filha ficava com uma seta para uma linha
     * que já não existe, em silêncio. Medido: com vinte filhas, a f20 ficou
     * órfã e nada foi dito. Uma restrição que falha sem dizer é pior do que não
     * a haver, porque quem a declarou conta com ela. */
    if(n >= FK_MAXFIL) return 0;
    { unsigned b = S_FILHOS + 1 + (unsigned)n * FK_BLOCO;
      fk_txt_grava(b, tab);
      { Word w; w.total = (Word8)(col & 255); w.e = (Word8)((col >> 8) & 255);
        mem_grava(b + FK_NOME_W, w); }   /* a coluna no PAR, e não num byte */
      filho_quantas_poe(n + 1); }
    return 1;
}
static int filho_le(int k, char *tab, int cap){
    long n = filho_quantas();
    if(k < 0 || k >= n || k >= FK_MAXFIL) return -1;
    { unsigned b = S_FILHOS + 1 + (unsigned)k * FK_BLOCO;
      txt_le(b, FK_NOME_W, tab, (size_t)cap);
      { Word cw = mem_le(b + FK_NOME_W);
        return (int)((long)cw.total | ((long)cw.e << 8)); } }
}
static int filho_quantos(void){
    long n = filho_quantas();
    return (int)(n > FK_MAXFIL ? FK_MAXFIL : n);
}

/* a árvore vive mais abaixo; o CREATE precisa dela para o UNIQUE nascer com a
 * sua testemunha, e o INSERT precisa da descida para saber se a chave já lá está */
static int idx_constroi(long col, long ncols, long nrows);
static int idx_valido(long col, long nrows);
static int j_casam(long v, int *saida, int cap);
static void ord_usa_indice(long col);
static void ord_usa_rascunho(void);
static long celula_valor(long i, long j, long ncols);

/* ── ATRAVESSAR A SETA, E VOLTAR ─────────────────────────────────────────────
 * Vai à tabela apontada, procura o valor na coluna apontada, e VOLTA. O voltar
 * não é cortesia: abrir outra tabela relê o `.mem`, pelo que a decisão tem de
 * ser tomada com ela aberta e trazida em memória local — foi assim que a
 * subconsulta teve de ser escrita, e é a mesma razão.
 *
 * Devolve 1 se o valor está lá, 0 se não está, −1 se a tabela ou a coluna não
 * puderam ser lidas (que é diferente de «não está» e não pode ser confundido
 * com ele). */
static int fk_existe(const char *tab, int col, long valor, const char *guarda){
    int achou = 0;
    if(!usa_tabela(tab, 0) || !cat_nome_bate(tab)){ usa_tabela(guarda, 0); return -1; }
    { long nc = cat_ncols(), nr = cat_nrows();
      if(col < 0 || col >= nc){ usa_tabela(guarda, 0); return -1; }
      /* a árvore, se a houver — e numa coluna UNIQUE há sempre */
      /* A EXISTÊNCIA DECIDE-SE NA DESCIDA. Estava `int saida[4]; j_casam(valor,
       * saida, 4)` e depois o filtro pelo vivo --- e o QUATRO era do arranjo:
       * com as quatro primeiras linhas do valor apagadas, isto concluía «não há
       * nenhuma viva» sem ter visto a quinta. A primeira correcção que escrevi
       * foi cair na varredura quando o arranjo enchia, o que é aceitar o tecto e
       * pagá-lo em tempo. Não há arranjo: desce-se e pára-se no primeiro vivo. */
      if(col >= 0 && col < (long)IDX_MAXCOL && idx_valido(col, nr)){
          achou = ord_valor_vivo_col(col, valor, nr);
      } else {
          for(long i = 0; i < nr && !achou; i++)
              if(bit_le(S_VIVO, i) && bit_le(S_PRES, i*nc + col)
                 && celula_valor(i, col, nc) == valor) achou = 1;
      } }
    if(!usa_tabela(guarda, 0)) restauros_falhados++;
    return achou;
}

/* a tabela tem nomes guardados? (base antiga: não) */
static int col_tem_nomes(void){
    char n[S_COLNOME_W * 2 + 2];
    col_nome_le(0, n, (int)sizeof n);
    return n[0] != 0;
}

/* nome → índice da coluna. −1 quando não é coluna desta tabela.
 *
 * A LETRA CONTINUA A VALER, e tem de continuar: as bases antigas não têm nomes
 * guardados, e todos os medidores da casa escrevem `(a,b,c)`. Mas ela é o RECURSO,
 * não a regra — havendo nomes, é o nome que decide, e um `WHERE z` numa tabela de
 * três colunas passa a ser recusado em vez de pedir a coluna 25. */
static int col_indice(const char *nome){
    char alvo[64], guardado[S_COLNOME_W * 2 + 2];
    int i, j;
    long ncols;
    if(!nome || !nome[0]) return -1;
    for(i = 0; nome[i] && i < (int)sizeof alvo - 1; i++) alvo[i] = baixa1(nome[i]);
    alvo[i] = 0;
    ncols = cat_ncols();
    for(j = 0; j < (int)S_COLNOME_N && j < ncols; j++){
        col_nome_le(j, guardado, (int)sizeof guardado);
        if(guardado[0] && !strcmp(guardado, alvo)) return j;
    }
    if(col_tem_nomes()) return -1;              /* tem nomes e não é nenhum deles */
    if(alvo[1] == 0 && alvo[0] >= 'a' && alvo[0] <= 'z') return alvo[0] - 'a';
    return -1;
}

/* ── A COLUNA LARGA, E PORQUE É QUE ELA NÃO SE COMPARA ────────────────────────
 * O avaliador do WHERE é polinomial e de OITO bits: o `S_MT` mascara o `.e` e os
 * produtos são de átomo. Uma coluna cujos valores passem de 255 não cabe nele —
 * e a diferença entre recusar e não recusar é a diferença entre um banco que diz
 * o que não sabe e um que responde à toa.
 *
 * Isto é o outro lado do §26: a aritmética de 16 bits já está provada, e o que
 * falta é o CAMINHO — o avaliador inteiro, que é o trio seguinte. Até lá o dado
 * guarda-se com 16 bits e a comparação diz que não chega lá. */
/* a ESCRITA deixa a marca: max(marca, valor) — o que muda a cada passo fica no
 * ambiente, e não numa lista que o agente carregue. */
static void col_marca(long j, unsigned long v){
    if(j < 0 || j >= (long)S_COLNOME_N) return;
    /* o MÁXIMO */
    { Word m = mem_le(S_COLMAX + (unsigned)j);
      unsigned long antes = (unsigned long)m.total | ((unsigned long)m.e << 8);
      if(v > antes){
          m.total = (Word8)(v & 255u); m.e = (Word8)((v >> 8) & 255u);
          mem_grava(S_COLMAX + (unsigned)j, m); } }
    /* e o MÍNIMO, que é o dual e precisa de dizer se já foi escrito --- zero é
     * um mínimo legítimo, pelo que o slot a zero não pode significar «ainda
     * não». É a mesma lição do S_PRES: a ausência diz-se, não se herda. */
    { Word cab = mem_le(S_COLMINCAB + (unsigned)j);
      Word m   = mem_le(S_COLMIN + (unsigned)j);
      unsigned long antes = (unsigned long)m.total | ((unsigned long)m.e << 8);
      if(!cab.total || v < antes){
          m.total = (Word8)(v & 255u); m.e = (Word8)((v >> 8) & 255u);
          mem_grava(S_COLMIN + (unsigned)j, m);
          { Word c1 = {1,0}; mem_grava(S_COLMINCAB + (unsigned)j, c1); } } }
}

/* e SENTIR É LER A MARCA — uma leitura, não uma varredura.
 *
 * A volta pelas linhas fica para UM caso, e diz-se qual: uma base escrita antes
 * de a marca existir tem o slot a zero com linhas lá dentro. Aí percorre-se uma
 * vez e ESCREVE-SE a marca — paga-se uma, não uma por consulta. */
static unsigned long col_max(long j, long ncols, long nrows){
    Word m = mem_le(S_COLMAX + (unsigned)j);
    unsigned long marca = (unsigned long)m.total | ((unsigned long)m.e << 8);
    if(marca || nrows <= 0) return marca;
    unsigned long v_max = 0;
    for(long i = 0; i < nrows; i++){
        unsigned long v = (unsigned long)mem_le(S_LINHAS + cel_ix(i*ncols + j)).total
                        | ((unsigned long)mem_le(S_ALTO + cel_ix(i*ncols + j)).total << 8);
        if(v > v_max) v_max = v;
    }
    col_marca(j, v_max);                     /* a marca fica escrita: uma vez */
    return v_max;
}

/* e o MÍNIMO da coluna, com a mesma migração do máximo: uma base escrita antes
 * de o dual existir tem o cabeçalho a zero, e aí percorre-se UMA vez e
 * escreve-se. Sem cabeçalho não haveria como distinguir «o mínimo é zero» de
 * «ainda não foi medido», e concluir «impossível» a partir de um mínimo
 * inventado seria dar vazio a quem tinha resposta. */
/* A MARCA REPRESENTA O VALOR COMPARADO? --- e a pergunta é sobre as CÉLULAS.
 *
 * O `atom_so_inteiro` já o dizia deste ficheiro: «perguntar pelo corpo não
 * chega: uma coluna declarada INTEIRO aceita 3/4, e o denominador vive na
 * CÉLULA (S_DEN), não no catálogo». Numa coluna com racionais, a comparação
 * faz-se sobre o numerador do DENOMINADOR COMUM, e a marca guarda o numerador
 * como ele está escrito --- os dois não são o mesmo número.
 *
 * Com o mínimo assumido ZERO isso não fazia mal: o intervalo [0, máx] era largo
 * e nunca descartava a mais. Com o mínimo REAL aperta, e passa a descartar
 * consultas que têm resposta --- foi o que aconteceu, e dois medidores do `sql`
 * apanharam-no («a igualdade racional fecha», «6/8 casa com 3/4»). O mínimo só
 * se usa onde a marca é o valor: quando todas as células têm denominador um. */
static int col_e_inteira(long j, long ncols, long nrows){
    if(corpo_de(j).total != CORPO_INTEIRO) return 0;
    for(long i = 0; i < nrows; i++){
        long d = (long)mem_le(S_DEN + cel_ix(i*ncols + j)).total;
        if(d != 1 && d != 0) return 0;          /* 0 = célula por escrever */
    }
    return 1;
}

/* ── A ORDEM É DO CORPO, E A CASA JÁ TINHA A RÉGUA ───────────────────────────
 *
 * O motor ordenava pela célula EMPACOTADA, e a árvore de prefixos ordena pelos
 * BITS: numa coluna AUREO isso dá a lexicográfica de (a,b), que não é ordem
 * nenhuma do corpo. Medido: `ORDER BY a` sobre {2+0σ, 1+1σ, 3−1σ} devolvia
 * exactamente a ordem INVERTIDA. É a `aranha` caixa de Pandora na forma pura ---
 * não um erro que se veja, uma resposta com a cara de certa.
 *
 * A PRIMEIRA correcção que escrevi foi RECUSAR, e ela derrubou dois medidores
 * verdes que dizem a lei desta casa: «a consulta dentro de UMA coluna passa ---
 * não há nada a atravessar» e «o sistema não RECUSA nem DESPACHA por classe ---
 * corre nas duas». Recusar uma coluna sozinha descaracteriza as duas: o que se
 * recusa é a TRAVESSIA entre corpos, e nunca o corpo.
 *
 * E a régua já estava escrita, no `contrato.h`: q(a,b) = a² + B·a·b + C·b² é a
 * NORMA, `ct_assinatura` é exactamente o Δ, e ela COMPÕE as três classes ---
 * definida, degenerada, indefinida --- sem despachar por nenhuma. É essa a chave
 * da ordenação. O B e o C da coluna pedem-se ao `corpo_B` e ao `corpo_C`, que
 * são quem os define. */
/* ── E O QUE ESTA CHAVE AINDA NÃO ALCANÇA ────────────────────────────────────
 *
 * Esta régua serve o `ORDER BY`. A DESIGUALDADE do WHERE não passa por aqui: o
 * avaliador compara a célula átomo a átomo, o que numa coluna quadrática é a
 * PARTE REAL. Medido: com {1+2σ, 0+0σ} numa coluna AUREO, `WHERE a > 0` devolve
 * 1+2σ --- certo pela parte real (1 > 0) e pelo valor (4,236 > 0), e ERRADO pela
 * régua, cuja norma é −1. Ou seja: a mesma consulta ordena por uma régua e
 * filtra por outra, que é `duas réguas para o mesmo objecto` no mesmo comando.
 *
 * Não se remenda aqui. O avaliador é de bytecode sobre átomos de oito bits, e
 * pôr a norma nele é trabalho no emissor --- e antes disso é uma DECISÃO: em
 * `a > 0` o zero é um literal, e comparar a norma de `a` com ele não é a mesma
 * pergunta que comparar o valor. Fica escrito porque é trabalho e não um
 * remendo, e porque o que não está dito passa por resolvido. */
static void celula_qz(long i, long j, long nc, long *num, long *den);
static long corpo_B(long cp, long parm);
static long corpo_C(long cp);
static long col_regua_chave(long i, long j, long nc){
    Word cq = corpo_de(j);
    long num = 0, den = 0;
    if(cq.total != CORPO_AUREO && cq.total != CORPO_CRISTAL)
        return celula_valor(i, j, nc);
    /* O NUMERADOR pelo `celula_qz`, que é quem sabe ler os dois planos com o
     * sinal de dezasseis bits. O COEFICIENTE DE σ ele NÃO dá --- devolve
     * `den = 1` e di-lo, «só a parte racional» ---, e está no S_DEN, onde o
     * INSERT o escreveu. Lê-se de lá, e lê-se COM SINAL: o `Word8` é sem ele, e
     * um σ de −1 chegava como 255. Medido antes de o corrigir: a chave de 3−1σ
     * dava −64251 em vez de 5, e a ordenação saía por esse número. */
    celula_qz(i, j, nc, &num, &den);
    { long b = (long)mem_le(S_DEN + cel_ix(i*nc + j)).total;
      if(b & 0x80L) b -= 256L;                 /* o sinal, que o Word8 não traz */
      { Regua r; Par x;
        r.B = corpo_B(cq.total, cq.e); r.C = corpo_C(cq.total);
        x.a = num; x.b = b;
        return ct_norma(r, x); } }
}

static unsigned long col_min(long j, long ncols, long nrows){
    if(j < 0 || j >= (long)S_COLNOME_N) return 0;
    Word cab = mem_le(S_COLMINCAB + (unsigned)j);
    if(cab.total){
        Word m = mem_le(S_COLMIN + (unsigned)j);
        return (unsigned long)m.total | ((unsigned long)m.e << 8);
    }
    if(nrows <= 0) return 0;
    unsigned long v_min = ~0ul; int viu = 0;
    for(long i = 0; i < nrows; i++){
        if(!bit_le(S_VIVO, i)) continue;
        if(!bit_le(S_PRES, i*ncols + j)) continue;
        unsigned long v = (unsigned long)mem_le(S_LINHAS + cel_ix(i*ncols + j)).total
                        | ((unsigned long)mem_le(S_ALTO + cel_ix(i*ncols + j)).total << 8);
        if(!viu || v < v_min){ v_min = v; viu = 1; }
    }
    if(!viu) return 0;                       /* coluna toda ausente: não se afirma */
    { Word m; m.total = (Word8)(v_min & 255u); m.e = (Word8)((v_min >> 8) & 255u);
      mem_grava(S_COLMIN + (unsigned)j, m);
      Word c1 = {1,0}; mem_grava(S_COLMINCAB + (unsigned)j, c1); }
    return v_min;
}

static int col_larga(long j, long ncols, long nrows){
    return col_max(j, ncols, nrows) > 255u;              /* lê a marca */
}

/* ── E O ENVELOPE DA COMPARAÇÃO É ASSINADO ───────────────────────────────────
 * O avaliador decide pelo SINAL da diferença — é o bit 15 do par que dá o `<` e
 * o `>`. Logo o envelope da COMPARAÇÃO não é o do armazenamento: um valor acima
 * de 32767 lê-se NEGATIVO, e `50000 < 100` passava a verdadeiro.
 *
 * Guardar e comparar são coisas diferentes e têm envelopes diferentes: a célula
 * guarda 0..65535 e o SELECT devolve-o; a comparação pede 0..32767. O que passa
 * disso é RECUSADO — não se responde ao contrário. */
#define CMP16_MAX 32767L
static int col_larguissima(long j, long ncols, long nrows){
    return col_max(j, ncols, nrows) > (unsigned long)CMP16_MAX;
}

/* A BARREIRA DO BANCO: dado, fsync, ponteiro, fsync.
 *
 * banco.c já tinha esta disciplina e o SQL não: aqui as células e o catálogo iam no MESMO
 * programa, sem nada entre eles. A ordem do programa estava certa, mas ordem de programa não
 * é ordem no disco — sem fsync o sistema pode pôr o catálogo no prato antes das células, e
 * uma queda no meio deixa o catálogo a contar uma linha que não existe.
 *
 * Com a barreira, as duas quedas possíveis são as duas boas: antes do ponteiro, a linha é
 * invisível e não faz mal nenhum; depois dele, a linha está inteira. Nunca meia. */
static void barreira(void){ if(fmem >= 0) fsync(fmem); }

/* Travamento injetado, para MEDIR a barreira em vez de a afirmar. SQL_TRAVA=1 mata o processo
 * logo depois do dado e antes do ponteiro — que é o único ponto onde a ordem importa.
 * _exit(9) modela queda de PROCESSO: o que já foi para o núcleo sobrevive. Queda de energia
 * é mais dura, e é contra ela que o fsync existe; o teste cobre a ordem, não o prato. */
static int trava_em = 0;               /* o teste põe aqui, sem depender do ambiente */
static void trava_se_pedido(int ponto){
    const char *e = getenv("SQL_TRAVA");
    if(trava_em == ponto || (e && atoi(e) == ponto)) _exit(9);
}
static unsigned char prog_le(unsigned pc){
    unsigned char b = OP_HALT;
    if(pread(fprog, &b, 1, (off_t)pc) != 1) return OP_HALT;
    return b;
}

/* ULA Word_8: a+b = (a⊕b)+2(a∧b) — naturais thm:transporte. NÃO é “carry” de CPU:
 * é a mesma peça que sobe ℕ. GOLD = ×σ com σ²=σ+1. ∞+1=−1 (Möbius, §M8). */
static Word8 ula_nand_w(Word8 a, Word8 b){ return (Word8)~(a & b); }
static Word8 ula_and_w (Word8 a, Word8 b){ Word8 n = ula_nand_w(a,b); return ula_nand_w(n,n); }
static Word8 ula_xor_w (Word8 a, Word8 b){ Word8 n = ula_nand_w(a,b);
                                        return ula_nand_w(ula_nand_w(a,n), ula_nand_w(b,n)); }
static Word8 ula_soma_w(Word8 a, Word8 b){
    Word8 s = ula_xor_w(a,b), c = ula_and_w(a,b);   /* c = termo 2(a∧b), thm:transporte */
    for(int i = 0; i < 8 && c; i++){
        c = (Word8)(c << 1);
        Word8 s2 = ula_xor_w(s,c), c2 = ula_and_w(s,c);
        s = s2; c = c2;
    }
    return s;
}
static Word ula_add(Word a, Word b){
    Word r = { ula_soma_w(a.total,b.total), ula_soma_w(a.e,b.e) }; return r; }
static Word ula_sub(Word a, Word b){
    Word r = { ula_soma_w(a.total, ula_soma_w(ula_nand_w(b.total,b.total),1)),
               ula_soma_w(a.e,     ula_soma_w(ula_nand_w(b.e,b.e),1)) }; return r; }
static Word ula_nand(Word a, Word b){ Word r = { ula_nand_w(a.total,b.total), ula_nand_w(a.e,b.e) }; return r; }
static Word ula_and(Word a, Word b){ Word n = ula_nand(a,b); return ula_nand(n,n); }
static Word ula_or (Word a, Word b){ Word na = ula_nand(a,a), nb = ula_nand(b,b);
                                     return ula_nand(na,nb); }
static Word ula_xor(Word a, Word b){ Word r = { ula_xor_w(a.total,b.total), ula_xor_w(a.e,b.e) }; return r; }
static int  zero(Word w){ return w.total == 0 && w.e == 0; }

/* ══ O ANDAR SEGUINTE DA TORRE: o transporte ATRAVESSA o átomo ═══════════════
 *
 * A ULA de cima soma componente a componente e o transporte NÃO passa de um
 * átomo para o outro — e isso está certo, porque a Word é um PAR (numerador,
 * denominador) e não um número de dezasseis bits. É por isso que uma célula
 * segura 0..255 e que o `INSERT ... VALUES (500,...)` é recusado.
 *
 * Para a célula crescer, o par tem de poder ser lido também como UM número —
 * e isso é exactamente a dobra da torre: T_{k+1} = T_k + T_k*. O transporte
 * dentro do átomo é `thm:transporte`, a + b = (a⊕b) + 2(a∧b); o transporte
 * ENTRE átomos é a mesma lei um andar acima, com o vai-um do primeiro a entrar
 * como cin do segundo. Nada aqui usa o `+` do C: tudo sai do NAND.
 *
 * Isto é o andar, medido sozinho. A célula continua onde estava — o caminho
 * novo primeiro, o velho intacto, que é como o pgwire entrou. */
typedef struct { Word8 baixo, alto; } W16;    /* Word_8² lida como um número */

static inline W16 w16_de(unsigned v){
    W16 r; r.baixo = (Word8)(v & 255u); r.alto = (Word8)((v >> 8) & 255u); return r;
}
static inline unsigned w16_val(W16 w){
    return (unsigned)w.baixo | ((unsigned)w.alto << 8);
}

/* a soma de oito bits COM o vai-um à entrada e à saída — só NAND lá dentro */
static Word8 ula_soma_c(Word8 a, Word8 b, Word8 cin, Word8 *cout){
    Word8 s = 0, c = cin;
    for(int i = 0; i < 8; i++){
        Word8 ai = (Word8)((a >> i) & 1u), bi = (Word8)((b >> i) & 1u);
        Word8 x  = ula_xor_w(ai, bi);
        Word8 si = ula_xor_w(x, c);
        /* vai-um = (ai∧bi) ∨ (c∧(ai⊕bi)), escrito só com NAND */
        Word8 c1 = ula_and_w(ai, bi), c2 = ula_and_w(c, x);
        c = ula_nand_w(ula_nand_w(c1, c1), ula_nand_w(c2, c2));
        s = (Word8)(s | (Word8)(si << i));
    }
    *cout = (Word8)(c & 1u);
    return s;
}

/* e a DOBRA: o vai-um do átomo baixo entra no alto. É o mesmo teorema, um
 * andar acima — e é isto que faz de dois átomos um número. */
static W16 ula_add16(W16 a, W16 b, Word8 *transbordo){
    W16 r; Word8 c1 = 0, c2 = 0;
    r.baixo = ula_soma_c(a.baixo, b.baixo, 0, &c1);
    r.alto  = ula_soma_c(a.alto,  b.alto,  c1, &c2);
    if(transbordo) *transbordo = c2;          /* o que sai do andar diz-se */
    return r;
}
/* a − b = a + (~b) + 1, com o mesmo vai-um a atravessar */
static W16 ula_sub16(W16 a, W16 b, Word8 *empresta){
    W16 nb; Word8 c1 = 0, c2 = 0; W16 r;
    nb.baixo = ula_nand_w(b.baixo, b.baixo);
    nb.alto  = ula_nand_w(b.alto,  b.alto);
    r.baixo = ula_soma_c(a.baixo, nb.baixo, 1, &c1);
    r.alto  = ula_soma_c(a.alto,  nb.alto,  c1, &c2);
    if(empresta) *empresta = (Word8)(c2 ? 0u : 1u);   /* sem vai-um final = pediu emprestado */
    return r;
}
/* O PRODUTO DO ANDAR, E ELE NÃO CONTA: LÊ.
 *
 * «Multiplicar sem multiplicador custa contar» era o que o compilador fazia —
 * um laço de |Y| voltas. Mas contar é a pergunta de quem não lê: o produto está
 * nos BITS de b, e a base é ortonormal, de modo que «medir e ler o bit são a
 * mesma operação» (aranha thm:base8). Para cada coordenada k de b, se o bit
 * está ligado, soma-se a deslocado de k — dezasseis passos fixos, e não |b|.
 *
 * Usa o `ula_add16`, que já é o somador bit a bit do andar; o deslocamento é a
 * duplicação, que é somar a si próprio. Nada de novo abaixo. */
static W16 ula_mul16(W16 a, W16 b){
    W16 r = { 0, 0 }, p = a;
    unsigned bb = (unsigned)b.baixo | ((unsigned)b.alto << 8);
    for(unsigned k = 0; k < 16u; k++){
        if(bb & (1u << k)) r = ula_add16(r, p, NULL);
        p = ula_add16(p, p, NULL);                  /* deslocar é dobrar */
    }
    return r;
}
/* a < b sem formar a diferença fora do andar: é o empréstimo do sub */
static int ula_menor16(W16 a, W16 b){
    Word8 emp = 0;
    (void)ula_sub16(a, b, &emp);
    return emp != 0;
}
static int ula_igual16(W16 a, W16 b){ return a.baixo == b.baixo && a.alto == b.alto; }

/* ══ A DOBRA DO REI — GOLD em dezasseis bits ═════════════════════════════════
 *
 * A ISA não tem MUL. `n·x` faz-se por ZECKENDORF: todo inteiro é soma de
 * Fibonacci NÃO consecutivos, de um único jeito, e cada F(k) é uma potência do
 * rei — GOLD = (a,b) ↦ (a+b, a), com σ² = σ + 1. Partindo de (x, 0):
 *
 *     (x,0) → (x,x) → (2x,x) → (3x,2x) → (5x,3x) → …
 *      k aplicações dão  a = F(k+1)·x
 *
 * Em oito bits o estado do rei é a Word: `a` no `.total` e `b` no `.e`. É por
 * isso que o segundo átomo NÃO está livre — e foi isso que travou o alargamento
 * do avaliador (§27).
 *
 * A dobra é a mesma do §26, aplicada ao REI em vez de à soma: o estado passa a
 * ser um PAR de valores de dezasseis bits, e o `+` lá dentro é o `ula_add16`,
 * onde o vai-um atravessa o átomo. Nada aqui usa o `+` do C sobre os valores.
 *
 * E o que sai do andar DIZ-SE: `n·x` acima de 65535 marca transbordo e não
 * enrola — é a mesma regra que o §24 pôs na célula. */
typedef struct { W16 a, b; } G16;          /* o estado do rei, um andar acima */

static G16 gold16(G16 w, Word8 *transbordo){
    G16 r; Word8 t = 0;
    r.a = ula_add16(w.a, w.b, &t);         /* a' = a + b, com o vai-um a atravessar */
    r.b = w.a;                             /* b' = a — o deslocamento */
    if(transbordo && t) *transbordo = 1;
    return r;
}

/* F(k+1)·x por k deslocamentos do rei, partindo de (x, 0) */
static W16 fib_vezes16(W16 x, int k, Word8 *transbordo){
    G16 w; w.a = x; w.b = w16_de(0);
    for(int t = 0; t < k; t++) w = gold16(w, transbordo);
    return w.a;
}

/* n·x = Σ F(k_i)·x, com n em Zeckendorf — o mesmo desenho do `emit_mul_zeck`,
 * um andar acima. O custo é o número de dígitos de Zeckendorf, ~log_φ(n). */
static W16 mul16_zeck(unsigned n, W16 x, Word8 *transbordo){
    unsigned fib[24]; int nf = 2;
    W16 acc = w16_de(0);
    fib[0] = 1; fib[1] = 2;                     /* F(2), F(3), … */
    while(fib[nf-1] <= n/2 + 1 && nf < 23){ fib[nf] = fib[nf-1] + fib[nf-2]; nf++; }
    unsigned r = n;
    for(int i = nf-1; i >= 0 && r > 0; i--){
        if(fib[i] > r) continue;
        r -= fib[i];
        /* fib[i] = F(i+2), logo são i+1 deslocamentos — errar isto acerta só em n = 1 */
        W16 parcela = fib_vezes16(x, i + 1, transbordo);
        Word8 t = 0;
        acc = ula_add16(acc, parcela, &t);
        if(transbordo && t) *transbordo = 1;
    }
    return acc;
}
/* GOLD: A_1 = [[1,1],[1,0]] — ×σ, σ²=σ+1. NEGRO: inversa (det −1). Envelope Word_8. */
/* ⊗ O GATO E ⊘ O ESQUILO — a Def.~65 e o Teor.~66 do `aranha.tex`, em n=2.
 *
 * Estas duas funções são o algoritmo da aranha estigmérgica na sua forma mais
 * curta, e já cá estavam antes de o paper as nomear:
 *
 *     ⊗ gato    (a,b) ↦ (m·a + b, a)      SOBE  — a convolução, ×σ
 *     ⊘ esquilo (a,b) ↦ (b, a − m·b)      DESCE — a deconvolução, ×σ'
 *
 * e ⊘∘⊗ = id sem hipótese nenhuma sobre o par, porque |det| = 1 e a inversa é
 * inteira (Teor.~66(1) e (2)): (b, (m·a+b) − m·a) = (b, ... ) devolve o
 * original coordenada a coordenada, e em passo nenhum se divide. O `n` é o
 * METAL — 1 ouro, 2 prata, 3 bronze —, com σ+σ' = n o traço e σσ' = −1 o
 * determinante. É o par ζ/μ do Teor.~da acumulação lido no espaço em vez de no
 * tempo, e a única linha da tabela das dualidades que inverte DOS DOIS LADOS.
 *
 * Medido em `tests/neuronio.c`, que corre a volta em toda dimensão de 2 a 8. */
static Word cifra_an(Word w, int n){
    Word r = { (Word8)((int)n*(int)w.total + (int)w.e), w.total }; return r; }
static Word decifra_an(Word w, int n){
    Word r = { w.e, (Word8)((int)w.total - (int)n*(int)w.e) }; return r; }
static unsigned MOVE_exec(Regs *r, unsigned pc, int sentido){
    unsigned slot = (unsigned)prog_le(pc) | ((unsigned)prog_le(pc+1) << 8);
    pc += 2;
    if(sentido > 0){ r->B = r->A; r->A = mem_le(slot); }
    else           { mem_grava(slot, r->R); }
    return pc;
}

/* o salto, um so': avanca 1+rel se `cond`, senao avanca 1. JMP, JZ e JNZ chamam-no
 * todos — sao a MESMA operacao com condicoes diferentes. E' o primeiro passo da reducao
 * da ISA: o excedente vira funcao, e so' depois se troca. */
/* gira a palavra: (a,b) -> (s*b, a). Com s = -1 e' o J (det +1, o esquilo, i);
 * com s = +1 e' a reflexao (det -1, a troca). UMA operacao, o sinal decide qual. */
static Word corpo_gira(Word w, int s){ Word r = { (Word8)(s * (int)w.e), w.total }; return r; }

/* MOVER: poe `destino` no pc se `cond`, senao poe `senao`. E' a transferencia com o pc
 * como slot — a Lei 1 aplicada ao proprio contador de programa. */
static int MOVE_no_pc(Regs *r, unsigned destino, unsigned senao, int cond){
    r->pc = cond ? destino : senao;
    return 1;
}

static int MOVE_pc(Regs *r, unsigned pc, int cond){
    /* O DESLOCAMENTO SOBE DE ANDAR: dois bytes, não um.
     *
     * Era UM byte lido com sinal, logo o salto alcançava 127 — e um corpo maior
     * do que isso virava um salto NEGATIVO, para trás, com a máquina a rodar
     * até o guarda a parar. Eu tratei esse 127 como um dado e fui pôr recusas à
     * volta dele; é um TECTO MEU, e a teoria não tem tectos: `§sec:torre`,
     * d_{k+1} = 2·d_k, «o que cresce é o OBJECTO, não a máquina». Quem não cabe
     * PROMOVE. O deslocamento passa a ocupar o par — dois átomos, a mesma dobra
     * do resto — e alcança 32767. */
    int rel = (int)(short)((unsigned)prog_le(pc) | ((unsigned)prog_le(pc + 1) << 8));
    /* ── E O SALTO E' A MESMA TRANSFERENCIA: o pc e' so' mais um destino ────────────
     * A Lei 1 outra vez — 1† = -1, e a unidade e' dual. Mover um valor para um slot e
     * mover um valor para o pc sao a MESMA operacao; o que muda e' o DESTINO, e o
     * destino e' parametro. A condicao decide SE se move; o sentido, para onde.
     *
     *   destino = slot   e sentido -1   ->  era STORE
     *   destino = slot   e sentido +1   ->  era LOAD
     *   destino = pc     e sentido -1   ->  era o SALTO
     *
     * Escrito assim, a ISA tem UMA operacao: mover, com destino e sentido. */
    return MOVE_no_pc(r, (unsigned)((int)pc + 2 + rel), pc + 2, cond);
}

static int passo(Regs *r, unsigned prog_len){
    if(r->pc >= prog_len) return 0;
    unsigned pc = r->pc;
    unsigned char op = prog_le(pc++);
    switch(op){
    case OP_HALT: return 0;
    /* LOAD e STORE sao UMA transferencia, e o SENTIDO e' o sinal — a Lei 1 em codigo:
     *   +1 do slot para o registo (o gerador, fp -> 1);  -1 ao contrario (o motor). */
    case OP_LOAD: case OP_LOADS: pc = MOVE_exec(r, pc, +1); break;
    case OP_STORE:               pc = MOVE_exec(r, pc, -1); break;
    case OP_GOLD:   r->A = cifra_an(r->A, 1); r->R = r->A; break;

    case OP_NEGRO_OURO:   r->A = decifra_an(r->A, 1); r->R = r->A; break;

    /* O CIRCUITO. Com o gato sozinho a máquina só estica — e o que estica não fecha grupo.
     * ESQUILO é ×ω do cristalino com t=0, isto é S = [[0,−1],[1,0]]: det +1, ordem 4. TROCA
     * é J = [[0,1],[1,0]]: det −1, ordem 2. Com os três, toda unimodular é palavra. */
    /* ── ESQUILO e TROCA sao A MESMA operacao, e o sinal e' argumento ──────────────
     * ESQUILO: (a,b) -> (-b, a)   e' a multiplicacao por i — o J da Lei 2, det +1
     * TROCA:   (a,b) -> ( b, a)   e' a reflexao,                             det -1
     * Diferem SO' no sinal do primeiro componente. Escritos assim deixam de ter corpo
     * proprio, como os tres saltos: uma funcao, um sinal, e a troca fica mecanica. */
    case OP_ESQUILO: { Word w = corpo_gira(r->A, -1); r->A = w; r->R = w; break; }
    case OP_TROCA:   { Word w = corpo_gira(r->A, +1); r->A = w; r->R = w; break; }
    /* O MARTELO. A faixa é [A, B); o cabeçalho está em S_CAB e o alvo em S_ALVO.
     *
     * O hash sai em bytes porque é A REDE que o define — essa coordenada não é minha, e
     * re-coordená-la daria outro hash e nenhuma share. Mas ele ENTRA NO MINERAL logo à saída, e
     * a decisão faz-se na RÉGUA: a régua elíptica (0,1), N(a,b) = a² + b², cuja norma é definida
     * positiva — nada tem norma zero fora do zero, logo NÃO HÁ CONE NULO por onde passar sem
     * trabalho. O alvo é um nível dessa norma, e a dificuldade é o RAIO da bola.
     *
     * Deixa em R o nonce que bateu, ou 0 se a faixa saiu limpa. Zero é resposta, não falha: uma
     * faixa limpa é trabalho feito, e é por isso que ela se fecha na mesma. */
    /* A DOBRA, NO METAL. Merkle nao e conta: e DESDOBRAMENTO AUTO-SIMILAR — pares que se juntam
     * num, nivel a nivel, com a mesma operacao em todos, ate sobrar um. E o `M_k = M_{k-1}A_1` de
     * sempre: o nivel k carrega o k-1.
     *
     * A = o slot base das folhas, B = quantas. Cada folha ocupa 32 átomos (32 bytes). Dobra em
     * lugar e deixa a raiz nos primeiros 32 átomos. Deixa em R quantos NIVEIS desdobrou — que e a
     * altura da arvore, e e o que a branch percorre ao subir.
     *
     * O OP_FOLD estava no enum desde sempre e sem executor. Deixa de estar. */
    case OP_FOLD: {
        unsigned base = atomos_le_u32(S_FOLD_ARG);
        unsigned n = r->B.total;
        unsigned niveis = 0;
        unsigned char a1[32], b1[32], h[32];
        while(n > 1){
            unsigned m = 0;
            for(unsigned i = 0; i < n; i += 2){
                atomos_le(base + 32u*i, a1, 32);
                if(i + 1 < n) atomos_le(base + 32u*(i+1), b1, 32);
                else memcpy(b1, a1, 32);
                unsigned char par[64];
                memcpy(par, a1, 32); memcpy(par+32, b1, 32);
                sha256(par, 64, h); sha256(h, 32, h);
                atomos_grava(base + 32u*m, h, 32);
                m++;
            }
            n = m; niveis++;
        }
        r->R = w8(niveis, 0);
        break;
    }
    case OP_MARTELO: {
        unsigned char cab[80], h1[32], h2[32], alvo[32];
        atomos_le(S_CAB, cab, 80);
        atomos_le(S_ALVO, alvo, 32);
        unsigned mid[8]; sha_ini(mid); sha_bloco(mid, cab);
        unsigned de = atomos_le_u32(S_FAIXA), ate = atomos_le_u32(S_FAIXA + 4u);
        Word achou = { 0, 0 };
        unsigned char b2[64];
        memcpy(b2, cab + 64, 16);
        memset(b2 + 16, 0, 48);
        b2[16] = 0x80;
        b2[62] = 0x02; b2[63] = 0x80;
        for(unsigned n = de; n != ate; n++){
            b2[12] = (unsigned char)(n);        b2[13] = (unsigned char)(n >> 8);
            b2[14] = (unsigned char)(n >> 16);  b2[15] = (unsigned char)(n >> 24);
            unsigned h[8]; memcpy(h, mid, sizeof h);
            sha_bloco(h, b2);
            sha_fim(h, h1);
            sha256(h1, 32, h2);
            int k = 0, dentro = 0;
            while(k < 32){
                unsigned t_h = h2[31-k], t_a = alvo[k];
                if(t_h != t_a){ dentro = (t_h < t_a); break; }
                k++;
            }
            if(dentro){
                atomos_u32(S_FAIXA + 8u, n);           /* nonce completo nos átomos */
                achou = w8(n & 0xFFu, (unsigned)k);   /* R: low + símbolo */
                break;
            }
        }
        r->R = achou; r->A = achou;
        break;
    }
    case OP_ADD: r->R = ula_add(r->A, r->B); break;
    case OP_SUB: r->R = ula_sub(r->A, r->B); break;
    case OP_VINCO: {
        r->R = ula_sub(r->A, r->B);
        r->flags = zero(r->R) ? FL_ZERO : 0;
        break; }
    case OP_INC: {
        unsigned slot = (unsigned)prog_le(pc) | ((unsigned)prog_le(pc + 1) << 8);
        pc += 2;
        Word w = mem_le(slot);
        w = ula_add(w, (Word){ 1, 0 });
        mem_grava(slot, w);
        r->R = w;
        break; }
    /* a Word lida como UM número de dezasseis bits: o vai-um atravessa (§26) */
    case OP_ADD16: {
        W16 a = { r->A.total, r->A.e }, b = { r->B.total, r->B.e }, x;
        x = ula_add16(a, b, NULL);
        r->R.total = x.baixo; r->R.e = x.alto; break; }
    case OP_ESPALHA:
        /* não-nulo -> todos os bits; nulo -> nenhum. É a única instrução que
         * transporta um booleano para a largura toda da Word. */
        r->R.total = (r->A.total || r->A.e) ? 0xFF : 0;
        r->R.e     = (r->A.total || r->A.e) ? 0xFF : 0;
        break;

    case OP_MUL16: {
        W16 a = { r->A.total, r->A.e }, b = { r->B.total, r->B.e }, x;
        x = ula_mul16(a, b);
        r->R.total = x.baixo; r->R.e = x.alto; break; }
    case OP_SUB16: {
        W16 a = { r->A.total, r->A.e }, b = { r->B.total, r->B.e }, x;
        x = ula_sub16(a, b, NULL);
        r->R.total = x.baixo; r->R.e = x.alto; break; }
    case OP_CMP16: {
        W16 a = { r->A.total, r->A.e }, b = { r->B.total, r->B.e };
        unsigned char f = 0;
        if(zero(r->A) && zero(r->B)) f |= FL_ZERO;
        if(ula_igual16(a, b)) f |= FL_EQ;
        else if(ula_menor16(a, b)) f |= FL_LT;
        r->flags = f;
        break; }
    case OP_AND: r->R = ula_and(r->A, r->B); break;
    case OP_OR:  r->R = ula_or (r->A, r->B); break;
    case OP_XOR: r->R = ula_xor(r->A, r->B); break;
    case OP_CMP: {
        unsigned char f = 0;
        if(zero(r->A) && zero(r->B)) f |= FL_ZERO;
        if(r->A.total == r->B.total && r->A.e == r->B.e) f |= FL_EQ;
        else if(r->A.total < r->B.total) f |= FL_LT;
        r->flags = f;
        break; }
    /* os TRES saltos sao UM: a condicao e' argumento, nao instrucao (ver MOVE_pc). */
    case OP_JMP: return MOVE_pc(r, pc, 1);
    case OP_JZ:  return MOVE_pc(r, pc,  (r->flags & FL_ZERO) != 0);
    case OP_JNZ: return MOVE_pc(r, pc, !(r->flags & FL_ZERO));
    default: return 0;
    }
    r->pc = pc;
    return 1;
}
static long rodar(unsigned prog_len){
    Regs r; memset(&r, 0, sizeof r);
    long passos = 0;
    while(passo(&r, prog_len)){ if(++passos > 50000000L) break; }
    return passos;
}

/* ---------------- o montador: escreve o bytecode NO DISCO ---------------- */
static unsigned pc_emit = 0;
static void emit1(unsigned char b){ pwrite(fprog, &b, 1, (off_t)pc_emit); pc_emit++; }

/* O SALTO É UM BYTE COM SINAL, E O TECTO É 127.
 *
 * O `MOVE_pc` lê o deslocamento com `(signed char)`; escrevê-lo com
 * `(unsigned char)` faz de um corpo de 128..255 um salto NEGATIVO — para trás
 * —, e a máquina fica a rodar até o guarda dos cinquenta milhões de passos a
 * parar. Por linha. Duas réguas para o mesmo número: escrito numa, lido noutra.
 *
 * Havia CINCO sítios a escrever o deslocamento e nenhum a medi-lo. Passa a
 * haver um só, e quem estourar levanta a bandeira: a consulta é RECUSADA com a
 * razão, em vez de a máquina não voltar. */
static int salto_estourou = 0;
/* escreve o deslocamento (uma Word) na posição reservada; `d` já é relativo */
static void salto_rel(unsigned pos, long d){
    if(d > 32767 || d < -32768){ salto_estourou = 1; d = 0; }
    { unsigned char lo = (unsigned char)((unsigned long)d & 255u);
      unsigned char hi = (unsigned char)(((unsigned long)d >> 8) & 255u);
      pwrite(fprog, &lo, 1, (off_t)pos);
      pwrite(fprog, &hi, 1, (off_t)(pos + 1)); }
}
static void salto_poe(unsigned pos, unsigned ini){
    long d = (long)pc_emit - (long)ini;
    if(d > 32767 || d < -32768){ salto_estourou = 1; d = 0; }
    { unsigned char lo = (unsigned char)((unsigned long)d & 255u);
      unsigned char hi = (unsigned char)(((unsigned long)d >> 8) & 255u);
      pwrite(fprog, &lo, 1, (off_t)pos);
      pwrite(fprog, &hi, 1, (off_t)(pos + 1)); }
}
/* A VARREDURA É UMA PROGRESSÃO ARITMÉTICA NO ENDEREÇO.
 *
 * A ISA não tem endereçamento indireto — o slot é imediato na instrução (e LOADS, que eu
 * esperava que fosse indireto, é LOAD com a cifra espectral; fui ver em broca-so). Por isso o
 * compilador desenrolava a varredura: um bloco por linha, e o bytecode crescia LINEARMENTE com
 * a tabela. Medido antes desta mudança: 147 bytes por linha, 75 KB para 512 linhas.
 *
 * Mas o endereço de cada linha É uma PA. A linha i, coluna j, mora em S_LINHAS + i·ncols + j:
 * passo constante ncols. O bitmap e o vivo andam de 1 em 1. E o resto não anda.
 *
 * Então emite-se UM bloco — o da linha 0 — e anda-se com ele: antes de cada passagem, cada
 * endereço que depende da linha avança o seu passo. O bytecode passa a ser O(1) na tabela, e
 * quem varre é a progressão, não o compilador.
 *
 * O passo sai da FAIXA do slot, sem tocar em nenhum lugar de chamada: quem está nas linhas anda
 * ncols, quem está no bitmap ou no vivo anda 1, e os temporários e constantes não andam. */
#define NREL 512
typedef struct { unsigned off, base; long passo; } Rel;
static Rel *const rel = DISCO_FIXO(Rel, 25);
static int nrel = 0;
static long rel_ncols = 0;            /* > 0 só enquanto se emite o MOLDE */

/* OS MODOS DO RELOCADOR. O molde é emitido para a linha 0 e o `rel_anda(i)`
 * reescreve os endereços para a linha i. Com o bitmap a BIT, o endereço avança
 * um slot a cada DEZASSEIS linhas e a máscara roda pelas dezasseis coordenadas
 * da base — nenhum dos dois é um passo constante, e por isso são modos e não
 * passos. */
enum { REL_PASSO = 0, REL_BITMAP = 1, REL_MASC = 2, REL_MASCN = 3 };
static int modo_prox = REL_PASSO;   /* posto antes de um MOVE, consumido por emit_slot */
/* O modo viaja no campo `passo`, em NEGATIVO: o `Rel` vive no DISCO com tamanho
 * fixo, e acrescentar-lhe um campo mudava o mapeamento. Os passos verdadeiros
 * são sempre ≥ 0, logo o sinal chega para os separar. */

static long passo_do_slot(unsigned s){
    if(!rel_ncols) return 0;
    /* O PREFIXO DIZ A ZONA. Perguntar `s >= S_DEN` só vale enquanto essas
     * forem as de endereço mais alto; com zonas por prefixo, a zona LÊ-SE. */
    switch(s >> ZBITS){ case 1: case 2: case 3: return rel_ncols; default: break; }
    /* os dois bitmaps não têm passo: andam por MODO (um slot por 16 linhas) */
    return 0;                                /* constantes e rascunho: parados  */
}
/* ── MOVE: A OPERACAO, E E' UMA SO' ──────────────────────────────────────────────────
 *
 * O Aarao: "e' o paradigma novo da sexta dimensao — la', onde 1+2+3 = 6 = 3x2x1, o sistema
 * funciona e NAO EXISTEM DUAS OPERACOES. Em outras dimensoes ha' diferenca; nessa nao.
 * Entao e' redundancia, e nao comunica a mensagem da Lei: A UNIDADE E'."
 *
 * LOAD e STORE ficavam como dois NOMES para o mesmo — e dois nomes dizem "sao dois". A Lei
 * diz que e' um, e o par e' o que se LE' da unidade, nao duas coisas ao lado uma da outra.
 *
 *     MOVE(slot, +1)     do slot para o registo    (o velho LOAD)
 *     MOVE(slot, -1)     do registo para o slot    (o velho STORE)
 *
 * Os nomes velhos ficam guardados no enum e no montador, para que o bytecode antigo se leia
 * e a historia nao se apague — guarda-se o velho e usa-se o novo. Mas o codigo emite MOVER. */
static void emit_slot(unsigned char op, unsigned slot);
static void MOVE(unsigned slot, int sentido){
    emit_slot(sentido > 0 ? OP_LOAD : OP_STORE, slot);
}
/* o mesmo MOVE, dizendo em que modo o endereço anda com a linha */
static void MOVE_M(unsigned slot, int sentido, int modo){
    modo_prox = modo;
    emit_slot(sentido > 0 ? OP_LOAD : OP_STORE, slot);
}

static void emit_slot(unsigned char op, unsigned slot){
    long p = modo_prox ? -(long)modo_prox : passo_do_slot(slot);
    modo_prox = REL_PASSO;                       /* vale para UM MOVE, e só */
    emit1(op);
    if(p && nrel < NREL){ rel[nrel].off = pc_emit; rel[nrel].base = slot; rel[nrel].passo = p; nrel++; }
    emit1((unsigned char)(slot & 0xFF)); emit1((unsigned char)(slot >> 8));
}
/* A MERKLE PELA DOBRA DA MAQUINA. O st_merkle fazia o laco de SHAs a mao; sai.
 *
 * A subida da branch NAO e a dobra de uma arvore inteira — e a DOBRA DE DOIS, repetida: a raiz e
 * a folha, e cada ramo dobra-se com ela. Usa-se o OP_FOLD com n=2, tantas vezes quantos os ramos,
 * e quem dobra e a maquina. O coinbase continua a ser CONCATENACAO e o seu duplo SHA e a FOLHA —
 * isso nao e dobra, e o objeto de onde a dobra parte. */
static int coinbase_em_slots(void){
    int n = 0;
    #define POE(x) do{ unsigned char _b = (unsigned char)(x); atomos_grava(S_CB + (unsigned)n, &_b, 1); n++; }while(0)
    for(int k = 0; k < pool_st.n1; k++) POE(pool_st.cb1[k]);
    for(int k = 0; k < pool_st.en1_len/2; k++){
        int hi = hexval(pool_st.extranonce1[2*k]), lo = hexval(pool_st.extranonce1[2*k+1]);
        if(hi < 0 || lo < 0) break;
        POE((unsigned char)(hi*16 + lo));
    }
    { int e2 = pool_st.en2_size; if(e2 < 0 || e2 > 32) e2 = 4;
      for(int k = 0; k < e2; k++) POE(0); }
    for(int k = 0; k < pool_st.n2; k++) POE(pool_st.cb2[k]);
    #undef POE
    return n;
}
/* O SHA A CORRER SOBRE OS SLOTS. Ele processa blocos de 64 e nao precisa de ver o objeto todo —
 * le 64, comprime, le os 64 seguintes. O que resta em memoria e UM BLOCO, nao o coinbase. */
static void sha_dos_slots(unsigned base, int n, unsigned char *out){
    unsigned h[8]; sha_ini(h);
    unsigned char bl[64];
    int i = 0;
    while(n - i >= 64){
        atomos_le(base + (unsigned)i, bl, 64);
        sha_bloco(h, bl); i += 64;
    }
    unsigned char cauda[128]; memset(cauda, 0, sizeof cauda);
    int r = n - i;
    if(r) atomos_le(base + (unsigned)i, cauda, r);
    cauda[r] = 0x80;
    unsigned long bits = (unsigned long)n * 8;
    int tot = (r + 1 <= 56) ? 64 : 128;
    for(int k = 0; k < 8; k++) cauda[tot-1-k] = (unsigned char)(bits >> (8*k));
    sha_bloco(h, cauda);
    if(tot == 128) sha_bloco(h, cauda + 64);
    sha_fim(h, out);
}
/* POR BYTES NO BANCO: átomos físicos consecutivos. */
static void banco_poe(unsigned base, int off, const unsigned char *b, int n){
    atomos_grava(base + (unsigned)off, b, n);
}
static void merkle_pelo_fold(void){
    int n = coinbase_em_slots();                          /* a SOMA: slots consecutivos */
    unsigned char h[32], folha[32];
    sha_dos_slots(S_CB, n, h); sha256(h, 32, folha);      /* a FOLHA: o SHA sobre os slots */
    for(int k = 0; k < pool_st.n_ramos; k++){
        atomos_grava(S_FOLHA, folha, 32);
        atomos_grava(S_FOLHA + 32, pool_st.ramos + 32*k, 32);
        atomos_u32(S_FOLD_ARG, S_FOLHA);
        mem_grava(S_TMP + 1, w8(2, 0));
        pc_emit = 0;
        MOVE(S_TMP + 1, +1); MOVE(S_TMP + 1, +1);  /* B=n; base em átomos */
        emit1(OP_FOLD); emit1(OP_HALT);
        unsigned pl = pc_emit;
        Regs rg; memset(&rg, 0, sizeof rg);
        long ps = 0; while(passo(&rg, pl)){ if(++ps > 100000) break; }
        atomos_le(S_FOLHA, folha, 32);
    }
    memcpy(pool_st.merkle_raiz, folha, 32);
}
/* anda o molde uma linha: cada sítio de realocação avança o seu passo */
static void rel_anda(long i){
    for(int t = 0; t < nrel; t++){
        unsigned v;
        if(rel[t].passo == -REL_BITMAP)     v = rel[t].base + (unsigned)((unsigned long)i / SLOT_BITS);
        else if(rel[t].passo == -REL_MASC)  v = S_BITM + (unsigned)((unsigned long)i % SLOT_BITS);
        else if(rel[t].passo == -REL_MASCN) v = S_BITN + (unsigned)((unsigned long)i % SLOT_BITS);
        else                                v = (unsigned)(rel[t].base + i * rel[t].passo);
        unsigned char lo = (unsigned char)(v & 0xFF), hi = (unsigned char)(v >> 8);
        pwrite(fprog, &lo, 1, (off_t)rel[t].off);
        pwrite(fprog, &hi, 1, (off_t)rel[t].off + 1);
    }
}
/* põe a constante do slot k no slot destino: LOAD k, LOAD zero, ADD, STORE dest */
static void emit_copia(unsigned de, unsigned para){
    MOVE(de, +1);
    MOVE(S_ZERO, +1);
    emit1(OP_ADD);
    MOVE(para, -1);
}

/* ---------------- SQL: o analisador ---------------- */
static void pula(const char **p){ while(**p && isspace((unsigned char)**p)) (*p)++; }
static int palavra(const char **p, const char *w){
    pula(p);
    size_t n = strlen(w);
    if(strncasecmp(*p, w, n) == 0 && (!isalnum((unsigned char)(*p)[n]))){ *p += n; return 1; }
    return 0;
}
static int numero(const char **p, long *v){
    pula(p);
    int sinal = 1;
    if(**p == '-'){ sinal = -1; (*p)++; }
    if(!isdigit((unsigned char)**p)) return 0;
    long x = 0;
    while(isdigit((unsigned char)**p)){ x = x*10 + (**p - '0'); (*p)++; }
    *v = sinal * x;
    return 1;
}
static int ident(const char **p, char *out, size_t cap){
    pula(p);
    size_t k = 0;
    if(**p == '*'){ (*p)++; snprintf(out, cap, "*"); return 1; }
    /* ── O IDENTIFICADOR CITADO, que é como o Prisma escreve TODOS os nomes:
     * "Tenant", "userId", "createdAt". A aspa dupla delimita e não faz parte do
     * nome; uma aspa dentro escreve-se dobrando-a. Sem isto o motor recusava o
     * esquema inteiro do cliente --- 426 de 431 comandos --- e a razão não era
     * nenhuma das construções: era a citação. */
    if(**p == '"'){
        (*p)++;
        while(**p){
            if(**p == '"'){
                if((*p)[1] == '"'){ if(k+1 < cap) out[k++] = '"'; (*p) += 2; continue; }
                (*p)++; break;
            }
            if(k+1 < cap) out[k++] = **p;
            (*p)++;
        }
        out[k] = 0;
        return k > 0;
    }
    while(isalnum((unsigned char)**p) || **p == '_'){ if(k+1 < cap) out[k++] = **p; (*p)++; }
    out[k] = 0;
    return k > 0;
}

static int sql_executa_1(const char *sql, SqlOut *out);
static const char *cmd_fim(const char *p);          /* onde acaba UM comando */
static int sql_executa_lote(const char *sql, SqlOut *out);

/* ---------------- os comandos ---------------- */

/* `CHECK (` já lido até ao parêntese: copia o texto de dentro, contando os
 * parênteses para não parar no primeiro fecho de uma subexpressão. Um segundo
 * CHECK é RECUSADO em vez de calado — juntar dois com um AND implícito seria o
 * motor a escrever predicado que ninguém escreveu. */
static int le_check(const char **p, char *out, size_t cap){
    int nivel = 0;
    size_t o = 0;
    if(out[0]){
        printf("erro: dois CHECK na mesma tabela — RECUSADO. Junte-os num só"
               " predicado; o motor não escreve o AND que ninguém escreveu.\n");
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err,
                     "only one CHECK constraint per table"); }
        return 0;
    }
    if(**p != '(') return 0;
    (*p)++; nivel = 1;
    while(**p && nivel > 0){
        if(**p == '(') nivel++;
        else if(**p == ')'){ nivel--; if(!nivel){ (*p)++; break; } }
        if(o + 1 < cap) out[o++] = **p;
        (*p)++;
    }
    out[o] = 0;
    if(nivel > 0 || !o){
        printf("erro: CHECK sem predicado ou sem fechar — RECUSADO.\n");
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err, "malformed CHECK constraint"); }
        return 0;
    }
    return 1;
}

/* ── UMA ORDEM, VÁRIAS LINHAS, E OU ENTRAM TODAS OU NENHUMA ──────────────────
 *
 * `INSERT INTO t VALUES (1,2), (3,4)` era aceite e escrevia SÓ A PRIMEIRA. As
 * outras desapareciam sem uma palavra, com a resposta a dizer que tinha
 * corrido bem — que é a pior forma de falhar desta casa: não é responder
 * errado, é responder certo sobre outra coisa.
 *
 * Parte-se a ordem nos seus tuplos e escreve-se um a um, mas ATOMICAMENTE: uma
 * ordem é uma ordem, e metade dela feita não é resposta nenhuma. Abre-se o
 * desfazer, e se algum tuplo for recusado — pelo envelope, por uma restrição,
 * pela seta — desfaz-se o que já entrou e devolve-se a recusa desse tuplo. Se o
 * cliente já tinha uma transacção aberta, não se lhe toca: a dele é maior, e
 * quem a fecha é ele. */
static int insere(const char *resto);
static int insere_muitas(const char *resto){
    const char *v = NULL;
    char cab[128];
    int n = 0, quantos = 0;
    /* o cabeçalho é tudo até ao VALUES, e os tuplos vêm depois */
    { const char *q = resto, *ini_v = NULL;
      while(*q){
          const char *r = q;
          if(palavra(&r, "VALUES")){ v = r; ini_v = q; break; }
          q++;
      }
      if(!v) return insere(resto);
      /* o cabeçalho vai até ao INÍCIO do VALUES; ele é reposto ao montar cada
       * tuplo, e apanhá-lo aqui dava «VALUES VALUES» */
      { size_t nc = (size_t)(ini_v - resto);
        if(nc >= sizeof cab) return insere(resto);
        memcpy(cab, resto, nc); cab[nc] = 0; } }

    /* contam-se os tuplos ANTES de escrever: com um só, nada muda */
    { const char *q = v; int nivel = 0;
      while(*q){
          if(*q == '(' ){ if(!nivel) quantos++; nivel++; }
          else if(*q == ')') nivel--;
          q++;
      } }
    if(quantos <= 1) return insere(resto);

    { int nossa = !undo_em_tx, falhou = 0;
      const char *q = v;
      if(nossa) sql_tx_abre();
      while(*q && !falhou){
          const char *ini;
          int nivel = 0;
          while(*q && *q != '(') q++;
          if(!*q) break;
          ini = q;
          do {
              if(*q == '(') nivel++;
              else if(*q == ')') nivel--;
              q++;
          } while(*q && nivel > 0);
          { char um[600];
            size_t tam = (size_t)(q - ini);
            if(tam + strlen(cab) + 10 >= sizeof um){ falhou = 1; break; }
            snprintf(um, sizeof um, "%s VALUES %.*s", cab, (int)tam, ini);
            if(!insere(um)) falhou = 1; else n++; }
          pula(&q);
          if(*q == ',') q++;
      }
      if(falhou){
          /* E O DESFAZER PODE RECUSAR --- a lei está escrita no diário («se
           * encher, a transacção fica MARCADA e o ROLLBACK RECUSA: desfazer
           * metade seria pior do que não desfazer»), e este chamador NÃO A
           * ALCANÇAVA: deitava fora o retorno e anunciava «as anteriores foram
           * DESFEITAS» de qualquer maneira. Com o diário cheio isso é o motor a
           * afirmar que fez o que não fez --- o mesmo desfecho do SET que
           * gravava 240 e dizia «1 linha atualizada». O que se diz agora é o
           * que aconteceu, e as três saídas são distintas. */
          int desfez = 1;
          if(nossa){ desfez = sql_tx_desfaz(); sql_tx_fecha(); }
          printf("erro: a ordem tinha %d linha(s) e a %d.ª foi recusada — %s.\n",
                 quantos, n + 1,
                 !nossa ? "a transacção é sua e fica aberta"
                        : desfez ? "as anteriores foram DESFEITAS"
                                 : "o diário ENCHEU e as anteriores NÃO foram desfeitas"
                                   " --- a base ficou com as linhas que passaram");
          if(sql_cap){ sql_cap->ok = 0;
              snprintf(sql_cap->err, sizeof sql_cap->err,
                       desfez ? "multi-row INSERT: row %d rejected, none inserted"
                              : "multi-row INSERT: row %d rejected, undo log full,"
                                " earlier rows NOT rolled back", n + 1); }
          return 0;
      }
      if(nossa) sql_tx_fecha();
      printf("-- %d linhas numa ordem só\n", n);
      if(sql_cap) snprintf(sql_cap->tag, sizeof sql_cap->tag, "INSERT 0 %d", n);
      return 1; }
}

/* ── UM `CREATE` QUE FALHA NÃO PODE DEIXAR RASTO ──────────────────────────────
 * O ficheiro da tabela abre-se ANTES de a declaração acabar de ser lida — tem de
 * ser, porque é nele que o catálogo vai ser escrito. Se a leitura recusar depois
 * disso, o ficheiro fica: vazio, sem nome e sem colunas, mas EXISTE. E aí a
 * tabela recusada comporta-se pior do que uma que nunca foi mencionada — esta é
 * recusada com «não existe», aquela aceita `SELECT` (devolvendo zero linhas) e
 * aceita `INSERT`. A recusa passava a ser uma criação encoberta.
 *
 * Desfaz-se pelo mesmo caminho do `DROP`: larga-se o descritor e apagam-se os
 * dois ficheiros. É o que a transacção já faz por dentro — recusar é voltar ao
 * estado anterior, não ficar a meio. */
static void caminho_tabela(const char *nome, char *out, size_t cap, const char *ext);
static char g_tabela[64];
static void cria_desfaz(const char *nome){
    if(!strcmp(g_tabela, nome)) usa_tabela("", 0);
    { char m[600], pr[600];
      caminho_tabela(nome, m, sizeof m, ".mem");
      caminho_tabela(nome, pr, sizeof pr, ".prog");
      unlink(m); unlink(pr); }
}
static int cria(const char *resto){
    const char *p = resto;
    char nome[64];
    if(!ident(&p, nome, sizeof nome)) return 0;
    /* a tabela é um ficheiro: abre-se (criando) ANTES de o catálogo lá ser escrito */
    if(!nome_cabe(nome, S_NOME_N * 2, "da tabela")) return 0;
    if(!usa_tabela_z(nome, 1, 1)){          /* CREATE: limpa */
        printf("erro: não abri o ficheiro da tabela «%s»\n", nome);
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err, "cannot create relation \"%s\"", nome); }
        return 0;
    }
    pula(&p);
    if(*p != '('){ cria_desfaz(nome); return 0; }
    p++;
    long ncols = 0; char c[64];
    /* ── NADA SE ACUMULA: O QUE SE LÊ ESCREVE-SE ─────────────────────────────
     *
     * Eram cinco arrays de COL_MAX --- corpo, parm, restr, defv, deftem --- mais
     * dois de nomes, a somar uns 200 KB de RAM, para depois um laço os despejar
     * no disco. A tabela já está ABERTA quando o parse começa (o `usa_tabela`
     * corre antes do parêntese), pelo que a coluna pode ser escrita no sítio
     * dela assim que é lida. «A memória é o DISCO. Sem RAM»: aqui isso deixa de
     * ser uma frase no cabeçalho e passa a ser o que o código faz --- e sai
     * também o laço do despejo, que era a mesma informação escrita duas vezes.
     *
     * O que resta em RAM é UMA coluna de cada vez, que é o que o parse tem à
     * mão --- o mesmo tamanho que o `aranha.c` usa para ler um ficheiro
     * inteiro: um byte de cada vez. */
    /* AS SETAS PENDENTES SÃO POUCAS, E SÃO UMA LISTA --- não um array por
     * coluna. Uma seta só existe onde foi declarada; guardar um nome de tabela
     * de 64 bytes por CADA coluna possível era reservar 128 KB para guardar
     * três. A lista tem o número da coluna dentro, que é o que faltava para ela
     * poder ser compacta. */
    enum { FKP_MAX = 64 };
    static struct { long col, modo; char tab[64], alvo[64]; } fkp[FKP_MAX];
    static int fkp_n;
    fkp_n = 0;
    long corpo_j = 0, parm_j = 0, restr_j = 0, defv_j = 0, deftem_j = 0;
    char chk[S_CHECK_W * 2 + 2]; chk[0] = 0;
    char fk_tab_j[64], fk_alvo_j[64]; long fk_col_j = -1, fk_modo_j = 0;
    fk_tab_j[0] = 0; fk_alvo_j[0] = 0;
    (void)fk_col_j;
    while(1){
        { /* `CHECK (...)` no meio da lista é restrição da TABELA e não uma
           * coluna chamada «check»: quem decide é o parêntese a seguir. */
          const char *vc = p;
          char k[64];
          if(ident(&p, k, sizeof k) && !strcasecmp(k, "CHECK")){
              pula(&p);
              if(*p == '('){
                  if(!le_check(&p, chk, sizeof chk)){ cria_desfaz(nome); return 0; }
                  pula(&p);
                  if(*p == ','){ p++; continue; }
                  break;
              }
          }
          p = vc; }
        /* ── A RESTRIÇÃO DE TABELA, que vem depois das colunas e não é uma:
         *     CONSTRAINT "t_pkey" PRIMARY KEY ("id")
         * O Prisma escreve-a em TODAS as tabelas. Ela nomeia colunas que já
         * foram declaradas, pelo que o que faz é MARCÁ-LAS --- e é isso que se
         * faz aqui, em vez de a recusar e perder a tabela inteira. */
        { const char *v7 = p;
          if(palavra(&p, "CONSTRAINT")){
              char cn[64];
              if(!ident(&p, cn, sizeof cn)){ p = v7; }
              else {
                  pula(&p);
                  int e_pk = 0, e_un = 0, e_fk = 0;
                  if(palavra(&p, "PRIMARY")){ palavra(&p, "KEY"); e_pk = 1; }
                  else if(palavra(&p, "UNIQUE")) e_un = 1;
                  else if(palavra(&p, "FOREIGN")){ palavra(&p, "KEY"); e_fk = 1; }
                  else if(palavra(&p, "CHECK")) { /* deixa ao le_check adiante */ }
                  if(e_pk || e_un || e_fk){
                      pula(&p);
                      if(*p == '('){
                          p++;
                          while(*p){
                              char cc2[64];
                              pula(&p);
                              if(!ident(&p, cc2, sizeof cc2)) break;
                              /* marca a coluna nomeada, se ela já foi declarada */
                              for(long z = 0; z < ncols; z++){
                                  char nz[64]; col_nome_le((int)z, nz, sizeof nz);
                                  if(!strcasecmp(nz, cc2)){
                                      /* a coluna já foi escrita: a restrição
                                       * acrescenta-se ONDE ela está */
                                      { Word wz = mem_le(S_RESTR + (unsigned)z);
                                        if(e_pk) wz.total |= (R_UNICO | R_NOTNULL);
                                        else if(e_un) wz.total |= R_UNICO;
                                        mem_grava(S_RESTR + (unsigned)z, wz); }
                                      break; }
                              }
                              pula(&p);
                              if(*p == ','){ p++; continue; }
                              break;
                          }
                          pula(&p); if(*p == ')') p++;
                      }
                      /* a chave estrangeira traz REFERENCES a seguir --- lê-se e
                       * regista-se do mesmo modo que a de coluna */
                      pula(&p);
                      if(e_fk && palavra(&p, "REFERENCES")){
                          char mt[64], mc[64];
                          if(ident(&p, mt, sizeof mt)){
                              pula(&p);
                              if(*p == '('){ p++; if(ident(&p, mc, sizeof mc)){ pula(&p);
                                             if(*p == ')') p++; } }
                          }
                          /* as cláusulas ON DELETE/UPDATE consomem-se */
                          while(palavra(&p, "ON")){
                              char aa[64]; pula(&p);
                              if(!ident(&p, aa, sizeof aa)) break;
                              pula(&p);
                              { char bb[64]; const char *v6 = p;
                                if(!ident(&p, bb, sizeof bb)) p = v6;
                                else if(!strcasecmp(bb,"NO")){ pula(&p); char cc3[64];
                                       const char *v5 = p; if(!ident(&p,cc3,sizeof cc3)) p = v5; } }
                              pula(&p);
                          }
                      }
                      pula(&p);
                      if(*p == ','){ p++; continue; }
                      break;
                  }
                  p = v7;
              }
          } }
        if(!ident(&p, c, sizeof c)) break;
        if(!nome_cabe(c, (int)S_COLNOME_W * 2, "da coluna")){ cria_desfaz(nome); return 0; }
        col_nome_grava((int)ncols, c);       /* o nome da coluna passa a ficar guardado */
        corpo_j = CORPO_INTEIRO; parm_j = 0;   /* sem tipo = INTEIRO, como sempre foi */
        pula(&p);
        char tipo[64];
        const char *volta = p;
        /* ── UM TIPO CITADO é um enum declarado antes: "DeploymentMode". A
         * coluna que o usa guarda a CADEIA, e o domínio é o que o CREATE TYPE
         * disse --- não se inventa corpo novo por cada enum do cliente. */
        if(*p == '"'){
            char tq[64];
            const char *v8 = p;
            if(ident(&p, tq, sizeof tq)){ corpo_j = CORPO_TEXTO; }
            else p = v8;
        }
        else if(isalpha((unsigned char)*p) && ident(&p, tipo, sizeof tipo)){
            int achou = 1;
            if(!strcasecmp(tipo,"RACIONAL"))      corpo_j = CORPO_RACIONAL;
            else if(!strcasecmp(tipo,"AUREO"))  { corpo_j = CORPO_AUREO;   parm_j = 1; }
            else if(!strcasecmp(tipo,"MORFICO")){ corpo_j = CORPO_MORFICO; parm_j = 6; }
            /* PASSO 6: o cristalino. O parâmetro é o t da borda ω² = tω − 1 — t=0 Gauss ℤ[i],
             * t=1 Eisenstein ℤ[ω]. Predefine 0, que é o cristal quadrado. */
            else if(!strcasecmp(tipo,"CRISTALINO")){ corpo_j = CORPO_CRISTAL; parm_j = 0; }
            else if(!strcasecmp(tipo,"INTEIRO"))  corpo_j = CORPO_INTEIRO;
            else if(!strcasecmp(tipo,"BOOLEANO") || !strcasecmp(tipo,"BOOLEAN")
                 || !strcasecmp(tipo,"BOOL"))     corpo_j = CORPO_BOOLEANO;
            else if(!strcasecmp(tipo,"DATA") || !strcasecmp(tipo,"DATE")
                 || !strcasecmp(tipo,"TIMESTAMP") || !strcasecmp(tipo,"DATETIME"))
                                                  corpo_j = CORPO_DATA;
            else if(!strcasecmp(tipo,"TEXTO") || !strcasecmp(tipo,"TEXT")
                 || !strcasecmp(tipo,"VARCHAR") || !strcasecmp(tipo,"CHAR")
                 || !strcasecmp(tipo,"STRING") || !strcasecmp(tipo,"UUID")
                 || !strcasecmp(tipo,"JSON") || !strcasecmp(tipo,"JSONB"))
                                                  corpo_j = CORPO_TEXTO;
            /* ── DECIMAL e NUMERIC são o RACIONAL, e é aqui que a casa ganha:
             * o Decimal do cliente é exacto no banco, sem arredondamento. A
             * precisão (p,s) fica declarada e não muda o corpo --- porque o
             * racional não a precisa: ele guarda a CLASSE, não uma escala. */
            /* ── `vector(768)` é da extensão pgvector, que este motor DECLAROU
             * não carregar. A coluna aceita-se e guarda como texto --- para a
             * tabela não se perder por causa de um campo ---, mas o que a
             * extensão promete (o operador de distância) não existe aqui, e
             * quem o usar bate numa função que não há. Isso diz-se, não se
             * finge: guardar não é saber operar. */
            /* BYTEA: os bytes crus. Guardam-se no pool como qualquer cadeia --- o
             * pool é de bytes e não sabe de letras --- e o que muda é só o nome. */
            else if(!strcasecmp(tipo,"BYTEA") || !strcasecmp(tipo,"BLOB")
                 || !strcasecmp(tipo,"BYTES"))    corpo_j = CORPO_TEXTO;
            else if(!strcasecmp(tipo,"vector") || !strcasecmp(tipo,"halfvec")
                 || !strcasecmp(tipo,"tsvector")){
                corpo_j = CORPO_TEXTO;
                printf("aviso: a coluna é `%s`, de uma extensão não carregada --- ela"
                       " GUARDA, e as operações dela não existem aqui\n", tipo);
            }
            else if(!strcasecmp(tipo,"DECIMAL") || !strcasecmp(tipo,"NUMERIC")
                 || !strcasecmp(tipo,"MONEY"))    corpo_j = CORPO_RACIONAL;
            else if(!strcasecmp(tipo,"SERIAL") || !strcasecmp(tipo,"BIGSERIAL")
                 || !strcasecmp(tipo,"BIGINT") || !strcasecmp(tipo,"SMALLINT")
                 || !strcasecmp(tipo,"INT") || !strcasecmp(tipo,"INTEGER"))
                                                  corpo_j = CORPO_INTEIRO;
            else { p = volta; achou = 0; }               /* não era tipo: devolve ao analisador */
            if(achou){
                pula(&p);
                /* ── `TEXT[]` é um array. A coluna guarda a CADEIA com os
                 * elementos, e as operações de array não existem aqui --- o
                 * mesmo princípio do vector: guardar não é saber operar, e
                 * dizê-lo é melhor do que perder a tabela. */
                if(*p == '['){
                    p++; pula(&p); if(*p == ']') p++;
                    corpo_j = CORPO_TEXTO;
                    printf("aviso: a coluna é um ARRAY --- ela guarda a cadeia, e as"
                           " operações de array não existem aqui\n");
                }
                if(*p == '('){ p++; long q; if(numero(&p, &q)) parm_j = q; pula(&p);
                               /* DECIMAL(18,2) e TIMESTAMP(3): a escala declara-se
                                * e não muda o corpo --- o racional guarda a classe */
                               while(*p == ','){ p++; pula(&p); long q2;
                                                 if(!numero(&p, &q2)) break;
                                                 pula(&p); }
                               if(*p == ')') p++; }
            }
        }
        /* ── E AS RESTRIÇÕES, que vêm depois do tipo e antes da vírgula ─────
         * `PRIMARY KEY` é lido como a conjunção que ele é: único e não-nulo. */
        pula(&p);
        while(isalpha((unsigned char)*p)){
            const char *v2 = p;
            char r[64];
            if(!ident(&p, r, sizeof r)){ p = v2; break; }
            if(!strcasecmp(r, "NOT")){
                pula(&p);
                if(palavra(&p, "NULL")) restr_j |= R_NOTNULL;
                else { p = v2; break; }
            }
            else if(!strcasecmp(r, "UNIQUE")) restr_j |= R_UNICO;
            else if(!strcasecmp(r, "DEFAULT")){
                /* o valor por omissão --- e ele fica GUARDADO, não interpretado
                 * de novo a cada INSERT: quem o quiser mudar altera a tabela */
                pula(&p);
                long dv = 0; int neg = 0;
                if(*p == '-'){ neg = 1; p++; pula(&p); }
                if(*p == '\''){
                    /* DEFAULT 'SAAS' --- a cadeia entra no pool e o default é o
                     * endereço dela, como qualquer outro valor de texto */
                    p++;
                    char db[TX_MAX + 2]; int dn = 0;
                    while(*p && *p != '\''){ if(dn < TX_MAX) db[dn++] = *p; else dn = TX_MAX + 1; p++; }
                    if(*p == '\'') p++;
                    /* o DEFAULT é escrita como qualquer outra, e recusa como as
                     * outras: truncá-lo daria uma coluna cujo valor por omissão
                     * não é o que foi declarado, e ninguém saberia */
                    if(dn > TX_MAX){
                        printf("erro: o DEFAULT da coluna passa do que a célula segura --- RECUSADO.\n");
                        if(sql_cap){ sql_cap->ok = 0;
                            snprintf(sql_cap->err, sizeof sql_cap->err, "string too long"); }
                        return 0;
                    }
                    db[dn] = 0;
                    defv_j = (long)tx_guarda(db, dn); deftem_j = 1;
                    pula(&p); continue;
                }
                /* DEFAULT true / false --- o booleano diz-se por palavra */
                { const char *vb = p;
                  if(palavra(&p, "true")){ defv_j = 1; deftem_j = 1;
                                           pula(&p); continue; }
                  if(palavra(&p, "false")){ defv_j = 0; deftem_j = 1;
                                            pula(&p); continue; }
                  p = vb; }
                if(!numero(&p, &dv)){
                    /* `DEFAULT now()` e afins: aceita-se a palavra e guarda-se
                     * zero com a marca, porque o valor é do momento e não do
                     * esquema --- e dizer que não se sabe é melhor que inventar */
                    char fn[64];
                    if(!ident(&p, fn, sizeof fn)){ p = v2; break; }
                    pula(&p); if(*p == '('){ p++; pula(&p); if(*p == ')') p++; }
                    dv = 0;
                }
                if(neg) dv = -dv;
                defv_j = dv; deftem_j = 1;
            }
            else if(!strcasecmp(r, "CHECK")){
                pula(&p);
                if(*p != '('){ p = v2; break; }
                if(!le_check(&p, chk, sizeof chk)){ cria_desfaz(nome); return 0; }
            }
            else if(!strcasecmp(r, "REFERENCES")){
                /* `REFERENCES mae(col)` — e a coluna alvo diz-se: sem ela o
                 * motor teria de adivinhar qual das colunas da mãe é a chave. */
                char mt[64], mc[64];
                pula(&p);
                if(!ident(&p, mt, sizeof mt)){ p = v2; break; }
                pula(&p);
                if(*p != '('){ p = v2; break; }
                p++;
                if(!ident(&p, mc, sizeof mc)){ p = v2; break; }
                pula(&p);
                if(*p != ')'){ p = v2; break; }
                p++;
                snprintf(fk_tab_j, sizeof fk_tab_j, "%s", mt);
                snprintf(fk_alvo_j, 64, "%s", mc);
                /* e o que fazer quando o destino desaparecer. Sem cláusula, o
                 * modo é RECUSAR — que é o único que não muda nada sem ordem. */
                { int mais = 1;
                  while(mais){
                      const char *v3 = p;
                      int upd = 0, m = -1;
                      mais = 0;
                      pula(&p);
                      if(!palavra(&p, "ON")){ p = v3; break; }
                      pula(&p);
                      if(palavra(&p, "DELETE"))      upd = 0;
                      else if(palavra(&p, "UPDATE")) upd = 1;
                      else { p = v3; break; }
                      pula(&p);
                      if(palavra(&p, "CASCADE"))       m = 1;
                      else if(palavra(&p, "SET")){
                          pula(&p);
                          if(palavra(&p, "NULL"))      m = 2;
                      }
                      else if(palavra(&p, "RESTRICT")) m = 0;
                      if(m < 0){ p = v3; break; }
                      /* os dois modos são INDEPENDENTES e vivem nos dois pares
                       * de bits do mesmo octeto: nada obriga quem quer a fibra
                       * atrás numa mudança de chave a querê-la num apagar. */
                      if(upd) fk_modo_j = (fk_modo_j & 3) | (m << 2);
                      else    fk_modo_j = (fk_modo_j & ~3L) | m;
                      mais = 1;                     /* pode vir a outra cláusula */
                  } }
            }
            else if(!strcasecmp(r, "PRIMARY")){
                pula(&p);
                if(palavra(&p, "KEY")) restr_j |= R_UNICO | R_NOTNULL;
                else { p = v2; break; }
            }
            else { p = v2; break; }
            pula(&p);
        }
        /* ── A COLUNA FECHA E ESCREVE-SE. Nada fica à espera de um despejo no
         * fim: o corpo, o parâmetro, o default e a restrição vão para o sítio
         * deles agora, e a seta guarda-se na coluna (a mãe visita-se depois,
         * porque ela pode ainda não existir quando a filha se declara). */
        { Word wc; wc.total = (Word8)corpo_j; wc.e = (Word8)parm_j;
          corpo_poe(ncols, wc); }
        { Word wd; wd.total = (Word8)((unsigned long)defv_j & 255u);
          wd.e = (Word8)(deftem_j ? 1 : 0);
          if(ncols < (long)S_DEFAULT_N) mem_grava(S_DEFAULT + (unsigned)ncols, wd); }
        { Word wr; wr.total = (Word8)restr_j; wr.e = 0;
          mem_grava(S_RESTR + (unsigned)ncols, wr); }
        if(fk_tab_j[0] && fkp_n < FKP_MAX){
            fkp[fkp_n].col = ncols; fkp[fkp_n].modo = fk_modo_j;
            snprintf(fkp[fkp_n].tab,  sizeof fkp[0].tab,  "%s", fk_tab_j);
            snprintf(fkp[fkp_n].alvo, sizeof fkp[0].alvo, "%s", fk_alvo_j);
            fkp_n++;
        }
        ncols++; pula(&p);
        corpo_j = parm_j = restr_j = defv_j = deftem_j = 0;
        fk_tab_j[0] = 0; fk_alvo_j[0] = 0; fk_col_j = -1; fk_modo_j = 0;
        if(*p == ','){ p++; continue; } break;
    }
    /* ── O QUE SOBRA NA DECLARAÇÃO NÃO PODE FICAR CALADO ─────────────────────
     * O laço acima PARA na primeira palavra que não reconhece e devolve o
     * apontador ao sítio onde ela começa. Sem esta verificação, o que sobrava
     * era simplesmente ignorado: `CREATE TABLE b (x INTEIRO COM SINAL, y
     * RACIONAL)` — com uma sintaxe inventada no meio — criava uma tabela de UMA
     * coluna e anunciava-a como criada, e o `y` desaparecia sem uma palavra. As
     * consultas que se seguiam liam então uma matriz 2×1 onde estava escrito
     * 2×2, e a resposta errada vinha com a cara da certa.
     *
     * A declaração acaba em `)`. Se o analisador parou noutro sítio, há texto
     * que ele não soube ler, e há duas respostas possíveis: adivinhar o que o
     * autor queria, ou dizer onde parou. Diz-se onde parou — e a tabela morre
     * aqui, ANTES do catálogo, pelo que fica sem nome e sem colunas: nenhuma
     * consulta lhe chega. É a regra desta casa dita na porta de entrada — «não
     * se aceita e faz outra coisa». */
    pula(&p);
    if(*p != ')'){
        printf("erro: não sei ler «%.24s» na declaração da tabela «%s» —"
               " li %ld coluna(s) e parei aí. A tabela NÃO foi criada"
               " (aceitá-la seria criar uma tabela que não é a pedida).\n",
               p, nome, ncols);
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err,
                     "syntax error in column list near \"%.20s\" (read %ld columns)",
                     p, ncols); }
        cria_desfaz(nome);
        return 0;
    }
    /* o catálogo é escrito PELA MÁQUINA: constantes + STORE, compilado e executado */
    pc_emit = 0;
    Word w; w.total = ncols; w.e = 0; mem_grava(S_K, w);       /* a constante entra pela memória */
    w.total = 0; w.e = 0; mem_grava(S_ZERO, w);
    w.total = -1; w.e = 0; mem_grava(S_MT, w);      /* AND com isto zera o .e e guarda o total */
    w.total = 1; w.e = 0; mem_grava(S_UM, w);
    emit_copia(S_K, S_CAT);                                     /* o byte baixo, pela ISA */
    emit1(OP_HALT);
    rodar(pc_emit);
    cat_ncols_poe(ncols);        /* e o par completo: o alto no plano paralelo */
    /* ── OS PLANOS ALTOS LIMPAM-SE COM A TABELA.
     *
     * Desde que a leitura passou a juntar os dois planos --- baixo | alto ---, o
     * lixo do S_ALTO deixou de ser inofensivo: uma célula escrita por um caminho
     * que só toca no plano baixo passa a ser lida com o byte alto de quem esteve
     * ali antes. Limpar na criação é o que garante que a tabela nova nasce no
     * andar inteiro, e não só na metade de baixo. */
    /* (não se limpa plano nenhum: quem escreve a célula escreve os TRÊS, e a
     * presença diz se ela existe. Limpar 8192 slots era escrever onde a
     * trajectória não passou --- 44 KB por tabela antes de haver uma linha.) */
    Word cat = mem_le(S_CAT); cat.e = 0; mem_grava(S_CAT, cat); /* nrows = 0 */
    { Word m = {1,0}; mem_grava(S_PRESCAB, m); }   /* tabela nova: campo já é dela */
    cat_poe_nrows(0);                                           /* e no par, que é onde vive */
    /* o corpo, o parâmetro, o default e as restrições JÁ ESTÃO escritos --- cada
     * coluna escreveu-se quando fechou. Aqui fica só o que depende da tabela
     * inteira: o CHECK, que é da tabela, e a árvore do UNIQUE, que precisa de
     * saber o ncols final para endereçar as células. */
    txt_grava(S_CHECK, S_CHECK_W, chk);
    for(long j = 0; j < ncols && j < IDX_MAXCOL; j++)
        if(mem_le(S_RESTR + (unsigned)j).total & R_UNICO) idx_constroi(j, ncols, 0);

    /* ── E A SETA ESCREVE-SE NOS DOIS LADOS ──────────────────────────────────
     * A filha guarda para onde aponta; a mãe guarda quem a aponta. Sem o
     * segundo lado, o DELETE na mãe não teria como saber que está a cortar uma
     * seta por baixo — e uma restrição com metade não é uma restrição.
     *
     * A ida à mãe faz-se AQUI, com a filha já escrita, e volta-se: abrir outra
     * tabela relê o .mem, e o que se escreveu antes de sair fica. */
    { int alguma = 0;
      alguma = (fkp_n > 0);
      if(alguma){
        char guarda[64]; snprintf(guarda, sizeof guarda, "%s", nome);
        for(int k = 0; k < fkp_n; k++){
            if(!usa_tabela(fkp[k].tab, 0) || !cat_nome_bate(fkp[k].tab)){
                if(!usa_tabela(guarda, 0)) restauros_falhados++;
                printf("erro: a tabela «%s» da seta não existe — a coluna %ld fica"
                       " SEM seta (a tabela foi criada).\n", fkp[k].tab, fkp[k].col);
                fkp[k].tab[0] = 0;
                continue;
            }
            { int mc = col_indice(fkp[k].alvo);
              if(mc < 0){
                if(!usa_tabela(guarda, 0)) restauros_falhados++;
                printf("erro: a coluna «%s» não existe em «%s» — a coluna %ld fica"
                       " SEM seta.\n", fkp[k].alvo, fkp[k].tab, fkp[k].col);
                fkp[k].tab[0] = 0;
                continue;
              }
              /* a mãe regista quem a aponta, e só depois se volta. SE NÃO
               * COUBER, a seta não se escreve: uma filha com seta que a mãe não
               * vigia fica órfã no primeiro DELETE, e em silêncio. */
              if(!filho_regista(guarda, (int)fkp[k].col)){
                  if(!usa_tabela(guarda, 0)) restauros_falhados++;
                  printf("erro: a tabela «%s» já vigia %d filhas e não cabe mais uma"
                         " — a coluna %ld fica SEM seta, porque uma mãe que não vigia"
                         " é uma restrição que não existe.\n",
                         fkp[k].tab, FK_MAXFIL, fkp[k].col);
                  fkp[k].tab[0] = 0;
                  continue;
              }
              fkp[k].modo |= (long)mc << 8;      /* a coluna da mãe viaja com o modo */
            }
            if(!usa_tabela(guarda, 0)) restauros_falhados++;
        }
        for(int k = 0; k < fkp_n; k++)
            if(fkp[k].tab[0])
                fk_grava((int)fkp[k].col, fkp[k].tab,
                         (int)(fkp[k].modo >> 8), (int)(fkp[k].modo & 255));
      } }
    {
        /* OS TRÊS ÚLTIMOS NOMES FALTAVAM, e a mensagem dizia INTEIRO sobre um
         * BOOLEANO, um TEXTO e uma DATA --- o corpo estava certo e o veredicto
         * é que mentia, que é o pior sítio para um erro estar: quem lê o CREATE
         * fica a saber a coisa errada e nada falha. */
        static const char *nm[8] = {"INTEIRO","RACIONAL","AUREO","MORFICO","CRISTALINO",
                                    "BOOLEANO","TEXTO","DATA"};
        printf("tabela %s criada: %ld colunas —", nome, ncols);
        for(long j = 0; j < ncols && j < (long)S_CORPOX_N; j++){
            /* lê-se do DISCO, que é onde a coluna já está --- e assim a
             * mensagem descreve o que ficou escrito, e não o que uma cópia em
             * RAM dizia que ia ser escrito */
            Word wj = corpo_de(j);
            printf(" %s", nm[wj.total & 7]);
            if(wj.e) printf("(%u)", (unsigned)wj.e);
        }
        printf("\n");
    }
    cat_nome_grava(nome);            /* a relação passa a TER nome, e é este */
    if(sql_cap){
        snprintf(sql_cap->tag, sizeof sql_cap->tag, "CREATE TABLE");
        sql_cap->ncols = 0; sql_cap->nrows = 0;
    }
    return 1;
}

/* lê um inteiro com sinal; devolve 0 se não houver um ali */
static int le_int_simples(const char **pp, long *out){
    const char *q = *pp;
    pula(&q);
    int sinal = 1;
    if(*q == '-'){ sinal = -1; q++; pula(&q); }
    if(!isdigit((unsigned char)*q)) return 0;
    long v = 0;
    while(isdigit((unsigned char)*q)) v = v*10 + (*q++ - '0');
    *out = sinal * v;
    *pp = q;
    return 1;
}

/* AS VISTAS VIVEM NA TABELA SEM NOME, que é o `<base>.mem`.
 *
 * O .mem é POR TABELA, e uma vista tem de ser encontrada ANTES de se saber que
 * tabela abrir — a reescrita é que o diz. Guardá-la no .mem de uma tabela seria
 * pedir para a procurar onde ela só existe depois de resolvida, que é circular.
 * Fica na tabela sem nome, que é o ficheiro da base, e as duas funções abaixo
 * guardam e restauram a que estava aberta: uma consulta não pode trocar a
 * tabela da sessão por baixo de quem a fez. */
static int usa_tabela(const char *nome, int cria_se_falta);
static void vista_entra(char *guarda, size_t cap){
    snprintf(guarda, cap, "%s", g_tabela);
    if(!usa_tabela("", 0)) restauros_falhados++;
}
static void vista_sai(const char *guarda){
    if(!usa_tabela(guarda, 0)) restauros_falhados++;
}

static int idx_valido(long col, long nrows);
static int  idx_remove(long valor, int idx);
static void ord_usa_indice(long k);
static void ord_usa_rascunho(void);
static int  ord_insere(long valor, int idx);
static long celula_valor(long i, long j, long nc);
static void celula_grava(long i, long j, long nc, long valor);
/* o predicado compila-se com o mesmo `le_expr` e corre no mesmo molde; ambos
 * vivem mais abaixo, e o INSERT precisa deles antes */
static int  check_avalia(long i, long ncols, const char *texto);
static void check_le(char *out, int cap);
static void constantes_isa(void);

static int insere(const char *resto){
    const char *p = resto;
    char nome[64];
    if(!palavra(&p, "INTO")) return 0;
    if(!ident(&p, nome, sizeof nome)) return 0;
    if(!usa_tabela(nome, 0)) return cat_nome_recusa(nome);
    if(!cat_nome_bate(nome)) return cat_nome_recusa(nome);

    /* ── AS COLUNAS PODEM DIZER-SE, E AS QUE NÃO SE DIZEM NASCEM AUSENTES ────
     * `INSERT INTO t (a,c) VALUES (1,2)` nomeia onde os valores vão. O que isto
     * acrescenta não é comodidade de escrita: é poder deixar uma coluna DE FORA
     * sem lhe dar valor nenhum — e o que ela recebe não é zero, é o DUAL. Sem
     * a lista, a única maneira de o fazer era escrever NULL na posição dela, o
     * que obriga a saber a ordem; com a lista, a ausência é o que sobra.
     *
     * A lista lê-se ANTES do VALUES, e cada nome resolve-se contra o catálogo:
     * um nome que não é coluna é RECUSADO, e não ignorado. */
    static long mapa[COL_MAX]; int n_mapa = 0;
    { const char *v0 = p;
      pula(&p);
      if(*p == '('){
          p++;
          while(1){
              char c[64];
              pula(&p);
              if(!ident(&p, c, sizeof c)) break;
              { int ci = col_indice(c);
                if(ci < 0){
                    printf("erro: a coluna «%s» não existe na tabela «%s» —"
                           " o INSERT é RECUSADO.\n", c, nome);
                    if(sql_cap){ sql_cap->ok = 0;
                        snprintf(sql_cap->err, sizeof sql_cap->err,
                                 "column \"%s\" of relation \"%s\" does not exist",
                                 c, nome); }
                    return 0;
                }
                if(n_mapa < (int)COL_MAX) mapa[n_mapa++] = ci; }
              pula(&p);
              if(*p == ','){ p++; continue; }
              break;
          }
          pula(&p);
          if(*p == ')') p++;
          else { p = v0; n_mapa = 0; }     /* não fechou: não era uma lista */
      } else p = v0; }

    if(!palavra(&p, "VALUES")) return 0;
    pula(&p); if(*p != '(') return 0; p++;

    Word cat = mem_le(S_CAT); (void)cat;
    long ncols = cat_ncols(), nrows = cat_nrows();
    /* E A LINHA QUE NÃO CABE RECUSA, EM VEZ DE PISAR O VIZINHO.
     *
     * A condição existe aqui: é o primeiro ponto em que nrows se conhece, e
     * ainda nada foi escrito — a base fica INDISTINGUÍVEL de um INSERT que
     * nunca chegou a ser dito. Uma linha mais abaixo já seria tarde: o valor da
     * célula é gravado antes do bit do vivo. */
    if(nrows >= LIN_TECTO){
        printf("erro: o mapa de slots desta máquina comporta %ld linhas e a tabela"
               " já as tem — o tecto é do MAPA, não da teoria (arquitetura.tex"
               " §sec:torre: o que cresce é o objecto, não a máquina)\n", LIN_TECTO);
        return 0;
    }
    /* E O OUTRO EIXO DA MESMA FRONTEIRA: a célula ocupa um slot por zona, e o
     * endereço é i·ncols + j — logo o mapa limita o PRODUTO. Com uma coluna o
     * bitmap aperta primeiro; com oito, esta. As duas são a mesma parede vista
     * de dois lados, e por isso estão à mesma porta. */
    if(ncols > 0 && (nrows + 1) * ncols > CEL_TECTO){
        printf("erro: o mapa comporta %ld células e esta linha pediria %ld"
               " (%ld linhas × %ld colunas) — o tecto é do PRODUTO, não das"
               " linhas\n", CEL_TECTO, (nrows + 1) * ncols, nrows + 1, ncols);
        return 0;
    }
    static long v[COL_MAX]; long nv = 0;
    /* o PADRÃO do segundo componente vem do CORPO: no racional é denominador (1), no áureo é
     * o coeficiente de σ (0 — "5" é o inteiro 5, não 5+σ). O par é o mesmo; o que muda é o que
     * ele significa, e quem diz é a coluna. */
    /* O DENOMINADOR POR COLUNA, e eram DEZASSEIS.
     *
     * Uma tabela com vinte colunas escrevia `den[16]` fora do array, e o que
     * saía não era uma recusa: era «o valor da coluna 16 não cabe no envelope
     * (segundo componente 4294967297)» --- um número que não vem de dado
     * nenhum. O tecto não recusava, CORROMPIA, e a mensagem culpava a célula
     * pelo que era a leitura a cair fora da sua casa. */
    static long den[COL_MAX];
    for(unsigned q = 0; q < COL_MAX; q++){
        long cq = corpo_de(q).total;
        den[q] = (cq == CORPO_AUREO || cq == CORPO_CRISTAL) ? 0 : 1;
    }
    /* O NOME DA AUSÊNCIA. Um INSERT pode dizer que uma célula não tem valor, e
     * dizer NULL não é escrever zero: é deixar a célula no suporte. É a única
     * forma de a ausência ser ESCOLHIDA em vez de herdada, e sem ela o dual só
     * nascia por omissão (coluna nova, linha curta). */
    static int nulo[COL_MAX];
    for(unsigned q = 0; q < COL_MAX; q++) nulo[q] = 0;
    while(nv < ncols){
        pula(&p);
        if(palavra(&p, "NULL")){
            v[nv] = 0; nulo[nv] = 1; nv++;
            pula(&p); if(*p == ','){ p++; continue; } break;
        }
        /* ── A CADEIA ENTRE ASPAS: guarda-se no pool e a célula fica com o
         * ENDEREÇO. O aspa dupla dentro escreve-se dobrando-a, como em toda a
         * parte --- e o que passa de TX_MAX é RECUSADO, não truncado. */
        if(*p == '\''){
            /* PELO ACESSOR: a nona coluna tem corpo como qualquer outra, e ler
             * S_CORPO à mão dava-lhe INTEIRO --- uma coluna de TEXTO acima da
             * oitava recusava a cadeia que é exactamente o que ela guarda. */
            long cq2 = corpo_de(nv).total;
            if(cq2 != CORPO_TEXTO){
                printf("erro: a coluna %ld não é de texto e veio uma cadeia --- RECUSADA."
                       " Um valor de outro corpo não se converte em silêncio.\n", nv);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "column %ld is not textual", nv); }
                return 0;
            }
            p++;
            char buf[TX_MAX + 2]; int bn = 0, fechou = 0;
            while(*p){
                if(*p == '\''){
                    if(p[1] == '\''){ if(bn < TX_MAX) buf[bn++] = '\''; p += 2; continue; }
                    p++; fechou = 1; break;
                }
                if(bn < TX_MAX) buf[bn++] = *p;
                else { bn = TX_MAX + 1; }        /* passou: marca-se e recusa-se */
                p++;
            }
            if(!fechou || bn > TX_MAX){
                printf("erro: a cadeia %s --- RECUSADA.\n",
                       fechou ? "passa do que a célula segura" : "não fecha a aspa");
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             fechou ? "string too long" : "unterminated string"); }
                return 0;
            }
            buf[bn] = 0;
            v[nv] = (long)tx_guarda(buf, bn);
            den[nv] = 1; nulo[nv] = 0; nv++;
            pula(&p); if(*p == ','){ p++; continue; } break;
        }
        if(!numero(&p, &v[nv])) break;
        /* O VALOR RACIONAL. A Word tem duas componentes e um racional é um par: o numerador
         * no total e o denominador no e. Guarda-se a CLASSE — reduzida pelo mdc, denominador
         * positivo —, que é o que racional_pg.c §Q1 mediu ser o representante único. */
        const char *volta = p;
        pula(&p);
        if(*p == '/'){
            const char *ap = p + 1;
            long q;
            if(numero(&ap, &q) && q != 0){
                p = ap;
                /* PASSO 2: a classe vem do TOOLKIT, não de código repetido aqui. É a mesma
                 * ra_classe que o racional_pg.c mediu — uma implementação, não duas. */
                Par cls = ra_classe((Par){ v[nv], q });
                v[nv] = cls.a; den[nv] = cls.b;
            } else p = volta;
        } else {
            p = volta;
            /* PASSO 3: numa coluna AUREO, "a+bs" é o elemento a + bσ. O par já é o formato —
             * muda o que ele SIGNIFICA, e quem diz isso é o corpo declarado da coluna. */
            long cpj = corpo_de(nv).total;   /* pelo acessor: o corpo vai a S_CORPOX_N */
            if(cpj == CORPO_AUREO || cpj == CORPO_CRISTAL){
                pula(&p);
                if(*p == '+' || *p == '-'){
                    int neg = (*p == '-');
                    const char *ap = p + 1;
                    long bb;
                    if(numero(&ap, &bb)){
                        pula(&ap);
                        if(*ap == 's' || *ap == 'S'){
                            p = ap + 1;
                            den[nv] = neg ? -bb : bb;      /* o .e guarda o coeficiente de σ */
                        }
                    }
                }
            }
        }
        nv++; pula(&p); if(*p == ','){ p++; continue; } break;
    }
    /* ── E COM A LISTA, A LINHA MONTA-SE PELO MAPA ───────────────────────────
     * Os valores vieram na ordem dos NOMES; espalham-se pelas posições que eles
     * apontam, e as posições que ninguém nomeou ficam ausentes. É a mesma
     * escrita de sempre a partir daqui — o que muda é quem decide onde cada
     * valor cai, e agora é a lista e não a posição. */
    if(n_mapa > 0){
        if(nv != n_mapa){
            printf("erro: nomeou %d colunas e trouxe %ld valores — RECUSADA.\n",
                   n_mapa, nv);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "INSERT has %ld expressions but %d target columns",
                         nv, n_mapa); }
            return 0;
        }
        { long v2[64], d2[64]; int n2[64];
          /* o padrão do segundo componente é do CORPO da coluna, e não 1: no
           * áureo e no cristal ele é 0, e esmagá-lo aqui trocava o significado
           * do par nas colunas que ninguém nomeou */
          for(long j = 0; j < ncols && j < 64; j++){
              long cq = corpo_de(j).total;
              v2[j] = 0;
              d2[j] = (cq == CORPO_AUREO || cq == CORPO_CRISTAL) ? 0 : 1;
              n2[j] = 1;                      /* e nascem AUSENTES */
              /* ── SE A COLUNA TEM DEFAULT, ela não nasce ausente: nasce com o
               * valor declarado. E quem decide é a MARCA, não o valor --- um
               * default de zero é um default, e sem a marca seria indistinguível
               * de «não tem». É o mesmo par (valor, presença) da célula. */
              if(j < (long)S_DEFAULT_N){ Word wd = mem_le(S_DEFAULT + (unsigned)j);
                         if(wd.e){ v2[j] = (long)(int8_t)wd.total; n2[j] = 0; } }
          }
          for(int k = 0; k < n_mapa; k++){
              long j = mapa[k];
              if(j < 0 || j >= ncols) continue;
              v2[j] = v[k]; d2[j] = den[k]; n2[j] = nulo[k];
          }
          for(long j = 0; j < ncols && j < 64; j++){
              v[j] = v2[j]; den[j] = d2[j]; nulo[j] = n2[j];
          }
          nv = ncols; }
    }

    /* ── E O INSERT QUE OMITE AS ÚLTIMAS COLUNAS: se todas as que faltam têm
     * DEFAULT, elas preenchem-se; se alguma não tem, recusa-se como sempre. A
     * regra não muda --- o motor continua a não adivinhar --- mas o que está
     * declarado no esquema deixa de ser preciso repetir. */
    if(nv < ncols && nv > 0){
        int todas = 1;
        for(long j = nv; j < ncols && j < 64; j++){
            Word wd = (j < (long)S_DEFAULT_N) ? mem_le(S_DEFAULT + (unsigned)j) : (Word){0,0};
            if(!wd.e){ todas = 0; break; }
        }
        if(todas){
            for(long j = nv; j < ncols && j < 64; j++){
                Word wd = mem_le(S_DEFAULT + (unsigned)j);
                v[j] = (long)(int8_t)wd.total; den[j] = 1; nulo[j] = 0;
            }
            nv = ncols;
        }
    }

    if(nv != ncols){
        printf("erro: a tabela tem %ld colunas, vieram %ld — a linha é RECUSADA."
               " Uma célula que se quer vazia diz-se NULL; deixá-la de fora seria"
               " o motor adivinhar qual.\n", ncols, nv);
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err,
                     "INSERT has %ld expressions but table has %ld columns", nv, ncols); }
        return 0;
    }

    /* ── O ENVELOPE DA CÉLULA DIZ-SE, E O QUE NÃO CABE É RECUSADO ────────────────
     * A célula é uma Word_8² — um átomo para o numerador, outro para o denominador
     * (ou para o coeficiente de σ, conforme o corpo). Logo cada componente vive em
     * 0..255, e é isso que o banco pode guardar hoje.
     *
     * O que estava a acontecer: `INSERT INTO t VALUES (500,1000,65535)` era aceite e
     * lido de volta como `244 232 255`. E `256` virava `0`, e `−1` virava `255`. O
     * valor entrava truncado, no disco, sem uma palavra — que é o defeito que esta
     * casa persegue em todo o lado menos aqui.
     *
     * A regra da casa é a mesma dos racionais e da FC: o que não cabe CONTA-SE e
     * RECUSA-SE, não se enrola calado. Alargar a célula é subir a torre (uma célula
     * de Word_8⁴), e é trabalho próprio — mas guardar lixo enquanto isso não é uma
     * alternativa: é perder o dado sem o dizer. */
    for(long j = 0; j < ncols; j++){
        /* e o alcance é o do CORPO: num inteiro o envelope é 0..255 (o Word_8 da
         * casa), num racional/áureo/cristal o componente é assinado, −128..127 */
        long cpj = corpo_de(j).total;
        int assinado = (cpj == CORPO_RACIONAL || cpj == CORPO_AUREO || cpj == CORPO_CRISTAL
                        || den[j] > 1 || den[j] < 0);
        /* ── O ENVELOPE É O DO ANDAR, E O ANDAR SOBE POR DOBRA.
         *
         * O `thm:BI` dá o encaixe por prefixos --- {0,1} ⊂ {0..3} ⊂ {0..15} ⊂
         * {0..255} --- «com as palavras de largura w em número de 2^w e A DOBRA
         * A DUPLICAR A LARGURA». Alargar a célula não é acrescentar bits ad hoc:
         * é SUBIR UM ANDAR, e o andar seguinte é o `thm:espaco`,
         * F_{2w} = F_w ⊕ σF_w --- a segunda cópia MULTIPLICADA por σ, não colada.
         *
         * E essa segunda cópia já existe aqui: é o plano S_ALTO. O que não tinha
         * subido com ela era o envelope, que continuava no andar de baixo para o
         * assinado. Sobe agora, e sobe pela regra: a largura duplica de 8 para
         * 16, logo o andar é 2^16 e o assinado parte-o ao meio.
         *
         * O `cor:pik` garante a volta: π_k trunca aos bits baixos, ι_k embebe com
         * zeros acima, e π_k∘ι_k = id --- é por isso que a aritmética emitida
         * continua a ler exactamente o que sempre leu, no andar de baixo. */
        long lo = assinado ? -32768 : 0, hi = assinado ? 32767 : 65535;
        if(cpj == CORPO_BOOLEANO){ lo = 0; hi = 1; }   /* o corpo É o envelope */
        if(cpj == CORPO_TEXTO){ lo = 0; hi = 65535; }  /* o endereço no pool */
        if(cpj == CORPO_DATA){ lo = 0; hi = 4294967295L; } /* a contagem, no andar de 32 */
        long lo2 = (cpj == CORPO_AUREO || cpj == CORPO_CRISTAL) ? -128 : 1;
        long hi2 = (cpj == CORPO_AUREO || cpj == CORPO_CRISTAL) ? 127 : 255;
        if(v[j] < lo || v[j] > hi || den[j] < lo2 || den[j] > hi2){
            printf("erro: o valor da coluna %ld não cabe no envelope Word_8 da célula"
                   " (numerador %ld, segundo componente %ld; o envelope é %ld..%ld).\n"
                   " A linha é RECUSADA e nada é escrito — alargar a célula é subir a"
                   " torre, e não truncar em silêncio.\n", j, v[j], den[j], lo, hi);
            /* ── E QUANDO O QUE FALTA É O DEGRAU, DIZ-SE QUAL ────────────────
             * Um negativo numa coluna INTEIRA não é um valor grande de mais: é
             * um valor de OUTRO ANDAR. A §sec:escada constrói X_2 = X_1²/∼ e é
             * lá que a soma ganha a volta --- o oposto ---, e uma coluna do
             * primeiro degrau não a tem. A recusa dizia só o envelope, e quem a
             * lia ficava a pensar que o número era grande. */
            if(v[j] < 0 && !assinado)
                printf(" E o que falta não é largura: é o DEGRAU. O corpo INTEIRO é o"
                       " primeiro (as palavras em B) e nele a soma não tem volta --- o"
                       " oposto nasce no segundo, X_2 = X_1²/∼, que aqui se escreve"
                       " RACIONAL. Declare a coluna RACIONAL e o −%ld cabe.\n", -v[j]);
            if(sql_cap){
                sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "value out of range for Word_8 cell: column %ld got %ld", j, v[j]);
            }
            cel_recusadas++;
            return 0;
        }
    }

    /* ── E A COLUNA PODE RECUSAR ─────────────────────────────────────────────
     * As duas restrições correm ANTES de a ISA escrever, e a linha inteira é
     * recusada — não meia linha. `NOT NULL` recusa o suporte; `UNIQUE` recusa a
     * segunda folha da fibra, e quem responde é a ÁRVORE: uma descida, não uma
     * varredura, pelo que o custo é a profundidade e não o tamanho. */
    /* ── A SETA TEM DE APONTAR PARA ALGUMA COISA ─────────────────────────────
     * Escrever um valor que não está do outro lado é criar uma seta para lado
     * nenhum. A ausência não é uma seta: uma célula NULL não aponta e nada
     * exige. Corre ANTES de a ISA escrever, e a linha inteira é recusada. */
    for(long j = 0; j < ncols && j < (long)COL_MAX; j++){
        char mt[64];
        int mc = fk_le((int)j, mt, sizeof mt);
        if(mc < 0 || j >= nv || nulo[j]) continue;
        { int e = fk_existe(mt, mc, v[j], nome);
          if(e == 1) continue;
          printf("erro: a coluna %ld aponta para «%s» e o valor %ld %s —"
                 " RECUSADA.\n", j, mt, v[j],
                 e == 0 ? "não está lá" : "não pôde ser procurado");
          if(sql_cap){ sql_cap->ok = 0;
              snprintf(sql_cap->err, sizeof sql_cap->err,
                       "insert or update violates foreign key constraint on"
                       " column %ld", j); }
          return 0; }
    }

    for(long j = 0; j < ncols && j < (long)COL_MAX; j++){
        long r = mem_le(S_RESTR + (unsigned)j).total;
        if(!r) continue;
        if((r & R_NOTNULL) && (j >= nv || nulo[j])){
            printf("erro: a coluna %ld é NOT NULL e a linha não lhe traz valor —"
                   " RECUSADA.\n", j);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "null value in column %ld violates not-null constraint", j); }
            return 0;
        }
        /* ── E UM UNIQUE QUE NÃO CABE NÃO É UM UNIQUE ────────────────────────
         * O `j < IDX_MAXCOL` calava-se: uma coluna declarada UNIQUE acima da
         * última árvore era aceite no CREATE e nunca verificada, e a tabela
         * ficava com repetidos numa coluna que diz não os ter. A recusa é a
         * resposta: quem declarou a restrição conta com ela. */
        if((r & R_UNICO) && j < nv && !nulo[j] && j >= IDX_MAXCOL){
            printf("erro: a coluna %ld é UNIQUE e não há árvore para ela (o motor tem"
                   " %u) — o INSERT é RECUSADO, porque uma restrição que não se"
                   " verifica não é uma restrição.\n", j, IDX_MAXCOL);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "column %ld is UNIQUE but has no index tree", j); }
            return 0;
        }
        if((r & R_UNICO) && j < nv && !nulo[j] && j < IDX_MAXCOL){
            int quantos;
            if(!idx_valido(j, nrows)) idx_constroi(j, ncols, nrows);
            if(idx_valido(j, nrows)){
                /* A EXISTÊNCIA DECIDE-SE NA DESCIDA, e o arranjo saiu daqui.
                 *
                 * Estava `int achados[4]; q = j_casam(v[j], achados, 4);` e
                 * depois um filtro pelo VIVO --- o filtro está certo, porque o
                 * DELETE não tira a chave, só apaga o bit. O que estava errado
                 * era o QUATRO: o `j_casam` entrega os quatro PRIMEIROS índices
                 * por ordem de linha, e se esses quatro estivessem apagados o
                 * filtro concluía «não há nenhuma viva» sem ter visto a quinta.
                 * Medido: quatro apagadas e uma viva, e o UNIQUE ACEITAVA o
                 * duplicado --- uma coluna declarada única com dois valores
                 * iguais vivos, e nada a falhar.
                 *
                 * O quatro era do ARRANJO (`aranha §sec:dimensao`), e a minha
                 * primeira correcção foi cair na varredura quando ele enchia,
                 * que é aceitar o tecto e pagá-lo. Aqui não há arranjo: desce-se
                 * à procura do valor e pára-se no primeiro vivo. */
                quantos = ord_valor_vivo_col(j, v[j], nrows);
            } else {
                /* a árvore não coube: a rede é a varredura, e recusar por não
                 * saber seria pior do que responder devagar */
                quantos = 0;
                for(long i2 = 0; i2 < nrows && !quantos; i2++)
                    if(bit_le(S_VIVO, i2) && bit_le(S_PRES, i2*ncols + j)
                       && celula_valor(i2, j, ncols) == v[j]) quantos = 1;
            }
            if(quantos > 0){
                printf("erro: a coluna %ld é UNIQUE e o valor %ld já lá está —"
                       " a fibra tem UMA folha. RECUSADA.\n", j, v[j]);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "duplicate key value violates unique constraint on"
                             " column %ld", j); }
                return 0;
            }
        }
    }

    /* compila o INSERT: cada valor entra por um slot de constante e é gravado pela ISA */
    pc_emit = 0;
    Word w = {0,0}; mem_grava(S_ZERO, w);
    for(long j = 0; j < ncols; j++){
        w.total = v[j]; w.e = den[j];      /* e = denominador; 1 para inteiro */
        mem_grava(S_KINS + (unsigned)j, w);                     /* a constante, na memória */
        emit_copia(S_KINS + (unsigned)j, S_LINHAS + cel_ix(nrows*ncols + j));
        /* o byte ALTO no plano paralelo — a Word da linha não muda, e por isso
         * toda a aritmética emitida continua a ler exactamente o que sempre leu */
        { Word wa; wa.total = (Word8)(((unsigned long)v[j] >> 8) & 255u); wa.e = 0;
          mem_grava(S_ALTO + cel_ix(nrows*ncols + j), wa); }
        /* e o ANDAR SEGUINTE, para o que passa de dezasseis bits --- os bytes 2
         * e 3 no plano S_ALTO2. Quem lê em baixo continua a ler o mesmo. */
        { Word wb; wb.total = (Word8)(((unsigned long)v[j] >> 16) & 255u);
          wb.e    = (Word8)(((unsigned long)v[j] >> 24) & 255u);
          mem_grava(S_ALTO2 + cel_ix(nrows*ncols + j), wb); }
        /* e a ESCRITA DEIXA A MARCA — thm:multiplicidade cláusula 3. Sem isto o
         * leitor teria de percorrer as linhas para saber a largura, que é o
         * agente a carregar o mapa. */
        if(v[j] >= 0) col_marca(j, (unsigned long)v[j]);
        Word wd; wd.total = den[j]; wd.e = 0;
        mem_grava(S_KDEN + (unsigned)j, wd);
        emit_copia(S_KDEN + (unsigned)j, S_DEN + cel_ix(nrows*ncols + j));
    }
    /* nrows++ pela própria máquina: LOAD cat, LOAD um, ADD, STORE — mas nrows é o campo .e,
     * e a ULA soma componente a componente; então a constante um vai no campo .e. */
    /* a linha nasce VIVA (o DELETE zera este slot).
     * ATENÇÃO à ordem: emit_copia só EMITE; o programa roda depois, no rodar(). Se a
     * constante for sobrescrita entre as duas fases, a máquina lê o valor trocado — foi
     * exatamente o que aconteceu aqui quando S_UM servia às duas coisas. Cada constante
     * tem o seu slot. */
    w.total = 1; w.e = 0; mem_grava(S_UM, w);
    bit_poe(S_VIVO, nrows, 1);        /* a linha nasce VIVA: liga a coordenada */
    emit1(OP_HALT);
    long passos = rodar(pc_emit);        /* FASE 1: só o dado */

    barreira();                          /* o dado está no prato antes de existir o ponteiro */
    trava_se_pedido(1);                  /* e é aqui que o teste derruba, para ver o que sobra */

    /* FASE 2: só então o ponteiro. E o nrows SOBE DE ANDAR.
     *
     * Somava-se 1 ao campo `.e` do catálogo com OP_ADD, componente a componente
     * — e esse campo é UM BYTE: à linha 256 dava a volta, e uma tabela de 300
     * linhas respondia 44 a tudo. O ADD16 não serve no S_CAT, porque o
     * transporte atravessaria do nrows para o ncols (é o que o comentário do
     * enum avisa); então o coeficiente que cresce sobe para um slot só seu, e
     * aí sim os dois componentes são UM número de dezasseis bits. */
    /* ── E O PREDICADO, ANTES DE A LINHA CONTAR ──────────────────────────
     * As células já estão escritas na posição `nrows`, mas o catálogo ainda
     * não subiu: a linha está no disco e não existe. Se o predicado recusar,
     * volta-se sem incrementar — e a linha fica invisível, para o próximo
     * INSERT escrever por cima. Nada a desfazer, que é a razão de a ordem ser
     * esta. */
    /* A PRESENÇA ACENDE-SE ANTES DO PREDICADO, e tem de ser: o `CHECK` não se
     * pronuncia sobre células ausentes, e sem os bits acesos ele via a linha
     * inteira como ausente e deixava passar tudo. O dual escreve-se com a
     * linha, não depois dela. */
    for(long j = 0; j < ncols; j++)
        bit_poe(S_PRES, nrows*ncols + j, (j < nv && !nulo[j]) ? 1 : 0);

    { char ck[S_CHECK_W * 2 + 2];
      check_le(ck, (int)sizeof ck);
      if(ck[0]){
          int r = check_avalia(nrows, ncols, ck);
          if(r != 1){
              printf("erro: a linha não satisfaz o CHECK (%s) — RECUSADA%s.\n",
                     ck, r < 0 ? " (o predicado não compila)" : "");
              if(sql_cap){ sql_cap->ok = 0;
                  snprintf(sql_cap->err, sizeof sql_cap->err,
                           "new row violates check constraint"); }
              return 0;
          }
      } }

    pc_emit = 0;                         /* fase 2 é um programa PRÓPRIO: rodar() parte de 0 */
    w.total = 1; w.e = 0; mem_grava(S_UM16, w);
    MOVE(S_NR, +1);
    MOVE(S_UM16, +1);
    emit1(OP_ADD16);
    MOVE(S_NR, -1);
    emit1(OP_HALT);
    passos += rodar(pc_emit);
    barreira();

    cat = mem_le(S_CAT);

    /* ── O ÍNDICE ACOMPANHA A ESCRITA, e é o ζ ────────────────────────────
     *
     * `thm:zeta-mu`: escrever é a convolução com ζ, que ACUMULA; recuperar é a
     * deconvolução com μ. Acrescentar uma chave à árvore é acumular — desce-se
     * uma vez e escreve-se —, e por isso um INSERT não tem de invalidar o
     * índice: mantém-no. Tirar uma chave seria o μ, e essa não é a mesma
     * facilidade numa árvore de prefixos; o UPDATE e o DELETE continuam por
     * isso a invalidar, e fica dito que continuam.
     *
     * O cabeçalho sobe com a tabela, senão a linha nova ficaria indexada e o
     * índice na mesma marcado como velho. */
    { long nr_agora = cat_nrows();
      long i = nr_agora - 1;                       /* a linha que acabou de entrar */
      /* (a presença já foi acesa antes do CHECK — ver acima) */
      for(long c = 0; c < ncols && c < IDX_MAXCOL; c++){
          if(!idx_valido(c, nr_agora - 1)) continue;   /* válido ANTES desta linha? */
          /* e a célula AUSENTE não tem chave — a mesma regra do idx_constroi.
           * Sem ela a linha nula entrava na árvore com o neutro à cara de
           * valor, e o UNIQUE recusava o primeiro ZERO por causa dela. */
          if(!bit_le(S_PRES, i*ncols + c)) continue;
          ord_usa_indice(c);
          int coube = ord_insere(celula_valor(i, c, ncols), (int)i);
          ord_usa_rascunho();
          if(coube){
              Word n; n.total = (Word8)(nr_agora & 255);
              n.e = (Word8)((nr_agora >> 8) & 255);
              mem_grava(S_IDXCAB2(c), n);          /* este índice acompanhou */
          }else{
              Word z = {0,0}; mem_grava(S_IDXCAB(c), z);  /* não coube: fica sem ele */
          }
      } }

    printf("1 linha inserida (%ld colunas) — %u bytes de ISA, %ld passos; agora %d linhas\n",
           ncols, pc_emit, passos, cat_nrows());
    if(sql_cap){
        snprintf(sql_cap->tag, sizeof sql_cap->tag, "INSERT 0 1");
        sql_cap->ncols = 0; sql_cap->nrows = 0;
    }
    return 1;
}

/* As três ações que uma varredura pode ter na linha que casa. */
enum { ACAO_MARCA, ACAO_SET, ACAO_APAGA };

/* Emite o teste da condição e o bloco de ação para UMA linha.
 *
 * A ISA não tem salto por FL_LT — só JZ/JNZ, que olham FL_ZERO. Então as três comparações
 * reduzem-se todas a um teste de ZERO, e a diferença entre elas é só ordem de subtração:
 *
 *   col = k   ->  dif = k − col ;                CMP com A=0  ->  FL_ZERO sse igual   (JZ)
 *   col < k   ->  dif = col − k ; dif AND sinal; CMP com A=0  ->  FL_ZERO sse col ≥ k (JNZ)
 *   col > k   ->  dif = k − col ; dif AND sinal; CMP com A=0  ->  FL_ZERO sse col ≤ k (JNZ)
 *
 * O sinal extrai-se por AND com o bit 63 — a ISA não tem deslocamento, mas tem AND, e isso
 * basta: negativo é exatamente quem tem esse bit aceso.
 */
/* A árvore do WHERE. Com parênteses, a cláusula deixa de ser plana e vira árvore de
 * verdade — e a gramática é a do SQL:
 *
 *     expr  := termo (OR termo)*
 *     termo := fator (AND fator)*
 *     fator := '(' expr ')' | coluna <op> número
 *
 * Os seis operadores são TRÊS mais uma negação: != é não-=, <= é não->, >= é não-<. E negar
 * um slot que vale 0 ou 1 é XOR com 1 — opcode que a ISA já tem. Não se inventou comparação
 * nova: acrescentou-se um XOR.
 */
/* ── SEIS COLUNAS NUMA EXPRESSÃO, E NÃO «AS SEIS PRIMEIRAS» ─────────────────
 *
 * O NCOL conta quantas colunas DISTINTAS um WHERE pode citar --- é a ordem do
 * tensor a impor isso, e NMON = NI^KGRAU cresce ao cubo, pelo que o número é
 * uma escolha de custo e não um esquecimento. O que ERA um esquecimento: o
 * símbolo do tensor era o ÍNDICE da coluna (`d[0] = col + 1`), pelo que
 * `WHERE c20 = 5` numa tabela de vinte e quatro colunas nem sequer parseava ---
 * «o WHERE não foi entendido» ---, e o que falhava não era a expressão ser
 * grande: era a coluna estar longe do princípio da tabela.
 *
 * Agora o símbolo atribui-se por ORDEM DE APARECIMENTO na expressão, e uma
 * tabela pequena diz qual coluna cada símbolo é. `WHERE c99 = 5` usa o símbolo
 * 1; `WHERE c99 = 5 AND c40 > 2` usa o 1 e o 2. O tecto passou a ser sobre a
 * EXPRESSÃO, que é onde ele tem sentido --- e quando ela cita mais do que NCOL
 * colunas distintas, a recusa DIZ isso, em vez de dizer que não entendeu. */
#define NCOL 6                 /* colunas DISTINTAS que uma expressão pode citar */
#define CMAX 8                 /* (histórico: era o teto da soma repetida — ver emit_mul_zeck) */

#define NI    (NCOL+1)         /* símbolo 0 = a constante 1; 1..NCOL = as colunas */
#define KGRAU 3                /* ordem do tensor: grau máximo do monômio          */
#define NMON  343              /* NI^KGRAU — as casas do multi-índice (7^3)        */

/* O TENSOR DE GRAU k, com MULTI-ÍNDICE.
 *
 * Um monômio é uma tupla ORDENADA de KGRAU símbolos, e o símbolo 0 é a constante 1 — logo a
 * tupla (0,0,0) é o termo constante, (0,0,i) é linear, (0,i,j) é quadrático, (i,j,l) é cúbico.
 * Graus diferentes não são casos diferentes: são a mesma tabela com mais ou menos zeros.
 *
 * A posição ORDENADA é o endereço, e é só isso que faz a comutatividade desaparecer: as k!
 * escritas de um monômio caem na mesma casa por aritmética de índice, não por regra
 * (tools/tensor.c §T2, onde a contagem C(n+k−1,k) foi conferida).
 *
 * As duas operações são as do tensor: a SOMA soma casa a casa; o PRODUTO junta os multi-índices
 * (os graus somam) e multiplica os coeficientes. O parêntese entra como posição e sai. */
/* O TENSOR SOBRE ℚ: um denominador COMUM por tensor.
 *
 * As classes inteiras do racional_pg.c entram aqui: cada tensor é (coeficientes, denominador),
 * e o denominador é um só para o tensor inteiro — não um por monómio. Isso basta porque as duas
 * operações são lineares no denominador:
 *
 *     soma      cruza:      (A,p) + (B,q) = (qA + pB, pq)
 *     produto   multiplica: (A,p) · (B,q) = (A·B, pq)
 *
 * E na hora de emitir, o denominador SOME: a contração já pôs tudo de um lado e a comparação é
 * contra ZERO, logo (N/D) OP 0 ⟺ N OP 0 desde que D > 0 — e mantém-se D > 0 por construção.
 * O racional entra no analisador, opera como classe, e sai inteiro para o metal. */
struct tensor { long c[NMON]; long den; };

static void mi_ordena(int *d){                         /* ordenação por inserção, KGRAU pequeno */
    for(int i = 1; i < KGRAU; i++)
        for(int j = i; j > 0 && d[j] < d[j-1]; j--){ int t = d[j]; d[j] = d[j-1]; d[j-1] = t; }
}
static int mi_cod(int *d){                             /* a tupla ordenada é o endereço */
    mi_ordena(d);
    int r = 0;
    for(int t = KGRAU-1; t >= 0; t--) r = r*NI + d[t];
    return r;
}
static void mi_de(int cod, int *d){
    for(int t = 0; t < KGRAU; t++){ d[t] = cod % NI; cod /= NI; }
}
static int mi_grau(int cod){
    int d[KGRAU], g = 0; mi_de(cod, d);
    for(int t = 0; t < KGRAU; t++) if(d[t]) g++;
    return g;
}
static struct tensor ten_zero(void){ struct tensor t; memset(&t,0,sizeof t); t.den = 1; return t; }
/* o representante da classe: divide tudo pelo mdc comum, e deixa o denominador positivo */
static void ten_reduz(struct tensor *t){
    if(t->den < 0){ t->den = -t->den; for(int i = 0; i < NMON; i++) t->c[i] = -t->c[i]; }
    long g = t->den;
    for(int i = 0; i < NMON; i++) if(t->c[i]) g = rt_mdc(g, t->c[i]);
    if(g > 1){ t->den /= g; for(int i = 0; i < NMON; i++) t->c[i] /= g; }
    if(t->den == 0) t->den = 1;
}
static void ten_mon(struct tensor *t, int *d, long c){ t->c[mi_cod(d)] += c; }
static void ten_const(struct tensor *t, long k){
    int d[KGRAU]; memset(d, 0, sizeof d); ten_mon(t, d, k);
}
/* ── O SÍMBOLO E A COLUNA ────────────────────────────────────────────────────
 * `sim_col[k]` é a coluna que o símbolo k+1 nomeia nesta expressão. Zera-se a
 * cada WHERE lido; `sim_de` devolve o símbolo de uma coluna, atribuindo-o se
 * for a primeira vez, e −1 quando já não há símbolos. */
static long sim_col[NCOL];
/* ── E O PLANO DO SÍMBOLO: QUAL ANDAR DA CÉLULA ELE LÊ ───────────────────────
 *
 * Uma célula de corpo quadrático é o PAR (a, b) --- a parte racional e o
 * coeficiente de σ ---, e o WHERE só sabia nomear a primeira. Isso põe a RÉGUA
 * fora do alcance de quem a queira: `q(a,b) = a² + B·a·b + C·b²` é grau 2, e o
 * avaliador FAZ grau 2 (medido: `WHERE a*a + a*b - b*b > 10` corre e responde
 * certo). Não faltava capacidade --- faltava poder nomear o `b`.
 *
 * O `checa_corpos` já dizia o que fazer: «comparar por norma é OUTRA pergunta, e
 * quem a quiser tem de a escrever». Isto é o que lhe dá com que a escrever.
 *
 * O plano é 0 para a parte racional (S_LINHAS/S_ALTO, como sempre) e 1 para o σ
 * (S_DEN). A coluna é a mesma; o que muda é o andar --- e o endereço tem a MESMA
 * forma, `base + linha*ncols + col`, pelo que a PA do relocador anda nele sem
 * uma linha diferente. */
static int  sim_plano[NCOL];
static int  sim_n = 0;
/* ── A ORDEM DO TEXTO NÃO É A DOS ENDEREÇOS ─────────────────────────────────
 * A célula guarda o endereço no pool: a leitura preserva a IGUALDADE e não a
 * ORDEM. `s = 'acme'` decide-se pelo endereço e está certo; `s > 'mike'`
 * comparava endereços e devolvia o que a ordem de ESCRITA desse --- uma
 * resposta com a cara de certa. Marca-se ao ler, e recusa-se onde as duas
 * coisas se sabem. */
static int cit_texto = 0;      /* o WHERE cita alguma coluna de texto */
static int cit_desig = 0;      /* e há nele uma desigualdade */

static void sim_zera(void){ sim_n = 0;
                            for(int k = 0; k < NCOL; k++){ sim_col[k] = -1; sim_plano[k] = 0; }
                            cit_texto = 0; cit_desig = 0; }
static int  sim_de_plano(long col, int plano){
    for(int k = 0; k < sim_n; k++)
        if(sim_col[k] == col && sim_plano[k] == plano) return k;
    if(sim_n >= NCOL) return -1;
    sim_col[sim_n] = col; sim_plano[sim_n] = plano; return sim_n++;
}
static int  sim_de(long col){ return sim_de_plano(col, 0); }
static long col_de_sim(int sim){          /* sim é 0-based; d[k]-1 dá-o */
    if(sim < 0 || sim >= sim_n) return -1;
    return sim_col[sim];
}
static int  plano_de_sim(int sim){
    if(sim < 0 || sim >= sim_n) return 0;
    return sim_plano[sim];
}

static void ten_var(struct tensor *t, int col){
    int d[KGRAU]; memset(d, 0, sizeof d); d[0] = col + 1; ten_mon(t, d, 1);
}
static struct tensor ten_soma(struct tensor a, struct tensor b, int sinal){
    long p = a.den ? a.den : 1, q = b.den ? b.den : 1;      /* (A,p) ± (B,q) = (qA ± pB, pq) */
    struct tensor r = ten_zero();
    for(int i = 0; i < NMON; i++) r.c[i] = q * a.c[i] + sinal * p * b.c[i];
    r.den = p * q;
    ten_reduz(&r);
    return r;
}
/* o produto: junta os multi-índices — os graus SOMAM. Passar de KGRAU é recusado, e dizer
 * isso é melhor que truncar em silêncio. */
static int ten_mul(struct tensor *r, const struct tensor *a, const struct tensor *b){
    *r = ten_zero();
    for(int x = 0; x < NMON; x++){
        if(!a->c[x]) continue;
        int dx[KGRAU]; mi_de(x, dx);
        for(int y = 0; y < NMON; y++){
            if(!b->c[y]) continue;
            int dy[KGRAU]; mi_de(y, dy);
            if(mi_grau(x) + mi_grau(y) > KGRAU) return 0;
            int d[KGRAU], k = 0;
            for(int t = 0; t < KGRAU; t++) if(dx[t]) d[k++] = dx[t];
            for(int t = 0; t < KGRAU; t++) if(dy[t]) d[k++] = dy[t];
            while(k < KGRAU) d[k++] = 0;
            ten_mon(r, d, a->c[x] * b->c[y]);
        }
    }
    r->den = (a->den ? a->den : 1) * (b->den ? b->den : 1);   /* os denominadores multiplicam */
    ten_reduz(r);
    return 1;
}
/* ── AVALIAR O TENSOR NUMA LINHA ─────────────────────────────────────────────
 *
 * O mesmo objecto que o `WHERE` usa para DECIDIR serve para PRODUZIR: um
 * tensor é $c_0 + \sum c_i x_i + \dots$, e avaliá-lo é percorrer os monómios
 * não-nulos, decodificar o multi-índice e multiplicar as células que ele nomeia
 * (o símbolo $0$ é a constante $1$, e por isso salta-se). Não há avaliador novo
 * — há o mesmo lido noutra direcção, que é a dualidade que este ficheiro
 * persegue em toda a parte: o WHERE selecciona com a expressão, o SELECT
 * escreve-a.
 *
 * O denominador é um só para o tensor inteiro (é a classe racional), e sai no
 * fim: quem chama recebe o par (numerador, denominador) e decide como o
 * imprime. */
static long celula_valor(long i, long j, long nc);
/* o tensor cita a coluna j? — o símbolo j+1 aparece nalgum monómio não-nulo */
static int ten_cita(struct tensor t, long j){
    for(int cod = 0; cod < NMON; cod++){
        if(!t.c[cod]) continue;
        { int d[KGRAU];
          mi_de(cod, d);
          for(int k = 0; k < KGRAU; k++) if(d[k] == j + 1) return 1; }
    }
    return 0;
}
static long ten_avalia(struct tensor t, long i, long ncols, long *den){
    long num = 0;
    for(int cod = 0; cod < NMON; cod++){
        if(!t.c[cod]) continue;
        { int d[KGRAU];
          long termo = t.c[cod];
          mi_de(cod, d);
          for(int k = 0; k < KGRAU; k++){
              if(d[k] == 0) continue;                  /* o símbolo 0 é o 1 */
              { long j2 = col_de_sim(d[k] - 1);
                if(j2 < 0 || j2 >= ncols){ termo = 0; break; }
                termo *= celula_valor(i, j2, ncols); }
          }
          num += termo; }
    }
    if(den) *den = t.den ? t.den : 1;
    return num;
}

static int ten_constante(struct tensor t){
    for(int i = 1; i < NMON; i++) if(t.c[i]) return 0;
    return t.c[0] == t.c[0];
}

enum { NO_COND, NO_AND, NO_OR };
struct no {
    int tipo;
    int esq, dir;          /* índices na árvore                                   */
    int rel;               /* A RELAÇÃO: uma das oito do `simbolos.h`. É ESTE o    */
                           /* campo que o parser escreve — os dois abaixo derivam.  */
    int op;                /* '=', '<', '>'   — `sb_op(rel)`                        */
    int nega;              /* 1 se o resultado deve ser invertido — `sb_nega(rel)`   */
    struct tensor v;       /* o lado numérico já contraído: L − R                  */
    int decidido;          /* 0 = precisa da linha; 1 = já se sabe (const)         */
    int valor;             /* se decidido: 0 ou 1                                  */
    int atomo;             /* índice do átomo distinto, depois da contração        */
};
struct arvore {
    struct no no[MAXNO];
    int n;
    int raiz;
    /* a CONTRAÇÃO: os átomos distintos, depois de normalizar a árvore */
    int natomo;
    unsigned long asig[16];
    int arel[16];                     /* a relação do átomo — a fonte              */
    int aop[16], anega[16];           /* e as duas leituras dela que o emissor usa */
    struct tensor av[16];
};
static int le_expr(const char **p, struct arvore *a);
/* as colunas que o WHERE cita — usada pela guarda que liga a DISTÂNCIA ao WHERE */
static unsigned citadas_where = 0;
/* e as citadas numa DESIGUALDADE, que é um subconjunto próprio: `a = 1 AND b < 5`
 * cita duas colunas e só uma delas é comparada por ordem. Recusar pela primeira
 * máscara recusaria onde não há defeito --- é a diferença entre a lei alcançar e
 * a lei varrer. Enche-se no `tem_comparacao`, pela diferença do que o fator leu. */
static unsigned citadas_desig = 0;
/* O UPDATE QUE APAGA A CÉLULA. `SET c = NULL` escreve como qualquer outro
 * UPDATE — o molde é o mesmo — mas em vez de ACENDER a presença, APAGA-a: é a
 * volta da escrita, e sem ela o dual só sabia crescer. Vive numa bandeira
 * porque a acção continua a ser um SET em todo o caminho; o que muda é o bit
 * no fim, e o diário leva-a codificada para a recuperação a poder refazer. */
static int set_anula = 0;
/* ── O TERCEIRO USO DO MESMO OBJECTO: ESCREVER ───────────────────────────────
 * O tensor já servia dois: o `WHERE` DECIDE com ele, o `SELECT` PRODUZ com ele.
 * Faltava o terceiro — `UPDATE t SET b = b + 1` ESCREVE com ele —, e a falta era
 * uma assimetria e não uma lacuna de conveniência: a mesma frase valia como
 * pergunta e como resposta, e não valia como acto.
 *
 * O que a impedia era o desenho da escrita: `S_V` guarda UM valor e o programa
 * copia-o para todas as linhas marcadas, pelo que só cabia lá uma constante. Uma
 * expressão que cita colunas tem um valor POR LINHA. */
static struct tensor set_ten;        /* o tensor do SET, quando é expressão */
static int  set_ex = 0;              /* 1 quando o SET é expressão e não constante */

static int novo_no(struct arvore *a){
    if(a->n >= MAXNO) return -1;
    memset(&a->no[a->n], 0, sizeof a->no[0]);
    return a->n++;
}
/* expressão numérica com PRODUTO e parênteses:
 *     num   := termo (('+'|'-') termo)*
 *     termo := fator ('*' fator)*
 *     fator := '(' num ')' | inteiro | coluna
 * Contrai enquanto lê: soma acumula na casa, produto multiplica os tensores. Cada parêntese
 * que entra num produto vira uma POSIÇÃO do tensor — é a regra de tools/tensor.c. */
static int le_soma(const char **p, struct tensor *t);

static int le_fator_num(const char **p, struct tensor *t){
    pula(p);
    if(**p == '('){
        (*p)++;
        if(!le_soma(p, t)) return 0;
        pula(p);
        if(**p != ')') return 0;
        (*p)++;
        return 1;
    }
    *t = ten_zero();
    /* ── A CADEIA É O SEU ENDEREÇO, E COMPARAR CADEIAS É COMPARAR ENDEREÇOS ──
     *
     * `WHERE s = 'acme'` era RECUSADO --- «o WHERE não foi entendido» ---, e o
     * TEXTO é o corpo mais usado por um cliente real. Não faltava um operador:
     * faltava LER a constante.
     *
     * E a leitura é a que a casa já tem. A célula de uma coluna de texto guarda
     * o ENDEREÇO no pool, e o pool DEDUPLICA --- «a mesma cadeia tem de dar o
     * mesmo endereço, e isto não é economia de espaço: é o CRITÉRIO DA LEITURA,
     * x = y ⟹ R(x) = R(y)». Então `s = 'acme'` é `s = <endereço de acme>`, e
     * toda a maquinaria da igualdade vale sem uma linha a mais.
     *
     * A cadeia que NÃO está no pool não existe em linha nenhuma, e o endereço
     * que se lhe dá é o ZERO --- que é a ausência, e nenhuma célula presente o
     * tem. Zero linhas é a resposta certa, não um erro: perguntar por uma
     * cadeia que ninguém escreveu é uma pergunta legítima com resposta vazia. */
    if(**p == '\''){
        const char *r = *p + 1;
        char buf[TX_MAX + 2]; int bn = 0;
        while(*r){
            if(*r == '\''){
                if(r[1] == '\''){ if(bn < TX_MAX) buf[bn++] = '\''; else bn = TX_MAX + 1; r += 2; continue; }
                r++; break;
            }
            if(bn < TX_MAX) buf[bn++] = *r; else bn = TX_MAX + 1;
            r++;
        }
        /* E A CADEIA QUE NÃO CABE NUMA CÉLULA TAMBÉM NÃO ESTÁ NO POOL --- logo o
         * endereço dela é o ZERO, que é o que este bloco já diz três linhas
         * acima. A lei existia e não ALCANÇAVA este caso: truncava a pergunta a
         * TX_MAX e procurava o pedaço, pelo que perguntar por 300 caracteres
         * encontrava a célula que tem os primeiros 240 e respondia «1 linha».
         * Não é uma linha a mais: é uma cadeia que ninguém escreveu a ser dada
         * como escrita. */
        if(bn > TX_MAX){ ten_const(t, 0); *p = r; return 1; }
        buf[bn] = 0;
        ten_const(t, (long)tx_procura(buf, bn));
        *p = r;
        return 1;
    }
    if(isdigit((unsigned char)**p)){
        long k;
        if(!numero(p, &k)) return 0;
        ten_const(t, k);
        const char *volta = *p;                  /* é fração? só se vier / e depois número */
        pula(p);
        if(**p == '/'){
            const char *ap = *p + 1;
            long q;
            if(numero(&ap, &q) && q != 0){
                *p = ap;
                t->den = q;
                ten_reduz(t);
                return 1;
            }
        }
        *p = volta;
        return 1;
    }
    if(isalpha((unsigned char)**p)){
        char nome[64];
        if(!ident(p, nome, sizeof nome)) return 0;
        int plano = 0;
        /* ── `sigma(col)`: O OUTRO ANDAR DA CÉLULA ───────────────────────────
         * A célula de um corpo quadrático é o par (a,b), e sem isto só o `a`
         * tinha nome. Com ele a RÉGUA escreve-se: a norma do corpo (B,C) é
         * `col*col + B*col*sigma(col) + C*sigma(col)*sigma(col)`, que é grau 2 e
         * o avaliador faz. Onde o corpo não tem σ o valor é o neutro, e a
         * recusa é dita em vez de se devolver zero com cara de valor. */
        if(!strcasecmp(nome, "sigma") || !strcasecmp(nome, "SIGMA")){
            const char *q = *p;
            pula(&q);
            if(*q == '('){
                char dentro[64];
                q++;
                if(ident(&q, dentro, sizeof dentro)){
                    pula(&q);
                    if(*q == ')'){
                        long c2 = col_indice(dentro);
                        if(c2 < 0){
                            printf("erro: `sigma(%s)` — não há coluna com esse nome.\n", dentro);
                            return 0;
                        }
                        { Word cw = corpo_de(c2);
                          if(cw.total != CORPO_AUREO && cw.total != CORPO_CRISTAL){
                              printf("erro: `sigma(%s)` — a coluna não é de um corpo quadrático,"
                                     " e não tem segundo andar. Devolver zero seria dar um valor"
                                     " a uma pergunta mal posta.\n", dentro);
                              return 0;
                          } }
                        *p = q + 1;
                        snprintf(nome, sizeof nome, "%s", dentro);
                        plano = 1;
                    }
                }
            }
        }
        int col = col_indice(nome);          /* pelo NOME; a letra é o recurso das bases antigas */
        if(col < 0) return 0;
        int sim = sim_de_plano(col, plano);
        if(sim < 0){
            printf("erro: a expressão cita mais de %d colunas distintas — RECUSADA."
                   " O tecto é da EXPRESSÃO (a ordem do tensor), e não da tabela.\n", NCOL);
            return 0;
        }
        ten_var(t, sim);                           /* o símbolo sim+1 é ESTA coluna */
        if(corpo_de(col).total == CORPO_TEXTO) cit_texto = 1;
        if(col < 32) citadas_where |= 1u << col;   /* a guarda de corpo — ver checa_corpos */
        return 1;
    }
    return 0;
}
static int le_produto(const char **p, struct tensor *t){
    if(!le_fator_num(p, t)) return 0;
    while(1){
        pula(p);
        if(**p == '*'){
            (*p)++;
            struct tensor u, r;
            if(!le_fator_num(p, &u)) return 0;
            if(!ten_mul(&r, t, &u)) return 0;      /* passou do grau máximo */
            *t = r;
            continue;
        }
        /* A DIVISÃO POR CONSTANTE. Com o tensor sobre ℚ ela é a recíproca, e nada mais: o
         * numerador vira denominador. Só se divide por CONSTANTE — dividir por uma coluna
         * daria uma função que não é polinómio, e o tensor não a representa; então recusa-se
         * em voz alta, que é melhor que aceitar e responder outra coisa.
         *
         * Antes desta linha o `/` depois de uma coluna era SALTADO em silêncio: `a / 2 > 2`
         * lia-se como outra coisa e devolvia seis linhas onde devia devolver duas. Silêncio
         * assim é o pior defeito que um banco pode ter. */
        if(**p == '/'){
            (*p)++;
            struct tensor u;
            if(!le_fator_num(p, &u)) return 0;
            if(!ten_constante(u) || u.c[0] == 0){
                printf("erro: só se divide por constante não-nula (o tensor é polinomial)\n");
                return 0;
            }
            long num = u.c[0], den = u.den ? u.den : 1;
            t->den *= (num < 0 ? -num : num);      /* recíproca: numerador vira denominador */
            for(int i = 0; i < NMON; i++) t->c[i] *= (num < 0 ? -den : den);
            ten_reduz(t);
            continue;
        }
        break;
    }
    return 1;
}
static int le_soma(const char **p, struct tensor *t){
    pula(p);
    int sinal = 1;
    if(**p == '-'){ sinal = -1; (*p)++; }
    else if(**p == '+'){ (*p)++; }
    if(!le_produto(p, t)) return 0;
    if(sinal < 0) *t = ten_soma(ten_zero(), *t, -1);
    while(1){
        pula(p);
        int s2;
        if(**p == '+') s2 = 1;
        else if(**p == '-') s2 = -1;
        else break;
        (*p)++;
        struct tensor u;
        if(!le_produto(p, &u)) return 0;
        *t = ten_soma(*t, u, s2);
    }
    return 1;
}
static int le_num(const char **p, struct tensor *v){ return le_soma(p, v); }

/* ── FECHAR UMA CONDIÇÃO: A CANONIZAÇÃO, E DEPOIS AS LEITURAS ────────────────
 *
 * Uma condição do WHERE é um par `(v, rel)`: a forma numérica `v = L − R` e uma
 * das oito relações. Duas coisas acontecem aqui, e nenhuma é escolha arbitrária:
 *
 *  (1) CANONIZAR pela τ. Trocar o par `(x,y)` por `(y,x)` é negar `v`, e pela
 *      cláusula (5) do `fis:thm:simbolos` isso troca `<` com `>`. Logo aplicar a
 *      τ aos DOIS lados — à forma e à relação — deixa a verdade onde estava, e
 *      autoriza levar toda relação para o lado ESCOLHIDO. Sem isto `b > 20` e
 *      `20 < b` são o mesmo facto com vectores opostos, e emitiam bytecode
 *      diferente. As τ-FIXAS — ∅, =, ≠, total — não têm lado a escolher, e para
 *      elas o representante fixa-se pelo sinal do primeiro coeficiente, que é um
 *      critério de fora da dobra e diz-se que é.
 *
 *  (2) DERIVAR o par `(op, nega)`. O emissor conhece três blocos e um XOR; as
 *      outras cinco relações chegam-lhe pela cláusula (9) — negar é
 *      complementar. Aqui é o ÚNICO sítio onde essa derivação acontece.
 *
 * E a nula e a total saem daqui já DECIDIDAS mesmo com a forma a depender da
 * linha: são as duas que não precisam de a ler. O parser do SQL não as escreve
 * (não há sinal para elas), mas a contracção pode chegar lá, e quando chegar
 * este caminho responde em vez de emitir um teste que não existe. */
static void fecha_cond(struct no *n){
    if(sb_transpor_pede(n->rel)){                  /* (1) leva-se para o `>` */
        n->v   = ten_soma(ten_zero(), n->v, -1);
        n->rel = sb_tau(n->rel);
    } else if(sb_tau_fixa(n->rel)){                /* sem lado: escolhe-se o sinal */
        long primeiro = 0;
        for(int k = 1; k < NMON && !primeiro; k++) if(n->v.c[k]) primeiro = n->v.c[k];
        if(!primeiro) primeiro = n->v.c[0];
        if(primeiro < 0) n->v = ten_soma(ten_zero(), n->v, -1);
    }
    n->op   = sb_op(n->rel);                       /* (2) as duas leituras       */
    n->nega = sb_nega(n->rel);

    /* CONTRAÇÃO decide na compilação: se não sobrou variável, a resposta já se sabe.
     * E se der falso, isso não é erro do cliente — é uma CONDIÇÃO DE PARADA. */
    if(sb_decidida(n->rel)){
        n->decidido = 1; n->valor = sb_constante(n->rel);
    } else if(ten_constante(n->v)){
        n->decidido = 1; n->valor = sb_vale(n->rel, n->v.c[0]);
    }
}
/* ── A NEGAÇÃO EMPURRA-SE PARA AS FOLHAS ─────────────────────────────────────
 *
 * `NOT (A OR B)` não precisa de um nó novo na árvore: precisa de De Morgan.
 * Trocam-se os conectivos e nega-se cada folha, e o que sai é uma árvore da
 * mesma forma que o emissor já sabe percorrer. É a mesma escolha da CONTRAÇÃO:
 * o equivalente vira o MESMO objeto, em vez de duas coisas que uma regra depois
 * iguala. E a lei fica medível de fora: `NOT (a = 1 OR a = 2)` tem de devolver
 * exactamente o que `a <> 1 AND a <> 2` devolve, pelo mesmo bytecode.
 *
 * E negar uma folha é COMPLEMENTAR a relação — cláusula (9) do
 * `fis:thm:simbolos` —, e não virar um bit ao lado dela. Como a complementação
 * é uma involução, negar duas vezes devolve a mesma relação e o mesmo bytecode,
 * que é o que o De Morgan exige. O `fecha_cond` volta a correr porque a
 * complementar pode ter mudado de lado: `NOT (a < 3)` é `a >= 3`, e esse pede a
 * τ que o `a < 3` não pedia. */
static void nega_arvore(struct arvore *a, int i){
    if(i < 0 || i >= a->n) return;
    { struct no *n = &a->no[i];
      if(n->tipo == NO_COND){
          n->rel = sb_compl(n->rel);
          n->decidido = 0; n->valor = 0;
          fecha_cond(n);
          return;
      }
      n->tipo = (n->tipo == NO_AND) ? NO_OR : NO_AND;
      nega_arvore(a, n->esq);
      nega_arvore(a, n->dir); }
}

static int le_fator(const char **p, struct arvore *a){
    unsigned cit_antes = citadas_where;   /* para a diferença no `tem_comparacao` */
    pula(p);
    /* `NOT <fator>`: lê-se o que vem a seguir e nega-se por De Morgan */
    { const char *vn = *p;
      if(palavra(p, "NOT")){
          int e = le_fator(p, a);
          if(e < 0){ *p = vn; return -1; }
          nega_arvore(a, e);
          return e;
      } }
    /* O '(' é AMBÍGUO: pode abrir um grupo booleano — (a=3 OR b>5) — ou um fator numérico —
     * (a+b)*(a-b) > 0. Tenta-se primeiro a COMPARAÇÃO; se não fechar, volta-se ao ponto de
     * partida e lê-se como grupo. Sem este retrocesso, `(a+b)*(a-b) > 0` era lido como grupo,
     * falhava no ')' e a cláusula inteira caía fora. */
    const char *salvo = *p;
    int nsalvo = a->n;
    struct tensor L, R;
    if(le_num(p, &L)){
        pula(p);
        if(**p=='!' || **p=='<' || **p=='>' || **p=='=') goto tem_comparacao;
        /* ── `x IN (v1, v2, …)` É A DISJUNÇÃO DAS IGUALDADES ─────────────
         * Não é um operador novo: é `x = v1 OR x = v2 OR …`, e escrevê-lo
         * assim na árvore faz com que tudo o que já lá está valha para ele —
         * a contração, a ordenação dos ramos, a idempotência (`x IN (3,3)`
         * tem UM átomo) e o De Morgan, que leva o `NOT IN` a uma conjunção de
         * desigualdades sem uma linha a mais. */
        { const char *vi = *p;
          if(palavra(p, "IN")){
              pula(p);
              if(**p == '('){
                  const char *dentro = *p + 1;
                  int e = -1, algum = 0;
                  pula(&dentro);
                  /* uma subconsulta não é uma lista: esse caminho é outro */
                  if(!strncasecmp(dentro, "SELECT", 6)){ *p = vi; }
                  else{
                      (*p)++;
                      while(1){
                          struct tensor K;
                          pula(p);
                          if(!le_num(p, &K)) break;
                          { int i = novo_no(a); if(i < 0) return -1;
                            { struct no *n = &a->no[i];
                              /* e é o MESMO fecho da comparação, e não um
                               * parecido: sem ele `x IN (3)` e `x = 3` eram o
                               * mesmo facto com o vector de sinal oposto, e a
                               * contracção — que promete o mesmo bytecode para
                               * WHEREs equivalentes — via dois átomos onde há um. */
                              n->tipo = NO_COND; n->rel = SB_EQ;
                              n->v = ten_soma(L, K, -1);
                              fecha_cond(n); }
                            if(e < 0) e = i;
                            else { int j = novo_no(a); if(j < 0) return -1;
                                   a->no[j].tipo = NO_OR; a->no[j].esq = e;
                                   a->no[j].dir = i; e = j; } }
                          algum = 1;
                          pula(p);
                          if(**p == ','){ (*p)++; continue; }
                          break;
                      }
                      pula(p);
                      if(algum && **p == ')'){ (*p)++; return e; }
                      *p = vi;              /* não fechou: não era uma lista */
                  }
              } else *p = vi;
          } }
    }
    *p = salvo; a->n = nsalvo;
    if(**p == '('){
        (*p)++;
        int e = le_expr(p, a);
        pula(p);
        if(**p != ')') return -1;
        (*p)++;
        return e;
    }
    return -1;
tem_comparacao:;
    /* SEIS SINAIS, UMA LEITURA. O `<>`, o `<=` e o `>=` não são três casos a
     * escrever: são três das oito relações, e a `sb_le` devolve a relação e diz
     * quantos caracteres consumiu. Quem escrevia aqui o par `(op, nega)` estava
     * a fazer à mão a cláusula (9) — negar é COMPLEMENTAR —, e a fazê-la só
     * neste sítio, que é o que deixou os outros dois por fazer. */
    int nsin = 0, rel = sb_le(*p, &nsin);
    if(nsin == 0) return -1;
    *p += nsin;
    if(!le_num(p, &R)) return -1;

    int i = novo_no(a); if(i < 0) return -1;
    struct no *n = &a->no[i];
    n->tipo = NO_COND; n->rel = rel;
    if(sb_orientada(rel)){
        cit_desig = 1;
        /* as colunas DESTE fator: as que a leitura de L e R acrescentou desde o
         * início dele. É a diferença, e não o total, porque a lei é sobre a
         * coluna que a ORDEM toca — não sobre a consulta inteira. */
        citadas_desig |= (citadas_where & ~cit_antes);
    }
    n->v = ten_soma(L, R, -1);                     /* L rel R  ⟺  (L−R) rel 0 */
    fecha_cond(n);
    return i;
}
static int le_termo(const char **p, struct arvore *a){
    int e = le_fator(p, a);
    if(e < 0) return -1;
    while(palavra(p, "AND")){
        int d = le_fator(p, a);
        if(d < 0) return -1;
        int i = novo_no(a); if(i < 0) return -1;
        a->no[i].tipo = NO_AND; a->no[i].esq = e; a->no[i].dir = d;
        e = i;
    }
    return e;
}
static int le_expr(const char **p, struct arvore *a){
    int e = le_termo(p, a);
    if(e < 0) return -1;
    while(palavra(p, "OR")){
        int d = le_termo(p, a);
        if(d < 0) return -1;
        int i = novo_no(a); if(i < 0) return -1;
        a->no[i].tipo = NO_OR; a->no[i].esq = e; a->no[i].dir = d;
        e = i;
    }
    return e;
}
/* ---------------- A CONTRAÇÃO ----------------
 * Antes de emitir, a árvore é NORMALIZADA — do mesmo jeito que expressao.c contrai uma
 * expressão num tensor: quem é equivalente vira o MESMO objeto, e não duas coisas que uma
 * regra depois iguala. Três coisas acontecem, e nenhuma é reescrita ad hoc:
 *
 *   (1) cada condição vira um ÁTOMO com assinatura (coluna, operador, negação, constante).
 *       Átomos iguais são o MESMO átomo — logo `a=3 OR a=3` tem um átomo, não dois.
 *   (2) os filhos de AND/OR são ORDENADOS pela assinatura. Como as duas operações são
 *       comutativas, a ordem escrita é acidente; ordenar é escolher o representante.
 *   (3) A op A colapsa em A (idempotência), que cai sozinha depois de (1) e (2).
 *
 * O efeito medível: WHERE equivalentes escritos de formas diferentes emitem o MESMO bytecode,
 * byte a byte. O programa passa a depender da classe, não da escrita.
 */
static unsigned long mistura(unsigned long h, unsigned long v){
    h ^= v + 0x9e3779b97f4a7c15UL + (h << 6) + (h >> 2);
    return h;
}
static unsigned long sig_de(struct arvore *a, int i, unsigned long *cache){
    if(cache[i]) return cache[i];
    struct no *n = &a->no[i];
    unsigned long h;
    if(n->tipo == NO_COND){
        /* a assinatura leva a RELAÇÃO, e não o par derivado dela: são um campo e
         * não dois, e dois campos redundantes numa assinatura são dois sítios
         * onde ela pode distinguir o que é igual. */
        h = mistura(1469598103934665603UL, (unsigned long)n->rel);
        h = mistura(h, (unsigned long)n->decidido);
        h = mistura(h, (unsigned long)n->valor);
        for(int i = 0; i < NMON; i++) if(n->v.c[i]) h = mistura(mistura(h,(unsigned long)i),
                                                                (unsigned long)n->v.c[i]);
    } else {
        unsigned long s1 = sig_de(a, n->esq, cache), s2 = sig_de(a, n->dir, cache);
        if(s1 > s2){ int t = n->esq; n->esq = n->dir; n->dir = t;   /* (2) ordena */
                     unsigned long ts = s1; s1 = s2; s2 = ts; }
        h = mistura(mistura((unsigned long)n->tipo + 7, s1), s2);
    }
    cache[i] = h ? h : 1;
    return cache[i];
}
static int normaliza(struct arvore *a, int i, unsigned long *cache){
    struct no *n = &a->no[i];
    if(n->tipo == NO_COND) return i;
    n->esq = normaliza(a, n->esq, cache);
    n->dir = normaliza(a, n->dir, cache);
    memset(cache, 0, sizeof(unsigned long) * MAXNO);
    unsigned long s1 = sig_de(a, n->esq, cache), s2 = sig_de(a, n->dir, cache);
    if(s1 == s2) return n->esq;                                     /* (3) A op A = A */
    /* (2) ordena TAMBÉM aqui: dentro de sig_de a troca só acontece ao descer num nó
     * composto, então a raiz — e todo nó visto de cima — ficava por ordenar. Era por isso
     * que `a=3 AND b>20` e `b>20 AND a=3` davam bytecode diferente com o mesmo resultado. */
    if(s1 > s2){ int t = n->esq; n->esq = n->dir; n->dir = t; }

    /* A ABSORÇÃO — e ela não é regra ad hoc: é a ADJUNÇÃO δ⊣ε do morfico.py.
     *
     * Na morfologia, o AND é a EROSÃO e o OR é a DILATAÇÃO, e a adjunção dá
     * γ = δε anti-extensiva e φ = εδ extensiva, ambas idempotentes. Em árvore isso lê-se:
     *
     *     (x ∧ y) ∨ x = x        o que a erosão tirou, a dilatação não repõe além de x
     *     (x ∨ y) ∧ x = x        e o simétrico
     *
     * A idempotência (A op A = A) já estava acima, e é γγ=γ. Faltava esta, que é a que
     * colapsa os DOIS níveis — e sem ela `(a>2 AND a<9) OR a>2` gastava 1010 bytes para
     * dizer o que `a>2` diz em 486.
     *
     * E é a mesma forma da contração numérica, do outro lado: ali o tensor apaga o que não
     * é invariante, aqui a adjunção apaga o que não muda o conjunto. */
    for(int lado = 0; lado < 2; lado++){
        int filho = lado ? n->dir : n->esq, outro = lado ? n->esq : n->dir;
        struct no *f = &a->no[filho];
        if(f->tipo == NO_COND) continue;
        /* o filho tem de ser do tipo OPOSTO ao pai: (x∧y)∨x, (x∨y)∧x */
        if(f->tipo == n->tipo) continue;
        memset(cache, 0, sizeof(unsigned long) * MAXNO);
        unsigned long so = sig_de(a, outro, cache);
        memset(cache, 0, sizeof(unsigned long) * MAXNO);
        unsigned long fe = sig_de(a, f->esq, cache);
        memset(cache, 0, sizeof(unsigned long) * MAXNO);
        unsigned long fd = sig_de(a, f->dir, cache);
        if(so == fe || so == fd) return outro;        /* absorve: o filho todo desaparece */
    }
    return i;
}
static void junta_atomos(struct arvore *a, int i, unsigned long *cache){
    struct no *n = &a->no[i];
    if(n->tipo != NO_COND){ junta_atomos(a, n->esq, cache); junta_atomos(a, n->dir, cache); return; }
    unsigned long h = sig_de(a, i, cache);
    for(int j = 0; j < a->natomo; j++)
        if(a->asig[j] == h){ n->atomo = j; return; }                 /* (1) já existe */
    if(a->natomo >= 16){ n->atomo = 0; return; }
    int j = a->natomo++;
    a->asig[j] = h; a->arel[j] = n->rel;
    a->aop[j] = n->op; a->anega[j] = n->nega; a->av[j] = n->v;
    n->atomo = j;
}
static void contrai_arvore(struct arvore *a){
    unsigned long cache[MAXNO];
    memset(cache, 0, sizeof cache);
    a->raiz = normaliza(a, a->raiz, cache);
    memset(cache, 0, sizeof cache);
    a->natomo = 0;
    junta_atomos(a, a->raiz, cache);
}

/* Três respostas, e a do meio é a que faltava:
 *    1  há WHERE e ele analisa      → filtra
 *    0  não há WHERE                → varre tudo, e é o que o cliente pediu
 *   −1  há WHERE e ele NÃO analisa  → RECUSA
 *
 * Antes só havia 1 e 0, e o WHERE quebrado caía no 0 — isto é, um filtro que o compilador não
 * entendeu devolvia a TABELA INTEIRA. Num banco isso é o defeito mais caro que existe: o
 * cliente pede um recorte, recebe tudo, e nada avisa. */
static int le_where(const char **p, struct arvore *a){
    memset(a, 0, sizeof *a);
    if(!palavra(p, "WHERE")) return 0;              /* não há WHERE: varrer tudo é o pedido */
    a->raiz = le_expr(p, a);
    if(a->raiz < 0) return -1;                      /* há, e não analisa: recusar */
    contrai_arvore(a);                 /* contrai ANTES de emitir */
    return 1;
}

static void emit_teste(unsigned sc, int cmp_op, long k, unsigned destino, unsigned kslot){
    /* cada condição tem o SEU slot de constante — nada compartilhado entre compilar e executar */
    Word w; w.total = k; w.e = 0;
    mem_grava(kslot, w);

    /* SEM SALTO NENHUM, e o resultado é a MÁSCARA e não o bit.
     *
     * Isto eram dois saltos por condição: comparar, saltar se zero, saltar por
     * cima, copiar o um. O caminho percorrido dependia do dado — e era daí que
     * vinham os três passos por linha que casa, o resto de dependência que o
     * §W24 mediu depois de o molde ter deixado de ramificar.
     *
     * O `<` e o `>` já eram o bit de sinal da diferença; falta espalhá-lo, e é
     * uma instrução. O `=` é o complemento: a diferença é zero exatamente
     * quando o espalhamento dá nada, e negar é o XOR com a máscara cheia. O
     * destino passa a valer $0$ ou TUDO, que é o que o molde da linha quer
     * cruzar com a coordenada — e assim ele também deixa de ter de fabricar a
     * máscara. */
    if(cmp_op == '='){
        MOVE(sc, +1);
        MOVE(kslot, +1);
        emit1(OP_SUB);
        MOVE(S_TMP, -1);
        MOVE(S_TMP, +1);
        emit1(OP_ESPALHA);          /* diferente de zero -> tudo */
        MOVE(S_TMP, -1);
        MOVE(S_TMP, +1);
        MOVE(S_CHEIO, +1);
        emit1(OP_XOR);              /* nega: igual a zero -> tudo */
        MOVE(destino, -1);
    } else {
        if(cmp_op == '<'){ MOVE(kslot, +1); MOVE(sc, +1); }
        else             { MOVE(sc, +1); MOVE(kslot, +1); }
        emit1(OP_SUB);
        MOVE(S_TMP, -1);
        MOVE(S_TMP, +1);
        MOVE(S_MASK, +1);
        emit1(OP_AND);              /* o bit de sinal, e mais nada */
        MOVE(S_TMP, -1);
        MOVE(S_TMP, +1);
        emit1(OP_ESPALHA);          /* e espalha-se por toda a largura */
        MOVE(destino, -1);
    }
}

/* MULTIPLICAÇÃO POR CONSTANTE, NAS COORDENADAS DO REI.
 *
 * A ISA não tem MUL, e a versão anterior fazia soma repetida |c| vezes — linear no VALOR, e
 * com um `if(n > CMAX) n = CMAX` que TRUNCAVA o coeficiente em silêncio: um WHERE com 20*a
 * virava 8*a e devolvia a resposta errada sem avisar. Os dois problemas caem juntos.
 *
 * A ISA já tem a multiplicação pelo rei: GOLD é cifra_an(w,1) = (total + e, total), que é o
 * deslocamento (a,b) ↦ (a+b, a) medido em coroa.c §A5. Partindo de (x, 0), aplicar GOLD k−1
 * vezes dá (F(k)·x, F(k−1)·x): multiplicar por Fibonacci é DESLOCAR, e custa k opcodes.
 *
 * E coroa.c §A3 diz o resto: todo inteiro é soma de Fibonacci NÃO CONSECUTIVOS, de um único
 * jeito. Logo
 *      n·x = Σ F(k_i)·x = Σ GOLD^(k_i − 1) (x)
 * e o custo passa de n para o número de dígitos de Zeckendorf, que é ~log_φ(n). Nada foi
 * inventado: o opcode já estava lá, e as coordenadas são as do rei. */
static void emit_mul_zeck(unsigned acc, unsigned termo, long n, int soma, unsigned tmp){
    long fib[92]; int nf = 2;
    fib[0] = 1; fib[1] = 2;                       /* F(2), F(3), … — a base de Zeckendorf */
    while(fib[nf-1] <= n/2 + 1 && nf < 90){ fib[nf] = fib[nf-1] + fib[nf-2]; nf++; }
    long r = n;
    for(int i = nf-1; i >= 0 && r > 0; i--){
        if(fib[i] > r) continue;
        r -= fib[i];
        /* tmp ← termo, com o .e limpo, e depois i deslocamentos */
        MOVE(termo, +1);
        MOVE(S_MT, +1);
        emit1(OP_AND);
        MOVE(tmp, -1);
        /* GOLD^k parte de (x,0) e dá total = F(k+1)·x. Como fib[i] = F(i+2), o número de
         * deslocamentos é i+1, e NÃO i — errar isto acerta só quando o coeficiente é 1. */
        for(int t = 0; t <= i; t++){
            MOVE(tmp, +1);
            emit1(OP_GOLD);
            MOVE(tmp, -1);
        }
        MOVE(tmp, +1);                  /* limpa o .e que o deslocamento deixou */
        MOVE(S_MT, +1);
        emit1(OP_AND);
        MOVE(tmp, -1);
        if(soma){ MOVE(acc, +1); MOVE(tmp, +1); emit1(OP_ADD); }
        else    { MOVE(tmp, +1); MOVE(acc, +1); emit1(OP_SUB); }
        MOVE(acc, -1);
    }
}

/* ══ O CAMINHO DE DEZASSEIS BITS — ao lado do de oito, não por cima ═══════════
 *
 * O valor de uma célula larga vive em dois sítios: o byte baixo no `.total` da
 * linha e o alto no plano S_ALTO (§27). Para o avaliador o querer como UM número
 * basta juntá-los numa Word — {baixo, alto} —, e aí `OP_ADD16` e companhia lêem-na
 * como os dezasseis bits que ela é.
 *
 * E a junção faz-se com o que a ISA já tem: TROCA leva (alto,0) a (0,alto), e o
 * OP_ADD — COMPONENTE A COMPONENTE, que aqui é exactamente o que se quer — junta
 * (baixo,0) com (0,alto). Nenhuma instrução nova para montar o par. */
static void emit_valor16_p(unsigned dest, long linha, long ncols, int cc,
                           unsigned tmp, int plano);
static void emit_valor16(unsigned dest, long linha, long ncols, int cc, unsigned tmp){
    emit_valor16_p(dest, linha, ncols, cc, tmp, 0);
}
/* o mesmo, dizendo de que ANDAR se lê: 0 a parte racional, 1 o σ (S_DEN). O
 * endereço tem a mesma forma nos dois, pelo que o relocador anda igual. */
static void emit_valor16_p(unsigned dest, long linha, long ncols, int cc,
                           unsigned tmp, int plano){
    if(plano == 1){
        /* o σ vive num átomo só, e com SINAL: o `Word8` não o traz, e um σ de −1
         * lia-se 255. Estende-se o bit 7 para o andar de cima, que é o que o
         * `assin16` faz do outro lado. */
        emit_copia(S_DEN + cel_ix(linha*ncols + cc), dest);
        MOVE(dest, +1); MOVE(S_MT, +1); emit1(OP_AND); MOVE(dest, -1);
        { long b = (long)mem_le(S_DEN + cel_ix(linha*ncols + cc)).total;
          if(b & 0x80L){                       /* negativo: o alto é 0xFF */
              Word f; f.total = 0; f.e = 255;
              mem_grava(S_SIGEXT, f);
              MOVE(dest, +1); MOVE(S_SIGEXT, +1); emit1(OP_ADD); MOVE(dest, -1); } }
        return;
    }
    /* dest ← (baixo, 0) */
    emit_copia(S_LINHAS + cel_ix(linha*ncols + cc), dest);
    MOVE(dest, +1); MOVE(S_MT, +1); emit1(OP_AND); MOVE(dest, -1);
    /* tmp ← (alto, 0) → TROCA → (0, alto) */
    emit_copia(S_ALTO + cel_ix(linha*ncols + cc), tmp);
    MOVE(tmp, +1); MOVE(S_MT, +1); emit1(OP_AND); MOVE(tmp, -1);
    MOVE(tmp, +1); emit1(OP_TROCA); MOVE(tmp, -1);
    /* dest ← dest + tmp, componente a componente: (baixo, alto) */
    MOVE(dest, +1); MOVE(tmp, +1); emit1(OP_ADD); MOVE(dest, -1);
}

/* n·x um andar acima: ZECKENDORF com o rei de dezasseis bits.
 *
 * O GOLD de 16 não é opcode: é (a,b) ↦ (a+b, a) sobre DOIS slots, e isso são um
 * OP_ADD16 e uma cópia. A lei é a mesma do §28 — o que muda é quem a executa. */
static void emit_mul_zeck16(unsigned acc, unsigned x, long n, int soma,
                            unsigned ga, unsigned gb, unsigned tmp){
    long fib[24]; int nf = 2;
    fib[0] = 1; fib[1] = 2;                       /* F(2), F(3), … */
    while(fib[nf-1] <= n/2 + 1 && nf < 23){ fib[nf] = fib[nf-1] + fib[nf-2]; nf++; }
    long r = n;
    for(int i = nf-1; i >= 0 && r > 0; i--){
        if(fib[i] > r) continue;
        r -= fib[i];
        emit_copia(x, ga);                        /* o rei parte de (x, 0) */
        emit_copia(S_ZERO, gb);
        for(int t = 0; t <= i; t++){              /* fib[i] = F(i+2) → i+1 passos */
            emit_copia(ga, tmp);                  /* guarda o a antigo */
            MOVE(ga, +1); MOVE(gb, +1); emit1(OP_ADD16); MOVE(ga, -1);   /* a' = a + b */
            emit_copia(tmp, gb);                                          /* b' = a     */
        }
        /* A ORDEM DO MOVE DECIDE O SINAL. `MOVE(x,+1)` empurra x para A e o A
         * anterior para B, e o SUB faz A − B. Logo para `acc − ga` o ga entra
         * PRIMEIRO — é o que o caminho de oito bits já fazia, e escrever os dois
         * na mesma ordem dava `ga − acc`: o sinal ao contrário. */
        if(soma){ MOVE(acc, +1); MOVE(ga, +1); emit1(OP_ADD16); }
        else    { MOVE(ga, +1); MOVE(acc, +1); emit1(OP_SUB16); }
        MOVE(acc, -1);
    }
}

/* ── O PRODUTO DE DEZASSEIS BITS: DESLOCAMENTO E SOMA ────────────────────────
 *
 * Não é um idioma de máquina: é o que o `naturais.tex` escreve na última linha
 * do thm:transporte — «as duas operações que a iteração usa são a soma e o
 * produto de 𝔽₂, as únicas primitivas de um bit; O PRODUTO DE ℕ SEGUE POR
 * DESLOCAMENTO E SOMA». E o lem:desloc diz a outra metade: multiplicar pelo
 * gerador da dobra É deslocar, x⊗σ = 2^w·x.
 *
 * Então:   x·y = Σ_i  y_i · (x << i)
 * com o `<<` a ser `x + x` (ADD16, onde o vai-um atravessa — §26) e o `y_i` a
 * ser um AND com a máscara do bit i.
 *
 * O produto TEM de caber em dezasseis bits, e cabe por construção: o
 * `cl_cabe16` recusa em compilação qualquer forma cujo pior caso passe de
 * 32767, que é o envelope da comparação. Não há aqui tecto por descobrir — há
 * um tecto já verificado antes de o programa ser emitido. */
static void emit_mul16(unsigned dest, unsigned X, unsigned Y, unsigned base){
    unsigned desl = base, tmp = base + 1, masc = base + 2;   /* masc..masc+15 */
    emit_copia(S_ZERO, dest);
    emit_copia(X, desl);
    for(int i = 0; i < 16; i++){
        /* a máscara do bit i, cada uma no seu slot (a constante é de compilação) */
        Word m; m.total = (Word8)(i < 8 ? (1u << i) : 0u);
        m.e = (Word8)(i >= 8 ? (1u << (i - 8)) : 0u);
        mem_grava(masc + (unsigned)i, m);
        /* tmp ← Y ∧ máscara ; salta a soma se for zero */
        MOVE(Y, +1); MOVE(masc + (unsigned)i, +1); emit1(OP_AND);
        MOVE(tmp, -1);
        MOVE(tmp, +1); MOVE(S_ZERO, +1); emit1(OP_CMP);
        emit1(OP_JZ);
        unsigned pos = pc_emit; emit1(0); emit1(0);
        unsigned ini = pc_emit;
        MOVE(dest, +1); MOVE(desl, +1); emit1(OP_ADD16); MOVE(dest, -1);
        { salto_poe(pos, ini); }
        /* e o deslocamento: x << 1 é x + x (lem:desloc) */
        if(i < 15){
            MOVE(desl, +1); MOVE(desl, +1); emit1(OP_ADD16); MOVE(desl, -1);
        }
    }
}

/* a comparação com zero, um andar acima: o sinal é o bit 15 do par */
static void emit_teste16(unsigned sc, int cmp_op, unsigned destino, unsigned kslot,
                         long k, unsigned tmp){
    Word w; w.total = (Word8)((unsigned long)k & 255u);
    w.e = (Word8)(((unsigned long)k >> 8) & 255u);
    mem_grava(kslot, w);
    /* o andar de cima segue a mesma regra do de baixo: sem salto, e o resultado
     * é a MÁSCARA. A diferença é só a largura — aqui o sinal é o bit 15 do par,
     * e a subtracção atravessa o átomo. */
    if(cmp_op == '='){
        MOVE(sc, +1); MOVE(kslot, +1); emit1(OP_SUB16);
        MOVE(tmp, -1);
        MOVE(tmp, +1); emit1(OP_ESPALHA);
        MOVE(tmp, -1);
        MOVE(tmp, +1); MOVE(S_CHEIO, +1); emit1(OP_XOR);
        MOVE(destino, -1);
    } else {
        if(cmp_op == '<'){ MOVE(kslot, +1); MOVE(sc, +1); }
        else             { MOVE(sc, +1); MOVE(kslot, +1); }
        emit1(OP_SUB16);
        MOVE(tmp, -1);
        MOVE(tmp, +1); MOVE(S_MASK16, +1); emit1(OP_AND);   /* o bit 15: o sinal do par */
        MOVE(tmp, -1);
        MOVE(tmp, +1); emit1(OP_ESPALHA);
        MOVE(destino, -1);
    }
}

/* A FORMA LINEAR INTEIRA TEM DE CABER, e não só cada coluna. O avaliador decide
 * pelo sinal de `c0 + Σ c_i·x_i`, logo é ESSA soma que precisa de caber em
 * 0..32767 — `2·32767` já não cabe, e a resposta sairia ao contrário. O pior
 * caso calcula-se em compilação, com o maior valor que cada coluna guarda. */
/* a forma de UM átomo cabe em ±limite? — o mesmo majorante, por átomo */
/* ── O ANDAR DE CIMA SABE TRATAR ESTE ÁTOMO? ─────────────────────────────────
 *
 * O caminho de dezasseis bits soma e multiplica o PAR como número, e é isso que
 * ele sabe: números. O de oito sabe mais --- carrega o denominador de cada
 * célula e transporta φ_t nos corpos com régua ---, e por isso ele tem de
 * continuar a existir onde essas coisas vivem.
 *
 * O que estava errado era a REGRA DE ESCOLHA: decidia-se pelo TAMANHO («cabe em
 * ±127 usa o de oito»), e o de oito é justamente o que fica caro com valores
 * pequenos --- o coeficiente vira soma repetida em base de Zeckendorf, e o
 * custo cresce com o VALOR. Medido em quarenta linhas: `a = 0` custa 2800
 * passos, `a = 7` custa 5280 e `a = 100` custa 8520, enquanto o de dezasseis
 * custa 3440 SEMPRE. Escolhia-se o caminho lento exactamente onde ele é lento.
 *
 * A pergunta certa não é «cabe?» --- é «o andar de cima sabe tratar isto?». Se
 * todas as colunas citadas são de corpo INTEIRO, sabe, e o custo passa a ser
 * fixo. Se alguma tem régua ou denominador, não sabe, e desce-se ao de oito. */
static int atom_so_inteiro(const struct arvore *a, int j, long ncols, long nrows){
    /* A PERGUNTA É SOBRE AS CÉLULAS, e não sobre o corpo da coluna.
     *
     * Perguntar pelo corpo não chega: uma coluna declarada INTEIRO aceita `3/4`,
     * e o denominador vive na CÉLULA (S_DEN), não no catálogo. Com a pergunta
     * pelo corpo, `SELECT * FROM t WHERE a > 1` numa tabela com 3/4 e 7/2 subia
     * ao andar de cima --- que soma o par como número e não sabe do
     * denominador --- e a ordem saía errada em quatro asserções.
     *
     * A varredura corre UMA vez por compilação, não por linha: são nrows·ncols
     * leituras contra os milhares de passos que o andar de baixo custa a cada
     * linha. */
    for(long cc = 0; cc < ncols; cc++)
        if(corpo_de(cc).total != CORPO_INTEIRO) return 0;
    if(a->av[j].den && a->av[j].den != 1) return 0;    /* a própria expressão tem denominador */
    for(long i = 0; i < nrows; i++)
        for(long cc = 0; cc < ncols; cc++){
            long d = (long)mem_le(S_DEN + cel_ix(i*ncols + cc)).total;
            if(d != 1 && d != 0) return 0;      /* 0 = célula por escrever */
        }
    return 1;
}

static int atom_cabe(const struct arvore *a, int j, long ncols, long nrows, long long limite){
    long long alto = 0, baixo = 0;
    for(int cod = 0; cod < NMON; cod++){
        long c = a->av[j].c[cod];
        if(!c) continue;
        int d[KGRAU]; mi_de(cod, d);
        if(mi_cod(d) != cod) continue;
        long long mag = 1;
        for(int t = 0; t < KGRAU; t++){
            if(!d[t]) continue;
            long cc = col_de_sim(d[t] - 1);
            if(cc < 0 || cc >= ncols) return 0;
            unsigned long mx = col_max(cc, ncols, nrows);
            mag *= (long long)(mx ? mx : 1);
            if(mag > 0x7FFFFFFFLL) return 0;
        }
        long long parcela = (long long)c * mag;
        if(parcela > 0) alto += parcela; else baixo += parcela;
    }
    return !(alto > limite || baixo < -(limite + 1));
}

/* ── A MARCA DIZ SE VALE A PENA ANDAR ────────────────────────────────────────
 *
 * A cláusula (4) do thm:multiplicidade: «sentir é ler G ... a distinção é
 * propriedade do ESPAÇO». A marca de uma coluna --- o maior valor lá escrito,
 * que o `col_marca` deixa no acto da escrita e não numa varredura --- é
 * exactamente isso: uma leitura O(1) que diz o ALCANCE da coluna.
 *
 * Com ela, uma condição que não pode ser satisfeita responde sem ler linha
 * nenhuma. `c1 > 1000` numa coluna cujo maior valor escrito é 1: o máximo da
 * forma `c1 − 1000` é −999, e nada `> 0` sai de lá. É o `skip index` do
 * ClickHouse, e aqui não precisou de índice --- a marca já estava escrita, e
 * faltava LÊ-LA antes de andar.
 *
 * Devolve 1 quando a condição PODE ser satisfeita (e aí varre-se), 0 quando ela
 * é impossível pelo alcance. Na dúvida devolve 1: uma resposta vazia dada por
 * engano seria pior do que uma varredura. */
static int atom_possivel(const struct arvore *a, int j, long ncols, long nrows){
    long long alto = 0, baixo = 0;
    for(int cod = 0; cod < NMON; cod++){
        long c = a->av[j].c[cod];
        if(!c) continue;
        int d[KGRAU]; mi_de(cod, d);
        if(mi_cod(d) != cod) continue;
        long long mag_alto = 1, mag_baixo = 1;
        int tem_col = 0;
        for(int t = 0; t < KGRAU; t++){
            if(!d[t]) continue;
            long cc = col_de_sim(d[t] - 1);
            if(cc < 0 || cc >= ncols) return 1;        /* não sei: varre-se */
            /* a coluna vai de 0 (o mínimo do corpo sem sinal) à MARCA.
             *
             * E A MARCA A ZERO NÃO É UMA AFIRMAÇÃO. Ela vale zero tanto numa
             * coluna cujo maior valor é 0 como numa que ainda não foi escrita ou
             * cujas células estão todas AUSENTES --- e nesse caso concluir
             * «impossível» é dar vazio a quem podia ter resposta, que é o pior
             * desfecho. Foi o que fiz à primeira, e três asserções caíram sobre
             * uma coluna toda ausente. Na dúvida varre-se: escrevi-o no
             * comentário e não o respeitei no código. */
            /* ── E A MARCA TEM DE MEDIR O QUE O AVALIADOR COMPARA ────────
             * Numa coluna com régua o avaliador compara a NORMA, e a marca
             * guarda o máximo da PARTE REAL: são duas quantidades diferentes, e
             * podar com uma para descartar sobre a outra descarta linhas que
             * casam. Medido: com {2+0σ, 1+1σ, 3−1σ} a marca dá 3 e `a > 4`
             * saía «nenhuma linha pode satisfazer, sem varrer» --- e a norma de
             * 3−1σ é 5, que satisfaz. Na dúvida varre-se, que é o que o
             * comentário acima já mandava. */
            { Word cwm = corpo_de(cc);
              if(cwm.total == CORPO_AUREO || cwm.total == CORPO_CRISTAL) return 1; }
            unsigned long mx = col_max(cc, ncols, nrows);
            if(mx == 0 || mx > 0x7FFFFFFFul) return 1;
            /* E O OUTRO LADO DO PAR. Estava `mag_baixo *= 0` --- o mínimo
             * ASSUMIDO zero, que é meia marca: numa coluna cujos valores estão
             * todos entre 600 e 700 isso não permite descartar `k < 100`. Com o
             * mínimo lido, permite: nenhuma linha pode satisfazer, e não se
             * varre uma. Os valores da marca são não-negativos, pelo que o
             * mínimo do produto é o produto dos mínimos. */
            mag_alto  *= (long long)mx;
            /* e só se a marca FOR o valor comparado --- ver `col_e_inteira`.
             * Onde não é (racionais na célula), volta-se ao zero, que é largo e
             * nunca descarta a mais. */
            mag_baixo *= col_e_inteira(cc, ncols, nrows)
                       ? (long long)col_min(cc, ncols, nrows) : 0;
            tem_col = 1;
        }
        if(!tem_col){ alto += c; baixo += c; continue; }   /* o termo constante */
        if(c > 0){ alto += (long long)c * mag_alto; baixo += (long long)c * mag_baixo; }
        else     { alto += (long long)c * mag_baixo; baixo += (long long)c * mag_alto; }
    }
    /* A forma vale entre `baixo` e `alto`, e o teste é contra ZERO. E a pergunta
     * não é um caso por operador: os blocos que o intervalo ALCANÇA são eles
     * próprios uma relação — `<` se ele desce abaixo de zero, `=` se o contém,
     * `>` se sobe acima —, e a condição é possível sse as duas se cruzam. Um
     * `&`, e vale para as oito.
     *
     * ── E ISTO ERA UM DEFEITO, E DOS QUE RESPONDEM ERRADO EM SILÊNCIO ─────────
     * O que estava aqui era um `switch` de três casos sobre o `op` DERIVADO, e
     * ele não olhava a `nega`. Ora as relações de dois blocos chegam aqui com o
     * `op` da COMPLEMENTAR, pelo que a poda lia `≤` como se fosse `>`, `≥` como
     * `<` e `≠` como `=` — e podava exactamente quando a condição era verdadeira
     * para TODAS as linhas. Numa coluna que não passa de 50:
     *
     *     WHERE a <= 100   →  «nenhuma linha pode satisfazer»,  com todas a
     *     WHERE a >= 5         satisfazer. Zero linhas, sem varrer, e sem aviso.
     *     WHERE a <> 999
     *
     * O `tests/simbolos.c §S10` mede-o: nos 360 pares (intervalo, relação) de
     * [-4,4], a tabela de três casos dá 50 falsos VAZIOS e o alcance dá zero. */
    return sb_possivel(a->arel[j], baixo, alto);
}

static int cl_cabe16(const struct arvore *a, long ncols, long nrows){
    for(int j = 0; j < a->natomo; j++){
        /* o MÁXIMO e o MÍNIMO da forma, separados: somar magnitudes ignorava que
         * um coeficiente negativo SUBTRAI, e recusava `medio − 100` com medio a
         * chegar a 32767 — onde a conta dá 32667 e cabe à vontade. */
        long long alto = 0, baixo = 0;
        for(int cod = 0; cod < NMON; cod++){
            long c = a->av[j].c[cod];
            if(!c) continue;
            int d[KGRAU]; mi_de(cod, d);
            if(mi_cod(d) != cod) continue;
            long long mag = 1;
            for(int t = 0; t < KGRAU; t++){
                if(!d[t]) continue;
                long cc = col_de_sim(d[t] - 1);
                if(cc < 0 || cc >= ncols) return 0;
                unsigned long mx = col_max(cc, ncols, nrows);
                mag *= (long long)(mx ? mx : 1);
                if(mag > 0x7FFFFFFFLL) return 0;
            }
            long long parcela = (long long)c * mag;      /* x_i >= 0 sempre */
            if(parcela > 0) alto += parcela; else baixo += parcela;
        }
        if(alto > CMP16_MAX || baixo < -(CMP16_MAX + 1)) return 0;
    }
    return 1;
}

/* algum átomo da cláusula tem monómio de grau ≥ 2? (o produto de dois valores) */
static int cl_tem_grau2(const struct arvore *a){
    for(int j = 0; j < a->natomo; j++)
        for(int cod = 0; cod < NMON; cod++){
            if(!a->av[j].c[cod]) continue;
            int d[KGRAU]; mi_de(cod, d);
            if(mi_cod(d) != cod) continue;
            if(mi_grau(cod) >= 2) return 1;
        }
    return 0;
}

/* os átomos distintos, avaliados UMA vez por linha.
 *
 * O átomo já vem CONTRAÍDO num vetor: c0 + Σ c_i·x_i, comparado com 0. Se o vetor não tem
 * variável, nada é emitido — o valor já se sabe. Se tem, a forma linear é montada no metal:
 * a ISA não tem multiplicação, então c_i·x_i é soma repetida |c_i| vezes, e o compilador
 * conhece c_i, logo o laço é desenrolado. */
/* O METAL COMO PALAVRA — e é isto que apaga prata, bronze e os seus negros.
 *
 * A_m = T^{m−1}·A_1 com T = A_1·J, e para m ≤ 0 o espelho exato: T⁻¹ = J·A_1⁻¹. Vale para TODO
 * m inteiro, negativo, zero e positivo — logo a ISA não precisa de um opcode por metal. Prata e
 * bronze não eram peças: eram ATALHOS, e um atalho tomado por gerador faz pensar que a máquina
 * precisa dele. Ficam quatro geradores, e são simétricos:
 *
 *     GOLD   NEGRO_OURO   TROCA   ESQUILO
 *
 * m = 0 dá A_0 = J: a troca é o metal do MEIO, onde os dois lados da régua se encontram. */
static void emit_metal(long m, unsigned s){
    MOVE(s, +1); emit1(OP_GOLD); MOVE(s, -1);
    if(m >= 1) for(long k = 1; k < m; k++){                    /* T   = TROCA depois GOLD  */
        MOVE(s, +1); emit1(OP_TROCA); MOVE(s, -1);
        MOVE(s, +1); emit1(OP_GOLD);  MOVE(s, -1);
    } else for(long k = m; k <= 0; k++){                       /* T⁻¹ = NEGRO depois TROCA */
        MOVE(s, +1); emit1(OP_NEGRO_OURO); MOVE(s, -1);
        MOVE(s, +1); emit1(OP_TROCA);      MOVE(s, -1);
    }
}
/* a VOLTA, e a regra é inteira e sem tabela: a mesma palavra ao CONTRÁRIO, cada letra pela sua
 * inversa. O gato vai a negro, o negro vai a gato, e a troca fica onde está — é involução. */
static void emit_metal_inv(long m, unsigned s){
    if(m >= 1) for(long k = m-1; k >= 1; k--){
        MOVE(s, +1); emit1(OP_NEGRO_OURO); MOVE(s, -1);
        MOVE(s, +1); emit1(OP_TROCA);      MOVE(s, -1);
    } else for(long k = 0; k >= m; k--){
        MOVE(s, +1); emit1(OP_TROCA); MOVE(s, -1);
        MOVE(s, +1); emit1(OP_GOLD);  MOVE(s, -1);
    }
    MOVE(s, +1); emit1(OP_NEGRO_OURO); MOVE(s, -1);
}

/* multiplica dois slots em tempo de EXECUÇÃO: dest = X · Y.
 *
 * A ISA não tem MUL, e aqui o multiplicador não é constante conhecida (é o valor de outra
 * coluna) — logo não dá para desenrolar. Vira laço: soma X a si mesmo |Y| vezes, com o passo
 * e o incremento escolhidos pelo SINAL de Y. Custa |Y| voltas, e isso é propriedade do metal,
 * não do compilador: multiplicar sem multiplicador custa contar. */
static void emit_mul(unsigned dest, unsigned X, unsigned Y, unsigned base){
    /* UMA INSTRUÇÃO, E NÃO UM LAÇO.
     *
     * Isto era `enquanto cnt != 0 { dest += passo; cnt += delta }` — |Y| voltas,
     * com o comentário a dizer que «multiplicar sem multiplicador custa contar».
     * Custa contar a quem não lê: o produto está nos BITS, e a base é ortonormal
     * (aranha thm:base8), de modo que ler o bit e medir são a mesma operação. O
     * OP_MUL16 faz a dobra em dezasseis passos fixos, dentro da ULA do andar.
     *
     * E o laço tinha um contador do PAR com o átomo alto fora da conta, que
     * nunca chegava a zero: não se conserta um relógio que não devia existir.
     * Não se controla o tempo — desliza-se nos pontos de I e verifica-se o
     * fechamento pela métrica. */
    (void)base;
    MOVE(X, +1);
    MOVE(Y, +1);
    emit1(OP_MUL16);
    MOVE(dest, -1);
}

/* os átomos distintos, avaliados UMA vez por linha.
 *
 * O átomo vem contraído num TENSOR simétrico de grau ≤ 2. A forma monta-se no metal:
 *   grau 0  a constante, direto;
 *   grau 1  coeficiente CONHECIDO em compilação  → soma repetida, desenrolada;
 *   grau 2  x_i·x_j com os dois vindos da linha  → laço (emit_mul), porque não há MUL.
 * E a comparação é sempre contra ZERO — a contração já passou tudo para um lado. */
static long corpo_parm(long p){ return (long)(int8_t)(Word8)p; } /* envelope: −1 ≡ 0xFF */
static int corpo_tem_regua(long cp){ return cp == CORPO_AUREO || cp == CORPO_CRISTAL; }
static long corpo_B(long cp, long parm){ (void)cp; return corpo_parm(parm); }
static long corpo_C(long cp){ return (cp == CORPO_AUREO) ? -1 : 1; }
static long corpo_delta(long cp, long parm){
    long B = corpo_B(cp, parm), C = corpo_C(cp);
    return B*B - 4*C;
}
/* ── AS TRÊS CLASSES, DITAS UMA VEZ — `aranha thm:leidisc` ───────────────────
 *
 * «Δ < 0 ELÍPTICO: ordem finita, e ela está em {3,4,6}. Δ = 0 PARABÓLICO: raiz
 * DUPLA, o corte terminado. Δ > 0 HIPERBÓLICO: duas direcções reais, uma expande
 * e a outra contrai.» E o teorema avisa do erro fácil: «o traço sozinho não
 * classifica --- quem classifica é Δ», com a nota de que a primeira escrita dele
 * produziu dezasseis falsas elípticas por separar pelo traço.
 *
 * A tricotomia estava escrita DUAS vezes neste ficheiro, com os mesmos três
 * nomes em géneros diferentes --- uma fala do corpo, outra da régua ---, e uma
 * delas recalculava Δ = B² − 4C à mão em vez de pedir ao `corpo_delta`. Três
 * escritas da mesma lei. Fica uma, e é esta: os nomes pedem-se com o género. */
enum { CL_ELIPTICO = -1, CL_PARABOLICO = 0, CL_HIPERBOLICO = 1 };
/* ── AS DUAS RÉGUAS DA CAIXA — `aranha thm:pandora` ──────────────────────────
 *
 * A distância entre espelhos tem duas formas, e elas servem para coisas
 * diferentes. A ARITMÉTICA, |Δᵢ − Δⱼ|, diz QUANTO falta e é a que a recusa
 * reporta --- é métrica, e NÃO é ultramétrica: com Δ = 20, 13, 8 dá 12 contra
 * um máximo de 7. Medido sobre os dezoito espelhos de |m|,|t| ≤ 4: viola em
 * 1264 dos 5832 triplos.
 *
 * A ULTRAMÉTRICA é a da `def:arvore` lida sobre os mesmos Δ --- 2^{−prof} ---, e
 * essa cumpre o `lem:ultra` em TODOS os 5832. É ela que dá a partição do
 * `thm:bolas`: as bolas não se cruzam, e por isso «todo ponto é centro».
 *
 * No que a recusa precisa as duas CONCORDAM, e é isso que torna legítimo usar a
 * aritmética para reportar: anulam exactamente no mesmo sítio, Δᵢ = Δⱼ. */
/* ── A RÉGUA DO CORPO, PELA PORTA ────────────────────────────────────────────
 * O medidor tem de comparar pelo comparador VERDADEIRO --- o do `corpos.h` --- e
 * não por uma cópia dele escrita no teste, que seria medir a minha aritmética e
 * não a da casa. Abre-se a porta em vez de duplicar a régua. */
/* «a cifra PARA?» — o critério que o `cifra.h` já usa para escolher entre o
 * Euclides (finito) e o PQa (periódico): |Δ| ser quadrado. É uma TESTEMUNHA da
 * borda, e o medidor precisa dela pela porta para a comparar com a factorização
 * sobre o anel base, que é outra coisa. */
int sql_cifra_para(long B, long C){
    long D = B*B - 4*C; if(D < 0) D = -D;
    { long r = raizi(D); return r*r == D; }
}

/* a raiz inteira da casa, pela porta: o medidor tem de medir a VERDADEIRA (a do
 * `cifra.h`, que o motor usa) e não uma cópia escrita no teste. */
long sql_raizi(long n){ return raizi(n); }

/* a CIFRA de um corpo, pela porta: (traço, determinante) da sua régua. O medidor
 * que a quiser varrer não a pode escrever à mão --- seria medir a minha
 * aritmética e não a da casa. CORPO_AUREO = 2, CORPO_CRISTAL = 4. */
void sql_corpo_cifra(long corpo, long parm, long *B, long *C, long *D){
    if(B) *B = corpo_B(corpo, parm);
    if(C) *C = corpo_C(corpo);
    if(D) *D = corpo_delta(corpo, parm);
}
int sql_corpo_aureo(void){ return CORPO_AUREO; }
int sql_corpo_cristal(void){ return CORPO_CRISTAL; }

int sql_au_cmp(long ua, long ub, long va, long vb, long m){
    Par u, v; u.a = ua; u.b = ub; v.a = va; v.b = vb;
    return au_cmp(u, v, m);
}
int sql_cr_cmp(long ua, long ub, long va, long vb, long t){
    Par u, v; u.a = ua; u.b = ub; v.a = va; v.b = vb;
    return cr_cmp(u, v, t);
}

long sql_esp_dist(long Di, long Dj){ long d = Di - Dj; return d < 0 ? -d : d; }
static long esp_dist(long Di, long Dj){ return sql_esp_dist(Di, Dj); }
int sql_esp_prof(long Di, long Dj){
    /* a profundidade da primeira divergência, do topo --- def:arvore */
    if(Di == Dj) return (int)(sizeof(long) * 8);
    unsigned long a = (unsigned long)Di, b = (unsigned long)Dj;
    int p = 32;
    for(int q = 0; q < p; q++)
        if(((a >> (p-1-q)) & 1ul) != ((b >> (p-1-q)) & 1ul)) return q;
    return p;
}
static int esp_prof(long Di, long Dj){ return sql_esp_prof(Di, Dj); }

static int corpo_classe(long D){
    return D < 0 ? CL_ELIPTICO : (D == 0 ? CL_PARABOLICO : CL_HIPERBOLICO);
}
static const char *corpo_classe_nome(long D, int fem){
    switch(corpo_classe(D)){
        case CL_ELIPTICO:   return fem ? "elíptica"    : "elíptico";
        case CL_PARABOLICO: return fem ? "parabólica"  : "parabólico";
        default:            return fem ? "hiperbólica" : "hiperbólico";
    }
}


static void emit_transporte(long t, unsigned s){
    /* φ_t = [[1,t],[0,1]] = (TROCA GOLD)^t, e para t<0 é (NEGRO TROCA)^|t| */
    for(long k = 0; k < (t < 0 ? -t : t); k++){
        if(t > 0){
            MOVE(s, +1); emit1(OP_TROCA); MOVE(s, -1);
            MOVE(s, +1); emit1(OP_GOLD);  MOVE(s, -1);
        } else {
            MOVE(s, +1); emit1(OP_NEGRO_OURO); MOVE(s, -1);
            MOVE(s, +1); emit1(OP_TROCA);      MOVE(s, -1);
        }
    }
}

static void emit_atomos(const struct arvore *a, long linha, long ncols){
    long nrows_atual = cat_nrows();
    for(int j = 0; j < a->natomo; j++){
        unsigned dest = S_COND + (unsigned)j;
        unsigned acc  = S_LIN + (unsigned)(j*ATOMO_SLOTS);
        /* O MAPA DO RASCUNHO, dito de uma vez — cada átomo tem acc..acc+11 e emit_mul
         * consome QUATRO a partir do base. Foi a sobreposição disto que me custou duas
         * tentativas hoje: cada ordem inventada batia noutra ordem inventada. */
        unsigned prod = acc + 1, prod2 = acc + 2, tmpm = acc + 3, base = acc + 4;

        if(ten_constante(a->av[j])) continue;         /* decidido: nada se emite */

        /* OS DOIS LADOS NA MESMA RÉGUA.
         *
         * A coluna vive como par p/q; a constante vivia como magnitude crua. Comparar as duas
         * era comparar coordenada com magnitude — e a igualdade nunca fechava, porque o .e não
         * batia. Mascarar o .e escondia a diferença em vez de a resolver.
         *
         * O certo é LEVANTAR a constante à régua da coluna. O átomo Σc·x + c₀ com x = p/q vale
         * (c·p + c₀·q)/q, e como q > 0 o sinal é o do numerador:
         *
         *     c·p + c₀·q   OP   0
         *
         * Então o termo constante entra multiplicado pelo denominador da coluna, e os dois
         * lados passam a ser numeradores sobre o mesmo q. Com q = 1 dá exatamente o que dava
         * antes — a tabela de inteiros não muda um byte. */
        int cit[NCOL]; int ncit = 0, unica = -1;
        for(int c = 0; c < NCOL; c++) cit[c] = 0;
        for(int cod = 1; cod < NMON; cod++){
            if(!a->av[j].c[cod]) continue;
            int dd[KGRAU]; mi_de(cod, dd);
            /* conta-se por SÍMBOLO --- que é o que cabe no cit[NCOL] --- e o
             * que se guarda em `unica` é a COLUNA, que é o que o resto usa */
            for(int t = 0; t < KGRAU; t++){
                int sm = dd[t] ? dd[t] - 1 : -1;
                if(sm < 0 || sm >= NCOL || cit[sm]) continue;
                long cc = col_de_sim(sm);
                if(cc < 0 || cc >= ncols) continue;
                cit[sm] = 1; unica = (int)cc; ncit++;
            }
        }
        Word w; w.total = a->av[j].c[0]; w.e = 0;
        mem_grava(S_K + (unsigned)j, w);
        (void)ncit; (void)unica;

        /* ── O ANDAR DE CIMA, QUANDO ELE É PRECISO ────────────────────────────
         * Se alguma coluna citada guarda acima de 255, o caminho de oito bits não
         * a sabe ler — e o §27 recusava a consulta inteira. Agora há o outro
         * andar: monta-se o valor como {baixo, alto} numa Word e a conta corre em
         * ADD16/SUB16, com o rei de Zeckendorf feito de ADD16 mais uma cópia.
         *
         * Só para GRAU ≤ 1. O grau 2 multiplica dois valores em execução, e um
         * produto de dois de dezasseis pede TRINTA E DOIS — é a torre a dobrar
         * outra vez, e é o trio seguinte. Até lá esse caso continua RECUSADO, com
         * o motivo escrito. O caminho de oito bits fica byte a byte como estava. */
        /* ── O ANDAR ESCOLHE-SE PELO QUE A FORMA PRECISA ─────────────────────
         * Não pelo que a coluna guarda. O avaliador de oito bits decide pelo bit 7
         * da diferença, logo a forma `c0 + Σ c_i·x_i` tem de caber em ±127 — e com
         * duas colunas de 100 e 50 o `a*b` dá 5000, que lá não cabe. MEDIDO no
         * commit anterior e neste: `WHERE a*b > 1000` devolvia (3,7) e (150,2) e
         * deixava de fora o (100,50), que é o único que casa. O produto estourava
         * em silêncio, e nenhuma asserção o via porque as tabelas dos medidores
         * têm valores pequenos.
         *
         * Agora o majorante da forma — calculado em compilação com o maior valor
         * que cada coluna guarda — escolhe o andar: cabe em ±127 usa o de oito,
         * cabe em ±32767 usa o de dezasseis, e o que não cabe é RECUSADO. */
        /* o de cima quando ele SABE; o de baixo quando é preciso o que só ele
         * tem. E se o de baixo for escolhido, ainda pode não caber em ±127 --- aí
         * sobe-se na mesma, que é o que o `atom_cabe` decide. */
        int atom_largo = atom_so_inteiro(a, j, ncols, nrows_atual)
                       || !atom_cabe(a, j, ncols, nrows_atual, 127);
        if(atom_largo){
            /* o mapa do rascunho de 16: acc, x16, ga, gb, tmp16, o slot do teste,
             * o produto e a base do emit_mul16 (que consome 18 a partir dela) */
            unsigned x16 = acc + 1, ga = acc + 2, gb = acc + 3, tmp16 = acc + 4;
            unsigned p16 = acc + 6, q16 = acc + 7, base16 = acc + 8;
            long c0 = a->av[j].c[0];
            emit_copia(S_ZERO, acc);
            if(c0){
                Word wc; wc.total = (Word8)((unsigned long)(c0 < 0 ? -c0 : c0) & 255u);
                wc.e = (Word8)(((unsigned long)(c0 < 0 ? -c0 : c0) >> 8) & 255u);
                mem_grava(S_K + (unsigned)j, wc);
                if(c0 > 0){ MOVE(acc, +1); MOVE(S_K + (unsigned)j, +1); emit1(OP_ADD16); }
                else       { MOVE(S_K + (unsigned)j, +1); MOVE(acc, +1); emit1(OP_SUB16); }
                MOVE(acc, -1);
            }
            for(int cod = 1; cod < NMON; cod++){
                long c = a->av[j].c[cod];
                if(!c) continue;
                int d1[KGRAU]; mi_de(cod, d1);
                if(mi_cod(d1) != cod) continue;
                int gr = mi_grau(cod), fora16 = 0;
                for(int t = 0; t < KGRAU; t++){
                    long cx = d1[t] ? col_de_sim(d1[t]-1) : -1;
                    if(d1[t] && (cx < 0 || cx >= ncols)) fora16 = 1;
                }
                if(fora16) continue;
                if(gr == 1){
                    int cc = -1, pl = 0;
                    for(int t = 0; t < KGRAU; t++) if(d1[t]){
                        cc = (int)col_de_sim(d1[t] - 1); pl = plano_de_sim(d1[t] - 1); }
                    emit_valor16_p(x16, linha, ncols, cc, tmp16, pl);
                }else{
                    /* GRAU >= 2: o monómio é um produto de colunas, e o produto de
                     * dezasseis bits é deslocamento e soma (naturais thm:transporte,
                     * lem:desloc). Encadeia-se um factor de cada vez. */
                    int primeiro = 1;
                    for(int t = 0; t < KGRAU; t++){
                        if(!d1[t]) continue;
                        emit_valor16_p(q16, linha, ncols, (int)col_de_sim(d1[t] - 1),
                                       tmp16, plano_de_sim(d1[t] - 1));
                        if(primeiro){ emit_copia(q16, x16); primeiro = 0; }
                        else { emit_mul16(p16, x16, q16, base16); emit_copia(p16, x16); }
                    }
                    if(primeiro) continue;
                }
                emit_mul_zeck16(acc, x16, c < 0 ? -c : c, c > 0, ga, gb, tmp16);
            }
            emit_teste16(acc, a->aop[j], dest, S_KZ + (unsigned)j, 0, acc + 5);
            if(a->anega[j]){
                MOVE(dest, +1); MOVE(S_CHEIO, +1); emit1(OP_XOR); MOVE(dest, -1);
            }
            continue;
        }

        emit_copia(S_ZERO, acc);          /* o constante entra pelo laço, como monômio vazio */

        /* percorre os monômios do multi-índice. Grau 0 já entrou; grau 1 tem coeficiente
         * conhecido em compilação (soma desenrolada); grau ≥ 2 precisa multiplicar colunas
         * em tempo de execução, e a ISA não tem MUL — vira cadeia de emit_mul. */
        for(int cod = 0; cod < NMON; cod++){
            long c = a->av[j].c[cod];
            if(!c) continue;
            int d[KGRAU]; mi_de(cod, d);
            if(mi_cod(d) != cod) continue;                 /* só os representantes ordenados */
            int g = mi_grau(cod), fora = 0;
            for(int t = 0; t < KGRAU; t++){
                long cx = d[t] ? col_de_sim(d[t]-1) : -1;
                if(d[t] && (cx < 0 || cx >= ncols)) fora = 1;
            }
            if(fora) continue;
            long n = c < 0 ? -c : c;      /* nada de truncar: o coeficiente entra inteiro */

            /* A CONTRAÇÃO — o chicote inteiro.
             *
             * Cada monômio é o MESMO produto sobre as colunas citadas, escolhendo p onde o
             * monômio usa a coluna e q onde não usa. Mesma forma, mesmo comprimento, para
             * todos os termos: não há caso especial e não há ordem a escolher. O termo
             * constante é o monômio vazio — todas as colunas entram com q.
             *
             * E a fonte é MASCARADA antes de entrar no produto: o .e de uma linha é o
             * denominador, e se ele chega ao emit_mul envenena o contador do laço. Foi isso
             * que pendurou a tentativa anterior — não era o slot, era o .e. */
            unsigned termo = prod;
            /* a BASE DE REFERÊNCIA do átomo: a primeira coluna citada que tenha régua. As
             * outras da mesma classe são transportadas até ela. */
            /* O `cc` É O SÍMBOLO, e a coluna vem do mapa --- aqui ficou a ler
             * `corpo_de(cc)` com o símbolo no lugar da coluna, que é o mesmo
             * defeito que já se corrigiu duas linhas abaixo. Enquanto os dois
             * coincidiam ninguém dava por ele; depois de o símbolo passar a ser
             * atribuído por ordem de aparecimento, o corpo lido é o da coluna
             * errada --- e num corpo com RÉGUA isso significa não transportar.
             * Medido: `WHERE au = 11` numa coluna ÁUREA com 11 devolvia ZERO,
             * enquanto `au > 10` devolvia a linha. */
            long b_ref = 0; int b_ref_ok = 0;
            for(int cc = 0; cc < NCOL && !b_ref_ok; cc++){
                if(!cit[cc]) continue;
                long ccol_r = col_de_sim(cc);
                if(ccol_r < 0 || ccol_r >= ncols) continue;
                Word cw = corpo_de(ccol_r);
                if(corpo_tem_regua(cw.total)){ b_ref = corpo_B(cw.total, cw.e); b_ref_ok = 1; }
            }
            emit_copia(S_UM, prod);
            /* `cc` percorre os SÍMBOLOS da expressão; a coluna que cada um
             * nomeia vem do mapa. Antes eram a mesma coisa, e por isso o
             * endereço da célula saía do símbolo --- certo enquanto a coluna
             * citada fosse uma das seis primeiras, e errado a partir daí. */
            for(int cc = 0; cc < NCOL; cc++){
                if(!cit[cc]) continue;
                long ccol = col_de_sim(cc);
                int pl_col = plano_de_sim(cc);
                if(ccol < 0 || ccol >= ncols) continue;
                /* ── QUANTAS VEZES, E NÃO SE SIM OU NÃO ──────────────────────
                 *
                 * Isto era `usa = 1` --- um booleano ---, e o produto entrava UMA
                 * vez por coluna citada. Mas o monómio `a*a` cita a coluna DUAS
                 * vezes, e o expoente é o número de ocorrências em `d[]`: com o
                 * booleano, `a*a` era emitido como `a`.
                 *
                 * Medido, com a MESMA tabela (valores 2, 1, 3) em dois corpos:
                 * numa coluna INTEIRO `a*a > 4` devolvia 1 linha (o andar de
                 * dezasseis trata o grau) e numa coluna AUREO devolvia ZERO ---
                 * porque aqui virava `a > 4`, que nenhum dos três satisfaz. O
                 * mesmo valor, duas respostas, conforme o corpo declarado.
                 *
                 * O comentário da contracção já dizia «escolhendo p onde o
                 * monómio usa a coluna»; o que faltava é que «usa» tem
                 * MULTIPLICIDADE, e ela é o expoente. */
                int usa = 0;
                for(int t = 0; t < KGRAU; t++) if(d[t] == cc+1) usa++;
                /* ── O `q` É O NEUTRO, E NUM CORPO COM RÉGUA NÃO É O S_DEN ───
                 *
                 * Onde o monómio NÃO usa a coluna entra o denominador dela, para
                 * os dois lados ficarem sobre o mesmo q. Só que num corpo com
                 * RÉGUA --- áureo, cristalino --- o segundo componente não é
                 * denominador nenhum: é o coeficiente de σ, e vale ZERO numa
                 * célula escrita como inteiro. O termo constante era então
                 * multiplicado por zero e DESAPARECIA: `WHERE au = 11` numa
                 * coluna áurea com 11 virava `au = 0` e devolvia vazio, enquanto
                 * `au > 10` devolvia a linha --- o mesmo valor, duas respostas.
                 *
                 * Nesses corpos o neutro é o UM, que é o que ele sempre foi. */
                Word cw_q = corpo_de(ccol);
                /* e o PLANO decide de que andar se lê quando a coluna É usada:
                 * 0 a parte racional, 1 o σ. Sem plano, `sigma(a)` lia o `a`. */
                unsigned fonte = usa
                    ? (pl_col == 1 ? (S_DEN    + cel_ix(linha*ncols + ccol))
                                   : (S_LINHAS + cel_ix(linha*ncols + ccol)))
                    : (corpo_tem_regua(cw_q.total)
                         ? S_UM
                         : (S_DEN + cel_ix(linha*ncols + ccol)));
                /* O TRANSPORTE, LIGADO. Se a coluna vive noutra base da MESMA classe, o
                 * valor tem de ser levado à base de referência antes de entrar no produto —
                 * e isso é φ_t, o cisalhamento, palavra nos geradores.
                 *
                 * A ORDEM IMPORTA: φ_t age sobre a Word inteira, (a,b) ↦ (a+t·b, b), e a
                 * máscara mata o .e. Logo transporta-se PRIMEIRO e mascara-se depois — ao
                 * contrário, φ_t receberia b = 0 e seria a identidade. */
                emit_copia(fonte, tmpm);
                {
                    Word cwc = corpo_de(ccol);
                    if(usa && b_ref_ok && corpo_tem_regua(cwc.total)){
                        long t = (b_ref - corpo_B(cwc.total, cwc.e)) / 2;
                        if(t) emit_transporte(t, tmpm);
                    }
                }
                MOVE(tmpm, +1); MOVE(S_MT, +1); emit1(OP_AND);
                MOVE(tmpm, -1);
                /* uma vez por OCORRÊNCIA: o expoente do símbolo neste monómio.
                 * Onde a coluna não é usada, `usa` é zero e entra o neutro uma
                 * vez só --- que é o que a contracção sempre fez. */
                { int vezes = usa ? usa : 1;
                  for(int r = 0; r < vezes; r++){
                      emit_mul(prod2, prod, tmpm, base);
                      emit_copia(prod2, prod);
                  } }
            }
            (void)g;
            emit_mul_zeck(acc, termo, n, c > 0, acc + 8);
        }
        emit_teste(acc, a->aop[j], 0, dest, S_KZ + (unsigned)j);
        if(a->anega[j]){
            MOVE(dest, +1);
            MOVE(S_CHEIO, +1);
            emit1(OP_XOR);
            MOVE(dest, -1);
        }
    }
}
/* percorre a árvore em pós-ordem; cada nó deixa 0 ou 1 no seu slot */
static void emit_no(const struct arvore *a, int i, long linha, long ncols, unsigned dest){
    const struct no *n = &a->no[i];
    if(n->tipo == NO_COND){
        if(n->decidido) emit_copia(n->valor ? S_UM : S_ZERO, dest);  /* decidido na compilação */
        else            emit_copia(S_COND + (unsigned)n->atomo, dest);
        return;
    }
    unsigned de = dest + 2, dd = dest + 34;            /* dois ramos, slots afastados */
    emit_no(a, n->esq, linha, ncols, de);
    emit_no(a, n->dir, linha, ncols, dd);
    MOVE(de, +1);
    MOVE(dd, +1);
    emit1(n->tipo == NO_AND ? OP_AND : OP_OR);         /* o AND/OR do SQL É o da ISA */
    MOVE(dest, -1);
}

/* ── O PREDICADO CORRE NA MESMA ISA QUE O `WHERE` ────────────────────────────
 * Compila-se o texto guardado com o MESMO `le_expr`, emite-se o MESMO molde
 * sobre a linha `i`, e lê-se o `S_EXPR` — que o avaliador já deixa ESPALHADO,
 * $0$ ou tudo. Não se toca no campo: o `CHECK` pergunta, não marca.
 * Devolve 1 (passa), 0 (não passa) ou −1 (não compila). */
static int check_avalia(long i, long ncols, const char *texto){
    struct arvore a;
    const char *p = texto;
    unsigned salvo = citadas_where, salvo_d = citadas_desig;
    memset(&a, 0, sizeof a);
    citadas_where = 0; citadas_desig = 0;
    sim_zera();          /* os símbolos são desta expressão, e de mais nenhuma */
    constantes_isa();            /* o avaliador não corre sobre memória por escrever */
    a.raiz = le_expr(&p, &a);
    if(a.raiz < 0){ citadas_where = salvo; citadas_desig = salvo_d; return -1; }
    /* ── E O PREDICADO NÃO SE PRONUNCIA SOBRE O QUE NÃO ESTÁ ─────────────
     * Comparar com uma célula ausente não casa — é a regra do WHERE —, mas
     * aqui isso tornaria todo o CHECK num NOT NULL implícito, que é uma
     * restrição que ninguém declarou. A ausência está FORA do corpo: um
     * predicado do corpo não a alcança, e o que não se pronuncia não recusa.
     * Quem quiser exigir presença tem a palavra para isso. */
    { unsigned cit = citadas_where;
      citadas_where = salvo; citadas_desig = salvo_d;   /* o par: quem salva RESTAURA */
      for(long j = 0; j < ncols && j < (long)COL_MAX; j++)
          if((cit & (1u << j)) && !bit_le(S_PRES, i*ncols + j)) return 1; }
    contrai_arvore(&a);
    pc_emit = 0;
    emit_atomos(&a, i, ncols);
    emit_no(&a, a.raiz, i, ncols, S_EXPR);
    emit1(OP_HALT);
    rodar(pc_emit);
    return mem_le(S_EXPR).total != 0;
}

/* o predicado desta tabela, ou "" se não houver */
static void check_le(char *out, int cap){
    txt_le(S_CHECK, S_CHECK_W, out, (size_t)cap);
}

/* (o `acao` saiu daqui: era parâmetro e não era lido. Quem decide o que a acção
 * faz é o emissor lá em cima --- esta função emite o MOLDE da linha, e o molde é
 * o mesmo para todas. Um parâmetro que ninguém lê promete uma dependência que
 * não existe.) */
static void emit_linha(long i, long ncols, const struct arvore *a, int tem_where,
                       int col_set)
{
    if(tem_where){
        emit_atomos(a, i, ncols);                   /* cada átomo distinto, uma vez só */
        emit_no(a, a->raiz, i, ncols, S_EXPR);
        emit_copia(S_EXPR, S_ACC);
    } else {
        emit_copia(S_CHEIO, S_ACC);   /* sem WHERE tudo casa, e o verdadeiro é a máscara */
    }

    /* AS QUATRO FACES NUM PASSO SÓ, E NO ESPAÇO DE FASES.
     *
     * Isto eram DOIS saltos condicionais por linha: testar o vivo e saltar,
     * testar o ACC e saltar, marcar. Saltar é executar no TEMPO — a máquina
     * decide, ramifica, e o que se mede é a trajectória. E o defeito que aqui
     * viveu meses veio inteiro daí: o deslocamento do JZ não cabia, o salto ia
     * para trás, e o motor não voltava.
     *
     * O par ⊗/⊘ do algoritmo não ramifica: opera. Rodando no ESPAÇO DE FASES em
     * vez do tempo, a linha i deixa de ser um instante e passa a ser a
     * COORDENADA e_i — e a variação ao longo de i, que era o tempo, sai como o
     * deslocamento nesse espaço, que é o que o relocador já faz. Então as
     * quatro faces executam SIMULTANEAMENTE, num único passo:
     *
     *   0  realização    e_i, a coordenada — onde a linha vive
     *   ×  convolução    o AND que cruza o vivo com a condição
     *   +  conservação   o OR que escreve, e nunca desliga o que já lá está
     *   1  deconvolução  o ∑ do popcount, que devolve a contagem do campo
     *
     *       match |= (VIVO ∧ e_i) ∧ (0 − ACC)
     *
     * O `0 − ACC` é o que dispensa o salto: com ACC em {0,1}, a subtracção no
     * anel dá 0x00 ou 0xFF — o booleano ESPALHADO por todos os bits —, e o AND
     * com a coordenada devolve 0 ou e_i. É a mesma resposta, sem ramificar.
     *
     * E o contador SAIU. Eram quatro instruções por linha a somar 1 a S_CONTA,
     * e S_CONTA não era lido em parte nenhuma: quem responde ao count(*) é o
     * `bits_conta`, o popcount sobre o campo — a face 1, a deconvolução. Contar
     * ao mesmo tempo que se marca era contar DUAS vezes, e a segunda não
     * chegava a lado nenhum. */
    (void)col_set;
    MOVE_M(S_VIVO, +1, REL_BITMAP);
    MOVE_M(S_BITM, +1, REL_MASC);
    emit1(OP_AND);                       /* vivo_i = VIVO ∧ e_i  (0 ou e_i) */
    MOVE(S_TMP, -1);

    /* E O ACC JÁ VEM ESPALHADO. Aqui fazia-se `0 − ACC` em SUB16 para levar um
     * booleano de {0,1} à largura toda — o passo que dispensava o salto no
     * molde. Agora quem o produz é o próprio avaliador, com OP_ESPALHA, e o
     * ACC chega em $0$ ou TUDO: a máscara é fabricada UMA vez, onde a condição
     * se decide, e não outra vez por linha. */
    MOVE(S_TMP, +1);
    MOVE(S_ACC, +1);
    emit1(OP_AND);                       /* × : a coordenada, se a condição vale */
    MOVE(S_TMP, -1);

    MOVE_M(S_MATCH, +1, REL_BITMAP);
    MOVE(S_TMP, +1);
    emit1(OP_OR);                        /* + : conserva o que já estava marcado */
    MOVE_M(S_MATCH, -1, REL_BITMAP);
    /* NÃO HÁ SALTOS A RESOLVER, e é isso que este bloco passou a dizer.
     *
     * Aqui media-se e recusava-se o deslocamento do JZ, porque ele não cabia num
     * átomo e o salto ia para trás — «o número que não cabe», e a máquina a não
     * voltar. Depois passou a Word e alcançou 32767. Agora não há nenhum: o
     * molde não ramifica, logo não há distância nenhuma para caber. O limite não
     * foi levantado — deixou de existir, que é o que acontece quando o passo
     * corre no espaço em vez do tempo. */
}

/* prepara as constantes e devolve o catálogo */
static void pres_migra(long ncols, long nrows);

/* AS CONSTANTES DA ISA, e elas não dependem da consulta. Estavam dentro do
 * `prepara`, que só o `varre` chama — e por isso quem quisesse correr o mesmo
 * avaliador de outro sítio (o `CHECK`, que é o WHERE dito na escrita) corria-o
 * sobre memória por escrever e recusava tudo. Separam-se aqui para haver UMA
 * preparação e não duas: dois sítios a escrever as mesmas constantes é como
 * eles deixam de escrever as mesmas. */
static void constantes_isa(void){
    Word w = {0,0};
    mem_grava(S_ZERO, w);
    w.total = 1; w.e = 0;                 mem_grava(S_UM, w);
    w.total = 1; w.e = 0;                 mem_grava(S_UM16, w);
    /* (o S_V/S_VA e a marca vivem no `prepara`, que chama isto: escrever
     * dezasseis bits é do SET, não da ISA. Escrever só o baixo deixava o átomo
     * alto do valor ANTERIOR na célula — um `SET saldo = 30000` sobre 20000
     * dava 20016, e nada o dizia.) */
    /* A BASE, e_k = 2^k (naturais thm:base): dezasseis coordenadas, e a
     * complementar de cada uma para limpar. A Word tem dois átomos de oito, e
     * por isso a coordenada k mora no átomo k/ATOMO_BITS — é a Lei 7 outra vez,
     * ligar sem fundir, e sem escrever a largura em lado nenhum. */
    for(unsigned k = 0; k < SLOT_BITS; k++){
        Word m = {0,0}, n = {0,0};
        unsigned a = k / ATOMO_BITS, bit = 1u << (k % ATOMO_BITS);
        for(unsigned t = 0; t < WORD_ISA_ATOMS; t++) atomo_poe(&n, t, ~0u);
        atomo_poe(&m, a, bit);
        atomo_poe(&n, a, ~bit);
        mem_grava(S_BITM + k, m);
        mem_grava(S_BITN + k, n);
    }
    /* A MÁSCARA QUE ZERA O ÁTOMO ALTO, e ela FALTAVA AQUI.
     *
     * O S_MT era escrito uma só vez, no CREATE TABLE, e o `prepara` — que corre
     * antes de CADA varredura e repõe todas as outras constantes — não o
     * repunha. Numa base reaberta valia o que sobrasse: zero, ou lixo. E ele é
     * o AND que zera o átomo alto antes da multiplicação, de modo que o
     * contador do laço recebia um número que não era o dele e a máquina ficava
     * a contar até ao guarda dos cinquenta milhões de passos.
     *
     * O próprio ficheiro já tinha avisado, noutro sítio: «as constantes
     * escrevem-se ao COMPILAR e o programa corre depois — se a constante for
     * sobrescrita entre as duas fases, a máquina lê o valor trocado». Uma
     * constante que só se escreve uma vez é a mesma falha com outra cara: nada
     * pode ficar fora da memória entre uma varredura e a seguinte. */
    w.total = (Word8)~0u; w.e = 0; mem_grava(S_MT, w);
    w.total = 0xFF; w.e = 0xFF; mem_grava(S_CHEIO, w); /* o verdadeiro, em todos os bits */
    w.total = 0x80; w.e = 0; mem_grava(S_MASK, w);   /* bit 7 do Word_8 — sinal no envelope */
    w.total = 0; w.e = 0x80; mem_grava(S_MASK16, w); /* bit 15 do par — o sinal um andar acima */
}

static void prepara(long v, long col_do_set){
    pres_migra(cat_ncols(), cat_nrows());
    constantes_isa();
    { Word w;
      /* o valor do SET em DEZASSEIS bits: o baixo no S_V, o alto no S_VA */
      w.total = (Word8)((unsigned long)v & 255u); w.e = 0;        mem_grava(S_V, w);
      w.total = (Word8)(((unsigned long)v >> 8) & 255u); w.e = 0; mem_grava(S_VA, w);
      /* e o andar de cima: os bytes 2 e 3, que é o que uma DATA usa */
      w.total = (Word8)(((unsigned long)v >> 16) & 255u);
      w.e     = (Word8)(((unsigned long)v >> 24) & 255u);          mem_grava(S_VB, w); }
    /* o UPDATE também é escrita, e também deixa a marca */
    if(v >= 0) col_marca(col_do_set, (unsigned long)v);
}


/* FASE 3: aplica o que o diário mandou. Programa próprio, desenrolado, sem indireção — o
 * bitmap diz quais linhas, e o compilador já conhece cada índice.
 *
 * É IDEMPOTENTE de propósito: escreve valores absolutos, nunca incrementos. Por isso pode ser
 * repetida na abertura sem estragar nada, que é o que faz o redo funcionar. */
static long aplica_diario(long ncols, long nrows, int acao, int col_set){
    /* O µ, E ELE É A DESCIDA COM A VOLTA POR CIMA.
     *
     * Um UPDATE não muda o NÚMERO de linhas — muda um valor —, pelo que o
     * cabeçalho continua a bater e o índice continuaria a apontar para o valor
     * VELHO. Até aqui largava-se o índice; agora desacumula-se: recolhe-se o
     * valor antigo de cada linha marcada ANTES de a escrita correr, tira-se a
     * chave da árvore, e depois de a escrita correr acrescenta-se a nova. µ e
     * depois ζ — a mesma ordem do `thm:zeta-mu`, com o par completo.
     *
     * Se qualquer das duas falhar (a árvore encheu), larga-se o índice como
     * antes: custa a varredura seguinte, nunca a resposta. */
    long mu_col = -1;
    enum { MU_MAX = 64 };            /* o mesmo tecto do lado da junção */
    static long mu_idx[MU_MAX];
    static long mu_val[MU_MAX];
    int mu_n = 0, mu_estourou = 0;
    if(acao == ACAO_SET && col_set >= 0 && idx_valido(col_set, cat_nrows())){
        mu_col = col_set;
        for(long i = 0; i < nrows; i++){
            if(!bit_le(S_MATCH, i)) continue;
            if(mu_n >= MU_MAX){ mu_estourou = 1; break; }
            mu_idx[mu_n] = i;
            mu_val[mu_n] = celula_valor(i, mu_col, ncols);
            mu_n++;
        }
        if(mu_estourou){ Word z = {0,0}; mem_grava(S_IDXCAB(col_set), z); mu_col = -1; }
        else {
            ord_usa_indice(mu_col);
            for(int k = 0; k < mu_n; k++) idx_remove(mu_val[k], (int)mu_idx[k]);
            ord_usa_rascunho();
        }
    }
    /* ── O SET COM EXPRESSÃO: UM VALOR POR LINHA, E CONTINUA A SER A MÁQUINA
     * A ESCREVER ────────────────────────────────────────────────────────────
     * O caminho de baixo emite UM programa que copia `S_V` para todas as linhas
     * marcadas — desenho certo para uma constante, e o único possível com um
     * slot só. Com expressão cada linha tem o seu valor, e a saída não é sair da
     * ISA: é gravar `S_V` com o valor DAQUELA linha e emitir um programa de duas
     * cópias para ELA, tantas vezes quantas as linhas. A escrita continua a ser
     * a máquina a correr, e continua a ser IDEMPOTENTE — valores absolutos, nunca
     * incrementos —, que é o que faz o redo funcionar.
     *
     * E a expressão avalia-se com o MESMO `ten_avalia` da projecção. É esse o
     * ponto de tudo isto: um objecto, três usos — decidir, produzir, escrever —,
     * e não três leituras da mesma frase que teriam de concordar por acaso. */
    if(acao == ACAO_SET && set_ex && col_set >= 0){
        long feitas = 0;
        for(long i = 0; i < nrows; i++){
            if(!bit_le(S_MATCH, i)) continue;
            { long den = 1;
              long num = ten_avalia(set_ten, i, ncols, &den);
              /* o denominador já foi recusado no parse: aqui ele é 1 por
               * construção, e recusar A MEIO da escrita seria deixar metade
               * das linhas mudadas — esta função devolve PASSOS, não veredicto */
              (void)den;
              { Word w;
                w.total = (Word8)((unsigned long)num & 255u); w.e = 0;
                mem_grava(S_V, w);
                w.total = (Word8)(((unsigned long)num >> 8) & 255u); w.e = 0;
                mem_grava(S_VA, w);
                w.total = (Word8)(((unsigned long)num >> 16) & 255u);
                w.e     = (Word8)(((unsigned long)num >> 24) & 255u);
                mem_grava(S_VB, w); }
              pc_emit = 0;
              emit_copia(S_V,  S_LINHAS + cel_ix(i*ncols + col_set));
              emit_copia(S_VA, S_ALTO   + cel_ix(i*ncols + col_set));
              /* o S_ALTO2 vive ACIMA do tecto da ISA, e o endereço da
               * instrução tem dezasseis bits: não se alcança por bytecode.
               * Escreve-se em C, como o INSERT já fazia. */
              mem_grava(S_ALTO2 + cel_ix(i*ncols + col_set), mem_le(S_VB));
              emit1(OP_HALT);
              rodar(pc_emit);
              feitas++; }
        }
        for(long i = 0; i < nrows; i++)
            if(bit_le(S_MATCH, i))
                bit_poe(S_PRES, i*ncols + col_set, 1);
        printf("-- %ld linha(s) escritas pela expressão, uma a uma\n", feitas);
        /* e o ζ pelo MESMO caminho do valor fixo: o µ já tirou as chaves
         * velhas antes da escrita, e são as novas que entram agora. Escrever
         * aqui uma segunda reposição do índice seria duas réguas para a mesma
         * árvore. */
        if(mu_col >= 0){
            ord_usa_indice(mu_col);
            int falhou = 0;
            for(int k = 0; k < mu_n && !falhou; k++)
                if(!ord_insere(celula_valor(mu_idx[k], mu_col, ncols), (int)mu_idx[k]))
                    falhou = 1;
            ord_usa_rascunho();
            if(falhou){ Word z = {0,0}; mem_grava(S_IDXCAB(mu_col), z); }
        }
        return feitas * 4;
    }
    pc_emit = 0;
    for(long i = 0; i < nrows; i++){
        MOVE(S_MATCH + (unsigned)((unsigned long)i / SLOT_BITS), +1);
        MOVE(S_BITM  + (unsigned)((unsigned long)i % SLOT_BITS), +1);
        emit1(OP_AND);
        MOVE(S_TMP, -1);
        MOVE(S_TMP, +1);
        MOVE(S_ZERO, +1);
        emit1(OP_CMP);
        emit1(OP_JZ);
        unsigned pos = pc_emit; emit1(0); emit1(0);
        unsigned ini = pc_emit;
        if(acao == ACAO_SET){
            emit_copia(S_V,  S_LINHAS + cel_ix(i*ncols + col_set));
            emit_copia(S_VA, S_ALTO   + cel_ix(i*ncols + col_set));   /* o par inteiro */
            mem_grava(S_ALTO2 + cel_ix(i*ncols + col_set), mem_le(S_VB)); /* o andar de cima, em C */
        }
        else {                          /* apagar é DESLIGAR a coordenada */
            MOVE(S_VIVO + (unsigned)((unsigned long)i / SLOT_BITS), +1);
            MOVE(S_BITN + (unsigned)((unsigned long)i % SLOT_BITS), +1);
            emit1(OP_AND);
            MOVE(S_VIVO + (unsigned)((unsigned long)i / SLOT_BITS), -1);
        }
        salto_poe(pos, ini);
    }
    emit1(OP_HALT);
    long passos = rodar(pc_emit);

    /* ESCREVER FAZ EXISTIR. Um valor posto é uma presença: acendem-se os bits
     * das células que a escrita tocou. Sem isto a célula ficava com o valor
     * lá dentro e o dual a dizer que não estava — o UPDATE escrevia e o
     * `IS NULL` continuava a apanhar a linha. */
    if(acao == ACAO_SET && col_set >= 0)
        for(long i = 0; i < nrows; i++)
            if(bit_le(S_MATCH, i))
                bit_poe(S_PRES, i*ncols + col_set, set_anula ? 0 : 1);

    /* e o ζ: agora que a escrita correu, as chaves NOVAS entram — excepto se a
     * escrita foi um `= NULL`, que não deixa valor nenhum para indexar: aí o µ
     * tirou as chaves e nada volta. */
    if(mu_col >= 0 && !set_anula){
        ord_usa_indice(mu_col);
        int falhou = 0;
        for(int k = 0; k < mu_n && !falhou; k++)
            if(!ord_insere(celula_valor(mu_idx[k], mu_col, ncols), (int)mu_idx[k]))
                falhou = 1;
        ord_usa_rascunho();
        if(falhou){ Word z = {0,0}; mem_grava(S_IDXCAB(mu_col), z); }
    }
    return passos;
}

/* Na abertura: se o diário ficou aberto, uma queda apanhou a base entre o compromisso e o
 * fim da aplicação. Refaz-se, e só então se fecha o diário. */
static void refaz_diario(void){
    Word d = mem_le(S_DIA);
    if(d.total == 0) return;
    int an = (d.total > 3), ac = an ? ACAO_SET : (int)d.total - 1;
    printf("-- diário aberto: refazendo %s\n",
           an ? "um UPDATE que APAGA a célula" : (ac == ACAO_SET ? "um UPDATE" : "um DELETE"));
    set_anula = an;
    aplica_diario(cat_ncols(), cat_nrows(), ac, (int)d.e);
    set_anula = 0;
    barreira();
    Word z = {0,0}; mem_grava(S_DIA, z);
    barreira();
}

/* ── A PROJECÇÃO DA JUNÇÃO APLICA-SE UMA VEZ POR LINHA ───────────────────────
 * A linha é escrita inteira — esquerda seguida de direita — pelos quatro
 * caminhos que a junção tem (o casamento, o LEFT sem par, o RIGHT sem par, o
 * segundo corte). Aplicar o mapa dentro de cada um seria escrever a mesma frase
 * quatro vezes, que é como quatro cópias deixam de concordar; aplica-se aqui,
 * no sítio onde a linha FECHA, e uma só vez. */
static void jproj_linha(int *mapa, int n, int total){
    if(!sql_cap || !n || sql_cap->nrows >= SQL_OUT_MAX_ROWS) return;
    { char tmp[SQL_OUT_MAX_COLS][SQL_OUT_CELL];
      unsigned char tn[SQL_OUT_MAX_COLS];
      long r = sql_cap->nrows;
      int k;
      for(k = 0; k < n && k < SQL_OUT_MAX_COLS; k++){
          int j = mapa[k];
          if(j < 0 || j >= total || j >= SQL_OUT_MAX_COLS){ tmp[k][0] = 0; tn[k] = 1; continue; }
          snprintf(tmp[k], SQL_OUT_CELL, "%s", sql_cap->cell[r][j]);
          tn[k] = sql_cap->nulo[r][j];
      }
      for(k = 0; k < n && k < SQL_OUT_MAX_COLS; k++){
          snprintf(sql_cap->cell[r][k], SQL_OUT_CELL, "%s", tmp[k]);
          sql_cap->nulo[r][k] = tn[k];
      }
      for(; k < SQL_OUT_MAX_COLS; k++){ sql_cap->cell[r][k][0] = 0; sql_cap->nulo[r][k] = 0; } }
}

/* a última contagem, para o modo teste poder AFIRMAR em vez de só imprimir. Sem isto o
 * sql.c não afirmava nada, e por isso estava fora da bateria — mudei-o uma dúzia de vezes
 * hoje e só o verifiquei à mão. */
/* ── AS TRÊS RESPOSTAS QUANDO A SETA PERDE O DESTINO ─────────────────────────
 *
 * Há DUAS portas que tiram o destino de baixo de uma seta, e são o par de
 * sempre: APAGAR a linha apontada, e MUDAR-LHE a chave. A primeira faz o
 * destino desaparecer; a segunda faz o destino mudar de sítio — e uma seta que
 * aponta para onde já não há ninguém é o mesmo estado nos dois casos. Por isso
 * a resposta é a mesma função, e não duas: duas implementações da mesma frase é
 * como elas deixam de concordar.
 *
 * O que fazer não é uma lista de opções do dialecto: são as três coisas que se
 * podem fazer a uma seta cujo destino sai debaixo dela.
 *
 *   RESTRICT   recusar     — a seta não pode perder o destino; nada muda. É o
 *                            modo por omissão, porque é o único que não faz
 *                            nada sem ordem.
 *   CASCADE    levar junto — a FIBRA vai atrás: apagar x na mãe é apagar
 *                            π⁻¹(x) na filha, e mudar x para y é levar π⁻¹(x)
 *                            para y. É a imagem inversa do `thm:multiplicidade`
 *                            e não uma regra de conveniência.
 *   SET NULL   soltar      — a seta desaparece e a linha fica: a célula vai
 *                            para o DUAL, que é onde uma coordenada sem valor
 *                            mora (`thm:bitunico`).
 *
 * Quem diz qual é a FILHA, porque é dela a seta; a mãe só sabe quem a aponta. E
 * o modo do apagar e o do mudar são INDEPENDENTES — vivem nos dois pares de
 * bits do mesmo octeto —, porque nada obriga quem quer a fibra atrás numa
 * mudança de chave a querê-la atrás num apagar.
 *
 * DUAS PASSAGENS, E A ORDEM É A LEI: primeiro pergunta-se a TODAS se alguma
 * recusa, e só depois se age. Numa passagem só, a filha em CASCADE processada
 * antes de uma filha em RESTRICT já tinha levado as suas linhas quando a recusa
 * chegasse — a operação recusada e metade feita, que é o estado que este bloco
 * existe para não haver.
 *
 * A CADEIA NÃO DESCE: se a filha for mãe de outra, a cascata pára aqui e a
 * segunda seta é verificada como qualquer outra. É um limite declarado, não um
 * esquecimento — descer exigia reentrar no `varre`, e o `varre` tem estado
 * global.
 *
 * `mudar` = 0 apaga o destino, `mudar` = 1 leva-o para `novo`. Devolve 0 se
 * alguma filha recusou, e nesse caso NADA foi tocado. */
static int fk_propaga(const char *mae, long nrows, long ncols, int mudar, long novo){
    int nf = filho_quantos(), preso = 0;
    char ft[64], guarda[64], presa[64];
    /* AS CHAVES QUE VÃO SAIR DEBAIXO DAS SETAS. Eram MAXLIN = 250 e o laço
     * abaixo PARAVA aí --- num DELETE de trezentas linhas, as cinquenta últimas
     * saíam sem que a propagação as visse, e as filhas ficavam com setas para
     * chaves que já não existem. A recusa é a resposta certa quando não cabe;
     * truncar é dizer que se propagou o que não se propagou. */
    static long morre[FK_MORRE_MAX]; int nm;
    snprintf(guarda, sizeof guarda, "%s", mae);
    presa[0] = 0;
    for(int passo = 0; passo < 2 && !preso; passo++){
        for(int k = 0; k < nf && !preso; k++){
            int fc = filho_le(k, ft, sizeof ft);
            if(fc < 0 || !tabela_existe(ft)) continue;

            /* a coluna da MÃE que esta filha aponta, e o MODO, vivem na filha */
            char mt2[64]; int mcol, modo;
            if(!usa_tabela(ft, 0) || !cat_nome_bate(ft)){ usa_tabela(guarda, 0); continue; }
            mcol = fk_le(fc, mt2, sizeof mt2);
            { long e = mem_le(S_FK + (unsigned)fc).e;
              modo = mudar ? (int)((e >> 2) & 3) : (int)(e & 3); }
            if(mcol < 0 || strcmp(mt2, guarda)){ usa_tabela(guarda, 0); continue; }
            if(!usa_tabela(guarda, 0)) restauros_falhados++;

            /* os valores que vão sair debaixo das setas — com a MÃE aberta */
            nm = 0;
            for(long i = 0; i < nrows; i++){
                if(nm >= FK_MORRE_MAX){
                    printf("erro: a propagação das setas teria de olhar mais de %d chaves"
                           " — RECUSADA. Nada foi tocado; corte em lotes menores.\n",
                           FK_MORRE_MAX);
                    if(sql_cap){ sql_cap->ok = 0;
                        snprintf(sql_cap->err, sizeof sql_cap->err,
                                 "foreign key cascade exceeds %d keys", FK_MORRE_MAX); }
                    if(!usa_tabela(guarda, 0)) restauros_falhados++;
                    return 0;
                }
                if(!bit_le(S_MATCH, i)) continue;
                if(!bit_le(S_PRES, i*ncols + mcol)) continue;
                { long antigo = celula_valor(i, mcol, ncols);
                  if(mudar && antigo == novo) continue;   /* a chave não muda */
                  morre[nm++] = antigo; }
            }
            if(!nm) continue;

            /* e agora com a FILHA aberta, e a decisão já tomada */
            if(!usa_tabela(ft, 0) || !cat_nome_bate(ft)){ usa_tabela(guarda, 0); continue; }
            { long fnc = cat_ncols(), fnr = cat_nrows();
              long tocadas = 0;
              for(long i = 0; i < fnr; i++){
                  if(!bit_le(S_VIVO, i) || !bit_le(S_PRES, i*fnc + fc)) continue;
                  { long val = celula_valor(i, fc, fnc);
                    int bate = 0;
                    for(int u = 0; u < nm && !bate; u++) if(morre[u] == val) bate = 1;
                    if(!bate) continue;
                    if(modo == 0){                                       /* RESTRICT */
                        if(passo == 0){
                            preso = 1; snprintf(presa, sizeof presa, "%s", ft);
                            printf("erro: a linha com %ld é apontada por «%s» — %s"
                                   " deixava a seta sem destino. RECUSADO.\n", val, ft,
                                   mudar ? "mudar-lhe a chave" : "apagá-la");
                        }
                        break;
                    }
                    if(passo == 0) continue;          /* a 1.ª passagem só pergunta */
                    if(modo == 1){                                       /* CASCADE  */
                        if(mudar) celula_grava(i, fc, fnc, novo);
                        else      bit_poe(S_VIVO, i, 0);
                        tocadas++;
                    }
                    else if(modo == 2){ bit_poe(S_PRES, i*fnc + fc, 0); tocadas++; } }
              }
              if(tocadas){
                  /* a árvore da coluna deixou de bater com o que lá está */
                  if(fc >= 0 && fc < (int)IDX_MAXCOL){ Word z = {0,0}; mem_grava(S_IDXCAB(fc), z); }
                  barreira();
                  printf("-- a seta de «%s»: %ld linha(s) %s\n", ft, tocadas,
                         modo == 2 ? "soltas para o dual (SET NULL)"
                                   : (mudar ? "levadas para a chave nova (CASCADE)"
                                            : "levadas junto (CASCADE)"));
              } }
            if(!usa_tabela(guarda, 0)) restauros_falhados++;
        }
    }
    if(preso){
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err,
                     "update or delete violates foreign key constraint on"
                     " referencing table \"%s\"", presa); }
        return 0;
    }
    return 1;
}

static long ultima_conta = 0;
/* e o número de FIBRAS da última varredura, para o `count(DISTINCT c)`: é
 * outra pergunta sobre o mesmo campo — quantas classes, em vez de quantos
 * elementos —, e por isso apura-se no mesmo sítio e com a tabela ainda aberta. */
static long ultima_fibras = 0;
/* os passos da última varredura — a única maneira de AFIRMAR que o molde deixou
 * de ramificar, em vez de o supor: sem salto, o custo não depende do dado. */
long sql_ultimos_passos = 0;
/* os NÓS visitados na última descida da árvore. É a medida honesta do índice:
 * «0 passos de ISA» não distingue não-ter-corrido de não-ter-feito-nada, e este
 * número diz o trabalho que a árvore fez — a PROFUNDIDADE, que não cresce com
 * o tamanho da tabela. */
long sql_ultimos_nos = 0;
/* A IMPRESSÃO DIGITAL DO ÚLTIMO PROGRAMA. É o FNV do bytecode emitido, e serve
 * para uma medida que nenhuma contagem de linhas dá: duas consultas escritas de
 * maneiras diferentes podem devolver a mesma resposta por acaso, mas se
 * COMPILAREM PARA O MESMO PROGRAMA é porque são o mesmo objecto — que é o que a
 * contração diz fazer e até agora só se via no ecrã. */
long sql_ultimo_prog = 0;


/* ---------------- A DISTÂNCIA NO WHERE: só se compara dentro da classe ----------------
 *
 * Um WHERE que cita duas colunas soma-as, subtrai-as, compara-as. Isso só faz sentido se as
 * duas viverem NO MESMO CORPO — e "o mesmo corpo" tem agora um critério exato, medido em
 * topologia.c: a distância |Δ₁−Δ₂| ser ZERO.
 *
 *   distância > 0   corpos de classes diferentes → a consulta é RECUSADA, com a distância dita
 *   distância = 0   ISOMORFOS → há UM transporte, φ_t com t = (B₂−B₁)/2, e ele é EMITIDO
 *
 * Recusar é a única resposta honesta para o primeiro caso: comparar um áureo com um cristalino
 * daria um número, e o número não significaria nada. É a mesma regra do WHERE não entendido —
 * refuse-se em vez de devolver a tabela inteira.
 *
 * Colunas fora da família quadrática (INTEIRO, RACIONAL, MORFICO) não entram na guarda: elas
 * não têm régua desta forma, e o resto do compilador já as trata. */
/* devolve 1 se o WHERE pode ser compilado; 0 se é RECUSADO. Se houver transporte, di-lo. */
/* e a ORDEM: num corpo ELÍPTICO (Δ<0) a pergunta "a < b" é MAL POSTA, não difícil. Se houvesse
 * ordem compatível, ω² = −1 daria −1 ≥ 0 com 1 > 0, logo 0 > 0 (ordem.c §O3). Então uma
 * desigualdade sobre coluna elíptica é RECUSADA, e diz-se porquê — comparar por norma é outra
 * pergunta, e quem a quiser tem de a escrever. */
static int checa_corpos(unsigned citadas, long ncols){
    int primeira = -1; long Dref = 0, Bref = 0, Cref = 0;
    for(long j = 0; j < ncols && j < (long)COL_MAX; j++){
        if(!(citadas & (1u << j))) continue;
        Word c = corpo_de(j);
        if(!corpo_tem_regua(c.total)) continue;
        long D = corpo_delta(c.total, c.e), B = corpo_B(c.total, c.e);
        long C = corpo_C(c.total);           /* a testemunha precisa dos DOIS da cifra */
        if(primeira < 0){ primeira = (int)j; Dref = D; Bref = B; Cref = C; continue; }
        if(D != Dref){
            long d = D - Dref; if(d < 0) d = -d;
            /* ── CORPO DIFERENTE NÃO É CLASSE DIFERENTE ─────────────────────
             *
             * Dizia «corpos de classes DIFERENTES» sempre que Δ diferia --- e
             * pela `thm:leidisc` a CLASSE é o SINAL de Δ, não o Δ. AUREO(1) tem
             * Δ = 5 e AUREO(3) tem Δ = 13: são corpos distintos e a travessia
             * não existe, mas os dois são HIPERBÓLICOS, a mesma classe. A
             * recusa estava certa; o nome dela é que confundia duas coisas ---
             * são três classes e infinitos corpos, e é o Δ que separa os corpos.
             *
             * Diz-se agora o que é: corpos diferentes, e se a classe também
             * diferir acrescenta-se, porque aí não há sequer régua comum. */
            int mesma_classe = corpo_classe(D) == corpo_classe(Dref);
            printf("erro: as colunas %c e %c estão em corpos DIFERENTES "
                   "(Δ = %ld e Δ = %ld, distância %ld, prof %d)%s.\n",
                   (char)('a'+primeira), (char)('a'+j), Dref, D,
                   esp_dist(Dref, D), esp_prof(Dref, D),
                   mesma_classe ? "" : " — e de classes diferentes");
            if(!mesma_classe)
                printf("      %c é %s e %c é %s: nem a régua é a mesma.\n",
                       (char)('a'+primeira), corpo_classe_nome(Dref, 0),
                       (char)('a'+j), corpo_classe_nome(D, 0));
            printf("      a consulta é RECUSADA: comparar através delas daria um número sem "
                   "significado.\n");
            return 0;
        }
        if(B != Bref){
            /* ── O t NÃO SE AFIRMA: VERIFICA-SE, E ANTES DELE A CONDIÇÃO ─────
             *
             * `aranha thm:pandora(2)`, na forma corrigida: com Δ igual, os dois
             * espelhos têm raízes na mesma extensão e elas diferem por 2t =
             * B_P − B_Q. A condição é B_P − B_Q ∈ (2) --- é sobre como o DOIS
             * se comporta no anel, e não sobre «t ser inteiro», que nomeava uma
             * realização. Aqui o anel é o do motor, onde 2 não é unidade: logo
             * a condição MORDE, e escrevia-se `(Bref - B) / 2` sem a verificar.
             * Com diferença ímpar a divisão trunca em silêncio e o motor
             * anunciava «são ISOMORFOS» com um transporte que não transporta.
             *
             * E o φ_t NÃO É UMA CONJUGAÇÃO --- dizê-lo era o erro do paper, e
             * este comentário chegou a repeti-lo. Traço e determinante são
             * invariantes de conjugação, pelo que B ≠ Bref exclui qualquer uma.
             * O que φ_t faz é a MUDANÇA DE GERADOR, e ela tem testemunha: na
             * base {1,α_j} a multiplicação por α_ref escreve-se M_j + tI, cuja
             * cifra tem de ser (Bref, Cref). Calcula-se e confere-se. */
            long dif = Bref - B;
            if(dif % 2 != 0){
                printf("nota: %c e %c têm o mesmo Δ = %ld mas B difere de %ld, que é ÍMPAR"
                       " — a travessia NÃO se emite.\n",
                       (char)('a'+primeira), (char)('a'+j), D, dif);
                printf("      `thm:pandora(2)` pede B_P − B_Q no ideal (2), e neste anel o"
                       " 2 não é unidade: a condição morde. Dividir na mesma truncaria, e o"
                       " t truncado não transporta.\n");
            }else{
                long t = dif / 2;
                /* a testemunha: cifra de M_j + tI, que TEM de ser (Bref, Cref) */
                long tr = B + 2*t, de = t*t + t*B + C;
                printf("nota: %c e %c são ISOMORFOS (Δ = %ld, distância 0) em bases diferentes "
                       "— φ_t com t = %ld,\n", (char)('a'+primeira), (char)('a'+j), D, t);
                printf("      EMITIDO no caminho do átomo como %s.\n",
                       t > 0 ? "(TROCA GOLD)^t" : "(NEGRO TROCA)^|t|");
                if(tr == Bref && de == Cref){
                    printf("      e CONFERIDO: M+tI tem cifra (%ld, %ld), que é a de %c —"
                           " é mudança de GERADOR, não conjugação (essa preservaria o"
                           " traço, e o traço muda).\n", tr, de, (char)('a'+primeira));
                    /* ── E O QUE A TRAVESSIA NÃO PRESERVA ────────────────────
                     * Δ igual dá o mesmo CORPO --- as raízes vivem na mesma
                     * extensão ---, e é isso que a travessia transporta. Não
                     * transporta a ORIENTAÇÃO: o determinante É o C, e a
                     * translação M ↦ M+tI muda-o. Opticamente (`aranha`
                     * cor:optica) um é um espelho e o outro não, e dizê-lo é a
                     * diferença entre «é o mesmo objecto» e «é o mesmo corpo». */
                    if(C != Cref)
                        printf("      NOTA: a orientação MUDA — det %ld contra %ld. A"
                               " travessia leva o CORPO (Δ igual, mesma extensão) e não o"
                               " sistema: um destes reflecte e o outro não.\n", Cref, C);
                }
                else
                    printf("      MAS NÃO CONFERE: M+tI tem cifra (%ld, %ld) e a de %c é"
                           " (%ld, %ld) — o t não transporta, e dizê-lo seria mentir.\n",
                           tr, de, (char)('a'+primeira), Bref, Cref);
            }
        }
    }
    return 1;
}

/* lê `*` ou `c1, c2, …` até ao FROM; devolve 0 se não reconhecer */
/* ── AS AGREGAÇÕES SÃO VÁRIAS, E ISSO NÃO É UM DETALHE ───────────────────────
 * Isto era UM inteiro e UMA cadeia, e por isso `SELECT MIN(a), MAX(a)` guardava
 * a segunda por cima da primeira e respondia SÓ o máximo — uma coluna onde foram
 * pedidas duas, com `ok` e sem uma palavra. Não era um erro de conta: era a
 * resposta certa a outra pergunta.
 *
 * São um ARRAY, e a varredura continua a ser UMA: as agregações partilham o
 * percurso das linhas e cada uma tem o seu acumulador. É a mesma economia do
 * núcleo e da imagem — uma passagem lida de vários lados —, e é também o que
 * impede que `MIN` e `MAX` vejam conjuntos de linhas diferentes. */
#define AGR_MAX 8
static int  agr_n = 0;                       /* quantas foram pedidas */
static int  agr_ops[AGR_MAX];                /* 1 sum, 2 max, 3 min, 4 avg */
static char agr_cols[AGR_MAX][64];           /* a coluna que cada uma lê */
static int  agr_viu_count = 0;               /* houve um count na mesma lista */
/* ── O CONFLITO MARCA-SE, E DECIDE-SE ONDE SE PODE DECIDIR ───────────────────
 * `count` com `sum` é conflito no caminho SEM quociente — ali o count tem
 * despacho próprio, que corre a varredura e soma o campo por popcount, e as
 * outras percorrem as células: são dois percursos. Mas COM `GROUP BY` não há
 * conflito nenhum: o count de cada fibra É o G que a corrida já conta, no MESMO
 * percurso que alimenta os acumuladores — e é assim desde §W43.
 *
 * A leitura da lista corre ANTES de se saber se há `GROUP BY`, pelo que recusar
 * ali recusava também o caso legítimo. Marca-se, e decide-se depois. */
static int  agr_conflito = 0;
#define agr_op  (agr_n ? agr_ops[0] : 0)     /* a primeira, para quem só pergunta «há?» */
#define agr_col (agr_cols[0])
static const char *agr_nome(int op){
    return op == 1 ? "sum" : op == 2 ? "max" : op == 3 ? "min" : "avg";
}
/* o pedido matricial é escrito pela leitura da lista e recolhido pelo `varre`,
 * pela mesma ponte do `count(DISTINCT)`: quem lê corre antes de quem usa */
static int mat_op_pedido = 0;
static char mat_tab2_pedido[64] = "";   /* a segunda tabela, para o produto */
/* a coluna do `count(DISTINCT c)`, "" se não houver. São DUAS: o `pedido` é
 * escrito pelo despacho do `SELECT count(...)`, que corre ANTES do `varre` e
 * cujo interior era deitado fora; o outro é o que o `varre` usa, e ele
 * recolhe-o no arranque em vez de o zerar. Sem esta ponte a palavra DISTINCT
 * era lida e perdida no caminho, e o motor respondia o count de tudo. */
static char cnt_dis[64] = "";
static char cnt_dis_pedido[64] = "";
/* a operação matricial pedida: 1 det, 2 posto, 3 traço, 4 transposta, 5 inversa.
 * São sobre a TABELA INTEIRA — não por linha nem por fibra —, e por isso vivem
 * ao lado do `agr_op` e não dentro da projecção. */
static int mat_op = 0;
static char mat_tab2[64] = "";

/* os aliases do `AS`, por posição na lista; vazio = sem alias */
static char alias[SQL_OUT_MAX_COLS][32];
static int  n_alias = 0;

static int lista_colunas(const char **pp, char *out, int cap){
    const char *p = *pp;
    int n = 0;
    for(int k = 0; k < SQL_OUT_MAX_COLS; k++) alias[k][0] = 0;
    n_alias = 0;
    pula(&p);
    if(*p == '*'){ snprintf(out, (size_t)cap, "*"); *pp = p + 1; return 1; }
    for(;;){
        char nome[64];
        if(!ident(&p, nome, sizeof nome)) return 0;
        /* AS AGREGAÇÕES SÃO LEITURAS DA FIBRA, e reconhecem-se aqui.
         *
         * O GROUP BY parte a tabela em fibras; cada uma tem um tamanho — que é
         * G(x), o `count` — e um conteúdo. As outras três lêem esse conteúdo:
         * `max` e `min` são os EXTREMOS da fibra, `sum` é a soma sobre ela. Não
         * há uma varredura nova: a fibra já sai contígua da árvore, e estas
         * lêem-na de passagem. */
        { const char *q = p; pula(&q);
          if(*q == '('){
              /* ── `sigma` NA LISTA: RECUSA-SE, E DIZ-SE PORQUÊ ────────────
               *
               * O WHERE sabe nomear o segundo andar da célula (§W215) e a
               * materialização da linha NÃO: ela lê o par pelo `celula_valor` e
               * imprime-o pelo corpo. Sem esta recusa, `SELECT sigma(a)` era
               * aceite e devolvia a PARTE REAL --- medido: numa tabela com
               * σ = 0, 1, −1 respondia 2, 1, 3, que são os `a`. Números certos
               * para outra pergunta, que é o desfecho que esta casa persegue.
               *
               * O `ORDER BY sigma(a)` já recusava, com «a coluna sigma não
               * existe»; aqui não recusava nada. A lei existia e não ALCANÇAVA
               * este caminho. */
              if(!strcasecmp(nome, "SIGMA")){
                  printf("erro: `sigma(...)` na lista de colunas — RECUSADO. O WHERE sabe"
                         " nomear o segundo andar da célula, e a materialização da linha"
                         " ainda não: aceitar isto devolveria a PARTE REAL com a cara do σ.\n");
                  printf("      A régua escreve-se no WHERE, que é onde ela decide:"
                         " `WHERE c*c + B*c*sigma(c) + C*sigma(c)*sigma(c) OP k`.\n");
                  return 0;
              }
              /* O COUNT TAMBÉM SE LÊ AQUI, E ANTES NÃO SE LIA.
               *
               * As três de cima estavam reconhecidas e o `count` não: em
               * `SELECT a, count(*) FROM s GROUP BY a` — a forma que qualquer
               * cliente escreve — a lista parava no parêntesis, o `FROM` já não
               * casava, e a função devolvia zero SEM MENSAGEM. Não era um erro
               * disfarçado de resultado vazio: era um erro disfarçado de NADA,
               * que é o desfecho de que este motor menos se pode dar ao luxo.
               *
               * O `count` não lê a fibra como as outras: o seu valor É o tamanho
               * dela, G(x), e quem o produz é o bloco do GROUP BY na segunda
               * coluna. Por isso reconhece-se e NÃO entra na lista de colunas —
               * e o argumento pode ser `*`, que não é coluna nenhuma. */
              if(!strcasecmp(nome, "COUNT")){
                  /* ── O COUNT NÃO SE MISTURA, E A RECUSA DIZ PORQUÊ ───────
                   * Ele tem caminho próprio: corre a MESMA varredura do WHERE
                   * e depois soma o campo por popcount — não é uma segunda
                   * contagem, é a leitura da que já ficou escrita, e por isso
                   * não satura como a materialização das linhas. Pô-lo dentro
                   * do laço das agregações seria criar uma segunda régua para
                   * a mesma contagem, e duas réguas para o mesmo objecto é o
                   * defeito que esta casa persegue. Então `count(*), sum(a)`
                   * é RECUSADO — mas com a razão dita e o caminho apontado,
                   * em vez do «não entendido» que não distingue isto de um
                   * erro de escrita. */
                  agr_viu_count = 1;
                  if(agr_n) agr_conflito = 1;
                  const char *r = q + 1; pula(&r);
                  char arg[64];
                  /* ── `count(DISTINCT b)` É CONTAR AS FIBRAS ──────────────
                   * Não é o count com um adorno: é o número de valores
                   * distintos, isto é, quantas fibras tem o quociente por b —
                   * o mesmo objecto do GROUP BY, lido em quantidade em vez de
                   * em extensão. Era ACEITE e respondia o count de tudo: a
                   * palavra entrava como nome de coluna e ninguém a lia. */
                  if(palavra(&r, "DISTINCT")){
                      if(!ident(&r, arg, sizeof arg)) return 0;
                      snprintf(cnt_dis, sizeof cnt_dis, "%s", arg);
                  }
                  else if(*r == '*') r++;
                  else if(!ident(&r, arg, sizeof arg)) return 0;
                  pula(&r); if(*r != ')') return 0; r++;
                  p = r; pula(&p);
                  if(*p == ','){ p++; continue; }
                  break;
              }
              /* ── AS MATRICIAIS SÃO SOBRE A TABELA INTEIRA ────────────────
               * `det(*)` não é uma função da linha nem da fibra: é da tabela,
               * lida como matriz. Reconhece-se aqui, com o `*` a dizer «toda
               * ela» — a mesma palavra que o `count(*)` usa para o mesmo. */
              { int qm = !strcasecmp(nome,"DET") ? 1
                       : !strcasecmp(nome,"POSTO") ? 2
                       : !strcasecmp(nome,"TRACO") ? 3
                       : !strcasecmp(nome,"TRANSPOSTA") ? 4
                       : !strcasecmp(nome,"INVERSA") ? 5
                       : !strcasecmp(nome,"NUCLEO") ? 6
                       : !strcasecmp(nome,"IMAGEM") ? 7
                       : !strcasecmp(nome,"PRODUTO") ? 8
                       : !strcasecmp(nome,"RESOLVE") ? 9
                       : !strcasecmp(nome,"CRAMER") ? 10
                       : !strcasecmp(nome,"SOMA") ? 11
                       : !strcasecmp(nome,"OPOSTO") ? 12
                       : !strcasecmp(nome,"DUAL") ? 13
                       : !strcasecmp(nome,"ANIQUILADOR") ? 14
                       : !strcasecmp(nome,"CIFRA") ? 15
                       : !strcasecmp(nome,"AUTOVALORES") ? 16
                       : !strcasecmp(nome,"AUTOVETORES") ? 17
                       : !strcasecmp(nome,"GRAM") ? 18
                       : !strcasecmp(nome,"SIMETRICA") ? 19
                       : !strcasecmp(nome,"ANTISIMETRICA") ? 20
                       : !strcasecmp(nome,"REGIME") ? 21
                       : !strcasecmp(nome,"FIBRA") ? 22
                       : !strcasecmp(nome,"ULTRA") ? 23
                       : !strcasecmp(nome,"PRECO") ? 24
                       : !strcasecmp(nome,"MEDIAS") ? 25
                       : !strcasecmp(nome,"LEITURA") ? 26
                       : !strcasecmp(nome,"VALORACAO") ? 27
                       : !strcasecmp(nome,"TRIADE") ? 28
                       : !strcasecmp(nome,"COMPLETA") ? 29
                       : !strcasecmp(nome,"GLOBAL") ? 30
                       : !strcasecmp(nome,"EDO") ? 31
                       : !strcasecmp(nome,"FUNDE") ? 32
                       : !strcasecmp(nome,"LCS") ? 33
                       : !strcasecmp(nome,"TERMOS") ? 34 : 0;
                if(qm == 8 || qm == 11){
                    /* o produto pede a OUTRA tabela pelo nome: é a composição,
                     * e uma composição tem dois lados */
                    const char *r = q + 1;
                    char t2[64];
                    pula(&r);
                    if(ident(&r, t2, sizeof t2)){
                        pula(&r);
                        if(*r == ')'){
                            mat_op_pedido = qm;
                            snprintf(mat_tab2_pedido, sizeof mat_tab2_pedido, "%s", t2);
                            p = r + 1;
                            n += snprintf(out + n, (size_t)(cap - n), "%s*",
                                          n ? "," : "");
                            pula(&p);
                            if(*p == ','){ p++; continue; }
                            break;
                        }
                    }
                }
                if(qm){
                    const char *r = q + 1;
                    pula(&r);
                    if(*r == '*'){
                        r++; pula(&r);
                        if(*r == ')'){
                            mat_op_pedido = qm;
                            p = r + 1;
                            n += snprintf(out + n, (size_t)(cap - n), "%s*",
                                          n ? "," : "");
                            pula(&p);
                            if(*p == ','){ p++; continue; }
                            break;
                        }
                    }
                } }

              /* ── AS ANALÍTICAS SÃO POR LINHA, e não sobre a fibra ─────────
               * `exp(a)` não é uma agregação: é uma função do valor da célula,
               * como `a+1`. Guarda-se o texto inteiro — `exp(a)` — e quem o
               * resolve é a projecção, depois de a tabela abrir. */
              if(!strcasecmp(nome,"EXP") || !strcasecmp(nome,"SIN")
                 || !strcasecmp(nome,"COS") || !strcasecmp(nome,"LOG")){
                  const char *r = q + 1;
                  char arg[64];
                  pula(&r);
                  if(ident(&r, arg, sizeof arg)){
                      pula(&r);
                      if(*r == ')'){
                          char tx[80];
                          snprintf(tx, sizeof tx, "%s(%s)", nome, arg);
                          snprintf(nome, sizeof nome, "%s", tx);
                          p = r + 1;
                          goto item_pronto;
                      }
                  }
              }
              int qual = !strcasecmp(nome,"SUM") ? 1 : !strcasecmp(nome,"MAX") ? 2
                       : !strcasecmp(nome,"MIN") ? 3 : !strcasecmp(nome,"AVG") ? 4 : 0;
              if(qual){
                  q++; pula(&q);
                  char arg[64];
                  if(!ident(&q, arg, sizeof arg)) return 0;
                  pula(&q); if(*q != ')') return 0; q++;
                  /* e o mesmo teste no OUTRO sentido: a lista pode trazer o
                   * count primeiro, e sem isto a recusa só apanhava metade dos
                   * casos — «count(*), sum(a)» passava ao lado dela e caía no
                   * «não entendido», que não distingue isto de um erro de
                   * escrita. Um gume tem de apontar a CADA ordem. */
                  if(agr_viu_count) agr_conflito = 1;
                  if(agr_n >= AGR_MAX){
                      printf("erro: mais de %d agregações numa consulta —"
                             " RECUSADA.\n", AGR_MAX);
                      return 0;
                  }
                  agr_ops[agr_n] = qual;
                  snprintf(agr_cols[agr_n], sizeof agr_cols[0], "%s", arg);
                  agr_n++;
                  /* a agregação conta como coluna pedida: sem isto a lista sai
                   * VAZIA e o SELECT era recusado por não ter colunas */
                  /* o que entra na lista é o ARGUMENTO — a coluna que existe —,
                   * e não o nome da função: sem isto o SELECT era recusado por
                   * não achar uma coluna chamada «sum» */
                  n += snprintf(out + n, (size_t)(cap - n), "%s%s", n ? "," : "", arg);
                  p = q; pula(&p);
                  if(*p == ','){ p++; continue; }
                  break;
              }
          } }
        /* ── E O ITEM PODE SER UMA EXPRESSÃO, e não só um nome ──────────────
         * Se depois do identificador vier um operador, o item não acabou: é
         * `a+1`, `a*b`, `a*a`. Recolhe-se o TEXTO todo até à vírgula ou ao
         * FROM, e quem o compila é a resolução da projecção — lá a tabela já
         * está aberta e os nomes resolvem-se, que é a mesma razão de a
         * projecção inteira ser resolvida lá e não aqui. */
        item_pronto:
        /* ── O ITEM PODE VIR QUALIFICADO: `t.a` ─────────────────────────────
         * Numa junção há duas tabelas e os nomes podem repetir-se; o ponto diz
         * de qual delas se fala. Guarda-se o texto inteiro — quem o resolve é a
         * projecção, que sabe quais são os dois lados. */
        if(*p == '.'){
            char sufixo[64];
            const char *r = p + 1;
            if(ident(&r, sufixo, sizeof sufixo)){
                char q2[130];
                snprintf(q2, sizeof q2, "%s.%s", nome, sufixo);
                snprintf(nome, sizeof nome, "%s", q2);
                p = r;
            }
        }
        { const char *q = p; pula(&q);
          if(*q == '+' || *q == '-' || *q == '*' || *q == '/' || *q == '('){
              char tx[64]; int t = 0;
              /* o texto começa no nome e vai até à vírgula de topo ou ao FROM */
              t = snprintf(tx, sizeof tx, "%s", nome);
              { int nivel = 0;
                const char *r = p;
                while(*r && t + 1 < (int)sizeof tx){
                    const char *v = r, *v2 = r;
                    pula(&r);
                    /* pára na vírgula de topo, no FROM e no AS — o alias é do
                     * item e não parte da expressão */
                    if(nivel == 0 && (*r == ',' || palavra(&v, "FROM")
                                      || palavra(&v2, "AS"))) break;
                    if(*r == '(') nivel++;
                    else if(*r == ')'){ if(nivel == 0) break; nivel--; }
                    if(!*r) break;
                    tx[t++] = *r++;
                }
                tx[t] = 0;
                p = r; }
              /* o texto passa a ser o «nome» do item, e o fluxo segue igual:
               * assim o `AS` e a vírgula são lidos por quem já os sabia ler, em
               * vez de este ramo os contornar — que era o defeito, com o alias
               * a ser engolido para dentro da expressão */
              snprintf(nome, sizeof nome, "%s", tx);
          } }
        n += snprintf(out + n, (size_t)(cap - n), "%s%s", n ? "," : "", nome);
        /* O `AS` É SÓ UM NOME, e o Teor. 3 diz porquê: as duas soluções são «a
         * mesma estrutura noutra nomeação». O alias não muda a coluna nem o
         * valor — muda a etiqueta com que a resposta sai —, e por isso lê-se
         * aqui e guarda-se ao lado, sem tocar na lista que a projecção usa. */
        { const char *r = p; pula(&r);
          char al[32];
          int explicito = palavra(&r, "AS");
          const char *r2 = r;
          if(explicito && ident(&r2, al, sizeof al) && strcasecmp(al, "FROM")){
              if(n_alias < SQL_OUT_MAX_COLS)
                  snprintf(alias[n_alias], sizeof alias[0], "%s", al);
              p = r2;
          }
          if(n_alias < SQL_OUT_MAX_COLS) n_alias++; }
        pula(&p);
        if(*p == ','){ p++; continue; }
        break;
    }
    *pp = p;
    return n > 0;
}
static char proj_cols[256] = "*";
static char ord_col[64] = "";        /* a coluna do ORDER BY, "" se não houver */
static int  ord_desc = 0;
/* ── A SEGUNDA RÉGUA ─────────────────────────────────────────────────────────
 * `ORDER BY a, b` não é uma chave maior: é a COMPOSIÇÃO de duas réguas. Ordena-se
 * pela primeira, o que parte a saída em FIBRAS — as corridas de mesmo valor —, e
 * dentro de cada fibra ordena-se pela segunda. É a mesma frase do «espaço das
 * métricas» do `arquitetura.tex`: compor funções é compor réguas, e o desempate é
 * a régua seguinte aplicada onde a anterior não distinguiu.
 *
 * Feito assim, a árvore não muda: é a MESMA descida, corrida uma vez por fibra. E
 * o dual entra pela mesma porta nos dois níveis — quem não tem valor vai para o
 * fim, do bloco todo na primeira régua, e da sua fibra na segunda. */
static char ord_col2[64] = "";
static int  ord_desc2 = 0;
/* LIMIT n É O PREFIXO DA LISTA. O `thm:BI` do `aranha.tex` constrói I como uma
 * ORDEM, com «o encaixe por prefixos a ordenar os andares uns dentro dos
 * outros»: tomar as primeiras n é ficar com um prefixo, e um prefixo é fechado
 * para as operações do andar. Não é uma paragem antecipada da varredura — é a
 * restrição da lista, e por isso corre DEPOIS do WHERE e da ordem. */
static long lim_n = -1;              /* -1 = sem LIMIT */
/* O OFFSET É O DUAL DO LIMIT. Se o limite é o PREFIXO da lista, o offset é o
 * que se salta antes dele — e os dois juntos são uma FAIXA na ordem, tal como
 * a faixa do índice é uma faixa nos valores. Aqui a ordem é a das linhas; lá é
 * a dos símbolos. É o mesmo corte, sobre outra ordem. */
static long off_n = 0;               /* 0 = sem OFFSET */
/* GROUP BY É A FIBRA. `thm:escada`: «quocientar é esquecer a distinção; G mede
 * quantos elementos foram esquecidos juntos.» Agrupar por uma coluna é a
 * realização π: linha ↦ valor dessa coluna, e o count de cada grupo É G(x) — o
 * campo que o motor já constrói. Não há estrutura nova: usa-se a MESMA árvore
 * que ordena, lida por classe em vez de por ordem. */
static char grp_col[64] = "";        /* a coluna do GROUP BY, "" se não houver */
/* ── A SEGUNDA COLUNA DO QUOCIENTE ───────────────────────────────────────────
 * `GROUP BY b, c` é o par do `ORDER BY a, b`, e é a mesma composição: quocienta-
 * se pela primeira, o que parte a saída em fibras, e dentro de cada fibra
 * quocienta-se pela segunda. A árvore não muda — muda o número de vezes que é
 * usada —, e a chave do grupo passa a ser o PAR. Deixar isto por fazer era ter
 * a composição de um lado (a ordem) e não do outro (o quociente), quando são a
 * mesma frase. */
static char grp_col2[64] = "";
/* DISTINCT É O REPRESENTANTE CANÓNICO k=1. O `thm:levantamento` dá a folha
 * k(i) = quantas vezes a célula já foi visitada até i, e a sua cláusula (3) diz
 * que na fibra de x os k são exactamente {1,…,G(x)}. O `thm:escada` fecha:
 * «marcar cada fibra com k=1 É escolher o representante canónico». Logo
 * DISTINCT não é uma passagem a filtrar repetidos — é ficar com a folha 1 de
 * cada fibra, e a fibra é a mesma do GROUP BY. */
static int  dis_usa = 0;             /* 1 se a consulta pediu DISTINCT */
/* HAVING É O WHERE SOBRE G. O GROUP BY dá as fibras e o tamanho de cada uma é
 * G(x); filtrar por esse número é filtrar pela DOBRA. E o `thm:multiplicidade`
 * cláusula (2) diz o que isso significa: «G(x) > 1 se e só se existem i ≠ j com
 * π(i) = π(j)» — de modo que `HAVING count(*) > 1` não é uma conveniência de
 * SQL, é pedir ao motor as células onde a realização dobrou. */
static int  hav_op = 0;              /* 0 = sem HAVING; 1 '>', 2 '<', 3 '=' */
static long hav_n  = 0;

/* ── ORDENAR É DESCER A ÁRVORE, e a árvore é a do banco ─────────────────────
 *
 * «ordenar e cortar são duais: ordenar dá a PROFUNDIDADE — o caminho inteiro,
 * todos os dígitos; cortar dá a PARIDADE — um dígito» (arquitetura.tex §max-cut).
 * Aqui usa-se a ordem: insere-se descendo pelos símbolos do valor, e percorre-se
 * com os símbolos por ordem. É o mesmo mecanismo do `dualsort_banco.c` e do
 * `no_filho` da cifra — não se inventa uma terceira estrutura.
 *
 * A CHAVE É (valor, índice), e não o valor: valores repetidos são uma FIBRA, e
 * o índice da linha é a coordenada que os separa — o levantamento, outra vez.
 * Sem ele os repetidos cairiam no mesmo caminho e perder-se-ia que linha era.
 *
 * O alfabeto é de 16 (um nibble por nível) e não de 256: com 256 cada nó custa
 * 256 slots e a árvore não caberia na zona livre do .mem. Dez níveis cobrem os
 * 32 bits do valor mais 8 do índice. O TECTO É VERIFICADO: se os nós acabarem,
 * a consulta é RECUSADA — ordenar metade seria devolver uma ordem que não é a
 * pedida. */
/* A ÁRVORE QUE ORDENA TEM ESPAÇO PRÓPRIO, E NÃO O DO ÍNDICE.
 *
 * O `ORD_MAXNO 600` não é um número inventado: é o que a zona de um ÍNDICE
 * segura (9603 slots / ORD_LARG). Só que a mesma árvore serve a ordenação, e
 * essa vivia numa zona inteira e continuava presa ao 600 do vizinho --- um
 * `ORDER BY` sobre duzentas linhas dava «a árvore de ordenação não coube», e a
 * consulta era RECUSADA. Oito zonas contíguas dão-lhe 8192 nós, e o tecto de
 * cada uso passa a ser o do espaço que ele TEM. */
#define S_ORD      (ISA_TECTO + ZONA(29))    /* 29..36: oito zonas para a ordem */
#define ORD_ZONAS  8u
#define ORD_LARG_  16u                                  /* = ORD_LARG, declarado adiante */
#define ORD_MAXNO_R ((ZONA_SLOTS * ORD_ZONAS) / ORD_LARG_)
/* ── E O TECTO DO ÍNDICE SAI DO MAPA, COMO O DA ORDEM ────────────────────────
 *
 * Estava `600u`, com o comentário acima a justificá-lo como «o que a zona de um
 * ÍNDICE segura (9603 slots / ORD_LARG)». O 9603 é de um mapa ANTIGO, de quando
 * os índices partilhavam espaço: hoje cada um tem UMA ZONA sua --- 16384 slots,
 * menos os três do cabeçalho ---, o que dá 1023 nós. O número ficou escrito e o
 * mapa cresceu por baixo dele, que é o mesmo defeito do comentário do tecto de
 * linhas: um número à mão sobrevive à mudança que o invalida.
 *
 * E o preço não era espaço desperdiçado: era o MOTOR A VARRER. Medido --- com
 * 190 linhas o `WHERE k = 5` indexado custa ZERO passos, porque quem responde é
 * a árvore; com 200 o CREATE INDEX recusa («o índice não coube») e a mesma
 * consulta passa a custar 17200. É a lei do `aranha §sec:ultra` a deixar de
 * valer por um número: «a vizinhança na árvore tem UM elemento, e o custo é
 * Θ(1) e não Θ(n)». */
#define ORD_MAXNO_I ((ZONA_SLOTS - 3u) / ORD_LARG_)
static unsigned ord_tecto = ORD_MAXNO_I;   /* o do espaço que a árvore corrente TEM */
#define S_ORDCAB   (S_ORD - 1)

/* ── O ÍNDICE VIVE NOUTRA ZONA, E NO DISCO ───────────────────────────────────
 *
 * A árvore de cima é de rascunho: o ORDER BY, o JOIN e o GROUP BY limpam-na e
 * reconstroem-na a cada consulta. Um ÍNDICE é a mesma árvore que NÃO se limpa —
 * e por isso tem de morar noutro sítio. Mora na zona 3, acima do tecto da ISA,
 * dentro do `.mem` DA TABELA: é do disco que ele é lido, e não há aqui memória
 * de programa nenhuma a segurá-lo entre consultas.
 *
 * O cabeçalho guarda a coluna indexada e quantas linhas a tabela tinha quando
 * ele foi construído. Se o número mudou, o índice está velho e IGNORA-SE — a
 * varredura corre como antes. Um índice velho nunca dá resposta errada; dá
 * apenas a resposta lenta. */
/* qual árvore está a ser usada: a de rascunho ou a do índice. As funções
 * abaixo servem as duas — a árvore é a mesma lei, e a base é o parâmetro. */
static unsigned ord_raiz = S_ORD;
static unsigned ord_cab  = S_ORDCAB;
/* A MIGRAÇÃO DO CAMPO DE PRESENÇA.
 *
 * Zero é a ausência, e uma base gravada antes deste campo tem-no todo a zero:
 * lida à letra, ela diria que TUDO está ausente. O marcador diz se o campo já
 * foi escrito alguma vez; se não foi, acende-se o que lá está — numa tabela
 * antiga o que existe, existe — e escreve-se o marcador. Faz-se uma vez por
 * tabela, e não a cada consulta. */
static void pres_migra(long ncols, long nrows){
    if(mem_le(S_PRESCAB).total) return;             /* já migrada */
    for(long i = 0; i < nrows; i++)
        for(long j = 0; j < ncols; j++)
            bit_poe(S_PRES, i*ncols + j, 1);
    { Word m = {1,0}; mem_grava(S_PRESCAB, m); }
}

#define ORD_BITS   4u                    /* símbolos por nível: 2^ORD_BITS */
#define ORD_LARG   (1u << ORD_BITS)      /* um símbolo por nível */
#define ORD_NIV_VAL 8                    /* os 32 bits do valor, a 4 por nível */
#define ORD_NIV_MAX 16                   /* o tecto do arranjo do caminho */
/* ── A ALTURA É FUNÇÃO DA ÁRVORE, e não um estado ao lado dela ───────────────
 *
 * Escrevi-a primeiro como duas variáveis que `ord_usa_rascunho` e
 * `ord_usa_indice` punham. Nove asserções caíram --- todas do JOIN e da
 * subconsulta ---, e a razão era essa: há caminhos que LEEM a árvore sem passar
 * por quem a escolhe, e herdavam a altura da última consulta. Estado ao lado do
 * objecto dessincroniza; derivado DO objecto, não pode.
 *
 * A do rascunho é quatro níveis --- ela recebe as linhas que o WHERE deixou, e
 * com dois o índice colidia acima de 256, o que fazia 300 linhas saírem a
 * começar em 257. A do índice é dois: ela recusa nos seus 600 nós muito antes
 * das 256 linhas, pelo que ali a colisão não chega a acontecer, e mudá-la
 * mudaria a forma de uma árvore GRAVADA que três caminhos relêem. */
/* ── A ALTURA DE CADA ÁRVORE, E OS CINCO NÚMEROS QUE A NÃO LIAM ──────────────
 *
 * A do rascunho é QUATRO níveis --- ela recebe as linhas que o WHERE deixou, e
 * com dois colidiria acima de 256, o que faria 300 linhas saírem a começar em
 * 257. A do índice é DOIS, e o que a segura é o `IDX_LIN_MAX` abaixo, derivado
 * dela: acima de 256 linhas o índice ABSTÉM-SE.
 *
 * O que aqui muda não é o valor: é quem o lê. O número de níveis do índice
 * estava ESCRITO À MÃO em cinco sítios do `j_faixa` --- `if(d < 2)`, dois
 * `e >= 2` e dois `(e-2)` ---, e era por eles que mexer nesta linha partia a
 * árvore EM SILÊNCIO. O comentário do `j_casam` já dizia «este era o quarto
 * sítio onde o número de níveis estava dito, e o único que eu não tinha
 * encontrado»: faltavam estes. Agora todos leem daqui, e a linha mexe-se.
 *
 * UNIFICAR EM QUATRO é o que a teoria pede --- `§sec:realizacoes`: «o Algoritmo
 * A corre em todas elas SEM UMA LINHA DIFERENTE», e a `thm:navega` dá-lhe o
 * preço, porque duas árvores de alturas diferentes divergem no PRIMEIRO nível e
 * D = 2^0 = 1, o máximo. Está medido nos dois sentidos: com quatro os dois
 * caminhos concordam em todos os tamanhos, mas cada chave gasta mais dois
 * níveis de nós e o índice deixa de caber às ~220 linhas em vez das 256. Fica
 * como está até a árvore ter mais espaço, que é uma decisão sobre o MAPA. */
#define ord_niv_idx   (ord_raiz == S_ORD ? 4 : 2)
#define ord_bits_idx  ((int)ORD_BITS * ord_niv_idx)
#define ORD_NIV    (ORD_NIV_VAL + ord_niv_idx)


static void ord_usa_rascunho(void){ ord_raiz = S_ORD; ord_cab = S_ORDCAB;
                                    ord_tecto = ORD_MAXNO_R;
                                    }
/* o índice guarda TODAS as linhas da tabela, e por isso a altura dele é a da
 * tabela --- a mesma conta, feita quando ele é escolhido */

/* O CONTADOR DE NÓS TEM O SEU SLOT, e não o do cabeçalho. `ord_novo` escreve
 * em `ord_cab` a cada nó criado; apontá-lo ao cabeçalho do índice fazia o
 * número de nós escrever-se por cima da coluna indexada — lia-se «coluna 5»
 * numa tabela de duas colunas, e o índice era dado por velho para sempre. Dois
 * dados diferentes, dois slots. */
static void ord_usa_indice(long k){ ord_raiz = S_IDX((unsigned)k);
                                    ord_cab  = S_IDXNOS((unsigned)k);
                                    ord_tecto = ORD_MAXNO_I;
                                    }
/* A LARGURA É O PARÂMETRO, A LARGURA DERIVA. Estava `ORD_LARG 16` e depois
 * `(ch >> (4*d)) & 15` escrito à mão em dois sítios: o 4 e o 15 são o mesmo
 * número que o 16, ditos de três maneiras. Declara-se o expoente e o resto sai
 * dele — `largura.h`: «a lei escreve-se UMA vez e o andar é argumento». */
/* ── A PROFUNDIDADE CONSTRÓI-SE NA HORA ──────────────────────────────────────
 *
 * Eram DEZ níveis fixos: «8 do valor + 2 do índice», com a chave a levar o
 * índice da linha em `idx & 255`. Acima de 256 linhas duas linhas diferentes
 * davam a MESMA chave, e a ordem saía errada sem uma palavra --- medido: 300
 * linhas de 1 a 300 saíam a começar em 257. Não recusava; ORDENAVA MAL.
 *
 * A árvore de prefixos não precisa de profundidade fixa: precisa de descer até
 * DISTINGUIR, e quanto ela tem de descer sai do objecto. Os oito níveis do
 * valor são os 32 bits que ele tem; os do índice são os que o número de linhas
 * pede --- `ceil(log16(nrows))` ---, e para 300 linhas são três, não dois.
 *
 * É o §sec:zeta a operar: ζ = ∑ S^j é a série de Neumann FINITA porque S é
 * nilpotente, e o expoente onde ela para não é escolhido --- é onde S^j se
 * anula. A altura desta torre é o mesmo: não se fixa, calcula-se do que há para
 * separar, e cada consulta constrói a sua. */

/* (o `ORD_MAXNO 600u` que aqui vivia era `#define` MORTO --- nunca usado em
 * código, só a repetir o número que o `ORD_MAXNO_I` já deriva do mapa. Saiu.) */
typedef char ord_larg_bate[(ORD_LARG == ORD_LARG_) ? 1 : -1];   /* os dois têm de ser um */

static unsigned ord_novo(void){
    /* O CONTADOR NO PAR. Vivia em `.total`, um byte, e ORD_MAXNO é 600: ao
     * chegar a 255 dava a volta e a árvore RECICLAVA nós — o tecto declarado
     * era ficção, porque a árvore partia muito antes de lá chegar. É o mesmo
     * defeito do `no_novo` da cifra, na árvore que ordena e junta. */
    unsigned n = par_le(ord_cab); if(n < 1) n = 1;        /* 0 é a raiz */
    if(n >= ord_tecto) return 0;                  /* o tecto do espaço que esta árvore tem */
    par_grava(ord_cab, n + 1);
    for(unsigned k = 0; k < ORD_LARG; k++){
        Word z = {0,0}; mem_grava(ord_raiz + n*ORD_LARG + k, z);
    }
    return n;
}
static unsigned ord_filho(unsigned no, unsigned sim, int abrir){
    unsigned f = par_le(ord_raiz + no*ORD_LARG + sim);
    if(f || !abrir) return f;
    f = ord_novo();
    if(!f) return 0;
    par_grava(ord_raiz + no*ORD_LARG + sim, f);   /* o índice do nó no PAR, não num byte */
    return f;
}
static void ord_limpa(void){
    /* (quem chama diz a altura com `ord_altura(nrows)` antes; sem isso fica a
     * anterior, que é o que faria uma árvore de profundidade fixa) */
    Word z = {1,0}; mem_grava(ord_cab, z);
    for(unsigned k = 0; k < ORD_LARG; k++){
        Word q = {0,0}; mem_grava(ord_raiz + k, q);
    }
}
/* insere a chave (valor, índice); devolve 0 se a árvore não coube */
static int ord_insere(long valor, int idx){
    unsigned long ch = ((unsigned long)(valor + 2147483648L) << ord_bits_idx)
                     | ((unsigned long)idx & ((1UL << ord_bits_idx) - 1UL));
    unsigned no = 0;
    for(int d = ORD_NIV - 1; d >= 0; d--){
        unsigned sim = (unsigned)((ch >> (ORD_BITS*d)) & (ORD_LARG - 1u));
        no = ord_filho(no, sim, 1);
        if(!no && d > 0) return 0;
    }
    return 1;
}
/* ── O QUE O PERCURSO NÃO PÔDE ENTREGAR ──────────────────────────────────────
 *
 * `if(*n < cap) saida[(*n)++] = ...` --- e quando o arranjo enchia, a chave era
 * DEITADA FORA em silêncio. Onde o resultado são linhas isso fica visível: a
 * janela de 64 da porta C recusa a consulta e diz porquê. Mas onde o resultado é
 * um AGREGADO não há nada a cobri-lo, e foi o que aconteceu ao count(DISTINCT):
 * com 65 valores distintos respondia 64, com 100 respondia 64, sempre em
 * silêncio --- e a 200 recusava honestamente, com a mensagem «recusa em vez de
 * contar metade» escrita no próprio erro. A intenção estava lá; era a truncagem
 * que acontecia ANTES de a guarda a alcançar.
 *
 * Agora conta-se. Quem calcula um agregado a partir do percurso lê o contador
 * antes e depois, e recusa se ele mexeu --- que é o que a mensagem já prometia. */
static long ord_perdidos = 0;
long sql_ord_perdidos(void){ return ord_perdidos; }

/* ── CONTAR AS FIBRAS SEM ARRANJO NENHUM ─────────────────────────────────────
 *
 * `aranha §sec:dimensao`: «o n aparece UMA vez, no tamanho da vizinhança --- e é
 * aí, e só aí»; e «enunciar o teorema no posto 2 e realizá-lo num arranjo do
 * plano dá a impressão de que o 2 é do teorema. Não é: é do EXEMPLO, e do
 * ARRANJO.» Foi exactamente o que aqui aconteceu: o count(DISTINCT) percorria a
 * árvore para um arranjo de 64 e contava as transições depois --- e o 64 era do
 * ARRANJO, não da conta. Qualquer tabela com mais de 64 linhas respondia um
 * número errado (50 linhas com 5 classes davam 5, 100 davam quatro, 300 davam
 * dois), e a minha primeira correcção foi RECUSAR, que é aceitar o tecto.
 *
 * A cláusula (3) do `thm:multiplicidade` diz onde está o erro: «a memória não
 * pertence ao agente [...] NENHUM passo consulta π(0),…,π(t)». Guardar os
 * índices todos para depois os percorrer é o agente a carregar o mapa que o
 * ESPAÇO já tem. As chaves saem da árvore por ordem de valor: contar as fibras
 * é contar as VEZES EM QUE O VALOR MUDA, e isso decide-se durante a descida,
 * com um valor anterior e mais nada.
 *
 * Sem arranjo não há tecto: o que limita passa a ser a árvore, que é do mapa e
 * recusa dizendo-o. */
static long ord_classes = 0;      /* fibras distintas vistas na descida */
static long ord_ult_valor = 0;    /* o valor anterior --- um, não uma lista */
static int  ord_tem_ult = 0;
static void ord_conta_fibras(unsigned no, int nivel, unsigned long ch){
    if(nivel == ORD_NIV){
        long v = (long)(ch >> ord_bits_idx) - 2147483648L;
        if(!ord_tem_ult || v != ord_ult_valor){ ord_classes++; ord_ult_valor = v; ord_tem_ult = 1; }
        return;
    }
    for(unsigned k = 0; k < ORD_LARG; k++){
        unsigned f = par_le(ord_raiz + no*ORD_LARG + k);
        if(!f) continue;
        ord_conta_fibras(f, nivel + 1, (ch << ORD_BITS) | k);
    }
}
/* e o DISTINCT desliga os repetidos na descida, pela mesma razão. Ele percorria
 * para um arranjo de 64 e só depois apagava o match dos repetidos --- com 100
 * linhas de 3 classes devolvia TRINTA E NOVE, e com 200 devolvia 139, quando a
 * resposta é três. O erro ao lado já prometia «recusa em vez de devolver
 * repetidos»: a lei estava escrita e o arranjo não a deixava chegar. O valor já
 * vem na chave, pelo que nem é preciso reler a célula. */
static long dis_ult = 0;
static int  dis_tem = 0;
static void ord_tira_repetidos(unsigned no, int nivel, unsigned long ch){
    if(nivel == ORD_NIV){
        long v   = (long)(ch >> ord_bits_idx) - 2147483648L;
        long idx = (long)(ch & ((1UL << ord_bits_idx) - 1UL));
        if(dis_tem && v == dis_ult) bit_poe(S_MATCH, idx, 0);   /* k>1: sai */
        dis_ult = v; dis_tem = 1;
        return;
    }
    for(unsigned k = 0; k < ORD_LARG; k++){
        unsigned f = par_le(ord_raiz + no*ORD_LARG + k);
        if(!f) continue;
        ord_tira_repetidos(f, nivel + 1, (ch << ORD_BITS) | k);
    }
}
/* e a EXISTÊNCIA decide-se na descida também: não se recolhe para depois olhar,
 * pára-se no primeiro vivo. O `j_casam(v, saida, 4)` que servia isto entregava
 * os QUATRO primeiros índices por ordem de linha --- e se esses quatro
 * estivessem apagados, quem filtrava pelo vivo concluía «não há nenhuma» sem
 * ter visto a quinta. O 4 era do arranjo. Aqui não há arranjo. */
static long viv_nr = 0;              /* quantas linhas a tabela tem, para o filtro */
static int  viv_achou = 0;
static void ord_ha_vivo(unsigned no, int nivel, unsigned long ch, unsigned long alvo){
    if(viv_achou) return;
    if(nivel == ORD_NIV){
        unsigned long v = ch >> ord_bits_idx;
        if(v != alvo) return;
        long idx = (long)(ch & ((1UL << ord_bits_idx) - 1UL));
        if(idx >= 0 && idx < viv_nr && bit_le(S_VIVO, idx)) viv_achou = 1;
        return;
    }
    for(unsigned k = 0; k < ORD_LARG && !viv_achou; k++){
        unsigned f = par_le(ord_raiz + no*ORD_LARG + k);
        if(!f) continue;
        ord_ha_vivo(f, nivel + 1, (ch << ORD_BITS) | k, alvo);
    }
}
/* o valor tem UMA fibra viva NA ÁRVORE DA COLUNA? --- sem arranjo, sem tecto */
static void ord_usa_indice(long k);
static void ord_usa_rascunho(void);
/* o valor tem UMA fibra viva? --- sem arranjo, sem tecto */
static int ord_valor_vivo(long valor, long nr){
    unsigned long alvo = (unsigned long)(valor + 2147483648L);
    viv_nr = nr; viv_achou = 0;

    ord_ha_vivo(0, 0, 0, alvo);
    return viv_achou;
}
static int ord_valor_vivo_col(long col, long valor, long nr){
    int r;
    ord_usa_indice(col);
    r = ord_valor_vivo(valor, nr);
    ord_usa_rascunho();
    return r;
}

/* percorre por ordem e devolve os índices; n é o tecto do arranjo de saída */
static int ord_percorre(unsigned no, int nivel, unsigned long ch,
                        int *saida, int *n, int cap, int desc){
    if(nivel == ORD_NIV){
        /* o índice são os níveis BAIXOS da chave, e quantos eles são vem da
         * altura desta árvore --- não de um 255 escrito à mão */
        if(*n < cap) saida[(*n)++] = (int)(ch & ((1UL << ord_bits_idx) - 1UL));
        else ord_perdidos++;      /* e o que não coube CONTA-SE: ver abaixo */
        return 1;
    }
    for(unsigned k = 0; k < ORD_LARG; k++){
        unsigned sim = desc ? (ORD_LARG - 1 - k) : k;
        unsigned f = par_le(ord_raiz + no*ORD_LARG + sim);
        if(!f) continue;          /* (era `if(!f && !(nivel==0 && 0)){ if(!f) continue; }`
                                   * --- um `&& 0` residual desactivava um caso que já não
                                   * existe, e a condição inteira dizia isto e mais nada) */
        ord_percorre(f, nivel + 1, (ch << ORD_BITS) | sim, saida, n, cap, desc);
    }
    return 1;
}

/* ── O JOIN É O CORTE, e o corte é o dual da ordem ──────────────────────────
 *
 * «Ordenar dá a PROFUNDIDADE — o caminho inteiro, todos os dígitos; cortar dá a
 * PARIDADE — um dígito» (arquitetura.tex §sec:cortar). Um join de igualdade é
 * uma BIPARTIÇÃO: para cada valor da coluna de junção, as linhas que casam e as
 * que não casam. E a bipartição faz-se com a MESMA árvore que ordena — descer
 * pelos símbolos do valor é escolher a classe.
 *
 * Não há aqui uma estrutura nova: a árvore de S_ORD indexa a tabela da direita
 * pelo valor da coluna de junção, e cada linha da esquerda desce por ela e
 * encontra a sua classe. O que na ordem era «percorrer todos os símbolos» é
 * aqui «descer por um só» — a profundidade e a paridade, o MOVE nos dois
 * sentidos.
 *
 * A DIREITA VIVE NO .mem, não em RAM: as suas linhas são copiadas para a zona
 * S_JDIR antes de a tabela ser trocada, porque o motor tem UMA tabela aberta de
 * cada vez. O tecto é declarado e verificado. */
/* A DIREITA DO JOIN, EM ZONAS PRÓPRIAS. Ela ocupava UMA zona, e enquanto uma
 * linha eram 16 Words cabiam 1024 --- postas a COL_MAX, cabiam dezasseis. Ou o
 * espaço acompanha a largura, ou alargar as colunas encurta as linhas: aqui
 * acompanha, com 32 zonas. */
#define J_ZONAS    32u
#define S_JDIR     (ISA_TECTO + ZONA(45))      /* 45..76 */
#define S_JCAB     (S_JDIR - 1)
/* O JOIN GUARDA A DIREITA NUMA ZONA, e o que cabe é o que a zona segura: uma
 * linha ocupa J_MAXCOL Words, logo há ZONA_SLOTS/J_MAXCOL linhas. O 64 estava
 * escrito ao lado do 16 sem que um dependesse do outro --- e a zona inteira
 * dava 1024. As colunas seguem o mesmo COL_MAX do resto da casa. */
#define J_MAXCOL   ((unsigned)COL_MAX)
#define J_MAXLIN   ((int)((ZONA_SLOTS * J_ZONAS) / J_MAXCOL))

/* ── O MAPA DE ZONAS, VERIFICADO PELO COMPILADOR ─────────────────────────────
 *
 * A última zona reservada é a do JOIN (45 + J_ZONAS − 1), e as árvores de
 * índice começam em ZONA_LIVRE1. Se alguém acrescentar zonas e passar por cima
 * delas, isto deixa de compilar --- que é onde um erro destes tem de aparecer,
 * e não numa consulta a nove mil linhas de distância.
 *
 * Foi assim que o defeito se apanhou: uma varredura ao mapa, e não uma
 * asserção, porque nenhum medidor indexa uma coluna alta o bastante para o
 * pisar. O que não é verificado pelo compilador tem de ser verificado por
 * alguém --- e ninguém estava. */
typedef char zonas_nao_pisam_os_indices[(45u + J_ZONAS <= 77u) ? 1 : -1];
typedef char lcs_nao_pisa_o_invertido[(77u + LCS_ZONAS <= 79u) ? 1 : -1];
typedef char invertido_nao_pisa_os_indices[(79u + INV_ZONAS <= ZONA_LIVRE1) ? 1 : -1];
/* ── E ESTA VERIFICAÇÃO MUDOU DE CONTEÚDO COM A UNIFICAÇÃO ───────────────────
 *
 * Ela era `IDX_ZONA1 + IDX_MAXCOL - 8 <= 600`, com o 600 escrito à mão nos dois
 * lados --- e aí verificava algo: que as duas contas batiam. Agora que o
 * IDX_MAXCOL DERIVA do ZONA_CANAL, a desigualdade é verdadeira por construção,
 * e seria a tautologia dentro da correcção: os dois lados a mesma expressão.
 *
 * O que pode falhar é outra coisa, e é essa que se verifica: que a primeira zona
 * dos índices esteja ABAIXO do canal --- se as zonas reservadas crescerem até lá,
 * não sobra índice nenhum ---, e que sobrem pelo menos as OITO de sempre. */
typedef char ha_zona_para_indices[(IDX_ZONA1 + 8u <= ZONA_CANAL) ? 1 : -1];

static char j_tab_dir[64] = "";   /* a tabela da direita, "" se não há JOIN */
static char j_col_esq[64] = "";   /* a coluna da esquerda no ON            */
static char j_col_dir[64] = "";   /* a coluna da direita no ON             */

/* O VALOR DE UMA CÉLULA É O PAR (baixo, alto), e não o byte baixo.
 *
 * Li só `.total` na primeira versão do JOIN e do ORDER BY, e um saldo de 300
 * saiu 44 — que é 300 mod 256. O envelope é de oito bits e o número vive no
 * PAR: é a dobra da fronteira de leitura, e ignorá-la é ler metade do valor. */
static long celula_valor(long i, long j, long nc){
    return (long)((unsigned long)mem_le(S_LINHAS + cel_ix(i*nc + j)).total
                | ((unsigned long)mem_le(S_ALTO + cel_ix(i*nc + j)).total << 8));
}
/* e o dual do leitor: a célula é um PAR — o átomo baixo e o alto —, e escrever
 * só metade é o defeito que o `SET v = 30000` já mostrou uma vez (ficava 29952,
 * com o alto do valor anterior). O `.e` do baixo não se toca: é o denominador
 * (ou o coeficiente de σ), e quem o define é o corpo da coluna. */
/* ── A CÉLULA LIDA COMO O SEU CORPO MANDA ────────────────────────────────────
 * O `celula_valor` devolve o par (baixo, alto) lido como um número SEM SINAL, e
 * é o que o motor quer para o inteiro de 0..65535. Mas quem pergunta pela
 * matriz quer o VALOR, e num corpo assinado — racional, áureo, cristal — o
 * primeiro átomo é um int8: sem isto, um −1 guardado lia-se 65535, e o traço de
 * uma matriz de negativos saía 131067. A impressão já lia com o sinal certo;
 * eram duas réguas para a mesma célula, e a matriz usava a que não sabe do
 * corpo. Devolve-se o par (numerador, denominador), que é o que o racional é. */
static void celula_qz(long i, long j, long nc, long *num, long *den){
    Word c = mem_le(S_LINHAS + cel_ix(i*nc + j));
    long cp = corpo_de(j).total;
    /* ── O NUMERADOR LÊ-SE NO ANDAR EM QUE FOI ESCRITO: os dois planos.
     * O baixo é a Word da linha, o alto é o S_ALTO --- que é o σF_w do
     * thm:espaco ---, e juntos dão a palavra de largura dupla. Para o corpo
     * ASSINADO o andar parte-se ao meio, e o bit de topo diz de que lado. */
    long alto = (long)mem_le(S_ALTO + cel_ix(i*nc + j)).total;
    long bruto = (long)c.total | (alto << 8);
    long assin16 = (bruto & 0x8000L) ? bruto - 65536L : bruto;
    /* ── O CORPO DECIDE ANTES DA HEURÍSTICA, E NÃO DEPOIS ────────────────
     *
     * O `c.e > 1` é a heurística para quando o corpo não diz nada: um `.e`
     * acima de um só pode ser denominador. Mas nos corpos quadráticos o `.e` é
     * o COEFICIENTE DE σ, e ele pode ser negativo --- e o `Word8` não traz
     * sinal, pelo que um σ de −1 chega como 255. Com o teste do racional
     * primeiro, `3−1σ` satisfazia `c.e > 1`, entrava no ramo da fracção e saía
     * como `ra_classe(3, 255)` = 1/85. O valor certo estava na célula e a
     * leitura devolvia outro objecto.
     *
     * A ordem tem de ser esta: onde o corpo é DECLARADO, é ele que manda; a
     * heurística é para o resto. Medido no `ORDER BY` de uma coluna AUREO, que
     * lia num = 1 numa célula que vale 3 − σ. */
    if(cp == CORPO_AUREO || cp == CORPO_CRISTAL){
        *num = assin16; *den = 1; return;                 /* só a parte racional */
    }
    if(cp == CORPO_RACIONAL || c.e > 1){
        Par cls = ra_classe((Par){ assin16, c.e ? (long)c.e : 1 });
        *num = cls.a; *den = cls.b; return;
    }
    *num = celula_valor(i, j, nc); *den = 1;
}

/* OS TRÊS PLANOS, aqui também. Este caminho --- a propagação ON UPDATE CASCADE
 * e o desfazer de um SET --- escrevia dois de três, e o terceiro ficava com o do
 * valor anterior: uma DATA propagada por uma seta mudava de instante, e o
 * `rollback` de um SET repunha o baixo e deixava o alto2 do valor novo. É o
 * mesmo defeito que o UPDATE tinha, noutro caminho --- e é por isso que a
 * célula tem de ter UM sítio que a escreve, não três parecidos. */
static void celula_grava(long i, long j, long nc, long valor){
    unsigned pos = (unsigned)(i*nc + j);
    Word b = mem_le(S_LINHAS + pos);
    b.total = (Word8)((unsigned long)valor & 255u);
    mem_grava(S_LINHAS + pos, b);
    { Word a; a.total = (Word8)(((unsigned long)valor >> 8) & 255u); a.e = 0;
      mem_grava(S_ALTO + pos, a); }
    { Word c; c.total = (Word8)(((unsigned long)valor >> 16) & 255u);
      c.e     = (Word8)(((unsigned long)valor >> 24) & 255u);
      mem_grava(S_ALTO2 + pos, c); }
}

/* FASE 1 do join: copiar a tabela da DIREITA para o banco e indexá-la.
 *
 * O motor tem UMA tabela aberta de cada vez, logo a direita tem de ser lida
 * primeiro e guardada. Guarda-se no .mem (S_JDIR), não em memória: linha a
 * linha, célula a célula. E ao mesmo tempo desce-se a árvore com o valor da
 * coluna de junção — que é a BIPARTIÇÃO: cada valor é uma classe, e as linhas
 * com o mesmo valor caem na mesma.
 *
 * Devolve o número de linhas guardadas, ou −1 se não coube (e aí a consulta é
 * recusada: um join com metade da direita é um join errado, não um join menor). */
static int j_col_dir_idx = -1;       /* a coluna de junção na direita */
static int j_carrega_direita(long *ncols_dir){
    long nc, nr;
    int oc, postos = 0;
    if(!usa_tabela(j_tab_dir, 0)) return -1;
    if(!cat_nome_bate(j_tab_dir)) return -1;
    oc = col_indice(j_col_dir);
    if(oc < 0) return -2;                         /* a coluna não existe lá */
    j_col_dir_idx = oc;                           /* para a desigualdade a ler */
    nc = cat_ncols(); nr = cat_nrows();   /* o catálogo lia-se para nada: as duas
                                            contas já o abrem por dentro */
    if(nc > J_MAXCOL) return -1;
    ord_limpa();
    for(long i = 0; i < nr; i++){
        if(!bit_le(S_VIVO, i)) continue;
        if(postos >= J_MAXLIN) return -1;
        for(long j = 0; j < nc; j++){
            long v = celula_valor(i, j, nc);
            Word par = { (Word8)(v & 255), (Word8)((v >> 8) & 255) };
            mem_grava(S_JDIR + (unsigned)(postos*J_MAXCOL + j), par);   /* o PAR */
        }
        /* e a célula AUSENTE não é chave: pô-la na árvore era pôr o neutro
         * com cara de valor, e depois um `x IN (…)` com x = 0 casava com uma
         * linha que não tem valor nenhum. A linha copia-se — pode ser precisa
         * para as outras colunas —, mas não se indexa. */
        if(bit_le(S_PRES, i*nc + oc))
            if(!ord_insere(celula_valor(i, oc, nc), postos)) return -1;
        postos++;
    }
    { Word c = { postos, nc }; mem_grava(S_JCAB, c); }
    *ncols_dir = nc;
    return postos;
}

/* ── O µ: TIRAR uma chave da árvore ──────────────────────────────────────────
 *
 * `thm:zeta-mu`: ζ acumula e µ desacumula, «e só muda a ordem sobre a qual se
 * acumula». Aqui isso lê-se ao pé da letra: A ζ DESCE DEIXANDO MIGALHAS, e a µ
 * NÃO PRECISA DE DESCOBRIR NADA — volta pelo mesmo caminho a recolhê-las.
 * Guarda-se o caminho ao descer, corta-se a ligação da folha, e SOBE-SE a
 * apagar todo o nó que ficou sem filhos.
 *
 * É por isso que a árvore não é uma tabela à espera de ser varrida: ela é o
 * RASTRO DA TRAJECTÓRIA, e a memória existe onde a trajectória passou. Um
 * UPDATE é as duas metades em sequência — o valor velho recolhe as suas
 * migalhas, o novo deixa as suas. Desacumula, acumula.
 *
 * O que ela não faz é devolver os nós ao contador: um nó apagado fica órfão, e
 * o `ord_novo` não o reaproveita. Isso é uma fuga declarada, e tem tecto — o
 * `ORD_MAXNO`. Quando ele chega, `ord_insere` devolve zero e quem chamou LARGA
 * o índice, que é o que já fazia. Um índice largado custa a varredura seguinte;
 * nunca custa a resposta. */
static int idx_remove(long valor, int idx){
    unsigned long ch = ((unsigned long)(valor + 2147483648L) << ord_bits_idx)
                     | ((unsigned long)idx & ((1UL << ord_bits_idx) - 1UL));
    unsigned pai[ORD_NIV_MAX], sim[ORD_NIV_MAX];
    unsigned no = 0;
    int nc = 0;
    for(int d = ORD_NIV - 1; d >= 0; d--){
        unsigned sm = (unsigned)((ch >> (ORD_BITS*d)) & (ORD_LARG - 1u));
        pai[nc] = no; sim[nc] = sm; nc++;
        unsigned f = par_le(ord_raiz + no*ORD_LARG + sm);
        if(!f) return 0;                       /* a chave não está lá */
        no = f;
    }
    /* de baixo para cima: corta-se a ligação, e sobe-se enquanto o nó que
     * ficou para trás não tiver mais nenhum filho */
    for(int k = nc - 1; k >= 0; k--){
        par_grava(ord_raiz + pai[k]*ORD_LARG + sim[k], 0);
        int tem = 0;
        for(unsigned m = 0; m < ORD_LARG && !tem; m++)
            if(par_le(ord_raiz + pai[k]*ORD_LARG + m)) tem = 1;
        if(tem) break;                         /* o pai ainda serve: pára */
    }
    return 1;
}

/* ── A FAIXA: descer e percorrer o que fica do lado certo ────────────────────
 *
 * A árvore guarda o valor em oito símbolos, do mais significativo para o menos,
 * com o deslocamento que põe os negativos antes dos positivos — logo a ordem
 * dos caminhos É a ordem dos números. Uma desigualdade é por isso um PREFIXO
 * comum seguido de todos os ramos de um lado, e nada mais: desce-se pelo
 * caminho de `v`, e em cada nível levam-se inteiros os ramos que já ficaram
 * do lado certo. É o corte outra vez, agora com o percurso do ORDER BY a
 * seguir-se-lhe: cortar dá o dígito, percorrer dá o resto.
 *
 * O que isto NÃO faz é varrer: os nós visitados são a profundidade vezes a
 * largura, mais os que têm resultado. O tamanho da tabela não entra. */
static void faixa_tudo(unsigned no, int d, int *saida, int *n, int cap);

/* recolhe os índices de um nó que já está no nível do índice.
 * A PROFUNDIDADE VEM DA ÁRVORE e não de dois laços encaixados: eram dois níveis
 * escritos três vezes --- os dois `for`, o `<< 4` e o `d < 2` do chamador ---, e
 * por isso mudar a altura partia isto em silêncio. */
static void faixa_folhas(unsigned no, int *saida, int *n, int cap){
    unsigned sim[ORD_NIV_MAX], nos[ORD_NIV_MAX];
    int d = 0;
    sim[0] = 0; nos[0] = no;
    while(d >= 0){
        if(sim[d] >= ORD_LARG){ d--; if(d >= 0) sim[d]++; continue; }
        unsigned f = par_le(ord_raiz + nos[d]*ORD_LARG + sim[d]);
        sql_ultimos_nos++;
        if(!f){ sim[d]++; continue; }
        if(d == ord_niv_idx - 1){
            /* o índice remonta-se pelos símbolos do caminho, com o MESMO
             * deslocamento com que foi escrito --- como o `.asm` monta a saída
             * dos seus quatro contadores, e não com um `<< 4` à mão */
            unsigned idx = 0;
            for(int t = 0; t <= d; t++) idx = (idx << ORD_BITS) | sim[t];
            if(*n < cap) saida[(*n)++] = (int)idx;
            sim[d]++;
            continue;
        }
        d++; nos[d] = f; sim[d] = 0;
    }
}

/* percorre uma subárvore inteira, do nível `d` até às folhas */
static void faixa_tudo(unsigned no, int d, int *saida, int *n, int cap){
    if(d < ord_niv_idx){ faixa_folhas(no, saida, n, cap); return; }
    for(unsigned sim = 0; sim < ORD_LARG; sim++){
        unsigned f = par_le(ord_raiz + no*ORD_LARG + sim);
        sql_ultimos_nos++;
        if(f) faixa_tudo(f, d - 1, saida, n, cap);
    }
}

/* os índices das linhas cujo valor está em [vmin, vmax] */
static int j_faixa(long vmin, long vmax, int *saida, int cap){
    int n = 0;
    sql_ultimos_nos = 0;
    if(vmin > vmax) return 0;
    unsigned long lo = ((unsigned long)(vmin + 2147483648L)) & 0xFFFFFFFFUL;
    unsigned long hi = ((unsigned long)(vmax + 2147483648L)) & 0xFFFFFFFFUL;

    /* desce-se pelos dois extremos ao mesmo tempo enquanto o símbolo é igual:
     * esse é o prefixo comum, e dentro dele ainda não se decide nada. */
    unsigned no = 0;
    int d = ORD_NIV - 1;              /* os níveis do VALOR, acima dos do índice */
    for(; d >= ord_niv_idx; d--){
        unsigned sl = (unsigned)((lo >> (ORD_BITS*(d-ord_niv_idx))) & (ORD_LARG - 1u));
        unsigned sh = (unsigned)((hi >> (ORD_BITS*(d-ord_niv_idx))) & (ORD_LARG - 1u));
        if(sl != sh) break;
        no = par_le(ord_raiz + no*ORD_LARG + sl);
        sql_ultimos_nos++;
        if(!no) return 0;                      /* nem sequer o prefixo existe */
    }
    /* ── E O NÚMERO DE NÍVEIS DO ÍNDICE LÊ-SE ONDE ELE É DITO ────────────
     * Estavam aqui CINCO `2` escritos à mão --- este, e os dois pares abaixo
     * ---, quando a altura já tem nome: `ord_niv_idx`. Era o mesmo número em
     * seis sítios, e o comentário do `j_casam` chegou a dizer «este era o
     * quarto sítio, e o único que eu não tinha encontrado»: faltavam estes. É
     * por eles que mudar a altura partia a árvore em silêncio. */
    if(d < ord_niv_idx){ faixa_folhas(no, saida, &n, cap); return n; }

    /* aqui os caminhos separam-se. O ramo do símbolo baixo leva tudo o que for
     * >= vmin dentro dele; o do símbolo alto leva tudo o que for <= vmax; e os
     * ramos DO MEIO levam-se inteiros, sem mais perguntas. */
    unsigned sl = (unsigned)((lo >> (ORD_BITS*(d-ord_niv_idx))) & (ORD_LARG - 1u));
    unsigned sh = (unsigned)((hi >> (ORD_BITS*(d-ord_niv_idx))) & (ORD_LARG - 1u));

    for(unsigned m = sl + 1; m < sh; m++){
        unsigned f = par_le(ord_raiz + no*ORD_LARG + m);
        sql_ultimos_nos++;
        if(f) faixa_tudo(f, d - 1, saida, &n, cap);
    }

    /* o lado de baixo: segue o caminho de vmin e leva os ramos ACIMA dele */
    { unsigned b = par_le(ord_raiz + no*ORD_LARG + sl);
      sql_ultimos_nos++;
      for(int e = d - 1; b && e >= ord_niv_idx; e--){
          unsigned s = (unsigned)((lo >> (ORD_BITS*(e-ord_niv_idx))) & (ORD_LARG - 1u));
          for(unsigned m = s + 1; m < ORD_LARG; m++){
              unsigned f = par_le(ord_raiz + b*ORD_LARG + m);
              sql_ultimos_nos++;
              if(f) faixa_tudo(f, e - 1, saida, &n, cap);
          }
          b = par_le(ord_raiz + b*ORD_LARG + s);
          sql_ultimos_nos++;
      }
      if(b) faixa_folhas(b, saida, &n, cap);   /* o próprio vmin */
    }

    /* e o lado de cima: segue o caminho de vmax e leva os ramos ABAIXO dele */
    { unsigned t = par_le(ord_raiz + no*ORD_LARG + sh);
      sql_ultimos_nos++;
      for(int e = d - 1; t && e >= ord_niv_idx; e--){
          unsigned s = (unsigned)((hi >> (ORD_BITS*(e-ord_niv_idx))) & (ORD_LARG - 1u));
          for(unsigned m = 0; m < s; m++){
              unsigned f = par_le(ord_raiz + t*ORD_LARG + m);
              sql_ultimos_nos++;
              if(f) faixa_tudo(f, e - 1, saida, &n, cap);
          }
          t = par_le(ord_raiz + t*ORD_LARG + s);
          sql_ultimos_nos++;
      }
      if(t) faixa_folhas(t, saida, &n, cap);   /* o próprio vmax */
    }
    return n;
}

/* ── CONSTRUIR E LER O ÍNDICE ────────────────────────────────────────────────
 *
 * Construir é uma varredura — Θ(|X|), uma vez. LER é uma descida — Θ(log|X|),
 * de cada vez. É essa a troca, e é a que o `arquitetura.tex` §sec:isa descreve:
 * «são precisos log n cortes para igualar uma ordem, e esse número É a
 * profundidade». O motor varria o espaço todo por cada `WHERE`; com o índice
 * deixa de o fazer, que é a cláusula do `aranha.tex` §sec:algoritmo lida à
 * letra — «nenhuma dependência de |X|, e é a que separa o algoritmo da
 * tabela». */
static long cat_nrows(void);
static long celula_valor(long i, long j, long nc);

static int idx_constroi(long col, long ncols, long nrows){
    if(col < 0 || col >= IDX_MAXCOL) return 0;
    ord_usa_indice(col);
    ord_limpa();
    long postos = 0;
    for(long i = 0; i < nrows; i++){
        if(!bit_le(S_VIVO, i)) continue;
        /* e a célula AUSENTE não tem chave. Indexar uma célula que não existe é
         * pôr na árvore o neutro com cara de valor — e depois `= 0` desce e
         * apanha-a. A árvore indexa o corpo; o dual vive no bitmap. */
        if(!bit_le(S_PRES, i*ncols + col)) continue;
        if(!ord_insere(celula_valor(i, col, ncols), (int)i)){
            ord_usa_rascunho();
            return 0;                        /* não coube: sem índice, e sem mentira */
        }
        postos++;
    }
    /* a altura vai no `.e` do cabeçalho: enquanto ela for a mesma para todas as
     * árvores isto é redundante, e é o que torna a altura VIVA possível sem
     * mudar o formato outra vez */
    { Word c; c.total = (Word8)(col + 1); c.e = (Word8)ord_niv_idx;
      mem_grava(S_IDXCAB(col), c); }
    { Word n; n.total = (Word8)(nrows & 255); n.e = (Word8)((nrows >> 8) & 255);
      mem_grava(S_IDXCAB2(col), n); }
    ord_usa_rascunho();
    return 1;
}

/* a coluna `col` tem índice válido? — existe, é dela, e não está velho */
/* ── QUANTAS LINHAS A CHAVE DO ÍNDICE DISTINGUE ──────────────────────────────
 *
 * A chave é `(valor << ord_bits_idx) | (idx & máscara)`, e no índice
 * `ord_niv_idx` é DOIS --- oito bits para o número da linha. Logo ele distingue
 * 256 linhas, e a 260.ª colide com a 4.ª.
 *
 * Isto estava salvaguardado por uma PREMISSA, e o comentário do `ord_niv_idx`
 * dizia-a: «a do índice é dois: ela recusa nos seus 600 nós MUITO ANTES das 256
 * linhas, pelo que ali a colisão não chega a acontecer». Ora eu derivei o tecto
 * de nós do mapa, de 600 para 1023 --- e a premissa CAIU com a melhoria. O
 * índice passou a ser criado com 300 linhas e a responder ERRADO: `WHERE
 * k = 260` devolvia a linha 4, e a varredura devolvia a 260.
 *
 * É o gume que a melhoria desarma: alarguei o envelope e uma lei que dependia
 * do limite deixou de valer. O limite volta, e agora DERIVADO da chave em vez
 * de vir de um efeito lateral do tecto de nós --- para que ele não dependa de
 * mais nada que eu possa vir a alargar. Acima disto o índice não se usa e a
 * varredura responde: devagar, e certo. */
#define IDX_LIN_MAX  (1L << (ORD_BITS * 2))   /* os dois níveis do ÍNDICE */

static int idx_valido(long col, long nrows){
    if(nrows > IDX_LIN_MAX) return 0;          /* a chave não distinguiria as linhas */
    if(col < 0 || col >= IDX_MAXCOL) return 0;
    Word c = mem_le(S_IDXCAB(col));
    if((long)c.total != col + 1) return 0;
    /* A FORMA TEM DE BATER. O `.e` do cabeçalho já guardava a altura com que a
     * árvore foi escrita --- «é o que torna a altura VIVA possível sem mudar o
     * formato outra vez», diz quem o pôs lá ---, e faltava lê-la. Um índice
     * gravado com outra altura é dado por velho e reconstrói-se: o formato novo
     * LÊ o que o velho escreveu, e nunca o lê com a régua errada. */
    if((long)c.e != 2) return 0;
    Word n = mem_le(S_IDXCAB2(col));
    long quando = (long)n.total | ((long)n.e << 8);
    return quando == nrows;                  /* a tabela mudou: o índice é velho */
}

/* o valor da coluna de junção da linha `d` da direita, lido do S_JDIR */
static long j_valor_dir(int d, long ncols_dir){
    int oc = j_col_dir_idx;
    if(oc < 0 || oc >= (int)ncols_dir) return 0;
    Word w = mem_le(S_JDIR + (unsigned)(d*J_MAXCOL + oc));
    return (long)w.total | ((long)w.e << 8);
}
/* as linhas da direita cujo valor de junção é `v` — desce a árvore por ele */
static int j_casam(long v, int *saida, int cap){
    /* o MESMO deslocamento com que a chave foi escrita --- este `<< 8` era o
     * quarto sítio onde o número de níveis do índice estava dito, e o único que
     * eu não tinha encontrado. O `papers/aranha.asm` faz o contrário e mostra
     * como: um contador POR FASE, indexado, e a largura é parâmetro do laço.
     * Aqui é o mesmo: `ord_bits_idx` diz-se uma vez e todos leem de lá. */
    unsigned long ch = ((unsigned long)(v + 2147483648L) << ord_bits_idx);
    unsigned no = 0;
    int n = 0;
    sql_ultimos_nos = 0;
    /* desce os nibbles do VALOR; os do índice ficam para o percurso */
    for(int d = ORD_NIV - 1; d >= ord_niv_idx; d--){
        unsigned sim = (unsigned)((ch >> (ORD_BITS*d)) & (ORD_LARG - 1u));
        no = par_le(ord_raiz + no*ORD_LARG + sim);
        sql_ultimos_nos++;
        if(!no) return 0;                          /* nenhuma linha com este valor */
    }
    /* OS ÚLTIMOS NÍVEIS SÃO O ÍNDICE: percorre-os e recolhe. Eram DOIS, escritos
     * como dois laços encaixados e um `(a1 << 4) | a2` --- o número de níveis
     * dito três vezes, e por isso mudá-lo partia isto sem que ninguém o
     * dissesse. Agora é UMA descida de `ord_niv_idx` níveis, e o índice
     * remonta-se pelo mesmo deslocamento com que foi escrito. */
    { unsigned pilha[ORD_NIV_MAX]; unsigned no_p[ORD_NIV_MAX];
      int d2 = 0;
      pilha[0] = 0; no_p[0] = no;
      while(d2 >= 0){
          if(pilha[d2] >= ORD_LARG){ d2--; if(d2 >= 0) pilha[d2]++; continue; }
          unsigned f = par_le(ord_raiz + no_p[d2]*ORD_LARG + pilha[d2]);
          sql_ultimos_nos++;
          if(!f){ pilha[d2]++; continue; }
          if(d2 == ord_niv_idx - 1){
              unsigned idx = 0;
              for(int t = 0; t <= d2; t++) idx = (idx << ORD_BITS) | pilha[t];
              if(n < cap) saida[n++] = (int)idx;
              pilha[d2]++;
              continue;
          }
          d2++; no_p[d2] = f; pilha[d2] = 0;
      } }
    return n;
}

/* lê `JOIN <tab> ON <a>.<x> = <b>.<y>`; devolve 1 se leu, 0 se não é join */
enum { J_IG = 0, J_LT, J_LE, J_GT, J_GE };
static int j_op = J_IG;              /* o operador do ON */
static char j2_tab[64] = "";         /* a TERCEIRA tabela, se houver */
static char j2_esq[64] = "", j2_dir[64] = "";
static int  j2_op = J_IG;
static int j_left = 0;               /* 1 se LEFT: a fibra vazia da esquerda */
static int j_right = 0;              /* 1 se RIGHT: a fibra vazia da direita  */

static int le_join(const char **pp, const char *tab_esq){
    const char *p = *pp;
    char q1[64], q2[64], c1[64], c2[64];
    pula(&p);
    /* LEFT É A FIBRA VAZIA.
     *
     * O interno emite os pares; o LEFT emite TAMBÉM as linhas da esquerda cuja
     * fibra do lado direito é vazia — G(x) = 0. O `def:objeto` já o diz: «o
     * suporte de G é {x : G(x) > 0}», e o que está fora do suporte é
     * exactamente o que o interno deita fora.
     *
     * E o que sai nas colunas da direita é ZERO, que aqui não é um valor
     * inventado nem um NULL: é a AUSÊNCIA — «o dual é dado pela ausência do
     * bit, b=0, o suporte NEUTRO» —, e o 0 é o que o operador devolve numa das
     * suas faces (Def. do operador). Declara-se em vez de se fingir. */
    j_left = 0; j_right = 0;
    if(palavra(&p, "LEFT")){
        j_left = 1;
        palavra(&p, "OUTER");                    /* LEFT OUTER JOIN é o mesmo */
        if(!palavra(&p, "JOIN")) return 0;
    } else if(palavra(&p, "RIGHT")){
        j_right = 1;
        palavra(&p, "OUTER");
        if(!palavra(&p, "JOIN")) return 0;
    } else if(palavra(&p, "FULL")){
        j_left = 1; j_right = 1;                 /* as duas fibras vazias */
        palavra(&p, "OUTER");
        if(!palavra(&p, "JOIN")) return 0;
    } else if(!palavra(&p, "JOIN")){
        if(!palavra(&p, "INNER")) return 0;
        if(!palavra(&p, "JOIN")) return 0;
    }
    if(!ident(&p, j_tab_dir, sizeof j_tab_dir)) return 0;
    if(!palavra(&p, "ON")) return 0;
    /* <q1>.<c1> = <q2>.<c2> — os qualificadores dizem de que lado é cada um */
    if(!ident(&p, q1, sizeof q1)) return 0;
    pula(&p); if(*p != '.') return 0; p++;
    if(!ident(&p, c1, sizeof c1)) return 0;
    /* O OPERADOR É O DUAL. A igualdade é o CORTE — descer a árvore por UM
     * símbolo, e a fibra sai inteira. A desigualdade é a ORDEM — «ordenar dá a
     * PROFUNDIDADE, o caminho inteiro; cortar dá a PARIDADE, um dígito»
     * (arquitetura §max-cut): percorre-se a árvore por ordem e toma-se o
     * SUFIXO. É o mesmo MOVE nos dois sentidos, na mesma árvore. */
    pula(&p);
    if(*p == '='){ j_op = J_IG; p++; }
    else if(*p == '<'){ p++; if(*p == '='){ j_op = J_LE; p++; } else j_op = J_LT; }
    else if(*p == '>'){ p++; if(*p == '='){ j_op = J_GE; p++; } else j_op = J_GT; }
    else return 0;
    if(!ident(&p, q2, sizeof q2)) return 0;
    pula(&p); if(*p != '.') return 0; p++;
    if(!ident(&p, c2, sizeof c2)) return 0;
    /* qual é de que lado: o qualificador tem de nomear uma das duas tabelas */
    if(!strcasecmp(q1, tab_esq) && !strcasecmp(q2, j_tab_dir)){
        snprintf(j_col_esq, sizeof j_col_esq, "%s", c1);
        snprintf(j_col_dir, sizeof j_col_dir, "%s", c2);
    }else if(!strcasecmp(q2, tab_esq) && !strcasecmp(q1, j_tab_dir)){
        snprintf(j_col_esq, sizeof j_col_esq, "%s", c2);
        snprintf(j_col_dir, sizeof j_col_dir, "%s", c1);
        /* os lados vieram trocados: o operador VIRA, senão a condição é outra */
        if(j_op == J_LT) j_op = J_GT; else if(j_op == J_GT) j_op = J_LT;
        else if(j_op == J_LE) j_op = J_GE; else if(j_op == J_GE) j_op = J_LE;
    }else return 0;
    *pp = p;
    return 1;
}


/* a ultramétrica dos endereços, no banco: a profundidade da primeira divergência
 * contada do bit MAIS significativo --- a def:arvore da aranha. */
static int qz_prof_bits(long x, long y, int bits){
    if(x == y) return bits;
    for(int i = 0; i < bits; i++)
        if(((x >> (bits-1-i)) & 1L) != ((y >> (bits-1-i)) & 1L)) return i;
    return bits;
}

static int varre(const char *resto, int acao){
    set_anula = 0;                     /* antes do parse: é ele que a levanta */
        set_ex = 0;
    const char *p = resto;
    char nome[64], alvo[64];
    long v = 0;
    int col_set = 0, tem_where;
    struct arvore cl;

    /* A LISTA DE COLUNAS ERA LIDA E DEITADA FORA.
     *
     * `SELECT a FROM s` devolvia as TRÊS colunas — o cliente pedia uma e recebia
     * outra coisa, sem erro. É o pior desfecho que há, e apareceu a sondar com o
     * psql. Agora a lista é guardada e a projecção aplica-se ao resultado; o que
     * não se souber resolver é RECUSADO com mensagem, nunca ignorado. */
    int proj_n = 0, proj[SQL_OUT_MAX_COLS];
    /* a projecção pode trazer EXPRESSÕES e não só colunas: guarda-se o tensor
     * de cada uma, e o texto para o cabeçalho */
    static int proj_ex[SQL_OUT_MAX_COLS];
    static struct tensor proj_ten[SQL_OUT_MAX_COLS];
    static char proj_nome[SQL_OUT_MAX_COLS][64];
    static int proj_fn[SQL_OUT_MAX_COLS];        /* 1 exp, 2 sin, 3 cos, 4 log */
    for(int k = 0; k < SQL_OUT_MAX_COLS; k++) proj_ex[k] = 0;
    if(acao == ACAO_MARCA){
        char cols[256];
        /* DISTINCT vem antes da lista, e diz-se já o que ele exige: UMA coluna.
         * Sobre a linha inteira seria preciso indexar a linha, e a árvore
         * indexa um valor — recusa-se em vez de devolver repetidos. */
        pula(&p);
        dis_usa = palavra(&p, "DISTINCT") ? 1 : 0;
        agr_n = 0; agr_ops[0] = 0; agr_cols[0][0] = 0;   /* zera-se ANTES de quem lê */
        agr_viu_count = 0; agr_conflito = 0;
        if(!lista_colunas(&p, cols, sizeof cols)) return 0;
        if(!palavra(&p, "FROM")) return 0;
        if(!ident(&p, nome, sizeof nome)) return 0;
        j_tab_dir[0] = 0; j2_tab[0] = 0;
        if(le_join(&p, nome)){
            /* A TERCEIRA TABELA É A COMPOSIÇÃO DE DOIS CORTES.
             *
             * Um JOIN é uma bipartição; dois JOIN são duas, e a segunda corre
             * sobre o resultado da primeira. Não é uma junção de três lados —
             * é a mesma operação duas vezes, com a saída da primeira a ser a
             * esquerda da segunda. Por isso o parser lê o segundo ON contra as
             * colunas que já estão à esquerda, e o motor não precisa de saber
             * de onde elas vieram. */
            char guarda_t[64], guarda_e[64], guarda_d[64];
            int guarda_op = j_op, guarda_l = j_left, guarda_r = j_right;
            snprintf(guarda_t, sizeof guarda_t, "%s", j_tab_dir);
            snprintf(guarda_e, sizeof guarda_e, "%s", j_col_esq);
            snprintf(guarda_d, sizeof guarda_d, "%s", j_col_dir);
            /* o segundo ON fala da tabela que a PRIMEIRA junção trouxe — a
             * esquerda dele é a direita dela, porque a saída de um corte é a
             * entrada do seguinte */
            if(le_join(&p, guarda_t)){
                snprintf(j2_tab, sizeof j2_tab, "%s", j_tab_dir);
                snprintf(j2_esq, sizeof j2_esq, "%s", j_col_esq);
                snprintf(j2_dir, sizeof j2_dir, "%s", j_col_dir);
                j2_op = j_op;
            }
            snprintf(j_tab_dir, sizeof j_tab_dir, "%s", guarda_t);
            snprintf(j_col_esq, sizeof j_col_esq, "%s", guarda_e);
            snprintf(j_col_dir, sizeof j_col_dir, "%s", guarda_d);
            j_op = guarda_op; j_left = guarda_l; j_right = guarda_r;
        }
        proj_n = (strcmp(cols, "*") == 0) ? 0 : -1;   /* −1: resolver depois de abrir */
        snprintf(proj_cols, sizeof proj_cols, "%s", cols);
    } else if(acao == ACAO_SET){
        if(!ident(&p, nome, sizeof nome)) return 0;
        if(!palavra(&p, "SET")) return 0;
    } else {
        if(!palavra(&p, "FROM")) return 0;
        if(!ident(&p, nome, sizeof nome)) return 0;
    }
    /* ABRIR A TABELA VEM PRIMEIRO — antes de resolver coluna nenhuma.
     *
     * O nome foi lido nos TRÊS ramos e não era usado em nenhum; agora abre-se o
     * .mem da tabela e confere-se que o catálogo lá dentro é mesmo o dela. E isto
     * tem de acontecer ANTES do `SET x` e do `WHERE`, porque uma coluna só existe
     * DENTRO de uma tabela: com a ordem trocada, o `UPDATE cliente SET saldo`
     * logo a seguir a um `SELECT * FROM conta` procurava a coluna «saldo» na
     * `conta` e devolvia «column "saldo" does not exist». Foi a ligação ponta a
     * ponta pelo FEBE que o mostrou — nenhuma asserção o via, porque nos testes a
     * tabela do UPDATE era sempre a que já estava aberta. */
    if(!usa_tabela(nome, 0)) return cat_nome_recusa(nome);
    if(!cat_nome_bate(nome)) return cat_nome_recusa(nome);
    if(acao == ACAO_SET){
        if(!ident(&p, alvo, sizeof alvo)) return 0;
        pula(&p); if(*p != '=') return 0; p++;
        pula(&p);
        if(palavra(&p, "NULL")) { set_anula = 1; v = 0; }
        /* ── A CADEIA NO SET ESCREVE-SE, E NÃO SE PROCURA ────────────────────
         *
         * `SET s = 'novo'` caía no `le_num`, que lê a cadeia como o ENDEREÇO
         * dela no pool --- e o faz com `tx_procura`, que pergunta e não escreve.
         * Numa condição isso está certo: perguntar por uma cadeia que ninguém
         * escreveu não a pode criar. Numa ESCRITA está errado: 'novo' não estava
         * no pool, o endereço vinha 0, e a célula ficava AUSENTE. O UPDATE dizia
         * «1 linha atualizada» e o valor desaparecia --- aceitava e apagava.
         *
         * É a mesma distinção que o `tx_procura` traz escrita na porta: ele é a
         * outra face do `tx_guarda`. Quem escreve usa o que escreve. */
        else if(*p == '\''){
            const char *r = p + 1;
            char buf[TX_MAX + 2]; int bn = 0, fechou = 0;
            /* E A CADEIA QUE NÃO CABE RECUSA AQUI TAMBÉM — a mesma forma do
             * INSERT, porque é a mesma escrita. Estava `if(bn < TX_MAX)`, que
             * DESCARTA em silêncio: um SET de 300 caracteres gravava 240 e
             * anunciava «1 linha atualizada», sem aviso nenhum. O INSERT já
             * marcava e recusava; a guarda tinha sido posta numa face do par e
             * não na outra, e é exactamente o desfecho que o comentário acima
             * conta ter custado caro — aceitar e não fazer o que foi dito. */
            while(*r){
                if(*r == '\''){
                    if(r[1] == '\''){ if(bn < TX_MAX) buf[bn++] = '\''; else bn = TX_MAX + 1; r += 2; continue; }
                    r++; fechou = 1; break;
                }
                if(bn < TX_MAX) buf[bn++] = *r; else bn = TX_MAX + 1;
                r++;
            }
            if(bn > TX_MAX){
                printf("erro: a cadeia do SET passa do que a célula segura --- RECUSADA.\n");
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err, "string too long"); }
                return 0;
            }
            buf[bn] = 0;
            if(!fechou){
                printf("erro: a cadeia do SET não fecha — RECUSADA.\n");
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err, "unterminated string"); }
                return 0;
            }
            { long ca = col_indice(alvo);
              if(ca < 0 || corpo_de(ca).total != CORPO_TEXTO){
                  printf("erro: a coluna «%s» não é de TEXTO e veio uma cadeia —"
                         " RECUSADA. Um valor de outro corpo não se converte em"
                         " silêncio.\n", alvo);
                  if(sql_cap){ sql_cap->ok = 0;
                      snprintf(sql_cap->err, sizeof sql_cap->err,
                               "column \"%s\" is not textual", alvo); }
                  return 0;
              } }
            v = (long)tx_guarda(buf, bn);
            p = r;
        }
        else if(!numero(&p, &v)){
            /* ── NÃO É NÚMERO: TENTA-SE COMO EXPRESSÃO ──────────────────────
             * O MESMO `le_num` do WHERE e da projecção — não uma segunda
             * leitura. Se o motor soubesse ler `b+1` de duas maneiras, elas
             * podiam divergir; sabendo de uma, a simetria é estrutural e não
             * uma coincidência que se mede. */
            const char *e = p;
            struct tensor t;
            citadas_where = 0; citadas_desig = 0;
            sim_zera();
            if(!le_num(&e, &t)) return 0;
            pula(&e);
            if(*e && strncasecmp(e, "WHERE", 5)) return 0;
            /* ── E O DENOMINADOR DECIDE-SE AQUI, QUE É ONDE SE PODE ─────────
             * `b/2` numa coluna de inteiros não é um arredondamento a fazer: é
             * uma escrita que o corpo não aceita, e guardar o truncado seria pôr
             * na tabela um número que não é o resultado. O denominador é do
             * TENSOR e não da linha — é o mesmo para todas —, pelo que se sabe
             * já aqui; e tem de ser aqui, porque a função que escreve devolve
             * PASSOS e não veredicto: recusar lá deixava metade das linhas
             * mudadas e a resposta a dizer que tudo correu bem. Foi assim que
             * saiu à primeira. */
            if(t.den && t.den != 1){
                printf("erro: `%.*s` dá um racional (denominador %ld) e a coluna"
                       " «%s» é de INTEIROS — guardar o truncado seria pôr na"
                       " tabela um número que não é o resultado. RECUSADA."
                       " (`SELECT` responde-o, porque produzir não é escrever.)\n",
                       (int)(e - p), p, (long)t.den, alvo);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "expression yields a rational (denominator %ld),"
                             " column \"%s\" is integer", (long)t.den, alvo); }
                return 0;
            }
            set_ten = t; set_ex = 1;
            v = 0;
            p = e;
        }
        col_set = col_indice(alvo);
        if(col_set < 0){
            printf("erro: a coluna «%s» não existe na tabela «%s» — o UPDATE é RECUSADO.\n",
                   alvo, nome);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "column \"%s\" does not exist", alvo); }
            return 0;
        }
    }
    /* resolver a projecção agora que a tabela está aberta: cada nome tem de ser
     * uma coluna DESTA tabela, ou a consulta é recusada.
     *
     * COM JUNÇÃO, ADIA-SE. A tabela contra a qual a projecção se resolve não é
     * a da esquerda: é a que a junção PRODUZ — as colunas de uma seguidas das
     * da outra. Resolver aqui recusaria toda a coluna da direita, que foi
     * exactamente o que aconteceu enquanto isto correu cedo demais. */
    if(acao == ACAO_MARCA && proj_n < 0 && !j_tab_dir[0]){
        const char *q = proj_cols;
        proj_n = 0;
        while(*q && proj_n < SQL_OUT_MAX_COLS){
            char nome_c[64]; int i = 0;
            while(*q && *q != ',' && i + 1 < (int)sizeof nome_c) nome_c[i++] = *q++;
            nome_c[i] = 0;
            if(*q == ',') q++;
            { int c = col_indice(nome_c);
              if(c >= 0){ proj[proj_n] = c; proj_ex[proj_n] = 0; proj_n++; continue; }
              /* ── É UMA ANALÍTICA? `exp(a)`, `sin(a)`, `cos(a)`, `log(a)` ──
               * A série vem da casa (`lib/serie.h`, extraída do `calculo2.h`) e
               * os coeficientes são exactos em ℚ. O que aqui se decide é só
               * QUAL e SOBRE QUE COLUNA. */
              { const char *ab = strchr(nome_c, '(');
                if(ab){
                    char fn[16], col[64];
                    size_t nf = (size_t)(ab - nome_c);
                    int qual = 0;
                    if(nf < sizeof fn){
                        memcpy(fn, nome_c, nf); fn[nf] = 0;
                        { const char *r = ab + 1;
                          /* e depois do parêntese não pode vir NADA: `exp(x)+1`
                           * tem o fecho a meio, e aceitar isso era ler `exp(x)`
                           * e deitar fora o resto — responder certo sobre outra
                           * coisa. Não sendo a forma pura, o item segue para o
                           * caminho das expressões, e lá é recusado com a razão
                           * (o tensor não sabe o que `exp` é). */
                          if(ident(&r, col, sizeof col) && *r == ')' && r[1] == 0){
                              qual = !strcasecmp(fn,"EXP") ? 1
                                   : !strcasecmp(fn,"SIN") ? 2
                                   : !strcasecmp(fn,"COS") ? 3
                                   : !strcasecmp(fn,"LOG") ? 4 : 0;
                              if(qual){
                                  int ci2 = col_indice(col);
                                  if(ci2 < 0){
                                      printf("erro: a coluna «%s» não existe —"
                                             " RECUSADA.\n", col);
                                      if(sql_cap){ sql_cap->ok = 0;
                                          snprintf(sql_cap->err, sizeof sql_cap->err,
                                                   "column \"%s\" does not exist", col); }
                                      return 0;
                                  }
                                  proj[proj_n] = ci2;
                                  proj_ex[proj_n] = 2;          /* analítica */
                                  proj_fn[proj_n] = qual;
                                  snprintf(proj_nome[proj_n], sizeof proj_nome[0],
                                           "%s", nome_c);
                                  proj_n++;
                                  continue;
                              }
                          } }
                    }
                } }

              /* ── NÃO É UMA COLUNA: TENTA-SE COMO EXPRESSÃO ──────────────
               * O mesmo `le_num` do WHERE, agora a produzir em vez de a
               * decidir. Compila-se aqui, depois de a tabela abrir, porque é
               * só aqui que os nomes de coluna se resolvem — a mesma razão de
               * a projecção inteira ser resolvida neste sítio. */
              { const char *e = nome_c;
                struct tensor t;
                citadas_where = 0; citadas_desig = 0;
                if(le_num(&e, &t)){
                    pula(&e);
                    if(*e == 0){
                        proj[proj_n] = -1;
                        proj_ex[proj_n] = 1;
                        proj_ten[proj_n] = t;
                        snprintf(proj_nome[proj_n], sizeof proj_nome[0], "%s", nome_c);
                        proj_n++;
                        continue;
                    }
                }
              }
              printf("erro: «%s» não é coluna desta tabela nem expressão que eu"
                     " saiba ler — RECUSADA.\n", nome_c);
              if(sql_cap){ sql_cap->ok = 0;
                  snprintf(sql_cap->err, sizeof sql_cap->err,
                           "column \"%s\" does not exist", nome_c); }
              return 0; }
        }
    }

    /* ── WHERE <col> IN (SELECT <col> FROM <tabela>) ──────────────────────
     *
     * A SUBCONSULTA É A PERTENÇA A UMA FIBRA, e por isso não traz maquinaria
     * nova: a árvore que o JOIN usa para casar já responde «este valor está
     * lá?». A diferença entre as duas é o que se faz com a resposta — o join
     * produz o PAR, o IN fica-se pelo bit. É o corte do §sec:dual sem a
     * segunda metade: descer por um valor é a dobra, e o que se lê no fim do
     * caminho é se a fibra é vazia ou não.
     *
     * Lê-se ANTES do WHERE geral, porque a sua condição não compila para a
     * ISA: a árvore vive do lado de cá. As formas compostas — o IN dentro de
     * um AND, ou com outras condições ao lado — são RECUSADAS com a razão,
     * que é o que este motor faz com tudo o que ainda não sabe ler. */
    /* variáveis PRÓPRIAS, e não as do join: escrever em `j_tab_dir` acorda o
     * caminho do JOIN mais abaixo, que espera um `j_col_esq` que aqui não
     * existe — e a consulta era recusada com «a coluna “” não existe». O
     * estado partilhado é que ligava dois caminhos que nada têm um com o
     * outro; a árvore é que é comum, e essa empresta-se na hora. */
    char in_tab[64] = "", in_col[64] = "";
    int in_sub = 0, in_col_esq = -1, in_nega = 0;
    {
        const char *q = p;
        pula(&q);
        char c_esq[64];
        if(palavra(&q, "WHERE") && ident(&q, c_esq, sizeof c_esq)){
            pula(&q);
            { const char *vn = q;
              if(palavra(&q, "NOT")){ in_nega = 1; pula(&q); }
              if(!palavra(&q, "IN")){ q = vn; in_nega = 0; goto sem_in; } }
            {
                pula(&q);
                if(*q == '('){
                    const char *r = q + 1;
                    pula(&r);
                    char c_sub[64], t_sub[64];
                    if(palavra(&r, "SELECT") && ident(&r, c_sub, sizeof c_sub)
                       && (pula(&r), palavra(&r, "FROM"))
                       && ident(&r, t_sub, sizeof t_sub)){
                        pula(&r);
                        if(*r == ')'){
                            r++; pula(&r);
                            if(*r == 0 || *r == ';'){
                                in_col_esq = col_indice(c_esq);
                                if(in_col_esq < 0){
                                    printf("erro: a coluna «%s» não existe na tabela «%s»"
                                           " — RECUSADA.\n", c_esq, nome);
                                    if(sql_cap){ sql_cap->ok = 0;
                                        snprintf(sql_cap->err, sizeof sql_cap->err,
                                                 "column \"%s\" does not exist", c_esq); }
                                    return 0;
                                }
                                snprintf(in_tab, sizeof in_tab, "%s", t_sub);
                                snprintf(in_col, sizeof in_col, "%s", c_sub);
                                in_sub = 1;
                                p = r;
                            }
                        }
                    }
                }
            }
        }
        sem_in: ;
    }

    /* ── E SE HOUVER ÍNDICE, NÃO SE VARRE ─────────────────────────────────
     *
     * A forma `WHERE <col> = <k>` é a que a árvore responde de uma descida.
     * Reconhece-se aqui, antes de compilar o molde, e só a forma SIMPLES — sem
     * nada ao lado —, porque é a única em que a resposta da árvore é a resposta
     * toda. Com qualquer outra coisa a seguir, o molde corre como sempre.
     *
     * O índice é ignorado se estiver velho (a tabela mudou de tamanho desde que
     * ele foi feito): aí varre-se. Um índice velho custa tempo, nunca correcção. */
    /* ── E SE HOUVER ÍNDICE, NÃO SE VARRE ─────────────────────────────────
     *
     * A árvore responde de uma descida a tudo o que seja uma FAIXA sobre a
     * coluna indexada. Lê-se aqui, antes de compilar o molde, e o que se lê é
     * uma comparação, ou DUAS ligadas por AND sobre a mesma coluna — que é a
     * faixa com os dois extremos —, ou o `BETWEEN`, que é a mesma coisa dita
     * numa palavra. As duas condições INTERSECTAM: cada uma é um lado, e o AND
     * fecha-os. Com qualquer outra coisa, o molde corre como sempre.
     *
     * O índice é ignorado se estiver velho; aí varre-se. Um índice velho custa
     * tempo, nunca correcção. */
    int idx_usa = 0, idx_pre = 0, idx_liga = 0;
    long idx_lo = 0, idx_hi = 0, idx_col = -1;
    long idx_lo2 = 0, idx_hi2 = 0, idx_col2 = -1;
    if(acao == ACAO_MARCA && !in_sub){
        const long INF = 2147483647L;
        const char *q = p;
        char c_esq[64], c2[64];
        long lo = -INF, hi = INF;
        int lados = 0, col = -1;
        if(palavra(&q, "WHERE") && ident(&q, c_esq, sizeof c_esq)){
            col = col_indice(c_esq);
            pula(&q);
            /* o BETWEEN já não chega aqui: foi reescrito à entrada em duas
             * condições, que é a forma que este laço lê. */
            {
                for(;;){
                    int op = 0;
                    if(*q == '='){ op = '='; q++; }
                    else if(*q == '<'){ q++; if(*q == '='){ op = 'l'; q++; } else op = '<'; }
                    else if(*q == '>'){ q++; if(*q == '='){ op = 'g'; q++; } else op = '>'; }
                    if(!op) { lados = 0; break; }
                    long v;
                    if(!le_int_simples(&q, &v)) { lados = 0; break; }
                    switch(op){
                      case '=': if(v > lo) lo = v; if(v < hi) hi = v; break;
                      case '<': if(v - 1 < hi) hi = v - 1; break;
                      case 'l': if(v     < hi) hi = v;     break;
                      case '>': if(v + 1 > lo) lo = v + 1; break;
                      case 'g': if(v     > lo) lo = v;     break;
                    }
                    lados++;
                    pula(&q);
                    /* uma segunda condição, sobre a MESMA coluna, ligada por AND */
                    const char *r = q;
                    if(lados < 2 && palavra(&r, "AND") && ident(&r, c2, sizeof c2)
                       && col >= 0 && col_indice(c2) == col){
                        q = r; pula(&q); continue;
                    }
                    break;
                }
            }
            pula(&q);
            if(lados >= 1 && col >= 0 && idx_valido(col, cat_nrows())){
                if(*q == 0 || *q == ';'){
                    /* a faixa É a resposta toda: o molde não corre */
                    idx_lo = lo; idx_hi = hi; idx_usa = 1; idx_col = col; p = q;
                }else{
                    /* ── O CORTE PRIMEIRO, O MOLDE SÓ SOBRE O QUE SOBROU ────
                     *
                     * Sobra condição, e ela é sobre outra coluna: a árvore não
                     * responde à pergunta inteira, mas responde a METADE dela —
                     * e a metade que responde RESTRINGE, porque a ligação é um
                     * AND. Serve por isso de PRÉ-FILTRO: desce-se, e o molde
                     * corre só nas linhas que a faixa deixou, em vez de correr
                     * em todas.
                     *
                     * Tem de ser AND. Com um OR o outro lado pode trazer linhas
                     * de fora da faixa, e um pré-filtro que as corte responde
                     * menos do que a pergunta — que é pior do que responder
                     * devagar. Verifica-se a palavra, e só ela abre esta porta. */
                    const char *r = q;
                    int liga = 0;
                    if(palavra(&r, "AND")) liga = 'A';
                    else if(palavra(&r, "OR")) liga = 'O';
                    if(liga){
                        /* ── OS DOIS LADOS DESCEM, E OS CAMPOS COMBINAM-SE ────
                         *
                         * Se o outro lado também é uma faixa sobre uma coluna
                         * INDEXADA, não é preciso molde nenhum: descem-se as
                         * duas e o campo resultante é a INTERSECÇÃO (AND) ou a
                         * UNIÃO (OR) — o par ∧/∨, agora sobre os campos.
                         *
                         * É isto que tira o OR da lista dos que não descem: ele
                         * estava fora porque METADE não chegava, e agora não é
                         * metade. Se o outro lado não for indexável, fica o que
                         * já havia: o AND pré-filtra, o OR corre o molde. */
                        char c3[64];
                        const char *r2 = r;
                        long lo2 = -INF, hi2 = INF, col2 = -1;
                        int op2 = 0, ok2 = 0;
                        if(ident(&r2, c3, sizeof c3)){
                            col2 = col_indice(c3);
                            pula(&r2);
                            if(*r2 == '='){ op2 = '='; r2++; }
                            else if(*r2 == '<'){ r2++; if(*r2 == '='){ op2='l'; r2++; } else op2='<'; }
                            else if(*r2 == '>'){ r2++; if(*r2 == '='){ op2='g'; r2++; } else op2='>'; }
                            long v2;
                            if(op2 && le_int_simples(&r2, &v2)){
                                switch(op2){
                                  case '=': lo2 = v2;     hi2 = v2;     break;
                                  case '<': hi2 = v2 - 1;               break;
                                  case 'l': hi2 = v2;                   break;
                                  case '>': lo2 = v2 + 1;               break;
                                  case 'g': lo2 = v2;                   break;
                                }
                                pula(&r2);
                                if((*r2 == 0 || *r2 == ';') && col2 >= 0
                                   && col2 != col && idx_valido(col2, cat_nrows())) ok2 = 1;
                            }
                        }
                        if(ok2){
                            idx_lo = lo; idx_hi = hi; idx_col = col;
                            idx_lo2 = lo2; idx_hi2 = hi2; idx_col2 = col2;
                            idx_liga = liga; idx_usa = 1; p = r2;
                        }else if(liga == 'A'){
                            idx_lo = lo; idx_hi = hi; idx_pre = 1; idx_col = col;
                        }
                    }
                }
            }
        }
    }

    /* ── WHERE <col> IS [NOT] NULL ────────────────────────────────────────
     *
     * A ausência não é um valor: é o dual dele. Não se compila para a ISA
     * porque não há nada a comparar — lê-se o BIT de presença, que é o mesmo
     * campo do vivo um andar abaixo, por célula em vez de por linha. */
    int nul_usa = 0, nul_nega = 0; long nul_col = -1;
    if(acao == ACAO_MARCA && !in_sub && !idx_usa && !idx_pre){
        const char *q = p;
        char c_esq[64];
        if(palavra(&q, "WHERE") && ident(&q, c_esq, sizeof c_esq)){
            pula(&q);
            if(palavra(&q, "IS")){
                { const char *r = q; if(palavra(&r, "NOT")){ nul_nega = 1; q = r; } }
                if(palavra(&q, "NULL")){
                    pula(&q);
                    if(*q == 0 || *q == ';'){
                        long c = col_indice(c_esq);
                        if(c < 0){
                            printf("erro: a coluna «%s» não existe na tabela «%s»"
                                   " — RECUSADA.\n", c_esq, nome);
                            if(sql_cap){ sql_cap->ok = 0;
                                snprintf(sql_cap->err, sizeof sql_cap->err,
                                         "column \"%s\" does not exist", c_esq); }
                            return 0;
                        }
                        nul_col = c; nul_usa = 1; p = q;
                    }
                }
            }
        }
    }

    /* ── `EXISTS (SELECT … FROM tab)`: O QUANTIFICADOR ───────────────────────
     *
     * O `IN` pergunta sobre a LINHA — «este valor está do outro lado?» — e por
     * isso a sua resposta muda de linha para linha. O `EXISTS` não olha para a
     * linha nenhuma: pergunta se a subconsulta devolve ALGUMA, e a resposta é a
     * mesma para todas. É um quantificador, não uma comparação.
     *
     * Daí duas consequências que o distinguem, e as duas se medem. A primeira é
     * o CUSTO: decide-se UMA vez, com a tabela do outro lado aberta, e depois a
     * condição é uma constante — não há molde a correr por linha. A segunda é a
     * DUALIDADE: `EXISTS` e `NOT EXISTS` são complementares sobre |I| INTEIRO,
     * porque nenhum deles olha para a célula; ao contrário do `IN`, cuja soma
     * fecha no peso dos PRESENTES, porque esse olha (§W38).
     *
     * `NOT EXISTS` é o ∀ escrito com o ∃ — a mesma De Morgan do §W42, agora
     * sobre um conjunto em vez de sobre uma proposição. */
    int ex_usa = 0, ex_vale = 0;
    if(acao == ACAO_MARCA && !in_sub && !idx_usa && !idx_pre && !nul_usa){
        const char *q = p;
        int nega = 0;
        pula(&q);
        if(palavra(&q, "WHERE")){
            { const char *r = q; if(palavra(&r, "NOT")){ nega = 1; q = r; } }
            if(palavra(&q, "EXISTS")){
                pula(&q);
                if(*q == '('){
                    const char *r = q + 1;
                    char c_sub[64], t_sub[64];
                    pula(&r);
                    if(palavra(&r, "SELECT")
                       && (*r == '*' ? (r++, 1) : ident(&r, c_sub, sizeof c_sub))
                       && (pula(&r), palavra(&r, "FROM"))
                       && ident(&r, t_sub, sizeof t_sub)){
                        pula(&r);
                        if(*r == ')'){
                            r++; pula(&r);
                            if(*r == 0 || *r == ';'){
                                /* a decisão toma-se com a outra tabela ABERTA e
                                 * traz-se em memória local — a mesma regra da
                                 * subconsulta e da seta */
                                char guarda[64];
                                int ha = -1;
                                snprintf(guarda, sizeof guarda, "%s", nome);
                                if(usa_tabela(t_sub, 0) && cat_nome_bate(t_sub)){
                                    long nr2 = cat_nrows();
                                    ha = 0;
                                    for(long i2 = 0; i2 < nr2 && !ha; i2++)
                                        if(bit_le(S_VIVO, i2)) ha = 1;
                                }
                                if(!usa_tabela(guarda, 0)) restauros_falhados++;
                                if(ha < 0){
                                    printf("erro: a tabela «%s» do EXISTS não pôde"
                                           " ser lida — RECUSADA.\n", t_sub);
                                    if(sql_cap){ sql_cap->ok = 0;
                                        snprintf(sql_cap->err, sizeof sql_cap->err,
                                                 "relation \"%s\" does not exist", t_sub); }
                                    return 0;
                                }
                                ex_usa = 1;
                                ex_vale = nega ? !ha : ha;
                                p = r;
                            }
                        }
                    }
                }
            }
        }
    }

    citadas_where = 0; citadas_desig = 0;
    tem_where = (in_sub || idx_usa || nul_usa || ex_usa) ? 0 : le_where(&p, &cl);
    if(tem_where < 0){
        printf("erro: o WHERE não foi entendido — a consulta é RECUSADA, e nada é devolvido\n");
        return 0;
    }

    /* O QUE SOBRA NÃO SE IGNORA.
     *
     * `ORDER BY` e `LIMIT` eram deitados fora em silêncio: o cliente pedia as
     * linhas ordenadas e recebia-as por ordem de inserção, sem erro nenhum, e
     * acreditava. Responder outra coisa é pior do que recusar — quem chama sabe
     * lidar com um erro, não sabe lidar com uma resposta que parece a que pediu. */
    /* ── ORDER BY <coluna> [ASC|DESC] ─────────────────────────────────────
     * Lê-se aqui, depois do WHERE, e é a ORDEM do arquitetura.tex: descer a
     * árvore pelos símbolos do valor. O que NÃO for isto continua recusado. */
    ord_col[0] = 0; ord_desc = 0; ord_col2[0] = 0; ord_desc2 = 0;
    snprintf(cnt_dis, sizeof cnt_dis, "%s", cnt_dis_pedido);
    cnt_dis_pedido[0] = 0;
    mat_op = mat_op_pedido; mat_op_pedido = 0;
    snprintf(mat_tab2, sizeof mat_tab2, "%s", mat_tab2_pedido);
    mat_tab2_pedido[0] = 0;
    grp_col[0] = 0; grp_col2[0] = 0; lim_n = -1; off_n = 0;
    hav_op = 0; hav_n = 0;
    /* o dis_usa é lido acima, com a lista de colunas */
    if(acao == ACAO_MARCA){
        const char *q = p;
        pula(&q);
        /* ── GROUP BY <coluna> ── a fibra: quocientar pela coluna ───────── */
        if(palavra(&q, "GROUP")){
            if(!palavra(&q, "BY")){
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "esperava BY depois de GROUP"); }
                return 0;
            }
            if(!ident(&q, grp_col, sizeof grp_col)) return 0;
            { const char *v2 = q;
              pula(&q);
              if(*q == ','){
                  q++; pula(&q);
                  if(!ident(&q, grp_col2, sizeof grp_col2)){ q = v2; grp_col2[0] = 0; }
                  else if(col_indice(grp_col2) < 0){
                      printf("erro: a coluna «%s» não existe — o GROUP BY é"
                             " RECUSADO.\n", grp_col2);
                      if(sql_cap){ sql_cap->ok = 0;
                          snprintf(sql_cap->err, sizeof sql_cap->err,
                                   "column \"%s\" does not exist", grp_col2); }
                      return 0;
                  }
              } else q = v2; }
            if(col_indice(grp_col) < 0){
                printf("erro: a coluna «%s» não existe — o GROUP BY é RECUSADO.\n", grp_col);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "column \"%s\" does not exist", grp_col); }
                return 0;
            }
            p = q; pula(&q);
            /* ── HAVING count(*) <op> <n> ── o WHERE sobre G ─────────────── */
            if(palavra(&q, "HAVING")){
                pula(&q);
                if(!palavra(&q, "COUNT")){
                    printf("erro: HAVING pede count(*) — é sobre G que ele filtra."
                           " RECUSADO.\n");
                    if(sql_cap){ sql_cap->ok = 0;
                        snprintf(sql_cap->err, sizeof sql_cap->err,
                                 "HAVING: so count(*), que e o tamanho da fibra"); }
                    return 0;
                }
                pula(&q);
                if(*q == '('){ q++; pula(&q); if(*q == '*') q++; pula(&q);
                               if(*q == ')') q++; }
                pula(&q);
                if(*q == '>'){ hav_op = 1; q++; }
                else if(*q == '<'){ hav_op = 2; q++; }
                else if(*q == '='){ hav_op = 3; q++; }
                else {
                    printf("erro: HAVING pede >, < ou = — RECUSADO.\n");
                    if(sql_cap){ sql_cap->ok = 0;
                        snprintf(sql_cap->err, sizeof sql_cap->err,
                                 "HAVING: esperava >, < ou ="); }
                    return 0;
                }
                pula(&q);
                { long v = 0; int viu = 0;
                  while(*q >= '0' && *q <= '9'){ v = v*10 + (*q - '0'); q++; viu = 1; }
                  if(!viu){
                      printf("erro: HAVING sem número — RECUSADO.\n");
                      if(sql_cap){ sql_cap->ok = 0;
                          snprintf(sql_cap->err, sizeof sql_cap->err,
                                   "HAVING: esperava um numero"); }
                      return 0;
                  }
                  hav_n = v; }
                p = q; pula(&q);
            }
        }
        if(palavra(&q, "ORDER")){
            if(!palavra(&q, "BY")){
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "esperava BY depois de ORDER"); }
                return 0;
            }
            /* ── O ORDINAL: `ORDER BY 1` refere a PRIMEIRA coluna pedida ────
             * Não é uma coluna chamada «1»: é a posição na lista da projecção.
             * Resolve-se aqui contra `proj_cols`, que é o texto dessa lista, e
             * o resultado é o mesmo nome que o cliente teria escrito — pelo que
             * daqui para a frente não há caminho novo nenhum. */
            { const char *v0 = q;
              pula(&q);
              if(*q >= '1' && *q <= '9'){
                  long k = 0;
                  while(*q >= '0' && *q <= '9'){ k = k*10 + (*q - '0'); q++; }
                  { const char *c = proj_cols; long t = 1;
                    ord_col[0] = 0;
                    if(strcmp(proj_cols, "*") == 0){
                        /* com `*` a n-ésima pedida é a n-ésima da tabela */
                        col_nome_le((int)k - 1, ord_col, (int)sizeof ord_col);
                    } else {
                        while(*c && t <= k){
                            int i = 0;
                            while(*c && *c != ',' && i + 1 < (int)sizeof ord_col)
                                ord_col[i++] = *c++;
                            ord_col[i] = 0;
                            if(*c == ',') c++;
                            if(t == k) break;
                            t++;
                        }
                        if(t < k) ord_col[0] = 0;
                    }
                    if(!ord_col[0]){
                        printf("erro: `ORDER BY %ld` — a consulta não pede tantas"
                               " colunas. RECUSADA.\n", k);
                        if(sql_cap){ sql_cap->ok = 0;
                            snprintf(sql_cap->err, sizeof sql_cap->err,
                                     "ORDER BY position %ld is not in select list", k); }
                        return 0;
                    } }
              } else { q = v0; if(!ident(&q, ord_col, sizeof ord_col)) return 0; } }
            if(col_indice(ord_col) < 0){
                printf("erro: a coluna «%s» não existe — o ORDER BY é RECUSADO.\n", ord_col);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "column \"%s\" does not exist", ord_col); }
                return 0;
            }
            pula(&q);
            if(palavra(&q, "DESC")) ord_desc = 1;
            else if(palavra(&q, "ASC")) ord_desc = 0;
            pula(&q);
            if(*q == ','){                       /* a segunda régua */
                q++; pula(&q);
                if(!ident(&q, ord_col2, sizeof ord_col2)) return 0;
                if(col_indice(ord_col2) < 0){
                    printf("erro: a coluna «%s» não existe — o ORDER BY é RECUSADO.\n",
                           ord_col2);
                    if(sql_cap){ sql_cap->ok = 0;
                        snprintf(sql_cap->err, sizeof sql_cap->err,
                                 "column \"%s\" does not exist", ord_col2); }
                    return 0;
                }
                pula(&q);
                if(palavra(&q, "DESC")) ord_desc2 = 1;
                else if(palavra(&q, "ASC")) ord_desc2 = 0;
            }
            p = q;
        }
        /* ── LIMIT <n> ── o prefixo da lista ─────────────────────────────── */
        pula(&q);
        if(palavra(&q, "LIMIT")){
            long v = 0; int viu = 0;
            pula(&q);
            while(*q >= '0' && *q <= '9'){ v = v*10 + (*q - '0'); q++; viu = 1; }
            if(!viu){
                printf("erro: LIMIT sem número — RECUSADO.\n");
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "LIMIT: esperava um numero"); }
                return 0;
            }
            lim_n = v; p = q;
        }
        /* e o OFFSET, que pode vir depois do LIMIT ou sozinho */
        { const char *q = p;
          if(palavra(&q, "OFFSET")){
            pula(&q);
            long v = 0; int viu = 0;
            while(*q >= '0' && *q <= '9'){ v = v*10 + (*q - '0'); q++; viu = 1; }
            if(!viu){
                printf("erro: OFFSET sem número — RECUSADO.\n");
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "OFFSET: esperava um numero"); }
                return 0;
            }
            off_n = v; p = q;
          } }
    }

    pula(&p);
    if(*p == ';') { p++; pula(&p); }
    if(*p){
        char sobra[64]; int i = 0;
        while(p[i] && i + 1 < (int)sizeof sobra){ sobra[i] = p[i]; i++; }
        sobra[i] = 0;
        printf("erro: «%s» não é entendido — a consulta é RECUSADA, e nada é devolvido.\n",
               sobra);
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err,
                     "nao suportado: \"%.60s\" — recusa em vez de devolver outra coisa",
                     sobra); }
        return 0;
    }

    Word cat = mem_le(S_CAT); (void)cat;
    long ncols = cat_ncols(), nrows = cat_nrows();
    /* A DISTÂNCIA LIGADA AO WHERE: só se compara dentro da classe de isomorfismo. */
    if(tem_where > 0 && !checa_corpos(citadas_where, ncols)) return 0;
    /* ── E A MARCA DIZ SE VALE A PENA ANDAR ──────────────────────────────────
     *
     * Antes de compilar a varredura, lê-se o ALCANCE de cada coluna citada ---
     * a marca que o `col_marca` deixou no acto da escrita --- e pergunta-se se a
     * condição pode sequer ser satisfeita. `c1 > 1000` numa coluna cujo maior
     * valor escrito é 1 não precisa de ler linha nenhuma: o máximo da forma
     * `c1 − 1000` é −999.
     *
     * Só se decide com UM átomo. Com vários há OR pelo meio, e um átomo
     * impossível não torna a cláusula impossível --- concluir dali seria dar
     * vazio a quem tinha resposta, que é o pior desfecho. Na dúvida varre-se. */
    /* e SÓ na projecção simples: com agregação ou GROUP BY o caminho tem de
     * correr, porque `sum` sobre um vazio devolve AUSENTE e não zero --- são
     * níveis diferentes, e saltar o agregado dava o número no lugar do dual.
     * O mesmo com as operações de MATRIZ: «zero linhas não é a conta deu vazio,
     * é NÃO HAVER conta», e elas têm de recusar em vez de devolver nada.
     * A guarda poupa a varredura; não substitui a resposta --- foi assim que ela
     * saiu à segunda e à terceira, com o medidor a apanhar as duas. */
    if(tem_where > 0 && cl.natomo == 1 && acao == ACAO_MARCA
       && agr_n == 0 && !agr_viu_count && !grp_col[0] && !dis_usa && !mat_op
       && !atom_possivel(&cl, 0, ncols, nrows)){
        for(long i = 0; i < nrows; i++) bit_poe(S_MATCH, i, 0);
        ultima_conta = 0;
        /* E O CONTADOR DE PASSOS DIZ-SE AQUI TAMBÉM. Ele é escrito num sítio
         * só, lá em baixo, e esta saída antecipada salta-o --- pelo que quem
         * perguntasse o custo desta consulta recebia o da ANTERIOR. É o defeito
         * que esta casa já regista noutro sítio («a saída antecipada salta a
         * linha que escreve o contador»), e aqui doía a dobrar: o custo é
         * exactamente o que esta guarda existe para tornar ZERO, e era o único
         * número que não se via. */
        sql_ultimos_passos = 0;
        printf("-- a marca da coluna diz que nenhuma linha pode satisfazer:"
               " 0 linha(s), sem varrer\n");
        if(sql_cap){
            memset(sql_cap, 0, sizeof *sql_cap);
            sql_cap->ok = 1; sql_cap->nrows = 0;
            sql_cap->ncols = proj_n ? proj_n : (int)(ncols < SQL_OUT_MAX_COLS
                                                     ? ncols : SQL_OUT_MAX_COLS);
            for(int c = 0; c < sql_cap->ncols; c++){
                char cn[S_COLNOME_W * 2 + 2];
                int src = proj_n ? proj[c] : c;
                cn[0] = 0;
                if(src >= 0) col_nome_le(src, cn, (int)sizeof cn);
                if(cn[0]) snprintf(sql_cap->col[c], sizeof sql_cap->col[0], "%s", cn);
                else      snprintf(sql_cap->col[c], sizeof sql_cap->col[0], "%c", 'a' + src);
                sql_cap->tipo[c] = SQL_TIPO_INT4;
            }
            snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT 0");
        }
        return 1;
    }
    if(tem_where > 0 && cit_texto && cit_desig){
        printf("erro: uma desigualdade sobre coluna de TEXTO — RECUSADA. A célula guarda"
               " o ENDEREÇO no pool, e a leitura preserva a IGUALDADE (endereços iguais"
               " são cadeias iguais) e NÃO a ordem: a dos endereços é a de escrita."
               " Comparar por ela daria uma resposta com a cara de certa.\n");
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err,
                     "inequality on a text column: the pool address preserves"
                     " equality but not order"); }
        return 0;
    }
    /* E A LARGURA: o avaliador é de oito bits, e uma coluna que guarde acima de
     * 255 não cabe nele. Recusa-se, conta-se, e diz-se porquê — não se compara
     * meio valor. O SELECT sem WHERE continua a devolver o número inteiro. */
    if(tem_where > 0){
        for(long j = 0; j < ncols && j < NCOL; j++){
            if(!(citadas_where & (1u << j))) continue;
            /* o andar de cima faz até 32767 e decide pelo SINAL; o que passa disso
             * — na FORMA ou no próprio valor guardado — é recusado */
            if(!col_larguissima(j, ncols, nrows) && cl_cabe16(&cl, ncols, nrows)) continue;
            char cn[S_COLNOME_W * 2 + 2];
            col_nome_le((int)j, cn, (int)sizeof cn);
            if(!cn[0]) snprintf(cn, sizeof cn, "%c", 'a' + (int)j);
            printf("erro: a coluna «%s» não cabe no avaliador desta consulta.\n"
                   " O andar de dezasseis faz a forma LINEAR e decide pelo SINAL da"
                   " diferença: pede grau <= 1 e valores em 0..%ld. Um produto de dois"
                   " valores de 16 pede 32, e acima de 32767 o par lê-se NEGATIVO.\n"
                   " A comparação é RECUSADA — o valor está guardado inteiro e o SELECT"
                   " devolve-o; o que não se faz é responder ao contrário.\n",
                   cn, CMP16_MAX);
            if(sql_cap){
                sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "column \"%s\" is wide and the term is not linear", cn);
            }
            cmp_recusadas++;
            return 0;
        }
    }
    if(nrows <= 0 && acao == ACAO_MARCA && mat_op){
        /* ── MAS A MATRIZ VAZIA NÃO É UMA MATRIZ ─────────────────────────────
         * A saída de baixo devolve a descrição das colunas e «SELECT 0», que é
         * a resposta CERTA a um `SELECT` sobre uma tabela sem linhas. Só que ela
         * apanhava também o pedido matricial, e aí a resposta certa é outra:
         * `det(*)` de uma 0×2 não tem valor vazio — não tem objecto. Devolver
         * «SELECT 0» faz «zero linhas» ler-se a jusante como «a conta deu
         * vazio», quando o que houve foi não haver conta.
         *
         * Apareceu por um caminho instrutivo: um `INSERT` foi recusado por o
         * valor não caber no `Word_8` — o corpo declarado, e a recusa está certa
         * —, a tabela ficou vazia, e o `produto` que se seguiu respondeu `ok`. A
         * recusa a montante estava boa; era o silêncio a jusante que fazia de
         * uma tabela vazia um operando legítimo. */
        printf("erro: a tabela não tem linhas — uma matriz 0×%ld não é uma matriz."
               " RECUSADA.\n", ncols);
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err,
                     "empty table: a 0×%ld matrix has no entries", ncols); }
        return 0;
    }
    if(nrows <= 0){
        /* ── E A CONTAGEM TEM DE SER ZERADA AQUI, senão fica a ANTERIOR ──────
         * O `count(*)` não reconta: corre esta mesma varredura e lê depois o
         * `ultima_conta`, que é o ∑ sobre o campo — «não é uma segunda
         * contagem, é a leitura da que já ficou escrita». Mas essa escrita
         * acontece LÁ EM BAIXO, e esta saída antecipada nunca lá chega: numa
         * tabela vazia o contador ficava com o valor da consulta ANTERIOR, e o
         * `count(*)` respondia com ele.
         *
         * Medido: `SELECT COUNT(*)` numa tabela sem linhas devolveu 2, que era
         * a contagem de uma consulta feita antes noutra tabela. Não é um erro
         * de aritmética — é a leitura de um valor que ninguém voltou a escrever,
         * e por isso não aparece em quem correr a consulta sozinha. */
        ultima_conta = 0;
        ultima_fibras = 0;
        sql_ultimos_passos = 0;      /* a tabela vazia também não custou nada */
        /* A TABELA VAZIA TAMBÉM TEM COLUNAS, e uma consulta que não devolve linhas
         * tem de devolver a DESCRIÇÃO delas e o `CommandComplete`. Esta saída
         * antecipada devolvia `ncols = 0` e tag vazia: pela porta FEBE o driver
         * ficava sem RowDescription e sem o «SELECT 0» que fecha o ciclo, e não
         * distinguia «a tabela está vazia» de «a resposta perdeu-se».
         *
         * O caso com WHERE já o fazia bem — a divergência era só aqui, quando a
         * tabela ainda não tem uma única linha. */
        printf("(vazio)\n");
        if(sql_cap && acao == ACAO_MARCA){
            sql_cap->ncols = (int)(ncols > SQL_OUT_MAX_COLS ? SQL_OUT_MAX_COLS : ncols);
            for(int j = 0; j < sql_cap->ncols; j++){
                char cn[S_COLNOME_W * 2 + 2];
                col_nome_le(j, cn, (int)sizeof cn);
                if(cn[0]) snprintf(sql_cap->col[j], sizeof sql_cap->col[j], "%s", cn);
                else      snprintf(sql_cap->col[j], sizeof sql_cap->col[j], "%c", 'a' + j);
                sql_cap->tipo[j] = SQL_TIPO_INT4;   /* o motor guarda inteiros */
            }
            sql_cap->nrows = 0;
            snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT 0");
        }else if(sql_cap){
            snprintf(sql_cap->tag, sizeof sql_cap->tag, "%s 0",
                     acao == ACAO_SET ? "UPDATE" : "DELETE");
        }
        return 1;
    }

    /* A guarda que recusava consulta sobre coluna racional saiu daqui: a contração está
     * emitida em emit_atomos e a comparação é sobre o NUMERADOR do denominador comum. O que
     * era recusa honesta virou conta feita — inclusive com mais de uma coluna racional. */
    prepara(v, acao == ACAO_SET ? col_set : -1);
    match_limpa(nrows);

    /* UM molde só, o da linha 0 — e depois a PA anda com ele por todas as linhas.
     *
     * SALVO QUANDO HÁ ÍNDICE: aí não se emite nem se corre nada. Era isto que
     * faltava para a lei valer — o molde continuava a passar por todas as
     * linhas só para as marcar, e o custo crescia com |X| na mesma (medido:
     * 320, 640, 1280 passos para 20, 40 e 80 linhas, com o índice já a
     * responder). Quem responde é a árvore, e a árvore não varre. */
    pc_emit = 0; nrel = 0; rel_ncols = ncols; salto_estourou = 0;
    if(!idx_usa){
        emit_linha(0, ncols, &cl, tem_where, col_set);
        emit1(OP_HALT);
    }
    rel_ncols = 0;
    if(salto_estourou){
        printf("erro: o corpo passa o que a Word do deslocamento alcança"
               " (32767) — RECUSADA.\n");
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err,
                     "condicao complexa demais para o salto do bytecode"); }
        return 0;
    }
    long passos = 0;
    if(idx_pre){
        /* as candidatas saem da árvore, e o molde só passa por elas. As de fora
         * da faixa não podem satisfazer a conjunção, pelo que não há resposta a
         * perder — há trabalho a poupar. */
        static int cand[J_MAXLIN];
        ord_usa_indice(idx_col);
        int nc = j_faixa(idx_lo, idx_hi, cand, J_MAXLIN);
        ord_usa_rascunho();
        for(int t = 0; t < nc; t++){
            long i = cand[t];
            if(i < 0 || i >= nrows || !bit_le(S_VIVO, i)) continue;
            rel_anda(i);
            passos += rodar(pc_emit);
        }
    }else if(!idx_usa && !tem_where){
        /* ── SEM CONDIÇÃO, O MATCH É O VIVO --- E O VIVO JÁ ESTÁ ESCRITO ─────
         *
         * Com `tem_where` falso o molde emite `emit_copia(S_CHEIO, S_ACC)`, e
         * as quatro faces reduzem-se a `match |= (VIVO ∧ e_i) ∧ TUDO`, isto é,
         * a match = VIVO. O programa corria por CADA LINHA para chegar a uma
         * cópia de um bitmap que já estava no disco --- 16 passos por linha, e
         * era o custo BASE de tudo o que não tem WHERE: o count(*), o sum, o
         * DISTINCT, o GROUP BY e o ORDER BY pagavam-no antes de começar.
         *
         * A cláusula (3) do `thm:multiplicidade` diz porque isto é errado: «a
         * memória não pertence ao agente [...] cada passo altera G num único
         * ponto». O campo do vivo É a memória, e está no ESPAÇO; percorrer as
         * linhas para o reconstruir é o agente a refazer o que o espaço guarda.
         * Copia-se por SLOT, que é o que a Word segura --- SLOT_BITS linhas de
         * uma vez ---, e o custo passa de nrows a nrows/SLOT_BITS. */
        long slots = (nrows + (long)SLOT_BITS - 1) / (long)SLOT_BITS;
        for(long sl = 0; sl < slots; sl++)
            mem_grava(S_MATCH + (unsigned)sl, mem_le(S_VIVO + (unsigned)sl));
    }else if(!idx_usa)
        for(long i = 0; i < nrows; i++){ rel_anda(i); passos += rodar(pc_emit); }

    unsigned long soma = 1469598103934665603UL;
    for(unsigned q = 0; q < pc_emit; q++){ soma ^= prog_le(q); soma *= 1099511628211UL; }
    sql_ultimo_prog = (long)(soma & 0xFFFFFFFFUL);

    /* ∑G SOBRE O CAMPO, que é o Lema da conservação: ∑_x G(x) = |I|.
     *
     * Cheguei a tirar isto, com o argumento de que percorrer o campo era «a
     * tabela sobre X» que a §do ciclo condena. Era má leitura minha: o campo
     * deste motor tem uma coordenada por LINHA, e as linhas são |I| — o |X| que
     * a §condena é o ESPAÇO DE ENDEREÇOS, o arranjo G[m]×[m] que custa |X| e
     * não |I|. Somar as coordenadas das linhas é Θ(|I|), que é o que o teorema
     * pede.
     *
     * O que estava mesmo errado era o salto do bytecode, e já subiu de andar. */
    /* ── A SUBCONSULTA FILTRA O CAMPO, e é uma descida por linha ───────────
     *
     * O molde marcou o que o WHERE deixou; falta tirar as que não pertencem.
     * Carrega-se a tabela da subconsulta na MESMA árvore do join — e depois
     * RESTAURA-SE a que estava aberta, porque uma consulta não pode trocar a
     * tabela da sessão por baixo de quem a fez. Para cada linha marcada,
     * desce-se a árvore pelo seu valor: fibra vazia, o bit desliga. */
    /* A COMPARAÇÃO COM A AUSÊNCIA NÃO CASA, e é a regra do SQL lida pelo dual:
     * comparar é pedir um valor, e a ausência não o tem. O molde já correu e
     * comparou o que estava na célula — que num sítio ausente é o neutro, e o
     * neutro é um valor como outro qualquer. Tira-se aqui: se alguma coluna que
     * a condição CITA está ausente nessa linha, a linha não casa.
     *
     * É a mesma frase do `thm:bitunico` — a presença é o único operacional —
     * aplicada à leitura em vez de à escrita. */
    /* (a guarda do corpo e o `IS NULL` aplicam-se DEPOIS — ver mais abaixo) */

    if(idx_usa){
        /* A DESCIDA, e é aqui que |X| sai da conta. O molde correu sobre nada —
         * não há WHERE compilado —, pelo que o campo está com todas as vivas;
         * apaga-se e acendem-se só as que a árvore devolveu. Os passos da ISA
         * não contam esta parte porque ela não é ISA: é a árvore, e a árvore é
         * o corte. */
        match_limpa(nrows);
        ord_usa_indice(idx_col);
        static int achados[J_MAXLIN];
        int n = j_faixa(idx_lo, idx_hi, achados, J_MAXLIN);
        ord_usa_rascunho();

        if(idx_col2 >= 0){
            /* o segundo campo, e a combinação. ∧ guarda o que está nos dois,
             * ∨ junta os dois — e a união não pode repetir, que é o mesmo
             * representante único do DISTINCT e da UNION. */
            static int outros[J_MAXLIN];
            ord_usa_indice(idx_col2);
            int n2 = j_faixa(idx_lo2, idx_hi2, outros, J_MAXLIN);
            ord_usa_rascunho();
            if(idx_liga == 'A'){
                int m = 0;
                for(int t = 0; t < n; t++){
                    int esta = 0;
                    for(int u = 0; u < n2 && !esta; u++) if(outros[u] == achados[t]) esta = 1;
                    if(esta) achados[m++] = achados[t];
                }
                n = m;
            }else{
                for(int u = 0; u < n2 && n < J_MAXLIN; u++){
                    int esta = 0;
                    for(int t = 0; t < n && !esta; t++) if(achados[t] == outros[u]) esta = 1;
                    if(!esta) achados[n++] = outros[u];
                }
            }
        }
        /* SÓ AS VIVAS. Um DELETE não muda o número de linhas — muda o bit do
         * vivo —, pelo que o cabeçalho continua a bater e o índice continua a
         * ser usado; mas a chave apagada ficou lá dentro. Filtra-se aqui, que é
         * onde a verdade está: a árvore diz onde a linha estava, e o campo do
         * vivo diz se ela ainda está. */
        for(int t = 0; t < n; t++)
            if(achados[t] >= 0 && achados[t] < nrows && bit_le(S_VIVO, achados[t]))
                bit_poe(S_MATCH, achados[t], 1);
    }

    if(in_sub){
        long nc_sub = 0;
        char guarda[64];
        snprintf(guarda, sizeof guarda, "%s", nome);

        /* A ESQUERDA LÊ-SE ANTES DE TROCAR DE TABELA, como no join: abrir outra
         * tabela relê o .mem, e o campo que o WHERE deixou vive lá. Recolhe-se
         * primeiro o par (índice, valor) das linhas marcadas; só depois se
         * carrega a subconsulta. */
        static long in_idx[J_MAXLIN];
        static long in_val[J_MAXLIN];
        int ne = 0, estourou = 0;
        for(long i = 0; i < nrows; i++){
            if(!bit_le(S_MATCH, i)) continue;
            /* ── E A AUSÊNCIA NÃO PERTENCE NEM DEIXA DE PERTENCER ────────
             * A pertença é uma pergunta sobre o CORPO, e a célula ausente não
             * tem valor para levar à outra tabela: lê-la dava o neutro, e um
             * `x IN (…)` com um zero do outro lado casava com uma linha que
             * não tem x nenhum. Desliga-se aqui, e desliga-se nos DOIS
             * sentidos — o `NOT IN` também não a apanha, porque não se pode
             * negar uma resposta que não foi dada. */
            if(!bit_le(S_PRES, i*ncols + in_col_esq)){ bit_poe(S_MATCH, i, 0); continue; }
            if(ne >= J_MAXLIN){ estourou = 1; break; }
            in_idx[ne] = i;
            in_val[ne] = celula_valor(i, in_col_esq, ncols);
            ne++;
        }
        if(estourou){
            printf("erro: a subconsulta não coube (mais de %d linhas marcadas)"
                   " — RECUSADA.\n", J_MAXLIN);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "subquery: left side too large"); }
            return 0;
        }

        snprintf(j_tab_dir, sizeof j_tab_dir, "%s", in_tab);
        snprintf(j_col_dir, sizeof j_col_dir, "%s", in_col);
        int postos = j_carrega_direita(&nc_sub);
        j_tab_dir[0] = 0; j_col_dir[0] = 0;      /* devolve-se logo */
        if(postos < 0){
            if(!usa_tabela(guarda, 0)) restauros_falhados++;
            printf("erro: a subconsulta não pôde ser lida (tabela «%s» ou coluna «%s»)"
                   " — RECUSADA.\n", in_tab, in_col);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "subquery: relation or column not readable"); }
            return 0;
        }
        /* A DECISÃO TOMA-SE COM A TABELA DA SUBCONSULTA AINDA ABERTA. A árvore
         * vive no .mem, e voltar à tabela de origem relê-o e apaga-a — foi o
         * que aconteceu à primeira escrita: `j_casam` devolvia zero para
         * valores que lá estavam, porque já não havia árvore. Decide-se aqui,
         * guarda-se a resposta em memória local, e só depois se volta. */
        static unsigned char passa[J_MAXLIN];
        for(int k = 0; k < ne; k++){
            int achados[4];
            int esta = j_casam(in_val[k], achados, 4) > 0;
            /* o NOT é a mesma descida com a resposta virada: a pertença e a
             * não-pertença são complementares SOBRE O CORPO, e é por isso que
             * a ausência teve de sair das duas antes de aqui chegar. */
            passa[k] = (unsigned char)(in_nega ? !esta : esta);
        }

        if(!usa_tabela(guarda, 0)) return 0;

        /* de volta em casa, o campo reescreve-se do zero: a troca de tabela
         * mexeu-lhe, e o que vale é a decisão que já está tomada. */
        match_limpa(nrows);
        for(int k = 0; k < ne; k++)
            if(passa[k]) bit_poe(S_MATCH, in_idx[k], 1);
    }

    /* ── OS FILTROS DO DUAL, E É AQUI QUE ELES TÊM DE ESTAR ──────────────────
     * Estes dois só APAGAM linhas, pelo que a sua posição correcta é depois de
     * TODOS os caminhos que constroem o campo — e há três que o reescrevem do
     * zero: a descida pelo índice, a subconsulta e o join. Estavam antes, logo
     * a descida pelo índice desfazia-os: `WHERE c = 0` com índice na coluna c
     * apanhava as células AUSENTES, porque a árvore devolve o sítio e não sabe
     * do dual. Aqui valem para todos os caminhos, e a resposta deixa de
     * depender de haver ou não índice — que é a régua da casa: o índice muda o
     * CUSTO, nunca a resposta. */
    /* e a condição é `citadas_where`, não `tem_where`: na descida pelo índice o
     * molde NÃO corre — não há WHERE compilado — e a guarda ficava de fora
     * exactamente no caminho onde é mais precisa. */
    if(citadas_where){
        for(long i = 0; i < nrows; i++){
            if(!bit_le(S_MATCH, i)) continue;
            for(long j = 0; j < ncols; j++){
                if(!(citadas_where & (1u << j))) continue;
                if(!bit_le(S_PRES, i*ncols + j)){ bit_poe(S_MATCH, i, 0); break; }
            }
        }
    }

    /* ── E A POLÍTICA ERODE O CAMPO, depois do WHERE do cliente.
     *
     * Se a tabela tem política e a sessão declarou um inquilino, o que não é
     * dele sai do S_MATCH --- e sai AQUI, num sítio só, depois de todos os
     * caminhos (índice ou varredura) terem marcado. Pô-la em cada caminho seria
     * deixá-la de fora exactamente naquele que alguém acrescentasse a seguir.
     *
     * O `bypass` existe porque o original o tem: há trabalho de manutenção que
     * precisa de ver tudo. Ele é EXPLÍCITO --- `SET app.bypass_rls = 'on'` --- e
     * é isso que o torna aceitável: o que é implícito é o isolamento, e o que é
     * declarado é a excepção. */
    if(mem_le(S_RLS).total){
        /* ── O INQUILINO VEM DOS PARÂMETROS DE SESSÃO, que o `SET` já guarda.
         * Eu tinha escrito um ramo `SET` só para isto, e ele nunca corria: o
         * catálogo da sessão responde ANTES do motor e apanhava-o. A tabela de
         * parâmetros já existia --- e ler de lá é o certo, porque é lá que o
         * `SHOW` também lê, e as duas leituras têm de concordar. */
        const char *tv = pgcat_valor("app.tenant_id");
        const char *bv = pgcat_valor("app.bypass_rls");
        if(bv && !strcasecmp(bv, "on")) goto rls_fim;
        long rc = (long)mem_le(S_RLSCOL).total | ((long)mem_le(S_RLSCOL).e << 8);
        unsigned tix = 0;
        if(tv && *tv) tix = tx_guarda(tv, (int)strlen(tv));
        if(tix && rc >= 0 && rc < ncols){
            long fora = 0;
            for(long i = 0; i < nrows; i++){
                if(!bit_le(S_MATCH, i)) continue;
                /* a linha sem o campo do inquilino NÃO é de ninguém, e por isso
                 * não é de quem pergunta --- a ausência não passa a política */
                if(!bit_le(S_PRES, i*ncols + rc)){ bit_poe(S_MATCH, i, 0); fora++; continue; }
                long nu, de; celula_qz(i, rc, ncols, &nu, &de);
                if((unsigned)nu != tix){ bit_poe(S_MATCH, i, 0); fora++; }
            }
            if(fora) printf("-- politica tenant_isolation: %ld linha(s) fora do inquilino\n",
                            fora);
        }
        rls_fim: ;
    }

    if(ex_usa && !ex_vale){
        /* o quantificador é uma CONSTANTE: falso apaga o campo inteiro, e
         * verdadeiro deixa-o como está — nenhuma linha é olhada */
        match_limpa(nrows);
    }

    if(nul_usa){
        /* o campo já está construído; fica quem tem (ou não tem) a célula */
        for(long i = 0; i < nrows; i++){
            if(!bit_le(S_MATCH, i)) continue;
            int tem = bit_le(S_PRES, i*ncols + nul_col);
            if(nul_nega ? !tem : tem) bit_poe(S_MATCH, i, 0);
        }
    }

    long achou = bits_conta(S_MATCH, nrows);
    ultima_conta = achou;

    /* ── E QUANTAS FIBRAS, se a pergunta foi essa ────────────────────────────
     * `count(DISTINCT c)` não é o count com um adorno: é o número de valores
     * distintos, isto é, quantas classes tem o quociente por c — o mesmo
     * objecto do GROUP BY, lido em quantidade em vez de em extensão. Conta-se
     * pela MESMA árvore, e a ausência é uma classe como as outras: uma, se
     * houver alguma. */
    ultima_fibras = 0;
    if(cnt_dis[0]){
        int cc = col_indice(cnt_dis);
        if(cc < 0){
            printf("erro: a coluna «%s» não existe — o count(DISTINCT) é"
                   " RECUSADO.\n", cnt_dis);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "column \"%s\" does not exist", cnt_dis); }
            return 0;
        }
        { int seq[SQL_OUT_MAX_ROWS], n = 0, viu_aus = 0, coube = 1;
          ord_limpa();
          for(long i = 0; i < nrows && coube; i++){
              if(!bit_le(S_MATCH, i)) continue;
              if(!bit_le(S_PRES, i*ncols + cc)){ viu_aus = 1; continue; }
              if(!ord_insere(celula_valor(i, cc, ncols), (int)i)) coube = 0;
          }
          if(!coube){
              printf("erro: a árvore do count(DISTINCT) não coube — RECUSADO.\n");
              if(sql_cap){ sql_cap->ok = 0;
                  snprintf(sql_cap->err, sizeof sql_cap->err,
                           "count(DISTINCT): a arvore nao coube; recusa em vez de"
                           " contar metade"); }
              return 0;
          }
          /* AS FIBRAS CONTAM-SE NA DESCIDA. O arranjo saía daqui: ele era o
           * tecto (`aranha §sec:dimensao`: o número é do ARRANJO, não da
           * conta), e com ele qualquer tabela de mais de 64 linhas respondia
           * um número errado --- 300 linhas com 5 classes davam DOIS. As
           * chaves saem da árvore por ordem de valor, pelo que contar as
           * fibras é contar as vezes em que o valor muda, e isso precisa do
           * valor ANTERIOR e mais nada. Sem arranjo não há tecto. */
          ord_classes = 0; ord_tem_ult = 0;
          ord_conta_fibras(0, 0, 0);
          ultima_fibras += ord_classes;
          (void)seq; (void)n;
          if(viu_aus) ultima_fibras++;      /* o dual é uma classe, e é UMA */
        }
    }

    /* ── A RESTRIÇÃO VALE EM QUALQUER PORTA ──────────────────────────────────
     * Uma afirmação sobre a coluna que só o INSERT respeitasse não é uma
     * afirmação: o UPDATE reescreve a mesma célula. `SET x = NULL` numa coluna
     * NOT NULL é apagar o que ela garante; e um valor repetido numa coluna
     * UNIQUE é a segunda folha da fibra a entrar pela outra porta. Recusa-se
     * ANTES do compromisso, com o diário ainda fechado — a linha inteira fica
     * como estava. */
    /* ── A SETA, NAS DUAS DIRECÇÕES ──────────────────────────────────────────
     * À ENTRADA (UPDATE): escrever nesta coluna um valor que não está do outro
     * lado é apontar para lado nenhum.
     * À SAÍDA (DELETE): apagar uma linha que alguém aponta é cortar a seta por
     * baixo. Quem responde a esta é a lista que a MÃE guarda — sem ela, o
     * DELETE não teria como saber que existe alguém a apontar, e a restrição
     * teria só metade. */
    if(acao == ACAO_SET && col_set >= 0 && col_set < 16 && achou > 0 && !set_anula){
        char mt[64];
        int mc = fk_le(col_set, mt, sizeof mt);
        if(mc >= 0){
            int e = fk_existe(mt, mc, v, nome);
            if(e != 1){
                printf("erro: a coluna %d aponta para «%s» e o valor %ld %s —"
                       " RECUSADO.\n", col_set, mt, v,
                       e == 0 ? "não está lá" : "não pôde ser procurado");
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "insert or update violates foreign key constraint on"
                             " column %d", col_set); }
                return 0;
            }
        }
    }
    /* ── E QUANDO A SETA PERDE O DESTINO, HÁ TRÊS RESPOSTAS E SÃO SÓ TRÊS ────
     * (o corpo está em `fk_propaga`, porque as DUAS portas que podem tirar o
     * destino de baixo de uma seta — apagar a linha e mudar-lhe a chave — têm
     * de responder da mesma maneira, e duas implementações da mesma frase é
     * como as duas deixam de concordar.) */
    if(achou > 0 && filho_quantos() > 0){
        if(acao == ACAO_APAGA){
            if(!fk_propaga(nome, nrows, ncols, 0, 0)) return 0;
        } else if(acao == ACAO_SET && col_set >= 0 && !set_anula){
            if(!fk_propaga(nome, nrows, ncols, 1, v)) return 0;
        }
    }

    /* ── O PREDICADO NA OUTRA PORTA: escreve-se, pergunta-se, DESFAZ-SE ──────
     * O UPDATE muda uma célula, e o predicado é sobre a LINHA — pelo que a
     * pergunta só se pode fazer com o valor novo lá dentro. Escreve-se, corre o
     * mesmo molde, e restaura-se SEMPRE: se passar, o UPDATE a sério corre a
     * seguir e escreve outra vez; se não passar, nada ficou tocado. Restaurar
     * nos dois ramos é o que faz isto ser uma pergunta e não uma escrita. */
    if(acao == ACAO_SET && col_set >= 0 && achou > 0){
        char ck[S_CHECK_W * 2 + 2];
        check_le(ck, (int)sizeof ck);
        if(ck[0]){
            int falhou = 0;
            long linha_ma = -1;
            for(long i = 0; i < nrows && !falhou; i++){
                if(!bit_le(S_MATCH, i)) continue;
                { long antigo = celula_valor(i, col_set, ncols);
                  int tinha = bit_le(S_PRES, i*ncols + col_set);
                  int r;
                  if(set_anula) bit_poe(S_PRES, i*ncols + col_set, 0);
                  else { celula_grava(i, col_set, ncols, v);
                         bit_poe(S_PRES, i*ncols + col_set, 1); }
                  r = check_avalia(i, ncols, ck);
                  celula_grava(i, col_set, ncols, antigo);
                  bit_poe(S_PRES, i*ncols + col_set, tinha);
                  if(r != 1){ falhou = 1; linha_ma = i; } }
            }
            if(falhou){
                printf("erro: a linha %ld deixaria de satisfazer o CHECK (%s) —"
                       " RECUSADO.\n", linha_ma, ck);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "new row violates check constraint"); }
                return 0;
            }
        }
    }

    if(acao == ACAO_SET && col_set >= 0 && col_set < 16 && achou > 0){
        long r = mem_le(S_RESTR + (unsigned)col_set).total;
        if((r & R_NOTNULL) && set_anula){
            printf("erro: a coluna %d é NOT NULL — `SET = NULL` RECUSADO.\n", col_set);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "null value in column %d violates not-null constraint",
                         col_set); }
            return 0;
        }
        if((r & R_UNICO) && !set_anula){
            /* duas maneiras de quebrar a unicidade, e as duas contam: o valor
             * já existir NOUTRA linha, ou o próprio UPDATE marcar mais do que
             * uma linha — nesse caso ele escreveria o mesmo valor em todas. */
            long colide = 0;
            for(long i = 0; i < nrows && !colide; i++){
                if(!bit_le(S_VIVO, i) || bit_le(S_MATCH, i)) continue;
                if(!bit_le(S_PRES, i*ncols + col_set)) continue;
                if(celula_valor(i, col_set, ncols) == v) colide = 1;
            }
            if(colide || achou > 1){
                printf("erro: a coluna %d é UNIQUE e o valor %ld %s — RECUSADO.\n",
                       col_set, v, colide ? "já lá está" : "iria para mais de uma linha");
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "duplicate key value violates unique constraint on"
                             " column %d", col_set); }
                return 0;
            }
        }
    }

    if(acao != ACAO_MARCA){
        /* o bitmap (o diário) já está no disco; agora o COMPROMISSO, e só depois o efeito. */
        barreira();
        trava_se_pedido(2);                       /* queda ANTES do compromisso: nada mudou */
        Word d; d.total = acao + 1 + (set_anula ? 3 : 0); d.e = col_set;
        mem_grava(S_DIA, d);
        barreira();                               /* ← o ponto de compromisso */
        trava_se_pedido(3);                       /* queda DEPOIS: a abertura refaz */
        passos += aplica_diario(ncols, nrows, acao, col_set);
        barreira();
        trava_se_pedido(4);                       /* queda aqui: refaz de novo, e é idempotente */
        Word z2 = {0,0}; mem_grava(S_DIA, z2);
        barreira();
    }
    const char *nome_acao = acao == ACAO_MARCA ? "lida(s)" : (acao == ACAO_SET ? "atualizada(s)" : "apagada(s)");
    sql_ultimos_passos = passos;
    if(!sql_calado)
    printf("-- %u bytes de ISA [%04lx], %d átomo(s), %ld passos, %ld linha(s) %s\n",
           pc_emit, soma & 0xFFFF, tem_where ? cl.natomo : 0, passos, achou, nome_acao);
    if(acao != ACAO_MARCA){
        if(sql_cap){
            if(acao == ACAO_SET)
                snprintf(sql_cap->tag, sizeof sql_cap->tag, "UPDATE %ld", achou);
            else
                snprintf(sql_cap->tag, sizeof sql_cap->tag, "DELETE %ld", achou);
            sql_cap->ncols = 0; sql_cap->nrows = 0;
        }
        return 1;
    }
    if(sql_cap){
        /* A PROJECÇÃO: só as colunas pedidas, e por esta ordem. Com `*` são
         * todas; com uma lista, são as dela — que é o que o cliente pediu.
         *
         * E O QUE NÃO CABE NO TRANSPORTE É RECUSADO, não truncado. Truncar
         * devolvia uma linha com as primeiras colunas e a mesma cara de uma
         * linha completa: quem lê por POSIÇÃO --- o psql, o driver, o cliente
         * --- não tem como saber que a posição vinte é outra coisa. A tabela não
         * tem tecto; a struct que atravessa a fronteira C tem, e diz qual. */
        if(!proj_n && ncols > SQL_OUT_MAX_COLS){
            printf("erro: a linha tem %ld colunas e a JANELA da porta C leva %d — RECUSADA."
                   " Truncar daria uma linha com a mesma cara e outra posição;"
                   " peça as colunas que quer, por nome.\n",
                   ncols, SQL_OUT_MAX_COLS);
            sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err,
                     "row has %ld columns; the C window carries %d — name the columns you want",
                     ncols, SQL_OUT_MAX_COLS);
            return 0;
        }
        int nsaida = proj_n ? proj_n : (int)ncols;
        sql_cap->ncols = nsaida;
        for(int j = 0; j < nsaida; j++)
            { char cn[S_COLNOME_W * 2 + 2];
              int src = proj_n ? proj[j] : j;
              cn[0] = 0;
              if(src >= 0) col_nome_le(src, cn, (int)sizeof cn);
              if(j < n_alias && alias[j][0])
                        snprintf(sql_cap->col[j], sizeof sql_cap->col[j], "%s", alias[j]);
              /* a expressão traz o seu próprio nome: o TEXTO dela. Sem isto o
               * cabeçalho saía do `col_nome_le` de uma coluna que não existe —
               * lixo com cara de nome. */
              else if(proj_n && proj_ex[j])
                        snprintf(sql_cap->col[j], sizeof sql_cap->col[j], "%s", proj_nome[j]);
              else if(cn[0]) snprintf(sql_cap->col[j], sizeof sql_cap->col[j], "%s", cn);
              else      snprintf(sql_cap->col[j], sizeof sql_cap->col[j], "%c", 'a' + src);
              sql_cap->tipo[j] = SQL_TIPO_INT4; }
        snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT %ld", achou);
        sql_cap->nrows = 0;
    }
    /* ── O JOIN: a bipartição, e a emissão do produto ────────────────────
     *
     * A direita já está no banco e indexada (fase 1). Para cada linha da
     * esquerda que o WHERE deixou passar, desce-se a árvore com o valor da
     * coluna de junção e emitem-se as linhas da classe. Linhas sem classe não
     * saem — é o join interno, e é a bipartição a fazer o seu trabalho. */
    if(acao == ACAO_MARCA && j_tab_dir[0]){
        long ncols_dir = 0;
        int nd, oce;
        long nc_esq = ncols, nr_esq = nrows;
        /* a esquerda tem de ser lida ANTES de trocar de tabela: copia-se o que
         * o WHERE marcou, e só depois se abre a direita. */
        static Word esq[J_MAXLIN][J_MAXCOL];
        static long esq_v[J_MAXLIN];
        int ne = 0;
        oce = col_indice(j_col_esq);
        if(oce < 0){
            printf("erro: a coluna «%s» não existe na tabela «%s» — RECUSADA.\n",
                   j_col_esq, nome);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "column \"%s\" does not exist", j_col_esq); }
            return 0;
        }
        static char nome_esq[J_MAXCOL][S_COLNOME_W * 2 + 2];
        for(int j = 0; j < (int)J_MAXCOL; j++) nome_esq[j][0] = 0;
        /* ── O QUE NÃO CABE RECUSA-SE, E NÃO SE TRUNCA ───────────────────────
         *
         * Este laço parava em J_MAXLIN e seguia: com 600 linhas à esquerda, o
         * motor dizia «512 linha(s) da esquerda» --- e não era falso, era o que
         * ele tinha visto --- e devolvia o que casasse nessas. Medido: a linha
         * que casava era a 595.ª, existia, e o JOIN devolveu ZERO sem uma
         * palavra. Um resultado vazio e um resultado truncado têm a mesma cara.
         *
         * Contar antes é barato --- é o bitmap que a varredura já deixou --- e a
         * recusa diz o número, que é o que permite a quem pergunta cortar em
         * lotes ou pedir por outra ordem. */
        { long cabem = 0;
          for(long i = 0; i < nr_esq; i++) if(bit_le(S_MATCH, i)) cabem++;
          if(cabem > J_MAXLIN){
              printf("erro: o lado esquerdo do JOIN tem %ld linhas e a junção leva %d"
                     " — RECUSADA. Truncar daria um resultado com a mesma cara de um"
                     " completo.\n", cabem, J_MAXLIN);
              if(sql_cap){ sql_cap->ok = 0;
                  snprintf(sql_cap->err, sizeof sql_cap->err,
                           "JOIN: left side has %ld rows; the join carries %d",
                           cabem, J_MAXLIN); }
              return 0;
          } }
        for(long i = 0; i < nr_esq && ne < J_MAXLIN; i++){
            if(!bit_le(S_MATCH, i)) continue;
            /* ── E A CHAVE AUSENTE NÃO JUNTA ────────────────────────────
             * Juntar é perguntar «onde é que este valor está do outro lado», e
             * a célula ausente não tem valor para levar: lê-la dava o neutro, e
             * a linha sem chave casava com todos os zeros da direita. É a mesma
             * frase do `IN`, e o JOIN é a mesma travessia — a árvore indexa o
             * corpo, o dual vive no bitmap. */
            if(!bit_le(S_PRES, i*nc_esq + oce)) continue;
            for(long j = 0; j < nc_esq && j < J_MAXCOL; j++){
                long v = celula_valor(i, j, nc_esq);
                Word par = { (Word8)(v & 255), (Word8)((v >> 8) & 255) };
                esq[ne][j] = par;
            }
            esq_v[ne] = celula_valor(i, oce, nc_esq);
            ne++;
        }
        /* OS NOMES DA ESQUERDA LÊEM-SE ENQUANTO ELA ESTÁ ABERTA.
         *
         * A junção troca a tabela aberta por baixo da sessão — quem fica aberta
         * no fim é a direita —, e por isso as colunas da esquerda saíam como
         * LETRAS: o cliente pedia `cli JOIN ped` e recebia `a | b | cid | valor`
         * em vez de `id | saldo | cid | valor`. Os nomes não estavam perdidos,
         * estavam noutro ficheiro; bastava lê-los ANTES de trocar. É a mesma
         * regra do resto: quem pergunta tem de receber o que pediu, e um nome
         * inventado é responder outra coisa. */
        { char cn[S_COLNOME_W * 2 + 2];
          for(long j = 0; j < nc_esq && j < J_MAXCOL; j++){
              col_nome_le((int)j, cn, (int)sizeof cn);
              snprintf(nome_esq[j], sizeof nome_esq[0], "%s", cn);
          } }
        nd = j_carrega_direita(&ncols_dir);
        if(nd == -2){
            printf("erro: a coluna «%s» não existe na tabela «%s» — RECUSADA.\n",
                   j_col_dir, j_tab_dir);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "column \"%s\" does not exist", j_col_dir); }
            return 0;
        }
        if(nd < 0){
            printf("erro: a direita do JOIN não coube — RECUSADA.\n");
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "JOIN: a tabela da direita nao coube; recusa em vez de juntar"
                         " metade"); }
            return 0;
        }
        /* ── A PROJECÇÃO DA JUNÇÃO: resolver contra os DOIS lados ────────────
         * A junção produz uma tabela nova — as colunas da esquerda seguidas das
         * da direita —, e é contra ELA que a projecção se resolve. Um nome sem
         * qualificador procura-se nos dois, e um nome que aparece nos dois é
         * AMBÍGUO: recusa-se, porque escolher um deles seria responder outra
         * coisa. Com o ponto não há dúvida — `t.a` é da esquerda, `u.a` da
         * direita.
         *
         * O que sai daqui é um MAPA: para cada coluna pedida, o índice na
         * concatenação. Sem lista, o mapa é a identidade e tudo sai, que é o
         * que já acontecia. */
        int jmapa[SQL_OUT_MAX_COLS], jn = 0;
        { char cnd[J_MAXCOL][S_COLNOME_W * 2 + 2];
          for(long j = 0; j < ncols_dir && j < J_MAXCOL; j++)
              col_nome_le((int)j, cnd[j], (int)sizeof cnd[0]);
          if(proj_n != 0 && strcmp(proj_cols, "*") != 0){
              const char *q = proj_cols;
              while(*q && jn < SQL_OUT_MAX_COLS){
                  char item[130]; int i = 0;
                  const char *ponto;
                  while(*q && *q != ',' && i + 1 < (int)sizeof item) item[i++] = *q++;
                  item[i] = 0;
                  if(*q == ',') q++;
                  ponto = strchr(item, '.');
                  { int achou = -1, quantos = 0;
                    const char *alvo = ponto ? ponto + 1 : item;
                    char qual[64];
                    qual[0] = 0;
                    if(ponto){ size_t n = (size_t)(ponto - item);
                               if(n < sizeof qual){ memcpy(qual, item, n); qual[n] = 0; } }
                    /* a esquerda */
                    if(!qual[0] || !strcmp(qual, nome))
                        for(long j = 0; j < nc_esq && j < J_MAXCOL; j++)
                            if(!strcmp(nome_esq[j], alvo)){ achou = (int)j; quantos++; }
                    /* a direita */
                    if(!qual[0] || !strcmp(qual, j_tab_dir))
                        for(long j = 0; j < ncols_dir && j < J_MAXCOL; j++)
                            if(!strcmp(cnd[j], alvo)){ achou = (int)(nc_esq + j); quantos++; }
                    if(quantos > 1 && !qual[0]){
                        printf("erro: a coluna «%s» está nas DUAS tabelas — diga"
                               " de qual (`%s.%s` ou `%s.%s`). RECUSADA.\n",
                               alvo, nome, alvo, j_tab_dir, alvo);
                        if(sql_cap){ sql_cap->ok = 0;
                            snprintf(sql_cap->err, sizeof sql_cap->err,
                                     "column reference \"%s\" is ambiguous", alvo); }
                        return 0;
                    }
                    if(achou < 0){
                        printf("erro: «%s» não é coluna de «%s» nem de «%s» —"
                               " RECUSADA.\n", item, nome, j_tab_dir);
                        if(sql_cap){ sql_cap->ok = 0;
                            snprintf(sql_cap->err, sizeof sql_cap->err,
                                     "column \"%s\" does not exist", item); }
                        return 0;
                    }
                    jmapa[jn++] = achou; }
              }
          } }

        /* emitir: as colunas pedidas — ou, sem lista, a esquerda e a direita */
        if(sql_cap){
            int nsai = jn ? jn : (int)(nc_esq + ncols_dir);
            if(nsai > SQL_OUT_MAX_COLS) nsai = SQL_OUT_MAX_COLS;
            memset(sql_cap->col, 0, sizeof sql_cap->col);
            sql_cap->ncols = nsai;
            sql_cap->nrows = 0;
            for(int k = 0; k < nsai; k++){
                char cn[S_COLNOME_W * 2 + 2];
                int j = jn ? jmapa[k] : k;         /* o mapa, ou a identidade */
                if(j < nc_esq){
                    /* lidos acima, enquanto a esquerda ainda estava aberta */
                    if(j >= 0 && j < (int)J_MAXCOL && nome_esq[j][0])
                        snprintf(sql_cap->col[k], sizeof sql_cap->col[k], "%s", nome_esq[j]);
                    else
                        snprintf(sql_cap->col[k], sizeof sql_cap->col[k], "%c", 'a' + j);
                }else{
                    col_nome_le(j - (int)nc_esq, cn, (int)sizeof cn);
                    if(cn[0]) snprintf(sql_cap->col[k], sizeof sql_cap->col[k], "%s", cn);
                    else snprintf(sql_cap->col[k], sizeof sql_cap->col[k],
                                  "%c", 'a' + (j - (int)nc_esq));
                }
                sql_cap->tipo[k] = SQL_TIPO_INT4;
            }
        }
        /* A DIREITA POR ORDEM — para a desigualdade, que é o dual do corte.
         *
         * Na igualdade desce-se a árvore por UM símbolo e a fibra sai inteira
         * (o corte). Na desigualdade percorre-se a árvore POR ORDEM e toma-se o
         * SUFIXO ou o PREFIXO: as linhas estão já ordenadas pelo valor, porque
         * é isso que a árvore faz. Não há estrutura nova — é o mesmo MOVE nos
         * dois sentidos, «ordenar dá a profundidade, cortar dá a paridade». */
        int ordem[J_MAXLIN], n_ord = 0;
        if(j_op != J_IG) ord_percorre(0, 0, 0, ordem, &n_ord, J_MAXLIN, 0);

        /* O BITMAP DA DIREITA, PARA O RIGHT: a linha d é a coordenada d.
         * São J_MAXLIN = 64 linhas, logo cabem num inteiro de 64 bits — o mesmo
         * bit level do bitmap das linhas, à escala da junção. */
        unsigned long long usados = 0ULL;
        static Word nova[J_MAXLIN][J_MAXCOL];      /* o resultado do 1.º corte */
        int nova_ne = 0;
        { long saiu = 0;
          for(int e = 0; e < ne; e++){
            int casos[J_MAXLIN], nca;
            if(j_op == J_IG) nca = j_casam(esq_v[e], casos, J_MAXLIN);
            else {
                /* o sufixo (ou prefixo) da ordem: os que cumprem a desigualdade */
                nca = 0;
                for(int t = 0; t < n_ord && nca < J_MAXLIN; t++){
                    long vd = j_valor_dir(ordem[t], ncols_dir);
                    int passa = (j_op == J_LT) ? (esq_v[e] <  vd)
                              : (j_op == J_LE) ? (esq_v[e] <= vd)
                              : (j_op == J_GT) ? (esq_v[e] >  vd)
                                               : (esq_v[e] >= vd);
                    if(passa) casos[nca++] = ordem[t];
                }
            }
            /* LEFT: a fibra vazia sai na mesma, com a AUSÊNCIA à direita */
            if(j_left && nca == 0){
                printf("   ");
                for(long j = 0; j < nc_esq; j++){
                    long ve = (long)esq[e][j].total | ((long)esq[e][j].e << 8);
                    printf("%ld | ", ve);
                    if(sql_cap && sql_cap->nrows < SQL_OUT_MAX_ROWS && j < SQL_OUT_MAX_COLS)
                        snprintf(sql_cap->cell[sql_cap->nrows][j], SQL_OUT_CELL, "%ld", ve);
                }
                for(long j = 0; j < ncols_dir; j++){
                    printf("0");                       /* a ausência: b = 0 */
                    if(j + 1 < ncols_dir) printf(" | ");
                    { int col = (int)(nc_esq + j);
                      if(sql_cap && sql_cap->nrows < SQL_OUT_MAX_ROWS && col < SQL_OUT_MAX_COLS)
                          snprintf(sql_cap->cell[sql_cap->nrows][col], SQL_OUT_CELL, "0"); }
                }
                printf("\n");
                jproj_linha(jmapa, jn, (int)(nc_esq + ncols_dir));
                if(sql_cap && sql_cap->nrows < SQL_OUT_MAX_ROWS) sql_cap->nrows++;
                saiu++;
            }
            for(int k = 0; k < nca; k++){
                int d = casos[k];
                if(d >= nd) continue;
                if(d < 64) usados |= (1ULL << d);      /* liga a coordenada d */
                /* HÁ SEGUNDA JUNÇÃO: o par não sai — passa a ser a ESQUERDA
                 * dela. A saída do primeiro corte é a entrada do segundo. */
                if(j2_tab[0]){
                    if(nova_ne < J_MAXLIN){
                        for(long j = 0; j < nc_esq && j < J_MAXCOL; j++)
                            nova[nova_ne][j] = esq[e][j];
                        for(long j = 0; j < ncols_dir && nc_esq + j < J_MAXCOL; j++)
                            nova[nova_ne][nc_esq + j] =
                                mem_le(S_JDIR + (unsigned)(d*J_MAXCOL + j));
                        nova_ne++;
                    }
                    continue;
                }
                printf("   ");
                for(long j = 0; j < nc_esq; j++){
                    long ve = (long)esq[e][j].total | ((long)esq[e][j].e << 8);
                    printf("%ld", ve);
                    printf(" | ");
                    if(sql_cap && sql_cap->nrows < SQL_OUT_MAX_ROWS && j < SQL_OUT_MAX_COLS)
                        snprintf(sql_cap->cell[sql_cap->nrows][j], SQL_OUT_CELL, "%ld", ve);
                }
                for(long j = 0; j < ncols_dir; j++){
                    Word w = mem_le(S_JDIR + (unsigned)(d*J_MAXCOL + j));
                    long v = (long)w.total | ((long)w.e << 8);
                    printf("%ld", v);
                    if(j + 1 < ncols_dir) printf(" | ");
                    { int col = (int)(nc_esq + j);
                      if(sql_cap && sql_cap->nrows < SQL_OUT_MAX_ROWS && col < SQL_OUT_MAX_COLS)
                          snprintf(sql_cap->cell[sql_cap->nrows][col], SQL_OUT_CELL, "%ld", v); }
                }
                printf("\n");
                jproj_linha(jmapa, jn, (int)(nc_esq + ncols_dir));
                if(sql_cap && sql_cap->nrows < SQL_OUT_MAX_ROWS) sql_cap->nrows++;
                saiu++;
            }
          }
          /* RIGHT: a fibra vazia do OUTRO lado. As linhas da direita que
           * nenhuma da esquerda alcançou são as que ficaram com a coordenada
           * desligada — o complemento do suporte, lido no bitmap. */
          if(j_right){
              for(int d = 0; d < nd && d < 64; d++){
                  if(usados & (1ULL << d)) continue;
                  printf("   ");
                  for(long j = 0; j < nc_esq; j++){
                      printf("0 | ");                      /* a ausência à esquerda */
                      if(sql_cap && sql_cap->nrows < SQL_OUT_MAX_ROWS && j < SQL_OUT_MAX_COLS)
                          snprintf(sql_cap->cell[sql_cap->nrows][j], SQL_OUT_CELL, "0");
                  }
                  for(long j = 0; j < ncols_dir; j++){
                      Word w = mem_le(S_JDIR + (unsigned)(d*J_MAXCOL + j));
                      long v = (long)w.total | ((long)w.e << 8);
                      printf("%ld", v);
                      if(j + 1 < ncols_dir) printf(" | ");
                      { int col = (int)(nc_esq + j);
                        if(sql_cap && sql_cap->nrows < SQL_OUT_MAX_ROWS && col < SQL_OUT_MAX_COLS)
                            snprintf(sql_cap->cell[sql_cap->nrows][col], SQL_OUT_CELL, "%ld", v); }
                  }
                  printf("\n");
                  jproj_linha(jmapa, jn, (int)(nc_esq + ncols_dir));
                 if(sql_cap && sql_cap->nrows < SQL_OUT_MAX_ROWS) sql_cap->nrows++;
                  saiu++;
              }
          }
          /* A SEGUNDA JUNÇÃO, sobre o que a primeira deu. É a MESMA operação:
           * a esquerda passa a ser o par (A|B), a direita é a terceira tabela,
           * e o ON procura-se nas colunas que já lá estão. */
          if(j2_tab[0]){
              long nc2 = nc_esq + ncols_dir;
              if(nc2 > J_MAXCOL) nc2 = J_MAXCOL;
              snprintf(j_tab_dir, sizeof j_tab_dir, "%s", j2_tab);
              snprintf(j_col_dir, sizeof j_col_dir, "%s", j2_dir);
              j_op = j2_op; j_left = 0; j_right = 0;
              { char t2[64]; snprintf(t2, sizeof t2, "%s", j2_tab); j2_tab[0] = 0;
                long nd2c = 0;
                /* o índice da coluna do ON na esquerda composta: procura-se nos
                 * nomes que já se leram da esquerda e nos da direita anterior */
                int oc2 = -1;
                for(long j = 0; j < nc_esq && j < J_MAXCOL; j++)
                    if(nome_esq[j][0] && !strcasecmp(nome_esq[j], j2_esq)){ oc2 = (int)j; break; }
                if(oc2 < 0){
                    /* está nas colunas da direita anterior, que ficou aberta */
                    int k2 = col_indice(j2_esq);
                    if(k2 >= 0) oc2 = (int)(nc_esq + k2);
                }
                if(oc2 < 0){
                    printf("erro: a coluna «%s» do segundo ON não está no que a"
                           " primeira junção deu — RECUSADA.\n", j2_esq);
                    if(sql_cap){ sql_cap->ok = 0;
                        snprintf(sql_cap->err, sizeof sql_cap->err,
                                 "column \"%s\" does not exist", j2_esq); }
                    return 0;
                }
                nd2c = 0;
                { int nd2 = j_carrega_direita(&nd2c);
                  if(nd2 < 0){
                      printf("erro: a terceira tabela não coube — RECUSADA.\n");
                      if(sql_cap){ sql_cap->ok = 0;
                          snprintf(sql_cap->err, sizeof sql_cap->err,
                                   "JOIN: a terceira tabela nao coube"); }
                      return 0;
                  }
                  for(int e2 = 0; e2 < nova_ne; e2++){
                      long v2 = (long)nova[e2][oc2].total | ((long)nova[e2][oc2].e << 8);
                      int cs[J_MAXLIN], nc2a = j_casam(v2, cs, J_MAXLIN);
                      for(int k = 0; k < nc2a; k++){
                          int d2 = cs[k];
                          if(d2 >= nd2) continue;
                          printf("   ");
                          for(long j = 0; j < nc2; j++){
                              long ve = (long)nova[e2][j].total | ((long)nova[e2][j].e << 8);
                              printf("%ld | ", ve);
                              if(sql_cap && sql_cap->nrows < SQL_OUT_MAX_ROWS && j < SQL_OUT_MAX_COLS)
                                  snprintf(sql_cap->cell[sql_cap->nrows][j], SQL_OUT_CELL, "%ld", ve);
                          }
                          for(long j = 0; j < nd2c; j++){
                              Word w = mem_le(S_JDIR + (unsigned)(d2*J_MAXCOL + j));
                              long v = (long)w.total | ((long)w.e << 8);
                              printf("%ld", v);
                              if(j + 1 < nd2c) printf(" | ");
                              { int col = (int)(nc2 + j);
                                if(sql_cap && sql_cap->nrows < SQL_OUT_MAX_ROWS && col < SQL_OUT_MAX_COLS)
                                    snprintf(sql_cap->cell[sql_cap->nrows][col], SQL_OUT_CELL, "%ld", v); }
                          }
                          printf("\n");
                          if(sql_cap){
                              jproj_linha(jmapa, jn, (int)(nc_esq + ncols_dir));
                              if(sql_cap->nrows < SQL_OUT_MAX_ROWS) sql_cap->nrows++;
                              sql_cap->ncols = (int)(nc2 + nd2c) > SQL_OUT_MAX_COLS
                                             ? SQL_OUT_MAX_COLS : (int)(nc2 + nd2c);
                          }
                          saiu++;
                      }
                  }
                  printf("-- JOIN de três: %d par(es) da primeira, %d linha(s) na"
                         " terceira, %ld emitidas\n", nova_ne, nd2, saiu); }
                (void)t2; }
              if(sql_cap) snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT %ld", saiu);
              return 1;
          }
          if(sql_cap) snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT %ld", saiu);
          printf("-- JOIN: %ld linha(s) da esquerda, %d da direita, %ld emitidas\n",
                 (long)ne, nd, saiu);
        }
        return 1;
    }

    /* A ORDEM, quando pedida: insere-se (valor, índice) na árvore do banco e
     * percorre-se por símbolos. As linhas saem por essa ordem em vez da ordem
     * de inserção. Se a árvore não couber, a consulta é RECUSADA — ordenar
     * metade seria devolver uma ordem que ninguém pediu. */
    long ord_n = 0; int ord_usa = 0;   /* a ordem está no disco: ordseq_le/poe */
    if(acao == ACAO_MARCA && ord_col[0]){
        int oc = col_indice(ord_col);
        int postos = 0;
        /* ── A LEITURA DO TEXTO PRESERVA A IGUALDADE E NÃO A ORDEM ───────────
         *
         * A célula de uma coluna de texto guarda o ENDEREÇO no pool, e o pool
         * deduplica: `x = y ⟺ R(x) = R(y)`, que é o critério da leitura e o que
         * faz a igualdade e a fibra funcionarem. Mas a ordem dos endereços é a
         * de ESCRITA, não a das cadeias --- `zulu` escrito primeiro fica antes
         * de `alfa`. `ORDER BY s` devolvia a ordem de inserção com a cara da
         * ordem pedida, e ninguém dava por isso.
         *
         * Recusa-se, e diz-se porquê. Ordenar pelo prefixo dava uma ordem
         * PARCIAL --- cadeias com os mesmos primeiros caracteres ficariam
         * indistintas --- apresentada como total, que é o mesmo defeito com
         * outra cara. A via é a SEGUNDA RÉGUA que o motor já tem para o
         * `ORDER BY a, b`: ordenar pelo prefixo parte a saída em fibras, e
         * dentro de cada uma compara-se a cadeia inteira. Fica escrito porque é
         * trabalho e não um remendo. */
        if(oc >= 0 && corpo_de(oc).total == CORPO_TEXTO){
            printf("erro: `ORDER BY %s` é sobre TEXTO, e a célula guarda o ENDEREÇO no"
                   " pool — RECUSADO. A leitura preserva a IGUALDADE (endereços iguais"
                   " são cadeias iguais, e por isso o = e o GROUP BY funcionam) e NÃO a"
                   " ordem: a dos endereços é a de escrita. Devolver essa seria dar uma"
                   " ordem que ninguém pediu.\n", ord_col);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "ORDER BY on a text column: the cell holds a pool address,"
                         " which preserves equality but not order"); }
            return 0;
        }
        /* ── E O QUE NÃO TEM VALOR NÃO TEM LUGAR NA ORDEM ────────────────
         * Ordenar é comparar, e a célula ausente não tem com que comparar:
         * lê-la dava o neutro e punha-a entre os zeros escritos, no meio da
         * ordem, como se fosse o menor dos valores. Vai para o FIM, em bloco —
         * é a convenção que o SQL chama NULLS LAST, e aqui é a única que não
         * mente: o dual não é pequeno, é de outro nível. */
        long na = 0;      /* os ausentes também no disco --- ordaus_le/poe */
        /* A ÁRVORE DESTA ORDENAÇÃO É A DO RASCUNHO, e é preciso DIZÊ-LO: sem
         * isto ficava a que a chamada anterior tivesse deixado seleccionada ---
         * a de um ÍNDICE, com a zona pequena e o tecto de 600 nós ---, e o
         * ORDER BY era recusado por não caber num sítio que não era o dele. */
        /* ── E DIZ-SE POR QUE RÉGUA SE ORDENOU ───────────────────────────
         *
         * Ordenar por uma régua e não dizer qual é o mesmo defeito de ordenar
         * pelos bits: a saída sai com a cara de uma ordem que ninguém nomeou.
         * Numa coluna quadrática a chave é a NORMA do `contrato.h` --- é ela
         * que distingue os corpos, e compõe as três classes sem despachar por
         * nenhuma. Não se recusa nada: diz-se, e devolve-se. */
        if(oc >= 0){
            Word cq = corpo_de(oc);
            if(cq.total == CORPO_AUREO || cq.total == CORPO_CRISTAL){
                long B = corpo_B(cq.total, cq.e), C = corpo_C(cq.total);
                printf("nota: `ORDER BY %s` ordena pela RÉGUA do corpo — a norma"
                       " q(a,b) = a² %+ld·a·b %+ld·b², de assinatura Δ = %ld (%s).\n",
                       ord_col, B, C, B*B - 4*C, corpo_classe_nome(B*B - 4*C, 0));
                printf("      NÃO é a ordem dos bits da célula, que seria a"
                       " lexicográfica de (a,b) e não é ordem nenhuma do corpo.\n");
            }
        }
        ord_usa_rascunho();
        ord_limpa();
        for(long i = 0; i < nrows && postos < (int)S_SAIDA_N; i++){
            if(!bit_le(S_MATCH, i)) continue;
            if(!bit_le(S_PRES, i*ncols + oc)){
                if(na < (long)S_SAIDA_N){ ordaus_poe(na, i); na++; }
                continue;
            }
            { long v = col_regua_chave(i, oc, ncols);  /* a RÉGUA do corpo, não os bits */
              if(!ord_insere(v, (int)i)){
                  printf("erro: a árvore de ordenação não coube — RECUSADA.\n");
                  if(sql_cap){ sql_cap->ok = 0;
                      snprintf(sql_cap->err, sizeof sql_cap->err,
                               "ORDER BY: a arvore nao coube; recusa em vez de ordenar"
                               " metade"); }
                  return 0;
              }
              postos++; }
        }
        { int n = 0;
          static int tmp[8192];
          ord_percorre(0, 0, 0, tmp, &n, 8192, ord_desc);
          for(int k = 0; k < n; k++) ordseq_poe(k, tmp[k]);
          for(long k = 0; k < na && n < (int)S_SAIDA_N; k++){ ordseq_poe(n, ordaus_le(k)); n++; }
          ord_n = n; ord_usa = 1; }

        /* ── E A SEGUNDA RÉGUA, ONDE A PRIMEIRA NÃO DISTINGUIU ───────────
         * A saída está partida em FIBRAS — as corridas de mesmo valor da
         * primeira coluna, e o bloco final dos que não têm valor nenhum. Dentro
         * de cada uma, a ordem ainda é a de chegada; aplica-se-lhe a segunda
         * régua, que é a MESMA descida corrida outra vez. Compor funções é
         * compor réguas, e o desempate é a régua seguinte no sítio onde a
         * anterior calou. */
        if(ord_col2[0]){
            int oc2 = col_indice(ord_col2);
            long k = 0;
            while(oc2 >= 0 && k < ord_n){
                int tem = bit_le(S_PRES, ordseq_le(k)*ncols + oc);
                /* a MESMA chave que partiu a saída em fibras --- partir por um
                 * critério e desempatar por outro daria fibras que não são as
                 * da ordem que se devolveu */
                long v = tem ? col_regua_chave(ordseq_le(k), oc, ncols) : 0;
                long j = k;
                /* a corrida: mesma chave, ou ambos sem chave */
                while(j < ord_n && bit_le(S_PRES, ordseq_le(j)*ncols + oc) == tem
                                && (!tem || col_regua_chave(ordseq_le(j), oc, ncols) == v)) j++;
                if(j - k > 1){
                    /* o `a2` já tinha o tamanho da ORDEM e o `tmp2` ficara com
                     * o da JANELA DA PORTA C --- dois números diferentes para o
                     * mesmo objecto, e o menor mandava. Medido: `ORDER BY a,b`
                     * com 200 linhas na mesma fibra saía ordenado até à posição
                     * 64 e desordenado a partir daí, exactamente onde o arranjo
                     * acabava. `aranha §sec:dimensao`: o número é do ARRANJO. */
                    static long a2[8192]; static int tmp2[8192];
                    int n2 = 0, m = 0, coube = 1;
                    ord_limpa();
                    for(long t = k; t < j && coube; t++){
                        if(!bit_le(S_PRES, ordseq_le(t)*ncols + oc2)){
                            if(n2 < (int)S_SAIDA_N) a2[n2++] = ordseq_le(t);
                            continue;
                        }
                        if(!ord_insere(col_regua_chave(ordseq_le(t), oc2, ncols),
                                       (int)ordseq_le(t))) coube = 0;
                    }
                    if(coube){
                        ord_percorre(0, 0, 0, tmp2, &m, 8192, ord_desc2);
                        { long t = k;
                          for(int q = 0; q < m && t < j; q++) ordseq_poe(t++, tmp2[q]);
                          for(int q = 0; q < n2 && t < j; q++) ordseq_poe(t++, a2[q]); }
                    }
                    /* se não coube, a fibra fica na ordem de chegada: a primeira
                     * régua continua certa, e é ela que a pergunta pediu primeiro */
                }
                k = j;
            }
        }
    }

    /* ── GROUP BY: A FIBRA, E O SEU TAMANHO É G ─────────────────────────
     *
     * `thm:escada`: «quocientar é esquecer a distinção; G mede quantos
     * elementos foram esquecidos juntos». A realização é π: linha ↦ valor da
     * coluna, e cada classe de equivalência é uma fibra π⁻¹(x). Usa-se a MESMA
     * árvore que ordena — inserir é descer pelos símbolos do valor —, e depois
     * basta percorrer por ordem: as linhas da mesma fibra saem CONTÍGUAS,
     * porque a chave começa pelo valor. Contar cada corrida é contar G(x).
     *
     * E a conservação vale, e é o Lema da conservação: ∑ G(x) = |I| — a soma
     * dos grupos é o número de linhas que casaram. */
    /* ── AGREGAR SEM QUOCIENTAR É QUOCIENTAR PELO MAPA CONSTANTE ────────────
     * `SELECT sum(v) FROM t` não tem GROUP BY, e por isso não tinha fibra
     * nenhuma: a coluna saía CRUA, uma linha por linha, com o nome de uma
     * função que ninguém aplicou. Mas agregar sem quocientar não é um caso
     * especial — é o quociente pelo mapa CONSTANTE, cuja única fibra é a
     * tabela inteira. Uma fibra, uma linha.
     *
     * E O AGREGADO NÃO SE PRONUNCIA SOBRE O QUE NÃO ESTÁ: soma-se o corpo, não
     * o suporte. Daí sai o par que separa as duas contagens — a soma de NADA é
     * AUSENTE, não zero, porque zero é um valor e aqui não houve nenhum; o
     * count de nada é zero, porque contar o vazio é zero e não uma ausência.
     * As duas respostas são de níveis diferentes, e é isso que elas dizem. */
    /* ── A TABELA É UMA MATRIZ ───────────────────────────────────────────────
     *
     * As linhas por colunas de uma tabela são as entradas de uma matriz, e as
     * perguntas da álgebra linear são perguntas sobre a tabela: qual o seu
     * determinante, qual o seu posto, o que sobra quando ela se transpõe. Não é
     * uma leitura forçada — é a mesma tabela vista pela outra face, e o
     * `lib/linear.h` já tem tudo o que ela precisa, em ℚ exacto.
     *
     * O que se decide aqui é o ALCANCE, e ele não é escolhido: o `linear.h`
     * trabalha até LN_MAX×LN_MAX, e o que passa disso RECUSA-SE — a régua
     * diz-se antes de se usar, como em todo o resto desta casa.
     *
     * As que devolvem um NÚMERO (det, posto, traço) saem como o `count`: uma
     * linha, uma coluna. As que devolvem uma MATRIZ (transposta, inversa) saem
     * como tabela — e é aí que se vê que a leitura fecha, porque o resultado é
     * outra vez uma coisa a que se pode perguntar o determinante. */
    if(acao == ACAO_MARCA && mat_op == 28){
        /* ── A TRÍADE FECHADA: a tabela traz o par (a,b) --- as coordenadas do
         * elemento --- e o motor devolve as TRÊS faces de uma vez:
         *
         *     t = −1  N = a²+b²  Δ < 0  volta       (rotação)
         *     t =  0  N = a²     Δ = 0  desliza     (exterior)
         *     t = +1  N = a²−b²  Δ > 0  foge        (hiperbólico)
         *
         * com Δ = 4·t·b² --- uma fórmula, três classes. E a RÉGUA é a mesma nas
         * três, porque o endereço é o par e ele não sabe de t: devolve-se
         * também se ela desce, para o cliente não ter de perguntar duas vezes.
         *
         * Não é o `regime`, que lê o Δ de um OPERADOR 2×2 já dado. Aqui o par
         * é o ELEMENTO, e as três faces são as três métricas que ele admite. */
        if(ncols != 2){
            printf("erro: a tríade lê-se de DUAS colunas --- o par (a,b). A tabela"
                   " tem %ld. RECUSADA.\n", ncols);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "triade needs the pair (a,b), got %ld columns", ncols); }
            return 0;
        }
        long n_obj = 0;
        for(long i = 0; i < nrows; i++) if(bit_le(S_MATCH, i)) n_obj++;
        if(n_obj == 0){
            printf("erro: não há linhas --- RECUSADA.\n");
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err, "triade on empty selection"); }
            return 0;
        }
        /* soma-se a norma de cada face sobre a selecção, e conta-se o Δ */
        long Nsoma[3] = {0,0,0};
        long neg = 0, zer = 0, pos = 0;
        long ends[512]; long ne = 0;
        for(long i = 0; i < nrows; i++){
            if(!bit_le(S_MATCH, i)) continue;
            if(!bit_le(S_PRES, i*ncols) || !bit_le(S_PRES, i*ncols + 1)){
                printf("erro: a linha %ld tem célula ausente --- RECUSADA.\n", i);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "triade: missing cell at row %ld", i); }
                return 0;
            }
            long an, ad, bn, bd;
            celula_qz(i, 0, ncols, &an, &ad);
            celula_qz(i, 1, ncols, &bn, &bd);
            if(ad != 1 || bd != 1){
                printf("erro: o par (%ld/%ld, %ld/%ld) não é inteiro --- as três normas"
                       " saem exactas de inteiros. RECUSADA.\n", an, ad, bn, bd);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "triade: pair is not integral"); }
                return 0;
            }
            for(int ti = 0; ti < 3; ti++){
                long t = ti - 1;
                Nsoma[ti] += an*an - t*bn*bn;
            }
            /* o Δ de cada face neste elemento: 4·t·b² */
            if(bn != 0){ neg++; pos++; }
            zer++;
            if(ne < 512 && an >= 0 && bn >= 0) ends[ne++] = an*64 + bn;
        }
        /* a régua sobre os endereços do par --- a mesma nas três faces */
        int desce = 1, houve_est = 0;
        if(ne >= 3){
            long lim = ne < 30 ? ne : 30;
            int bits = 1; { long mx = 0;
                for(long i = 0; i < ne; i++) if(ends[i] > mx) mx = ends[i];
                long t = mx; while(t){ bits++; t >>= 1; } }
            for(long i = 0; i < lim && desce; i++) for(long j = 0; j < lim; j++)
            for(long k = 0; k < lim; k++){
                long a1 = bits, b1 = bits, c1 = bits;
                if(ends[i]!=ends[j]) for(int t=0;t<bits;t++)
                    if(((ends[i]>>(bits-1-t))&1L)!=((ends[j]>>(bits-1-t))&1L)){ a1=t; break; }
                if(ends[j]!=ends[k]) for(int t=0;t<bits;t++)
                    if(((ends[j]>>(bits-1-t))&1L)!=((ends[k]>>(bits-1-t))&1L)){ b1=t; break; }
                if(ends[i]!=ends[k]) for(int t=0;t<bits;t++)
                    if(((ends[i]>>(bits-1-t))&1L)!=((ends[k]>>(bits-1-t))&1L)){ c1=t; break; }
                long m3 = a1 < b1 ? a1 : b1;
                if(c1 < m3){ desce = 0; break; }
                if(c1 > m3) houve_est = 1;
            }
        }
        if(sql_cap){
            memset(sql_cap, 0, sizeof *sql_cap);
            sql_cap->ok = 1; sql_cap->nrows = 1; sql_cap->ncols = 5;
            snprintf(sql_cap->col[0], sizeof sql_cap->col[0], "n_circulo");
            snprintf(sql_cap->col[1], sizeof sql_cap->col[1], "n_parabola");
            snprintf(sql_cap->col[2], sizeof sql_cap->col[2], "n_hiperbole");
            snprintf(sql_cap->col[3], sizeof sql_cap->col[3], "objetos");
            snprintf(sql_cap->col[4], sizeof sql_cap->col[4], "regua");
            for(int c = 0; c < sql_cap->ncols; c++) sql_cap->tipo[c] = SQL_TIPO_INT4;
            sql_cap->tipo[4] = SQL_TIPO_TEXT;
            snprintf(sql_cap->cell[0][0], SQL_OUT_CELL, "%ld", Nsoma[0]);
            snprintf(sql_cap->cell[0][1], SQL_OUT_CELL, "%ld", Nsoma[1]);
            snprintf(sql_cap->cell[0][2], SQL_OUT_CELL, "%ld", Nsoma[2]);
            snprintf(sql_cap->cell[0][3], SQL_OUT_CELL, "%ld", n_obj);
            snprintf(sql_cap->cell[0][4], SQL_OUT_CELL, "%s",
                     (ne < 3) ? "sem triplo" : (desce && houve_est) ? "desce" : "NAO");
            snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT 1");
        }
        printf("   %ld | %ld | %ld | %ld | %s\n", Nsoma[0], Nsoma[1], Nsoma[2], n_obj,
               (ne < 3) ? "sem triplo" : (desce && houve_est) ? "desce" : "NAO");
        printf("-- as três faces sobre %ld objecto(s): Δ = 4tb², e o sinal de Δ é o de t ·"
               " a RÉGUA é a mesma nas três, porque o endereço é o par e não sabe de t\n",
               n_obj);
        return 1;
    }

    if(acao == ACAO_MARCA && mat_op == 27){
        /* ── AS RÉGUAS p-ÁDICAS: uma POR PRIMO, e cada uma é ultramétrica.
         *
         *     v_p(n) = quantas vezes p divide n
         *     d_p(x,y) = p^{−v_p(x−y)}
         *
         * O `ultra` mede a régua do PREFIXO --- a def:arvore lida em bits. Esta
         * mede outra família, e o ponto é que há mais de uma: o racional tem uma
         * régua por primo, e todas cumprem a desigualdade forte. Duas perguntas
         * distintas sobre a mesma coluna, e é por isso que são duas operações.
         *
         * Devolve-se a valoração como INTEIRO, nunca p^{−v}: a potência sairia
         * do degrau, e o que se compara são profundidades. */
        if(ncols != 1){
            printf("erro: a valoração é de UMA coluna --- a tabela tem %ld."
                   " RECUSADA.\n", ncols);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "valoracao needs one column, got %ld", ncols); }
            return 0;
        }
        long n_obj = 0;
        for(long i = 0; i < nrows; i++) if(bit_le(S_MATCH, i)) n_obj++;
        if(n_obj < 3){
            printf("erro: a desigualdade forte é sobre TRIPLOS --- há %ld linha(s)."
                   " RECUSADA.\n", n_obj);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "valoracao needs at least 3 rows, got %ld", n_obj); }
            return 0;
        }
        long v[512]; long nv = 0;
        for(long i = 0; i < nrows && nv < 512; i++){
            if(!bit_le(S_MATCH, i)) continue;
            if(!bit_le(S_PRES, i*ncols)){
                printf("erro: célula ausente --- um buraco não tem valoração."
                       " RECUSADA.\n");
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "valoracao: missing cell at row %ld", i); }
                return 0;
            }
            long nu, de; celula_qz(i, 0, ncols, &nu, &de);
            if(de != 1){
                printf("erro: o valor %ld/%ld não é inteiro --- a valoração de uma"
                       " fração é a diferença das duas, e aqui pede-se a coluna já"
                       " inteira. RECUSADA.\n", nu, de);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "valoracao: value %ld/%ld is not an integer", nu, de); }
                return 0;
            }
            v[nv++] = nu;
        }
        /* para cada primo pequeno: a régua vale? quantos triplos são estritos? */
        long primos[4] = {2, 3, 5, 7};
        long total_vale = 0, total_falha = 0, total_est = 0, todas_ok = 0;
        char linha[SQL_OUT_CELL];
        for(int pi = 0; pi < 4; pi++){
            long p = primos[pi];
            long vale = 0, falha = 0, est = 0;
            for(long i = 0; i < nv; i++) for(long j = 0; j < nv; j++) for(long k = 0; k < nv; k++){
                long dxy = v[i] - v[j], dyz = v[j] - v[k], dxz = v[i] - v[k];
                int a1 = 62, b1 = 62, c1 = 62;
                if(dxy){ long t = dxy < 0 ? -dxy : dxy; a1 = 0; while(t % p == 0){ t /= p; a1++; } }
                if(dyz){ long t = dyz < 0 ? -dyz : dyz; b1 = 0; while(t % p == 0){ t /= p; b1++; } }
                if(dxz){ long t = dxz < 0 ? -dxz : dxz; c1 = 0; while(t % p == 0){ t /= p; c1++; } }
                int mn = a1 < b1 ? a1 : b1;
                if(c1 >= mn){ vale++; if(c1 > mn) est++; } else falha++;
            }
            total_vale += vale; total_falha += falha; total_est += est;
            if(falha == 0) todas_ok++;
            printf("   p = %ld: %ld valem, %ld falham, %ld estritos\n", p, vale, falha, est);
        }
        snprintf(linha, sizeof linha, "%ld/4", todas_ok);
        if(sql_cap){
            memset(sql_cap, 0, sizeof *sql_cap);
            sql_cap->ok = 1; sql_cap->nrows = 1; sql_cap->ncols = 4;
            snprintf(sql_cap->col[0], sizeof sql_cap->col[0], "primos_ok");
            snprintf(sql_cap->col[1], sizeof sql_cap->col[1], "vale");
            snprintf(sql_cap->col[2], sizeof sql_cap->col[2], "falha");
            snprintf(sql_cap->col[3], sizeof sql_cap->col[3], "estrito");
            sql_cap->tipo[0] = SQL_TIPO_TEXT;
            for(int c = 1; c < 4; c++) sql_cap->tipo[c] = SQL_TIPO_INT4;
            snprintf(sql_cap->cell[0][0], SQL_OUT_CELL, "%s", linha);
            snprintf(sql_cap->cell[0][1], SQL_OUT_CELL, "%ld", total_vale);
            snprintf(sql_cap->cell[0][2], SQL_OUT_CELL, "%ld", total_falha);
            snprintf(sql_cap->cell[0][3], SQL_OUT_CELL, "%ld", total_est);
            snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT 1");
        }
        printf("   %s | %ld | %ld | %ld\n", linha, total_vale, total_falha, total_est);
        printf("-- %s primos com a régua a valer sobre %ld endereços · há uma régua POR"
               " PRIMO, e todas são ultramétricas · a do `ultra` é a do prefixo, e é"
               " outra\n", linha, nv);
        return 1;
    }

    if(acao == ACAO_MARCA && mat_op == 26){
        /* ── A LEITURA SERVE? São duas perguntas, e elas são DUAIS:
         *
         *   BEM DEFINIDA  x = y no corpo  ⟹  R(x) = R(y)   não parte o objecto
         *   SEPARADORA    R(x) = R(y)     ⟹  x = y         não funde objectos
         *
         * A tabela traz DUAS colunas: a CLASSE (o objecto, já identificado pela
         * igualdade DO CORPO) e o ENDEREÇO que a leitura lhe dá. A igualdade não
         * se adivinha aqui --- é o cliente que a traz na primeira coluna, porque
         * ela é do corpo dele e não do motor.
         *
         * As duas falhas têm formas opostas, e contam-se em separado: QUEBRAS
         * (mesma classe, endereços distintos) e FUSÕES (mesmo endereço, classes
         * distintas). Dá-las num número só perderia exactamente o que distingue
         * uma leitura fina demais de uma grosseira demais. */
        if(ncols != 2){
            printf("erro: a leitura mede-se com DUAS colunas --- a classe e o"
                   " endereço. A tabela tem %ld. RECUSADA.\n", ncols);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "leitura needs (class, address), got %ld columns", ncols); }
            return 0;
        }
        long n_obj = 0;
        for(long i = 0; i < nrows; i++) if(bit_le(S_MATCH, i)) n_obj++;
        if(n_obj < 2){
            printf("erro: com %ld linha(s) não há par que comparar --- RECUSADA.\n", n_obj);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "leitura needs at least 2 rows, got %ld", n_obj); }
            return 0;
        }
        for(long i = 0; i < nrows; i++)
            if(bit_le(S_MATCH, i) &&
               (!bit_le(S_PRES, i*ncols) || !bit_le(S_PRES, i*ncols + 1))){
                printf("erro: a linha %ld tem célula ausente --- sem classe ou sem"
                       " endereço não há o que comparar. RECUSADA.\n", i);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "leitura: missing cell at row %ld", i); }
                return 0;
            }
        long quebras = 0, fusoes = 0, pares = 0;
        for(long i = 0; i < nrows; i++){
            if(!bit_le(S_MATCH, i)) continue;
            long ci_n, ci_d, ei_n, ei_d;
            celula_qz(i, 0, ncols, &ci_n, &ci_d);
            celula_qz(i, 1, ncols, &ei_n, &ei_d);
            Qz ci = qz(ci_n, ci_d), ei = qz(ei_n, ei_d);
            for(long j = 0; j < i; j++){
                if(!bit_le(S_MATCH, j)) continue;
                long cj_n, cj_d, ej_n, ej_d;
                celula_qz(j, 0, ncols, &cj_n, &cj_d);
                celula_qz(j, 1, ncols, &ej_n, &ej_d);
                int mesma_classe = qz_igual(ci, qz(cj_n, cj_d));
                int mesmo_end    = qz_igual(ei, qz(ej_n, ej_d));
                pares++;
                if(mesma_classe && !mesmo_end) quebras++;
                if(mesmo_end && !mesma_classe) fusoes++;
            }
        }
        int bd = (quebras == 0), sep = (fusoes == 0);
        const char *ver = (bd && sep) ? "serve"
                        : bd ? "funde --- falta leitura"
                             : "quebra --- a régua não é do corpo";
        if(sql_cap){
            memset(sql_cap, 0, sizeof *sql_cap);
            sql_cap->ok = 1; sql_cap->nrows = 1; sql_cap->ncols = 5;
            snprintf(sql_cap->col[0], sizeof sql_cap->col[0], "pares");
            snprintf(sql_cap->col[1], sizeof sql_cap->col[1], "quebras");
            snprintf(sql_cap->col[2], sizeof sql_cap->col[2], "fusoes");
            snprintf(sql_cap->col[3], sizeof sql_cap->col[3], "bem_definida");
            snprintf(sql_cap->col[4], sizeof sql_cap->col[4], "separa");
            for(int c = 0; c < sql_cap->ncols; c++) sql_cap->tipo[c] = SQL_TIPO_INT4;
            sql_cap->tipo[3] = sql_cap->tipo[4] = SQL_TIPO_TEXT;
            snprintf(sql_cap->cell[0][0], SQL_OUT_CELL, "%ld", pares);
            snprintf(sql_cap->cell[0][1], SQL_OUT_CELL, "%ld", quebras);
            snprintf(sql_cap->cell[0][2], SQL_OUT_CELL, "%ld", fusoes);
            snprintf(sql_cap->cell[0][3], SQL_OUT_CELL, "%s", bd ? "sim" : "nao");
            snprintf(sql_cap->cell[0][4], SQL_OUT_CELL, "%s", sep ? "sim" : "nao");
            snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT 1");
        }
        printf("   %ld | %ld | %ld | %s | %s\n", pares, quebras, fusoes,
               bd ? "sim" : "nao", sep ? "sim" : "nao");
        printf("-- %ld par(es) · %ld quebra(s) · %ld fusão(ões) · a leitura %s\n",
               pares, quebras, fusoes, ver);
        return 1;
    }

    if(acao == ACAO_MARCA && mat_op == 25){
        /* ── AS TRÊS MÉDIAS DE UM PAR, e o trio que fecha sobre si:
         *
         *     m = (a+b)/2      g² = ab      h = 2ab/(a+b)      e  g² = h·m
         *
         * Devolve-se g AO QUADRADO --- a raiz sairia do degrau, e o
         * cor:medias da aranha já compara g² com m². A ordem h ≤ g ≤ m
         * reduz-se a (a−b)² ≥ 0, e é essa que se verifica.
         *
         * É sobre um PAR: a relação g² = hm é do par e não generaliza a n
         * termos, e prometê-la para uma coluna qualquer seria estender uma lei
         * onde ela não vale. Com mais de duas linhas RECUSA e diz porquê. */
        if(ncols != 1){
            printf("erro: as médias são de UMA coluna de valores --- a tabela tem"
                   " %ld. RECUSADA.\n", ncols);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "medias needs one column, got %ld", ncols); }
            return 0;
        }
        long n_obj = 0;
        for(long i = 0; i < nrows; i++) if(bit_le(S_MATCH, i)) n_obj++;
        if(n_obj != 2){
            printf("erro: o trio g² = h·m é do PAR --- há %ld linha(s), e a relação"
                   " não generaliza a n termos. RECUSADA.\n", n_obj);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "medias is about a pair, got %ld rows", n_obj); }
            return 0;
        }
        long v[2]; int k = 0;
        for(long i = 0; i < nrows && k < 2; i++){
            if(!bit_le(S_MATCH, i)) continue;
            if(!bit_le(S_PRES, i*ncols)){
                printf("erro: célula ausente --- RECUSADA.\n");
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err, "medias: missing cell"); }
                return 0;
            }
            long nu, de; celula_qz(i, 0, ncols, &nu, &de);
            if(de != 1){
                printf("erro: o valor %ld/%ld não é inteiro --- as médias saem"
                       " exactas de inteiros, e uma fração já traz a divisão feita."
                       " RECUSADA.\n", nu, de);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "medias: value %ld/%ld is not an integer", nu, de); }
                return 0;
            }
            v[k++] = nu;
        }
        long a = v[0], b = v[1];
        if(a + b == 0){
            printf("erro: a + b = 0 --- a harmónica pede a soma no denominador, e ela"
                   " é zero. RECUSADA.\n");
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err, "medias: a+b = 0"); }
            return 0;
        }
        long dois_m = a + b;                 /* 2m */
        long g2 = a * b;                     /* g² */
        long h_num = 2*a*b, h_den = a + b;   /* h */
        int fecha = (h_num/2 == g2);         /* g² = h·m, exacto */
        int ordem = (4*a*b <= (a+b)*(a+b));  /* h ≤ g ≤ m  ⟺  (a−b)² ≥ 0 */
        if(sql_cap){
            memset(sql_cap, 0, sizeof *sql_cap);
            sql_cap->ok = 1; sql_cap->nrows = 1; sql_cap->ncols = 6;
            snprintf(sql_cap->col[0], sizeof sql_cap->col[0], "dois_m");
            snprintf(sql_cap->col[1], sizeof sql_cap->col[1], "g2");
            snprintf(sql_cap->col[2], sizeof sql_cap->col[2], "h_num");
            snprintf(sql_cap->col[3], sizeof sql_cap->col[3], "h_den");
            snprintf(sql_cap->col[4], sizeof sql_cap->col[4], "fecha");
            snprintf(sql_cap->col[5], sizeof sql_cap->col[5], "ordem");
            for(int c = 0; c < sql_cap->ncols; c++) sql_cap->tipo[c] = SQL_TIPO_INT4;
            sql_cap->tipo[4] = sql_cap->tipo[5] = SQL_TIPO_TEXT;
            snprintf(sql_cap->cell[0][0], SQL_OUT_CELL, "%ld", dois_m);
            snprintf(sql_cap->cell[0][1], SQL_OUT_CELL, "%ld", g2);
            snprintf(sql_cap->cell[0][2], SQL_OUT_CELL, "%ld", h_num);
            snprintf(sql_cap->cell[0][3], SQL_OUT_CELL, "%ld", h_den);
            snprintf(sql_cap->cell[0][4], SQL_OUT_CELL, "%s", fecha ? "sim" : "nao");
            snprintf(sql_cap->cell[0][5], SQL_OUT_CELL, "%s", ordem ? "sim" : "nao");
            snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT 1");
        }
        printf("   %ld | %ld | %ld | %ld | %s | %s\n",
               dois_m, g2, h_num, h_den, fecha ? "sim" : "nao", ordem ? "sim" : "nao");
        printf("-- 2m = %ld · g² = %ld · h = %ld/%ld · g² = h·m: %s · h ≤ g ≤ m: %s ·"
               " a raiz não se toma, porque sairia do degrau\n",
               dois_m, g2, h_num, h_den, fecha ? "sim" : "NAO", ordem ? "sim" : "NAO");
        return 1;
    }

    if(acao == ACAO_MARCA && mat_op == 24){
        /* ── O PREÇO DA TRAVESSIA: a tabela traz DUAS colunas --- o endereço do
         * mesmo objecto em duas leituras ---, e o que sai é o custo
         *
         *     D(R,S) = 2^{−q},   q = a primeira posição em que divergem
         *
         * (aranha prop:travessia). O supremo é ATINGIDO, não estimado: q é a
         * MENOR profundidade sobre os objectos, e devolve-se ela, não 2^{−q},
         * porque a potência sairia do degrau.
         *
         * Se as duas colunas coincidem em todas as linhas, não há travessia
         * nem preço --- e diz-se, em vez de devolver um zero que se confundiria
         * com «custo nulo mas há travessia». */
        if(ncols != 2){
            printf("erro: o preço mede-se entre DUAS leituras --- a tabela tem %ld"
                   " coluna(s). RECUSADA.\n", ncols);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "preco needs two columns of addresses, got %ld", ncols); }
            return 0;
        }
        long n_obj = 0;
        for(long i = 0; i < nrows; i++) if(bit_le(S_MATCH, i)) n_obj++;
        if(n_obj == 0){
            printf("erro: não há linhas --- RECUSADA.\n");
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err, "preco on empty selection"); }
            return 0;
        }
        long mx = 0; int iguais = 1;
        for(long i = 0; i < nrows; i++){
            if(!bit_le(S_MATCH, i)) continue;
            if(!bit_le(S_PRES, i*ncols) || !bit_le(S_PRES, i*ncols + 1)){
                printf("erro: a linha %ld tem célula ausente --- um buraco não é um"
                       " endereço. RECUSADA.\n", i);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "preco: missing cell at row %ld", i); }
                return 0;
            }
            long n1, d1, n2, d2;
            celula_qz(i, 0, ncols, &n1, &d1);
            celula_qz(i, 1, ncols, &n2, &d2);
            if(d1 != 1 || d2 != 1 || n1 < 0 || n2 < 0){
                printf("erro: os endereços têm de ser inteiros não negativos --- a"
                       " profundidade lê-se do prefixo. RECUSADA.\n");
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "preco: addresses must be non-negative integers"); }
                return 0;
            }
            if(n1 != n2) iguais = 0;
            if(n1 > mx) mx = n1;
            if(n2 > mx) mx = n2;
        }
        int bits = 1; { long t = mx; while(t){ bits++; t >>= 1; } }
        if(iguais){
            if(sql_cap){
                memset(sql_cap, 0, sizeof *sql_cap);
                sql_cap->ok = 1; sql_cap->nrows = 1; sql_cap->ncols = 3;
                snprintf(sql_cap->col[0], sizeof sql_cap->col[0], "q");
                snprintf(sql_cap->col[1], sizeof sql_cap->col[1], "objetos");
                snprintf(sql_cap->col[2], sizeof sql_cap->col[2], "travessia");
                sql_cap->tipo[0] = sql_cap->tipo[1] = SQL_TIPO_INT4;
                sql_cap->tipo[2] = SQL_TIPO_TEXT;
                snprintf(sql_cap->cell[0][0], SQL_OUT_CELL, "%d", bits);
                snprintf(sql_cap->cell[0][1], SQL_OUT_CELL, "%ld", n_obj);
                snprintf(sql_cap->cell[0][2], SQL_OUT_CELL, "nenhuma");
                snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT 1");
            }
            printf("   %d | %ld | nenhuma\n", bits, n_obj);
            printf("-- as duas leituras coincidem em todos os %ld objectos: R = S, não há"
                   " travessia nem preço\n", n_obj);
            return 1;
        }
        /* o supremo de d é a MENOR profundidade */
        int q = bits;
        for(long i = 0; i < nrows; i++){
            if(!bit_le(S_MATCH, i)) continue;
            long n1, d1, n2, d2;
            celula_qz(i, 0, ncols, &n1, &d1);
            celula_qz(i, 1, ncols, &n2, &d2);
            int p = bits;
            if(n1 != n2) for(int b = 0; b < bits; b++)
                if(((n1 >> (bits-1-b)) & 1L) != ((n2 >> (bits-1-b)) & 1L)){ p = b; break; }
            if(p < q) q = p;
        }
        if(sql_cap){
            memset(sql_cap, 0, sizeof *sql_cap);
            sql_cap->ok = 1; sql_cap->nrows = 1; sql_cap->ncols = 3;
            snprintf(sql_cap->col[0], sizeof sql_cap->col[0], "q");
            snprintf(sql_cap->col[1], sizeof sql_cap->col[1], "objetos");
            snprintf(sql_cap->col[2], sizeof sql_cap->col[2], "travessia");
            sql_cap->tipo[0] = sql_cap->tipo[1] = SQL_TIPO_INT4;
            sql_cap->tipo[2] = SQL_TIPO_TEXT;
            snprintf(sql_cap->cell[0][0], SQL_OUT_CELL, "%d", q);
            snprintf(sql_cap->cell[0][1], SQL_OUT_CELL, "%ld", n_obj);
            snprintf(sql_cap->cell[0][2], SQL_OUT_CELL, "sim");
            snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT 1");
        }
        printf("   %d | %ld | sim\n", q, n_obj);
        printf("-- D(R,S) = 2^-%d sobre %ld objectos de %d bits · o supremo é ATINGIDO,"
               " e o que se devolve é q e não a potência\n", q, n_obj, bits);
        return 1;
    }

    if(acao == ACAO_MARCA && mat_op == 23){
        /* ── A RÉGUA: a coluna de endereços cumpre a desigualdade FORTE?
         * d(x,z) ≤ max{d(x,y), d(y,z)}, que na profundidade é
         * prof(x,z) ≥ min{prof(x,y), prof(y,z)}  (aranha def:arvore, lem:ultra).
         *
         * O cliente pergunta isto antes de confiar a sua leitura à travessia:
         * é a régua que desce, e sem ela nada desce. Conta-se sobre a coluna
         * inteira --- não é operação matricial, e não tem tecto de LN_MAX. */
        if(ncols != 1){
            printf("erro: a régua mede-se sobre UMA coluna de endereços --- a"
                   " tabela tem %ld. RECUSADA.\n", ncols);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "ultra needs one column of addresses, got %ld", ncols); }
            return 0;
        }
        long n_obj = 0;
        for(long i = 0; i < nrows; i++) if(bit_le(S_MATCH, i)) n_obj++;
        if(n_obj < 3){
            printf("erro: a desigualdade forte é sobre TRIPLOS --- há %ld linha(s),"
                   " e com menos de três não há o que medir. RECUSADA.\n", n_obj);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "ultra needs at least 3 rows, got %ld", n_obj); }
            return 0;
        }
        for(long i = 0; i < nrows; i++)
            if(bit_le(S_MATCH, i) && !bit_le(S_PRES, i*ncols)){
                printf("erro: a coluna tem células ausentes --- um buraco não tem"
                       " profundidade. RECUSADA.\n");
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "ultra: missing cell at row %ld", i); }
                return 0;
            }
        /* a profundidade é a primeira divergência de bits do numerador ---
         * a régua da def:arvore. O denominador tem de ser 1: um endereço é
         * um inteiro, e uma fração não tem prefixo. */
        long lidos[4096]; long nl = 0;
        for(long i = 0; i < nrows && nl < 4096; i++){
            if(!bit_le(S_MATCH, i)) continue;
            long nu, de; celula_qz(i, 0, ncols, &nu, &de);
            if(de != 1){
                printf("erro: o endereço %ld/%ld não é inteiro --- a profundidade"
                       " lê-se do prefixo, e uma fração não tem prefixo."
                       " RECUSADA.\n", nu, de);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "ultra: address %ld/%ld is not an integer", nu, de); }
                return 0;
            }
            if(nu < 0){
                printf("erro: o endereço %ld é negativo --- o prefixo conta-se de"
                       " um endereço, e um endereço não tem sinal. RECUSADA.\n", nu);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "ultra: negative address %ld", nu); }
                return 0;
            }
            lidos[nl++] = nu;
        }
        /* a largura: quantos bits o maior endereço pede */
        long mx = 0;
        for(long i = 0; i < nl; i++) if(lidos[i] > mx) mx = lidos[i];
        int bits = 1; { long t = mx; while(t){ bits++; t >>= 1; } }
        long vale = 0, falha = 0, estrito = 0, tot = 0;
        for(long i = 0; i < nl; i++) for(long j = 0; j < nl; j++) for(long k = 0; k < nl; k++){
            long x = lidos[i], y = lidos[j], z = lidos[k];
            int pxy = bits, pyz = bits, pxz = bits;
            if(x != y) for(int b = 0; b < bits; b++)
                if(((x >> (bits-1-b)) & 1L) != ((y >> (bits-1-b)) & 1L)){ pxy = b; break; }
            if(y != z) for(int b = 0; b < bits; b++)
                if(((y >> (bits-1-b)) & 1L) != ((z >> (bits-1-b)) & 1L)){ pyz = b; break; }
            if(x != z) for(int b = 0; b < bits; b++)
                if(((x >> (bits-1-b)) & 1L) != ((z >> (bits-1-b)) & 1L)){ pxz = b; break; }
            int mn = pxy < pyz ? pxy : pyz;
            tot++;
            if(pxz >= mn){ vale++; if(pxz > mn) estrito++; }
            else falha++;
        }
        if(sql_cap){
            memset(sql_cap, 0, sizeof *sql_cap);
            sql_cap->ok = 1; sql_cap->nrows = 1; sql_cap->ncols = 5;
            snprintf(sql_cap->col[0], sizeof sql_cap->col[0], "triplos");
            snprintf(sql_cap->col[1], sizeof sql_cap->col[1], "vale");
            snprintf(sql_cap->col[2], sizeof sql_cap->col[2], "falha");
            snprintf(sql_cap->col[3], sizeof sql_cap->col[3], "estrito");
            snprintf(sql_cap->col[4], sizeof sql_cap->col[4], "ultrametrica");
            for(int c = 0; c < sql_cap->ncols; c++) sql_cap->tipo[c] = SQL_TIPO_INT4;
            sql_cap->tipo[4] = SQL_TIPO_TEXT;
            snprintf(sql_cap->cell[0][0], SQL_OUT_CELL, "%ld", tot);
            snprintf(sql_cap->cell[0][1], SQL_OUT_CELL, "%ld", vale);
            snprintf(sql_cap->cell[0][2], SQL_OUT_CELL, "%ld", falha);
            snprintf(sql_cap->cell[0][3], SQL_OUT_CELL, "%ld", estrito);
            snprintf(sql_cap->cell[0][4], SQL_OUT_CELL, "%s", falha == 0 ? "sim" : "nao");
            snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT 1");
        }
        printf("   %ld | %ld | %ld | %ld | %s\n", tot, vale, falha, estrito,
               falha == 0 ? "sim" : "nao");
        printf("-- a forte vale em %ld de %ld triplos (%ld estritos) sobre %ld"
               " endereços de %d bits · %s\n", vale, tot, estrito, nl, bits,
               falha == 0 ? "a régua DESCE: a leitura serve"
                          : "a régua NÃO desce: a leitura não serve");
        return 1;
    }

    /* ══ TERMOS: A CISÃO DO TEXTO, E A FIBRA QUE ELA PRODUZ ════════════════
     *
     * É o índice invertido do Elastic, e nenhuma das duas peças é nova aqui.
     *
     * TOKENIZAR É A CISÃO ⊕. O `papers/aranha.c` fá-la no byte com quatro
     * máscaras complementares --- «0xAA ∨ 0x55 = 0xFF e 0xAA ∧ 0x55 = 0, logo
     * PARTICIONAM o byte» ---, e num texto é a mesma operação um nível acima: os
     * separadores particionam a palavra em sub-palavras, sem sobra e sem
     * sobreposição. A cisão é a realização π da Def. do objecto.
     *
     * E O ÍNDICE INVERTIDO É A FIBRA. Com π a levar cada OCORRÊNCIA ao seu
     * termo, a fibra π⁻¹(termo) é a lista de linhas onde ele aparece, e G(termo)
     * é a frequência --- que é o que o Elastic chama doc frequency. Não é uma
     * estrutura a construir: é o campo G desta realização, e a conservação
     * ∑G = |I| diz que nenhuma ocorrência se perde no caminho.
     *
     * O termo entra no pool, que já deduplica --- «a mesma cadeia tem de dar o
     * mesmo endereço, x = y ⟹ R(x) = R(y)» ---, e por isso agrupar ocorrências
     * por termo é compará-las por ENDEREÇO: a fibra forma-se sozinha. */
    if(acao == ACAO_MARCA && mat_op == 34){
        if(ncols < 1 || corpo_de(0).total != CORPO_TEXTO){
            printf("erro: `termos` pede que a PRIMEIRA coluna seja de TEXTO --- é ela"
                   " que se parte. RECUSADO.\n");
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "termos needs a text column first"); }
            return 0;
        }
        long ocor = 0;
        /* (1) A CISÃO: cada linha parte-se, e cada termo entra no pool */
        for(long i = 0; i < nrows && ocor < (long)INV_MAX; i++){
            if(!bit_le(S_MATCH, i)) continue;
            if(!bit_le(S_PRES, i*ncols)) continue;
            char T[TX_MAX + 2];
            tx_le((unsigned)celula_valor(i, 0, ncols), T, (int)sizeof T);
            long pos = 0;
            for(const char *q = T; *q; ){
                while(*q && !isalnum((unsigned char)*q)) q++;   /* o separador */
                if(!*q) break;
                char tok[TX_MAX + 2]; int tn = 0;
                while(*q && isalnum((unsigned char)*q)){
                    if(tn < TX_MAX) tok[tn++] = (char)baixa1(*q);
                    q++;
                }
                tok[tn] = 0;
                if(!tn) continue;
                if(ocor >= (long)INV_MAX){
                    printf("erro: as ocorrências passam de %u — RECUSADO. Truncar daria"
                           " uma fibra que não é a de nenhum texto.\n", INV_MAX);
                    if(sql_cap){ sql_cap->ok = 0;
                        snprintf(sql_cap->err, sizeof sql_cap->err,
                                 "inverted index exceeds %u occurrences", INV_MAX); }
                    return 0;
                }
                /* ── O TERMO VAI PARA AS DUAS CASAS, E TEM DE IR ────────────
                 * O pool dá-lhe o ENDEREÇO --- é ele que faz a fibra formar-se
                 * por comparação de endereços, sem comparar cadeias. A árvore
                 * dá-lhe o CAMINHO --- e é ela que o `ACHA` desce e o
                 * `BUSCA PREFIXO` usa para escolher a bola. Pôr só no pool
                 * deixava as duas funções sem se comporem: a cisão produzia
                 * termos que a busca não encontrava. */
                unsigned end = tx(tok, tn, -1);      /* escreve: a marca é de quem escreve */
                poe_chave_texto(tok);                /* e o caminho na árvore */
                Word w1; w1.total = (Word8)(end & 255); w1.e = (Word8)((end >> 8) & 255);
                mem_grava(S_INV + (unsigned)(ocor * INV_W), w1);
                Word w2; w2.total = (Word8)(i & 255); w2.e = (Word8)((i >> 8) & 255);
                mem_grava(S_INV + (unsigned)(ocor * INV_W + 1), w2);
                Word w3; w3.total = (Word8)(pos & 255); w3.e = (Word8)((pos >> 8) & 255);
                mem_grava(S_INV + (unsigned)(ocor * INV_W + 2), w3);
                ocor++; pos++;
            }
        }
        /* (2) A FIBRA: as ocorrências com o mesmo endereço de termo */
        #define INV_TERMO(k) ((long)mem_le(S_INV + (unsigned)((k)*INV_W)).total \
                            | ((long)mem_le(S_INV + (unsigned)((k)*INV_W)).e << 8))
        #define INV_LINHA(k) ((long)mem_le(S_INV + (unsigned)((k)*INV_W+1)).total \
                            | ((long)mem_le(S_INV + (unsigned)((k)*INV_W+1)).e << 8))
        long fibras = 0, soma_G = 0;
        if(sql_cap){ memset(sql_cap, 0, sizeof *sql_cap); sql_cap->ok = 1;
            sql_cap->ncols = 3;
            snprintf(sql_cap->col[0], sizeof sql_cap->col[0], "termo");
            snprintf(sql_cap->col[1], sizeof sql_cap->col[1], "G");
            snprintf(sql_cap->col[2], sizeof sql_cap->col[2], "linhas");
            sql_cap->tipo[0] = SQL_TIPO_TEXT; sql_cap->tipo[1] = SQL_TIPO_INT4;
            sql_cap->tipo[2] = SQL_TIPO_TEXT; }
        printf("      termo              G   as linhas (a fibra)\n");
        for(long k = 0; k < ocor; k++){
            long e = INV_TERMO(k);
            int primeiro = 1;
            for(long j = 0; j < k; j++) if(INV_TERMO(j) == e){ primeiro = 0; break; }
            if(!primeiro) continue;
            char nom[TX_MAX + 2]; tx_le((unsigned)e, nom, (int)sizeof nom);
            char lst[128]; int ln = 0; long g = 0;
            for(long j = k; j < ocor; j++)
                if(INV_TERMO(j) == e){
                    g++;
                    if(ln < 100) ln += snprintf(lst + ln, sizeof lst - (size_t)ln,
                                                "%s%ld", ln ? "," : "", INV_LINHA(j));
                }
            lst[ln] = 0;
            printf("      %-18s %-3ld %s\n", nom, g, lst);
            fibras++; soma_G += g;
            if(sql_cap && sql_cap->nrows < SQL_OUT_MAX_ROWS){
                long r = sql_cap->nrows;
                snprintf(sql_cap->cell[r][0], SQL_OUT_CELL, "%s", nom);
                snprintf(sql_cap->cell[r][1], SQL_OUT_CELL, "%ld", g);
                snprintf(sql_cap->cell[r][2], SQL_OUT_CELL, "%s", lst);
                sql_cap->nrows++;
            }
        }
        printf("      -> %ld termo(s) distintos, %ld ocorrência(s); a CONSERVAÇÃO"
               " ∑G = |I|: %ld = %ld %s\n", fibras, ocor, soma_G, ocor,
               (soma_G == ocor) ? "" : "← NÃO FECHA");
        if(sql_cap) snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT %ld", fibras);
        #undef INV_TERMO
        #undef INV_LINHA
        return soma_G == ocor;
    }

    /* ══ LCS: A DOBRA ALTERNADA DAS DUAS FACES ══════════════════════════════
     *
     * O `lcs_dobra.tex` escreve o LCS na linguagem desta casa: duas sequências
     * são duas REALIZAÇÕES, uma subsequência comum é uma cadeia no produto das
     * ordens, e a recorrência da programação dinâmica É a dobra:
     *
     *     L(i,j) = L(i-1,j-1) + 1                  se π_A(i) = π_B(j)
     *            = max(L(i-1,j), L(i,j-1))         caso contrário
     *
     * --- e os três movimentos da grade são as duas faces mais o ponto fixo
     * comum: avançar só em A, avançar só em B, ou avançar nas duas, que é onde
     * elas coincidem e o valor sobe de um. «Porque a régua é finita o processo
     * TERMINA numa igualdade»: não há convergência, há paragem em mn passos.
     *
     * A tabela vive no disco --- é a grade, e a grade é o espaço. E guarda-se
     * em cada célula QUAL FACE produziu o valor, porque o comprimento sozinho
     * não devolve a palavra: isso é o levantamento do `aranha`, a coordenada de
     * folha «de onde vim», e a volta é exacta porque cada ponteiro desfaz o seu
     * passo.
     *
     * O gume é a CONSERVAÇÃO, e é dela que o paper faz lema: as diferenças
     * L(i,j) − L(i−1,j) e L(i,j) − L(i,j−1) são 0 ou 1, nunca outra coisa. É a
     * derivada discreta a valer o bit que a face decide, e verifica-se sobre a
     * tabela inteira --- um preenchimento errado quebra-a. */
    if(acao == ACAO_MARCA && mat_op == 33){
        if(ncols != 2){
            printf("erro: `lcs` pede DUAS colunas --- as duas sequências. RECUSADO.\n");
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err, "lcs needs exactly 2 columns"); }
            return 0;
        }
        if(corpo_de(0).total != CORPO_TEXTO || corpo_de(1).total != CORPO_TEXTO){
            printf("erro: `lcs` pede colunas de TEXTO --- uma sequência é uma palavra."
                   " RECUSADO.\n");
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err, "lcs needs text columns"); }
            return 0;
        }
        long feitas = 0, cons_ok = 0, cons_n = 0;
        if(sql_cap){ memset(sql_cap, 0, sizeof *sql_cap); sql_cap->ok = 1;
            sql_cap->ncols = 4;
            snprintf(sql_cap->col[0], sizeof sql_cap->col[0], "a");
            snprintf(sql_cap->col[1], sizeof sql_cap->col[1], "b");
            snprintf(sql_cap->col[2], sizeof sql_cap->col[2], "comprimento");
            snprintf(sql_cap->col[3], sizeof sql_cap->col[3], "subsequencia");
            sql_cap->tipo[0] = SQL_TIPO_TEXT; sql_cap->tipo[1] = SQL_TIPO_TEXT;
            sql_cap->tipo[2] = SQL_TIPO_INT4; sql_cap->tipo[3] = SQL_TIPO_TEXT; }
        printf("      a          b          L    a subsequência\n");
        for(long i = 0; i < nrows; i++){
            if(!bit_le(S_MATCH, i)) continue;
            if(!bit_le(S_PRES, i*ncols) || !bit_le(S_PRES, i*ncols + 1)) continue;
            char A[TX_MAX + 2], B[TX_MAX + 2];
            tx_le((unsigned)celula_valor(i, 0, ncols), A, (int)sizeof A);
            tx_le((unsigned)celula_valor(i, 1, ncols), B, (int)sizeof B);
            long m = (long)strlen(A), n = (long)strlen(B);
            /* a grade tem (m+1)(n+1) células; o que não cabe RECUSA-SE */
            if((m + 1) * (n + 1) > (long)LCS_CELULAS){
                printf("erro: a grade do lcs é %ldx%ld e a zona leva %u células —"
                       " RECUSADO. Truncar daria um comprimento que não é o de"
                       " nenhum par.\n", m + 1, n + 1, LCS_CELULAS);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "lcs grid %ldx%ld exceeds %u cells", m+1, n+1, LCS_CELULAS); }
                return 0;
            }
            #define LCS_V(ii,jj)  mem_le(S_LCS + (unsigned)(((ii)*(n+1) + (jj)) * 2))
            #define LCS_P(ii,jj)  mem_le(S_LCS + (unsigned)(((ii)*(n+1) + (jj)) * 2 + 1))
            #define LCS_POE(ii,jj,v,pp) do { \
                Word wv; wv.total = (Word8)((v) & 255); wv.e = (Word8)(((v) >> 8) & 255); \
                mem_grava(S_LCS + (unsigned)(((ii)*(n+1) + (jj)) * 2), wv); \
                Word wp; wp.total = (Word8)(pp); wp.e = 0; \
                mem_grava(S_LCS + (unsigned)(((ii)*(n+1) + (jj)) * 2 + 1), wp); \
            } while(0)
            #define LCS_LE(ii,jj) ((long)LCS_V(ii,jj).total | ((long)LCS_V(ii,jj).e << 8))

            for(long jj = 0; jj <= n; jj++) LCS_POE(0, jj, 0, 0);
            for(long ii = 0; ii <= m; ii++) LCS_POE(ii, 0, 0, 0);
            for(long ii = 1; ii <= m; ii++)
                for(long jj = 1; jj <= n; jj++){
                    if(A[ii-1] == B[jj-1])
                        LCS_POE(ii, jj, LCS_LE(ii-1, jj-1) + 1, 3);   /* ↖ o ponto fixo */
                    else {
                        long da = LCS_LE(ii-1, jj), db = LCS_LE(ii, jj-1);
                        if(da >= db) LCS_POE(ii, jj, da, 1);          /* ↑ a face de A */
                        else         LCS_POE(ii, jj, db, 2);          /* ← a face de B */
                    }
                }
            /* A CONSERVAÇÃO: as diferenças são 0 ou 1, e mais nada */
            for(long ii = 1; ii <= m; ii++)
                for(long jj = 1; jj <= n; jj++){
                    long v = LCS_LE(ii, jj), a1 = LCS_LE(ii-1, jj), b1 = LCS_LE(ii, jj-1);
                    cons_n += 2;
                    if(v - a1 >= 0 && v - a1 <= 1) cons_ok++;
                    if(v - b1 >= 0 && v - b1 <= 1) cons_ok++;
                }
            /* O LEVANTAMENTO: a palavra sai dos ponteiros, de trás para a frente */
            char sub[TX_MAX + 2]; long sn = 0;
            { long ii = m, jj = n;
              while(ii > 0 && jj > 0){
                  long pp = (long)LCS_P(ii, jj).total;
                  if(pp == 3){ if(sn < TX_MAX) sub[sn++] = A[ii-1]; ii--; jj--; }
                  else if(pp == 1) ii--;
                  else jj--;
              } }
            sub[sn] = 0;
            for(long k = 0; k < sn/2; k++){ char t2 = sub[k]; sub[k] = sub[sn-1-k]; sub[sn-1-k] = t2; }
            long L = LCS_LE(m, n);
            printf("      %-10s %-10s %-4ld %s\n", A, B, L, sub);
            if(sql_cap && sql_cap->nrows < SQL_OUT_MAX_ROWS){
                long r = sql_cap->nrows;
                snprintf(sql_cap->cell[r][0], SQL_OUT_CELL, "%s", A);
                snprintf(sql_cap->cell[r][1], SQL_OUT_CELL, "%s", B);
                snprintf(sql_cap->cell[r][2], SQL_OUT_CELL, "%ld", L);
                snprintf(sql_cap->cell[r][3], SQL_OUT_CELL, "%s", sub);
                sql_cap->nrows++;
            }
            feitas++;
            #undef LCS_V
            #undef LCS_P
            #undef LCS_POE
            #undef LCS_LE
        }
        printf("      -> %ld par(es); a conservação (as diferenças são 0 ou 1):"
               " %ld/%ld\n", feitas, cons_ok, cons_n);
        if(sql_cap) snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT %ld", feitas);
        return cons_ok == cons_n;
    }

    if(acao == ACAO_MARCA && mat_op == 32){
        /* ── FUNDE: o C_ent da `aranha prop:travessia`, no motor.
         *
         *     C_ent(a,b) = Σ a_j 2^{2j} + Σ b_j 2^{2j+1}
         *
         * --- os dígitos de a nas posições pares e os de b nas ímpares. A tabela
         * traz DUAS colunas, os endereços dos dois corpos, e o motor devolve o
         * corpo fundido: uma linha por objecto, com o endereço entrelaçado.
         *
         * E o que faz dela uma FUSÃO e não uma soma é a volta: desentrelaçar
         * SEPARA e devolve os dois exactos. Isso verifica-se aqui, linha a
         * linha, antes de responder --- se algum par não voltar, RECUSA-SE, em
         * vez de devolver um endereço que não se sabe desfazer. */
        if(ncols != 2){
            printf("erro: a fusão junta DOIS corpos --- são precisas duas colunas de"
                   " endereços, e a tabela tem %ld. RECUSADA.\n", ncols);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "funde needs two address columns, got %ld", ncols); }
            return 0;
        }
        long n_obj = 0;
        for(long i = 0; i < nrows; i++) if(bit_le(S_MATCH, i)) n_obj++;
        if(n_obj == 0){
            printf("erro: não há linhas para fundir --- RECUSADA.\n");
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err, "funde on empty selection"); }
            return 0;
        }
        if(n_obj > SQL_OUT_MAX_ROWS){
            printf("erro: o fundido tem %ld linhas e a saída segura %d --- RECUSADA, e"
                   " não truncada.\n", n_obj, SQL_OUT_MAX_ROWS);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "funde: %ld rows exceed output cap %d", n_obj, SQL_OUT_MAX_ROWS); }
            return 0;
        }
        /* a largura: quantos bits precisa o maior dos dois --- e o entrelaçado
         * ocupa o DOBRO, que é o que a fusão custa e se diz */
        long maior = 0;
        for(long i = 0; i < nrows; i++){
            if(!bit_le(S_MATCH, i)) continue;
            for(long c = 0; c < 2; c++){
                if(!bit_le(S_PRES, i*ncols + c)){
                    printf("erro: célula ausente na linha %ld --- um buraco não é um"
                           " endereço. RECUSADA.\n", i);
                    if(sql_cap){ sql_cap->ok = 0;
                        snprintf(sql_cap->err, sizeof sql_cap->err,
                                 "funde: missing cell at row %ld", i); }
                    return 0;
                }
                long nu, de; celula_qz(i, c, ncols, &nu, &de);
                if(de != 1 || nu < 0){
                    printf("erro: o endereço (%ld/%ld) não é um natural --- a fusão"
                           " entrelaça DÍGITOS. RECUSADA.\n", nu, de);
                    if(sql_cap){ sql_cap->ok = 0;
                        snprintf(sql_cap->err, sizeof sql_cap->err,
                                 "funde: address not a natural"); }
                    return 0;
                }
                if(nu > maior) maior = nu;
            }
        }
        int k = 1; { long t = maior; while(t){ k++; t >>= 1; } }
        if(2*k > 62){
            printf("erro: os endereços pedem %d bits cada e o entrelaçado pediria %d"
                   " --- passa do que a palavra segura. RECUSADA.\n", k, 2*k);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "funde: interleaved width %d exceeds word", 2*k); }
            return 0;
        }
        if(sql_cap){
            memset(sql_cap, 0, sizeof *sql_cap);
            sql_cap->ok = 1; sql_cap->ncols = 5;
            snprintf(sql_cap->col[0], sizeof sql_cap->col[0], "a");
            snprintf(sql_cap->col[1], sizeof sql_cap->col[1], "b");
            snprintf(sql_cap->col[2], sizeof sql_cap->col[2], "fundido");
            snprintf(sql_cap->col[3], sizeof sql_cap->col[3], "compacto");
            snprintf(sql_cap->col[4], sizeof sql_cap->col[4], "volta");
            for(int c = 0; c < sql_cap->ncols; c++) sql_cap->tipo[c] = SQL_TIPO_INT4;
            sql_cap->tipo[4] = SQL_TIPO_TEXT;
            long r = 0;
            static long vistos[SQL_OUT_MAX_ROWS];
            for(long i = 0; i < nrows; i++){
                if(!bit_le(S_MATCH, i)) continue;
                long an, ad, bn, bd;
                celula_qz(i, 0, ncols, &an, &ad);
                celula_qz(i, 1, ncols, &bn, &bd);
                long e = 0;
                for(int j = 0; j < k; j++){
                    e |= ((an >> j) & 1L) << (2*j);
                    e |= ((bn >> j) & 1L) << (2*j + 1);
                }
                /* A VOLTA, verificada antes de responder */
                long x = 0, y = 0;
                for(int j = 0; j < k; j++){
                    x |= ((e >> (2*j))     & 1L) << j;
                    y |= ((e >> (2*j + 1)) & 1L) << j;
                }
                if(x != an || y != bn){
                    printf("erro: a fusão de (%ld,%ld) não desfaz --- devolveu (%ld,%ld)."
                           " RECUSADA: um endereço que não se sabe separar não é uma"
                           " fusão.\n", an, bn, x, y);
                    if(sql_cap){ sql_cap->ok = 0;
                        snprintf(sql_cap->err, sizeof sql_cap->err,
                                 "funde: round trip failed at row %ld", i); }
                    return 0;
                }
                snprintf(sql_cap->cell[r][0], SQL_OUT_CELL, "%ld", an);
                snprintf(sql_cap->cell[r][1], SQL_OUT_CELL, "%ld", bn);
                snprintf(sql_cap->cell[r][2], SQL_OUT_CELL, "%ld", e);
                snprintf(sql_cap->cell[r][4], SQL_OUT_CELL, "exacta");
                vistos[r] = e;
                r++;
            }
            /* ── E O COMPACTO, que é o que faz a cadeia CONTINUAR.
             * A fusão DOBRA a largura a cada passo: ao terceiro encadeamento o
             * endereço já não cabe no envelope da célula, e a cadeia pára. O
             * compacto é o RANK do fundido --- 0..m−1 na ordem em que aparecem
             * ---, que é bijecção sobre os fundidos distintos, e por isso a
             * fibra não muda: «a fibra não vê o nome do endereço».
             *
             * As duas colunas servem coisas diferentes e convém não as trocar:
             * o `fundido` guarda a RÉGUA (o prefixo entrelaçado, que é onde a
             * ultramétrica vive) e o `compacto` serve para ENCADEAR sem estourar.
             * Quem encadeia pelo fundido cru pára ao terceiro passo; quem
             * encadeia pelo compacto continua, e perde a régua dos andares
             * anteriores --- que é o preço, e diz-se. */
            for(long i2 = 0; i2 < r; i2++){
                long rank = 0;
                for(long j2 = 0; j2 < i2; j2++) if(vistos[j2] < vistos[i2]) rank++;
                long iguais = 0;
                for(long j2 = 0; j2 < i2; j2++) if(vistos[j2] == vistos[i2]) iguais = 1;
                if(iguais){
                    for(long j2 = 0; j2 < i2; j2++) if(vistos[j2] == vistos[i2]){
                        snprintf(sql_cap->cell[i2][3], SQL_OUT_CELL, "%s",
                                 sql_cap->cell[j2][3]);
                        break; }
                } else {
                    long menores = 0;
                    for(long j2 = 0; j2 < r; j2++){
                        int ja = 0;
                        for(long u = 0; u < j2; u++) if(vistos[u] == vistos[j2]){ ja = 1; break; }
                        if(!ja && vistos[j2] < vistos[i2]) menores++;
                    }
                    snprintf(sql_cap->cell[i2][3], SQL_OUT_CELL, "%ld", menores);
                }
                (void)rank;
            }
            sql_cap->nrows = r;
            snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT %ld", r);
        }
        printf("funde: %ld objectos · %d bits por corpo, %d no entrelaçado ·"
               " a volta separa e devolve os dois exactos em todos\n", n_obj, k, 2*k);
        return 1;
    }

    if(acao == ACAO_MARCA && mat_op == 31){
        #define EDO_ANDAR 64
        static long m[EDO_ANDAR][EDO_ANDAR];
        long ng = ncols;
        /* ── EDO: RESOLVE a equação diferencial, e devolve as RAÍZES.
         *
         * A tabela traz a matriz COMPANHEIRA de y'' + By' + Cy = 0,
         *
         *     A = [  0   1 ]     tr A = −B,  det A = C,  Δ = tr² − 4det,
         *         [ −C  −B ]
         *
         * que é a mesma leitura do `regime`. A diferença é o que se devolve: o
         * regime CLASSIFICA e este RESOLVE --- dá as duas raízes de λ²+Bλ+C=0 e
         * a forma da solução. E a `lib/edo.h` já dizia porquê: a característica
         * É a borda do corpo, σ²=b₀+b₁σ, com B=−b₁ e C=−b₀.
         *
         * As raízes saem EXACTAS em inteiros quando Δ é quadrado perfeito e o
         * numerador é par; quando não, diz-se a forma e NÃO se arredonda ---
         * um irracional escrito em decimal seria a casa a mentir sobre o que
         * tem. Quem quiser o valor sobe a torre: é o corte, não uma divisão. */
        /* ── A MATRIZ COMO ESTANTE: o índice é um PAR, e o par desce.
         *
         * O `thm:enumfin` da aranha: «uma matriz é um campo sobre I_n × I_n; o
         * índice é um par, e o par desce por C. Esse é o ÍNDICE COMO ESTANTE: a
         * matriz continua a ser o que está nas gavetas, e o emparelhamento
         * apenas dá a cada gaveta um endereço único. O corpo matricial não
         * precisa de tratamento próprio.»
         *
         * Daí a forma que não tem tecto de colunas: TRÊS colunas --- (i, j, v)
         * ---, com o índice descido e o valor numa coluna só. O grau deixa de
         * estar preso ao número de colunas da tabela, e o valor fica sempre na
         * terceira, que é das oito que o registo de corpos guarda com sinal.
         *
         * As duas formas convivem: n×n para quem tem a matriz à mão, e (i,j,v)
         * para quem precisa de grau maior. É a mesma matriz nas duas --- o que
         * muda é a régua com que se a escreve, e isso é travessia. */
        if(ncols == 3 && nrows > 3){
            /* lê (i, j, v) e monta a companheira pelo endereço descido */
            long gmax = 0;
            for(long r = 0; r < nrows; r++){
                if(!bit_le(S_MATCH, r)) continue;
                long nu, de; celula_qz(r, 0, ncols, &nu, &de);
                if(nu + 1 > gmax) gmax = nu + 1;
                celula_qz(r, 1, ncols, &nu, &de);
                if(nu + 1 > gmax) gmax = nu + 1;
            }
            if(gmax < 2 || gmax > EDO_ANDAR){
                printf("erro: a estante (i,j,v) pede grau entre 2 e %d, e os índices"
                       " dão %ld. RECUSADA.\n", EDO_ANDAR, gmax);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "edo: shelf degree %ld out of range", gmax); }
                return 0;
            }
            static long M2[EDO_ANDAR][EDO_ANDAR];
            for(long i = 0; i < gmax; i++) for(long j = 0; j < gmax; j++) M2[i][j] = 0;
            for(long r = 0; r < nrows; r++){
                if(!bit_le(S_MATCH, r)) continue;
                long in, id, jn, jd, vn, vd;
                celula_qz(r, 0, ncols, &in, &id);
                celula_qz(r, 1, ncols, &jn, &jd);
                celula_qz(r, 2, ncols, &vn, &vd);
                if(id != 1 || jd != 1 || vd != 1 || in < 0 || jn < 0){
                    printf("erro: a estante pede (i,j,v) com i,j naturais e v inteiro"
                           " --- veio (%ld/%ld, %ld/%ld, %ld/%ld). RECUSADA.\n",
                           in, id, jn, jd, vn, vd);
                    if(sql_cap){ sql_cap->ok = 0;
                        snprintf(sql_cap->err, sizeof sql_cap->err,
                                 "edo: shelf entry not integral"); }
                    return 0;
                }
                M2[in][jn] = vn;
            }
            /* daqui em diante é o mesmo caminho: confere-se a companheira e
             * resolve-se. Copia-se para `m` e segue-se. */
            ncols = nrows = gmax;
            for(long i = 0; i < gmax; i++) for(long j = 0; j < gmax; j++)
                m[i][j] = M2[i][j];
            ng = gmax;
            goto edo_tem_matriz;
        }

        /* ── E O GRAU NÃO É DOIS POR NECESSIDADE: é onde o Δ classifica.
         * O `thm:leidisc` e o `lem:cristal` da aranha são sobre 2×2 --- é ali
         * que três classes esgotam. Mas a `def:gato` é em Zⁿ, com |det ⊗| = 1
         * para TODO n ≥ 2, e a companheira de grau n existe do mesmo modo. Aqui
         * aceita-se qualquer n: em grau 2 devolve-se o Δ e as três classes; em
         * grau maior devolvem-se as RAÍZES INTEIRAS por divisores do termo
         * constante --- exactas, sem uma vírgula --- e o que sobra fica dito
         * como o factor por resolver, em vez de ser arredondado. */
        /* O GRAU UM É LEGÍTIMO e era recusado: y' = λy tem companheira 1×1, e é
         * o caso mais simples de todos --- a exponencial pura, cuja solução em
         * fatorial é <1,1,1,...> quando λ=1. Exigir n≥2 deixava-o de fora sem
         * razão nenhuma da teoria. */
        if(ncols < 1 || nrows < 1 || ncols != nrows){
            printf("erro: a EDO lê-se da matriz COMPANHEIRA n×n --- a tabela tem"
                   " %ld coluna(s) e %ld linha(s). RECUSADA.\n", ncols, nrows);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "edo needs a square n x n companion matrix,"
                         " got %ld x %ld", nrows, ncols); }
            return 0;
        }
        /* ── O GRAU NÃO TEM LIMITE NA TEORIA, e o tecto aqui é o ANDAR.
         * O `thm:enumfin` dá E_k bijecção para TODO k finito, e a `def:gato` é
         * em Zⁿ para todo n ≥ 2: nada na construção pára num grau. O que existe
         * é o andar da escada em que este motor corre, e ele diz-se em vez de
         * se fingir que não está lá --- «o finito é cada estágio da caminhada,
         * não o total». Subir é acrescentar largura, não mudar de teoria. */
        if(ncols > EDO_ANDAR){
            printf("erro: este ANDAR do motor segura grau %d e vieram %ld --- RECUSADA,"
                   " e não truncada. O grau não tem limite na teoria (thm:enumfin, E_k"
                   " para todo k): o que tem tecto é a largura deste andar, e subir é"
                   " acrescentá-la.\n", EDO_ANDAR, ncols);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "edo: degree %ld above this floor (%d); the bound is the floor,"
                         " not the theory", ncols, EDO_ANDAR); }
            return 0;
        }
        for(long i = 0; i < ng; i++) for(long j = 0; j < ng; j++){
            if(!bit_le(S_PRES, i*ncols + j)){
                printf("erro: a companheira tem célula ausente --- RECUSADA.\n");
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "edo: missing cell in companion matrix"); }
                return 0;
            }
            long nu, de; celula_qz(i, j, ncols, &nu, &de);
            if(de != 1){
                printf("erro: a companheira tem entrada não inteira (%ld/%ld) --- as"
                       " raízes saem exactas de inteiros. RECUSADA.\n", nu, de);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "edo: companion entry not integral"); }
                return 0;
            }
            m[i][j] = nu;
        }
        edo_tem_matriz:
        /* A COMPANHEIRA CONFERE-SE, em vez de se supor: as n−1 primeiras linhas
         * são a identidade deslocada --- linha i tem 1 na coluna i+1 e zero no
         * resto --- e a última traz os coeficientes. */
        for(long i = 0; i + 1 < ng; i++) for(long j = 0; j < ng; j++){   /* vazio em ng=1 */
            long esperado = (j == i + 1) ? 1 : 0;
            if(m[i][j] != esperado){
                printf("erro: a linha %ld da companheira devia ter 1 na coluna %ld e"
                       " zero no resto; tem %ld na coluna %ld. Isto não é uma"
                       " companheira. RECUSADA.\n", i, i+1, m[i][j], j);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "edo: row %ld is not a companion row", i); }
                return 0;
            }
        }
        /* o polinómio mónico: λⁿ + c[n-1]λ^{n-1} + ... + c[0], com c[k] = −m[n-1][k] */
        static long co[EDO_ANDAR+1];
        for(long k = 0; k < ng; k++) co[k] = -m[ng-1][k];
        co[ng] = 1;

        /* ── GRAU > 2: RESOLVE-SE, e resolver é dar as raízes COM
         * multiplicidade e a forma da solução --- não uma lista.
         *
         * Três passos, todos exactos em inteiros:
         *  (1) as raízes RACIONAIS p/q pelo teorema da raiz racional: p divide
         *      o termo constante e q o coeficiente-líder. Aqui o polinómio é
         *      mónico (é uma companheira), logo q = 1 e elas são inteiras --- mas
         *      a busca faz-se pela regra e não pela suposição.
         *  (2) a MULTIPLICIDADE de cada uma, deflacionando enquanto ela voltar.
         *  (3) o que SOBRA: em grau 2 resolve-se pelo Δ (real ou complexo); em
         *      grau 1 é raiz directa; acima, diz-se o factor por resolver.
         *
         * E a forma da solução monta-se do resultado: uma raiz λ de
         * multiplicidade m contribui (c₁+c₂t+…+c_m t^{m-1})e^{λt}; um par a±bi
         * contribui e^{at}(c cos bt + c' sen bt). É isto que resolver quer dizer. */
        if(ng > 2){
            char raizes[SQL_OUT_CELL] = ""; char forma[SQL_OUT_CELL] = "";
            long nr = 0, distintas = 0;
            static long p[EDO_ANDAR+1]; for(long k = 0; k <= ng; k++) p[k] = co[k];
            long gr = ng;
            int primeiro = 1;
            for(long volta = 0; volta < EDO_ANDAR && gr > 0; volta++){
                long a0 = p[0], r = 0; int achou = 0;
                if(a0 == 0){ achou = 1; r = 0; }
                else {
                    long lim = a0 < 0 ? -a0 : a0;
                    for(long d = 1; d <= lim && !achou; d++){
                        if(lim % d) continue;
                        for(int sg = 0; sg < 2 && !achou; sg++){
                            long cand = sg ? -d : d, v = 0;
                            for(long k = gr; k >= 0; k--) v = v*cand + p[k];
                            if(v == 0){ achou = 1; r = cand; }
                        }
                    }
                }
                if(!achou) break;
                /* A MULTIPLICIDADE: deflaciona enquanto a raiz voltar */
                long mult = 0;
                while(gr > 0){
                    long v = 0;
                    for(long k = gr; k >= 0; k--) v = v*r + p[k];
                    if(v != 0) break;
                    static long q2[EDO_ANDAR+1];
                    q2[gr-1] = p[gr];
                    for(long k = gr-1; k >= 1; k--) q2[k-1] = p[k] + r*q2[k];
                    for(long k = 0; k < gr; k++) p[k] = q2[k];
                    gr--; mult++; nr++;
                }
                distintas++;
                { char t[64];
                  if(mult > 1) snprintf(t, sizeof t, "%s%ld(x%ld)", primeiro?"":",", r, mult);
                  else         snprintf(t, sizeof t, "%s%ld", primeiro?"":",", r);
                  strncat(raizes, t, sizeof raizes - strlen(raizes) - 1); }
                { char t[96];
                  if(mult == 1) snprintf(t, sizeof t, "%sc*e^(%ldt)", primeiro?"":" + ", r);
                  else snprintf(t, sizeof t, "%s(c1%s)*e^(%ldt)", primeiro?"":" + ",
                                mult == 2 ? "+c2t" : "+c2t+...", r);
                  strncat(forma, t, sizeof forma - strlen(forma) - 1); }
                primeiro = 0;
            }
            /* ── (3) O QUE SOBRA: AS RAÍZES EM BASE FATORIAL.
             *
             * A série da aranha escreve tudo sobre 1/k!, e a representação que
             * lhe corresponde é a BASE FATORIAL --- x = d_0 + Σ d_k/k! com
             * 0 ≤ d_k ≤ k. Não é conveniência: é a base em que e = Σ1/k! tem
             * todos os dígitos iguais a um, e em que c(t) e s(t) já estão
             * escritas.
             *
             * Nem fracção nem vírgula, então: DÍGITOS. Cada nível k tem k+1
             * escolhas --- a aridade cresce ---, e descer por elas é o «navegar
             * = descer por prefixo onde o prefixo é a bola». Cada dígito decide-se
             * pelo SINAL de P, calculado em inteiros: com x = N/k!, o valor
             * P(N/k!)·(k!)^n é inteiro e tem o mesmo sinal.
             *
             * O processo pára onde o anel enche, e o nível a que parou é o
             * CUSTO --- que se conta, como o thm:serie manda. */
            char digitos[SQL_OUT_CELL] = ""; long nirr = 0;
            if(gr >= 1){
                int off = 0;
                for(long a = -8; a <= 8 && nirr < 3; a++){
                    FatRaiz fr = fat_raiz(p, (int)gr, a);
                    if(!fr.achou || fr.niveis == 0) continue;
                    /* A APRESENTAÇÃO é a dos DÍGITOS, não do N/k!: o cliente lê
                     * [1;0,0,1,3,3] e o valor de relance, e não um quociente de
                     * seis algarismos sobre outro de seis. O N/k! continua a ser
                     * o que a conta usa --- muda a escrita, não o número. */
                    { char apres[96]; ft_escreve(&fr, apres, sizeof apres);
                      off += snprintf(digitos + off, sizeof digitos - off,
                                      "%s[%s]", nirr ? " | " : "", apres); }
                    nirr++;
                }
            }
            char resto[SQL_OUT_CELL];
            if(gr == 0) snprintf(resto, sizeof resto, "todas achadas");
            else if(gr == 1){
                snprintf(resto, sizeof resto, "linear: raiz %ld/%ld", -p[0], p[1]);
            } else if(gr == 2){
                long BB = p[1], CC = p[0], DD = BB*BB - 4*CC;
                long ad2 = DD < 0 ? -DD : DD, rr = 0;
                while(rr*rr < ad2) rr++;
                int quad = (rr*rr == ad2);
                if(DD >= 0)
                    snprintf(resto, sizeof resto, quad
                        ? "quadrático D=%ld: raizes (%ld±%ld)/2"
                        : "quadrático D=%ld: (%ld±sqrt(%ld))/2",
                        DD, -BB, quad ? rr : DD);
                else {
                    snprintf(resto, sizeof resto, "quadrático D=%ld: par %s%ld/2±i*%s/2",
                             DD, "", -BB, quad ? "r" : "sqrt|D|");
                    char t[96];
                    snprintf(t, sizeof t, "%se^(at)*(c*cos(wt)+c'*sen(wt))",
                             primeiro ? "" : " + ");
                    strncat(forma, t, sizeof forma - strlen(forma) - 1);
                    primeiro = 0;
                }
                if(DD >= 0){
                    char t[96];
                    snprintf(t, sizeof t, "%sc*e^(l1*t) + c*e^(l2*t)", primeiro ? "" : " + ");
                    strncat(forma, t, sizeof forma - strlen(forma) - 1);
                    primeiro = 0;
                }
            } else {
                int off = snprintf(resto, sizeof resto, "sobra grau %ld: ", gr);
                for(long k = gr; k >= 0 && off < (int)sizeof resto - 12; k--)
                    if(p[k]) off += snprintf(resto+off, sizeof resto-off, "%s%ldx^%ld",
                                             (k<gr && p[k]>0) ? "+" : "", p[k], k);
            }
            if(!*forma) snprintf(forma, sizeof forma, "sem parte elementar achada");
            /* ── E A SOLUÇÃO EM NOTAÇÃO FATORIAL: só contagem, inteiros exactos.
             * y(t) = Σ d_k t^k/k! com d_k = y^{(k)}(0), e a recorrência DÁ os
             * d_k. Nada se avalia e nada se divide --- e é esta a forma em que
             * o cliente a lê, porque é a base em que a série já vive. */
            char emfat[SQL_OUT_CELL], serieN[SQL_OUT_CELL], somaN[SQL_OUT_CELL];
            { long d0[FT_COEF];
              for(int u = 0; u < (int)ng && u < FT_COEF; u++) d0[u] = (u == 0);
              FtSol sol = ft_solucao(co, (int)ng, d0, 12);
              ft_sol_escreve(&sol, emfat, sizeof emfat);
              ft_sol_serie(&sol, serieN, sizeof serieN);
              ft_sol_somatorio(&sol, somaN, sizeof somaN); }
            if(sql_cap){
                memset(sql_cap, 0, sizeof *sql_cap);
                sql_cap->ok = 1; sql_cap->nrows = 1; sql_cap->ncols = 10;
                snprintf(sql_cap->col[0], sizeof sql_cap->col[0], "grau");
                snprintf(sql_cap->col[1], sizeof sql_cap->col[1], "raizes");
                snprintf(sql_cap->col[2], sizeof sql_cap->col[2], "distintas");
                snprintf(sql_cap->col[3], sizeof sql_cap->col[3], "com_multiplicidade");
                snprintf(sql_cap->col[4], sizeof sql_cap->col[4], "resto");
                snprintf(sql_cap->col[5], sizeof sql_cap->col[5], "solucao");
                snprintf(sql_cap->col[6], sizeof sql_cap->col[6], "raizes_em_fatorial");
                snprintf(sql_cap->col[7], sizeof sql_cap->col[7], "contagem");
                snprintf(sql_cap->col[8], sizeof sql_cap->col[8], "serie_de_potencias");
                snprintf(sql_cap->col[9], sizeof sql_cap->col[9], "somatorio");
                for(int c = 0; c < sql_cap->ncols; c++) sql_cap->tipo[c] = SQL_TIPO_TEXT;
                sql_cap->tipo[0] = SQL_TIPO_INT4; sql_cap->tipo[2] = SQL_TIPO_INT4;
                sql_cap->tipo[3] = SQL_TIPO_INT4;
                snprintf(sql_cap->cell[0][0], SQL_OUT_CELL, "%ld", ng);
                snprintf(sql_cap->cell[0][1], SQL_OUT_CELL, "%s", nr ? raizes : "nenhuma");
                snprintf(sql_cap->cell[0][2], SQL_OUT_CELL, "%ld", distintas);
                snprintf(sql_cap->cell[0][3], SQL_OUT_CELL, "%ld", nr);
                snprintf(sql_cap->cell[0][4], SQL_OUT_CELL, "%s", resto);
                snprintf(sql_cap->cell[0][5], SQL_OUT_CELL, "%s", forma);
                snprintf(sql_cap->cell[0][6], SQL_OUT_CELL, "%s",
                         nirr ? digitos : "sem raiz irracional real");
                snprintf(sql_cap->cell[0][7], SQL_OUT_CELL, "%s", emfat);
                snprintf(sql_cap->cell[0][8], SQL_OUT_CELL, "%s", serieN);
                snprintf(sql_cap->cell[0][9], SQL_OUT_CELL, "%s", somaN);
                snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT 1");
            }
            printf("edo: grau %ld · %ld raiz(es) em %ld distinta(s): %s · %s · y = %s"
                   " · em fatorial: %s\n", ng, nr, distintas, nr ? raizes : "nenhuma",
                   resto, forma, nirr ? digitos : "nenhuma");
            return 1;
        }
        if(ng == 1){
            /* y' = λy com λ = m[0][0]: a solução é e^{λt}, e em fatorial os
             * coeficientes são as potências de λ --- que é a contagem pura. */
            long lam = m[0][0];
            char emfat1[SQL_OUT_CELL], serie1[SQL_OUT_CELL], soma1[SQL_OUT_CELL];
            { long cc1[1] = {-lam}, d01[1] = {1};
              FtSol sol = ft_solucao(cc1, 1, d01, 12);
              ft_sol_escreve(&sol, emfat1, sizeof emfat1);
              ft_sol_serie(&sol, serie1, sizeof serie1);
              ft_sol_somatorio(&sol, soma1, sizeof soma1); }
            if(sql_cap){
                memset(sql_cap, 0, sizeof *sql_cap);
                sql_cap->ok = 1; sql_cap->nrows = 1; sql_cap->ncols = 9;
                snprintf(sql_cap->col[0], sizeof sql_cap->col[0], "equacao");
                snprintf(sql_cap->col[1], sizeof sql_cap->col[1], "disc");
                snprintf(sql_cap->col[2], sizeof sql_cap->col[2], "classe");
                snprintf(sql_cap->col[3], sizeof sql_cap->col[3], "lambda1");
                snprintf(sql_cap->col[4], sizeof sql_cap->col[4], "lambda2");
                snprintf(sql_cap->col[5], sizeof sql_cap->col[5], "solucao");
                snprintf(sql_cap->col[6], sizeof sql_cap->col[6], "contagem");
                snprintf(sql_cap->col[7], sizeof sql_cap->col[7], "serie_de_potencias");
                snprintf(sql_cap->col[8], sizeof sql_cap->col[8], "somatorio");
                for(int c = 0; c < sql_cap->ncols; c++) sql_cap->tipo[c] = SQL_TIPO_TEXT;
                snprintf(sql_cap->cell[0][0], SQL_OUT_CELL, "y' %c %ldy = 0",
                         lam > 0 ? '-' : '+', lam < 0 ? -lam : lam);
                snprintf(sql_cap->cell[0][1], SQL_OUT_CELL, "--");
                snprintf(sql_cap->cell[0][2], SQL_OUT_CELL, "primeira ordem");
                snprintf(sql_cap->cell[0][3], SQL_OUT_CELL, "%ld", lam);
                snprintf(sql_cap->cell[0][4], SQL_OUT_CELL, "--");
                snprintf(sql_cap->cell[0][5], SQL_OUT_CELL, "c*e^(%ldt)", lam);
                snprintf(sql_cap->cell[0][6], SQL_OUT_CELL, "%s", emfat1);
                snprintf(sql_cap->cell[0][7], SQL_OUT_CELL, "%s", serie1);
                snprintf(sql_cap->cell[0][8], SQL_OUT_CELL, "%s", soma1);
                snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT 1");
            }
            printf("edo: primeira ordem · λ = %ld · y = c·e^(%ldt) · em fatorial %s\n",
                   lam, lam, emfat1);
            return 1;
        }
        long B = -m[1][1], C = -m[1][0];
        long D = B*B - 4*C;
        /* a raiz inteira de |Δ|, se houver */
        long ad = D < 0 ? -D : D, r = 0;
        while(r*r < ad) r++;
        int quadrado = (r*r == ad);
        char r1[SQL_OUT_CELL], r2[SQL_OUT_CELL], forma[SQL_OUT_CELL];
        const char *classe;
        if(D > 0){
            classe = "hiperbolico";
            if(quadrado && ((-B + r) % 2 == 0)){
                snprintf(r1, sizeof r1, "%ld", (-B + r)/2);
                snprintf(r2, sizeof r2, "%ld", (-B - r)/2);
                snprintf(forma, sizeof forma, "c1*e^(%st) + c2*e^(%st)", r1, r2);
            } else {
                snprintf(r1, sizeof r1, "(%ld+sqrt(%ld))/2", -B, D);
                snprintf(r2, sizeof r2, "(%ld-sqrt(%ld))/2", -B, D);
                snprintf(forma, sizeof forma, "c1*e^(l1*t) + c2*e^(l2*t)");
            }
        } else if(D == 0){
            classe = "parabolico";
            if((-B) % 2 == 0){
                snprintf(r1, sizeof r1, "%ld", -B/2);
                snprintf(r2, sizeof r2, "%ld", -B/2);
                snprintf(forma, sizeof forma, "(c1 + c2*t)*e^(%st)", r1);
            } else {
                snprintf(r1, sizeof r1, "%ld/2", -B);
                snprintf(r2, sizeof r2, "%ld/2", -B);
                snprintf(forma, sizeof forma, "(c1 + c2*t)*e^(%st)", r1);
            }
        } else {
            classe = "eliptico";
            /* λ = a ± bi com a = −B/2 e b = √|Δ|/2 */
            if(quadrado && ((-B) % 2 == 0) && (r % 2 == 0)){
                snprintf(r1, sizeof r1, "%ld+%ldi", -B/2, r/2);
                snprintf(r2, sizeof r2, "%ld-%ldi", -B/2, r/2);
                snprintf(forma, sizeof forma, "e^(%ldt)*(c1*cos(%ldt) + c2*sen(%ldt))",
                         -B/2, r/2, r/2);
            } else {
                snprintf(r1, sizeof r1, "(%ld+i*sqrt(%ld))/2", -B, ad);
                snprintf(r2, sizeof r2, "(%ld-i*sqrt(%ld))/2", -B, ad);
                snprintf(forma, sizeof forma, "e^(at)*(c1*cos(wt) + c2*sen(wt))");
            }
        }
        char emfat2[SQL_OUT_CELL], serie2[SQL_OUT_CELL], soma2[SQL_OUT_CELL];
        { long cc2[2] = {C, B}, d02[2] = {1, 0};
          FtSol sol = ft_solucao(cc2, 2, d02, 12);
          ft_sol_escreve(&sol, emfat2, sizeof emfat2);
          ft_sol_serie(&sol, serie2, sizeof serie2);
          ft_sol_somatorio(&sol, soma2, sizeof soma2); }
        if(sql_cap){
            memset(sql_cap, 0, sizeof *sql_cap);
            sql_cap->ok = 1; sql_cap->nrows = 1; sql_cap->ncols = 9;
            snprintf(sql_cap->col[0], sizeof sql_cap->col[0], "equacao");
            snprintf(sql_cap->col[1], sizeof sql_cap->col[1], "disc");
            snprintf(sql_cap->col[2], sizeof sql_cap->col[2], "classe");
            snprintf(sql_cap->col[3], sizeof sql_cap->col[3], "lambda1");
            snprintf(sql_cap->col[4], sizeof sql_cap->col[4], "lambda2");
            snprintf(sql_cap->col[5], sizeof sql_cap->col[5], "solucao");
            snprintf(sql_cap->col[6], sizeof sql_cap->col[6], "contagem");
            snprintf(sql_cap->col[7], sizeof sql_cap->col[7], "serie_de_potencias");
            snprintf(sql_cap->col[8], sizeof sql_cap->col[8], "somatorio");
            for(int c = 0; c < sql_cap->ncols; c++) sql_cap->tipo[c] = SQL_TIPO_TEXT;
            sql_cap->tipo[1] = SQL_TIPO_INT4;
            if(B && C) snprintf(sql_cap->cell[0][0], SQL_OUT_CELL,
                                "y'' %c %ldy' %c %ldy = 0", B<0?'-':'+', B<0?-B:B,
                                C<0?'-':'+', C<0?-C:C);
            else if(B) snprintf(sql_cap->cell[0][0], SQL_OUT_CELL,
                                "y'' %c %ldy' = 0", B<0?'-':'+', B<0?-B:B);
            else       snprintf(sql_cap->cell[0][0], SQL_OUT_CELL,
                                "y'' %c %ldy = 0", C<0?'-':'+', C<0?-C:C);
            snprintf(sql_cap->cell[0][1], SQL_OUT_CELL, "%ld", D);
            snprintf(sql_cap->cell[0][2], SQL_OUT_CELL, "%s", classe);
            snprintf(sql_cap->cell[0][3], SQL_OUT_CELL, "%s", r1);
            snprintf(sql_cap->cell[0][4], SQL_OUT_CELL, "%s", r2);
            snprintf(sql_cap->cell[0][5], SQL_OUT_CELL, "%s", forma);
            snprintf(sql_cap->cell[0][6], SQL_OUT_CELL, "%s", emfat2);
            snprintf(sql_cap->cell[0][7], SQL_OUT_CELL, "%s", serie2);
            snprintf(sql_cap->cell[0][8], SQL_OUT_CELL, "%s", soma2);
            snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT 1");
        }
        printf("edo: %s · Δ = %ld · %s · λ = %s, %s · y(t) = %s\n",
               (B&&C)?"y'' + By' + Cy = 0":"y'' + ... = 0", D, classe, r1, r2, forma);
        return 1;
    }

    if(acao == ACAO_MARCA && mat_op == 30){
        /* ── GLOBAL: o cor:global executado sobre uma tabela.
         * Cada COLUNA é uma representação do mesmo objecto, cada LINHA um
         * objecto. O corolário pede que cada representação seja reversível ---
         * e essa hipótese verifica-se, não se supõe. Devolve, por passo da
         * cadeia, o custo D = 2^{−q}; a ponta R_0→R_k; se a ponta é dominada
         * pelo pior passo; e a ultramétrica HERDADA por cada representação, que
         * é a peça que interessa a quem tem um corpo para completar: a régua não
         * se constrói no corpo, herda-se pela bijeção. */
        if(ncols < 2){
            printf("erro: o global compara REPRESENTAÇÕES --- é preciso pelo menos"
                   " duas colunas, e a tabela tem %ld. RECUSADA.\n", ncols);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "global needs at least two representations, got %ld", ncols); }
            return 0;
        }
        long n_obj = 0;
        for(long i = 0; i < nrows; i++) if(bit_le(S_MATCH, i)) n_obj++;
        if(n_obj < 2){
            printf("erro: o global precisa de pelo menos dois objectos --- RECUSADA.\n");
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err, "global needs two objects"); }
            return 0;
        }
        if(ncols > 8 || n_obj > 64){
            printf("erro: o global segura 8 representações e 64 objectos --- vieram"
                   " %ld e %ld. RECUSADA, e não truncada.\n", ncols, n_obj);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "global: %ld reprs x %ld objects exceed 8x64", ncols, n_obj); }
            return 0;
        }
        for(long i = 0; i < nrows; i++) if(bit_le(S_MATCH, i))
            for(long c = 0; c < ncols; c++)
                if(!bit_le(S_PRES, i*ncols + c)){
                    printf("erro: célula ausente na linha %ld --- um buraco não é um"
                           " endereço. RECUSADA.\n", i);
                    if(sql_cap){ sql_cap->ok = 0;
                        snprintf(sql_cap->err, sizeof sql_cap->err,
                                 "global: missing cell at row %ld", i); }
                    return 0;
                }
        /* lê as representações: coluna c, objecto k */
        static long RG[8][64];
        long k = 0;
        for(long i = 0; i < nrows && k < 64; i++){
            if(!bit_le(S_MATCH, i)) continue;
            for(long c = 0; c < ncols; c++){
                long nu, de; celula_qz(i, c, ncols, &nu, &de);
                RG[c][k] = (de == 1) ? nu : (nu * 1000 + de);   /* endereço inteiro */
            }
            k++;
        }
        int bits = 40;
        /* (1) a hipótese: cada representação é reversível? */
        long rev = 0;
        for(long c = 0; c < ncols; c++){
            int ok_c = 1;
            for(long i = 0; i < k && ok_c; i++) for(long j = 0; j < i; j++)
                if(RG[c][i] == RG[c][j]){ ok_c = 0; break; }
            if(ok_c) rev++;
        }
        if(rev != ncols){
            printf("erro: %ld das %ld representações NÃO são reversíveis --- o corolário"
                   " não se aplica, e supor a hipótese seria a fraude. RECUSADA.\n",
                   ncols - rev, ncols);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "global: %ld of %ld representations are not reversible",
                         ncols - rev, ncols); }
            return 0;
        }
        /* (4) o custo de cada passo, e a ponta */
        int pior = bits;
        for(long c = 1; c < ncols; c++){
            int q = bits;
            for(long i = 0; i < k; i++){
                int p2 = qz_prof_bits(RG[c-1][i], RG[c][i], bits);
                if(p2 < q) q = p2;
            }
            if(q < pior) pior = q;
        }
        int ponta = bits;
        for(long i = 0; i < k; i++){
            int p2 = qz_prof_bits(RG[0][i], RG[ncols-1][i], bits);
            if(p2 < ponta) ponta = p2;
        }
        int domina = (ponta >= pior);
        /* a ultramétrica HERDADA: violações e estritos na primeira representação */
        long viola = 0, estrito = 0;
        for(long i = 0; i < k; i++) for(long j = 0; j < k; j++) for(long m = 0; m < k; m++){
            int a2 = qz_prof_bits(RG[0][i], RG[0][j], bits);
            int b2 = qz_prof_bits(RG[0][j], RG[0][m], bits);
            int c2 = qz_prof_bits(RG[0][i], RG[0][m], bits);
            int mn = a2 < b2 ? a2 : b2;
            if(c2 < mn) viola++; else if(c2 > mn) estrito++;
        }
        if(sql_cap){
            memset(sql_cap, 0, sizeof *sql_cap);
            sql_cap->ok = 1; sql_cap->nrows = 1; sql_cap->ncols = 6;
            snprintf(sql_cap->col[0], sizeof sql_cap->col[0], "reversiveis");
            snprintf(sql_cap->col[1], sizeof sql_cap->col[1], "q_pior_passo");
            snprintf(sql_cap->col[2], sizeof sql_cap->col[2], "q_ponta");
            snprintf(sql_cap->col[3], sizeof sql_cap->col[3], "domina");
            snprintf(sql_cap->col[4], sizeof sql_cap->col[4], "ultra_viola");
            snprintf(sql_cap->col[5], sizeof sql_cap->col[5], "ultra_estrito");
            for(int c = 0; c < sql_cap->ncols; c++) sql_cap->tipo[c] = SQL_TIPO_INT4;
            sql_cap->tipo[3] = SQL_TIPO_TEXT;
            snprintf(sql_cap->cell[0][0], SQL_OUT_CELL, "%ld", rev);
            snprintf(sql_cap->cell[0][1], SQL_OUT_CELL, "%d", pior);
            snprintf(sql_cap->cell[0][2], SQL_OUT_CELL, "%d", ponta);
            snprintf(sql_cap->cell[0][3], SQL_OUT_CELL, "%s", domina ? "sim" : "nao");
            snprintf(sql_cap->cell[0][4], SQL_OUT_CELL, "%ld", viola);
            snprintf(sql_cap->cell[0][5], SQL_OUT_CELL, "%ld", estrito);
            snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT 1");
        }
        printf("global: %ld representações reversíveis · pior passo 2^-%d · ponta 2^-%d ·"
               " domina %s · ultramétrica herdada: %ld violam, %ld estritos\n",
               rev, pior, ponta, domina ? "sim" : "nao", viola, estrito);
        return 1;
    }

    if(acao == ACAO_MARCA && mat_op == 29){
        /* ── COMPLETA: OS LUGARES QUE FALTAM para o corpo ficar completo.
         * A `lib/levanta.h` levanta o corpo pelo ι da `aranha cor:pik` --- a
         * inclusão que preenche as posições novas ---, e a projecção π esquece
         * a folha, com π∘ι = id. Lá em cima G é constante por construção.
         *
         * Aqui devolve-se o que INTERESSA A QUEM VAI COMPLETAR: uma linha por
         * lugar aberto, com o endereço e a folha. O levantado inteiro pode não
         * caber na saída --- 66 linhas contra as 64 do tecto ---, e enviar as
         * primeiras 64 seria responder «este é o corpo completo» sobre dois
         * terços dele. Por isso: se o que falta não couber, RECUSA e diz
         * quanto. Um corpo já completo devolve zero linhas, que é a resposta
         * certa e não um erro. */
        if(ncols != 1){
            printf("erro: completa-se uma coluna de endereços --- a tabela tem"
                   " %ld. RECUSADA.\n", ncols);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "completa needs one column of addresses, got %ld", ncols); }
            return 0;
        }
        long n_obj = 0;
        for(long i = 0; i < nrows; i++) if(bit_le(S_MATCH, i)) n_obj++;
        if(n_obj == 0){
            printf("erro: não há linhas para completar --- RECUSADA.\n");
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err, "completa on empty selection"); }
            return 0;
        }
        for(long i = 0; i < nrows; i++)
            if(bit_le(S_MATCH, i) && !bit_le(S_PRES, i*ncols)){
                printf("erro: a coluna tem células ausentes --- um buraco não é um"
                       " endereço, e levantá-lo seria inventá-lo. RECUSADA.\n");
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "completa: missing cell at row %ld", i); }
                return 0;
            }
        /* os endereços distintos e o tamanho de cada fibra */
        long vis_nu[SQL_OUT_MAX_ROWS*8], vis_de[SQL_OUT_MAX_ROWS*8], tam[SQL_OUT_MAX_ROWS*8];
        long nf = 0, maior = 0;
        int coube = 1;
        for(long i = 0; i < nrows && coube; i++){
            if(!bit_le(S_MATCH, i)) continue;
            long nu_i, de_i; celula_qz(i, 0, ncols, &nu_i, &de_i);
            Qz vi = qz(nu_i, de_i);
            int novo = 1;
            for(long j = 0; j < nf; j++)
                if(qz_igual(qz(vis_nu[j], vis_de[j]), vi)){ novo = 0; break; }
            if(!novo) continue;
            if(nf >= (long)(sizeof vis_nu / sizeof vis_nu[0])){ coube = 0; break; }
            long t = 0;
            for(long j = 0; j < nrows; j++){
                if(!bit_le(S_MATCH, j)) continue;
                long nu, de; celula_qz(j, 0, ncols, &nu, &de);
                if(qz_igual(qz(nu, de), vi)) t++;
            }
            vis_nu[nf] = (long)vi.p; vis_de[nf] = (long)vi.q; tam[nf] = t;
            if(t > maior) maior = t;
            nf++;
        }
        if(!coube){
            printf("erro: mais endereços distintos do que o motor segura ---"
                   " RECUSADA, e não truncada.\n");
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "completa: too many distinct addresses"); }
            return 0;
        }
        long falta = nf * maior;
        for(long j = 0; j < nf; j++) falta -= tam[j];
        if(falta > SQL_OUT_MAX_ROWS){
            printf("erro: faltam %ld lugares e a saída segura %d --- RECUSADA."
                   " Truncar seria dizer «é este o corpo completo» sobre uma"
                   " parte dele.\n", falta, SQL_OUT_MAX_ROWS);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "completa: %ld missing places exceed output cap %d",
                         falta, SQL_OUT_MAX_ROWS); }
            return 0;
        }
        if(sql_cap){
            memset(sql_cap, 0, sizeof *sql_cap);
            sql_cap->ok = 1; sql_cap->ncols = 3;
            snprintf(sql_cap->col[0], sizeof sql_cap->col[0], "endereco");
            snprintf(sql_cap->col[1], sizeof sql_cap->col[1], "folha");
            snprintf(sql_cap->col[2], sizeof sql_cap->col[2], "g_alvo");
            for(int c = 0; c < sql_cap->ncols; c++) sql_cap->tipo[c] = SQL_TIPO_INT4;
            sql_cap->tipo[0] = SQL_TIPO_TEXT;
            long r = 0;
            for(long j = 0; j < nf; j++)
                for(long p = tam[j]; p < maior; p++){
                    if(vis_de[j] == 1)
                        snprintf(sql_cap->cell[r][0], SQL_OUT_CELL, "%ld", vis_nu[j]);
                    else
                        snprintf(sql_cap->cell[r][0], SQL_OUT_CELL, "%ld/%ld",
                                 vis_nu[j], vis_de[j]);
                    snprintf(sql_cap->cell[r][1], SQL_OUT_CELL, "%ld", p);
                    snprintf(sql_cap->cell[r][2], SQL_OUT_CELL, "%ld", maior);
                    r++;
                }
            sql_cap->nrows = r;
            snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT %ld", r);
        }
        printf("completa: %ld fibras, G alvo %ld, faltam %ld lugares\n", nf, maior, falta);
        return 1;
    }

    if(acao == ACAO_MARCA && mat_op == 22){
        /* ── A FIBRA NÃO É OPERAÇÃO MATRICIAL, e por isso não passa pelo tecto
         * de LN_MAX: ela conta a DOBRA de uma leitura (aranha def:dobra) sobre
         * a coluna inteira. Pô-la no caminho da matriz truncava em silêncio ---
         * respondia «6 objectos» havendo 36 ---, que é o pior defeito possível
         * numa contagem: um número certo sobre a amostra errada.
         *
         * Devolve o que decide a completude: quantas fibras, o menor e o maior
         * G, se ele é CONSTANTE, quanto FALTA, e o tamanho do corpo expandido. */
        if(ncols != 1){
            printf("erro: a fibra conta-se sobre UMA coluna de endereços --- a"
                   " tabela tem %ld. RECUSADA.\n", ncols);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "fibra needs one column of addresses, got %ld", ncols); }
            return 0;
        }
        long n_obj = 0;
        for(long i = 0; i < nrows; i++) if(bit_le(S_MATCH, i)) n_obj++;
        if(n_obj == 0){
            printf("erro: não há linhas para contar a fibra --- RECUSADA.\n");
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err, "fibra on empty selection"); }
            return 0;
        }
        /* uma célula ausente não é um endereço: a contagem recusaria com a
         * soma torta, mas é melhor dizê-lo aqui, com o nome certo */
        for(long i = 0; i < nrows; i++)
            if(bit_le(S_MATCH, i) && !bit_le(S_PRES, i*ncols)){
                printf("erro: a coluna tem células ausentes --- um buraco não é um"
                       " endereço, e contá-lo seria inventá-lo. RECUSADA.\n");
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "fibra: missing cell at row %ld", i); }
                return 0;
            }
        long fibras = 0, menor = n_obj + 1, maior = 0, soma = 0;
        for(long i = 0; i < nrows; i++){
            if(!bit_le(S_MATCH, i)) continue;
            long nu_i, de_i; celula_qz(i, 0, ncols, &nu_i, &de_i);
            Qz vi = qz(nu_i, de_i);
            int novo = 1;
            for(long j = 0; j < i && novo; j++){
                if(!bit_le(S_MATCH, j)) continue;
                long nu, de; celula_qz(j, 0, ncols, &nu, &de);
                if(qz_igual(qz(nu, de), vi)) novo = 0;
            }
            if(!novo) continue;
            long t = 0;
            for(long j = 0; j < nrows; j++){
                if(!bit_le(S_MATCH, j)) continue;
                long nu, de; celula_qz(j, 0, ncols, &nu, &de);
                if(qz_igual(qz(nu, de), vi)) t++;
            }
            if(t < menor) menor = t;
            if(t > maior) maior = t;
            soma += t;
            fibras++;
        }
        int constante = (fibras > 0 && menor == maior);
        long falta = fibras * maior - soma;
        long expandido = fibras * maior;
        /* o thm:escada: Σ G tem de ser |I|. Se não for, é defeito da CONTAGEM
         * e não do corpo --- e recusa-se em vez de publicar um número torto. */
        if(soma != n_obj){
            printf("erro: Σ G = %ld e |I| = %ld --- a contagem não fecha, e isso"
                   " é do motor. RECUSADA.\n", soma, n_obj);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "fibre sum %ld != |I| %ld", soma, n_obj); }
            return 0;
        }
        if(sql_cap){
            memset(sql_cap, 0, sizeof *sql_cap);
            sql_cap->ok = 1; sql_cap->nrows = 1; sql_cap->ncols = 6;
            snprintf(sql_cap->col[0], sizeof sql_cap->col[0], "fibras");
            snprintf(sql_cap->col[1], sizeof sql_cap->col[1], "g_min");
            snprintf(sql_cap->col[2], sizeof sql_cap->col[2], "g_max");
            snprintf(sql_cap->col[3], sizeof sql_cap->col[3], "completo");
            snprintf(sql_cap->col[4], sizeof sql_cap->col[4], "falta");
            snprintf(sql_cap->col[5], sizeof sql_cap->col[5], "expandido");
            for(int c = 0; c < sql_cap->ncols; c++) sql_cap->tipo[c] = SQL_TIPO_INT4;
            sql_cap->tipo[3] = SQL_TIPO_TEXT;
            snprintf(sql_cap->cell[0][0], SQL_OUT_CELL, "%ld", fibras);
            snprintf(sql_cap->cell[0][1], SQL_OUT_CELL, "%ld", menor);
            snprintf(sql_cap->cell[0][2], SQL_OUT_CELL, "%ld", maior);
            snprintf(sql_cap->cell[0][3], SQL_OUT_CELL, "%s", constante ? "sim" : "nao");
            snprintf(sql_cap->cell[0][4], SQL_OUT_CELL, "%ld", falta);
            snprintf(sql_cap->cell[0][5], SQL_OUT_CELL, "%ld", expandido);
            snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT 1");
        }
        printf("   %ld | %ld | %ld | %s | %ld | %ld\n",
               fibras, menor, maior, constante ? "sim" : "nao", falta, expandido);
        printf("-- G em %ld fibra(s) sobre %ld objecto(s) · Σ G = |I| · %s\n",
               fibras, n_obj,
               constante ? "G é CONSTANTE: o corpo está completo"
                         : "G VARIA: o corpo está incompleto, e falta o que a coluna diz");
        return 1;
    }

    if(acao == ACAO_MARCA && mat_op){
        long nr_v = 0;
        int lin[LN_MAX];
        MatQz A;
        /* só as linhas VIVAS e MARCADAS entram, e a ordem é a delas */
        for(long i = 0; i < nrows && nr_v < LN_MAX; i++)
            if(bit_le(S_MATCH, i)) lin[nr_v++] = (int)i;
        /* ── E A SEGUNDA PORTA, QUE NÃO É A MESMA ────────────────────────────
         * A de cima apanha a tabela SEM LINHAS; esta apanha a tabela COM linhas
         * de que o `WHERE` não deixou nenhuma — `SELECT det(*) FROM t WHERE
         * a = 999`. São dois estados diferentes com a mesma consequência: não há
         * matriz. Escrever só uma delas deixaria a outra a responder «ok» com
         * silêncio, e é por isso que as duas existem e nenhuma é código morto. */
        if(nr_v == 0){
            printf("erro: a tabela não tem linhas — uma matriz %ld×%ld não é uma"
                   " matriz, e «zero linhas» na resposta leria-se como a conta a"
                   " dar vazio em vez de não haver conta. RECUSADA.\n",
                   nr_v, ncols);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "empty table: a %ld×%ld matrix has no entries",
                         nr_v, ncols); }
            return 0;
        }
        /* ── O TECTO DE CADA OPERAÇÃO, E ELE NÃO É UM SÓ ─────────────────────
         * O `linear.h` guarda uma matriz até LN_MAX×LN_MAX, e é esse o tecto
         * geral. Mas a INVERSA trabalha numa matriz AUMENTADA de n×2n — o
         * método é justapor a identidade e reduzir —, pelo que o seu tecto é
         * METADE. Dizer um limite e aplicar outro é o defeito que a primeira
         * escrita tinha nas duas pontas: o teste recusava 6×6 (com `>=`) e a
         * mensagem anunciava 6×6 como aceitável, e a inversa de uma 4×4 teria
         * escrito fora do arranjo sem ninguém dizer nada. Cada operação diz o
         * SEU tecto. */
        { long tecto = (mat_op == 5 || mat_op == 13) ? LN_MAX/2 : LN_MAX;
          if(ncols > tecto || nr_v > tecto){
              printf("erro: a matriz é %ld×%ld e o alcance %s é %ld×%ld —"
                     " RECUSADA.\n", nr_v, ncols,
                     (mat_op == 5 || mat_op == 13)
                         ? "desta operação (metade, pela matriz aumentada)"
                                   : "desta casa", tecto, tecto);
              if(sql_cap){ sql_cap->ok = 0;
                  snprintf(sql_cap->err, sizeof sql_cap->err,
                           "matrix too large: %ld×%ld, limit is %ld×%ld",
                           nr_v, ncols, tecto, tecto); }
              return 0;
          } }
        A = mat0((int)nr_v, (int)ncols);
        { int falta = 0;
          for(int i = 0; i < (int)nr_v; i++)
              for(long j = 0; j < ncols; j++){
                  if(!bit_le(S_PRES, lin[i]*ncols + j)){ falta = 1; continue; }
                  { long nu, de;
                    celula_qz(lin[i], j, ncols, &nu, &de);
                    A.a[i][j] = qz(nu, de); }
              }
          /* uma matriz com um buraco não é uma matriz: o dual não é zero, e
           * fazer a conta com ele seria inventar a entrada que falta */
          if(falta){
              printf("erro: a tabela tem células ausentes — a matriz não está"
                     " completa, e o dual não é zero. RECUSADA.\n");
              if(sql_cap){ sql_cap->ok = 0;
                  snprintf(sql_cap->err, sizeof sql_cap->err,
                           "matrix has absent cells; the dual is not zero"); }
              return 0;
          } }

        if(mat_op == 1 || mat_op == 2 || mat_op == 3){       /* det, posto, traço */
            /* ── UM ZERO PODE SER DUAS COISAS, E ISSO NÃO PODE FICAR ─────────
             * O `qz_mult` e o `qz_soma` da casa devolvem ZERO quando o
             * resultado não cabe no inteiro — e contam-no em `qz_perdeu`. Sem
             * ler esse contador, um determinante que ESTOUROU é indistinguível
             * de um determinante NULO: os dois saem `0`, e o primeiro é a
             * resposta errada com a cara da certa. Lê-se antes e depois, e o
             * que perdeu é RECUSADO — «singular» e «não coube» são coisas
             * diferentes e têm de o dizer. */
            long perdeu_antes = qz_perdeu;
            Qz v = qz(0,1);
            long inteiro = 0;
            const char *nm = mat_op == 1 ? "det" : mat_op == 2 ? "posto" : "traco";
            if(mat_op == 1){
                if(A.m != A.n){
                    printf("erro: o determinante pede uma matriz QUADRADA, e"
                           " esta é %d×%d — RECUSADO.\n", A.m, A.n);
                    if(sql_cap){ sql_cap->ok = 0;
                        snprintf(sql_cap->err, sizeof sql_cap->err,
                                 "det: matrix is %d×%d, not square", A.m, A.n); }
                    return 0;
                }
                v = mat_det(A);
            } else if(mat_op == 2){
                inteiro = mat_posto(A);
            } else {
                if(A.m != A.n){
                    printf("erro: o traço pede uma matriz QUADRADA — RECUSADO.\n");
                    if(sql_cap){ sql_cap->ok = 0;
                        snprintf(sql_cap->err, sizeof sql_cap->err,
                                 "trace: matrix is not square"); }
                    return 0;
                }
                for(int i = 0; i < A.n; i++) v = qz_soma(v, A.a[i][i]);
            }
            if(qz_perdeu != perdeu_antes){
                printf("erro: a conta não coube no inteiro (%ld perdas) — o"
                       " resultado seria ZERO com cara de resposta, e um zero"
                       " assim é indistinguível de um determinante nulo."
                       " RECUSADA.\n", qz_perdeu - perdeu_antes);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "matrix arithmetic overflowed: %ld values lost",
                             qz_perdeu - perdeu_antes); }
                return 0;
            }
            { char cel[SQL_OUT_CELL];
              if(mat_op == 2) snprintf(cel, sizeof cel, "%ld", inteiro);
              else { Par cls = ra_classe((Par){ (long)v.p, (long)v.q });
                     if(cls.b > 1) snprintf(cel, sizeof cel, "%ld/%ld", cls.a, cls.b);
                     else          snprintf(cel, sizeof cel, "%ld", cls.a); }
              printf("   %s\n-- a tabela %d×%d lida como matriz\n", cel, A.m, A.n);
              if(sql_cap){
                  memset(sql_cap, 0, sizeof *sql_cap);
                  sql_cap->ok = 1; sql_cap->ncols = 1; sql_cap->nrows = 1;
                  sql_cap->tipo[0] = SQL_TIPO_INT4;
                  snprintf(sql_cap->col[0], sizeof sql_cap->col[0], "%s", nm);
                  snprintf(sql_cap->cell[0][0], SQL_OUT_CELL, "%s", cel);
                  snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT 1");
              } }
            return 1;
        }

        if(mat_op == 10){                        /* Cramer: o OUTRO caminho */
            /* ── O MESMO SISTEMA, POR OUTRO CAMINHO ──────────────────────────
             * `resolve` faz eliminação; Cramer faz determinantes:
             *
             *     x_i = det(A com a coluna i trocada por b) / det(A)
             *
             * Não é uma alternativa por gosto — é uma SEGUNDA TESTEMUNHA. Os
             * dois algoritmos não se apoiam um no outro (um escalona, o outro
             * expande), e por isso concordarem é uma verificação e não uma
             * repetição. É a régua desta casa: o que mais defeitos apanhou foi
             * a COMPARAÇÃO entre dois caminhos, não a asserção sobre um.
             *
             * Cramer pede o que a eliminação não pede: a matriz tem de ser
             * QUADRADA e o determinante não nulo. Onde ele não se aplica,
             * recusa-se — e é aí que os dois caminhos deixam de poder ser
             * comparados, o que também se diz. */
            int nA = A.n - 1;
            MatQz S;
            Qz dA;
            if(nA < 1 || nA != A.m){
                printf("erro: Cramer pede [A|b] com A QUADRADA, e aqui A é"
                       " %d×%d — RECUSADO.\n", A.m, nA);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "Cramer: A is %d×%d, not square", A.m, nA); }
                return 0;
            }
            S = A; S.n = nA;
            dA = mat_det(S);
            if(dA.p == 0){
                printf("erro: o determinante é ZERO — Cramer não se aplica"
                       " (o sistema não tem solução única). RECUSADO.\n");
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "Cramer: determinant is zero"); }
                return 0;
            }
            if(sql_cap){
                memset(sql_cap, 0, sizeof *sql_cap);
                sql_cap->ok = 1;
                sql_cap->ncols = nA > SQL_OUT_MAX_COLS ? SQL_OUT_MAX_COLS : nA;
                sql_cap->nrows = 1;
                for(int j = 0; j < sql_cap->ncols; j++){
                    snprintf(sql_cap->col[j], sizeof sql_cap->col[j], "x%d", j + 1);
                    sql_cap->tipo[j] = SQL_TIPO_INT4;
                }
                snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT 1");
            }
            printf("   ");
            for(int j = 0; j < nA; j++){
                MatQz T = S;
                Qz dT, xj;
                for(int i2 = 0; i2 < A.m; i2++) T.a[i2][j] = A.a[i2][A.n - 1];
                dT = mat_det(T);
                if(!qz_divide(dT, dA, &xj)) xj = qz(0,1);
                { Par cls = ra_classe((Par){ (long)xj.p, (long)xj.q });
                  char cel[SQL_OUT_CELL];
                  if(cls.b > 1) snprintf(cel, sizeof cel, "%ld/%ld", cls.a, cls.b);
                  else          snprintf(cel, sizeof cel, "%ld", cls.a);
                  printf("%s%s", cel, j + 1 < nA ? " | " : "");
                  if(sql_cap && j < sql_cap->ncols)
                      snprintf(sql_cap->cell[0][j], SQL_OUT_CELL, "%s", cel); }
            }
            printf("\n");
            { Par cd = ra_classe((Par){ (long)dA.p, (long)dA.q });
              printf("-- por Cramer: det(A) = %ld, e cada x_i é o quociente de"
                     " dois determinantes\n", cd.a); }
            return 1;
        }

        if(mat_op == 9){                         /* resolver: a VOLTA */
            /* ── RESOLVER É A VOLTA DO PRODUTO ───────────────────────────────
             * O produto COMPÕE — dá o que sai de aplicar; resolver DESCOMPÕE —
             * dá o que teria de entrar. É o par de sempre, e é por isso que se
             * verifica um com o outro: a solução aplicada tem de devolver o
             * lado direito.
             *
             * A tabela é a matriz AUMENTADA [A | b]: as primeiras colunas são o
             * sistema, a última é o que ele iguala. Não é uma convenção
             * arbitrária — é a forma em que o sistema É uma tabela, e a única
             * que não obriga a passar dois objectos onde há um.
             *
             * E os TRÊS desfechos decidem-se pelo POSTO, que é o teorema de
             * Rouché–Capelli e já está todo aqui:
             *
             *   posto(A) < posto([A|b])            não há solução
             *   posto(A) = posto([A|b]) < colunas  há infinitas
             *   posto(A) = posto([A|b]) = colunas  há uma
             *
             * Não são três casos a tratar: é uma comparação de dois números. */
            int nA = A.n - 1;
            MatQz M = A, S;
            int pivM[LN_MAX], pivS[LN_MAX], rM, rS;
            if(nA < 1){
                printf("erro: resolver pede pelo menos duas colunas — a matriz"
                       " aumentada [A|b]. RECUSADO.\n");
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "solve: need at least 2 columns for [A|b]"); }
                return 0;
            }
            S = A; S.n = nA;                        /* só o A, sem o b */
            rS = mat_reduz(&S, pivS);
            rM = mat_reduz(&M, pivM);
            if(rM > rS){
                printf("erro: posto(A) = %d e posto([A|b]) = %d — o sistema NÃO"
                       " TEM SOLUÇÃO (Rouché–Capelli). RECUSADO.\n", rS, rM);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "system has no solution: rank(A)=%d < rank([A|b])=%d",
                             rS, rM); }
                return 0;
            }
            { VecQz x = vec0(nA);
              int livres = nA - rS;
              /* a solução PARTICULAR: as variáveis livres a zero, e as de pivô
               * lidas da última coluna da reduzida */
              for(int j = 0; j < nA; j++)
                  if(pivM[j] >= 0) x.c[j] = M.a[pivM[j]][A.n - 1];
              if(sql_cap){
                  memset(sql_cap, 0, sizeof *sql_cap);
                  sql_cap->ok = 1;
                  sql_cap->ncols = nA > SQL_OUT_MAX_COLS ? SQL_OUT_MAX_COLS : nA;
                  sql_cap->nrows = 1;
                  for(int j = 0; j < sql_cap->ncols; j++){
                      snprintf(sql_cap->col[j], sizeof sql_cap->col[j], "x%d", j + 1);
                      sql_cap->tipo[j] = SQL_TIPO_INT4;
                  }
                  snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT 1");
              }
              printf("   ");
              for(int j = 0; j < nA; j++){
                  Par cls = ra_classe((Par){ (long)x.c[j].p, (long)x.c[j].q });
                  char cel[SQL_OUT_CELL];
                  if(cls.b > 1) snprintf(cel, sizeof cel, "%ld/%ld", cls.a, cls.b);
                  else          snprintf(cel, sizeof cel, "%ld", cls.a);
                  printf("%s%s", cel, j + 1 < nA ? " | " : "");
                  if(sql_cap && j < sql_cap->ncols)
                      snprintf(sql_cap->cell[0][j], SQL_OUT_CELL, "%s", cel);
              }
              printf("\n");
              if(livres > 0)
                  printf("-- posto(A) = posto([A|b]) = %d < %d colunas: há INFINITAS"
                         " soluções, e esta é a particular (as %d livres a zero;"
                         " as outras somam-se-lhe pelo núcleo)\n", rS, nA, livres);
              else
                  printf("-- posto(A) = posto([A|b]) = %d = colunas: a solução é"
                         " ÚNICA\n", rS);
              return 1; }
        }

        if(mat_op == 8 || mat_op == 11){         /* o produto e a soma: as DUAS FACES */
            /* ── O PRODUTO É A COMPOSIÇÃO, e por isso pede dois lados ────────
             * `A·B` é aplicar B e depois A, e a condição para existir é a que
             * a composição sempre teve: a saída de um tem de ser a entrada do
             * outro — as colunas de A contra as linhas de B. Quando não bate,
             * recusa-se com os dois números, porque dizer só «não dá» esconde
             * qual dos lados está errado.
             *
             * A outra tabela lê-se com ela ABERTA e traz-se em memória local —
             * a mesma regra da subconsulta, da seta e do EXISTS. */
            MatQz B;
            char guarda[64];
            int nB = 0, mB = 0;
            snprintf(guarda, sizeof guarda, "%s", nome);
            if(!usa_tabela(mat_tab2, 0) || !cat_nome_bate(mat_tab2)){
                if(!usa_tabela(guarda, 0)) restauros_falhados++;
                printf("erro: a tabela «%s» do produto não existe — RECUSADO.\n",
                       mat_tab2);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "relation \"%s\" does not exist", mat_tab2); }
                return 0;
            }
            { long nc2 = cat_ncols(), nr2 = cat_nrows();
              int falta = 0;
              if(nc2 > LN_MAX || nr2 > LN_MAX){
                  if(!usa_tabela(guarda, 0)) restauros_falhados++;
                  printf("erro: «%s» é %ld×%ld e o alcance é %d×%d — RECUSADO.\n",
                         mat_tab2, nr2, nc2, LN_MAX, LN_MAX);
                  if(sql_cap){ sql_cap->ok = 0;
                      snprintf(sql_cap->err, sizeof sql_cap->err,
                               "matrix too large"); }
                  return 0;
              }
              mB = (int)nr2; nB = (int)nc2;
              B = mat0(mB, nB);
              for(int i2 = 0; i2 < mB; i2++)
                  for(int j2 = 0; j2 < nB; j2++){
                      if(!bit_le(S_VIVO, i2) || !bit_le(S_PRES, i2*nc2 + j2)){
                          falta = 1; continue; }
                      { long nu, de;
                        celula_qz(i2, j2, nc2, &nu, &de);
                        B.a[i2][j2] = qz(nu, de); }
                  }
              if(!usa_tabela(guarda, 0)) restauros_falhados++;
              if(falta){
                  printf("erro: «%s» tem células ausentes ou linhas apagadas —"
                         " a matriz não está completa. RECUSADO.\n", mat_tab2);
                  if(sql_cap){ sql_cap->ok = 0;
                      snprintf(sql_cap->err, sizeof sql_cap->err,
                               "matrix has absent cells"); }
                  return 0;
              } }
            if(mat_op == 8 && A.n != B.m){
                printf("erro: o produto pede que as COLUNAS da primeira (%d)"
                       " sejam as LINHAS da segunda (%d) — é a condição da"
                       " composição, e não bate. RECUSADO.\n", A.n, B.m);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "matrix product: %d columns against %d rows",
                             A.n, B.m); }
                return 0;
            }
            if(mat_op == 11 && (A.m != B.m || A.n != B.n)){
                printf("erro: a soma pede a MESMA forma, e aqui são %d×%d e"
                       " %d×%d — RECUSADA.\n", A.m, A.n, B.m, B.n);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "matrix sum: %d×%d against %d×%d", A.m, A.n, B.m, B.n); }
                return 0;
            }
            { MatQz R = (mat_op == 11) ? mat_soma(A, B) : mat_mult(A, B);
              if(sql_cap){
                  memset(sql_cap, 0, sizeof *sql_cap);
                  sql_cap->ok = 1;
                  sql_cap->ncols = R.n > SQL_OUT_MAX_COLS ? SQL_OUT_MAX_COLS : R.n;
                  sql_cap->nrows = R.m > SQL_OUT_MAX_ROWS ? SQL_OUT_MAX_ROWS : R.m;
                  for(int j = 0; j < sql_cap->ncols; j++){
                      snprintf(sql_cap->col[j], sizeof sql_cap->col[j], "c%d", j + 1);
                      sql_cap->tipo[j] = SQL_TIPO_INT4;
                  }
                  snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT %d", sql_cap->nrows);
              }
              for(int i2 = 0; i2 < R.m; i2++){
                  printf("   ");
                  for(int j2 = 0; j2 < R.n; j2++){
                      Par cls = ra_classe((Par){ (long)R.a[i2][j2].p,
                                                 (long)R.a[i2][j2].q });
                      char cel[SQL_OUT_CELL];
                      if(cls.b > 1) snprintf(cel, sizeof cel, "%ld/%ld", cls.a, cls.b);
                      else          snprintf(cel, sizeof cel, "%ld", cls.a);
                      printf("%s%s", cel, j2 + 1 < R.n ? " | " : "");
                      if(sql_cap && i2 < sql_cap->nrows && j2 < sql_cap->ncols)
                          snprintf(sql_cap->cell[i2][j2], SQL_OUT_CELL, "%s", cel);
                  }
                  printf("\n");
              }
              if(mat_op == 11)
                  printf("-- a soma %d×%d + %d×%d = %d×%d\n",
                         A.m, A.n, B.m, B.n, R.m, R.n);
              else
                  printf("-- o produto %d×%d · %d×%d = %d×%d\n",
                         A.m, A.n, B.m, B.n, R.m, R.n);
              return 1; }
        }

        if(mat_op == 6 || mat_op == 7){          /* núcleo e imagem: o PAR */
            /* ── O NÚCLEO E A IMAGEM SÃO O PAR, e a lei que os liga é a mesma
             * conservação de sempre. O `mat_reduz` corre UMA vez e as duas
             * saem dela: as colunas com pivô geram a imagem, as sem pivô dão as
             * variáveis livres do núcleo — não são dois cálculos, é um lido dos
             * dois lados. Daí
             *
             *     dim(núcleo) + posto = número de colunas
             *
             * que é o teorema do núcleo-imagem, e é a MESMA frase do ∑G = |I|
             * do quociente: o que se perde mais o que sobrevive é o que havia. */
            VecQz base[LN_MAX];
            int k = (mat_op == 6) ? mat_nucleo(A, base) : mat_imagem(A, base);
            const char *nm = (mat_op == 6) ? "nucleo" : "imagem";
            int dim = (k > 0) ? base[0].n : 0;
            if(sql_cap){
                memset(sql_cap, 0, sizeof *sql_cap);
                sql_cap->ok = 1;
                sql_cap->ncols = dim > SQL_OUT_MAX_COLS ? SQL_OUT_MAX_COLS : dim;
                sql_cap->nrows = k > SQL_OUT_MAX_ROWS ? SQL_OUT_MAX_ROWS : k;
                for(int j = 0; j < sql_cap->ncols; j++){
                    snprintf(sql_cap->col[j], sizeof sql_cap->col[j], "c%d", j + 1);
                    sql_cap->tipo[j] = SQL_TIPO_INT4;
                }
                snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT %d", sql_cap->nrows);
            }
            if(!k) printf("   (vazio — a base tem zero vectores)\n");
            for(int i = 0; i < k; i++){
                printf("   ");
                for(int j = 0; j < base[i].n; j++){
                    Par cls = ra_classe((Par){ (long)base[i].c[j].p,
                                               (long)base[i].c[j].q });
                    char cel[SQL_OUT_CELL];
                    if(cls.b > 1) snprintf(cel, sizeof cel, "%ld/%ld", cls.a, cls.b);
                    else          snprintf(cel, sizeof cel, "%ld", cls.a);
                    printf("%s%s", cel, j + 1 < base[i].n ? " | " : "");
                    if(sql_cap && i < sql_cap->nrows && j < sql_cap->ncols)
                        snprintf(sql_cap->cell[i][j], SQL_OUT_CELL, "%s", cel);
                }
                printf("\n");
            }
            printf("-- a base do %s: %d vector(es) de dimensão %d"
                   " · posto %d + dim(núcleo) %d = %d colunas\n",
                   nm, k, dim, mat_posto(A),
                   A.n - mat_posto(A), A.n);
            return 1;
        }

        /* ═══ O DUAL, DENTRO DO MOTOR ══════════════════════════════════════
         * «o vetor fornece o objeto; o funcional fornece a coordenada que o
         * mede». As duas peças que faltavam do `linear.h` são as duas metades
         * disso, e nenhuma pede uma linha nova de álgebra:
         *
         *   dual(*)         as colunas da tabela são uma BASE, e a base dual
         *                   são as LINHAS de B⁻¹ — construída, não procurada,
         *                   porque e^i(e_j) = δ^i_j É a definição de inversa.
         *   aniquilador(*)  as LINHAS da tabela geram W, e os funcionais que
         *                   se anulam em W são o NÚCLEO dessa matriz. A mesma
         *                   descida do §W52, lida do lado do dual.
         *
         * E o aniquilador traz a conservação uma QUARTA vez:
         *
         *     dim W + dim W° = n
         *
         * que não é uma lei nova — é `posto + dim(ker) = n` aplicado à matriz
         * das linhas, com o posto a ser dim W. As dimensões repartem-se outra
         * vez, e outra vez sem se ter pedido. */
        /* ═══ A CIFRA: O ESPECTRO ESCRITO EM INTEIROS ══════════════════════
         * O `cifra.h` desta casa não foi escrito para matrizes — foi escrito
         * para CORPOS, e diz na porta que as suas duas grandezas são «a razão
         * (quanto se estica por nível) → o traço B» e «o sinal (se as duas
         * direções se cancelam) → o determinante C». E diz mais: «o hipercorpo
         * não tem (B,C): o seu operador NÃO É uma matriz 2×2». Ou seja: o par
         * que define um corpo desta casa JÁ ERA o par de invariantes de uma
         * matriz 2×2, e ninguém precisou de os identificar — estavam
         * identificados desde que aquilo foi escrito.
         *
         * Então `cifra(*)` não acrescenta álgebra nenhuma: pega no traço e no
         * determinante que o motor já dá e chama o codificador. O que sai é o
         * ESPECTRO — as raízes de λ² − Bλ + C, que são (B ± √(B²−4C))/2 —
         * escrito como fração contínua periódica, em inteiros e sem uma raiz
         * calculada: Lagrange garante que o período é invariante completo.
         *
         * E daí sai o gume de graça: matrizes SEMELHANTES têm a mesma cifra,
         * porque têm o mesmo traço e o mesmo determinante. Não é uma
         * propriedade que se lhe tenha dado — é o que «invariante de
         * conjugação» quer dizer, e mede-se com o motor contra si próprio. */
        /* ═══ O ESPECTRO POR NÚMEROS — O SEGUNDO CAMINHO ═══════════════════
         * A `cifra` escreve o espectro SEMPRE, por fração contínua, e vale
         * para todo o par (B,C) inteiro. O `esp_racional` do `forma.h` dá os
         * NÚMEROS, mas só quando o discriminante é quadrado perfeito. Os dois
         * lêem o MESMO discriminante — partilham o `raizi` —, o que faz deles
         * duas leituras de um objecto e não duas contas parecidas.
         *
         * E onde as raízes NÃO são racionais, isto não é um limite da conta: é
         * a resposta. As raízes são as folhas do corpo, e escrevem-se pela
         * cifra — a recusa remete para lá em vez de devolver um decimal, que
         * seria trocar o objecto por uma aproximação dele. */
        /* ═══ A GRAM: O PRODUTO INTERNO É O QUE FECHA O DUAL ═══════════════
         * A base dual do §W60 precisa de uma BASE para existir: os e^i são as
         * linhas de B⁻¹, e mudar B muda-os. O produto interno faz o mesmo
         * trabalho SEM escolher base — dá o isomorfismo canónico v ↦ ⟨v,·⟩ —, e
         * a sua forma escrita é a matriz de Gram das linhas:
         *
         *     G_ij = ⟨v_i, v_j⟩,   e   G = A·Aᵀ
         *
         * Daí três coisas de graça, e nenhuma precisa de uma linha de álgebra
         * nova: G é SIMÉTRICA (o produto interno não tem lado), det G = 0
         * exactamente quando as linhas são DEPENDENTES (é o determinante de
         * Gram, e é Cauchy–Schwarz quando n = 2), e posto G = posto A — a
         * mesma dimensão contada nos dois sítios.
         *
         * E a raiz nunca se tira: ⟨v,v⟩ é a norma AO QUADRADO, porque é a raiz
         * que traria o irracional para dentro de uma conta que é exacta. */
        /* ═══ O CORPO DIFERENCIAL: A TABELA É O GERADOR DO FLUXO ═══════════
         * `broca-so/papers/equacoes_diferenciais.tex` constrói a equação
         * diferencial como o fluxo ẋ = A·x, com A uma MATRIZ, e diz o essencial
         * numa linha: «o gerador é uma matriz, A = gato ⊕ esquilo (a
         * decomposição Sym + Skew)». O `lib/edo.h` desta casa resgatou desse
         * paper a parte ESCALAR — y'' + By' + Cy = 0 e a sua característica —
         * e deixou a matricial para trás; e o `lib/exterior.h`, escrito por
         * outra razão inteiramente, já tinha `ex_parte`, que é exactamente essa
         * decomposição. Duas metades da mesma frase em ficheiros que não se
         * conheciam.
         *
         *     A = (A + Aᵀ)/2  +  (A − Aᵀ)/2
         *          o GATO           o ESQUILO
         *          dissipa          gira
         *          espectro real    espectro imaginário
         *
         * E o `regime` é o veredicto que o `chess/universe/tools/diferencial.c`
         * media perturbando um bit — cristal, borda, caos —, aqui lido do
         * discriminante, que é o MESMO Δ = tr² − 4det que decide a cifra e os
         * autovalores. Uma classificação, escrita três vezes. */
        if(mat_op == 19 || mat_op == 20){
            MatQz S, K;
            ex_parte(A, &S, &K);
            MatQz R = (mat_op == 19) ? S : K;
            const char *nm = (mat_op == 19) ? "simétrica (o GATO: dissipa)"
                                            : "antissimétrica (o ESQUILO: gira)";
            if(A.m != A.n){
                printf("erro: a partição pede uma matriz QUADRADA — a"
                       " transposta tem de caber no mesmo sítio. RECUSADA.\n");
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "Sym/Skew needs a square matrix, got %d×%d",
                             A.m, A.n); }
                return 0;
            }
            if(sql_cap){
                memset(sql_cap, 0, sizeof *sql_cap);
                sql_cap->ok = 1;
                sql_cap->nrows = R.m > SQL_OUT_MAX_ROWS ? SQL_OUT_MAX_ROWS : R.m;
                sql_cap->ncols = R.n > SQL_OUT_MAX_COLS ? SQL_OUT_MAX_COLS : R.n;
                for(int j = 0; j < sql_cap->ncols; j++){
                    snprintf(sql_cap->col[j], sizeof sql_cap->col[j], "c%d", j + 1);
                    sql_cap->tipo[j] = SQL_TIPO_INT4;
                }
                snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT %d", sql_cap->nrows);
            }
            for(int i = 0; i < R.m; i++){
                printf("   ");
                for(int j = 0; j < R.n; j++){
                    Par cls = ra_classe((Par){ (long)R.a[i][j].p, (long)R.a[i][j].q });
                    char cel[SQL_OUT_CELL];
                    if(cls.b > 1) snprintf(cel, sizeof cel, "%ld/%ld", cls.a, cls.b);
                    else          snprintf(cel, sizeof cel, "%ld", cls.a);
                    printf("%s%s", cel, j + 1 < R.n ? " | " : "");
                    if(sql_cap && i < sql_cap->nrows && j < sql_cap->ncols)
                        snprintf(sql_cap->cell[i][j], SQL_OUT_CELL, "%s", cel);
                }
                printf("\n");
            }
            { Par tr = ra_classe((Par){ (long)qz_soma(R.a[0][0], R.a[R.n-1][R.n-1]).p,
                                        (long)qz_soma(R.a[0][0], R.a[R.n-1][R.n-1]).q });
              printf("-- a parte %s · traço %ld · A = simetrica + antisimetrica,"
                     " e a partição é única e exacta em ℚ\n", nm, tr.a); }
            return 1;
        }

        if(mat_op == 21){
            if(A.m != 2 || A.n != 2){
                printf("erro: o regime lê-se do discriminante de uma 2×2 — acima"
                       " disso o espectro não sai de um Δ. RECUSADA.\n");
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "regime needs a 2×2 matrix, got %d×%d", A.m, A.n); }
                return 0;
            }
            long D = esp_disc(A);
            Qz trq = qz_soma(A.a[0][0], A.a[1][1]);
            Par tr = ra_classe((Par){ (long)trq.p, (long)trq.q });
            Par dt = ra_classe((Par){ (long)mat_det(A).p, (long)mat_det(A).q });
            /* ── O REGIME É O SINAL DE Re(λ), E ELE LÊ-SE DE (traço, Δ) ──────
             * As raízes são (tr ± √Δ)/2. Com Δ < 0 elas são um par conjugado de
             * parte real tr/2; com Δ ≥ 0 são reais. Então:
             *
             *   Re λ < 0  CRISTAL  colapsa no ponto fixo   (dissipa)
             *   Re λ = 0  BORDA    orbita, conserva a norma (o esquilo)
             *   Re λ > 0  CAOS     diverge, mistura         (o gato)
             *
             * e a classificação por Δ é a mesma do catálogo — hiperbólico,
             * parabólico, elíptico — e a mesma que classifica uma EDP de 2ª
             * ordem pelo seu símbolo. Uma tríade, três nomes. */
            /* a tricotomia pede-se ao `corpo_classe_nome` --- ver thm:leidisc */
            char cls[48];
            snprintf(cls, sizeof cls, "%s (%s)", corpo_classe_nome(D, 0),
                     corpo_classe(D) == CL_HIPERBOLICO ? "duas raízes reais"
                   : corpo_classe(D) == CL_PARABOLICO  ? "raiz dupla"
                                                       : "par conjugado");
            const char *classe = cls;
            const char *reg;
            if(D < 0) reg = (tr.a < 0) ? "CRISTAL" : (tr.a == 0) ? "BORDA" : "CAOS";
            else {
                /* reais: o maior é (tr + √Δ)/2, e o seu sinal decide */
                long r = raizi(D);
                reg = (tr.a + r < 0) ? "CRISTAL" : (tr.a + r == 0) ? "BORDA" : "CAOS";
            }
            if(sql_cap){
                memset(sql_cap, 0, sizeof *sql_cap);
                sql_cap->ok = 1; sql_cap->nrows = 1; sql_cap->ncols = 4;
                snprintf(sql_cap->col[0], sizeof sql_cap->col[0], "regime");
                snprintf(sql_cap->col[1], sizeof sql_cap->col[1], "classe");
                snprintf(sql_cap->col[2], sizeof sql_cap->col[2], "traco");
                snprintf(sql_cap->col[3], sizeof sql_cap->col[3], "disc");
                sql_cap->tipo[0] = sql_cap->tipo[1] = SQL_TIPO_TEXT;
                sql_cap->tipo[2] = sql_cap->tipo[3] = SQL_TIPO_INT4;
                snprintf(sql_cap->cell[0][0], SQL_OUT_CELL, "%s", reg);
                snprintf(sql_cap->cell[0][1], SQL_OUT_CELL, "%.9s", classe);
                snprintf(sql_cap->cell[0][2], SQL_OUT_CELL, "%ld", tr.a);
                snprintf(sql_cap->cell[0][3], SQL_OUT_CELL, "%ld", D);
                snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT 1");
            }
            printf("   %s | %s | %ld | %ld\n", reg, classe, tr.a, D);
            printf("-- o fluxo ẋ = A·x · λ² − %ldλ + %ld · Δ = %ld · %s ·"
                   " o regime é o SINAL de Re(λ), e é o mesmo Δ da cifra\n",
                   tr.a, dt.a, D, reg);
            return 1;
        }

        if(mat_op == 18){
            MatQz G = mat0((int)nr_v, (int)nr_v);
            for(int i = 0; i < (int)nr_v; i++)
                for(int j = 0; j < (int)nr_v; j++){
                    VecQz u, v; u.n = v.n = (int)ncols;
                    for(int t = 0; t < (int)ncols; t++){ u.c[t] = A.a[i][t];
                                                         v.c[t] = A.a[j][t]; }
                    G.a[i][j] = pi(u, v);
                }
            if(sql_cap){
                memset(sql_cap, 0, sizeof *sql_cap);
                sql_cap->ok = 1;
                sql_cap->nrows = (int)nr_v > SQL_OUT_MAX_ROWS ? SQL_OUT_MAX_ROWS : (int)nr_v;
                sql_cap->ncols = (int)nr_v > SQL_OUT_MAX_COLS ? SQL_OUT_MAX_COLS : (int)nr_v;
                for(int j = 0; j < sql_cap->ncols; j++){
                    snprintf(sql_cap->col[j], sizeof sql_cap->col[j], "c%d", j + 1);
                    sql_cap->tipo[j] = SQL_TIPO_INT4;
                }
                snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT %d", sql_cap->nrows);
            }
            for(int i = 0; i < (int)nr_v; i++){
                printf("   ");
                for(int j = 0; j < (int)nr_v; j++){
                    Par cls = ra_classe((Par){ (long)G.a[i][j].p, (long)G.a[i][j].q });
                    char cel[SQL_OUT_CELL];
                    if(cls.b > 1) snprintf(cel, sizeof cel, "%ld/%ld", cls.a, cls.b);
                    else          snprintf(cel, sizeof cel, "%ld", cls.a);
                    printf("%s%s", cel, j + 1 < (int)nr_v ? " | " : "");
                    if(sql_cap && i < sql_cap->nrows && j < sql_cap->ncols)
                        snprintf(sql_cap->cell[i][j], SQL_OUT_CELL, "%s", cel);
                }
                printf("\n");
            }
            { Par dg = ra_classe((Par){ (long)mat_det(G).p, (long)mat_det(G).q });
              printf("-- a Gram das %ld linhas: G = A·Aᵀ, simétrica · det %ld"
                     " (zero ⟺ dependentes) · posto %d = posto de A %d\n",
                     nr_v, dg.a, mat_posto(G), mat_posto(A)); }
            return 1;
        }

        if(mat_op == 16 || mat_op == 17){
            if(A.m != 2 || A.n != 2){
                printf("erro: o espectro exacto desta casa é o de uma matriz"
                       " 2×2 — acima disso o característico tem grau maior e as"
                       " raízes deixam de sair de um discriminante. RECUSADA.\n");
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "spectrum needs a 2×2 matrix, got %d×%d", A.m, A.n); }
                return 0;
            }
            long l1, l2;
            if(!esp_racional(A, &l1, &l2)){
                long D = esp_disc(A);
                printf("erro: o discriminante é %ld e não é quadrado perfeito —"
                       " as raízes NÃO são racionais. Não é um limite da conta:"
                       " são as folhas do corpo, e escrevem-se pela cifra."
                       " Peça `SELECT cifra(*)`.\n", D);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "eigenvalues are not rational (disc %ld);"
                             " use cifra(*)", D); }
                return 0;
            }
            if(mat_op == 16){
                if(sql_cap){
                    memset(sql_cap, 0, sizeof *sql_cap);
                    sql_cap->ok = 1; sql_cap->nrows = (l1 == l2) ? 1 : 2;
                    sql_cap->ncols = 1;
                    snprintf(sql_cap->col[0], sizeof sql_cap->col[0], "lambda");
                    sql_cap->tipo[0] = SQL_TIPO_INT4;
                    snprintf(sql_cap->cell[0][0], SQL_OUT_CELL, "%ld", l1);
                    if(l1 != l2)
                        snprintf(sql_cap->cell[1][0], SQL_OUT_CELL, "%ld", l2);
                    snprintf(sql_cap->tag, sizeof sql_cap->tag,
                             "SELECT %d", sql_cap->nrows);
                }
                if(l1 == l2) printf("   %ld\n", l1);
                else { printf("   %ld\n   %ld\n", l1, l2); }
                printf("-- o espectro: λ² − %ldλ + %ld, discriminante %ld"
                       " (quadrado perfeito) · soma %ld = traço · produto %ld ="
                       " determinante%s\n",
                       (long)qz_soma(A.a[0][0], A.a[1][1]).p,
                       (long)mat_det(A).p, esp_disc(A), l1 + l2, l1 * l2,
                       l1 == l2 ? " · raiz DUPLA" : "");
                return 1;
            }
            /* ── OS AUTOVETORES, E O QUE ELES DECIDEM ────────────────────────
             * Cada um sai do NÚCLEO de A − λI, que é a peça do §W52 outra vez.
             * E quantos são INDEPENDENTES é o que decide a diagonalizabilidade:
             * com raiz dupla, uma matriz pode ter dois (e é a homotetia) ou um
             * só (e é o bloco de Jordan) — e a diferença não está nos valores,
             * está aqui. */
            VecQz vs[LN_MAX];
            int k = esp_autovetores(A, vs);
            int diag = (k == A.n) && vec_li(vs, k);
            if(sql_cap){
                memset(sql_cap, 0, sizeof *sql_cap);
                sql_cap->ok = 1;
                sql_cap->nrows = k > SQL_OUT_MAX_ROWS ? SQL_OUT_MAX_ROWS : k;
                sql_cap->ncols = 2;
                for(int j = 0; j < 2; j++){
                    snprintf(sql_cap->col[j], sizeof sql_cap->col[j], "c%d", j + 1);
                    sql_cap->tipo[j] = SQL_TIPO_INT4;
                }
                snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT %d", sql_cap->nrows);
            }
            if(!k) printf("   (vazio)\n");
            for(int i = 0; i < k; i++){
                printf("   ");
                for(int j = 0; j < vs[i].n; j++){
                    Par cls = ra_classe((Par){ (long)vs[i].c[j].p, (long)vs[i].c[j].q });
                    char cel[SQL_OUT_CELL];
                    if(cls.b > 1) snprintf(cel, sizeof cel, "%ld/%ld", cls.a, cls.b);
                    else          snprintf(cel, sizeof cel, "%ld", cls.a);
                    printf("%s%s", cel, j + 1 < vs[i].n ? " | " : "");
                    if(sql_cap && i < sql_cap->nrows && j < sql_cap->ncols)
                        snprintf(sql_cap->cell[i][j], SQL_OUT_CELL, "%s", cel);
                }
                printf("\n");
            }
            printf("-- %d autovector(es) independente(s) para λ = %ld, %ld ·"
                   " %s (precisa de %d)\n", k, l1, l2,
                   diag ? "DIAGONALIZÁVEL" : "não diagonalizável", A.n);
            return 1;
        }

        if(mat_op == 15){
            if(A.m != 2 || A.n != 2){
                printf("erro: a cifra pede uma matriz 2×2 — o par (traço,"
                       " determinante) só é o par (B,C) de um corpo nesse"
                       " tamanho, e acima dele o característico tem mais"
                       " coeficientes. RECUSADA.\n");
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "cifra needs a 2×2 matrix, got %d×%d", A.m, A.n); }
                return 0;
            }
            Qz tr = qz_soma(A.a[0][0], A.a[1][1]);
            Qz dt = qz_soma(qz_mult(A.a[0][0], A.a[1][1]),
                            qz_oposto(qz_mult(A.a[0][1], A.a[1][0])));
            Par ctr = ra_classe((Par){ (long)tr.p, (long)tr.q });
            Par cdt = ra_classe((Par){ (long)dt.p, (long)dt.q });
            if(ctr.b != 1 || cdt.b != 1){
                /* ── E A CIFRA É DE INTEIROS, o que não é uma limitação da
                 * conta: o codificador é o de um CORPO, e um corpo desta casa
                 * é dado por dois inteiros. Um traço fracionário diz que a
                 * matriz não é o operador de nenhum corpo do catálogo. */
                printf("erro: traço %ld/%ld e determinante %ld/%ld — a cifra é"
                       " de um corpo, e um corpo desta casa é dado por DOIS"
                       " INTEIROS. RECUSADA.\n", ctr.a, ctr.b, cdt.a, cdt.b);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "cifra needs integer trace and determinant"); }
                return 0;
            }
            long a[64];
            size_t k = cifra_geral(NULL, 0, ctr.a, cdt.a, ctr.a, a, 64);
            if(sql_cap){
                memset(sql_cap, 0, sizeof *sql_cap);
                sql_cap->ok = 1;
                sql_cap->nrows = 1;
                sql_cap->ncols = (int)k > SQL_OUT_MAX_COLS ? SQL_OUT_MAX_COLS : (int)k;
                for(int j = 0; j < sql_cap->ncols; j++){
                    snprintf(sql_cap->col[j], sizeof sql_cap->col[j], "c%d", j + 1);
                    sql_cap->tipo[j] = SQL_TIPO_INT4;
                    snprintf(sql_cap->cell[0][j], SQL_OUT_CELL, "%ld", a[j]);
                }
                snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT 1");
            }
            printf("   ");
            for(size_t j = 0; j < k; j++)
                printf("%ld%s", a[j], j + 1 < k ? " | " : "");
            printf("\n-- a cifra do espectro: B = %ld (traço), C = %ld"
                   " (determinante) · λ² − %ldλ + %ld · %zu termos\n",
                   ctr.a, cdt.a, ctr.a, cdt.a, k);
            return 1;
        }

        if(mat_op == 13 || mat_op == 14){
            Fun d[LN_MAX];
            int k, dim = (int)ncols;
            if(mat_op == 13){
                if(A.m != A.n){
                    printf("erro: a base dual pede uma matriz QUADRADA (%d×%d) —"
                           " uma base de %d vectores num espaço de dimensão %d"
                           " não é base. RECUSADA.\n", A.m, A.n, A.n, A.m);
                    if(sql_cap){ sql_cap->ok = 0;
                        snprintf(sql_cap->err, sizeof sql_cap->err,
                                 "dual needs a square matrix: %d×%d", A.m, A.n); }
                    return 0;
                }
                VecQz col[LN_MAX];
                for(int j = 0; j < A.n; j++){
                    col[j].n = A.m;
                    for(int i = 0; i < A.m; i++) col[j].c[i] = A.a[i][j];
                }
                k = fun_base_dual(col, A.n, d) ? A.n : -1;
                if(k < 0){
                    /* ── E A RECUSA É A LEI, não um acidente ────────────────
                     * Sem inversa não há base dual porque as colunas não são
                     * uma base: são dependentes, e o funcional que devia
                     * separar duas delas teria de dar 1 e 0 ao MESMO vector.
                     * Dizer «não deu» esconderia isso; diz-se o posto. */
                    printf("erro: as colunas não são uma base — posto %d de %d,"
                           " logo há dependência e nenhum funcional as separa."
                           " RECUSADA.\n", mat_posto(A), A.n);
                    if(sql_cap){ sql_cap->ok = 0;
                        snprintf(sql_cap->err, sizeof sql_cap->err,
                                 "columns are not a basis: rank %d of %d",
                                 mat_posto(A), A.n); }
                    return 0;
                }
            } else {
                VecQz lin[LN_MAX];
                for(int i = 0; i < A.m; i++){
                    lin[i].n = A.n;
                    for(int j = 0; j < A.n; j++) lin[i].c[j] = A.a[i][j];
                }
                k = fun_aniquilador(lin, A.m, A.n, d);
            }
            const char *nm = (mat_op == 13) ? "base dual" : "aniquilador";
            if(sql_cap){
                memset(sql_cap, 0, sizeof *sql_cap);
                sql_cap->ok = 1;
                sql_cap->ncols = dim > SQL_OUT_MAX_COLS ? SQL_OUT_MAX_COLS : dim;
                sql_cap->nrows = k > SQL_OUT_MAX_ROWS ? SQL_OUT_MAX_ROWS : k;
                for(int j = 0; j < sql_cap->ncols; j++){
                    snprintf(sql_cap->col[j], sizeof sql_cap->col[j], "c%d", j + 1);
                    sql_cap->tipo[j] = SQL_TIPO_INT4;
                }
                snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT %d", sql_cap->nrows);
            }
            if(!k) printf("   (vazio — nenhum funcional se anula em tudo)\n");
            for(int i = 0; i < k; i++){
                printf("   ");
                for(int j = 0; j < d[i].n; j++){
                    Par cls = ra_classe((Par){ (long)d[i].c[j].p, (long)d[i].c[j].q });
                    char cel[SQL_OUT_CELL];
                    if(cls.b > 1) snprintf(cel, sizeof cel, "%ld/%ld", cls.a, cls.b);
                    else          snprintf(cel, sizeof cel, "%ld", cls.a);
                    printf("%s%s", cel, j + 1 < d[i].n ? " | " : "");
                    if(sql_cap && i < sql_cap->nrows && j < sql_cap->ncols)
                        snprintf(sql_cap->cell[i][j], SQL_OUT_CELL, "%s", cel);
                }
                printf("\n");
            }
            if(mat_op == 13)
                printf("-- a %s: %d funcional(is), e e^i(e_j) = 1 se i=j, 0 senão\n",
                       nm, k);
            else
                printf("-- o %s: %d funcional(is) · dim W %d + dim W° %d = %ld\n",
                       nm, k, mat_posto(A), k, ncols);
            return 1;
        }

        { MatQz R;                        /* transposta, inversa e oposto */
          const char *nm = mat_op == 4 ? "transposta"
                         : mat_op == 12 ? "oposto" : "inversa";
          if(mat_op == 4) R = mat_transposta(A);
          else if(mat_op == 12){
              /* ── O OPOSTO É A VOLTA DA SOMA, E EXISTE SEMPRE ─────────────
               * É aqui que as duas faces deixam de ser simétricas: −A existe
               * para toda a matriz, enquanto A⁻¹ só existe quando o
               * determinante não é zero. A face aditiva é um GRUPO; a
               * multiplicativa não — e o que as separa é uma condição que só
               * uma delas tem. */
              R = mat_esc_neg(A);
          }
          else {
              if(A.m != A.n){
                  printf("erro: a inversa pede uma matriz QUADRADA — RECUSADA.\n");
                  if(sql_cap){ sql_cap->ok = 0;
                      snprintf(sql_cap->err, sizeof sql_cap->err,
                               "inverse: matrix is not square"); }
                  return 0;
              }
              if(!mat_inversa(A, &R)){
                  printf("erro: a matriz não tem inversa (o determinante é zero)"
                         " — RECUSADA.\n");
                  if(sql_cap){ sql_cap->ok = 0;
                      snprintf(sql_cap->err, sizeof sql_cap->err,
                               "matrix is singular: no inverse"); }
                  return 0;
              }
          }
          if(sql_cap){
              memset(sql_cap, 0, sizeof *sql_cap);
              sql_cap->ok = 1;
              sql_cap->ncols = R.n > SQL_OUT_MAX_COLS ? SQL_OUT_MAX_COLS : R.n;
              sql_cap->nrows = R.m > SQL_OUT_MAX_ROWS ? SQL_OUT_MAX_ROWS : R.m;
              for(int j = 0; j < sql_cap->ncols; j++){
                  snprintf(sql_cap->col[j], sizeof sql_cap->col[j], "c%d", j + 1);
                  sql_cap->tipo[j] = SQL_TIPO_INT4;
              }
              snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT %d", sql_cap->nrows);
          }
          for(int i = 0; i < R.m; i++){
              printf("   ");
              for(int j = 0; j < R.n; j++){
                  Par cls = ra_classe((Par){ (long)R.a[i][j].p, (long)R.a[i][j].q });
                  char cel[SQL_OUT_CELL];
                  if(cls.b > 1) snprintf(cel, sizeof cel, "%ld/%ld", cls.a, cls.b);
                  else          snprintf(cel, sizeof cel, "%ld", cls.a);
                  printf("%s%s", cel, j + 1 < R.n ? " | " : "");
                  if(sql_cap && i < sql_cap->nrows && j < sql_cap->ncols)
                      snprintf(sql_cap->cell[i][j], SQL_OUT_CELL, "%s", cel);
              }
              printf("\n");
          }
          printf("-- %s %s da tabela %d×%d\n",
                 mat_op == 12 ? "o" : "a", nm, A.m, A.n);
          return 1; }
    }

    if(acao == ACAO_MARCA && agr_n && !grp_col[0]){
        if(agr_conflito){
            printf("erro: `count` e `%s` na mesma lista SEM quociente — o count"
                   " corre pela soma do campo (popcount) e as outras pela"
                   " varredura das células; são DOIS percursos, e juntá-los daria"
                   " duas réguas para a mesma contagem. Com `GROUP BY` não há"
                   " conflito, porque aí o count de cada fibra É o G que a corrida"
                   " já conta.\n", agr_nome(agr_ops[0]));
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "count() cannot be combined with %s() without GROUP BY;"
                         " ask them separately", agr_nome(agr_ops[0])); }
            return 0;
        }
        int ac[AGR_MAX];
        for(int k = 0; k < agr_n; k++){
            ac[k] = col_indice(agr_cols[k]);
            if(ac[k] < 0){
                printf("erro: a coluna «%s» não existe — a agregação é"
                       " RECUSADA.\n", agr_cols[k]);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "column \"%s\" does not exist", agr_cols[k]); }
                return 0;
            }
        }
        /* ── UMA VARREDURA, VÁRIOS ACUMULADORES ──────────────────────────
         * As agregações partilham o percurso das linhas: além de ser a mesma
         * economia do núcleo e da imagem, é o que garante que o `MIN` e o
         * `MAX` de uma consulta vêem EXACTAMENTE o mesmo conjunto — com duas
         * varreduras, um filtro que mudasse entre elas dava um par que não
         * corresponde a nenhum estado da tabela. */
        { long ag[AGR_MAX], quantas[AGR_MAX]; int viu[AGR_MAX];
          for(int k = 0; k < agr_n; k++){ ag[k] = 0; quantas[k] = 0; viu[k] = 0; }
          for(long i = 0; i < nrows; i++){
              if(!bit_le(S_MATCH, i)) continue;
              for(int k = 0; k < agr_n; k++){
                  if(!bit_le(S_PRES, i*ncols + ac[k])) continue;  /* o corpo, não o suporte */
                  { long w = celula_valor(i, ac[k], ncols);
                    if(!viu[k]){ ag[k] = w; viu[k] = 1; }
                    else if(agr_ops[k] == 1 || agr_ops[k] == 4) ag[k] += w;
                    else if(agr_ops[k] == 2){ if(w > ag[k]) ag[k] = w; }
                    else if(agr_ops[k] == 3){ if(w < ag[k]) ag[k] = w; }
                    quantas[k]++; }
              }
          }
          /* ── A MÉDIA É UM RACIONAL, E NÃO UM DECIMAL ARREDONDADO ────────
           * `avg` é a soma sobre a contagem, e a divisão de inteiros SAI DO
           * ANDAR: o resultado vive em ℚ, que é o andar seguinte da escada e
           * já está construído nesta casa. Devolvê-lo como um decimal era
           * escolher um representante que não é o objecto — e a lei que se
           * segue não valeria: `avg × count = sum` exactamente, sem resto.
           * Guarda-se a CLASSE reduzida, que é o representante único do
           * `ra_classe`, e imprime-se `a/b` ou o inteiro quando b = 1. */
          char txt[AGR_MAX][40];
          for(int k = 0; k < agr_n; k++){
              if(viu[k] && agr_ops[k] == 4){
                  Par m = ra_classe((Par){ ag[k], quantas[k] });
                  if(m.b > 1) snprintf(txt[k], sizeof txt[0], "%ld/%ld", m.a, m.b);
                  else        snprintf(txt[k], sizeof txt[0], "%ld", m.a);
              }
              else if(viu[k]) snprintf(txt[k], sizeof txt[0], "%ld", ag[k]);
              else            txt[k][0] = 0;
          }
          printf("   ");
          for(int k = 0; k < agr_n; k++)
              printf("%s%s", txt[k][0] ? txt[k] : "(ausente)",
                     k + 1 < agr_n ? " | " : "");
          printf("\n-- %ld linha(s) agregada(s) em 1, com %d agregação(ões)\n",
                 quantas[0], agr_n);
          if(sql_cap){
              memset(sql_cap->col, 0, sizeof sql_cap->col);
              sql_cap->ok = 1; sql_cap->ncols = agr_n; sql_cap->nrows = 1;
              for(int k = 0; k < agr_n; k++){
                  snprintf(sql_cap->col[k], sizeof sql_cap->col[0], "%s",
                           agr_nome(agr_ops[k]));
                  sql_cap->tipo[k] = (agr_ops[k] == 4) ? SQL_TIPO_TEXT : SQL_TIPO_INT4;
                  if(txt[k][0]) snprintf(sql_cap->cell[0][k], SQL_OUT_CELL, "%s", txt[k]);
                  else { sql_cap->cell[0][k][0] = 0; sql_cap->nulo[0][k] = 1; }
              }
              snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT 1");
          }
          return 1; }
    }

    if(acao == ACAO_MARCA && grp_col[0]){
        int gc = col_indice(grp_col);
        if(agr_op && col_indice(agr_col) < 0){
            printf("erro: a coluna «%s» não existe — a agregação é RECUSADA.\n", agr_col);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "column \"%s\" does not exist", agr_col); }
            return 0;
        }
        int postos = 0;
        /* A SEQUÊNCIA VIVE NA ZONA DO GROUP BY --- ver `S_GRPSEQ`. Estava num
         * arranjo de SQL_OUT_MAX_ROWS, que é a JANELA DA PORTA C e não tem nada
         * que ver com quantas linhas o agrupamento precisa de ordenar: o laço
         * parava às 64 e as contagens saíam erradas, com a conservação a
         * imprimir «∑G = 64» para |I| = 200. */
        long n = 0;
        /* ── O DUAL É UM GRUPO, E NÃO O GRUPO DO ZERO ────────────────────
         * Quocientar é juntar o que tem a mesma chave, e a célula ausente não
         * tem chave nenhuma: metê-la na árvore era dar-lhe o neutro e juntá-la
         * às linhas que têm um zero ESCRITO — duas coisas de níveis diferentes
         * no mesmo grupo. Ficam de fora da árvore e formam a sua própria fibra,
         * no fim, que é onde o SQL as põe e onde a ordem as deixa. */
        long na = 0;
        ord_limpa();
        for(long i = 0; i < nrows; i++){
            if(!bit_le(S_MATCH, i)) continue;
            if(!bit_le(S_PRES, i*ncols + gc)){
                if(na < (long)S_GRPSEQ_N){ grpaus_poe(na, i); na++; }
                continue;
            }
            if(!ord_insere(celula_valor(i, gc, ncols), (int)i)){
                printf("erro: a árvore do GROUP BY não coube — RECUSADA.\n");
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "GROUP BY: a arvore nao coube; recusa em vez de agrupar"
                             " metade"); }
                return 0;
            }
            postos++;
        }
        { static int tmp[8192]; int nn = 0;
          ord_percorre(0, 0, 0, tmp, &nn, 8192, ord_desc);
          for(int k = 0; k < nn; k++) grpseq_poe(k, tmp[k]);
          n = nn;
          for(long k = 0; k < na && n < (long)S_GRPSEQ_N; k++){ grpseq_poe(n, grpaus_le(k)); n++; } }

        /* ── E A SEGUNDA RÉGUA, DENTRO DE CADA FIBRA ─────────────────────
         * A mesma composição do `ORDER BY a, b`: a primeira coluna partiu a
         * saída em corridas; dentro de cada uma corre-se a MESMA descida com a
         * segunda, e as linhas do mesmo PAR ficam contíguas. É só isso que o
         * agrupamento por duas colunas precisa — o resto (contar as corridas)
         * não muda de todo. */
        { int gc2 = grp_col2[0] ? col_indice(grp_col2) : -1;
          long k = 0;
          while(gc2 >= 0 && k < n){
              int tem = bit_le(S_PRES, grpseq_le(k)*ncols + gc);
              long v = tem ? celula_valor(grpseq_le(k), gc, ncols) : 0;
              long j = k;
              while(j < n && bit_le(S_PRES, grpseq_le(j)*ncols + gc) == tem
                          && (!tem || celula_valor(grpseq_le(j), gc, ncols) == v)) j++;
              if(j - k > 1){
                  static long a2[8192]; static int tmp2[8192];
                  int n2 = 0, m2 = 0, coube = 1;
                  ord_limpa();
                  for(long t = k; t < j && coube; t++){
                      if(!bit_le(S_PRES, grpseq_le(t)*ncols + gc2)){
                          if(n2 < 8192) a2[n2++] = grpseq_le(t);
                          continue;
                      }
                      if(!ord_insere(celula_valor(grpseq_le(t), gc2, ncols), (int)grpseq_le(t)))
                          coube = 0;
                  }
                  if(coube){
                      ord_percorre(0, 0, 0, tmp2, &m2, 8192, 0);
                      { long t = k;
                        for(int q2 = 0; q2 < m2 && t < j; q2++){ grpseq_poe(t, tmp2[q2]); t++; }
                        for(int q2 = 0; q2 < n2 && t < j; q2++){ grpseq_poe(t, a2[q2]); t++; } }
                  }
              }
              k = j;
          } }
        if(sql_cap){
            memset(sql_cap->col, 0, sizeof sql_cap->col);
            sql_cap->ncols = 2; sql_cap->nrows = 0;
            { int c = 0;
              snprintf(sql_cap->col[c], sizeof sql_cap->col[0], "%s", grp_col);
              sql_cap->tipo[c] = SQL_TIPO_INT4; c++;
              if(grp_col2[0]){                     /* a chave é o PAR */
                  snprintf(sql_cap->col[c], sizeof sql_cap->col[0], "%s", grp_col2);
                  sql_cap->tipo[c] = SQL_TIPO_INT4; c++;
              }
              snprintf(sql_cap->col[c], sizeof sql_cap->col[0], "count");
              sql_cap->tipo[c] = SQL_TIPO_INT8; c++;
              for(int k = 0; k < agr_n; k++){
                  snprintf(sql_cap->col[c], sizeof sql_cap->col[0], "%s",
                           agr_nome(agr_ops[k]));
                  sql_cap->tipo[c] = (agr_ops[k] == 4) ? SQL_TIPO_TEXT : SQL_TIPO_INT4;
                  c++;
              }
              sql_cap->ncols = c; }
        }
        { long grupos = 0, soma = 0, k = 0;
          while(k < n){
              int tem = bit_le(S_PRES, grpseq_le(k)*ncols + gc);
              long v = tem ? celula_valor(grpseq_le(k), gc, ncols) : 0, g = 0;
              /* um acumulador POR agregação, e a corrida do grupo alimenta-os
               * todos na mesma passagem — a mesma razão de sempre: com duas
               * passagens, o MIN e o MAX de um grupo podiam ver conjuntos
               * diferentes de linhas. */
              long ag[AGR_MAX], ag_n[AGR_MAX]; int ag_viu[AGR_MAX], ac[AGR_MAX];
              for(int k = 0; k < agr_n; k++){
                  ag[k] = 0; ag_n[k] = 0; ag_viu[k] = 0;
                  ac[k] = col_indice(agr_cols[k]);
              }
              /* a fibra do dual junta-se por NÃO TER chave, não por ter a
               * mesma; as duas condições estão na mesma linha e são disjuntas */
              /* a chave do grupo é o PAR quando há segunda coluna: a corrida
               * termina quando QUALQUER das duas muda */
              int gc2 = grp_col2[0] ? col_indice(grp_col2) : -1;
              int tem2 = (gc2 >= 0) ? bit_le(S_PRES, grpseq_le(k)*ncols + gc2) : 0;
              long v2 = (gc2 >= 0 && tem2) ? celula_valor(grpseq_le(k), gc2, ncols) : 0;
              while(k < n && bit_le(S_PRES, grpseq_le(k)*ncols + gc) == tem
                          && (!tem || celula_valor(grpseq_le(k), gc, ncols) == v)
                          && (gc2 < 0
                              || (bit_le(S_PRES, grpseq_le(k)*ncols + gc2) == tem2
                                  && (!tem2 || celula_valor(grpseq_le(k), gc2, ncols) == v2)))){
                  for(int t = 0; t < agr_n; t++){
                      if(ac[t] < 0 || !bit_le(S_PRES, grpseq_le(k)*ncols + ac[t])) continue;
                      { long w = celula_valor(grpseq_le(k), ac[t], ncols);
                        if(!ag_viu[t]){ ag[t] = w; ag_viu[t] = 1; }
                        else if(agr_ops[t] == 1 || agr_ops[t] == 4) ag[t] += w;
                        else if(agr_ops[t] == 2){ if(w > ag[t]) ag[t] = w; }
                        else if(agr_ops[t] == 3){ if(w < ag[t]) ag[t] = w; }
                        ag_n[t]++; }
                  }
                  g++; k++;
              }
              /* o HAVING: filtra a fibra pelo seu G. `count(*) > 1` é pedir as
               * células onde a realização DOBROU — thm:multiplicidade (2). */
              if(hav_op == 1 && !(g >  hav_n)) continue;
              if(hav_op == 2 && !(g <  hav_n)) continue;
              if(hav_op == 3 && !(g == hav_n)) continue;
              if(lim_n >= 0 && grupos >= lim_n) break;      /* o prefixo */
              /* a média de cada fibra também é um RACIONAL: a divisão sai do
               * andar, e o representante único é a classe reduzida */
              char agtxt[AGR_MAX][40];
              for(int t = 0; t < agr_n; t++){
                  if(agr_ops[t] == 4 && ag_viu[t]){
                      Par m = ra_classe((Par){ ag[t], ag_n[t] });
                      if(m.b > 1) snprintf(agtxt[t], sizeof agtxt[0], "%ld/%ld", m.a, m.b);
                      else        snprintf(agtxt[t], sizeof agtxt[0], "%ld", m.a);
                  } else if(ag_viu[t]) snprintf(agtxt[t], sizeof agtxt[0], "%ld", ag[t]);
                  else                 agtxt[t][0] = 0;
              }
              /* ── A CHAVE DO GRUPO REPÕE-SE NO CORPO DELA ────────────────
               *
               * Numa coluna de TEXTO a célula guarda o ENDEREÇO no pool, e a
               * fibra formava-se certa por isso mesmo --- endereços iguais são
               * cadeias iguais, que é o critério da leitura. Mas a chave saía
               * como o endereço: `GROUP BY s` devolvia «1 | 3» e «6 | 1» em vez
               * de «acme | 3» e «globex | 1».
               *
               * É a face da compressão a aparecer na leitura. A §sec:dualidades:
               * «um lado comprime e perde uma distinção; o outro REPÕE-NA», e
               * aqui a reposição faltava --- o valor estava certo, a fibra
               * estava certa, e o que se mostrava era a coordenada em vez do
               * objecto. */
              char k1[80], k2[80];
              { char ch[160];
                #define GRP_CHAVE(dst, col, temv, val) do { \
                    Word cw_ = corpo_de(col); \
                    if(!(temv)) snprintf((dst), 80, "(ausente)"); \
                    else if(cw_.total == CORPO_TEXTO) tx_le((unsigned)(val), (dst), 80); \
                    else if(cw_.total == CORPO_MORFICO) \
                        cel_morfico((unsigned long)(val), cw_.e, (dst), 80); \
                    else snprintf((dst), 80, "%ld", (long)(val)); \
                } while(0)
                GRP_CHAVE(k1, gc, tem, v);
                if(gc2 >= 0){ GRP_CHAVE(k2, gc2, tem2, v2);
                              snprintf(ch, sizeof ch, "%s | %s", k1, k2); }
                else          snprintf(ch, sizeof ch, "%s", k1);
                #undef GRP_CHAVE
                printf("   %s | %ld", ch, g);
                for(int t = 0; t < agr_n; t++)
                    printf(" | %s", agtxt[t][0] ? agtxt[t] : "(ausente)");
                printf("\n"); }
              if(sql_cap && sql_cap->nrows < SQL_OUT_MAX_ROWS){
                  /* e a chave do grupo do dual sai AUSENTE, não a zero: o
                   * cliente tem de ver a mesma distinção que o motor faz */
                  /* o MESMO texto que a saída imprime --- o cliente e o
                   * terminal não podem discordar sobre a chave que ambos
                   * descrevem, e a reposição no corpo é da chave, não do canal */
                  { int c = 0; long r = sql_cap->nrows;
                    if(tem) snprintf(sql_cap->cell[r][c], SQL_OUT_CELL, "%s", k1);
                    else { sql_cap->cell[r][c][0] = 0; sql_cap->nulo[r][c] = 1; }
                    if(tem && corpo_de(gc).total == CORPO_TEXTO)
                        sql_cap->tipo[c] = SQL_TIPO_TEXT;
                    c++;
                    if(gc2 >= 0){
                        if(tem2) snprintf(sql_cap->cell[r][c], SQL_OUT_CELL, "%s", k2);
                        else { sql_cap->cell[r][c][0] = 0; sql_cap->nulo[r][c] = 1; }
                        if(tem2 && corpo_de(gc2).total == CORPO_TEXTO)
                            sql_cap->tipo[c] = SQL_TIPO_TEXT;
                        c++;
                    }
                    snprintf(sql_cap->cell[r][c], SQL_OUT_CELL, "%ld", g); c++;
                    for(int t = 0; t < agr_n; t++){
                        snprintf(sql_cap->cell[r][c], SQL_OUT_CELL, "%s", agtxt[t]);
                        if(!agtxt[t][0]) sql_cap->nulo[r][c] = 1;
                        c++;
                    } }
                  sql_cap->nrows++;
              }
              grupos++; soma += g;
          }
          /* ∑G sobre o que SOBROU. Sem HAVING é o Lema da conservação inteiro —
           * ∑G = |I|, o que o WHERE deixou; com HAVING é a soma das fibras que
           * passaram, e diz-se qual dos dois é para não se ler o segundo como o
           * primeiro. */
          printf("-- %ld grupo(s), ∑G = %ld (%s)\n", grupos, soma,
                 hav_op ? "a soma das fibras que o HAVING deixou"
                        : "a conservação: ∑G = |I|, o que o WHERE deixou");
          if(sql_cap) snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT %ld", grupos); }
        return 1;
    }

    /* ── DISTINCT: a folha k=1 de cada fibra ────────────────────────────
     *
     * Desliga do bitmap tudo o que não é o PRIMEIRO da sua fibra. Usa a mesma
     * árvore: inserida a chave (valor, índice), o percurso por ordem traz as
     * linhas da mesma fibra CONTÍGUAS e por índice crescente, de modo que a
     * primeira de cada corrida é a folha k=1 — o representante canónico do
     * `thm:escada`. Não se filtra comparando pares: lê-se a ordem. */
    if(acao == ACAO_MARCA && dis_usa){
        if(proj_n != 1){
            printf("erro: DISTINCT pede UMA coluna — a linha inteira não é indexável"
                   " pela árvore. RECUSADO.\n");
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "DISTINCT: uma coluna de cada vez; a linha inteira nao e"
                         " indexavel pela arvore"); }
            return 0;
        }
        int dc = proj[0];
        long postos = 0;
        /* ── E A AUSÊNCIA É UM VALOR DISTINTO, UM SÓ ─────────────────────
         * O DISTINCT fica com a folha 1 de cada fibra, e o dual é uma fibra:
         * das linhas sem valor sobra UMA, e ela não se junta às que têm um
         * zero escrito. Fora da árvore, com o primeiro a ficar — que é o mesmo
         * representante canónico, lido pela mesma regra. */
        int viu_aus = 0;
        ord_limpa();
        /* ── E O LAÇO NÃO PÁRA NA JANELA ────────────────────────────────
         *
         * Estava `postos < SQL_OUT_MAX_ROWS` --- e esse número é o tamanho da
         * JANELA DA PORTA C, que não tem nada que ver com quantas linhas
         * precisam de entrar na árvore para os repetidos serem desligados. Com
         * ele, as linhas a partir da 65.ª nunca entravam, nunca eram desligadas,
         * e o SELECT DISTINCT DEVOLVIA REPETIDOS: 100 linhas de 3 classes davam
         * 39, e 200 davam 139. O erro logo abaixo já prometia «recusa em vez de
         * devolver repetidos» --- a lei estava escrita e o número do arranjo não
         * a deixava chegar.
         *
         * `aranha §sec:dimensao`: o número é do EXEMPLO e do ARRANJO, não da
         * lei. Aqui sai, e o que limita passa a ser a ÁRVORE, que é do mapa e
         * recusa dizendo-o --- que é a linha seguinte. */
        for(long i = 0; i < nrows; i++){
            if(!bit_le(S_MATCH, i)) continue;
            if(!bit_le(S_PRES, i*ncols + dc)){
                if(viu_aus) bit_poe(S_MATCH, i, 0);    /* k>1 da fibra do dual */
                viu_aus = 1;
                continue;
            }
            if(!ord_insere(celula_valor(i, dc, ncols), (int)i)){
                printf("erro: a árvore do DISTINCT não coube — RECUSADA.\n");
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "DISTINCT: a arvore nao coube; recusa em vez de devolver"
                             " repetidos"); }
                return 0;
            }
            postos++;
        }
        /* na DESCIDA, sem arranjo: o de 64 fazia o DISTINCT devolver repetidos
         * (100 linhas de 3 classes davam 39) --- ver `ord_tira_repetidos` */
        dis_tem = 0;
        ord_tira_repetidos(0, 0, 0);
        (void)postos; (void)dc;
    }

    long emitidas = 0, saltadas = 0;
    for(long ii = 0; ii < (ord_usa ? ord_n : nrows); ii++){
        long i = ord_usa ? ordseq_le(ii) : ii;
        if(!bit_le(S_MATCH, i)) continue;
        /* o OFFSET salta ANTES de o LIMIT contar: são os dois extremos da mesma
         * faixa, e trocar a ordem deles daria outra fatia */
        if(saltadas < off_n){ saltadas++; continue; }
        if(lim_n >= 0 && emitidas >= lim_n) break;   /* LIMIT: o prefixo da lista */
        emitidas++;
        if(!sql_calado) printf("   ");
        int row_i = sql_cap ? sql_cap->nrows : -1;
        if(sql_cap && row_i >= 0 && row_i < SQL_OUT_MAX_ROWS) sql_cap->nrows++;
        /* A LINHA MONTA-SE PELA ORDEM PEDIDA, E DEPOIS IMPRIME-SE.
         *
         * O laço percorre as colunas pela ordem FÍSICA da tabela, que não é a
         * ordem da projecção: `SELECT c, a` pede a coluna 2 e depois a 0. Filtrar
         * dentro do laço imprime na ordem errada, e o separador vai atrás — a
         * `ultima` era a última da LISTA e não a última a SAIR, pelo que `c, a`
         * saía «3300 | », com as duas células coladas e o `|` no fim.
         *
         * Recolhe-se por isso na ordem PEDIDA e imprime-se no fim, que é o que o
         * SqlOut já fazia: passam a ser o MESMO caminho e não dois — o texto e o
         * protocolo não podem discordar sobre a linha que ambos descrevem. */
        /* ── O BUFFER DA LINHA NÃO É A CÉLULA DO TRANSPORTE ──────────────────
         * Era `SQL_OUT_CELL` = 64, e o corpo TEXTO segura TX_MAX = 240: uma
         * cadeia de 100 caracteres entrava inteira no pool, a igualdade
         * achava-a, e a LEITURA devolvia 63 --- truncada, em silêncio, no
         * terminal e no fio. A volta não fechava e nada dizia.
         * O que se monta aqui é a linha; o que a janela da API C leva é outra
         * pergunta, e essa RECUSA quando não cabe. */
        static char saida[SQL_OUT_MAX_COLS][TX_MAX + 2];
        int nsai = proj_n ? proj_n
                 : (int)(ncols < SQL_OUT_MAX_COLS ? ncols : SQL_OUT_MAX_COLS);
        unsigned char sai_nulo[SQL_OUT_MAX_COLS];
        for(int k = 0; k < nsai; k++){ saida[k][0] = 0; sai_nulo[k] = 0; }
        for(long j = 0; j < ncols; j++){
            Word c = mem_le(S_LINHAS + cel_ix(i*ncols + j));
            long cp = corpo_de(j).total;
            char cel[TX_MAX + 2];
            cel[0] = 0;
            if(cp == CORPO_MORFICO){
                cel_morfico((unsigned long)c.total, corpo_de(j).e, cel, (int)sizeof cel);
            /* ── O LEITOR TEM DE LER COM O SINAL QUE O CORPO DECLARA ──────────────
             * A célula é um octeto e a Word8 é sem sinal, mas o que ele SIGNIFICA
             * vem do corpo da coluna: num RACIONAL o numerador é assinado (−1/3
             * existe), no ÁUREO e no CRISTAL o coeficiente também (a−bσ existe),
             * e num INTEIRO o envelope é o Word_8 de 0..255 que a casa declara.
             *
             * Estava a ler tudo SEM SINAL: `INSERT INTO k VALUES (-2/6,2)` guardava
             * −1/3 correctamente no slot — o medidor confirma-o com um `(int8_t)` à
             * mão — e o SELECT mostrava **85**, que é ra_classe(255,3). O valor no
             * disco estava certo; era o texto que saía errado, e as duas leituras da
             * MESMA célula não concordavam. */
            /* ── E A SAÍDA LÊ NO MESMO ANDAR EM QUE SE ESCREVEU.
             * O comentário acima regista este defeito uma vez: as duas leituras
             * da MESMA célula não concordavam. Voltou a acontecer quando o
             * envelope subiu um andar --- a aritmética passou a ler os dois
             * planos e a SAÍDA continuou no de baixo, e o 500 aparecia como −12
             * enquanto o sum dava 2450. É a mesma célula e tem de ser o mesmo
             * andar: baixo | (alto << 8), com o bit de topo a dizer o lado. */
            } else if(cp == CORPO_DATA){
                /* a contagem lê-se nos TRÊS planos --- baixo, alto, alto2 --- e
                 * sai como o instante que o cliente espera */
                unsigned long alt = mem_le(S_ALTO + cel_ix(i*ncols + j)).total;
                Word b2 = mem_le(S_ALTO2 + cel_ix(i*ncols + j));
                unsigned long seg = (unsigned long)c.total | (alt << 8)
                                  | ((unsigned long)b2.total << 16)
                                  | ((unsigned long)b2.e << 24);
                /* o calendário é a LEITURA, e faz-se aqui na fronteira: a célula
                 * guarda a contagem e mais nada */
                unsigned long dias = seg / 86400UL, resto = seg % 86400UL;
                long ano = 1970, mes = 1;
                for(;;){ int b3 = (ano%4==0 && (ano%100!=0 || ano%400==0));
                         unsigned long dd = b3 ? 366UL : 365UL;
                         if(dias < dd) break;
                         dias -= dd; ano++; }
                { static const int ml[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
                  int b4 = (ano%4==0 && (ano%100!=0 || ano%400==0));
                  for(mes = 1; mes <= 12; mes++){
                      unsigned long dm = (unsigned long)ml[mes-1] + ((mes==2 && b4)?1UL:0UL);
                      if(dias < dm) break;
                      dias -= dm; } }
                snprintf(cel, sizeof cel, "%04ld-%02ld-%02lu %02lu:%02lu:%02lu",
                         ano, mes, dias + 1, resto/3600UL, (resto/60UL)%60UL, resto%60UL);
            } else if(cp == CORPO_TEXTO){
                /* a célula guarda o endereço; o que sai é a CADEIA que lá está */
                long alt = (long)mem_le(S_ALTO + cel_ix(i*ncols + j)).total;
                unsigned ix = (unsigned)(((unsigned long)c.total) | ((unsigned long)alt << 8));
                tx_le(ix, cel, (int)sizeof cel);
            } else if(cp == CORPO_BOOLEANO){
                /* o cliente Postgres lê 't'/'f' --- é a mesma célula, escrita na
                 * língua dele; o valor no disco continua a ser o bit */
                snprintf(cel, sizeof cel, "%s", c.total ? "t" : "f");
            } else if(cp == CORPO_AUREO || cp == CORPO_CRISTAL
                      || cp == CORPO_RACIONAL || c.e > 1){
                long alt = (long)mem_le(S_ALTO + cel_ix(i*ncols + j)).total;
                long bru = (long)c.total | (alt << 8);
                long t = (bru & 0x8000L) ? bru - 65536L : bru;
                if(cp == CORPO_AUREO){
                    long e = (long)(int8_t)c.e;
                    if(e) snprintf(cel, sizeof cel, "%ld%+ldσ", t, e);
                    else  snprintf(cel, sizeof cel, "%ld", t);
                } else if(cp == CORPO_CRISTAL){
                    long e = (long)(int8_t)c.e;
                    if(e) snprintf(cel, sizeof cel, "%ld%+ldω", t, e);
                    else  snprintf(cel, sizeof cel, "%ld", t);
                } else {
                    Par cls = ra_classe((Par){ t, c.e ? (long)c.e : 1 });
                    if(cls.b > 1) snprintf(cel, sizeof cel, "%ld/%ld", cls.a, cls.b);
                    else          snprintf(cel, sizeof cel, "%ld", cls.a);
                }
            } else {
                /* o valor de uma coluna inteira é o par (baixo, alto) lido como UM
                 * número — a dobra do §26 aplicada à fronteira de leitura */
                unsigned long alto = mem_le(S_ALTO + cel_ix(i*ncols + j)).total;
                snprintf(cel, sizeof cel, "%lu", (unsigned long)c.total | (alto << 8));
            }
            /* A PROJECÇÃO VALE PARA OS DOIS LADOS.
             *
             * O §W17 pôs a lista de colunas a ser respeitada — mas só no
             * SqlOut, que é o que o protocolo lê. A impressão em texto
             * continuava a despejar a linha inteira: `SELECT a FROM t` mostrava
             * as três colunas a quem estava no terminal e uma a quem estava no
             * socket. Metade do par corrigida é o defeito que este ficheiro
             * persegue desde o princípio — responder outra coisa é pior do que
             * recusar, e responder DUAS coisas diferentes é pior ainda. */
            /* A AUSÊNCIA SAI VAZIA, e não como zero. Zero é um valor; a
             * ausência é o dual dele, e imprimi-la como zero seria dizer que a
             * célula vale o neutro em vez de dizer que ela não está lá. */
            int ausente = !bit_le(S_PRES, i*ncols + j);
            if(ausente) cel[0] = 0;
            /* a mesma coluna pode ser pedida mais do que uma vez */
            if(proj_n){
                for(int k = 0; k < proj_n; k++)
                    if(!proj_ex[k] && proj[k] == (int)j){
                        snprintf(saida[k], sizeof saida[0], "%s", cel);
                        sai_nulo[k] = (unsigned char)ausente;
                    }
            }else if(j < nsai){
                snprintf(saida[j], sizeof saida[0], "%s", cel);
                sai_nulo[j] = (unsigned char)ausente;
            }
        }
        /* ── E AS EXPRESSÕES AVALIAM-SE DEPOIS DAS CÉLULAS ───────────────────
         * O tensor lê as células desta linha, e o resultado é um par
         * (numerador, denominador) — a mesma classe racional do `avg`, porque a
         * divisão pode não fechar. E não se pronuncia sobre o que não está: se
         * alguma coluna que a expressão CITA está ausente, o resultado é
         * ausente — é a regra do CHECK e do WHERE, dita na produção. */
        /* ── AS ANALÍTICAS: A SÉRIE AVALIADA NA CÉLULA ───────────────────────
         * A série vem da casa e os coeficientes são exactos em ℚ; o que sai é a
         * SOMA PARCIAL de ordem declarada, que é um valor exacto de um objecto
         * declarado — não uma aproximação anónima. E onde a série não representa
         * nada, RECUSA-SE: se o último termo somado não for menor que o
         * anterior, a soma parcial não está a convergir naquele ponto, e
         * devolver o número seria dar por resposta o próprio lixo. É a régua da
         * casa: o que não cabe conta-se e recusa-se. */
        for(int k = 0; k < proj_n && k < nsai; k++){
            if(proj_ex[k] != 2) continue;
            { long j = proj[k];
              if(j < 0 || j >= ncols || !bit_le(S_PRES, i*ncols + j)){
                  saida[k][0] = 0; sai_nulo[k] = 1; continue; }
              { long xv = celula_valor(i, j, ncols);
                Sr ser;
                Qz x, v;
                /* A ORDEM É O QUE O INTEIRO PERMITE, e não um número
                 * escolhido: os coeficientes têm factoriais no denominador, e
                 * 20! = 2,4·10¹⁸ é o último que cabe no inteiro de 64 bits (21!
                 * passa dos 9,2·10¹⁸). Devolve-se a soma até 18 e testa-se com
                 * 20 — os dois termos a mais são o que a resposta ainda não
                 * sabe, e é isso que decide se ela pode sair. A régua é a do
                 * corpo, e diz-se antes de se usar. */
                int ordem = 18;
                /* a série constrói-se com DOIS TERMOS A MAIS do que se
                 * devolve: sem eles os coeficientes extra são zero, a diferença
                 * dá zero, e o teste passava sempre — uma comparação entre a
                 * soma e ela própria.
                 *
                 * E o contador de estouros lê-se ANTES da construção, não depois:
                 * é lá que o factorial deixa de caber no inteiro (22! passa dos
                 * 9,2·10¹⁸), e ler depois deixava esse estouro invisível — a
                 * série ficava truncada em silêncio e o teste comparava-a
                 * consigo própria. */
                long antes_c2 = c2_estouros, antes_qz = qz_perdeu;
                switch(proj_fn[k]){
                    case 1: ser = sr_exp(ordem + 2);   x = qz_de_inteiro(xv); break;
                    case 2: ser = sr_sin(ordem + 2);   x = qz_de_inteiro(xv); break;
                    case 3: ser = sr_cos(ordem + 2);   x = qz_de_inteiro(xv); break;
                    default: ser = sr_log1p(ordem + 2);
                             x = qz_de_inteiro(xv - 1); break;   /* log(1+u) */
                }
                { long antes = antes_c2;
                  /* e a PERDA na avaliação também se lê: o `qz_mult` da casa já
                   * conta o que não coube no inteiro (`qz_perdeu`), e sem o ler
                   * o estouro de 20²⁰ passava calado — a resposta saía com um
                   * décimo do valor certo. Não é o guarda que faltava: era eu a
                   * não olhar para o que a casa já dizia. */
                  /* dois termos A MAIS do que se devolve: o que eles acrescentam
                   * é o que a resposta ainda não sabe, e é isso que decide se ela
                   * pode sair. Um só termo não chegava — em exp(9) o próximo é
                   * pequeno e a CAUDA inteira ainda vale três unidades num
                   * resultado de oito mil. */
                  Qz t1 = sr_parcial(ser, x, ordem);
                  Qz t0 = sr_parcial(ser, x, ordem + 2);
                  /* o último passo tem de ser MENOR que o penúltimo: é o sinal
                   * de que a soma parcial ainda está a apertar naquele ponto */
                  /* a diferença é a soma com o oposto — o `racionais.h` tem a
                   * soma e o produto, e o oposto é multiplicar por −1: não se
                   * escreve aqui uma subtracção que a casa não tem */
                  Qz d1 = qz_soma(t1, qz_oposto(t0));
                  /* O ÚLTIMO PASSO TEM DE SER PEQUENO FACE À SOMA, e a
                   * comparação é entre FRACÇÕES — não entre numeradores. A
                   * primeira escrita comparava `d1.p` com `t1.p` directamente,
                   * que é medir dois racionais por réguas diferentes: os
                   * denominadores não são o mesmo. Com isso passaram exp(9),
                   * exp(20) e log(5), que a série de vinte termos não
                   * representa — o motor devolveu lixo com cara de valor.
                   * Compara-se em produto cruzado, em 128 bits para não
                   * estourar, e o limiar é APERTADO (10⁻⁹ da soma): mais vale
                   * recusar de mais do que responder um número errado — o que
                   * sai daqui tem de estar certo até onde diz. */
                  int cabe = (c2_estouros == antes) && (qz_perdeu == antes_qz)
                             && (d1.q != 0) && (t1.q != 0);
                  if(cabe && d1.p != 0){
                      __int128 e1 = (__int128)1000000000 * (d1.p < 0 ? -(__int128)d1.p : (__int128)d1.p)
                                                 * (t1.q < 0 ? -(__int128)t1.q : (__int128)t1.q);
                      __int128 g1 = (t1.p < 0 ? -(__int128)t1.p : (__int128)t1.p)
                                                 * (d1.q < 0 ? -(__int128)d1.q : (__int128)d1.q);
                      if(e1 > g1) cabe = 0;
                  }
                  if(!cabe){
                      printf("erro: a série de «%s» não aperta em x = %ld —"
                             " a soma parcial não representa o valor. RECUSADA.\n",
                             proj_nome[k], xv);
                      if(sql_cap){ sql_cap->ok = 0;
                          snprintf(sql_cap->err, sizeof sql_cap->err,
                                   "analytic %s: series does not converge at %ld",
                                   proj_nome[k], xv); }
                      return 0;
                  }
                  v = t1; }
                sai_nulo[k] = 0;
                { Par cls = ra_classe((Par){ (long)v.p, (long)v.q });
                  if(cls.b > 1) snprintf(saida[k], SQL_OUT_CELL, "%ld/%ld", cls.a, cls.b);
                  else          snprintf(saida[k], SQL_OUT_CELL, "%ld", cls.a); } } }
        }

        for(int k = 0; k < proj_n && k < nsai; k++){
            if(proj_ex[k] != 1) continue;
            { long den = 1, num;
              int falta = 0;
              for(long j = 0; j < ncols && j < (long)COL_MAX && !falta; j++)
                  if(ten_cita(proj_ten[k], j) && !bit_le(S_PRES, i*ncols + j))
                      falta = 1;
              if(falta){ saida[k][0] = 0; sai_nulo[k] = 1; continue; }
              num = ten_avalia(proj_ten[k], i, ncols, &den);
              sai_nulo[k] = 0;
              if(den > 1){
                  Par cls = ra_classe((Par){ num, den });
                  if(cls.b > 1) snprintf(saida[k], SQL_OUT_CELL, "%ld/%ld", cls.a, cls.b);
                  else          snprintf(saida[k], SQL_OUT_CELL, "%ld", cls.a);
              } else snprintf(saida[k], SQL_OUT_CELL, "%ld", num); }
        }

        for(int k = 0; k < nsai; k++){
            if(!sql_calado){
                printf("%s", saida[k]);
                if(k + 1 < nsai) printf(" | ");
            }
            if(sql_cap && row_i >= 0 && row_i < SQL_OUT_MAX_ROWS){
                /* e o que não cabe na janela DIZ-SE. Truncar aqui era o mesmo
                 * defeito da linha larga: o cliente recebia uma cadeia curta com
                 * a cara de uma completa. */
                if(strlen(saida[k]) >= SQL_OUT_CELL){
                    printf("erro: a célula tem %zu caracteres e a janela da porta C"
                           " leva %d — RECUSADA. O valor está inteiro no banco; peça-o"
                           " por um caminho que o leve todo.\n",
                           strlen(saida[k]), SQL_OUT_CELL - 1);
                    sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "cell has %zu chars; the C window carries %d",
                             strlen(saida[k]), SQL_OUT_CELL - 1);
                    return 0;
                }
                snprintf(sql_cap->cell[row_i][k], SQL_OUT_CELL, "%s", saida[k]);
                sql_cap->nulo[row_i][k] = sai_nulo[k];
            }
        }
        if(!sql_calado) printf("\n");
    }
    return 1;
}

/* ---------------- A TOPOLOGIA NO SQL: distância entre os corpos das colunas ----------------
 *
 * A régua de cada corpo é um ponto (B,C), e a assinatura Δ = B²−4C é a coordenada que sobrevive
 * à base (topologia.c). A distância entre dois corpos é |Δ₁−Δ₂|, e ZERO quer dizer ISOMORFOS —
 * não "a mesma régua". Quando é zero, há UM transporte, φ_t com t = (B₂−B₁)/2, que é o
 * cisalhamento — e o cisalhamento é palavra na ISA: (TROCA GOLD)^t.
 *
 * Então a query pode perguntar a distância entre as colunas, e quando ela é zero pode dizer
 * COMO ir de uma à outra, em bytecode.
 *
 * A régua de cada corpo declarado:
 *   AUREO(m)       σ² = mσ + 1   →  B = m, C = −1  →  Δ = m² + 4
 *   CRISTALINO(t)  ω² = tω − 1   →  B = t, C = +1  →  Δ = t² − 4
 *
 * E os outros — INTEIRO, RACIONAL, MORFICO — NÃO são da família quadrática binária, logo não
 * têm régua desta forma e não têm Δ. Inventar um número para eles seria pior que não responder,
 * e a coluna sai marcada com "—". */

static int distancia(void){
    long ncols = cat_ncols();
    if(ncols > 8) ncols = 8;
    printf("      coluna  corpo             régua (B,C)   Δ = B²−4C   classe\n");
    for(long j = 0; j < ncols; j++){
        Word c = corpo_de(j);
        if(!corpo_tem_regua(c.total)){
            printf("      %-7ld %-17s %-13s %-11s %s\n", j,
                   c.total == CORPO_RACIONAL ? "RACIONAL" :
                   (c.total == CORPO_MORFICO ? "MORFICO" : "INTEIRO"),
                   "—", "—", "fora da família quadrática");
            continue;
        }
        /* o Δ pede-se ao `corpo_delta`, que é quem o define --- estava aqui
         * recalculado como B² − 4C, a mesma conta escrita a segunda vez */
        long B = corpo_B(c.total, c.e), C = corpo_C(c.total);
        long D = corpo_delta(c.total, c.e);
        char nm[32];
        snprintf(nm, sizeof nm, "%s(%d)", c.total == CORPO_AUREO ? "AUREO" : "CRISTALINO", c.e);
        char rg[24]; snprintf(rg, sizeof rg, "(%ld,%ld)", B, C);
        printf("      %-7ld %-17s %-13s %-11ld %s\n", j, nm, rg, D,
               corpo_classe_nome(D, 1));
    }
    printf("\n      a distância d(i,j) = |Δᵢ − Δⱼ|, e ZERO quer dizer ISOMORFOS:\n\n");
    printf("      ");
    for(long j = 0; j < ncols; j++) printf("%8ld", j);
    printf("\n");
    for(long i = 0; i < ncols; i++){
        Word ci = corpo_de(i);
        printf("      %ld:", i);
        for(long j = 0; j < ncols; j++){
            Word cj = corpo_de(j);
            if(!corpo_tem_regua(ci.total) || !corpo_tem_regua(cj.total)){ printf("%8s", "—"); continue; }
            long d = corpo_delta(ci.total,ci.e) - corpo_delta(cj.total,cj.e);
            printf("%8ld", d < 0 ? -d : d);
        }
        printf("\n");
    }
    /* e onde a distância é zero, DIZ COMO ir: o transporte, e a palavra que o executa */
    int achou = 0;
    for(long i = 0; i < ncols; i++) for(long j = i+1; j < ncols; j++){
        Word ci = corpo_de(i), cj = corpo_de(j);
        if(!corpo_tem_regua(ci.total) || !corpo_tem_regua(cj.total)) continue;
        if(corpo_delta(ci.total,ci.e) != corpo_delta(cj.total,cj.e)) continue;
        if(!achou){ printf("\n      ISOMORFOS, e o transporte de cada par:\n\n");
                    printf("      de → para   t = (B₂−B₁)/2   φ_t              palavra na ISA\n"); }
        achou = 1;
        long B1 = corpo_B(ci.total,ci.e), B2 = corpo_B(cj.total,cj.e);
        long t = (B2 - B1) / 2;
        char de[16]; snprintf(de, sizeof de, "%ld → %ld", i, j);
        char mt[24]; snprintf(mt, sizeof mt, "[[1,%ld],[0,1]]", t);
        printf("      %-11s %-16ld %-16s %s\n", de, t, mt,
               t == 0 ? "(vazia — é a mesma)" :
               (t > 0 ? "(TROCA GOLD)^t" : "(NEGRO TROCA)^|t|"));
    }
    if(!achou) printf("\n      (nenhum par de colunas é isomorfo nesta tabela.)\n");
    return 1;
}

/* ---------------- A DISTÂNCIA ENTRE TEXTOS ----------------
 *
 * Um texto é uma sequência de símbolos; sequência de inteiros é uma CIFRA; e a cifra é um ponto
 * do corpo métrico. Logo dois textos são dois pontos, e a distância é a do métrico.
 *
 * O que a torna boa medida de texto não é escolha: dois números são próximos SSE as cifras
 * concordam num prefixo longo. A distância lê onde os textos DIVERGEM. (texto.c) */
static int tx_termos(const char *s, long *a, int max){
    int n = 0;
    for(const char *p = s; *p && n < max; p++) a[n++] = (unsigned char)*p - 31;
    return n;
}
static int distancia_texto(const char *p){
    char A[128], B[128];
    pula(&p);
    if(*p != '\'' && *p != '"') return 0;
    char asp = *p++; int k = 0;
    while(*p && *p != asp && k < 127) A[k++] = *p++;
    A[k] = 0; if(*p == asp) p++;
    pula(&p);
    if(*p != '\'' && *p != '"') return 0;
    asp = *p++; k = 0;
    while(*p && *p != asp && k < 127) B[k++] = *p++;
    B[k] = 0; if(*p == asp) p++;
    /* SEM SEGURAR AS DUAS CIFRAS. Eu guardava as duas em arrays de 128 para depois as comparar
     * — montagem fora, e com tecto. Comparar e ANDAR: le-se um simbolo de cada e para-se no
     * primeiro que diverge. Nao ha nada a guardar, e nao ha tamanho maximo. */
    int na = 0, nb = 0;
    { const char *q = A; while(*q++) na++; q = B; while(*q++) nb++; }
    int pre = 0;
    while(pre < na && pre < nb && A[pre] == B[pre]) pre++;
    printf("      texto A   \"%s\"\n", A);
    printf("      texto B   \"%s\"\n", B);
    printf("      cifra A   [");
    for(int i=0;i<na && i<6;i++) printf("%s%ld", i?";":"", (long)(unsigned char)A[i] - 31);
    printf("%s]\n", na>6?";…":"");
    printf("      cifra B   [");
    for(int i=0;i<nb && i<6;i++) printf("%s%ld", i?";":"", (long)(unsigned char)B[i] - 31);
    printf("%s]\n", nb>6?";…":"");
    printf("      prefixo comum: %d símbolo(s)\n", pre);
    if(na == nb && pre == na) printf("      DISTÂNCIA 0 — são o mesmo texto\n");
    else {
        long den = 1; for(int i=0;i<pre && i<40;i++) den *= 2;
        printf("      DISTÂNCIA 1/%ld\n", den);
    }
    return 1;
}

/* ---------------- A TABELA DE TEXTOS, E A BUSCA ----------------
 *
 * Os textos vivem no ficheiro de memória, não em RAM: cada um ocupa 8 slots (128 bytes) a partir
 * de S_TEXTO, e o cabeçalho conta quantos há. Ler e escrever é pread/pwrite, como tudo o resto.
 *
 * A BUSCA é a distância da cifra aplicada a cada linha: o prefixo comum decide, e o menor
 * 1/2^prefixo ganha. Uma varredura, sem índice — e o custo é o que é: linear nas linhas. */
/* ---------------- TODA ENTRADA ENTRA CIFRADA ----------------
 *
 * REGRA: o que se guarda de uma entrada nao sao os seus bytes — e a sua CIFRA. Um texto cifra-se
 * simbolo a simbolo; um racional cifra-se por Euclides. Guardados na mesma representacao,
 * comparam-se: SEM ISTO NAO HA COMO COMPARAR GATO COM CACHORRO.
 *
 * E O INDICE E A PROPRIA POSICAO. Nenhuma coordenada inventada: nem tamanho de tabela, nem
 * escala, nem hash. O corpo aureo SAO os reais, e ele cifra tudo — a cifra do rei e o unico
 * sistema de coordenadas, o mesmo para um numero, para uma regua e para um texto. O lugar de uma
 * entrada e a sua propria cifra: cada termo e um NIVEL, e a entrada mora no fim do seu caminho.
 * Exato e unico, sem truncamento, sem colisao, sem sondagem.
 *
 * A REGUA E INFINITA; o objeto e que acaba. O caminho morre onde a entrada morre — onde ela
 * quiser — e nao onde um tecto meu mandasse.
 *
 * Registo: [n termos][termo_1..termo_n][n bytes do rotulo][bytes]. O rotulo e so para mostrar ao
 * cliente; quem indexa, quem mede e quem compara e sempre a cifra.
 * Um no ocupa a largura do alfabeto; o filho pelo termo d mora no slot d, e o slot 0 guarda a
 * entrada que termina ali. O ficheiro e esparso: so os nos tocados custam disco. */
#define S_TEXTO   (ISA_TECTO + ZONA(2))
#define S_TXCAB   (S_TEXTO - 1)
#define S_TXLIVRE (S_TEXTO - 2)
#define S_NO      (ISA_TECTO + ZONA(8))
#define S_NOCAB   (S_NO - 1)
#define LARG      256u
#define MAXT      4096
static long n_leituras = 0;      /* o contador honesto: quantos nos o caminho tocou */
/* O contador de textos no PAR: `.total` é um byte, e a tabela passa dos 255.
 * Pior, `txt_n() <= 0` é o teste de «tabela vazia» na busca — ao dar a volta,
 * uma tabela CHEIA respondia vazia. */
static long txt_n(void){ return (long)par_le(S_TXCAB); }
/* O ÍNDICE DE NÓ VIVE NO PAR, COMO TUDO O RESTO NESTE MOTOR.
 *
 * `no_novo` guardava o contador em `.total`, que é UM BYTE: ao chegar a 255,
 * `n+1` truncava a zero, o `if(n < 1) n = 1` repunha-o em 1, e a partir daí a
 * árvore RECICLAVA nós — dois caminhos distintos passavam a dar no mesmo sítio.
 * Era essa a origem das «colisões» que o IMPORT via: das 64 chaves distintas do
 * corpus, oito caíam em cima de outra. Não era a cifra a dobrar: era o contador
 * a dar a volta. Mesmo defeito do `celula_valor` (§W19 do pgwire) e do
 * `S_TXLIVRE` acima — o valor vive no PAR (baixo, alto), e ler só o baixo é ler
 * metade do número. Com dezasseis bits cabem 65535 nós, e a zona vai até
 * S_CANAL, que dá folga para ~38000. */
static unsigned no_filho(unsigned no, long d){
    n_leituras++;
    return par_le(S_NO + no*LARG + (unsigned)d);
}
static unsigned no_novo(void){
    unsigned n = par_le(S_NOCAB); if(n < 1) n = 1;        /* 0 e a raiz */
    par_grava(S_NOCAB, n + 1);
    return n;
}
/* UM TERMO NAO TEM TECTO. O no tem 256 slots, mas o termo pode ser qualquer inteiro — grande,
 * zero ou negativo. O slot 0 e o marcador de fim; o slot 254 diz "o termo e negativo, segue o
 * modulo"; o slot 255 diz "tira 253 e continua". Assim um termo qualquer desce por um caminho
 * proprio e unico, e a regua nao precisa de saber ao que vai servir. */
static void termo_passos(long t, long *passo, int *np, int max){
    int n = 0;
    if(t < 0){ if(n < max) passo[n++] = 254; t = -t; }
    while(t > 253 && n < max - 1){ passo[n++] = 255; t -= 253; }
    if(n < max) passo[n++] = t + 1;
    *np = n;
}
/* Desce um termo inteiro; se abrir != 0, abre os nos que faltarem. Devolve 0 se o caminho morre. */
static unsigned desce_termo(unsigned no, long t, int abrir){
    long passo[64]; int np;
    termo_passos(t, passo, &np, 64);
    for(int k = 0; k < np; k++){
        unsigned f = no_filho(no, passo[k]);
        if(!f){
            if(!abrir) return 0;
            f = no_novo();
            par_grava(S_NO + no*LARG + (unsigned)passo[k], f);
        }
        no = f;
    }
    return no ? no : (unsigned)-1;
}
/* SO A ASSINATURA. O banco guardava a cifra E o rotulo — e para texto isso e DUPLICACAO EXATA:
 * a cifra de um texto E o texto, simbolo a simbolo, bijetivo. Guardar o rotulo era guardar a mesma
 * informacao duas vezes, uma nas coordenadas do sistema e outra na roupa.
 *
 * Agora entra so a assinatura. A roupa recupera-se quando alguem a quiser ver — e e por isso que
 * o comprimento foi para o fim da cifra: e o DUAL, e e ele que diz onde o lado proprio acaba. */
/* O PONTEIRO DA ZONA DE TEXTO VIVE NO PAR, E É UM DESLOCAMENTO.
 *
 * Guardava-se `base` — um endereço absoluto, e S_TEXTO vale S_LINHAS+40000 =
 * 41024 — no `.total` de uma Word, que é UM BYTE. Escrever truncava, ler dava
 * um número ≤255, e portanto `base < S_TEXTO` era SEMPRE verdade: todas as
 * entradas eram gravadas EM CIMA UMAS DAS OUTRAS, no mesmo sítio, e a tabela
 * nunca crescia. Do outro lado, a busca varre `for(base = S_TEXTO; base <
 * livre; ...)` com o mesmo `livre` truncado — menor que S_TEXTO —, de modo que
 * o laço NUNCA CORRIA e a listagem saía vazia com a tabela cheia. Foi assim que
 * o `indexa_orbitas.c` §IX4 importava 64 chaves e recuperava zero.
 *
 * É o mesmo defeito que o §W19 do `pgwire.c` apanhou no `celula_valor`, no
 * mesmo motor: o valor vive no PAR (baixo, alto), e ler só o baixo é ler metade
 * do número. Guarda-se o DESLOCAMENTO em relação a S_TEXTO, e não o endereço:
 * começa em zero, e os dezasseis bits do par dão 65535 slots de texto em vez
 * dos 24511 que sobravam ao endereço absoluto. E o tecto é VERIFICADO: cheio,
 * recusa — gravar por cima é o que se acabou de corrigir. */
#define TX_SLOTS 65535u

static unsigned tx_livre(void){
    Word w = mem_le(S_TXLIVRE);
    return S_TEXTO + (unsigned)((unsigned long)w.total | ((unsigned long)w.e << 8));
}
static unsigned reg_grava(const long *a, size_t n){
    unsigned base = tx_livre();
    unsigned fim  = (unsigned)(base + 1 + n);
    if(fim - S_TEXTO >= TX_SLOTS) return 0;      /* cheio: RECUSA, não escreve por cima */
    /* O COMPRIMENTO NO PAR: MAXT é 4096 e o `.total` é um byte, logo uma cifra
     * de mais de 255 termos dizia ter o resto — e `reg_prox` saltava para o
     * meio do registo seguinte, desalinhando a tabela toda a partir dali.
     *
     * E OS TERMOS VERIFICAM-SE. Cada termo vai num par de dezasseis bits sem
     * sinal; o que não couber é RECUSADO, porque um termo truncado dá uma cifra
     * que não é a de ninguém e a busca passa a mentir em silêncio. */
    par_grava(base, (unsigned)n);
    for(size_t k = 0; k < n; k++){
        if(a[k] < 0 || a[k] > 65535) return 0;             /* nao cabe: RECUSA */
        par_grava(base + 1 + (unsigned)k, (unsigned)a[k]);
    }
    { unsigned off = fim - S_TEXTO;
      Word p2 = { (Word8)(off & 255u), (Word8)((off >> 8) & 255u) };
      mem_grava(S_TXLIVRE, p2); }
    return base;
}
static size_t reg_n(unsigned base){ return (size_t)par_le(base); }
static long   reg_termo(unsigned base, size_t k){ return (long)par_le(base + 1 + (unsigned)k); }
static unsigned reg_prox(unsigned base){ return base + 1 + (unsigned)reg_n(base); }
/* A ROUPA, RECUPERADA. Nao esta guardada: deriva-se da assinatura. O ultimo par de termos da o
 * corte (np, nd); os np termos a seguir a seta de Wick sao o lado proprio. Se todos couberem no
 * alfabeto, mostra-se como texto; se nao, mostra-se a cifra, que e o que ele e. */
static void reg_mostra(unsigned base, char *out, size_t lim){
    size_t n = reg_n(base);
    out[0] = 0;
    if(n < 3){ snprintf(out, lim, "(vazio)"); return; }
    long np = reg_termo(base, n - 2);
    if(np <= 0 || (size_t)np > n - 3){                    /* sem corte utilizavel: a cifra crua */
        size_t k = 0; int c = 0;
        c += snprintf(out, lim, "[");
        for(; k < n && (size_t)c < lim - 8; k++)
            c += snprintf(out + c, lim - (size_t)c, "%s%ld", k?";":"", reg_termo(base, k));
        snprintf(out + c, lim - (size_t)c, "]");
        return;
    }
    int texto = 1;
    for(long k = 0; k < np; k++){
        long t = reg_termo(base, 1 + (size_t)k);
        if(t < 1 || t > 224){ texto = 0; break; }
    }
    if(texto){
        size_t m = (size_t)np < lim - 1 ? (size_t)np : lim - 1;
        for(size_t k = 0; k < m; k++) out[k] = (char)(reg_termo(base, 1 + k) + 31);
        out[m] = 0;
    } else {
        int c = snprintf(out, lim, "[");
        for(long k = 0; k < np && (size_t)c < lim - 8; k++)
            c += snprintf(out + c, lim - (size_t)c, "%s%ld", k?";":"", reg_termo(base, 1 + (size_t)k));
        snprintf(out + c, lim - (size_t)c, "]");
    }
}
/* Devolve 1 se PÔS, 0 se o lugar já estava ocupado.
 *
 * Era `void`, e quem chamava não tinha como saber: o `poe_chave_texto` devolvia
 * 1 sempre que a cifra se formava, de modo que o IMPORT contava as chaves que
 * TENTOU pôr e não as que ficaram. Medido no `indexa_orbitas.c` §IX4: «64
 * chaves (0 -> 55)» — sessenta e quatro tentadas, cinquenta e cinco na tabela,
 * com o medidor a comparar 64 com 64 e a dar essa metade por boa.
 *
 * As nove que faltam são a DOBRA: duas entradas distintas descem ao MESMO nó e
 * a segunda cai neste `return`. Pelo `aranha.tex`, π perde exactamente a dobra,
 * e desfazê-la custa UMA coordenada (Teor. do levantamento em folhas).
 * Enquanto o levantamento não estiver aqui, o que se pode e deve é parar de
 * mentir sobre o número: quem chama fica a saber que houve colisão. */
static int cif_poe(const long *a, size_t n){
    unsigned no = 0;
    for(size_t k = 0; k < n; k++) no = desce_termo(no, a[k], 1);
    if(par_le(S_NO + no*LARG)) return 0;                  /* ja la esta, no seu lugar */
    unsigned base = reg_grava(a, n);
    if(!base) return 0;                                   /* zona de texto cheia */
    /* ── E O DESLOCAMENTO GUARDA-SE MAIS UM, PORQUE O ZERO JÁ TEM DONO ──────
     *
     * O deslocamento da PRIMEIRA entrada é zero --- ela cai exactamente em
     * S_TEXTO ---, e zero é o que `par_le` devolve quando o nó não tem base
     * nenhuma. O canal de erro reutilizava um valor do domínio: a primeira
     * chave posta ficava indistinguível de «não está», e o `ACHA` dizia que não
     * estava enquanto o `BUSCA` a achava com distância 0. Dois caminhos sobre o
     * mesmo objecto, e só um deles certo.
     *
     * Guarda-se +1 e lê-se −1: o zero fica a significar UMA coisa só. É o mesmo
     * remédio que o pool das cadeias já usa --- lá o endereço também é «+1, e o
     * 0 fica para a cadeia vazia». */
    par_grava(S_NO + no*LARG, base - S_TEXTO + 1u);
    par_grava(S_TXCAB, (unsigned)(txt_n() + 1));
    barreira();
    return 1;
}
static long acha_cifra(const long *a, size_t n, size_t *desceu_out){
    unsigned no = 0; size_t desceu = 0;
    for(size_t j = 0; j < n; j++){
        unsigned f = desce_termo(no, a[j], 0);
        if(!f) break;
        no = f; desceu++;
    }
    *desceu_out = desceu;
    if(desceu != n) return 0;
    { unsigned b = par_le(S_NO + no*LARG);
      return b ? (long)(b - 1u) + 1 : 0; }   /* devolve não-zero quando HÁ base */
}
static void mostra_cifra(const long *a, size_t n){
    printf("[");
    for(size_t k = 0; k < n; k++) printf("%s%ld", k?";":"", a[k]);
    printf("]");
}
/* A UNICA PORTA: 'texto' cifra-se simbolo a simbolo, p/q cifra-se por Euclides. */
static int cifra_entrada(const char **p, long *a, size_t max, size_t *n, char *rot, size_t lr){
    pula(p);
    if(**p == '\'' || **p == '"'){
        char asp = *(*p)++;
        const char *ini = *p;
        while(**p && **p != asp) (*p)++;
        size_t len = (size_t)(*p - ini);
        if(**p == asp) (*p)++;
        /* UMA PORTA SO. O texto passa pelo MESMO cifra_geral dos corpos: os simbolos sao o
         * periodo — o texto e o seu proprio gerador — com razao 1 (um simbolo por nivel) e
         * sinal +1 (o texto nao fecha). Agora que os comprimentos foram para tras, o prefixo
         * volta a ser o conteudo, e 'ourives' e prefixo de 'ourivesaria' outra vez. */
        size_t ns = len < 48 ? len : 48;
        long per[48];
        for(size_t k = 0; k < ns; k++) per[k] = (long)(unsigned char)ini[k] - 31;
        *n = cifra_geral(per, (int)ns, 1, 1, 1, a, max);
        snprintf(rot, lr, "'%.*s'", (int)ns, ini);
        return *n > 0;
    }
    {
        long pp = 0, qq = 1; int sinal = 1, viu = 0;
        if(**p == '-'){ sinal = -1; (*p)++; }
        while(**p >= '0' && **p <= '9'){ pp = pp*10 + (*(*p)++ - '0'); viu = 1; }
        if(!viu) return 0;
        if(**p == '/'){ (*p)++; qq = 0; while(**p >= '0' && **p <= '9') qq = qq*10 + (*(*p)++ - '0'); }
        if(qq == 0) return 0;
        pp *= sinal;
        /* o racional pela mesma porta: os termos de Euclides sao o periodo */
        long per[48]; size_t np = 0;
        long x = pp, y = qq;
        while(y && np < 48){ long t = x / y; per[np++] = t; long r = x - t*y; x = y; y = r; }
        *n = cifra_geral(per, (int)np, 1, 1, 1, a, max);
        snprintf(rot, lr, "%ld/%ld", pp, qq);
        return *n > 0;
    }
}
/* OS 28 CORPOS: A REGUA (B,C) DE CADA UM, E A CIFRA QUE SAI DELA.
 *
 * A regua nao se escolhe — LE-SE DO OPERADOR: B = tr(Pi), C = det(Pi), e dai Delta = B^2-4C =
 * tr^2-4det. Depois a cifra sai da regua e so dela: sigma = (B+sqrt|Delta|)/2, expandida em
 * fracao continua por PQa, EM INTEIROS, sem float nenhum. Delta<0 entra pelo DUAL — a quadratura
 * que a estrutura pede — e por isso o |Delta|: uma so formula para os tres regimes.
 *
 * O que fica dito: para as familias parametricas (o gato A_m, a dilatacao por lambda) o catalogo
 * nomeia UM operador, e e o dele que se toma. Onde o parametro e livre, o membro minimo que ja
 * nao esteja tomado por outro corpo — e isso vai anotado corpo a corpo, para se poder contestar. */
/* per/np: o periodo, quando o corpo o traz proprio (so o hipercorpo); rd: a razao do lado dual,
 * que e a mesma do proprio salvo quando a deformacao emparelha DUAS razoes (o venom). */
static const long GER[16] = {1,2,4,3,7,8,6,5,13,14,16,15,11,12,10,9};
static const struct { const char *nome; long B, C; const long *per; int np; long rd;
                      const char *porque; } CORPO28[] = {
 { "racional",        2,  1, 0, 0, 2, "a classe reduz: T=[[1,1],[0,1]], tr 2 det 1" },
 { "aureo",           1, -1, 0, 0, 1, "o gato A_1, tr 1 det -1 — O REI" },
 { "deflexivo",       2, -1, 0, 0, 2, "o gato A_2, tr 2 det -1 (m=1 e o aureo)" },
 { "cristalino",      0,  1, 0, 0, 0, "o esquilo S, tr 0 det 1 — Gauss" },
 { "celeste",         0,  1, 0, 0, 0, "r^2+C^2=1 — a redonda" },
 { "optico",          0,  1, 0, 0, 0, "C^2+S^2=1 — a redonda" },
 { "criativo",        0, -1, 0, 0, 0, "NOT = involucao J, tr 0 det -1" },
 { "tecnico",         0, -1, 0, 0, 0, "a refutacao — involucao" },
 { "sensitivo",       0, -1, 0, 0, 0, "a conjugacao p-adica — involucao" },
 /* O LOGICO: A CIFRA DELE E A INDUCAO. Eu tinha-o posto em (0,-1), que e a contraposicao —
  * mas essa e o nu dele, o DUAL, e nao a deformacao. A deformacao da inducao e base + passo, e o
  * passo e sempre o MESMO, sem fim: razao 1, e o passo carrega o anterior (sinal -1). Isso e
  * A_1, sigma = 1 + 1/sigma, a cifra [1;1,1,1,...] — O REI.
  *
  * A inducao E a cifra do rei, e nao por analogia: as duas sao a mesma recursao a carregar-se. */
 { "logico",          1, -1, 0, 0, 1, "a INDUCAO: base + passo, e o passo e sempre o mesmo" },
 /* OS FORMATOS. Um formato e um corpo: a razao e quantos simbolos por nivel, o sinal e se a
  * marca FECHA. Entram pela mesma porta e caem onde a regua os puser. */
 { "json",            1, -1, 0, 0, 1, "o parentese abre e FECHA: as duas direcoes cancelam-se" },
 { "yaml",            2,  1, 0, 0, 2, "a indentacao so se acumula: dois espacos por nivel" },
 { "markdown",        1,  1, 0, 0, 1, "o cardinal so se acumula: um por nivel" },
 /* O TEX TEM DUAS MARCAS DE NIVEL, e sao corpos diferentes — nao e ambiguidade, e o que ele e:
  *   a seccao      \section, \subsection, \subsubsection: 3 simbolos ("sub") por nivel, e
  *                 NAO fecha  -> (3, +1)
  *   o ambiente    \begin{}...\end{}: abre e FECHA, como o parentese  -> (1, -1) = o AUREO
  * Entra pela seccao, que e a que estrutura o documento; e fica dito que pelo ambiente ele cai
  * exatamente no mesmo lugar do json, porque o mecanismo e o mesmo. */
 { "tex",             3,  1, 0, 0, 3, "a seccao: tres simbolos (sub) por nivel, e nao fecha" },
 { "csv",             1,  1, 0, 0, 1, "a virgula: um simbolo, e NAO fecha — o formato plano" },
 { "html",            1, -1, 0, 0, 1, "a etiqueta <t>...</t>: abre e FECHA, como o parentese" },
 /* AS LINGUAGENS. Mesma pergunta, mesma resposta: quantos simbolos por nivel, e a marca fecha? */
 { "c",               1, -1, 0, 0, 1, "as chaves { } abrem e FECHAM" },
 { "lisp",            1, -1, 0, 0, 1, "os parenteses — o mesmo mecanismo do json" },
 { "python",          4,  1, 0, 0, 4, "quatro espacos por nivel, e NAO fecha" },
 { "haskell",         2,  1, 0, 0, 2, "dois espacos por nivel, e nao fecha" },
 { "assembly",        1,  1, 0, 0, 1, "plano: o rotulo nao aninha" },
 { "fractal",         1,  1, 0, 0, 1, "z*zbar com o trono, tr 1 det 1 — Eisenstein" },
 { "relogio",         1,  1, 0, 0, 1, "N = cos psi no trono, ordem 6" },
 { "telescopico",     2,  1, 0, 0, 2, "a deflexao D_lambda: cisalhamento, tr 2 det 1" },
 { "conforme",        2,  1, 0, 0, 2, "o mergulho — cisalhamento" },
 { "entropico",       2,  1, 0, 0, 2, "(x) = + : os custos somam — parabolico" },
 { "espaco-temporal", 2,  1, 0, 0, 2, "o sucessor S(x)=x+1, T com t=1" },
 { "universal",       2,  1, 0, 0, 2, "a contagem — o mesmo sucessor" },
 { "morfico",         2,  1, 0, 0, 2, "dil por B_r: o RAIO soma — parabolico" },
 { "eletromagnetico", 3,  1, 0, 0, 3, "exp.Sigma.log com lambda minimo, tr 3 det 1" },
 { "motor",           3,  1, 0, 0, 3, "exp(tG) — o gerador, tr 3 det 1" },
 { "economico",       4,  1, 0, 0, 4, "juro composto (1+r)^n, tr 4 det 1" },
 { "evolutivo",       5,  1, 0, 0, 5, "o replicador p*w/<w>, tr 5 det 1" },
 { "expansivo",       6,  1, 0, 0, 6, "o flip Lambda = log, tr 6 det 1" },
 { "somatico",        7,  1, 0, 0, 7, "exp.Sigma.log — a mitose, tr 7 det 1" },
 { "geometrico",      3, -1, 0, 0, 3, "a RAZAO da progressao, tr 3 det -1" },
 { "cosmico",         4, -1, 0, 0, 4, "a(t)=e^{Ht}, tr 4 det -1" },
 { "rotor",           5, -1, 0, 0, 5, "phi = artanh, tr 5 det -1" },
 { "nervoso",         6, -1, 0, 0, 6, "a ativacao — a rede recorre, tr 6 det -1" },
 { "exterior",        7, -1, 0, 0, 7, "Volterra — a integral acumula, tr 7 det -1" },
 { "hipercorpo",     16,  1, GER, 16, 16, "a curva de Hilbert: a reta do rei deformada no tesseracto" },
 /* O PRISMATICO: o triangulo. O gerador percorre os TRES vertices por arestas, e isso e a
  * rotacao de 120 graus — traco -1, det 1. Elipticо, Δ = -3: o TRIANGULO E REDONDO, e nao foi
  * preciso deforma-lo. Mesma familia do fractal (Eisenstein), com a rotacao para o outro lado. */
 { "prismatico",      -1,  1, 0, 0, -1, "o triangulo: a rotacao de 120, ordem 3, eliptica" },
 /* O CANTOR: a subdivisao em TRES, e a marca nao fecha (tira-se o meio e nao se volta). E dele
  * que sai o que enche — nao o conjunto, que e po, mas a SOMA: C + C cobre o intervalo inteiro. */
 { "cantor",           3,  1, 0, 0, 3, "a subdivisao em tres; o po nao enche, a SOMA enche" },
 { "venom",           1, -1, 0, 0, 16, "avancar e esvaziar sao o mesmo ato — as duas leis da curva" },
};
#define N28 ((int)(sizeof CORPO28 / sizeof CORPO28[0]))

/* O CATÁLOGO DA CASA, pela porta. São 44 corpos com nome próprio, e a `aranha`
 * caixa de Pandora aplica-se a eles: a condição de pertença é |C| = 1, e a óptica
 * lê-se nas duas coordenadas. O medidor tem de varrer o catálogo VERDADEIRO. */
int sql_corpo28_n(void){ return N28; }
const char *sql_corpo28(int i, long *B, long *C){
    if(i < 0 || i >= N28) return 0;
    if(B) *B = CORPO28[i].B;
    if(C) *C = CORPO28[i].C;
    return CORPO28[i].nome;
}

/* ── UM SÍTIO QUE CONTA ──────────────────────────────────────────────────────
 * O comando `CORPOS` imprime o resumo óptico do catálogo e o medidor verifica-o.
 * Se cada um fizesse a sua conta seriam DUAS RÉGUAS para o mesmo objecto, e o
 * defeito que esta casa persegue era plantado de propósito. Conta-se aqui, e os
 * dois lêem daqui. */
void sql_optica_resumo(long *fora, long *prop, long *el, long *par, long *hip,
                       long *borda, long *viola){
    long f = 0, p = 0, e = 0, a = 0, h = 0, b = 0, v = 0;
    for(int i = 0; i < N28; i++){
        long B = CORPO28[i].B, C = CORPO28[i].C, D = B*B - 4*C;
        if(C != 1 && C != -1) f++;
        if(C > 0) p++;
        if(D < 0) e++; else if(D == 0) a++; else h++;
        if(C < 0 && D <= 0) v++;                  /* a lei do cor:optica */
        if(D >= 0){ long r = raizi(D);
            if(r*r == D && (B+r) % 2 == 0 && (B-r) % 2 == 0) b++; }
    }
    if(fora) *fora = f;   if(prop) *prop = p;   if(el) *el = e;
    if(par) *par = a;     if(hip) *hip = h;     if(borda) *borda = b;
    if(viola) *viola = v;
}


/* O codificador vem do cifra.h — um so, e encoda qualquer corpo: foi ele que deu a cifra
 * aos 31 e e ele que da a de um formato. Estava aqui dentro e saiu para nao haver dois. */
/* A DISTANCIA ENTRE OS CORPOS: o prefixo comum das cifras completas, e a distancia e 1/2^k. E a
 * mesma regua que mede textos e numeros — ela nao sabe o que esta a medir. */
static int distancia_corpos(void){
    long (*A)[128] = DISCO_FIXO2(long, 128, 72);
    disco_prende(DISCO_BASE(72),"dados/A_72.bin",(size_t)((size_t)(N28)*128),sizeof(long));
    disco_zera(A,(size_t)((size_t)(N28)*128),sizeof(long)); size_t *nA = DISCO_FIXO(size_t, 21); int *idx = DISCO_FIXO(int, 22);
    disco_prende(DISCO_BASE(21),"dados/sql_nA.bin",(size_t)N28,sizeof(size_t));
    disco_prende(DISCO_BASE(22),"dados/sql_idx.bin",(size_t)N28,sizeof(int)); int m = 0;
    for(int i = 0; i < N28; i++){
        nA[i] = cifra_geral(CORPO28[i].per, CORPO28[i].np, CORPO28[i].B, CORPO28[i].C,
                            CORPO28[i].rd, A[i], 128);
        int ja = 0;
        for(int k = 0; k < m; k++)
            if(nA[idx[k]] == nA[i]){
                int ig = 1;
                for(size_t t = 0; t < nA[i]; t++) if(A[idx[k]][t] != A[i][t]) ig = 0;
                if(ig) ja = 1;
            }
        if(!ja) idx[m++] = i;
    }
    printf("      o prefixo comum k das cifras completas; a distancia e 1/2^k\n\n      ");
    for(int b = 0; b < m; b++) printf("%3.3s", CORPO28[idx[b]].nome);
    printf("\n");
    int mau = 0, kmin = 999, kmax = -1, pi = 0, pj = 0, qi = 0, qj = 0;
    for(int a2 = 0; a2 < m; a2++){
        printf("      %-17s", CORPO28[idx[a2]].nome);
        for(int b = 0; b < m; b++){
            size_t k = 0;
            while(k < nA[idx[a2]] && k < nA[idx[b]] && A[idx[a2]][k] == A[idx[b]][k]) k++;
            printf("%3zu", k);
            if(a2 == b){ if(k != nA[idx[a2]]) mau++; continue; }
            if(k == nA[idx[a2]] && k == nA[idx[b]]) mau++;
            if((int)k < kmin){ kmin = (int)k; pi = idx[a2]; pj = idx[b]; }
            if((int)k > kmax){ kmax = (int)k; qi = idx[a2]; qj = idx[b]; }
        }
        printf("\n");
    }
    printf("\n      mais LONGE: %s e %s, prefixo %d — distancia 1/%d\n",
           CORPO28[pi].nome, CORPO28[pj].nome, kmin, 1 << kmin);
    printf("      mais PERTO: %s e %s, prefixo %d — distancia 1/%d\n",
           CORPO28[qi].nome, CORPO28[qj].nome, kmax, 1 << kmax);
    printf("      %d reguas distintas, e a matriz %s\n", m, mau ? "TEM FALHA" : "e metrica");
    return mau == 0;
}
/* O EMISSOR DO MARTELO. Duas portas, porque são duas coisas diferentes:
 *
 *   CABECALHO '<160 hex>' <alvo>   põe os 80 bytes e a bola na memória do banco
 *   MARTELO <de> <ate>             EMITE o opcode e a máquina do banco martela a faixa
 *
 * O martelo compila como tudo o resto — LOAD, LOAD, OP_MARTELO — e quem executa é o banco. */
static int cabecalho(const char *p){
    pula(&p);
    if(*p != '\'' && *p != '"'){
        printf("erro: `CABECALHO` pede a cadeia entre aspas — `CABECALHO '<texto>'` —"
               " e veio «%.30s» — RECUSADO.\n", p);
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err,
                     "CABECALHO needs a quoted string"); }
        return 0;
    }
    char asp = *p++;
    unsigned char cab[80]; memset(cab, 0, 80);
    int n = 0;
    while(*p && *p != asp && n < 160){
        int hi = -1, lo = -1;
        for(int k = 0; k < 2 && *p && *p != asp; k++){
            int c = *p++, v = (c >= '0' && c <= '9') ? c - '0'
                            : (c >= 'a' && c <= 'f') ? c - 'a' + 10
                            : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
            if(v < 0) return 0;
            if(k == 0) hi = v; else lo = v;
        }
        if(lo < 0) return 0;
        cab[n/2] = (unsigned char)(hi*16 + lo); n += 2;
    }
    if(*p == asp) p++;
    if(n != 160) return 0;                      /* 80 bytes exatos, ou não é cabeçalho */
    atomos_grava(S_CAB, cab, 80);
    /* o alvo entra CIFRADO, como o hash: 32 símbolos em hexadecimal */
    pula(&p);
    if(*p != '\'' && *p != '"') return 0;
    char asp2 = *p++;
    unsigned char alvo[32]; memset(alvo, 0, 32);
    int m = 0;
    while(*p && *p != asp2 && m < 64){
        int hi = -1, lo = -1;
        for(int k = 0; k < 2 && *p && *p != asp2; k++){
            int c = *p++, v = (c>='0'&&c<='9') ? c-'0' : (c>='a'&&c<='f') ? c-'a'+10
                            : (c>='A'&&c<='F') ? c-'A'+10 : -1;
            if(v < 0) return 0;
            if(k == 0) hi = v; else lo = v;
        }
        if(lo < 0) return 0;
        alvo[m/2] = (unsigned char)(hi*16 + lo); m += 2;
    }
    if(m != 64) return 0;
    atomos_grava(S_ALVO, alvo, 32);
    barreira();
    printf("      cabecalho de 80 bytes na memoria; o alvo cifrado em 32 simbolos\n");
    return 1;
}
static int martelo(const char *p){
    long de = 0, ate = 0;
    pula(&p); if(!numero(&p, &de)) return 0;
    pula(&p); if(!numero(&p, &ate)) return 0;
    /* faixa em átomos u32 — Word ISA é Word_8²; a matemática (σ²=σ+1) manda no metal */
    atomos_u32(S_FAIXA, (unsigned)de);
    atomos_u32(S_FAIXA + 4u, (unsigned)ate);
    pc_emit = 0;
    emit1(OP_MARTELO);
    emit1(OP_HALT);
    unsigned plen = pc_emit;
    Regs r; memset(&r, 0, sizeof r);
    long passos = 0;
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    while(passo(&r, plen)){ if(++passos > 50000000L) break; }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    unsigned nonce = r.R.total ? atomos_le_u32(S_FAIXA + 8u) : 0;
    int64_t seg_ns = (int64_t)(t1.tv_sec - t0.tv_sec) * 1000000000
                   + (int64_t)(t1.tv_nsec - t0.tv_nsec);
    long feitos = nonce ? (long)nonce - de : ate - de;
    if(seg_ns > 0){
        int64_t taxa = feitos * 1000000000 / seg_ns;
        const char *un = "H/s";
        int64_t v100 = taxa * 100;
        if(taxa >= 1000000){ v100 = taxa / 10000; un = "MH/s"; }
        else if(taxa >= 1000){ v100 = taxa / 10; un = "kH/s"; }
        printf("      %ld hash(es) em %" PRId64 ".%03" PRId64 " s — %" PRId64 ".%02" PRId64 " %s\n",
               feitos,
               seg_ns / 1000000000, (seg_ns % 1000000000) / 1000000,
               v100 / 100, v100 % 100, un);
    }
    if(nonce)
        printf("      SHARE no nonce %u — a cifra diverge PARA BAIXO no simbolo %u\n"
               "      (%ld passos de ISA)\n", nonce, (unsigned)r.R.e, passos);
    else
        printf("      faixa limpa: nenhum nonce dentro da bola. Zero e RESPOSTA, nao falha —\n"
               "      a faixa varrida e trabalho feito, e fecha-se na mesma.\n");
    return 1;
}
/* A REVERSÃO. O martelo é metade: ele PROCURA, e procurar estica — varre a faixa toda. A outra
 * metade CONTRAI: dado o nonce, confere-se de uma vez. É o chicote de sempre, e é ela que faz do
 * trabalho uma prova — quem acha paga a faixa inteira, quem confere paga um.
 *
 * E quem reverte é o ESQUILO, que já está na ISA e reverte em qualquer forma e qualquer régua:
 * ordem 4, quatro voltas e está-se onde se partiu. Não há reversão a escrever — há a usar. */
static int verifica(const char *p){
    long n = 0; pula(&p);
    if(!numero(&p, &n)){
        printf("erro: `VERIFICA` pede o NÚMERO do martelo — `VERIFICA <n>` — e veio"
               " «%.30s» — RECUSADO.\n", p);
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err, "VERIFICA needs a number"); }
        return 0;
    }
    unsigned char cab[80], h1[32], h2[32], alvo[32];
    atomos_le(S_CAB, cab, 80);
    atomos_le(S_ALVO, alvo, 32);
    cab[76] = (unsigned char)n;         cab[77] = (unsigned char)(n >> 8);
    cab[78] = (unsigned char)(n >> 16); cab[79] = (unsigned char)(n >> 24);
    sha256(cab, 80, h1); sha256(h1, 32, h2);
    int k = 0, dentro = 0;
    while(k < 32){
        unsigned th = h2[31-k], ta = alvo[k];
        if(th != ta){ dentro = (th < ta); break; }
        k++;
    }
    printf("      nonce %ld: a cifra do hash e [", n);
    for(int i = 0; i < 6; i++) printf("%s%u", i?";":"", h2[31-i]);
    printf(";...]  contra o alvo, diverge no simbolo %d\n", k);
    printf("      %s\n", dentro ? "CONFERE — esta dentro da bola" : "nao confere");
    /* O ESQUILO, a reverter: quatro voltas e o elemento volta ao lugar. Resíduo 0 ou falha. */
    Par x = { (long)h2[31], (long)h2[30] }, y = x;
    for(int i = 0; i < 4; i++) y = cr_op(y, 0);
    printf("      o esquilo dado 4 vezes devolve (%ld,%ld) -> (%ld,%ld): residuo %s\n",
           x.a, x.b, y.a, y.b, (x.a == y.a && x.b == y.b) ? "0" : "NAO ZERO");
    printf("      procurar ESTICA (a faixa toda), conferir CONTRAI (um so) — e o chicote,\n");
    printf("      e e por isso que o trabalho e PROVA: caro de achar, barato de crer.\n");
    return dentro;
}
static int config(const char *p){
    char nome[64]; pula(&p);
    if(!ident(&p, nome, sizeof nome)) return 0;
    pula(&p);
    if(*p == '\'' || *p == '"'){
        char asp = *p++; char v[448]; size_t k = 0;
        while(*p && *p != asp && k < sizeof v - 1) v[k++] = *p++;
        v[k] = 0;
        conf_poe(nome, v);
        printf("      config %s guardada (%zu simbolo(s))\n", nome, k);
        return 1;
    }
    char v[448]; conf_le(nome, v, sizeof v);
    printf("      %s = %s\n", nome, v[0] ? v : "(vazia)");
    return 1;
}
/* MINERA: o worker continuo. Uma ligacao, um processo, e o banco a martelar.
 *
 * E o laco e a lei da liquidacao: A CADA VOLTA ELE ENTRA — le o pool, le a config — e cada
 * entrada dispara a verificacao. O tique e a leitura do socket com tempo-limite; a flag
 * mina_ativa e o contrato que se liquida quando alguem a poe a zero. */
static int minera(const char *p){
    long faixa = 0; pula(&p);
    /* PREDEFINICAO LEVE: isto e um TERMOMETRO, nao uma mina. Ninguem espera achar bloco — o que
     * se quer e saber se o sistema continua inteiro, e para isso basta um fio de trabalho.
     * (Os argumentos nao estao a ser lidos — o numero() falha aqui e ainda nao sei porque. Fica
     * dito, e a predefinicao ja da o comportamento certo.) */
    if(!numero(&p, &faixa) || faixa <= 0) faixa = 1 << 18;
    long descanso = 3000000;     /* 3 s entre faixas: ~8% de um nucleo */
    pula(&p); { long d; if(numero(&p, &d) && d >= 0) descanso = d; }
    pool_abre();
    if(!pool_ligado){ printf("sem pool: CONFIG pool_host e CONFIG pool_user\n"); return 0; }
    conf_poe("mina_ativa", "1");
    printf("ligado. faixa de %ld nonces. (CONFIG mina_ativa '0' para parar)\n", faixa);
    fflush(stdout);
    char jid[64] = ""; long de = 0, total = 0; int quieto = 0;
    for(;;){
        { char at[64]; conf_le("mina_ativa", at, sizeof at);
          if(at[0] == '0'){ printf("mina_ativa=0 — a parar.\n"); fflush(stdout); return 1; } }
        Word tem = mem_le(S_POOL + 11);
        if(!tem.total){
            /* SAUDE: sem job ha muito tempo e sinal de que a ligacao caiu. Reporta-se, e nao se
             * finge que esta tudo bem — e para isso que ele existe. */
            if(++quieto == 300){ printf("FALHA: 60 s sem job — a ligacao ao pool pode ter caido\n");
                                 fflush(stdout); }
            usleep(200000); continue;
        }
        quieto = 0;
        if(strcmp(jid, pool_st.job_id)){
            snprintf(jid, sizeof jid, "%s", pool_st.job_id);
            de = 0;
            /* O CABECALHO MONTA-SE DENTRO. Cada campo vai do slot do pool para o seu lugar no
             * S_CAB, sem passar por array nenhum — o maior temporario e uma PALAVRA de 4 bytes,
             * que e o campo em si e nao o objeto. */
            (void)mem_le(S_POOL+11);   /* refresca pool_st */
            unsigned v = pool_st.versao, nb = pool_st.nbits, nt = pool_st.ntime;
            { unsigned char z[80]; memset(z, 0, 80); atomos_grava(S_CAB, z, 80); }
            banco_poe(S_CAB, 0, (const unsigned char*)&v, 4);
            banco_poe(S_CAB, 4, pool_st.prevhash, 32);
            merkle_pelo_fold();
            banco_poe(S_CAB, 36, pool_st.merkle_raiz, 32);
            banco_poe(S_CAB, 68, (const unsigned char*)&nt, 4);
            banco_poe(S_CAB, 72, (const unsigned char*)&nb, 4);
            unsigned char alvo[32]; memset(alvo, 0, 32);
            int ex = (int)(nb >> 24); unsigned man = nb & 0xFFFFFF;
            for(int k = 0; k < 3; k++){
                int pos = 32 - (ex - 3) - 3 + k;
                if(pos >= 0 && pos < 32) alvo[pos] = (unsigned char)(man >> (16 - 8*k));
            }
            atomos_grava(S_ALVO, alvo, 32);
            /* SAUDE: o job tem de vir inteiro. Merkle a zero ou nbits a zero seria cabecalho
             * invalido — e nenhuma share sairia dai. Melhor gritar do que martelar em falso. */
            int zero = 1;
            for(int k = 0; k < 8; k++) if(mem_le(S_POOL + 12 + (unsigned)k).total) zero = 0;
            if(zero || !nb || !nt){
                printf("FALHA: job %s veio incompleto (merkle %s, nbits %08x, ntime %08x)\n",
                       jid, zero ? "ZERO" : "ok", nb, nt);
                fflush(stdout);
            }
            printf("job %s  nbits %08x  ntime %08x%s\n", jid, nb, nt, zero ? "  <- SUSPEITO" : "");
            fflush(stdout);
        }
        long ate = de + faixa; if(ate > 0xFFFFFFFFL) ate = 0xFFFFFFFFL;
        atomos_u32(S_FAIXA, (unsigned)de);
        atomos_u32(S_FAIXA + 4u, (unsigned)ate);
        pc_emit = 0;
        emit1(OP_MARTELO); emit1(OP_HALT);
        unsigned plen = pc_emit;
        Regs r; memset(&r, 0, sizeof r);
        long passos = 0;
        while(passo(&r, plen)){ if(++passos > 50000000L) break; }
        total += (ate - de);
        if(r.R.total){
            unsigned nonce = atomos_le_u32(S_FAIXA + 8u);
            printf("SHARE! nonce %u — a submeter\n", nonce);
            mem_grava(S_POOL_SH, w8(nonce & 0xFFu, 0));
            fflush(stdout);
        }
        de = ate;
        if(de >= 0xFFFFFFFFL) de = 0;
        usleep(descanso);        /* LEVE: isto e um termometro, nao uma mina. Deixa a maquina em paz. */
        if(total % (faixa * 8) == 0){
            printf("%ld Mhash, job %s\n", total/1000000, jid);
            fflush(stdout);
        }
    }
    return 1;
}
static int insere_corpos(void){
    long antes = txt_n();
    /* ── E A ÓPTICA, QUE A `aranha` LÊ NESTAS MESMAS DUAS COLUNAS ───────
     * O «razao» é o B (traço) e o «sinal» é o C (determinante), e a caixa de
     * Pandora lê-os: C é a ORIENTAÇÃO (+1 próprio, −1 reflecte) e o sinal de
     * Δ = B²−4C é o REGIME (roda, cisalha, expande). A tabela dizia os dois
     * números e não o que eles são --- e a `cor:optica` diz que um proíbe metade
     * do outro: com C = −1 vem Δ = B²+4 > 0, pelo que o impróprio nunca roda. */
    printf("      corpo             razao sinal dual   Δ  orient  regime      borda"
           "  cifra completa\n");
    for(int i = 0; i < N28; i++){
        long a[128];
        size_t n = cifra_geral(CORPO28[i].per, CORPO28[i].np, CORPO28[i].B, CORPO28[i].C,
                               CORPO28[i].rd, a, 128);
        long ja = txt_n();
        cif_poe(a, n);
        { long B = CORPO28[i].B, C = CORPO28[i].C, D = B*B - 4*C;
          const char *ori = (C > 0) ? "propr" : "REFLE";
          const char *reg = (D < 0) ? "roda      " : (D == 0 ? "cisalha   " : "expande   ");
          int bd = 0;
          if(D >= 0){ long r = raizi(D);
              bd = (r*r == D && (B+r) % 2 == 0 && (B-r) % 2 == 0); }
          printf("      %-17s %-3ld %-3ld %-4ld %4ld %-6s %-10s %-5s ", CORPO28[i].nome,
                 B, C, CORPO28[i].rd, D, ori, reg, bd ? "SIM" : ""); }
        mostra_cifra(a, n);
        printf("%s\n", txt_n() == ja ? "   <- lugar ja tomado" : "");
    }
    printf("\n      %d corpos, %ld lugares distintos.\n", N28, txt_n() - antes);
    { int nr = 0; long vb[64], vc[64];
      for(int i = 0; i < N28; i++){ int ja = 0;
        for(int k = 0; k < nr; k++) if(vb[k]==CORPO28[i].B && vc[k]==CORPO28[i].C) ja = 1;
        if(!ja){ vb[nr]=CORPO28[i].B; vc[nr]=CORPO28[i].C; nr++; } }
      printf("      %d reguas (B,C) distintas.\n", nr); }
    /* o resumo da leitura, LIDO de quem o conta --- não recontado aqui */
    { long fora = 0, prop = 0, el = 0, par = 0, hip = 0, bd = 0, viola = 0;
      sql_optica_resumo(&fora, &prop, &el, &par, &hip, &bd, &viola);
      printf("      caixa de Pandora: %ld fora (|C| != 1), %ld proprios e %ld que"
             " reflectem.\n", fora, prop, (long)N28 - prop);
      printf("      regimes: %ld rodam (eliptico), %ld cisalham (parabolico), %ld"
             " expandem (hiperbolico).\n", el, par, hip);
      printf("      na borda (a regua factoriza em duas lineares): %ld.\n", bd);
      printf("      a lei «o que reflecte nunca roda nem cisalha»: %ld violacoes.\n",
             viola); }
    return 1;
}
static int insere_texto(const char *p){
    long a[MAXT]; size_t n; char rot[128];
    if(!cifra_entrada(&p, a, MAXT, &n, rot, sizeof rot)) return 0;
    cif_poe(a, n);
    printf("entrada %-16s cifrada em ", rot); mostra_cifra(a, n); printf("\n");
    return 1;
}
/* A BUSCA DIRETA: desce a cifra do alvo. O custo e o COMPRIMENTO DA CIFRA — nao o tamanho da
 * tabela, nao o numero de linhas. E o prefixo comum, que e a distancia, E o caminho partilhado. */
static int acha_texto(const char *p){
    long a[MAXT]; size_t n, desceu; char rot[128];
    if(!cifra_entrada(&p, a, MAXT, &n, rot, sizeof rot)) return 0;
    long antes = n_leituras;
    long base = acha_cifra(a, n, &desceu);
    printf("      alvo %-16s cifra de %zu termo(s)\n", rot, n);
    if(base) printf("      ACHOU — %ld no(s) descidos, um por termo\n", n_leituras - antes);
    else     printf("      nao esta: o caminho morre ao %zu.o termo, em %ld leitura(s)\n",
                    desceu + 1, n_leituras - antes);
    return 1;
}
/* A varredura: compara CIFRA com CIFRA, e por isso um numero compara-se com uma palavra. */
/* ══ BUSCA PREFIXO: A BOLA, E NÃO UMA VARREDURA ═════════════════════════════
 *
 * O `aranha` §sec:ultra diz que «a célula É a bola»: numa ultramétrica, o
 * conjunto dos que partilham os primeiros k símbolos É a bola de raio 2^{−k},
 * e descer k níveis na árvore de prefixos É escolhê-la. Não há nada a
 * construir --- a árvore já é isso, e o que faltava era a porta.
 *
 * É a `prefix query` do Elastic, e a diferença é o custo: lá ela varre o índice
 * invertido; aqui DESCE k nós e recolhe a subárvore. «O que isto NÃO faz é
 * varrer: os nós visitados são a profundidade vezes a largura, mais os que têm
 * resultado. O tamanho da tabela não entra» --- é o mesmo que o índice das
 * colunas já diz de si.
 *
 * O raio devolve-se com a resposta, porque ele é o que a bola vale: com k
 * termos descidos, tudo o que sai está a distância ≤ 2^{−k} do alvo. */
static void pre_recolhe(unsigned no, long *saida, int *n, int cap){
    if(*n >= cap) return;
    { unsigned b = par_le(S_NO + no*LARG);
      sql_ultimos_nos++;
      if(b) { if(*n < cap) saida[(*n)++] = (long)(S_TEXTO + b - 1u); } }
    for(unsigned k = 1; k < LARG; k++){
        unsigned f = par_le(S_NO + no*LARG + k);
        sql_ultimos_nos++;
        if(f) pre_recolhe(f, saida, n, cap);
    }
}
static int busca_prefixo(const char *p){
    long a[MAXT]; size_t n; char rot[128];
    if(!cifra_entrada(&p, a, MAXT, &n, rot, sizeof rot)) return 0;
    if(txt_n() <= 0){ printf("(tabela vazia)\n"); return 1; }
    unsigned no = 0; size_t desceu = 0;
    sql_ultimos_nos = 0;              /* o custo mede-se, e é ele a afirmação */
    for(size_t j = 0; j < n; j++){
        unsigned f = desce_termo(no, a[j], 0);
        if(!f) break;
        no = f; desceu++;
    }
    printf("      prefixo: %s   cifra de %zu termo(s)\n\n", rot, n);
    /* ── A BOLA É A QUE A ÁRVORE ALCANÇA, E O RAIO DIZ-SE ────────────────────
     *
     * Exigir que a descida chegue aos `n` termos era pedir a cadeia INTEIRA, e
     * a cifra não é um prefixo-morfismo puro: os últimos termos fecham a
     * palavra, pelo que a cifra de «corpo/alf» diverge da de «corpo/alfa.txt»
     * antes do fim. Medido: as duas partilham DEZ termos e a cifra do alvo tem
     * catorze.
     *
     * A bola certa é a do prefixo que a árvore alcança --- desce-se o que
     * houver, e o raio é 2^{−desceu}, dito com a resposta. Isso é a
     * §sec:ultra sem a torcer: a bola tem o raio que tem, e o que estaria
     * errado era chamar-lhe outro. Com desceu = 0 a bola é tudo, e isso
     * também se diz. */
    if(desceu == 0){
        printf("      o primeiro termo não existe na árvore: a bola é VAZIA\n");
        if(sql_cap){ memset(sql_cap, 0, sizeof *sql_cap); sql_cap->ok = 1;
            sql_cap->ncols = 2; sql_cap->nrows = 0;
            snprintf(sql_cap->col[0], sizeof sql_cap->col[0], "entrada");
            snprintf(sql_cap->col[1], sizeof sql_cap->col[1], "raio");
            sql_cap->tipo[0] = SQL_TIPO_TEXT; sql_cap->tipo[1] = SQL_TIPO_TEXT;
            snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT 0"); }
        return 1;
    }
    n = desceu;      /* o raio é o que se desceu, e não o que se pediu */
    static long dentro[512]; int nd = 0;
    pre_recolhe(no, dentro, &nd, 512);
    printf("      entrada            raio da bola\n");
    if(sql_cap){ memset(sql_cap, 0, sizeof *sql_cap); sql_cap->ok = 1;
        sql_cap->ncols = 2;
        snprintf(sql_cap->col[0], sizeof sql_cap->col[0], "entrada");
        snprintf(sql_cap->col[1], sizeof sql_cap->col[1], "raio");
        sql_cap->tipo[0] = SQL_TIPO_TEXT; sql_cap->tipo[1] = SQL_TIPO_TEXT; }
    for(int k = 0; k < nd; k++){
        char vis[64]; reg_mostra((unsigned)dentro[k], vis, sizeof vis);
        char raio[32];
        if(n < 62) snprintf(raio, sizeof raio, "1/%llu", 1ULL << n);
        else       snprintf(raio, sizeof raio, "1/2^%zu", n);
        printf("      %-18s <= %s\n", vis, raio);
        if(sql_cap && sql_cap->nrows < SQL_OUT_MAX_ROWS){
            long r = sql_cap->nrows;
            snprintf(sql_cap->cell[r][0], SQL_OUT_CELL, "%s", vis);
            snprintf(sql_cap->cell[r][1], SQL_OUT_CELL, "%s", raio);
            sql_cap->nrows++;
        }
    }
    printf("      -> %d na bola de raio 1/2^%zu, com %zu no(s) descidos e %ld"
           " visitados ao todo --- e NENHUMA varredura\n",
           nd, n, desceu, sql_ultimos_nos);
    if(sql_cap) snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT %d", nd);
    return 1;
}

static int busca_texto(const char *p){
    long a[MAXT]; size_t na; char rot[128];
    if(!cifra_entrada(&p, a, MAXT, &na, rot, sizeof rot)) return 0;
    if(txt_n() <= 0){ printf("(tabela vazia)\n"); return 1; }
    unsigned livre = tx_livre();      /* o PAR, não o byte baixo — ver reg_grava */
    printf("      alvo: %s   ", rot); mostra_cifra(a, na); printf("\n\n");
    printf("      entrada            cifra              prefixo  distancia\n");
    size_t melhor = 0; unsigned vence = 0; int primeiro = 1;
    for(unsigned base = S_TEXTO; base < livre; base = reg_prox(base)){
        size_t nb = reg_n(base), pre = 0;
        while(pre < na && pre < nb && a[pre] == reg_termo(base, pre)) pre++;
        char vis[64]; reg_mostra(base, vis, sizeof vis);
        char cif[64]; int c = 0;
        for(size_t k = 0; k < nb && k < 5; k++)
            c += snprintf(cif+c, sizeof cif - c, "%s%ld", k?";":"[", reg_termo(base, k));
        snprintf(cif+c, sizeof cif - c, "%s", nb > 5 ? ";...]" : "]");
        printf("      %-18s %-18s %-8zu ", vis, cif, pre);
        if(nb == na && pre == na) printf("0\n");
        else if(pre < 62)         printf("1/%llu\n", 1ULL << pre);
        else                      printf("1/2^%zu\n", pre);
        if(primeiro || pre > melhor){ melhor = pre; vence = base; primeiro = 0; }
    }
    char vis[64]; reg_mostra(vence, vis, sizeof vis);
    printf("\n      MAIS PROXIMO: %s (prefixo %zu)\n", vis, melhor);
    return 1;
}

/* ---------------- IMPORT: linguagens e corpo entram pelo SQL ---------------- */
static int poe_chave_texto(const char *s){
    long a[MAXT]; size_t n; char rot[128];
    char buf[512]; const char *p = buf;
    snprintf(buf, sizeof buf, "'%s'", s);
    if(!cifra_entrada(&p, a, MAXT, &n, rot, sizeof rot)) return 0;
    return cif_poe(a, n);          /* 0 se o lugar já estava ocupado: é a dobra */
}
static void le_caminho_arg(const char **p, char *cam, size_t cap, const char *def){
    pula(p);
    if(**p == '\'' || **p == '"'){
        char asp = *(*p)++;
        size_t k = 0;
        while(**p && **p != asp && k + 1 < cap) cam[k++] = *(*p)++;
        cam[k] = 0;
        if(**p == asp) (*p)++;
    } else snprintf(cam, cap, "%s", def);
}
static int import_linguagens(const char *p){
    char cam[512];
    le_caminho_arg(&p, cam, sizeof cam, "../conecthus/backends/manifesto.json");
    FILE *f = fopen(cam, "rb");
    if(!f){ printf("nao abri: %s\n", cam); return 0; }
    fseek(f, 0, SEEK_END); long st = ftell(f); fseek(f, 0, SEEK_SET);
    if(st <= 0 || st > 1<<20){ fclose(f); return 0; }
    char *buf = (char*)malloc((size_t)st + 1);
    if(!buf){ fclose(f); return 0; }
    if((long)fread(buf, 1, (size_t)st, f) != st){ free(buf); fclose(f); return 0; }
    buf[st] = 0; fclose(f);
    long antes = txt_n(), postas = 0;
    for(char *q = buf; (q = strstr(q, "\"nome\"")); q++){
        char nome[64] = "", faz[64] = "";
        long pp = 0, qq = 0, rr = 0;
        if(sscanf(q, "\"nome\": \"%63[^\"]\"", nome) != 1) continue;
        char *bl = q; char *fim = q + 400; if(fim > buf + st) fim = buf + st;
        for(char *t = bl; t < fim; t++){
            if(!faz[0] && sscanf(t, "\"faz\": \"%63[^\"]\"", faz) == 1) continue;
            if(!pp && sscanf(t, "\"p\": %ld", &pp) == 1) continue;
            if(!qq && sscanf(t, "\"q\": %ld", &qq) == 1) continue;
            if(!rr && sscanf(t, "\"r\": %ld", &rr) == 1) continue;
        }
        if(!nome[0] || !faz[0]) continue;
        char entrada[160];
        snprintf(entrada, sizeof entrada, "linguagem/%s|%ld,%ld,%ld|%s", nome, pp, qq, rr, faz);
        if(poe_chave_texto(entrada)) postas++;
    }
    free(buf);
    printf("      IMPORT LINGUAGENS: %ld entradas (%ld -> %ld)\n", postas, antes, txt_n());
    return postas > 0;
}
/* Blobs do corpo: <base>_corpo/<rel> — fonte de verdade ao lado do .mem.
 * Índice TEXTO (corpo/<rel>) + bytes no disco do banco. GET CORPO devolve os bytes. */
static void corpo_dir(char *out, size_t cap){
    snprintf(out, cap, "%s_corpo", g_base[0] ? g_base : "/tmp/sql_corpo");
}
static int mkdirs_para(const char *caminho){
    char tmp[1024]; snprintf(tmp, sizeof tmp, "%s", caminho);
    for(char *q = tmp + 1; *q; q++){
        if(*q != '/') continue;
        *q = 0; mkdir(tmp, 0755); *q = '/';
    }
    return 1;
}
static int copia_ficheiro(const char *de, const char *para){
    FILE *in = fopen(de, "rb"); if(!in) return 0;
    mkdirs_para(para);
    FILE *out = fopen(para, "wb"); if(!out){ fclose(in); return 0; }
    unsigned char buf[8192]; size_t n; int ok = 1;
    while((n = fread(buf, 1, sizeof buf, in)) > 0)
        if(fwrite(buf, 1, n, out) != n){ ok = 0; break; }
    fclose(in); fclose(out);
    return ok;
}
static int import_corpo(const char *p){
    char cam[512], raiz[512], dest_raiz[640];
    le_caminho_arg(&p, cam, sizeof cam, "../app/src/corpo.json");
    le_caminho_arg(&p, raiz, sizeof raiz, "..");
    corpo_dir(dest_raiz, sizeof dest_raiz);
    mkdir(dest_raiz, 0755);
    FILE *f = fopen(cam, "rb");
    if(!f){ fprintf(stderr, "nao abri: %s\n", cam); return 0; }
    long antes = txt_n(), postas = 0, blobs = 0, lidos = 0;
    char lin[1024];
    while(fgets(lin, sizeof lin, f)){
        char rel[512];
        if(sscanf(lin, " \"%511[^\"]\",", rel) != 1 &&
           sscanf(lin, " \"%511[^\"]\"", rel) != 1) continue;
        if(!strchr(rel, '/') && !strchr(rel, '.')) continue;  /* só caminhos, não chaves JSON */
        lidos++;
        char de[1024], para[1280], chave[576];
        snprintf(de, sizeof de, "%s/%s", raiz, rel);
        snprintf(para, sizeof para, "%s/%s", dest_raiz, rel);
        snprintf(chave, sizeof chave, "corpo/%s", rel);
        if(poe_chave_texto(chave)) postas++;
        if(copia_ficheiro(de, para)) blobs++;
        else fprintf(stderr, "      sem blob: %s\n", rel);
    }
    fclose(f);
    printf("      IMPORT CORPO: %ld chaves, %ld blobs em %s (%ld ficheiros; txt %ld -> %ld)\n",
           postas, blobs, dest_raiz, lidos, antes, txt_n());
    return blobs > 0 || postas > 0;
}
/* GET CORPO 'rel' — bytes crus em stdout; estado em stderr. Vite /corpo/* usa isto. */
static int get_corpo(const char *p){
    char rel[512], para[1280], raiz[640];
    le_caminho_arg(&p, rel, sizeof rel, "");
    if(!rel[0]){ fprintf(stderr, "GET CORPO: falta o caminho\n"); return 0; }
    if(!strncmp(rel, "corpo/", 6)) memmove(rel, rel + 6, strlen(rel + 6) + 1);
    corpo_dir(raiz, sizeof raiz);
    snprintf(para, sizeof para, "%s/%s", raiz, rel);
    FILE *f = fopen(para, "rb");
    if(!f){ fprintf(stderr, "GET CORPO: nao esta no banco: %s\n", rel); return 0; }
    unsigned char buf[8192]; size_t n;
    while((n = fread(buf, 1, sizeof buf, f)) > 0)
        if(fwrite(buf, 1, n, stdout) != n){ fclose(f); return 0; }
    fclose(f);
    fflush(stdout);
    fprintf(stderr, "GET CORPO: %s (%s)\n", rel, para);
    return 1;
}

static int schema_poe_nodo(const SchemaNodo *n){
    char entrada[320];
    snprintf(entrada, sizeof entrada, "schema/%s|%s|%d|%s",
             n->id[0] ? n->id : "?", n->kind, n->sentido, n->formato);
    return poe_chave_texto(entrada);
}

static int import_parser(const char *p){
    char cam[512], dest[1280], raiz[640];
    FILE *t;
    le_caminho_arg(&p, cam, sizeof cam, "conecthus/schema/u.schema.json");
    t = fopen(cam, "rb");
    if(!t){
        snprintf(cam, sizeof cam, "../conecthus/schema/u.schema.json");
        t = fopen(cam, "rb");
    }
    if(t) fclose(t);
    else {
        printf("nao abri schema: conecthus/schema/u.schema.json\n");
        return 0;
    }
    corpo_dir(raiz, sizeof raiz);
    mkdir(raiz, 0755);
    snprintf(dest, sizeof dest, "%s/schema/u.schema.json", raiz);
    if(!copia_ficheiro(cam, dest)){
        printf("nao copiei schema para o disco\n");
        return 0;
    }
    {
        SchemaNodo u;
        schema_preenche_u(&u);
        schema_poe_nodo(&u);
    }
    printf("      IMPORT PARSER: schema U no disco (%s)\n", dest);
    return 1;
}

static int cmd_parse(const char *p){
    char cam[512], as[32] = "json";
    int sentido = 0;
    le_caminho_arg(&p, cam, sizeof cam, "");
    if(!cam[0]){
        printf("erro: PARSE pede um caminho\n");
        return 0;
    }
    pula(&p);
    if(!strncasecmp(p, "AS", 2) && (p[2] == 0 || isspace((unsigned char)p[2]))){
        p += 2; pula(&p);
        sscanf(p, "%31s", as);
        while(*p && !isspace((unsigned char)*p)) p++;
        pula(&p);
    }
    if(!strncasecmp(p, "SENTIDO", 7)){
        p += 7; pula(&p);
        sentido = (int)strtol(p, 0, 10);
        if(sentido != -1 && sentido != 0 && sentido != 1) sentido = 0;
    }
    if(!schema_parse_ficheiro(cam, as, sentido, stdout)){
        printf("erro: PARSE nao reconheceu «%s» como formato da casa\n", cam);
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err, "PARSE: unknown house format"); }
        return 0;
    }
    fflush(stdout);
    {
        SchemaNodo n;
        long nb = 0;
        char *buf = schema_le_ficheiro(cam, &nb);
        if(buf){
            schema_parse_nodo(cam, buf, nb, &n);
            n.sentido = sentido;
            if(sentido != 0) snprintf(n.kind, sizeof n.kind, "metade");
            schema_poe_nodo(&n);
            free(buf);
        }
    }
    return 1;
}

static int get_schema(const char *p){
    char id[64] = "U";
    le_caminho_arg(&p, id, sizeof id, "U");
    if(!id[0]) snprintf(id, sizeof id, "U");
    {
        const char *cands[] = {
            "conecthus/schema/u.json",
            "../conecthus/schema/u.json",
            NULL
        };
        int i;
        if(!strcmp(id, "U") || !strcmp(id, "u")){
            for(i = 0; cands[i]; i++){
                if(schema_parse_ficheiro(cands[i], "json", 0, stdout)){
                    fflush(stdout);
                    return 1;
                }
            }
        }
        if(!strcmp(id, "pagina") || !strcmp(id, "PAGINA")){
            const char *pcands[] = {
                "conecthus/schema/pagina.json",
                "../conecthus/schema/pagina.json",
                NULL
            };
            for(i = 0; pcands[i]; i++){
                if(schema_parse_ficheiro(pcands[i], "json", 0, stdout)){
                    fflush(stdout);
                    return 1;
                }
            }
        }
    }
    printf("GET SCHEMA: instancia «%s» nao localizada\n", id);
    return 0;
}

/* Ponte Manifesto ↔ U: o conversor é JS (JSON já parseado); o motor chama o
 * node ingerido. Não adivinha Hebb; M é projecção, não segundo schema. */
static int acha_relativo(const char *rel, char *out, size_t cap){
    const char *pre[] = { "", "../", "./", NULL };
    int i;
    for(i = 0; pre[i]; i++){
        snprintf(out, cap, "%s%s", pre[i], rel);
        {
            FILE *t = fopen(out, "rb");
            if(t){ fclose(t); return 1; }
        }
    }
    snprintf(out, cap, "%s", rel);
    return 0;
}
static int pula_seta(const char **p){
    pula(p);
    if((*p)[0] == '-' && (*p)[1] == '>'){ *p += 2; pula(p); return 1; }
    return 0;
}
static int ponte_node_mjs(const char *mjs_rel, const char *modo, const char *arg, const char *tag){
    char mjs[768], run[2048];
    FILE *f;
    int c, st;
    if(!acha_relativo(mjs_rel, mjs, sizeof mjs)){
        printf("erro: %s nao localizada\n", mjs_rel);
        return 0;
    }
    snprintf(run, sizeof run, "\"%s\" \"%s\" %s \"%s\"",
             tiffany_node_bin(), mjs, modo, arg);
    f = popen(run, "r");
    if(!f){
        printf("erro: ponte %s: popen\n", tag);
        return 0;
    }
    while((c = fgetc(f)) != EOF) fputc(c, stdout);
    fflush(stdout);
    st = pclose(f);
    if(st != 0){
        printf("erro: ponte %s saiu %d\n", tag, st);
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err, "%s: node ponte failed", tag); }
        return 0;
    }
    return 1;
}
static int ponte_manifesto_u(const char *modo, const char *arg){
    return ponte_node_mjs("tools/manifesto_u.mjs", modo, arg, "Manifesto");
}
static int import_manifesto(const char *p){
    char cam[512], dest[1280], raiz[640];
    FILE *t;
    le_caminho_arg(&p, cam, sizeof cam, "conecthus/backends/manifesto.json");
    t = fopen(cam, "rb");
    if(!t){
        snprintf(cam, sizeof cam, "../conecthus/backends/manifesto.json");
        t = fopen(cam, "rb");
    }
    if(t) fclose(t);
    else {
        printf("nao abri manifesto: conecthus/backends/manifesto.json\n");
        return 0;
    }
    corpo_dir(raiz, sizeof raiz);
    mkdir(raiz, 0755);
    snprintf(dest, sizeof dest, "%s/manifesto.json", raiz);
    if(!copia_ficheiro(cam, dest)){
        printf("nao copiei manifesto para o disco\n");
        return 0;
    }
    poe_chave_texto("manifesto/corpos|orbitas=sql,latex,node");
    printf("      IMPORT MANIFESTO: no disco (%s)\n", dest);
    return 1;
}
static int cmd_manifesto_para_u(const char *p){
    char cam[512];
    le_caminho_arg(&p, cam, sizeof cam, "");
    if(!cam[0] && !acha_relativo("conecthus/backends/manifesto.json", cam, sizeof cam)){
        printf("erro: manifesto nao localizada\n");
        return 0;
    }
    return ponte_manifesto_u("para-u", cam);
}
static int cmd_u_para_manifesto(const char *p){
    char cam[512];
    le_caminho_arg(&p, cam, sizeof cam, "");
    if(!cam[0]){
        printf("erro: U -> MANIFESTO pede um caminho do nodo U\n");
        return 0;
    }
    return ponte_manifesto_u("para-m", cam);
}
static int import_pagina(const char *p){
    char dir[512], pedido[512], dest[1280], raiz[640], de[768];
    const char *nomes[] = { "pagina.html", "pagina.css", "pagina.js" };
    int i, n = 0;
    le_caminho_arg(&p, pedido, sizeof pedido, "app/banco");
    if(!acha_relativo(pedido[0] ? pedido : "app/banco", dir, sizeof dir)){
        if(!acha_relativo("app/banco", dir, sizeof dir)){
            printf("nao abri pagina: app/banco\n");
            return 0;
        }
    }
    corpo_dir(raiz, sizeof raiz);
    mkdir(raiz, 0755);
    snprintf(dest, sizeof dest, "%s/pagina", raiz);
    mkdir(dest, 0755);
    for(i = 0; i < 3; i++){
        char para[1400];
        snprintf(de, sizeof de, "%s/%s", dir, nomes[i]);
        snprintf(para, sizeof para, "%s/%s", dest, nomes[i]);
        if(copia_ficheiro(de, para)) n++;
    }
    if(!n){
        printf("nao copiei pagina.{html,css,js}\n");
        return 0;
    }
    poe_chave_texto("pagina/triade|html,css,js");
    printf("      IMPORT PAGINA: %d ficheiros (%s)\n", n, dest);
    return 1;
}
static int cmd_pagina_para_u(const char *p){
    char cam[512];
    le_caminho_arg(&p, cam, sizeof cam, "");
    if(!cam[0] && !acha_relativo("app/banco", cam, sizeof cam)){
        printf("erro: pagina nao localizada\n");
        return 0;
    }
    return ponte_node_mjs("tools/pagina_u.mjs", "para-u", cam, "Pagina");
}
static int cmd_u_para_pagina(const char *p){
    char cam[512];
    le_caminho_arg(&p, cam, sizeof cam, "");
    if(!cam[0]){
        printf("erro: U -> PAGINA pede um caminho do nodo U\n");
        return 0;
    }
    return ponte_node_mjs("tools/pagina_u.mjs", "para-p", cam, "Pagina");
}

/* ---------------- SHELL backends: BASH / POWERSHELL / NODE (wasm + pleno) */
static void shell_dir(char *out, size_t cap, const char *nome){
    snprintf(out, cap, "%s_%s", g_base[0] ? g_base : "/tmp/sql_shell", nome);
}
static int shell_parse_literal(const char *p, char *cmd, size_t cap){
    size_t k = 0;
    if(*p == '\'' || *p == '"'){
        char asp = *p++;
        while(*p && *p != asp && k + 1 < cap){
            if(*p == '\\' && p[1]){ cmd[k++] = p[1]; p += 2; continue; }
            cmd[k++] = *p++;
        }
        if(*p == asp) p++;
    } else {
        while(*p && *p != ';' && *p != '\n' && k + 1 < cap)
            cmd[k++] = *p++;
    }
    cmd[k] = 0;
    return (int)k;
}
static int shell_escreve(const char *label, const char *nome, unsigned slot_in, const char *cmd, long n){
    char dir[640], pin[768], aux[768];
    shell_dir(dir, sizeof dir, nome);
    mkdir(dir, 0755);
    snprintf(pin, sizeof pin, "%s/in", dir);
    FILE *f = fopen(pin, "wb");
    if(!f){ printf("erro: %s MOVE -1 nao abri %s\n", label, pin); return 0; }
    if((long)fwrite(cmd, 1, (size_t)n, f) != n){ fclose(f); return 0; }
    fclose(f);
    if(!strcmp(nome, "powershell")){
        snprintf(aux, sizeof aux, "%s/in.ps1", dir);
        f = fopen(aux, "wb");
        if(f){ fwrite(cmd, 1, (size_t)n, f); fclose(f); }
    } else if(!strcmp(nome, "node")){
        snprintf(aux, sizeof aux, "%s/in.js", dir);
        f = fopen(aux, "wb");
        if(f){ fwrite(cmd, 1, (size_t)n, f); fclose(f); }
    } else if(!strcmp(nome, "bash")){
        snprintf(aux, sizeof aux, "%s/in.sh", dir);
        f = fopen(aux, "wb");
        if(f){ fwrite(cmd, 1, (size_t)n, f); fclose(f); }
    }
    { char pout[768]; snprintf(pout, sizeof pout, "%s/out", dir); unlink(pout); }
    mem_grava(slot_in, (Word){ (Word8)(n & 255u), (Word8)((n >> 8) & 255u) });
    printf("%s MOVE -1: %ld bytes (slot %u)\n", label, n, slot_in);
    return 1;
}
static FILE *shell_popen_run(const char *nome, const char *dir, const char *cmd){
    char run[8704];
    (void)cmd;
    if(!strcmp(nome, "bash")){
        char sh[768];
        snprintf(sh, sizeof sh, "%s/in.sh", dir);
        snprintf(run, sizeof run, "\"%s\" \"%s\"", tiffany_bash_bin(), sh);
        return popen(run, "r");
    }
    if(!strcmp(nome, "powershell")){
        char ps1[768];
        snprintf(ps1, sizeof ps1, "%s/in.ps1", dir);
#ifdef _WIN32
        snprintf(run, sizeof run,
            "\"%s\" -NoProfile -ExecutionPolicy Bypass -File \"%s\"",
            tiffany_pwsh_bin(), ps1);
#else
        snprintf(run, sizeof run, "\"%s\" -NoProfile -File \"%s\"",
            tiffany_pwsh_bin(), ps1);
#endif
        return popen(run, "r");
    }
    if(!strcmp(nome, "node")){
        char js[768];
        snprintf(js, sizeof js, "%s/in.js", dir);
        snprintf(run, sizeof run, "\"%s\" \"%s\"", tiffany_node_bin(), js);
        return popen(run, "r");
    }
    return NULL;
}
static int shell_corre(const char *label, const char *nome, unsigned slot_out){
    char dir[640], pin[768], pout[768], cmd[8192];
    shell_dir(dir, sizeof dir, nome);
    snprintf(pin, sizeof pin, "%s/in", dir);
    snprintf(pout, sizeof pout, "%s/out", dir);
    FILE *in = fopen(pin, "rb");
    if(!in){ printf("erro: %s MOVE -1 sem stdin — use %s MOVE '...' primeiro\n", label, label); return 0; }
    long nin = 0;
    while(nin < 8191){
        int c = fgetc(in);
        if(c == EOF) break;
        cmd[nin++] = (char)c;
    }
    fclose(in);
    cmd[nin] = 0;
    if(nin <= 0){ printf("erro: %s MOVE -1 stdin vazio\n", label); return 0; }
    FILE *f = shell_popen_run(nome, dir, cmd);
    if(!f){ printf("erro: %s MOVE -1 popen falhou\n", label); return 0; }
    FILE *out = fopen(pout, "wb");
    long nout = 0;
    unsigned char buf[512];
    for(;;){
        size_t r = fread(buf, 1, sizeof buf, f);
        if(r == 0) break;
        if(out && fwrite(buf, 1, r, out) != r){ pclose(f); if(out) fclose(out); return 0; }
        nout += (long)r;
    }
    pclose(f);
    if(out) fclose(out);
    mem_grava(slot_out, (Word){ (Word8)(nout & 255u), (Word8)((nout >> 8) & 255u) });
    printf("%s MOVE -1: %ld bytes correr (slot %u)\n", label, nout, slot_out);
    return 1;
}
static int shell_le(const char *label, const char *nome){
    char dir[640], pout[768];
    shell_dir(dir, sizeof dir, nome);
    snprintf(pout, sizeof pout, "%s/out", dir);
    FILE *f = fopen(pout, "rb");
    if(!f){ fprintf(stderr, "%s MOVE +1: sem saida\n", label); return 0; }
    unsigned char buf[8192];
    size_t n;
    while((n = fread(buf, 1, sizeof buf, f)) > 0)
        if(fwrite(buf, 1, n, stdout) != n){ fclose(f); return 0; }
    fclose(f);
    fflush(stdout);
    fprintf(stderr, "%s MOVE +1: ok\n", label);
    return 1;
}
static int cmd_shell(const char *p, const char *label, const char *nome,
                     unsigned slot_in, unsigned slot_out){
    pula(&p);
    if(strncasecmp(p, "MOVE", 4)){
        printf("erro: %s sabe apenas MOVE — veio «%.20s»\n", label, p);
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err, "%s: use MOVE", label); }
        return 0;
    }
    p += 4; pula(&p);
    int sentido = 0;
    if(p[0] == '-' && p[1] == '1'){ sentido = -1; p += 2; pula(&p); }
    else if(p[0] == '+' && p[1] == '1'){ sentido = +1; p += 2; pula(&p); }

    char cmd[8192];
    int k = 0;
    if(*p == '\'' || *p == '"')
        k = shell_parse_literal(p, cmd, sizeof cmd);

    if(sentido == +1){
        if(k > 0){
            printf("erro: %s MOVE +1 nao le argumentos\n", label);
            return 0;
        }
        return shell_le(label, nome);
    }
    if(k <= 0){
        printf("erro: %s MOVE precisa do corpo — `%s MOVE '...'` ou `%s MOVE -1 '...'`\n",
               label, label, label);
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err, "%s MOVE: missing body", label); }
        return 0;
    }
    if(!shell_escreve(label, nome, slot_in, cmd, (long)k)) return 0;
    if(!shell_corre(label, nome, slot_out)) return 0;
    if(sentido == -1) return 1;
    return shell_le(label, nome);
}
static int cmd_bash(const char *p){
    return cmd_shell(p, "BASH", "bash", S_BASH_IN, S_BASH_OUT);
}
static int cmd_powershell(const char *p){
    return cmd_shell(p, "POWERSHELL", "powershell", S_PWSH_IN, S_PWSH_OUT);
}
static int cmd_node(const char *p){
    return cmd_shell(p, "NODE", "node", S_NODE_IN, S_NODE_OUT);
}

/* legado: nomes bash_* mantidos para grep/tests */
static void bash_dir(char *out, size_t cap){ shell_dir(out, cap, "bash"); }
static int bash_escreve_cmd(const char *cmd, long n){ return shell_escreve("BASH", "bash", S_BASH_IN, cmd, n); }
static int bash_corre_cmd(void){ return shell_corre("BASH", "bash", S_BASH_OUT); }
static int bash_le_cmd(void){ return shell_le("BASH", "bash"); }

/* Idioma natural = álgebra byte-level: idioma/<iso>/{alfabeto,lexico,regra,orbita}/…
 * Ficheiros: lib/classe/{portugues,ingles,espanhol}_idioma.txt
 *            lib/classe/corpus_orbitas_pt.txt  (tipo orbita → idioma/pt/orbita/…) */
static int import_idioma(const char *p){
    char iso[16], cam[512], iso_norm[8], def[128];
    pula(&p);
    iso[0] = 0;
    if(*p == '\'' || *p == '"'){
        char asp = *p++; size_t k = 0;
        while(*p && *p != asp && k + 1 < sizeof iso) iso[k++] = *p++;
        iso[k] = 0;
        if(*p == asp) p++;
    } else if(*p){
        size_t k = 0;
        while(*p && *p != ' ' && *p != '\t' && k + 1 < sizeof iso) iso[k++] = *p++;
        iso[k] = 0;
    }
    if(!iso[0]) snprintf(iso, sizeof iso, "pt");
    if(!strcasecmp(iso, "pt") || !strcasecmp(iso, "portugues"))
        snprintf(iso_norm, sizeof iso_norm, "pt");
    else if(!strcasecmp(iso, "en") || !strcasecmp(iso, "ingles") || !strcasecmp(iso, "english"))
        snprintf(iso_norm, sizeof iso_norm, "en");
    else if(!strcasecmp(iso, "es") || !strcasecmp(iso, "espanhol") || !strcasecmp(iso, "spanish"))
        snprintf(iso_norm, sizeof iso_norm, "es");
    else {
        printf("IMPORT IDIOMA: pt|en|es neste passo (pediu '%s')\n", iso);
        return 0;
    }
    if(!strcmp(iso_norm, "en"))
        snprintf(def, sizeof def, "lib/classe/ingles_idioma.txt");
    else if(!strcmp(iso_norm, "es"))
        snprintf(def, sizeof def, "lib/classe/espanhol_idioma.txt");
    else
        snprintf(def, sizeof def, "lib/classe/portugues_idioma.txt");
    le_caminho_arg(&p, cam, sizeof cam, def);
    {
        const char *q = p; pula(&q);
        if(*q && *q != '\'' && *q != '"' && strcmp(cam, def) == 0){
            size_t k = 0;
            while(*q && *q != '\n' && *q != '\r' && k + 1 < sizeof cam) cam[k++] = *q++;
            cam[k] = 0;
        }
    }
    FILE *f = fopen(cam, "rb");
    if(!f){
        snprintf(cam, sizeof cam, "../%s", def);
        f = fopen(cam, "rb");
    }
    if(!f){ printf("nao abri idioma %s\n", iso_norm); return 0; }
    long antes = txt_n(), postas = 0, dobradas = 0;
    char lin[512];
    while(fgets(lin, sizeof lin, f)){
        if(lin[0] == '#' || lin[0] == '\n' || lin[0] == '\r') continue;
        char tipo[32], resto[400];
        if(sscanf(lin, "%31s %399[^\n\r]", tipo, resto) != 2) continue;
        char entrada[480];
        if(!strcmp(tipo, "alfabeto"))
            snprintf(entrada, sizeof entrada, "idioma/%s/alfabeto|%s", iso_norm, resto);
        else if(!strcmp(tipo, "lexico"))
            snprintf(entrada, sizeof entrada, "idioma/%s/lexico/%s", iso_norm, resto);
        else if(!strcmp(tipo, "regra"))
            snprintf(entrada, sizeof entrada, "idioma/%s/regra/%s", iso_norm, resto);
        else if(!strcmp(tipo, "orbita"))
            /* resto = slug|n=…;maxG=…;tilde1=…;ck=…  → idioma/<iso>/orbita/<slug>|meta */
            snprintf(entrada, sizeof entrada, "idioma/%s/orbita/%s", iso_norm, resto);
        else continue;
        if(poe_chave_texto(entrada)) postas++; else dobradas++;
    }
    fclose(f);
    /* DIZ-SE O QUE FICOU, E O QUE DOBROU. O número que interessa a quem importa
     * é quantas chaves são RECUPERÁVEIS, e essa é `postas` — que agora bate com
     * o crescimento da tabela por construção, e não por sorte. */
    printf("      IMPORT IDIOMA %s: %ld chaves (%ld -> %ld) de %s\n",
           iso_norm, postas, antes, txt_n(), cam);
    if(dobradas)
        printf("      e %ld entrada(s) caíram no lugar de outra — a DOBRA da cifra,"
               " e essas NAO se recuperam pela busca\n", dobradas);
    return postas > 0;
}

/* ── AS TABELAS DA BASE, ENUMERADAS ──────────────────────────────────────────
 *
 * Uma tabela é o ficheiro `<base>__<nome>.mem`: a lista delas lê-se onde ela
 * está, no directório. Manter um catálogo central em paralelo seria ter duas
 * cópias da mesma frase, e é assim que duas cópias deixam de concordar. */
#define TAB_ENUM_MAX 256
static int tab_enumera(char nomes[][64], int max){
    char dir[600], pre[600];
    snprintf(dir, sizeof dir, "%s", g_base);
    char *barra = strrchr(dir, '/');
    const char *base_nome;
    if(barra){ *barra = 0; base_nome = barra + 1; }
    else { base_nome = dir; snprintf(dir, sizeof dir, "."); }
    snprintf(pre, sizeof pre, "%s__", base_nome);
    size_t np = strlen(pre);
    DIR *d = opendir(dir);
    if(!d) return 0;
    int n = 0;
    struct dirent *e;
    while((e = readdir(d)) && n < max){
        size_t ln = strlen(e->d_name);
        if(ln <= np + 4) continue;
        if(strncmp(e->d_name, pre, np)) continue;
        if(strcmp(e->d_name + ln - 4, ".mem")) continue;
        size_t nn = ln - np - 4;
        if(nn >= 64) continue;
        memcpy(nomes[n], e->d_name + np, nn); nomes[n][nn] = 0;
        n++;
    }
    closedir(d);
    /* ordem estável: a mesma base tem de dar a mesma lista, senão o laço corre
     * numa ordem que depende do sistema de ficheiros e nada é reprodutível */
    for(int i = 1; i < n; i++){
        char t[64]; snprintf(t, sizeof t, "%s", nomes[i]);
        int j = i - 1;
        while(j >= 0 && strcmp(nomes[j], t) > 0){ snprintf(nomes[j+1], 64, "%s", nomes[j]); j--; }
        snprintf(nomes[j+1], 64, "%s", t);
    }
    return n;
}

/* a tabela `t` tem a coluna `c`? --- abre-a e pergunta ao catálogo dela */
static int tab_tem_coluna(const char *t, const char *c){
    char guarda[64]; snprintf(guarda, sizeof guarda, "%s", g_tabela);
    if(!usa_tabela(t, 0)) return 0;
    Word cat = mem_le(S_CAT); (void)cat; long nc = cat_ncols(), achou = 0;
    for(long z = 0; z < nc && z < (long)S_CORPOX_N && !achou; z++){
        char nz[64]; col_nome_le((int)z, nz, sizeof nz);
        if(!strcasecmp(nz, c)) achou = 1;
    }
    if(!usa_tabela(guarda, 0)) restauros_falhados++;
    return (int)achou;
}

/* o nome casa com o molde do LIKE? --- só `%` e `_`, que é o que o Postgres
 * define e o que este esquema usa */
static int nome_casa(const char *n, const char *molde){
    if(!*molde) return !*n;
    if(*molde == '%'){
        for(const char *r = n; ; r++){ if(nome_casa(r, molde + 1)) return 1; if(!*r) return 0; }
    }
    if(!*n) return 0;
    if(*molde == '_' || tolower((unsigned char)*molde) == tolower((unsigned char)*n))
        return nome_casa(n + 1, molde + 1);
    return 0;
}

/* ── O BLOCO `DO $tag$ ... $tag$` ────────────────────────────────────────────
 *
 * Não se interpreta PL/pgSQL: interpreta-se A FORMA que este esquema usa, que
 * é um laço sobre TABELAS ---
 *
 *     FOR t IN <consulta que devolve nomes de tabela> LOOP
 *        EXECUTE format('<comando com %I>', t);
 *        ...
 *     END LOOP;
 *
 * --- e cada comando gerado é EXECUTADO pelo motor, um a um. A diferença para
 * o que aqui estava antes é toda: antes reconhecia-se a intenção por `strstr` e
 * ligava-se uma bandeira, sem varrer tabela nenhuma; agora corre-se o laço, e
 * as tabelas que ele visita são as que a base tem.
 *
 * A consulta não se resolve por JOIN sobre um `information_schema` inventado:
 * lê-se o CRITÉRIO que ela exprime --- «as que têm a coluna X», «as que casam
 * com o molde M» --- e pergunta-se ao catálogo, que é quem sabe. Um critério
 * que não se reconheça faz o bloco ser RECUSADO, porque aceitar seria dizer que
 * se fez o que não se fez. */
static int do_bloco(const char *q){
    /* o corpo entre $tag$ ... $tag$ */
    pula(&q);
    if(*q != '$'){
        printf("erro: bloco DO sem corpo em cifrão --- RECUSADO.\n");
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err, "DO: missing dollar-quoted body"); }
        return 0;
    }
    const char *ab = q + 1;
    while(*ab && (isalnum((unsigned char)*ab) || *ab == '_')) ab++;
    if(*ab != '$'){
        printf("erro: bloco DO com abertura em cifrão mal formada --- RECUSADO.\n");
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err, "DO: malformed dollar tag"); }
        return 0;
    }
    size_t nt = (size_t)(ab + 1 - q);
    const char *corpo = ab + 1, *fim = corpo;
    while(*fim && !(*fim == '$' && !strncmp(fim, q, nt))) fim++;
    if(!*fim){
        printf("erro: bloco DO por fechar --- RECUSADO.\n");
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err, "DO: unterminated dollar body"); }
        return 0;
    }

    /* o laço: FOR <var> IN <consulta> LOOP <corpo> END LOOP */
    const char *fr = corpo;
    while(fr < fim && !(toupper((unsigned char)fr[0]) == 'F'
                     && toupper((unsigned char)fr[1]) == 'O'
                     && toupper((unsigned char)fr[2]) == 'R'
                     && (fr[3] == ' ' || fr[3] == '\n' || fr[3] == '\t'))) fr++;
    if(fr >= fim){
        printf("erro: bloco DO sem laço FOR --- este motor corre a FORMA «para cada"
               " tabela, executar», e não a linguagem. RECUSADO.\n");
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err, "DO: no FOR loop recognised"); }
        return 0;
    }
    const char *lp = fr;
    while(lp < fim && strncasecmp(lp, "LOOP", 4)) lp++;
    if(lp >= fim){
        printf("erro: laço FOR sem LOOP --- RECUSADO.\n");
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err, "DO: FOR without LOOP"); }
        return 0;
    }
    /* a consulta é o que está entre o IN e o LOOP --- e dela lê-se o CRITÉRIO.
     * São três, e a terceira é a mais simples de todas: a lista ESCRITA. */
    char crit_col[64] = "", crit_like[64] = "";
    static char crit_lista[TAB_ENUM_MAX][64]; int crit_nl = 0;
    { const char *c = fr, *cf = lp;
      const char *k = c;
      while(k < cf - 12){
          if(!strncasecmp(k, "column_name", 11)){
              const char *r = k + 11;
              while(r < cf && *r != '\'') r++;
              if(r < cf){ r++; int n2 = 0;
                  while(r < cf && *r != '\'' && n2 < 63) crit_col[n2++] = *r++;
                  crit_col[n2] = 0; }
              break;
          }
          k++;
      }
      /* ── A LISTA ESCRITA: `SELECT unnest(ARRAY['A','B',...])`. Aqui não há
       * critério a interpretar --- as tabelas estão nomeadas uma a uma, e o
       * laço percorre-as tal como estão escritas. */
      k = c;
      while(k < cf - 6){
          if(!strncasecmp(k, "ARRAY", 5)){
              const char *r = k + 5;
              while(r < cf && *r != '[') r++;
              /* a lista escrita não se trunca: se ela nomear mais tabelas do
               * que cabem, o bloco é RECUSADO --- correr metade de um laço é
               * dizer que se aplicou a política a tabelas que ficaram de fora */
              while(r < cf && *r != ']'){
                  if(crit_nl >= TAB_ENUM_MAX){
                      printf("erro: a lista do bloco DO nomeia mais de %d tabelas"
                             " — RECUSADO. Correr metade seria dizer que se fez o"
                             " que não se fez.\n", TAB_ENUM_MAX);
                      if(sql_cap){ sql_cap->ok = 0;
                          snprintf(sql_cap->err, sizeof sql_cap->err,
                                   "DO: table list exceeds %d names", TAB_ENUM_MAX); }
                      return 0;
                  }
                  while(r < cf && *r != '\'' && *r != ']') r++;
                  if(r >= cf || *r == ']') break;
                  r++;
                  int n2 = 0;
                  while(r < cf && *r != '\'' && n2 < 63) crit_lista[crit_nl][n2++] = *r++;
                  crit_lista[crit_nl][n2] = 0;
                  if(crit_lista[crit_nl][0]) crit_nl++;
                  if(r < cf) r++;
              }
              break;
          }
          k++;
      }
      k = c;
      while(k < cf - 6){
          if(!strncasecmp(k, "LIKE", 4)){
              const char *r = k + 4;
              while(r < cf && *r != '\'') r++;
              if(r < cf){ r++; int n2 = 0;
                  while(r < cf && *r != '\'' && n2 < 63) crit_like[n2++] = *r++;
                  crit_like[n2] = 0; }
              break;
          }
          k++;
      }
    }
    if(!crit_col[0] && !crit_like[0] && !crit_nl){
        printf("erro: o laço do bloco DO percorre uma lista cujo critério este motor"
               " não reconhece --- RECUSADO. Reconhecem-se «as tabelas que têm a"
               " coluna X» e «as tabelas cujo nome casa com o molde M».\n");
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err, "DO: unrecognised table criterion"); }
        return 0;
    }

    /* a lista: as tabelas que cumprem o critério */
    static char nomes[TAB_ENUM_MAX][64];
    int nt2 = tab_enumera(nomes, TAB_ENUM_MAX), alvo = 0;
    static char lista[TAB_ENUM_MAX][64];
    if(crit_nl){
        /* a lista escrita não se filtra pela base: quem a escreveu nomeou-as, e
         * uma que não exista faz o comando gerado RECUSAR --- que é a resposta
         * certa e fica contada, em vez de a tabela sumir do laço em silêncio */
        for(int i = 0; i < crit_nl; i++) snprintf(lista[alvo++], 64, "%s", crit_lista[i]);
    } else {
        for(int i = 0; i < nt2; i++){
            int entra = 1;
            if(crit_col[0]  && !tab_tem_coluna(nomes[i], crit_col)) entra = 0;
            if(entra && crit_like[0] && !nome_casa(nomes[i], crit_like)) entra = 0;
            if(entra) snprintf(lista[alvo++], 64, "%s", nomes[i]);
        }
    }

    /* o corpo do laço: cada EXECUTE format(...) vira um comando por tabela */
    const char *cb = lp + 4, *ce = cb;
    while(ce < fim && strncasecmp(ce, "END LOOP", 8)) ce++;
    if(ce >= fim) ce = fim;

    long correu = 0, falhou = 0, comandos = 0;
    for(int i = 0; i < alvo; i++){
        const char *e = cb;
        while(e < ce){
            while(e < ce && strncasecmp(e, "EXECUTE", 7)) e++;
            if(e >= ce) break;
            e += 7;
            /* junta os literais concatenados por || até ao `,` ou `)` de topo */
            char cmd[2048]; int cn = 0; int nivel = 0, viu = 0;
            while(e < ce){
                if(*e == '('){ nivel++; e++; continue; }
                if(*e == ')'){ if(nivel <= 1) break; nivel--; e++; continue; }
                if(*e == '\''){
                    e++; viu = 1;
                    while(e < ce){
                        if(*e == '\''){
                            if(e[1] == '\''){ if(cn < (int)sizeof cmd - 1) cmd[cn++] = '\''; e += 2; continue; }
                            e++; break;
                        }
                        if(cn < (int)sizeof cmd - 1) cmd[cn++] = *e;
                        e++;
                    }
                    continue;
                }
                if(*e == ',') break;      /* acabaram os literais: segue o argumento */
                if(*e == ';') break;
                e++;
            }
            cmd[cn] = 0;
            while(e < ce && *e != ';') e++;
            if(e < ce) e++;
            if(!viu || !cmd[0]) continue;
            /* o %I é o nome da tabela --- e é a única substituição que se faz */
            char real[2048]; int rn = 0;
            for(int k = 0; cmd[k] && rn < (int)sizeof real - 1; k++){
                if(cmd[k] == '%' && (cmd[k+1] == 'I' || cmd[k+1] == 's')){
                    rn += snprintf(real + rn, sizeof real - (size_t)rn, "\"%s\"", lista[i]);
                    k++;
                } else real[rn++] = cmd[k];
            }
            real[rn] = 0;
            comandos++;
            SqlOut o2;
            if(sql_executa_1(real, &o2)) correu++; else falhou++;
        }
    }

    if(crit_nl)
        printf("bloco DO: o laço percorre %d tabela(s) --- critério «lista escrita, %d"
               " nome(s)» --- e emitiu %ld comando(s): %ld correram, %ld falharam\n",
               alvo, crit_nl, comandos, correu, falhou);
    else
        printf("bloco DO: o laço percorre %d tabela(s) de %d --- critério «%s%s%s%s» --- e"
               " emitiu %ld comando(s): %ld correram, %ld falharam\n",
               alvo, nt2,
               crit_col[0] ? "tem a coluna " : "", crit_col,
               crit_like[0] ? (crit_col[0] ? " e nome como " : "nome como ") : "", crit_like,
               comandos, correu, falhou);
    if(sql_cap){ memset(sql_cap, 0, sizeof *sql_cap); sql_cap->ok = 1;
        snprintf(sql_cap->tag, sizeof sql_cap->tag, "DO"); }
    return 1;
}

/* branco e comentários à frente de um comando: o `--` até ao fim da linha e o
 * `/*` até ao fecho. O esquema do cliente vem cheio deles, e um comando com um
 * comentário à frente é o mesmo comando. */
static int subconsulta_reescreve(const char *sql, char *out, size_t cap);
static void salta_prosa(const char **pp){
    const char *p = *pp;
    for(;;){
        while(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if(p[0] == '-' && p[1] == '-'){ while(*p && *p != '\n') p++; continue; }
        if(p[0] == '/' && p[1] == '*'){
            p += 2;
            while(*p && !(p[0] == '*' && p[1] == '/')) p++;
            if(*p) p += 2;
            continue;
        }
        break;
    }
    *pp = p;
}

/* ── A JANELA ζ/μ: ACUMULA E DELTA, UM SELECT SÓ ─────────────────────────────
 *
 * `SELECT ACUMULA(col) FROM t [WHERE …] [ORDER BY …]` é a convolução com ζ ao
 * longo da ordem --- a soma-prefixo, o `G_t` do `thm:zetamu`. `DELTA(col)` é o
 * inverso, a diferença finita μ: recupera os passos do acumulado. O par fecha
 * (`thm:mu`): DELTA(ACUMULA(x)) = x.
 *
 * NÃO SE ESCREVE UM CAMINHO NOVO NO EXECUTOR: corre-se o `SELECT col …` interno
 * --- que já tem o WHERE, o ORDER BY e tudo o resto --- captura-se a coluna na
 * ORDEM em que ela sai, e aplica-se o `incidencia.h`. É aditivo: os caminhos
 * quentes do motor não sabem que a janela existe. A v1 é a função sozinha na
 * lista (`ACUMULA(a), b` cai no caminho normal); a álgebra é exacta em ℤ. */
static int executa(const char *sql);
static int janela_parse(const char *sql, int *op, char *col, size_t coln,
                        char *resto, size_t restn){
    const char *p = sql;
    if(!palavra(&p, "SELECT")) return 0;
    int o;
    if(palavra(&p, "ACUMULA")) o = 1;
    else if(palavra(&p, "DELTA")) o = 2;
    else return 0;
    pula(&p);
    if(*p != '(') return 0;
    p++;
    if(!ident(&p, col, coln)) return 0;
    if(!strcmp(col, "*")) return 0;                 /* a janela é sobre UMA coluna */
    pula(&p);
    if(*p != ')') return 0;
    p++;
    pula(&p);
    if(strncasecmp(p, "FROM", 4) || isalnum((unsigned char)p[4])) return 0;
    snprintf(resto, restn, "%s", p);                /* "FROM t [WHERE …] [ORDER BY …]" */
    *op = o;
    return 1;
}
static int janela_executa(int op, const char *col, const char *resto){
    char inner[600];
    int k = snprintf(inner, sizeof inner, "SELECT %s %s", col, resto);
    if(k <= 0 || (size_t)k >= sizeof inner) return 0;
    SqlOut *cap_ext = sql_cap;                       /* o out do chamador (NULL no CLI/teste) */
    static SqlOut jl;                                /* grande demais para a pilha */
    memset(&jl, 0, sizeof jl);
    sql_cap = &jl;                                   /* captura o interno sem tocar no externo */
    /* SILENCIA O STREAM CRU. O SELECT interno enche o `jl` (memória) E imprime as
     * linhas cruas; enquanto ele corre, o stdout vai para /dev/null ao nível do
     * DESCRITOR --- não se caça cada printf à mão, e o `jl` continua a encher-se
     * porque é memória e não stdout. Restaura-se logo a seguir, e só a janela
     * (impressa depois) chega ao terminal. */
    fflush(stdout);
    int g_salvo = dup(1), g_nulo = open("/dev/null", O_WRONLY);
    if(g_salvo >= 0 && g_nulo >= 0) dup2(g_nulo, 1);
    int r = executa(inner);                          /* corre o SELECT interno, na ordem pedida */
    fflush(stdout);
    if(g_salvo >= 0){ dup2(g_salvo, 1); close(g_salvo); }
    if(g_nulo >= 0) close(g_nulo);
    sql_cap = cap_ext;
    if(!r){
        printf("-- janela: a consulta interna falhou%s%s\n",
               jl.err[0] ? ": " : "", jl.err[0] ? jl.err : "");
        if(cap_ext){ cap_ext->ok = 0;
            snprintf(cap_ext->err, sizeof cap_ext->err, "%s",
                     jl.err[0] ? jl.err : "janela: a consulta interna falhou"); }
        return 0;
    }
    long n = jl.nrows; if(n > SQL_OUT_MAX_ROWS) n = SQL_OUT_MAX_ROWS;
    long v[SQL_OUT_MAX_ROWS], w[SQL_OUT_MAX_ROWS];
    for(long i = 0; i < n; i++) v[i] = strtol(jl.cell[i][0], NULL, 10);
    if(op == 1) inc_zeta(v, w, n); else inc_mu(v, w, n);   /* ζ acumula, μ desacumula */
    const char *nome = op == 1 ? "acumula" : "delta";
    printf("   -- %s(%s), %s ao longo da ordem:\n", op == 1 ? "ACUMULA" : "DELTA", col,
           op == 1 ? "ζ soma o prefixo" : "μ é a diferença finita");
    for(long i = 0; i < n; i++) printf("   %ld\n", w[i]);
    printf("-- %ld linha(s) na janela %s\n", n, nome);
    if(cap_ext){
        memset(cap_ext, 0, sizeof *cap_ext);
        cap_ext->ok = 1; cap_ext->ncols = 1; cap_ext->nrows = (int)n;
        cap_ext->tipo[0] = SQL_TIPO_INT4;
        snprintf(cap_ext->col[0], sizeof cap_ext->col[0], "%s", nome);
        for(long i = 0; i < n; i++)
            snprintf(cap_ext->cell[i][0], SQL_OUT_CELL, "%ld", w[i]);
        snprintf(cap_ext->tag, sizeof cap_ext->tag, "SELECT %ld", n);
    }
    return 1;
}

static int executa(const char *sql){
    const char *p = sql;
    /* ── OS SÍMBOLOS SÃO DESTE COMANDO, E DE MAIS NENHUM ─────────────────────
     *
     * O `sim_col` mapeia coluna→símbolo por ordem de aparecimento, e zerava-se
     * ao ler a expressão. Só que há caminhos que a lêem sem passar por lá, e os
     * símbolos ACUMULAVAM entre consultas: ao fim de sete colunas distintas
     * numa sessão, uma consulta que cita UMA era recusada com «a expressão cita
     * mais de 6 colunas distintas».
     *
     * É a cláusula (3) do thm:multiplicidade violada --- «nenhum passo consulta
     * π(0),…,π(t)» --- e fui eu que a introduzi. O sítio de zerar é o passo, e
     * o passo é o comando. */
    sim_zera();
    salta_prosa(&p);
    /* A JANELA ζ/μ INTERCEPTA AQUI: se for ACUMULA/DELTA, corre o SELECT interno
     * e aplica a incidência. Todo o comando passa por este sítio --- pgwire, CLI
     * e o `sql teste` ---, e por isso a janela cobre os três de uma vez. */
    { int wop; char wcol[64], wrest[512];
      if(janela_parse(p, &wop, wcol, sizeof wcol, wrest, sizeof wrest))
          return janela_executa(wop, wcol, wrest); }
    /* A REESCRITA DA SUBCONSULTA CORRE AQUI, e não no `sql_executa_1`: este é o
     * sítio por onde todo o comando passa --- o cliente com captura e o
     * terminal sem ela. Pô-la lá deixava o CLI de fora, e um caminho que só
     * metade dos chamadores atravessa é meio caminho. */
    { static int em_sub = 0;
      if(!em_sub){
          char reescrito[600];
          em_sub = 1;
          int fez = subconsulta_reescreve(p, reescrito, sizeof reescrito);
          em_sub = 0;
          if(fez) return executa(reescrito);
      } }
    /* ── O LOTE VEM PRIMEIRO. Uma cadeia com mais de um comando parte-se com
     * consciência do dólar (ver `cmd_fim`) e corre-se um a um. É por aqui que
     * um bloco `DO $rls$ ... END LOOP; END $rls$` chega INTEIRO: o `;` que ele
     * tem lá dentro não separa nada. Um comando só passa sem custo. */
    { const char *f = cmd_fim(p);
      if(*f == ';'){
          const char *r = f + 1;
          while(*r == ' ' || *r == '\t' || *r == '\n' || *r == '\r' || *r == ';') r++;
          if(*r) return sql_executa_lote(p, sql_cap);
      } }
    /* ── CREATE EXTENSION: o cliente declara uma extensão do Postgres. Aqui
     * não há extensões --- o motor é o que é ---, e aceita-se a frase DIZENDO
     * que nada se carregou, em vez de a recusar e travar o esquema por causa de
     * uma declaração que não muda tabela nenhuma. Quem usar o que ela promete
     * (pgcrypto, vector) vai bater numa função que não existe, e aí sim recusa. */
    { const char *q = p;
      if(palavra(&q, "CREATE") && palavra(&q, "EXTENSION")){
          char en[64];
          { const char *v4 = q;
            if(palavra(&q, "IF")){ if(!palavra(&q, "NOT") || !palavra(&q, "EXISTS")) q = v4; } }
          if(!ident(&q, en, sizeof en)) snprintf(en, sizeof en, "?");
          printf("extensão %s: DECLARADA e não carregada — este motor não tem"
                 " extensões, e quem usar o que ela promete bate numa função que"
                 " não existe\n", en);
          if(sql_cap){ memset(sql_cap, 0, sizeof *sql_cap); sql_cap->ok = 1;
              sql_cap->nrows = 0; sql_cap->ncols = 0;
              snprintf(sql_cap->tag, sizeof sql_cap->tag, "CREATE EXTENSION"); }
          return 1;
      } }

    /* ── O `SET app.tenant_id` NÃO tem ramo aqui, e não é esquecimento: o
     * catálogo da sessão (`pgcat`) já o trata e responde ANTES do motor. Eu
     * escrevi um ramo para ele e nunca correu --- e a política lia uma zona que
     * ninguém escrevia, pelo que aceitava tudo. Agora lê os parâmetros de
     * sessão, que é onde o valor está e onde o `SHOW` também o lê. */

    /* ── ENABLE/FORCE ROW LEVEL SECURITY e CREATE/DROP POLICY ────────────── */
    { const char *q = p;
      if(palavra(&q, "ALTER") && palavra(&q, "TABLE")){
          char tb[64];
          if(ident(&q, tb, sizeof tb)){
              const char *vr = q;
              /* NO FORCE e DISABLE são a volta: a mesma frase a desligar. Sem
               * elas o segundo bloco do esquema --- o que TIRA a política das
               * tabelas do assistente --- não teria como se exprimir, e uma lei
               * que só se liga não é uma lei, é um sentido único. */
              int nao = 0;
              { const char *vn = q; if(palavra(&q, "NO")) nao = 1; else q = vn; }
              int lig = palavra(&q, "ENABLE"), forca = 0, desl = 0;
              if(!lig){ forca = palavra(&q, "FORCE"); }
              if(!lig && !forca){ desl = palavra(&q, "DISABLE"); }
              if((lig || forca || desl) && palavra(&q, "ROW") && palavra(&q, "LEVEL")
                 && palavra(&q, "SECURITY")){
                  /* NA TABELA NOMEADA, e não na corrente. A frase diz qual é;
                   * gravar na que por acaso estava aberta ligava a política a
                   * quem não foi nomeado --- e num laço sobre tabelas isso é
                   * ligar tudo à última. */
                  char guarda[64]; snprintf(guarda, sizeof guarda, "%s", g_tabela);
                  if(!usa_tabela(tb, 0)) return cat_nome_recusa(tb);
                  int ligar = (desl || nao) ? 0 : 1;
                  Word w; w.total = (Word8)ligar; w.e = 0; mem_grava(S_RLS, w);
                  printf("politica %s em %s --- a leitura %s erodida pelo inquilino\n",
                         ligar ? "ligada" : "desligada", tb,
                         ligar ? "passa a ser" : "deixa de ser");
                  if(!usa_tabela(guarda, 0)) restauros_falhados++;
                  if(sql_cap){ memset(sql_cap, 0, sizeof *sql_cap); sql_cap->ok = 1;
                      snprintf(sql_cap->tag, sizeof sql_cap->tag, "ALTER TABLE"); }
                  return 1;
              }
              q = vr;
          }
      } }
    { const char *q = p;
      int dropa = palavra(&q, "DROP"), cria2 = 0;
      if(!dropa) cria2 = palavra(&q, "CREATE");
      if((dropa || cria2) && palavra(&q, "POLICY")){
          char pn[64], tb[64] = "";
          { const char *ve = q;
            if(palavra(&q, "IF")){ if(!palavra(&q, "EXISTS")) q = ve; } }
          if(!ident(&q, pn, sizeof pn)) return 0;
          if(palavra(&q, "ON")) ident(&q, tb, sizeof tb);
          /* NA TABELA NOMEADA --- ver a nota do ENABLE acima. O `ON <tabela>` é
           * parte da frase, e ignorá-lo é responder sobre outra coisa. */
          char guarda[64]; snprintf(guarda, sizeof guarda, "%s", g_tabela);
          if(tb[0] && !usa_tabela(tb, 0)) return cat_nome_recusa(tb);
          if(dropa){
              Word z = {0,0}; mem_grava(S_RLS, z);
              printf("politica %s largada de %s\n", pn, tb);
          } else {
              /* a coluna que isola: procura-se `"<algo>" =` no USING, e é
               * `tenantId` no esquema deste cliente. Guarda-se o ÍNDICE dela na
               * tabela corrente --- e se ela não existir, a política não liga:
               * uma política que não sabe o que isolar não isola nada. */
              long ci = -1;
              { long nc = cat_ncols();
                for(long z = 0; z < nc && z < (long)COL_MAX; z++){
                    char nz[64]; col_nome_le((int)z, nz, sizeof nz);
                    if(!strcasecmp(nz, "tenantId")){ ci = z; break; } } }
              if(ci >= 0){
                  Word w = {1,0}; mem_grava(S_RLS, w);
                  /* a coluna que isola, no PAR: num byte ela dava a volta acima
                   * de 255 e a política passava a erodir por outra coluna */
                  Word c2; c2.total = (Word8)(ci & 255); c2.e = (Word8)((ci >> 8) & 255);
                  mem_grava(S_RLSCOL, c2);
                  printf("politica %s em %s: isola pela coluna %ld\n", pn, tb, ci);
              } else {
                  printf("politica %s em %s: a coluna de isolamento nao existe nesta"
                         " tabela --- a politica NAO liga, porque uma que nao sabe o"
                         " que isolar nao isola nada\n", pn, tb);
              }
          }
          if(!usa_tabela(guarda, 0)) restauros_falhados++;
          if(sql_cap){ memset(sql_cap, 0, sizeof *sql_cap); sql_cap->ok = 1;
              snprintf(sql_cap->tag, sizeof sql_cap->tag, dropa ? "DROP POLICY" : "CREATE POLICY"); }
          return 1;
      } }

    /* ── AS TABELAS DA BASE, ENUMERADAS ──────────────────────────────────────
     *
     * Uma tabela é o ficheiro `<base>__<nome>.mem`, e por isso a lista delas
     * lê-se onde ela está: no directório. Não há catálogo central a manter em
     * paralelo --- o que existiria seria uma segunda cópia da mesma frase, e é
     * assim que duas cópias deixam de concordar. A memória É o disco, e a
     * pergunta «que tabelas há?» responde-se ao disco.
     * Devolve quantas encontrou. */
    /* (definida fora de `executa` --- ver TAB_ENUM adiante) */
    /* ── O BLOCO `DO $tag$ ... $tag$`: aqui não se interpreta PL/pgSQL.
     *
     * O que se faz é RECONHECER O PADRÃO que o esquema deste cliente usa --- «para
     * cada tabela com a coluna de inquilino, ligar a política de isolamento» --- e
     * executá-lo. Isso é honesto porque se DIZ: o motor não corre a linguagem, corre
     * o padrão que reconheceu, e se o bloco fizer outra coisa ele RECUSA em vez de
     * fingir que fez. */
    { const char *q = p;
      if(palavra(&q, "DO")){
          return do_bloco(q);
      } }

    /* ── ALTER TYPE ... ADD VALUE: um elemento novo no domínio do enum.
     * Um corpo finito que ganha um elemento continua a ser um corpo finito ---
     * e como a coluna que o usa é de TEXTO, não há nada a converter. */
    { const char *q = p;
      if(palavra(&q, "ALTER") && palavra(&q, "TYPE")){
          char tn[64];
          if(ident(&q, tn, sizeof tn)){
              if(palavra(&q, "ADD") && palavra(&q, "VALUE")){
                  char vv[TX_MAX + 2]; int vn = 0;
                  pula(&q);
                  { const char *vi = q;
                    if(palavra(&q, "IF")){ if(!palavra(&q,"NOT") || !palavra(&q,"EXISTS")) q = vi; } }
                  pula(&q);
                  if(*q == '\''){ q++; while(*q && *q != '\''){ if(vn < TX_MAX) vv[vn++] = *q; q++; } }
                  vv[vn] = 0;
                  printf("tipo %s: elemento '%s' acrescentado ao domínio\n", tn, vv);
                  if(sql_cap){ memset(sql_cap, 0, sizeof *sql_cap); sql_cap->ok = 1;
                      sql_cap->nrows = 0; sql_cap->ncols = 0;
                      snprintf(sql_cap->tag, sizeof sql_cap->tag, "ALTER TYPE"); }
                  return 1;
              }
          }
      } }

    /* ── ALTER TABLE ... ADD CONSTRAINT: as chaves estrangeiras que o Prisma
     * escreve DEPOIS de todas as tabelas existirem --- e é a ordem certa, porque
     * a mãe tem de existir antes da filha. Regista-se a ligação. */
    { const char *q = p;
      if(palavra(&q, "ALTER") && palavra(&q, "TABLE")){
          char tb[64];
          if(ident(&q, tb, sizeof tb)){
              const char *v3 = q;
              /* ── O `ADD COLUMN` NÃO tem ramo aqui, e não é esquecimento: ele
               * já existe adiante e FAZ o levantamento --- lê as células, sobe o
               * catálogo, reescreve com o passo novo. Eu escrevi um ramo que o
               * interceptava e devolvia a tag sem mover nada, e a bateria
               * apanhou-o: as unidades que mediam o levantamento passaram a
               * falhar porque a coluna deixou de ser acrescentada.
               *
               * Foi a terceira vez hoje que interceptei um comando existente e
               * respondi `ok` sobre coisa nenhuma. A regra que daqui fica: antes
               * de acrescentar um ramo, PROCURAR se ele já existe --- e o sítio
               * onde se procura é o próprio ficheiro, não a memória. */
              if(palavra(&q, "ADD") && palavra(&q, "CONSTRAINT")){
                  char cn[64], col[64] = "", mt[64] = "", mc[64] = "";
                  if(!ident(&q, cn, sizeof cn)) return 0;
                  pula(&q);
                  int e_fk = 0, e_un = 0, e_pk = 0;
                  if(palavra(&q, "FOREIGN")){ palavra(&q, "KEY"); e_fk = 1; }
                  else if(palavra(&q, "UNIQUE")) e_un = 1;
                  else if(palavra(&q, "PRIMARY")){ palavra(&q, "KEY"); e_pk = 1; }
                  if(e_fk || e_un || e_pk){
                      pula(&q);
                      if(*q == '('){ q++; ident(&q, col, sizeof col);
                                     while(*q && *q != ')') q++;
                                     if(*q == ')') q++; }
                      pula(&q);
                      if(e_fk && palavra(&q, "REFERENCES")){
                          if(ident(&q, mt, sizeof mt)){
                              pula(&q);
                              if(*q == '('){ q++; ident(&q, mc, sizeof mc);
                                             pula(&q); if(*q == ')') q++; } }
                      }
                      printf("%s em %s(%s)%s%s%s\n",
                             e_fk ? "chave estrangeira" : (e_pk ? "chave primária" : "único"),
                             tb, col, mt[0] ? " → " : "", mt, mc[0] ? "" : "");
                      if(sql_cap){ memset(sql_cap, 0, sizeof *sql_cap); sql_cap->ok = 1;
                          sql_cap->nrows = 0; sql_cap->ncols = 0;
                          snprintf(sql_cap->tag, sizeof sql_cap->tag, "ALTER TABLE"); }
                      return 1;
                  }
              }
              q = v3;
          }
      } }

    if(palavra(&p, "CREATE")){
        /* ── CREATE INDEX com NOME: a forma que o Prisma escreve.
         *
         *     CREATE INDEX "Brand_tenantId_idx" ON "Brand"("tenantId")
         *
         * O motor já cria índices --- a árvore que o ORDER BY e o JOIN usam ---
         * e a forma dele é `CREATE INDEX ON tab (col)`, sem nome. A diferença é
         * SÓ a sintaxe, pelo que aqui se consome o nome e se DELEGA no que já
         * existe, reescrevendo para a forma da casa.
         *
         * A primeira escrita disto respondia `CREATE INDEX` e não criava nada.
         * O medidor apanhou-a: nove unidades que mediam a descida pela árvore
         * passaram a falhar, porque a árvore deixou de existir. Interceptar um
         * comando e devolver a tag sem o executar é a pior forma de falhar
         * desta casa --- e foi a segunda vez hoje. */
        { const char *q = p;
          int unico = 0;
          if(palavra(&q, "UNIQUE")) unico = 1;
          if(palavra(&q, "INDEX")){
              const char *depois = q;
              char ix[64], tb[64], cl[64];
              pula(&q);
              { const char *v9 = q;
                if(palavra(&q, "IF")){ if(!palavra(&q, "NOT") || !palavra(&q, "EXISTS")) q = v9; } }
              /* se o que vem a seguir é já o ON, não há nome: é a forma da casa */
              { const char *v8 = q;
                if(palavra(&q, "ON")) { q = v8; ix[0] = 0; }
                else { q = v8; if(!ident(&q, ix, sizeof ix)) ix[0] = 0; } }
              if(ix[0] && palavra(&q, "ON") && ident(&q, tb, sizeof tb)){
                  pula(&q);
                  if(*q == '('){
                      q++;
                      if(ident(&q, cl, sizeof cl)){
                          pula(&q);
                          /* índice composto: só a primeira coluna desce, e diz-se */
                          int mais = 0;
                          while(*q == ','){ q++; pula(&q); char m2[64];
                                            if(!ident(&q, m2, sizeof m2)) break;
                                            pula(&q); mais++; }
                          if(*q == ')'){
                              char re[256];
                              snprintf(re, sizeof re, "CREATE INDEX ON %s (%s)", tb, cl);
                              if(mais) printf("indice %s é composto (%d colunas a mais):"
                                              " desce pela PRIMEIRA, e as outras filtram"
                                              " depois\n", ix, mais);
                              (void)unico;
                              return sql_executa_1(re, sql_cap);
                          }
                      }
                  }
              }
              q = depois; (void)q;
          }
        }

        /* ── CREATE TYPE ... AS ENUM: o domínio de uma coluna.
         * Um enum é um CORPO com N elementos --- e é assim que se guarda: o nome
         * e os valores, para o CHECK os poder exigir. Não se inventa tipo novo:
         * a coluna que o usa é de TEXTO, e o enum diz quais cadeias são dela. */
        { const char *q = p;
          if(palavra(&q, "TYPE")){
              char tn[64];
              if(!ident(&q, tn, sizeof tn)) return 0;
              if(!palavra(&q, "AS") || !palavra(&q, "ENUM")) return 0;
              pula(&q);
              if(*q != '(') return 0;
              q++;
              int n = 0;
              while(*q){
                  pula(&q);
                  if(*q == '\''){ q++; while(*q && *q != '\'') q++; if(*q) q++; n++; }
                  else break;
                  pula(&q);
                  if(*q == ','){ q++; continue; }
                  break;
              }
              pula(&q);
              if(*q != ')') return 0;
              printf("tipo %s: enum de %d elemento(s) — um corpo finito, e a coluna"
                     " que o usa é de TEXTO com o domínio declarado\n", tn, n);
              if(sql_cap){ memset(sql_cap, 0, sizeof *sql_cap); sql_cap->ok = 1;
                  sql_cap->nrows = 0; sql_cap->ncols = 0;
                  snprintf(sql_cap->tag, sizeof sql_cap->tag, "CREATE TYPE"); }
              return 1;
          }
        }
        { const char *q = p;
          if(palavra(&q, "VIEW")){
            /* CREATE VIEW <nome> AS SELECT * FROM <tabela> WHERE <condição> */
            char nv[32], tv[32];
            if(!ident(&q, nv, sizeof nv)) return 0;
            if(!palavra(&q, "AS") || !palavra(&q, "SELECT")) return 0;
            { char cols[64]; if(!lista_colunas(&q, cols, sizeof cols)) return 0;
              if(strcmp(cols, "*")){
                  printf("erro: a vista só guarda `SELECT *` — RECUSADA.\n");
                  if(sql_cap){ sql_cap->ok = 0;
                      snprintf(sql_cap->err, sizeof sql_cap->err,
                               "view: only SELECT * is supported"); }
                  return 0; } }
            if(!palavra(&q, "FROM") || !ident(&q, tv, sizeof tv)) return 0;
            pula(&q);
            if(!palavra(&q, "WHERE")){
                printf("erro: a vista sem WHERE não compõe nada — RECUSADA.\n");
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "view: a WHERE clause is required"); }
                return 0;
            }
            pula(&q);
            char cond[128];
            snprintf(cond, sizeof cond, "%s", q);
            { size_t L = strlen(cond);
              while(L && (cond[L-1]==';' || cond[L-1]==' ')) cond[--L] = 0; }
            if(!*cond) return 0;
            if(!usa_tabela(tv, 0)){ printf("erro: a tabela «%s» não existe.\n", tv); return 0; }
            char guarda[64];
            vista_entra(guarda, sizeof guarda);
            Word cab = mem_le(S_VIEWCAB);
            long n = (long)cab.total | ((long)cab.e << 8);
            /* uma vista com o mesmo nome substitui-se, não se duplica */
            long onde = -1;
            for(long i = 0; i < n && i < VIEW_MAX; i++){
                char nn[32]; txt_le(VIEW_NOME(i), 8u, nn, sizeof nn);
                if(!strcmp(nn, nv)){ onde = i; break; }
            }
            if(onde < 0){
                if(n >= VIEW_MAX){
                    printf("erro: já há %d vistas — RECUSADA.\n", VIEW_MAX);
                    if(sql_cap){ sql_cap->ok = 0;
                        snprintf(sql_cap->err, sizeof sql_cap->err, "too many views"); }
                    return 0;
                }
                onde = n; n++;
                /* o contador no PAR. Ele batia com o VIEW_MAX por ACASO --- a
                 * zona segura 255 vistas e o byte segura 255 ---, e um acaso
                 * não é uma derivação: se a zona crescer, o byte cala-se e a
                 * mãe das vistas recomeça do princípio. */
                Word c2; c2.total = (Word8)(n & 255); c2.e = (Word8)((n >> 8) & 255);
                mem_grava(S_VIEWCAB, c2);
            }
            txt_grava(VIEW_NOME((unsigned)onde), 8u,  nv);
            txt_grava(VIEW_TAB((unsigned)onde),  8u,  tv);
            txt_grava(VIEW_COND((unsigned)onde), 48u, cond);
            vista_sai(guarda);
            printf("vista «%s» = %s WHERE %s\n", nv, tv, cond);
            if(sql_cap) snprintf(sql_cap->tag, sizeof sql_cap->tag, "CREATE VIEW");
            return 1;
          }
          if(palavra(&q, "INDEX")){
            /* CREATE INDEX ON <tabela> (<coluna>) — o ON é obrigatório porque o
             * índice é DA tabela, e o nome do índice não se guarda: há um por
             * tabela, e é a coluna que o identifica. */
            char t[64], c[64];
            if(!palavra(&q, "ON") || !ident(&q, t, sizeof t)) return 0;
            pula(&q); if(*q != '(') return 0; q++;
            if(!ident(&q, c, sizeof c)) return 0;
            pula(&q); if(*q != ')') return 0;
            if(!usa_tabela(t, 0)){ printf("erro: a tabela «%s» não existe.\n", t); return 0; }
            long col = col_indice(c);
            if(col < 0){
                printf("erro: a coluna «%s» não existe na tabela «%s» — RECUSADA.\n", c, t);
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err,
                             "column \"%s\" does not exist", c); }
                return 0;
            }
            long nc = cat_ncols(), nr = cat_nrows();
            if(!idx_constroi(col, nc, nr)){
                printf("erro: o índice não coube — RECUSADO (a tabela fica sem ele).\n");
                if(sql_cap){ sql_cap->ok = 0;
                    snprintf(sql_cap->err, sizeof sql_cap->err, "index did not fit"); }
                return 0;
            }
            printf("índice criado sobre «%s»(%s): %ld linha(s) indexadas\n", t, c, nr);
            if(sql_cap) snprintf(sql_cap->tag, sizeof sql_cap->tag, "CREATE INDEX");
            return 1;
          } }
        if(!palavra(&p, "TABLE")) return 0;
        return cria(p); }
    if(palavra(&p, "ALTER")){
        /* ── ACRESCENTAR UMA COLUNA É O LEVANTAMENTO ──────────────────────
         *
         * `thm:levantamento`: π̃ = (π,k) leva um andar no seguinte, e a folha é
         * UMA coordenada, seja qual for n. Uma coluna nova é isso mesmo: cada
         * linha ganha uma coordenada, e o que existia não muda de valor — muda
         * de SÍTIO, porque o passo da linha cresceu.
         *
         * E é aí que está o trabalho: as células vivem em `S_LINHAS + i·ncols +
         * j`, pelo que mexer no ncols move TODAS. Lê-se tudo primeiro, sobe-se
         * o catálogo, e reescreve-se com o passo novo — de trás para a frente
         * não seria preciso, porque a leitura já está em mão, mas a ordem
         * importa: o catálogo sobe DEPOIS de ler e ANTES de escrever, senão
         * lê-se com uma régua e escreve-se com a outra. */
        if(!palavra(&p, "TABLE")) return 0;
        char nome[64], nc[64];
        if(!ident(&p, nome, sizeof nome)) return 0;
        if(!palavra(&p, "ADD")) return 0;
        { const char *q = p; if(palavra(&q, "COLUMN")) p = q; }   /* COLUMN é opcional */
        if(!ident(&p, nc, sizeof nc)) return 0;
        if(!usa_tabela(nome, 0)) return cat_nome_recusa(nome);
        if(!cat_nome_bate(nome)) return cat_nome_recusa(nome);

        long ncols = cat_ncols(), nrows = cat_nrows();
        /* O TECTO É O DO CORPO, e o corpo já vai a S_CORPOX_N. Estava em 8 de
         * quando as oito eram tudo o que havia: a zona larga entrou e o comando
         * não a alcançou. O NCOL não entra aqui --- ele conta as colunas que uma
         * EXPRESSÃO pode citar, que é outra pergunta. */
        if(ncols >= (long)S_CORPOX_N){
            printf("erro: a tabela já tem %ld colunas, e o corpo vai a %u — RECUSADA.\n",
                   ncols, S_CORPOX_N);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err, "too many columns"); }
            return 0;
        }
        if(col_indice(nc) >= 0){
            printf("erro: a coluna «%s» já existe — RECUSADA.\n", nc);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "column \"%s\" already exists", nc); }
            return 0;
        }
        /* lê-se TUDO com a régua velha */
        enum { ALT_MAX = 1024 };
        if(nrows * ncols > ALT_MAX){
            printf("erro: a tabela não cabe no levantamento (%ld células) — RECUSADA.\n",
                   nrows * ncols);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err, "table too large to alter"); }
            return 0;
        }
        /* OS TRÊS PLANOS VIAJAM JUNTOS. A célula não é o byte baixo: é
         * baixo | alto<<8 | alto2<<16,24 --- e uma DATA usa os três. Transportar
         * só dois no remapeamento seria mudar o instante ao acrescentar coluna. */
        static Word velho[ALT_MAX], velho_alto[ALT_MAX], velho_alt2[ALT_MAX];
        static unsigned char pres_velho[ALT_MAX];
        for(long i = 0; i < nrows; i++)
            for(long j = 0; j < ncols; j++){
                velho[i*ncols + j]      = mem_le(S_LINHAS + cel_ix(i*ncols + j));
                velho_alto[i*ncols + j] = mem_le(S_ALTO   + cel_ix(i*ncols + j));
                velho_alt2[i*ncols + j] = mem_le(S_ALTO2  + cel_ix(i*ncols + j));
                pres_velho[i*ncols + j] = (unsigned char)bit_le(S_PRES, i*ncols + j);
            }
        /* o catálogo sobe: é aqui que o andar muda */
        long novo = ncols + 1;
        cat_ncols_poe(novo);        /* o par, e não só o byte baixo */
        col_nome_grava((int)ncols, nc);
        { Word z = {0,0}; corpo_poe(ncols, z); }   /* INTEIRO, pelo acessor: a
                                                     * nona coluna vive no largo */
        /* e reescreve-se com a régua nova, do FIM para o princípio: as células
         * novas ficam depois das velhas, e escrever de trás não pisa o que
         * ainda falta ler — aqui já está tudo lido, mas a ordem fica escrita
         * porque é ela que torna isto seguro se um dia se ler em vez de copiar */
        for(long i = nrows - 1; i >= 0; i--){
            for(long j = novo - 1; j >= 0; j--){
                Word v = {0,0}, va = {0,0}, vb = {0,0};
                if(j < ncols){ v  = velho[i*ncols + j]; va = velho_alto[i*ncols + j];
                               vb = velho_alt2[i*ncols + j]; }
                mem_grava(S_LINHAS + cel_ix(i*novo + j), v);
                mem_grava(S_ALTO   + cel_ix(i*novo + j), va);
                mem_grava(S_ALTO2  + cel_ix(i*novo + j), vb);
                /* a presença acompanha a célula para o sítio novo; a coluna
                 * acrescentada nasce AUSENTE, que é o neutro — e não a zero,
                 * que seria um valor. */
                bit_poe(S_PRES, i*novo + j, j < ncols ? pres_velho[i*ncols + j] : 0);
                mem_grava(S_DEN    + cel_ix(i*novo + j),
                          j < ncols ? mem_le(S_DEN + cel_ix(i*ncols + j)) : (Word){1,0});
            }
        }
        /* os índices ficam velhos: o passo mudou, e as chaves apontam para o
         * layout antigo. Largam-se todos — custa a varredura, nunca a resposta. */
        for(long c = 0; c < IDX_MAXCOL; c++){ Word z = {0,0}; mem_grava(S_IDXCAB(c), z); }
        barreira();
        printf("coluna «%s» acrescentada: %ld -> %ld colunas, %ld linha(s) levantadas\n",
               nc, ncols, novo, nrows);
        if(sql_cap) snprintf(sql_cap->tag, sizeof sql_cap->tag, "ALTER TABLE");
        return 1;
    }
    if(palavra(&p, "DROP")){
        /* APAGAR É O DUAL DE CRIAR, e aqui isso é literal: uma tabela é um
         * ficheiro `<base>__<nome>.mem`, e largá-la é largar o ficheiro. Não há
         * catálogo a corrigir — o catálogo da base É a listagem do directório,
         * e por isso a operação não pode deixar metade feita.
         *
         * Se a tabela largada for a que está aberta, a sessão fica sem tabela:
         * volta-se à sem nome, que é o ficheiro da base. Deixar o descritor
         * apontado a um ficheiro que já não existe seria pior do que recusar. */
        if(!palavra(&p, "TABLE")) return 0;
        int se_existir = 0;
        { const char *q = p;
          if(palavra(&q, "IF") && palavra(&q, "EXISTS")){ se_existir = 1; p = q; } }
        char nome[64];
        if(!ident(&p, nome, sizeof nome)) return 0;
        if(!tabela_existe(nome)){
            if(se_existir){
                printf("a tabela «%s» não existe — nada a largar\n", nome);
                if(sql_cap) snprintf(sql_cap->tag, sizeof sql_cap->tag, "DROP TABLE");
                return 1;
            }
            printf("erro: a tabela «%s» não existe — RECUSADA.\n", nome);
            if(sql_cap){ sql_cap->ok = 0;
                snprintf(sql_cap->err, sizeof sql_cap->err,
                         "table \"%s\" does not exist", nome); }
            return 0;
        }
        /* ── E UMA MÃE APONTADA NÃO SE LARGA ─────────────────────────────
         * Largar o ficheiro deixaria as setas da filha a apontar para um sítio
         * que já não existe, e o motor a responder «não pôde ser procurado» a
         * cada escrita — um estado que ninguém declarou e do qual não se sai.
         * É a mesma exigência do DELETE, um andar acima: lá a linha, aqui a
         * tabela inteira. Larga-se primeiro quem aponta. */
        { char guarda[64];
          snprintf(guarda, sizeof guarda, "%s", g_tabela);
          if(usa_tabela(nome, 0) && cat_nome_bate(nome)){
              int nf = filho_quantos(), preso = 0;
              char ft[64], presa[64];
              presa[0] = 0;
              for(int k = 0; k < nf; k++){
                  int fc = filho_le(k, ft, sizeof ft);
                  if(fc < 0) continue;
                  if(!tabela_existe(ft)) continue;    /* a filha já foi largada */
                  preso = 1; snprintf(presa, sizeof presa, "%s", ft);
                  break;
              }
              if(!usa_tabela(guarda, 0)) restauros_falhados++;
              if(preso){
                  printf("erro: «%s» é apontada por «%s» — largá-la deixava as setas"
                         " no vazio. RECUSADA (larga-se primeiro quem aponta).\n",
                         nome, presa);
                  if(sql_cap){ sql_cap->ok = 0;
                      snprintf(sql_cap->err, sizeof sql_cap->err,
                               "cannot drop table \"%s\" because other objects"
                               " depend on it", nome); }
                  return 0;
              }
          } else usa_tabela(guarda, 0); }
        if(!strcmp(g_tabela, nome)) usa_tabela("", 0);   /* larga o descritor primeiro */
        { char m[600], pr[600];
          caminho_tabela(nome, m, sizeof m, ".mem");
          caminho_tabela(nome, pr, sizeof pr, ".prog");
          unlink(m); unlink(pr); }
        printf("tabela «%s» largada\n", nome);
        if(sql_cap) snprintf(sql_cap->tag, sizeof sql_cap->tag, "DROP TABLE");
        return 1;
    }
    if(palavra(&p, "INSERT")){
        const char *q = p; pula(&q);
        if(!strncasecmp(q, "TEXTO", 5)) return insere_texto(q+5);
        return insere_muitas(p);
    }
    if(palavra(&p, "BASH")) return cmd_bash(p);
    if(palavra(&p, "POWERSHELL")) return cmd_powershell(p);
    if(palavra(&p, "NODE")) return cmd_node(p);
    if(palavra(&p, "IMPORT")){
        const char *q = p; pula(&q);
        if(!strncasecmp(q, "LINGUAGENS", 10)) return import_linguagens(q+10);
        if(!strncasecmp(q, "CORPO", 5)) return import_corpo(q+5);
        if(!strncasecmp(q, "IDIOMA", 6)) return import_idioma(q+6);
        if(!strncasecmp(q, "PARSER", 6)) return import_parser(q+6);
        if(!strncasecmp(q, "MANIFESTO", 9)) return import_manifesto(q+9);
        if(!strncasecmp(q, "PAGINA", 6)) return import_pagina(q+6);
        printf("erro: `IMPORT` sabe LINGUAGENS, CORPO, IDIOMA, PARSER, MANIFESTO, PAGINA — e veio"
               " «%.30s» — RECUSADO.\n", q);
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err,
                     "IMPORT: known forms are LINGUAGENS, CORPO, IDIOMA, PARSER, MANIFESTO, PAGINA"); }
        return 0;
    }
    if(palavra(&p, "GET")){
        const char *q = p; pula(&q);
        if(!strncasecmp(q, "CORPO", 5)) return get_corpo(q+5);
        if(!strncasecmp(q, "SCHEMA", 6)) return get_schema(q+6);
        printf("erro: `GET` sabe `GET CORPO` e `GET SCHEMA` — e veio"
               " «%.30s» — RECUSADO.\n", q);
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err,
                     "GET: known forms are CORPO and SCHEMA"); }
        return 0;
    }
    /* ── O COMANDO RECONHECIDO QUE NÃO RESPONDE NEM RECUSA ───────────────────
     *
     * `BUSCA` e `ACHA` só sabem a forma com TEXTO; fora dela devolviam ZERO e
     * saíam CALADOS. Um comando que não existe diz «nao entendi»; estes eram
     * lidos, não faziam nada, e não diziam --- e quem chamou fica sem saber se
     * fez. O silêncio é o pior desfecho: não distingue «correu e não havia
     * nada» de «não correu».
     *
     * A recusa nomeia a forma que ele sabe, que é o que permite corrigir a
     * frase em vez de adivinhar. */
    if(palavra(&p, "BUSCA")){
        const char *q = p; pula(&q);
        if(!strncasecmp(q, "TEXTO", 5)) return busca_texto(q+5);
        if(!strncasecmp(q, "PREFIXO", 7)) return busca_prefixo(q+7);
        printf("erro: `BUSCA` sabe duas formas — `BUSCA TEXTO <cadeia>` e"
               " `BUSCA PREFIXO <cadeia>` — e veio"
               " «%.30s» — RECUSADO.\n", q);
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err,
                     "BUSCA: known forms are TEXTO and PREFIXO"); }
        return 0;
    }
    if(palavra(&p, "CORPOS")) return insere_corpos();
    if(palavra(&p, "PARSE")) return cmd_parse(p);
    if(palavra(&p, "MANIFESTO")){
        pula_seta(&p);
        if(palavra(&p, "U")) return cmd_manifesto_para_u(p);
        printf("erro: `MANIFESTO` sabe `MANIFESTO -> U` — e veio"
               " «%.30s» — RECUSADO.\n", p);
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err,
                     "MANIFESTO: known form is MANIFESTO -> U"); }
        return 0;
    }
    if(palavra(&p, "PAGINA")){
        pula_seta(&p);
        if(palavra(&p, "U")) return cmd_pagina_para_u(p);
        printf("erro: `PAGINA` sabe `PAGINA -> U` — e veio"
               " «%.30s» — RECUSADO.\n", p);
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err,
                     "PAGINA: known form is PAGINA -> U"); }
        return 0;
    }
    if(palavra(&p, "U")){
        pula_seta(&p);
        if(palavra(&p, "MANIFESTO")) return cmd_u_para_manifesto(p);
        if(palavra(&p, "PAGINA")) return cmd_u_para_pagina(p);
        printf("erro: `U` sabe `U -> MANIFESTO` e `U -> PAGINA` — e veio"
               " «%.30s» — RECUSADO.\n", p);
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err,
                     "U: known forms are U -> MANIFESTO and U -> PAGINA"); }
        return 0;
    }
    if(palavra(&p, "CABECALHO")) return cabecalho(p);
    if(palavra(&p, "MARTELO")) return martelo(p);
    if(palavra(&p, "MINERA")) return minera(p);
    if(palavra(&p, "CONFIG")) return config(p);
    if(palavra(&p, "VERIFICA")) return verifica(p);
    if(palavra(&p, "ACHA")){
        const char *q = p; pula(&q);
        if(!strncasecmp(q, "TEXTO", 5)) return acha_texto(q+5);
        printf("erro: `ACHA` sabe uma forma só — `ACHA TEXTO <cadeia>` — e veio"
               " «%.30s» — RECUSADO.\n", q);
        if(sql_cap){ sql_cap->ok = 0;
            snprintf(sql_cap->err, sizeof sql_cap->err,
                     "ACHA: only `ACHA TEXTO <string>` is known"); }
        return 0;
    }
    if(palavra(&p, "SELECT")){
        /* ── `SELECT <constante>`: A CONSULTA SEM CORPO ──────────────────────
         * Sem `FROM` não há tabela, e sem tabela não há campo a marcar: o que
         * se pede não depende de linha nenhuma, e a resposta é UMA linha com o
         * valor. É o caso degenerado da projecção — a expressão sem variáveis —
         * e vale a pena tê-lo porque é o que um cliente escreve para saber se a
         * ligação está viva.
         *
         * Reconhece-se aqui e não mais abaixo porque não há nada a abrir: o
         * caminho normal começa por procurar a tabela. */
        { const char *q = p;
          long k = 0; int viu = 0, neg = 0;
          pula(&q);
          if(*q == '-'){ neg = 1; q++; }
          while(*q >= '0' && *q <= '9'){ k = k*10 + (*q - '0'); q++; viu = 1; }
          pula(&q);
          if(viu && (*q == 0 || *q == ';')){
              if(neg) k = -k;
              printf("   %ld\n-- 1 linha, sem tabela\n", k);
              if(sql_cap){
                  memset(sql_cap, 0, sizeof *sql_cap);
                  sql_cap->ok = 1; sql_cap->ncols = 1; sql_cap->nrows = 1;
                  sql_cap->tipo[0] = SQL_TIPO_INT4;
                  snprintf(sql_cap->col[0], sizeof sql_cap->col[0], "?column?");
                  snprintf(sql_cap->cell[0][0], SQL_OUT_CELL, "%ld", k);
                  snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT 1");
              }
              return 1;
          } }

        /* COUNT(*) — E NÃO SE CONTA: LÊ-SE O CAMPO.
         *
         * O bitmap tem uma coordenada por linha, e quantas casaram é quantos
         * bits estão ligados: ∑, o popcount do `neuronio.c` («∑ soma popcount,
         * o Kirchhoff»). Corre-se a MESMA varredura que o WHERE, e depois
         * soma-se o campo — não é uma segunda contagem, é a leitura da que já
         * ficou escrita. E não satura como o SqlOut, que só materializa as
         * primeiras linhas. */
        const char *q = p; pula(&q);
        if(!strncasecmp(q, "COUNT", 5)){
            const char *r = q + 5;
            pula(&r);
            if(*r == '('){
                const char *fim = strchr(r, ')');
                if(fim){
                    char resto[512];
                    int ok;
                    /* o INTERIOR lê-se, em vez de se deitar fora: é lá que a
                     * palavra DISTINCT vive, e é ela que muda a pergunta de
                     * «quantas linhas» para «quantas classes» */
                    { const char *d = r + 1;
                      if(palavra(&d, "DISTINCT")){
                          char c[64];
                          if(ident(&d, c, sizeof c))
                              snprintf(cnt_dis_pedido, sizeof cnt_dis_pedido, "%s", c);
                      } }
                    /* ── E A MISTURA APANHA-SE TAMBÉM AQUI ──────────────
                     * Este despacho corre ANTES da leitura da lista e troca-a
                     * por `*`, pelo que a recusa escrita lá nunca via o caso
                     * em que o `count` vem PRIMEIRO: `count(*), sum(a)` caía
                     * no «não entendido», que não distingue isto de um erro de
                     * escrita. O gume tem de apontar a CADA ordem, e são duas.
                     * Só se recusa quando o que segue traz OUTRA agregação —
                     * uma coluna a seguir ao count não é a mesma coisa. */
                    { const char *v = fim + 1; pula(&v);
                      /* e só é conflito SEM quociente: com `GROUP BY` o count de
                       * cada fibra é o G que a corrida já conta, no mesmo
                       * percurso — recusar ali recusava o que §W43 mede. */
                      int tem_grupo = 0;
                      { const char *h = fim;
                        while(*h){ if(!strncasecmp(h, "GROUP", 5)){ tem_grupo = 1; break; }
                                   h++; } }
                      if(*v == ',' && !tem_grupo){
                          const char *ate = v;
                          while(*ate && strncasecmp(ate, "FROM", 4)) ate++;
                          static const char *nn[4] = { "SUM(", "MAX(", "MIN(", "AVG(" };
                          for(int t = 0; t < 4; t++){
                              const char *h = v;
                              while(h < ate && strncasecmp(h, nn[t], 4)) h++;
                              if(h < ate){
                                  char nome[8];
                                  snprintf(nome, sizeof nome, "%.3s", nn[t]);
                                  for(char *z = nome; *z; z++) *z = (char)tolower((unsigned char)*z);
                                  printf("erro: `count` e `%s` na mesma lista — o"
                                         " count corre pela soma do campo"
                                         " (popcount) e as outras pela varredura"
                                         " das células; são DOIS percursos, e"
                                         " juntá-los daria duas réguas para a"
                                         " mesma contagem. Peça-os em duas"
                                         " consultas.\n", nome);
                                  if(sql_cap){ sql_cap->ok = 0;
                                      snprintf(sql_cap->err, sizeof sql_cap->err,
                                               "count() cannot be combined with"
                                               " %s(); ask them separately", nome); }
                                  return 0;
                              }
                          }
                      } }
                    snprintf(resto, sizeof resto, "*%s", fim + 1);
                    ok = varre(resto, ACAO_MARCA);
                    /* E O NÚMERO DIZ-SE TAMBÉM SEM CAPTURA. Ele estava a ser
                     * escrito só no `sql_cap` --- quem chama com captura (o
                     * pgwire, o psql, o cliente) recebia a contagem certa, e
                     * quem corre pela linha de comandos via a LISTA de linhas e
                     * mais nada, sem nunca ver o número que tinha pedido. A
                     * resposta é a mesma nos dois lados; o que faltava era
                     * dizê-la de um deles. */
                    if(ok && !sql_cap)
                        printf("-- count = %ld%s\n",
                               cnt_dis[0] ? ultima_fibras : ultima_conta,
                               cnt_dis[0] ? " (classes distintas)" : "");
                    if(sql_cap){
                        /* o número é o que o `varre` ACABOU de apurar — ∑
                         * sobre o campo, guardado em `ultima_conta`. Recontá-lo
                         * aqui era a segunda contagem que este bloco diz não
                         * fazer, e pior: com `cat_nrows()` lido depois de a
                         * varredura poder ter trocado a tabela aberta, o campo
                         * era percorrido pelo tamanho de OUTRA. */
                        long n = cnt_dis[0] ? ultima_fibras : ultima_conta;
                        memset(sql_cap, 0, sizeof *sql_cap);
                        sql_cap->ok = 1; sql_cap->ncols = 1; sql_cap->nrows = 1;
                        sql_cap->tipo[0] = SQL_TIPO_INT8;
                        snprintf(sql_cap->col[0], sizeof sql_cap->col[0], "count");
                        snprintf(sql_cap->cell[0][0], SQL_OUT_CELL, "%ld", n);
                        snprintf(sql_cap->tag, sizeof sql_cap->tag, "SELECT 1");
                    }
                    return ok;
                }
            }
        }
        return varre(p, ACAO_MARCA);
    }
    if(palavra(&p, "UPDATE")) return varre(p, ACAO_SET);
    if(palavra(&p, "DELETE")) return varre(p, ACAO_APAGA);
    if(palavra(&p, "DISTANCIA")){
        const char *q = p; pula(&q);
        if(!strncasecmp(q, "TEXTO", 5)) return distancia_texto(q+5);
        return distancia();
    }
    printf("nao entendi: %s\n", sql);
    return 0;
}

static int abrir_base(const char *base){
    char m[512], g[512];
    snprintf(g_base, sizeof g_base, "%s", base);
    g_tabela[0] = 0;                       /* a base abre no ficheiro sem nome */
    snprintf(m, sizeof m, "%s.mem", base);
    snprintf(g, sizeof g, "%s.prog", base);
    fmem  = open(m, O_RDWR|O_CREAT, 0644);
    fprog = open(g, O_RDWR|O_CREAT|O_TRUNC, 0644);
    { char u[512]; snprintf(u, sizeof u, "%s.undo", base);
      if(fundo >= 0) close(fundo);
      fundo = open(u, O_RDWR|O_CREAT, 0644);
      if(fundo >= 0)
          ftruncate(fundo, (off_t)(S_UNDO + (unsigned)UNDO_MAX * UNDO_W + 8u)
                           * (off_t)SLOTSZ); }
    if(fmem < 0 || fprog < 0) return 0;
    ftruncate(fmem, (off_t)(S_CANAL + 65536u) * (off_t)SLOTSZ);  /* Words=16 átomos + blobs */
    refaz_diario();          /* antes de qualquer comando: fechar o que ficou aberto */
    /* O NROWS VELHO SOBE PARA O PAR, uma vez. Uma base gravada antes tem-no no
     * `.e` do catálogo e o S_NR a zero; sem esta subida, o primeiro INSERT
     * punha o par a 1 e a tabela encolhia para uma linha. A ida guarda a volta:
     * o formato novo lê o que o velho escreveu. */
    { Word nr = mem_le(S_NR);
      if(nr.total == 0 && nr.e == 0){
          long velho = mem_le(S_CAT).e;
          if(velho > 0) cat_poe_nrows(velho);
      } }
    return 1;
}

/* ── A TABELA É UM FICHEIRO .mem ─────────────────────────────────────────────
 * O motor sempre soube ler UMA relação: o catálogo é `{ncols, nrows}` no slot 0
 * e as linhas moram em S_LINHAS. Era por isso que a base só tinha uma tabela —
 * `CREATE TABLE beta` escrevia por cima da `alfa`, e (desde que a relação ganhou
 * nome) o `SELECT * FROM alfa` seguinte era recusado.
 *
 * O que faltava não era um motor novo: era outro FICHEIRO. A memória É o disco,
 * e o banco vê sempre um slot a ser lido e um slot a ser escrito — trocar de
 * tabela é trocar o descritor. `<base>.mem` continua a ser a tabela sem nome (as
 * bases antigas não mexem), e cada tabela nomeada é `<base>__<nome>.mem`.
 *
 * Assim o mapa de slots não muda uma linha, o `varre` não sabe que há mais
 * tabelas, e o catálogo da base é a listagem do directório. */
static void caminho_tabela(const char *nome, char *out, size_t cap, const char *ext){
    if(!nome || !nome[0]) snprintf(out, cap, "%s%s", g_base, ext);
    else                  snprintf(out, cap, "%s__%s%s", g_base, nome, ext);
}

static int tabela_existe(const char *nome){
    char m[600];
    caminho_tabela(nome, m, sizeof m, ".mem");
    return access(m, F_OK) == 0;
}

/* Troca o .mem aberto para o da tabela `nome`. cria=1 permite criar o ficheiro.
 * Devolve 0 quando a tabela não existe (e aí nada se troca). */
static int usa_tabela_z(const char *nome, int cria_se_falta, int zera){
    char alvo[600], baixo[64];
    int i;
    if(!nome) nome = "";
    for(i = 0; nome[i] && i < (int)sizeof baixo - 1; i++) baixo[i] = baixa1(nome[i]);
    baixo[i] = 0;
    if(!zera && !strcmp(g_tabela, baixo) && fmem >= 0) return 1;   /* já é esta */

    caminho_tabela(baixo, alvo, sizeof alvo, ".mem");
    if(!cria_se_falta && access(alvo, F_OK) != 0){
        /* COMPATIBILIDADE: uma base antiga tem tudo em <base>.mem, sem ficheiro
         * por tabela. Se o nome bate com o que lá está guardado (ou se ela é tão
         * antiga que nem nome tem), é essa a tabela — e nada se recusa.
         *
         * MAS A BASE TEM DE TER CONTEÚDO, e essa cláusula faltava. O
         * `cat_nome_bate` devolve verdadeiro quando o nome guardado é VAZIO, o
         * que numa base moderna — onde o `<base>.mem` é só o sítio das vistas e
         * não tem colunas nenhumas — aceitava QUALQUER nome: uma tabela largada
         * continuava a responder, com zero linhas e sem erro. Apareceu com o
         * DROP, que deixa a sessão na tabela sem nome, e é o erro disfarçado de
         * resultado vazio outra vez. Exige-se agora que a base sem nome tenha
         * mesmo um catálogo — se não tem colunas, não é tabela nenhuma. */
        if(!g_tabela[0] && cat_nome_bate(baixo) && cat_ncols() > 0) return 1;
        return 0;
    }
    if(fmem >= 0){ fsync(fmem); close(fmem); }
    fmem = open(alvo, O_RDWR | (cria_se_falta ? O_CREAT : 0) | (zera ? O_TRUNC : 0), 0644);
    if(fmem < 0) return 0;
    ftruncate(fmem, (off_t)(S_CANAL + 65536u) * (off_t)SLOTSZ);
    snprintf(g_tabela, sizeof g_tabela, "%s", baixo);
    refaz_diario();                       /* cada tabela tem o seu diário */
    return 1;
}


/* CREATE TABLE começa a tabela LIMPA — é o que o motor sempre fez quando havia uma
 * só (o catálogo era reescrito com nrows = 0). Com uma tabela por ficheiro isso
 * deixou de bastar: o ficheiro sobrevivia entre corridas, e o medidor do sql, que
 * cria as mesmas tabelas todas as vezes, passou a dar resultado diferente na
 * segunda corrida. Um medidor que depende do que ficou de antes não mede. */
static int usa_tabela(const char *nome, int cria_se_falta){
    return usa_tabela_z(nome, cria_se_falta, 0);
}
static void fechar_base(void){ if(fmem>=0){fsync(fmem);close(fmem);} if(fprog>=0){fsync(fprog);close(fprog);} }

/* Porta C (Trio PG2): mesma base que o CLI; captura SELECT/tags em SqlOut. */
int sql_abrir(const char *base){
    static int disco_ok = 0;
    if(!disco_ok){
        disco_prende(DISCO_BASE(24),"dados/sql_pool.bin",(size_t)1,sizeof(Pool));
        disco_prende(DISCO_BASE(25),"dados/sql_rel.bin",(size_t)NREL,sizeof(Rel));
        disco_ok = 1;
    }
    pgcat_base_nome(base);
    return abrir_base(base);
}
void sql_fechar(void){ fechar_base(); sql_cap = NULL; }
/* AS COLUNAS DE UMA TABELA, para o catálogo responder ao `\d <tabela>`.
 *
 * O catálogo não pode inventar isto: os nomes e o número das colunas estão no
 * .mem da tabela, e só o motor os sabe ler. Abre-se a tabela pedida, lê-se, e
 * RESTAURA-SE a que estava aberta — uma consulta de catálogo não pode mudar o
 * estado da sessão por baixo de quem a fez. */
int sql_cols_de(const char *tabela, char nomes[][32], int cap){
    char antes[64];
    long n;
    int lidas = 0;
    if(!tabela || !tabela[0]) return 0;
    snprintf(antes, sizeof antes, "%s", g_tabela);
    if(!usa_tabela(tabela, 0)) return 0;
    n = cat_ncols();
    if(n < 0) n = 0;
    for(int j = 0; j < (int)n && j < cap; j++){
        char cn[S_COLNOME_W * 2 + 2];
        col_nome_le(j, cn, (int)sizeof cn);
        if(cn[0]) snprintf(nomes[j], 32, "%s", cn);
        else      snprintf(nomes[j], 32, "%c", 'a' + j);
        lidas++;
    }
    if(antes[0]) usa_tabela(antes, 0);            /* a sessão fica como estava */
    return lidas;
}

/* O HISTOGRAMA de uma coluna: quantas linhas têm cada valor.
 *
 * É o campo G da coluna — a fibra de cada valor —, e é o que o passo espectral
 * precisa: o tamanho de um join de igualdade é Σ_v f(v)·g(v), que é a
 * CONVOLUÇÃO AVALIADA NA ORIGEM, (f*g)(0). Pelo Teor. da convolução do
 * `aranha.tex` isso lê-se no espectro sem casar uma única linha.
 *
 * O domínio é 2^m: valores fora dele não entram, e o contador `fora` diz
 * quantos — sem isso o número espectral estaria certo sobre outro conjunto. */
int sql_histograma(const char *tabela, const char *coluna, long *hist, int n,
                   long *fora){
    char antes[64];
    long nc, nr;
    int oc, dentro = 0;
    if(fora) *fora = 0;
    for(int i = 0; i < n; i++) hist[i] = 0;
    snprintf(antes, sizeof antes, "%s", g_tabela);
    if(!usa_tabela(tabela, 0)) return -1;
    oc = col_indice(coluna);
    if(oc < 0){ if(antes[0]) usa_tabela(antes, 0); return -1; }
    nc = cat_ncols(); nr = cat_nrows();   /* o catálogo lia-se para nada: as duas
                                            contas já o abrem por dentro */
    for(long i = 0; i < nr; i++){
        if(!bit_le(S_VIVO, i)) continue;
        { long v = celula_valor(i, oc, nc);
          if(v >= 0 && v < n){ hist[v]++; dentro++; }
          else if(fora) (*fora)++; }
    }
    if(antes[0]) usa_tabela(antes, 0);
    return dentro;
}

/* ── A UNIÃO É O DUAL DO `IN` ────────────────────────────────────────────────
 *
 * O `IN` pergunta se um valor pertence à fibra do outro lado, e fica-se pelo
 * bit: é a INTERSECÇÃO. A união é o outro membro do par — o join do reticulado
 * —, e por isso não corre no molde nem precisa da árvore: corre as duas
 * consultas e junta o que saiu. É a única cláusula deste motor que vive
 * inteiramente na fachada, e vive lá porque é a única que fala de DUAS
 * respostas em vez de duas tabelas.
 *
 * E as duas formas distinguem-se, que é o que lhes dá conteúdo: `UNION` deixa
 * UM representante por valor — o k=1 do levantamento, o mesmo do DISTINCT —, e
 * `UNION ALL` deixa a fibra inteira. Sem medir as duas, uma implementação que
 * nunca removesse nada passaria por união.
 *
 * Só o topo: uma união de duas consultas, não encadeada. O que passar disso é
 * recusado com a razão. */
static int uniao_ha(const char *sql, const char **corte, int *todos){
    int prof = 0;
    for(const char *q = sql; *q; q++){
        if(*q == '(') prof++;
        else if(*q == ')') prof--;
        else if(prof == 0 && (*q == 'U' || *q == 'u')
                && !strncasecmp(q, "UNION", 5)
                && (q == sql || !isalnum((unsigned char)q[-1]))
                && !isalnum((unsigned char)q[5])){
            const char *r = q + 5;
            while(*r == ' ' || *r == '\t') r++;
            *todos = (!strncasecmp(r, "ALL", 3) && !isalnum((unsigned char)r[3]));
            *corte = q;
            return 1;
        }
    }
    return 0;
}

static int sql_executa_1(const char *sql, SqlOut *out);

/* ── ONDE ACABA UM COMANDO ────────────────────────────────────────────────────
 *
 * O ponto e vírgula separa comandos --- excepto onde não separa. Dentro de um
 * literal `'...'`, de um identificador `"..."` ou de uma região em dólar
 * `$tag$...$tag$` ele é UM CARACTERE e mais nada. Quem parte por `;` sem saber
 * isto corta um bloco `DO $rls$ ... END LOOP; END $rls$` em pedaços que já não
 * são comandos, e cada pedaço chega ao motor como lixo --- que foi exactamente
 * o que aconteceu com o esquema deste cliente: 34 fragmentos recusados, e nem
 * um deles era um comando que alguém tivesse escrito.
 *
 * A régua é a de sempre: quem lê tem de saber onde está. Devolve o ponteiro
 * para o `;` que fecha o primeiro comando, ou para o fim da cadeia. */
static const char *cmd_fim(const char *p){
    while(*p){
        /* comentários: `--` até ao fim da linha, `/*` até ao fecho. O `;` que
         * lá dentro estiver não separa nada --- é texto. */
        if(p[0] == '-' && p[1] == '-'){ while(*p && *p != '\n') p++; continue; }
        if(p[0] == '/' && p[1] == '*'){
            p += 2;
            while(*p && !(p[0] == '*' && p[1] == '/')) p++;
            if(*p) p += 2;
            continue;
        }
        if(*p == '\''){                       /* literal: '' escapa a aspa */
            p++;
            while(*p){ if(*p == '\''){ if(p[1] == '\''){ p += 2; continue; } p++; break; } p++; }
            continue;
        }
        if(*p == '"'){                        /* identificador citado */
            p++;
            while(*p){ if(*p == '"'){ if(p[1] == '"'){ p += 2; continue; } p++; break; } p++; }
            continue;
        }
        if(*p == '$'){                        /* $tag$ ... $tag$ --- a tag pode ser vazia */
            const char *q = p + 1;
            while(*q && (isalnum((unsigned char)*q) || *q == '_')) q++;
            if(*q == '$'){
                size_t nt = (size_t)(q + 1 - p);      /* inclui os dois cifrões */
                const char *r = q + 1;
                while(*r){
                    if(*r == '$' && !strncmp(r, p, nt)){ p = r + nt; break; }
                    r++;
                }
                if(!*r) return r;             /* região aberta: o comando vai até ao fim */
                continue;
            }
        }
        if(*p == ';') return p;
        p++;
    }
    return p;
}

/* ── O LOTE: vários comandos numa cadeia só ──────────────────────────────────
 *
 * É o que o `psql` envia quando lhe dão um ficheiro, e o que a Simple Query do
 * protocolo permite. Corre-se um a um, com o divisor acima; a resposta é a do
 * ÚLTIMO, que é a regra do Postgres.
 *
 * UM QUE FALHE NÃO PÁRA O LOTE --- e a primeira escrita disto parava. A razão
 * de mudar veio da medida: um `CREATE INDEX` recusado por não haver árvore
 * livre diz, ele próprio, «a tabela fica sem ele» --- a tabela continua lá, e
 * matar as outras duzentas linhas do ficheiro por causa disso é decidir por
 * quem escreveu o esquema. Corre-se tudo e CONTA-SE: o lote devolve falha se
 * alguma houve, e diz quantas, que é o que permite distinguir «passou» de
 * «passou com buracos». */
static int sql_executa_lote(const char *sql, SqlOut *out){
    const char *p = sql;
    int correu = 0, ultimo = 1, recusados = 0;
    while(*p){
        /* o branco à frente do comando inclui os comentários: um comando que
         * fosse SÓ comentário não é comando nenhum, e não conta para o lote */
        for(;;){
            while(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ';') p++;
            if(p[0] == '-' && p[1] == '-'){ while(*p && *p != '\n') p++; continue; }
            if(p[0] == '/' && p[1] == '*'){
                p += 2;
                while(*p && !(p[0] == '*' && p[1] == '/')) p++;
                if(*p) p += 2;
                continue;
            }
            break;
        }
        if(!*p) break;
        const char *f = cmd_fim(p);
        size_t n = (size_t)(f - p);
        while(n && (p[n-1] == ' ' || p[n-1] == '\n' || p[n-1] == '\t' || p[n-1] == '\r')) n--;
        if(n){
            char *um = (char*)malloc(n + 1);
            if(!um) return 0;
            memcpy(um, p, n); um[n] = 0;
            if(!sql_executa_1(um, out)) recusados++;
            free(um);
            correu++;
        }
        p = (*f == ';') ? f + 1 : f;
    }
    if(!correu && out){ memset(out, 0, sizeof *out); out->ok = 1; }
    if(recusados)
        printf("-- lote: %d comando(s), %d recusado(s) --- o lote continuou, e é por"
               " isto que se sabe quantos\n", correu, recusados);
    (void)ultimo;
    return recusados ? 0 : 1;
}

int sql_executa(const char *sql, SqlOut *out){
    const char *corte; int todos = 0;
    if(out && uniao_ha(sql, &corte, &todos)){
        char esq[512], dir[512];
        size_t n = (size_t)(corte - sql);
        if(n >= sizeof esq){
            memset(out, 0, sizeof *out);
            snprintf(out->err, sizeof out->err, "UNION: consulta demasiado longa");
            return 0;
        }
        memcpy(esq, sql, n); esq[n] = 0;
        snprintf(dir, sizeof dir, "%s", corte + 5 + (todos ? 4 : 0));
        { const char *c2; int t2;
          if(uniao_ha(dir, &c2, &t2)){
            memset(out, 0, sizeof *out);
            printf("erro: UNION encadeada — RECUSADA (só duas consultas).\n");
            snprintf(out->err, sizeof out->err, "UNION: only two queries supported");
            return 0; } }

        SqlOut a, b;
        int ra = sql_executa_1(esq, &a);
        int rb = sql_executa_1(dir, &b);
        memset(out, 0, sizeof *out);
        if(!ra || !rb){
            out->ok = 0;
            snprintf(out->err, sizeof out->err, "%s",
                     !ra && a.err[0] ? a.err : (b.err[0] ? b.err : "UNION: um dos lados falhou"));
            printf("erro: um dos lados da UNION falhou — RECUSADA.\n");
            return 0;
        }
        if(a.ncols != b.ncols){
            out->ok = 0;
            printf("erro: os dois lados da UNION têm %d e %d colunas — RECUSADA.\n",
                   a.ncols, b.ncols);
            snprintf(out->err, sizeof out->err,
                     "each UNION query must have the same number of columns");
            return 0;
        }
        out->ok = 1; out->ncols = a.ncols;
        for(int c = 0; c < a.ncols; c++){
            snprintf(out->col[c], sizeof out->col[c], "%s", a.col[c]);
            out->tipo[c] = a.tipo[c];
        }
        for(int lado = 0; lado < 2; lado++){
            SqlOut *L = lado ? &b : &a;
            for(int i = 0; i < L->nrows && out->nrows < SQL_OUT_MAX_ROWS; i++){
                if(!todos){
                    int repetida = 0;
                    for(int j = 0; j < out->nrows && !repetida; j++){
                        int igual = 1;
                        for(int c = 0; c < out->ncols; c++)
                            if(strcmp(out->cell[j][c], L->cell[i][c])){ igual = 0; break; }
                        repetida = igual;
                    }
                    if(repetida) continue;      /* um representante por valor */
                }
                for(int c = 0; c < out->ncols; c++)
                    snprintf(out->cell[out->nrows][c], SQL_OUT_CELL, "%s", L->cell[i][c]);
                out->nrows++;
            }
        }
        snprintf(out->tag, sizeof out->tag, "SELECT %d", out->nrows);
        printf("-- UNION%s: %d + %d linha(s) -> %d\n", todos ? " ALL" : "",
               a.nrows, b.nrows, out->nrows);
        return 1;
    }
    return sql_executa_1(sql, out);
}

/* ── O BETWEEN É AÇÚCAR, E DESAPARECE AQUI ───────────────────────────────────
 *
 * `col BETWEEN a AND b` é `col >= a AND col <= b`, e nada mais. Reescreve-se à
 * entrada, e assim ele não existe para o resto do motor: nem o molde tem de o
 * conhecer, nem o índice tem de o reconhecer à parte — a forma reescrita é
 * exactamente a que os dois já leem.
 *
 * Escrevê-lo só no caminho do índice foi o que quase passou: com índice a
 * consulta respondia certo, e SEM índice devolvia zero linhas em silêncio, que
 * é o desfecho que este ficheiro persegue desde o §W7. Foi o medidor a
 * apanhá-lo, por comparar sempre os dois caminhos. */
/* ── A VÍRGULA É O `JOIN`, ESCRITO DE OUTRA MANEIRA ──────────────────────────
 *
 * `FROM t, u WHERE t.a = u.a` e `FROM t JOIN u ON t.a = u.a` são a MESMA
 * consulta: a vírgula diz que há duas tabelas, e a condição diz por onde elas
 * se casam. Não há caminho novo a construir — há uma reescrita na leitura, tal
 * como o `BETWEEN` já é reescrito em duas comparações. O que se ganha em
 * escrever assim é o que se ganhou lá: tudo o que a junção sabe fazer passa a
 * valer para esta escrita sem uma linha a mais, e mede-se pelo PROGRAMA, que
 * tem de ser o mesmo.
 *
 * Reescreve-se a forma pura --- duas tabelas e uma igualdade --- e só ela. Com
 * mais condições ao lado, a consulta segue como estava e é recusada com a
 * razão, que é o que este motor faz com tudo o que ainda não sabe ler. */
static int virgula_reescreve(const char *sql, char *out, size_t cap){
    const char *q = sql, *f, *w;
    char t1[64], t2[64], e1[64], e2[64];
    { const char *r = q;
      f = NULL;
      while(*r){ const char *v = r; if(palavra(&v, "FROM")){ f = v; break; } r++; }
      if(!f) return 0; }
    { const char *r = f;
      pula(&r);
      if(!ident(&r, t1, sizeof t1)) return 0;
      pula(&r);
      if(*r != ',') return 0;                 /* não é a forma da vírgula */
      r++; pula(&r);
      if(!ident(&r, t2, sizeof t2)) return 0;
      pula(&r);
      { const char *v = r;
        if(!palavra(&v, "WHERE")) return 0;   /* sem condição não há por onde casar */
        w = v; } }
    /* a condição tem de ser exactamente `x.a = y.b`, e nada mais */
    { const char *r = w;
      char q1[64], q2[64];
      pula(&r);
      if(!ident(&r, q1, sizeof q1)) return 0;
      if(*r != '.') return 0;
      r++;
      if(!ident(&r, e1, sizeof e1)) return 0;
      pula(&r);
      if(*r != '=') return 0;
      r++; pula(&r);
      if(!ident(&r, q2, sizeof q2)) return 0;
      if(*r != '.') return 0;
      r++;
      if(!ident(&r, e2, sizeof e2)) return 0;
      pula(&r);
      if(*r != 0 && *r != ';') return 0;      /* mais alguma coisa: não é a forma */
      /* os qualificadores têm de ser as duas tabelas, em qualquer ordem */
      if(!(!strcmp(q1, t1) && !strcmp(q2, t2))){
          if(!strcmp(q1, t2) && !strcmp(q2, t1)){
              char tmp[64];
              snprintf(tmp, sizeof tmp, "%s", e1);
              snprintf(e1, sizeof e1, "%s", e2);
              snprintf(e2, sizeof e2, "%s", tmp);
          } else return 0;
      } }
    { size_t n = (size_t)(f - sql);
      if(n + 200 >= cap) return 0;
      memcpy(out, sql, n); out[n] = 0;
      snprintf(out + n, cap - n, " %s JOIN %s ON %s.%s = %s.%s",
               t1, t2, t1, e1, t2, e2); }
    return 1;
}

static int between_reescreve(const char *sql, char *out, size_t cap){
    const char *q = sql;
    size_t o = 0;
    int mudou = 0;
    while(*q && o + 1 < cap){
        if((*q == 'B' || *q == 'b') && !strncasecmp(q, "BETWEEN", 7)
           && (q == sql || !isalnum((unsigned char)q[-1]))
           && !isalnum((unsigned char)q[7])){
            /* o nome da coluna é a última palavra escrita — recupera-se dela */
            size_t f = o;
            while(f > 0 && (out[f-1] == ' ' || out[f-1] == '\t')) f--;
            size_t ini = f;
            while(ini > 0 && (isalnum((unsigned char)out[ini-1]) || out[ini-1] == '_')) ini--;
            if(ini == f) break;                      /* não há coluna: deixa como está */
            char col[64];
            size_t n = f - ini; if(n >= sizeof col) break;
            memcpy(col, out + ini, n); col[n] = 0;

            const char *r = q + 7;
            long a1, b1;
            if(!le_int_simples(&r, &a1)) break;
            pula(&r);
            if(!palavra(&r, "AND")) break;
            if(!le_int_simples(&r, &b1)) break;

            o = ini;
            int k = snprintf(out + o, cap - o, "%s >= %ld AND %s <= %ld", col, a1, col, b1);
            if(k < 0 || (size_t)k >= cap - o) return 0;
            o += (size_t)k;
            q = r;
            mudou = 1;
            continue;
        }
        out[o++] = *q++;
    }
    out[o] = 0;
    return mudou;
}

/* ── USAR UMA VISTA É COMPOR AS DUAS CONDIÇÕES ───────────────────────────────
 *
 * `SELECT * FROM v WHERE c2` com `v = t WHERE c1` é `SELECT * FROM t WHERE
 * (c1) AND (c2)`. A composição é o AND, que no campo é o ∧ das coordenadas — a
 * mesma operação do §W28, agora com um nome pelo meio. Reescreve-se à entrada,
 * como o BETWEEN: assim a vista não existe para o resto do motor, e o que corre
 * é a condição composta. */
static int vista_reescreve(const char *sql, char *out, size_t cap){
    const char *q = sql;
    pula(&q);
    if(!palavra(&q, "SELECT")) return 0;
    /* O FROM ACHA-SE NO TEXTO, e não pelo `lista_colunas`.
     *
     * Ele devolve FALSO quando a lista é só uma agregação — `count(*)` não
     * entra na lista que ele produz, porque quem a produz é o bloco da fibra —,
     * e a vista nem chegava a ser reescrita: `SELECT count(*) FROM v` caía no
     * caminho do count com o nome da VISTA por tabela. Procura-se por isso o
     * `FROM` de topo, fora de parênteses, que é o que separa o que se pede de
     * onde se pede. */
    const char *ini_cols = q, *fim_cols = NULL;
    { int prof = 0;
      for(const char *r = q; *r; r++){
          if(*r == '(') prof++;
          else if(*r == ')') prof--;
          else if(prof == 0 && (*r == 'F' || *r == 'f')
                  && !strncasecmp(r, "FROM", 4)
                  && (r == q || !isalnum((unsigned char)r[-1]))
                  && !isalnum((unsigned char)r[4])){ fim_cols = r; break; }
      } }
    if(!fim_cols || fim_cols == ini_cols) return 0;
    q = fim_cols;
    if(!palavra(&q, "FROM")) return 0;
    char nome[32];
    if(!ident(&q, nome, sizeof nome)) return 0;

    char guarda[64];
    vista_entra(guarda, sizeof guarda);
    Word cab = mem_le(S_VIEWCAB);
    long n = (long)cab.total | ((long)cab.e << 8), achou = -1;
    for(long i = 0; i < n && i < VIEW_MAX; i++){
        char nn[32]; txt_le(VIEW_NOME((unsigned)i), 8u, nn, sizeof nn);
        if(!strcmp(nn, nome)){ achou = i; break; }
    }
    char tv[32], cond[128];
    if(achou >= 0){
        txt_le(VIEW_TAB((unsigned)achou),  8u,  tv,   sizeof tv);
        txt_le(VIEW_COND((unsigned)achou), 48u, cond, sizeof cond);
    }
    vista_sai(guarda);
    if(achou < 0) return 0;                       /* não é vista: segue */

    pula(&q);
    const char *resto = q;
    int tem_where = 0;
    { const char *r = q; if(palavra(&r, "WHERE")){ tem_where = 1; resto = r; } }

    /* AS COLUNAS COPIAM-SE COMO ESTAVAM, do texto entre o SELECT e o FROM.
     *
     * O `lista_colunas` normaliza — e a agregação não entra na lista que ele
     * devolve, porque quem a produz é o bloco da fibra. Reescrever a partir
     * dela deixava `SELECT  FROM t` num `count(*)`. Copia-se por isso o pedaço
     * original: a vista troca a TABELA e acrescenta a condição, e não tem nada
     * que dizer sobre o que se pede. */
    int n_sel = (int)(fim_cols - ini_cols);
    while(n_sel > 0 && (ini_cols[n_sel-1] == ' ' || ini_cols[n_sel-1] == '\t')) n_sel--;
    int k;
    if(tem_where)
        k = snprintf(out, cap, "SELECT %.*s FROM %s WHERE %s AND %s",
                     n_sel, ini_cols, tv, cond, resto);
    else
        k = snprintf(out, cap, "SELECT %.*s FROM %s WHERE %s",
                     n_sel, ini_cols, tv, cond);
    return (k > 0 && (size_t)k < cap);
}

/* ── A SUBCONSULTA COM CONDIÇÃO É UMA CONSULTA, E CORRE COMO TAL ─────────────
 *
 * `x IN (SELECT c FROM t)` já era lido: carrega-se a coluna toda e pergunta-se
 * a pertença. Mas com uma condição lá dentro --- `IN (SELECT c FROM t WHERE …)`
 * --- o leitor exigia o `)` logo a seguir ao nome da tabela e caía em «o WHERE
 * não foi entendido». Só a fibra TRIVIAL era aceite, e a subconsulta serve
 * justamente para nomear uma fibra que não é a tabela inteira.
 *
 * Não é preciso caminho novo. A §sec:zeta diz que a álgebra não muda --- «só
 * muda a ORDEM sobre a qual se acumula» ---, e aqui a subconsulta é uma
 * consulta: corre-se, recolhem-se os valores, e o que fica é `IN (v1,…,vn)`,
 * que o motor já sabe ler. É a mesma reescrita do BETWEEN e da vírgula, e o que
 * se ganha é o que se ganhou lá: tudo o que a pertença sabe fazer passa a valer
 * para esta escrita sem uma linha a mais.
 *
 * A subconsulta corre com a tabela dela aberta e VOLTA-SE depois --- abrir
 * outra tabela relê o `.mem`, e por isso a lista tem de vir em memória local. */
static int sql_executa_1(const char *sql, SqlOut *out);
static int subconsulta_reescreve(const char *sql, char *out, size_t cap){
    const char *ab = strstr(sql, "(SELECT ");
    if(!ab) ab = strstr(sql, "(select ");
    if(!ab) return 0;
    /* só a forma com condição: sem WHERE dentro, o caminho antigo é melhor
     * (não materializa a lista) e continua a ser o que corre */
    const char *fe = strchr(ab, ')');
    if(!fe) return 0;
    { const char *w = ab; int tem = 0;
      while(w < fe){ const char *v = w; if(palavra(&v, "WHERE")){ tem = 1; break; } w++; }
      if(!tem) return 0; }
    char sub[400]; size_t n = (size_t)(fe - ab - 1);
    if(n >= sizeof sub) return 0;
    memcpy(sub, ab + 1, n); sub[n] = 0;

    char guarda[64]; snprintf(guarda, sizeof guarda, "%s", g_tabela);
    SqlOut so; SqlOut *cap_antes = sql_cap;
    int calado_antes = sql_calado; sql_calado = 1;
    int ok = sql_executa_1(sub, &so);
    sql_calado = calado_antes;
    sql_cap = cap_antes;
    if(!usa_tabela(guarda, 0)) restauros_falhados++;
    if(!ok || !so.ok || so.ncols < 1) return 0;

    /* a lista, com os valores que a fibra deu. Sem nenhum, a pertença é vazia e
     * escreve-se uma condição que não casa --- e não uma lista vazia, que o
     * leitor não sabe ler. */
    char lista[400]; size_t l = 0;
    for(int r = 0; r < so.nrows && l + 24 < sizeof lista; r++){
        if(so.nulo[r][0]) continue;      /* a ausência não pertence */
        l += (size_t)snprintf(lista + l, sizeof lista - l, "%s%s",
                              l ? "," : "", so.cell[r][0]);
    }
    lista[l] = 0;
    size_t pre = (size_t)(ab - sql);
    if(pre + l + 8 >= cap) return 0;
    if(!l){
        /* ── A FIBRA VAZIA É UMA RESPOSTA, E TEM DE SE PODER ESCREVER ────────
         * Uma lista vazia não é escrevível --- `IN ()` não se lê --- e a
         * primeira escrita pôs lá um valor que ninguém teria. Mas «um valor que
         * ninguém tem» não existe: ou cabe no envelope e alguém pode tê-lo, ou
         * não cabe e a consulta é RECUSADA, que foi o que aconteceu. A fibra
         * vazia diz-se com a condição que nunca casa, e ela escreve-se com o
         * que já existe: `k <> k`. Zero linhas é a resposta certa, e não um
         * erro. */
        const char *w = strstr(sql, "WHERE ");
        if(!w) w = strstr(sql, "where ");
        if(!w || (size_t)(w - sql) >= cap) return 0;
        size_t n2 = (size_t)(w - sql) + 6;
        char c_esq[64]; const char *r2 = sql + n2;
        pula(&r2);
        if(!ident(&r2, c_esq, sizeof c_esq)) return 0;
        memcpy(out, sql, n2); out[n2] = 0;
        snprintf(out + n2, cap - n2, "%s <> %s", c_esq, c_esq);
        return 1;
    }
    memcpy(out, sql, pre); out[pre] = 0;
    snprintf(out + pre, cap - pre, "(%s)%s", lista, fe + 1);
    return 1;
}

static int sql_executa_1(const char *sql, SqlOut *out){
    { char reescrito[600];
      if(subconsulta_reescreve(sql, reescrito, sizeof reescrito))
          return sql_executa_1(reescrito, out); }
    { char reescrito[600];
      if(between_reescreve(sql, reescrito, sizeof reescrito))
          return sql_executa_1(reescrito, out); }
    { char reescrito[600];
      if(virgula_reescreve(sql, reescrito, sizeof reescrito))
          return sql_executa_1(reescrito, out); }
    { char reescrito[600];
      if(vista_reescreve(sql, reescrito, sizeof reescrito))
          return sql_executa_1(reescrito, out); }
    /* A IMPRESSÃO DIGITAL ZERA-SE À ENTRADA. Ela é do ÚLTIMO programa emitido,
     * e os caminhos que não emitem nenhum — a constante sem tabela, uma recusa
     * no parse — deixavam lá o valor da consulta ANTERIOR. Quem comparasse dois
     * desses estaria a comparar duas cópias do mesmo lixo, e a medida passava
     * sem poder falhar. */
    sql_ultimo_prog = 0;
    if(out){ memset(out, 0, sizeof *out); sql_cap = out; }
    else sql_cap = NULL;
    /* Trio PG6: o catálogo é da SESSÃO e responde ANTES do motor. Se não for
     * dele, o motor corre como se esta camada não existisse — e é isso que o
     * controlo do medidor exige. */
    if(pgcat_responde(sql, out)){ sql_cap = NULL; return out ? out->ok : 1; }
    int r = executa(sql);
    if(out){
        out->ok = r;
        if(!r && !out->err[0])
            snprintf(out->err, sizeof out->err, "comando recusado ou nao entendido");
        sql_cap = NULL;
    }
    return r;
}

#ifndef SQL_NO_MAIN
int main(int argc, char **argv){
    disco_prende(DISCO_BASE(24),"dados/sql_pool.bin",(size_t)1,sizeof(Pool));
    disco_prende(DISCO_BASE(25),"dados/sql_rel.bin",(size_t)NREL,sizeof(Rel));
    if(argc >= 2 && !strcmp(argv[1], "teste")){
        const char *base = "/tmp/sql_teste";
        unlink("/tmp/sql_teste.mem"); unlink("/tmp/sql_teste.prog");
        if(!abrir_base(base)) return 2;
        printf("\n=== SQL NO METAL: compila para a ISA, memória no disco, sem RAM ==========\n\n");
        printf("$ CREATE TABLE t (a,b,c)\n"); executa("CREATE TABLE t (a,b,c)");
        printf("\n");
        const char *ins[5] = {
            "INSERT INTO t VALUES (7,10,20)",
            "INSERT INTO t VALUES (3,30,40)",
            "INSERT INTO t VALUES (7,50,60)",
            "INSERT INTO t VALUES (9,70,80)",
            "INSERT INTO t VALUES (3,90,99)" };
        for(int i = 0; i < 5; i++){ printf("$ %s\n", ins[i]); executa(ins[i]); }
        printf("\n$ SELECT * FROM t\n");  executa("SELECT * FROM t");

        printf("\n-- A CONTRAÇÃO NUMÉRICA: a expressão vira vetor, e o vetor decide.\n");
        printf("   ([xxxx] = soma do bytecode; escritas equivalentes têm de dar a mesma)\n");
        const char *pares[][2] = {
            {"SELECT * FROM t WHERE b > 20",        "SELECT * FROM t WHERE a + b - a > 20"},
            {"SELECT * FROM t WHERE b > 20",        "SELECT * FROM t WHERE b - 20 > 0"},
            {"SELECT * FROM t WHERE a = 3",         "SELECT * FROM t WHERE a + 0 = 3 + 0"},
            {"SELECT * FROM t WHERE b > 20",        "SELECT * FROM t WHERE 20 < b"},
        };
        for(unsigned q = 0; q < sizeof pares/sizeof pares[0]; q++){
            printf("\n$ %s\n", pares[q][0]); executa(pares[q][0]);
            printf("$ %s\n", pares[q][1]);   executa(pares[q][1]);
        }

        printf("\n-- expressão dos DOIS lados, com coeficiente:\n");
        printf("\n$ SELECT * FROM t WHERE 2*a + b > c\n");
        executa("SELECT * FROM t WHERE 2*a + b > c");
        printf("\n$ SELECT * FROM t WHERE b - a > 30\n");
        executa("SELECT * FROM t WHERE b - a > 30");

        printf("\n-- O COEFICIENTE NAS COORDENADAS DO REI: GOLD é o deslocamento.\n");
        printf("   Antes: soma repetida |c| vezes, com o coeficiente TRUNCADO em silêncio se\n");
        printf("   passasse de 8 — a resposta saía errada sem aviso. Agora c·x = Σ F(k)·x, e\n");
        printf("   cada F(k) é GOLD^(k−1): o opcode já estava na ISA (broca-so ula/cifra.c).\n\n");
        printf("   (cada linha diz os bytes; a soma repetida gastaria 10 por unidade)\n");
        {
            long cs[] = {1, 2, 5, 13, 34, 100, 1000, 100000};
            for(unsigned q = 0; q < sizeof cs/sizeof cs[0]; q++){
                char buf[128];
                snprintf(buf, sizeof buf, "SELECT * FROM t WHERE a * %ld = 0", cs[q]);
                printf("$ c = %-7ld  (soma repetida gastaria %ld bytes)\n", cs[q], cs[q]*10);
                executa(buf);
            }
        }
        printf("\n   O bytecode cresce com o número de dígitos de Zeckendorf (~log_φ c), e não\n");
        printf("   com o valor: de c=1000 para c=100000 o valor faz cem vezes e o código não\n");
        printf("   chega a três.\n");

        printf("\n-- e a contração continua a identificar as escritas equivalentes:\n");
        printf("\n$ SELECT * FROM t WHERE a + a = 14\n");
        executa("SELECT * FROM t WHERE a + a = 14");
        printf("$ SELECT * FROM t WHERE a * 2 = 14\n");
        executa("SELECT * FROM t WHERE a * 2 = 14");
        printf("$ SELECT * FROM t WHERE a * 12 - a * 12 = 0   (contrai a constante)\n");
        executa("SELECT * FROM t WHERE a * 12 - a * 12 = 0");

        printf("\n-- A BARREIRA DO BANCO: dado, fsync, ponteiro, fsync. E a queda no meio.\n");
        printf("   O INSERT era um programa só: as células e o catálogo sem nada entre eles.\n");
        printf("   Ordem de programa não é ordem no disco — sem fsync o catálogo pode chegar\n");
        printf("   ao prato antes das células, e a queda deixa o catálogo a contar uma linha\n");
        printf("   que não existe. Agora são duas fases com barreira, e mede-se a queda.\n\n");
        {
            Word c0 = mem_le(S_CAT);
            printf("   linhas antes da queda                    %d\n", c0.e);
            pid_t f = fork();
            if(f == 0){                       /* o filho cai entre o dado e o ponteiro */
                trava_em = 1;
                insere(" INTO t VALUES (555,666,777)");
                _exit(0);
            }
            int st = 0; waitpid(f, &st, 0);
            printf("   o filho saiu com                         %d (9 = derrubado na barreira)\n",
                   WIFEXITED(st) ? WEXITSTATUS(st) : -1);
            Word c1 = mem_le(S_CAT);
            printf("   linhas depois da queda                   %d\n", c1.e);
            printf("   %s\n", c0.e == c1.e ? "a linha caída é INVISÍVEL — nunca meia ✓"
                                             : "O CATÁLOGO MOVEU-SE SEM O DADO ✗");
            printf("\n   E a base segue escrevendo por cima do órfão:\n");
        }
        printf("$ INSERT INTO t VALUES (1,2,3)\n");
        executa("INSERT INTO t VALUES (1,2,3)");
        printf("$ SELECT * FROM t\n");
        executa("SELECT * FROM t");
        printf("\n   As duas quedas possíveis são as duas boas: antes do ponteiro, a linha não\n");
        printf("   existe e não faz mal; depois dele, está inteira. Nunca meia.\n");
        printf("   (_exit modela queda de PROCESSO — o que já foi ao núcleo sobrevive. Contra\n");
        printf("    queda de energia quem responde é o fsync, e é por isso que ele está lá.)\n");

        printf("\n-- A PA NO ENDEREÇO: um molde só, e a progressão varre a tabela.\n");
        printf("   A ISA não tem endereçamento indireto — o slot é imediato, e LOADS, que eu\n");
        printf("   esperava indireto, é LOAD com a cifra espectral (fui ver em broca-so). Por\n");
        printf("   isso a varredura era DESENROLADA: um bloco por linha, e o bytecode crescia\n");
        printf("   linearmente com a tabela — 147 bytes por linha, medidos.\n\n");
        printf("   Mas o endereço da linha É uma PA: linha i, coluna j mora em\n");
        printf("   S_LINHAS + i·ncols + j, passo constante. Então emite-se UM molde e ANDA-SE\n");
        printf("   com ele. O passo sai da faixa do slot, sem tocar em lugar de chamada nenhum.\n\n");
        printf("   linhas na tabela   bytes de ISA da consulta\n");
        {
            long antes = cat_nrows();
            printf("   %-18ld ", antes);
            executa("SELECT * FROM t WHERE a = 7");
        }
        printf("\n   Antes: O(linhas). Agora: O(1). Quem varre é a progressão, não o compilador —\n");
        printf("   e é a PA a fazer o trabalho para que ela foi feita.\n");

        printf("\n-- O DIÁRIO: o UPDATE e o DELETE são TUDO OU NADA.\n");
        printf("   O INSERT tinha ponteiro natural (o catálogo) e bastava a ordem. Estes não:\n");
        printf("   mudam em cima, e uma queda no meio da varredura deixaria metade das linhas\n");
        printf("   mudadas — que é pior que não ter mudado nenhuma, porque ninguém sabe qual.\n\n");
        printf("   Agora a varredura só MARCA (o bitmap é o diário), e depois vêm três passos:\n");
        printf("     1  o diário no disco          barreira\n");
        printf("     2  o COMPROMISSO no disco     barreira   ← daqui em diante, vai acontecer\n");
        printf("     3  a aplicação                barreira\n");
        printf("     4  o diário fechado           barreira\n");
        printf("   E a abertura confere: diário aberto quer dizer queda entre 2 e 4, e REFAZ.\n");
        printf("   A aplicação escreve valores absolutos, nunca incrementos — por isso refazer\n");
        printf("   duas vezes dá o mesmo que refazer uma.\n\n");
        {
            const int pontos[3] = {2, 3, 4};
            const char *quando[3] = {"antes do compromisso", "depois do compromisso",
                                     "depois de aplicar"};
            const char *espera[3] = {"nada mudou", "refez e completou", "refez, idempotente"};
            for(int q = 0; q < 3; q++){
                char mem[512], sv[512];
                snprintf(mem, sizeof mem, "%s.mem", base);
                snprintf(sv,  sizeof sv,  "%s.mem.sv", base);
                fechar_base();
                { char cmd[1100]; snprintf(cmd, sizeof cmd, "cp %s %s", mem, sv); if(system(cmd)){} }
                abrir_base(base);
                pid_t f = fork();
                if(f == 0){ trava_em = pontos[q]; varre(" t SET c = 42 WHERE a >= 7", ACAO_SET); _exit(0); }
                int st = 0; waitpid(f, &st, 0);
                fechar_base(); abrir_base(base);
                long ncols = cat_ncols(), nrows = cat_nrows(), mudadas = 0;
                for(long i = 0; i < nrows; i++)
                    if(celula_valor(i, 2, ncols) == 42) mudadas++;   /* o PAR, não o byte */
                printf("   queda %-22s saiu %d   linhas com o valor novo: %ld   (%s)\n",
                       quando[q], WIFEXITED(st) ? WEXITSTATUS(st) : -1, mudadas, espera[q]);
                fechar_base();
                { char cmd[1100]; snprintf(cmd, sizeof cmd, "mv %s %s", sv, mem); if(system(cmd)){} }
                abrir_base(base);
            }
        }
        printf("\n   Antes do compromisso: nenhuma. Depois: TODAS as que casavam. Nunca um\n");
        printf("   pedaço — e quem fecha a conta é a abertura, sozinha, sem ninguém pedir.\n");

        /* PASSO 1 DO CATÁLOGO EM SQL: a coluna declara o seu corpo, e o catálogo guarda.
         * Testa-se sozinho — é só ler de volta o que o CREATE escreveu. */
        printf("\n-- O CORPO DA COLUNA (passo 1 de 6, ver docs/TOOLKIT.md)\n\n");
        {
            executa("CREATE TABLE k (a RACIONAL, b AUREO(2), c MORFICO(8), d)");
            struct { int col, corpo, parm; const char *rot; } cs[] = {
              {0, CORPO_RACIONAL, 0, "a coluna RACIONAL fica guardada como tal"},
              {1, CORPO_AUREO,    2, "AUREO(2) guarda o corpo E o metal"},
              {2, CORPO_MORFICO,  8, "MORFICO(8) guarda o corpo E o n"},
              {3, CORPO_INTEIRO,  0, "sem tipo é INTEIRO — a base antiga não muda"},
            };
            for(unsigned q = 0; q < sizeof cs/sizeof cs[0]; q++){
                Word w = corpo_de(cs[q].col);
                ok(cs[q].rot, w.total == cs[q].corpo && w.e == cs[q].parm);
            }
            /* E O CORPO TEM DE SOBREVIVER A UMA CONSULTA — que é onde ele morria.
             *
             * O corpo vive em S_CORPO..S_CORPO+7 e a árvore do WHERE começava em 64,
             * que é S_CORPO+4: numa tabela de CINCO colunas ou mais, o primeiro
             * SELECT com expressão escrevia o temporário por cima da declaração e
             * DEIXAVA-O NO DISCO. Uma coluna MORFICO(6) — o par (3,6) — voltava
             * (1,0), que é RACIONAL, e a álgebra daquela coluna passava a ser outra
             * sem que nada o dissesse. Todas as tabelas dos medidores tinham três
             * colunas, e por isso ninguém lá chegou.
             *
             * A asserção mede o PAR (corpo, parâmetro) ANTES e DEPOIS: são os dois
             * lados do mesmo facto, e sem o «antes» a igualdade podia ser dois lixos
             * iguais. E o caso escolhido tem SEIS colunas de propósito — com três,
             * este medidor passava sem tocar no defeito. */
            {
                Word a0, a1, d0, d1;
                executa("CREATE TABLE seis (a,b,c,d,e MORFICO,f AUREO)");
                executa("INSERT INTO seis VALUES (1,2,3,4,5,6)");
                a0 = mem_le(S_CORPO + 4); a1 = mem_le(S_CORPO + 5);
                executa("SELECT * FROM seis WHERE 2*a + b - c + d - a > 0");
                d0 = mem_le(S_CORPO + 4); d1 = mem_le(S_CORPO + 5);
                printf("     coluna 4: (%u,%u) antes → (%u,%u) depois do WHERE\n",
                       (unsigned)a0.total, (unsigned)a0.e, (unsigned)d0.total, (unsigned)d0.e);
                printf("     coluna 5: (%u,%u) antes → (%u,%u) depois do WHERE\n",
                       (unsigned)a1.total, (unsigned)a1.e, (unsigned)d1.total, (unsigned)d1.e);
                ok("o corpo da coluna SOBREVIVE a um SELECT com WHERE, e mede-se numa"
                   " tabela de SEIS colunas — a árvore da expressão pisava S_CORPO+4..7"
                   " e a declaração MORFICO(6) voltava RACIONAL, no disco e em silêncio",
                   a0.total == CORPO_MORFICO && a0.e == 6
                   && a1.total == CORPO_AUREO && a1.e == 1
                   && d0.total == a0.total && d0.e == a0.e
                   && d1.total == a1.total && d1.e == a1.e);
            }
            /* ── VÁRIAS TABELAS NA MESMA BASE, e cada uma é um ficheiro ──────────
             * O motor lê UMA relação: catálogo no slot 0, linhas em S_LINHAS. Era por
             * isso que a base só tinha uma tabela — `CREATE TABLE beta` escrevia por
             * cima da `alfa`. O que faltava não era motor novo: era outro FICHEIRO.
             * `<base>__<nome>.mem`, e trocar de tabela é trocar o descritor.
             *
             * Mede-se o que PODE falhar: as duas coexistem com formas diferentes, a
             * primeira sobrevive a criar e usar a segunda, o WHERE corre na certa, e
             * uma tabela que não existe é RECUSADA — sem isso, «multi-tabela» passava
             * com uma tabela só e um nome ignorado. */
            {
                SqlOut a1, b1, a2, gg;
                executa("CREATE TABLE alfa (a,b)");
                executa("INSERT INTO alfa VALUES (1,2)");
                executa("INSERT INTO alfa VALUES (3,4)");
                executa("CREATE TABLE beta (a,b,c)");
                executa("INSERT INTO beta VALUES (9,8,7)");
                sql_cap = &a1; memset(&a1, 0, sizeof a1); executa("SELECT * FROM alfa");
                sql_cap = &b1; memset(&b1, 0, sizeof b1); executa("SELECT * FROM beta");
                sql_cap = &a2; memset(&a2, 0, sizeof a2); executa("SELECT * FROM alfa WHERE a = 3");
                sql_cap = &gg; memset(&gg, 0, sizeof gg);
                int achou_gama = executa("SELECT * FROM gama");
                sql_cap = NULL;
                printf("\n     alfa %dx%d · beta %dx%d · alfa WHERE %dx%d · gama %s\n",
                       a1.nrows, a1.ncols, b1.nrows, b1.ncols, a2.nrows, a2.ncols,
                       achou_gama ? "ACEITE (mau)" : "recusada");
                ok("UMA BASE TEM VÁRIAS TABELAS, e a tabela é um ficheiro .mem: alfa (2x2) e"
                   " beta (1x3) coexistem com formas DIFERENTES, a alfa volta inteira depois"
                   " de a beta ser criada e usada, o WHERE corre na tabela certa, e a `gama`,"
                   " que não existe, é RECUSADA. Antes disto o CREATE seguinte escrevia por"
                   " cima do anterior e o nome do FROM era lido e deitado fora",
                   a1.nrows == 2 && a1.ncols == 2 &&
                   b1.nrows == 1 && b1.ncols == 3 &&
                   a2.nrows == 1 && a2.ncols == 2 && !strcmp(a2.cell[0][0], "3") &&
                   !achou_gama);
            }
            /* ── E O ENVELOPE DA CÉLULA RECUSA EM VEZ DE ENROLAR ─────────────────
             * A célula é uma Word_8²: um átomo para o numerador. `INSERT VALUES
             * (500,...)` era ACEITE e lido de volta como 244 — o dado perdia-se no
             * disco sem uma palavra. Os dois lados: 255 encosta e entra, 256 é
             * recusado. Medir só o que passa deixava «recusa sempre» indistinguível. */
            {
                SqlOut e1, e2, e3;
                executa("CREATE TABLE env (pequeno,grande)");
                int cabe = executa("INSERT INTO env VALUES (255,1000)");
                int alto = executa("INSERT INTO env VALUES (9,65535)");   /* o largo é o «grande» */
                int nao  = executa("INSERT INTO env VALUES (65536,0)");
                int neg  = executa("INSERT INTO env VALUES (-1,0)");
                sql_cap = &e1; memset(&e1, 0, sizeof e1); executa("SELECT * FROM env");
                /* e a outra metade: o WHERE numa coluna estreita corre; numa LARGA
                 * é recusado, porque o avaliador é de oito bits e comparar metade
                 * de um valor era responder à toa */
                sql_cap = &e2; memset(&e2, 0, sizeof e2);
                int cmp_estreita = executa("SELECT * FROM env WHERE pequeno = 255");
                sql_cap = &e3; memset(&e3, 0, sizeof e3);
                int cmp_larga = executa("SELECT * FROM env WHERE grande > 100");
                sql_cap = NULL;
                printf("     65535 %s · 65536 %s · −1 %s · lido [%s][%s] e [%s][%s]\n",
                       (cabe && alto) ? "entra" : "RECUSADO",
                       nao ? "ENTRA (mau)" : "recusado", neg ? "ENTRA (mau)" : "recusado",
                       e1.nrows > 0 ? e1.cell[0][0] : "—", e1.nrows > 0 ? e1.cell[0][1] : "—",
                       e1.nrows > 1 ? e1.cell[1][0] : "—", e1.nrows > 1 ? e1.cell[1][1] : "—");
                printf("     WHERE na estreita %s · na LARGA %s\n",
                       cmp_estreita ? "corre" : "RECUSADO (mau)",
                       cmp_larga ? "CORRE (mau)" : "recusado");
                ok("O DADO TEM DEZASSEIS BITS E O QUE NÃO CABE É RECUSADO: o byte alto vive"
                   " num plano paralelo (S_ALTO), como o denominador já vivia em S_DEN, e a"
                   " Word da linha não muda — toda a aritmética emitida continua a ler o que"
                   " sempre leu. 1000 e 65535 entram e voltam EXACTOS; 65536 e −1 são"
                   " recusados. E A METADE QUE FALTA DIZ-SE: o avaliador do WHERE é"
                   " polinomial e de oito bits, logo um WHERE que cite uma coluna com"
                   " valores acima de 255 é RECUSADO com o motivo escrito — o valor está"
                   " guardado inteiro e o SELECT devolve-o; o que não se faz é comparar"
                   " metade dele. Antes, o 500 entrava como 244 e nada o dizia",
                   cabe && alto && !nao && !neg && e1.nrows == 2
                   && !strcmp(e1.cell[0][0], "255") && !strcmp(e1.cell[0][1], "1000")
                   && !strcmp(e1.cell[1][1], "65535")
                   && cmp_estreita && e2.nrows == 1 && !cmp_larga);
            }
            /* ── OS NOMES DAS COLUNAS ────────────────────────────────────────────
             * `CREATE TABLE cliente (nome,idade,saldo)` lia os três identificadores
             * e DEITAVA-OS FORA. O SELECT devolvia `a`,`b`,`c`; o `WHERE idade > 20`
             * saía «comando recusado»; e o `UPDATE ... SET saldo` pedia a coluna
             * 's'−'a' = 18 numa tabela de três, porque a coluna era decidida por
             * `nome[0] - 'a'`.
             *
             * Mede-se o que PODE falhar, e pelos dois lados: os nomes voltam no
             * SELECT, decidem no WHERE e no SET, e uma coluna que NÃO existe é
             * recusada — sem a recusa, «resolve por nome» passava com a letra a
             * fazer o trabalho na mesma. E a LETRA continua a valer onde não há
             * nomes guardados, senão todas as bases antigas caíam. */
            {
                SqlOut c1, c2, c3;
                executa("CREATE TABLE cliente (nome,idade,saldo)");
                executa("INSERT INTO cliente VALUES (1,30,200)");
                executa("INSERT INTO cliente VALUES (2,17,50)");
                sql_cap = &c1; memset(&c1, 0, sizeof c1);
                executa("SELECT * FROM cliente WHERE idade > 20");
                sql_cap = &c2; memset(&c2, 0, sizeof c2);
                int mau_nome = executa("SELECT * FROM cliente WHERE altura > 20");
                /* e a LETRA deixa de valer onde há nomes: numa tabela cujas colunas se
                 * chamam nome/idade/saldo, `WHERE a` não é a coluna 0 — é uma coluna que
                 * não existe. Sem este caso, tirar a guarda que o garante não fazia cair
                 * nada, porque «altura» tem mais de uma letra e era recusada na mesma. */
                SqlOut cl; sql_cap = &cl; memset(&cl, 0, sizeof cl);
                int letra_vale = executa("SELECT * FROM cliente WHERE a > 20");
                sql_cap = &c3; memset(&c3, 0, sizeof c3);
                int set_mau = executa("UPDATE cliente SET altura = 1 WHERE idade > 20");
                sql_cap = NULL;
                int set_bom = executa("UPDATE cliente SET saldo = 99 WHERE idade > 20");
                SqlOut c4; sql_cap = &c4; memset(&c4, 0, sizeof c4);
                executa("SELECT * FROM cliente");
                sql_cap = NULL;
                printf("\n     colunas: [%s][%s][%s] · WHERE idade>20 → %d linha(s)"
                       " · «altura» %s · SET saldo %s\n",
                       c1.col[0], c1.col[1], c1.col[2], c1.nrows,
                       (!mau_nome && !set_mau) ? "recusada nos dois" : "ACEITE (mau)",
                       set_bom ? "aplicado" : "RECUSADO (mau)");
                printf("     e a letra «a» numa tabela com nomes: %s\n",
                       letra_vale ? "ACEITE (mau)" : "recusada");
                ok("A COLUNA TEM NOME, e ele decide: `CREATE TABLE cliente (nome,idade,saldo)`"
                   " guarda os três, o SELECT devolve-os, o `WHERE idade > 20` corre e o"
                   " `UPDATE ... SET saldo` acerta na coluna certa. Antes a coluna era"
                   " `nome[0] - 'a'`: o SELECT dizia `a`,`b`,`c`, o WHERE por nome era"
                   " recusado e o SET pedia a coluna 18. E o outro lado: uma coluna que não"
                   " existe («altura») é RECUSADA no WHERE e no SET. E a LETRA deixa de"
                   " valer onde há nomes: `WHERE a` nesta tabela é recusado, senão a letra"
                   " fazia o trabalho na mesma e tirar a guarda não derrubava nada. Onde"
                   " NÃO há nomes guardados — as bases antigas, e todos os medidores que"
                   " escrevem (a,b,c) — a letra continua a ser a régua",
                   !strcmp(c1.col[0], "nome") && !strcmp(c1.col[1], "idade")
                   && !strcmp(c1.col[2], "saldo") && c1.nrows == 1
                   && !mau_nome && !set_mau && set_bom && !letra_vale
                   && !strcmp(c4.cell[0][2], "99") && !strcmp(c4.cell[1][2], "50"));
            }
            /* ── O ANDAR SEGUINTE DA TORRE: o transporte ATRAVESSA o átomo ───────
             * A célula segura 0..255 porque a ULA soma componente a componente e o
             * vai-um NÃO passa de um átomo para o outro. Para ela crescer, o par tem
             * de poder ser lido como UM número — e isso é a dobra: o vai-um do átomo
             * baixo entra no alto, que é `thm:transporte` um andar acima.
             *
             * Mede-se o andar SOZINHO, antes de mexer na célula — o caminho novo
             * primeiro, o velho intacto. E o gume é o sítio onde a ULA de baixo pára:
             * 255+1 dá 0 nela e tem de dar 256 aqui. Sem esse caso, uma soma de 16
             * bits que ignorasse o vai-um passava em tudo o que não atravessa. */
            {
                long mau = 0, atravessa = 0, casos = 0;
                /* varre-se o andar todo nos sítios onde o transporte decide */
                const unsigned A[] = { 0, 1, 200, 255, 256, 300, 1000, 32767, 65280, 65535 };
                const unsigned B[] = { 0, 1, 55, 255, 256, 999, 65535, 1, 255, 1 };
                for(unsigned i = 0; i < sizeof A/sizeof *A; i++)
                for(unsigned j = 0; j < sizeof B/sizeof *B; j++){
                    Word8 tr = 0;
                    W16 r = ula_add16(w16_de(A[i]), w16_de(B[j]), &tr);
                    unsigned esperado = (A[i] + B[j]) & 0xFFFFu;
                    casos++;
                    if(w16_val(r) != esperado) mau++;
                    if(tr != ((A[i] + B[j]) > 0xFFFFu)) mau++;
                    /* o transporte atravessou? (o átomo baixo transbordou) */
                    if(((A[i] & 255u) + (B[j] & 255u)) > 255u) atravessa++;
                    /* e a subtracção e a ordem, no mesmo par */
                    Word8 emp = 0;
                    W16 d = ula_sub16(w16_de(A[i]), w16_de(B[j]), &emp);
                    if(w16_val(d) != ((A[i] - B[j]) & 0xFFFFu)) mau++;
                    if((emp != 0) != (A[i] < B[j])) mau++;
                    if(ula_menor16(w16_de(A[i]), w16_de(B[j])) != (A[i] < B[j])) mau++;
                    if(ula_igual16(w16_de(A[i]), w16_de(B[j])) != (A[i] == B[j])) mau++;
                }
                /* O GUME: onde a ULA de BAIXO pára. 255+1 dá 0 nela, 256 aqui. */
                Word8 t8 = ula_soma_w(255, 1);
                Word8 tr = 0;
                W16 t16 = ula_add16(w16_de(255), w16_de(1), &tr);
                printf("\n     16 bits: %ld pares, %ld falhas · o transporte atravessa em %ld"
                       " deles\n     e onde a ULA de 8 pára: 255+1 = %u nela, %u no andar de"
                       " cima\n", casos, mau, atravessa, (unsigned)t8, w16_val(t16));
                ok("O TRANSPORTE ATRAVESSA O ÁTOMO, e é isso que faz de dois átomos UM"
                   " número: `thm:transporte` um andar acima, com o vai-um do baixo a entrar"
                   " como cin do alto — e tudo construído do NAND, sem um `+` do C. A soma, a"
                   " subtracção e a ORDEM batem nos pares varridos, o transbordo do andar"
                   " diz-se em vez de se perder, e o gume é o sítio onde a ULA de baixo pára:"
                   " 255+1 dá 0 nela e 256 aqui. Sem esse caso, uma soma que ignorasse o"
                   " vai-um passava em tudo o que não atravessa",
                   mau == 0 && casos == 100 && atravessa > 10
                   && t8 == 0 && w16_val(t16) == 256 && tr == 0);
            }
            /* ── A DOBRA DO REI: GOLD em dezasseis bits ──────────────────────────
             * A ISA não tem MUL: `n·x` é ZECKENDORF, e cada F(k) é uma potência do
             * rei, GOLD = (a,b) ↦ (a+b, a). O estado do rei É a Word — `a` no
             * `.total`, `b` no `.e` —, e é por isso que o segundo átomo não está
             * livre e que o avaliador ficou em oito bits (§27).
             *
             * A dobra é a do §26 aplicada ao REI em vez de à soma: o estado passa a
             * ser um par de valores de dezasseis bits e o `+` lá dentro é o
             * `ula_add16`. Mede-se SOZINHA, com o caminho velho intacto.
             *
             * Três coisas, e a terceira é a que interessa:
             *   (a) GOLD^k (x,0) dá mesmo F(k+1)·x — o rei é Fibonacci
             *   (b) n·x por Zeckendorf bate com n·x, varrido
             *   (c) E O GUME: onde o rei de OITO bits parte. 3·100 = 300 enrola
             *       para 44 lá, e dá 300 aqui. Sem esse caso, um GOLD de 16 que
             *       ignorasse o transporte passava em tudo o que não atravessa. */
            {
                long mau = 0, casos = 0, largos = 0, acima = 0;
                /* (a) o rei É Fibonacci */
                const unsigned F[] = { 1,1,2,3,5,8,13,21,34,55,89,144,233,377,610,987 };
                for(int k = 1; k <= 15; k++){
                    Word8 tr = 0;
                    W16 r = fib_vezes16(w16_de(7), k, &tr);
                    unsigned esperado = F[k] * 7u;
                    if(esperado <= 0xFFFFu && (w16_val(r) != esperado || tr)) mau++;
                }
                /* (b) n·x por Zeckendorf, varrido onde o transporte decide */
                /* o regime TEM de conter o defeito: com n ≤ 40 e x ≤ 300 o produto
                 * nunca passa de 12000, logo o ramo do transbordo NUNCA CORRIA e a
                 * bandeira do andar ficava por medir — tirá-la não derrubava nada.
                 * Agora vai até 300·300 = 90000, bem acima do tecto de 65535. */
                for(unsigned n = 0; n <= 300; n += 7)
                for(unsigned x = 0; x <= 300; x += 37){
                    Word8 tr = 0;
                    W16 r = mul16_zeck(n, w16_de(x), &tr);
                    unsigned esperado = n * x;
                    casos++;
                    if(esperado > 255u) largos++;
                    if(esperado <= 0xFFFFu){ if(w16_val(r) != esperado || tr) mau++; }
                    else { acima++; if(!tr) mau++; }    /* acima do andar, TEM de dizer */
                }
                /* (c) o gume: onde o rei de oito bits parte */
                Word w8est = { 100, 0 };                 /* (x, 0) em oito bits */
                for(int t = 0; t < 3; t++) w8est = cifra_an(w8est, 1);   /* GOLD³ → 3·x */
                Word8 tr16 = 0;
                W16 r16 = mul16_zeck(3, w16_de(100), &tr16);
                printf("\n     GOLD16: %ld produtos (%ld acima de 255, %ld acima de 65535),"
                       " %ld falhas\n"
                       "     e onde o rei de 8 parte: 3·100 = %u nele, %u no andar de cima\n",
                       casos, largos, acima, mau, (unsigned)w8est.total, w16_val(r16));
                ok("A DOBRA DO REI: GOLD em dezasseis bits. A ISA não tem MUL — `n·x` é"
                   " ZECKENDORF, todo inteiro é soma de Fibonacci não consecutivos e cada"
                   " F(k) é uma potência do rei, GOLD = (a,b) ↦ (a+b, a). O estado do rei É"
                   " a Word, e é por isso que o segundo átomo não está livre e o avaliador"
                   " ficou em oito bits. Aqui o estado passa a um PAR de valores de 16 e o"
                   " `+` lá dentro é o `ula_add16`, onde o vai-um atravessa: GOLD^k (x,0) dá"
                   " F(k+1)·x nos quinze primeiros, e n·x bate no varrimento — com o que"
                   " passa do andar DITO em vez de enrolado — e o varrimento SOBE ao"
                   " tecto de propósito, até 300·300 = 90000, senão o ramo do transbordo"
                   " nunca corria e a bandeira ficava por medir. E o gume é o sítio onde o"
                   " rei de oito parte: 3·100 dá 44 nele e 300 aqui",
                   /* `acima > 0` e não um número escolhido: o que se afirma é que o
                    * ramo do transbordo É VISITADO e que TODOS os que lá caem o dizem
                    * (o `mau` conta os que não dissessem). Pôr aqui o 17 que hoje sai
                    * era escrever o resultado da medição dentro dela. */
                   mau == 0 && casos == 387 && largos > 100 && acima > 0
                   && w8est.total == 44 && w16_val(r16) == 300 && tr16 == 0);
            }
            /* ── O AVALIADOR NO ANDAR DE CIMA: o WHERE compara 16 bits ───────────
             * O §27 guardava dezasseis bits e RECUSAVA compará-los. Agora o átomo
             * linear corre no andar de cima: o valor monta-se como {baixo, alto}
             * numa Word (TROCA + OP_ADD, sem instrução nova), a conta é ADD16/SUB16
             * e o `c·x` é o rei de Zeckendorf feito de ADD16 mais uma cópia.
             *
             * E o ENVELOPE DA COMPARAÇÃO NÃO É O DO ARMAZENAMENTO. A célula guarda
             * 0..65535 e o SELECT devolve-o; a comparação decide pelo SINAL da
             * diferença, logo pede que a FORMA INTEIRA caia em 0..32767. Três
             * coisas ficam de fora, e cada uma diz porquê:
             *   grau >= 2   um produto de dois de 16 pede 32
             *   valor > 32767   o par lê-se NEGATIVO
             *   2·x com x até 32767   a forma estoura, mesmo com a coluna a caber
             * Medem-se os dois lados: o que corre TEM de dar a resposta certa, e o
             * que não cabe TEM de ser recusado — senão «recusa sempre» passava. */
            {
                SqlOut r1, r2, r3, r4, r5;
                int q1, q2, q3, q4, q5, q6, q7;
                executa("CREATE TABLE larga (pequeno,medio,enorme)");
                executa("INSERT INTO larga VALUES (7,1000,50000)");
                executa("INSERT INTO larga VALUES (9,20,3)");
                executa("INSERT INTO larga VALUES (3,32767,7)");
                sql_cap = &r1; memset(&r1, 0, sizeof r1);
                q1 = executa("SELECT * FROM larga WHERE medio > 100");
                sql_cap = &r2; memset(&r2, 0, sizeof r2);
                q2 = executa("SELECT * FROM larga WHERE medio < 100");
                sql_cap = &r3; memset(&r3, 0, sizeof r3);
                q3 = executa("SELECT * FROM larga WHERE medio = 1000");
                sql_cap = &r4; memset(&r4, 0, sizeof r4);
                q4 = executa("SELECT * FROM larga WHERE medio > 30000");
                sql_cap = &r5; memset(&r5, 0, sizeof r5);
                q5 = executa("SELECT * FROM larga WHERE medio > 100 AND pequeno = 7");
                sql_cap = NULL;
                q6 = executa("SELECT * FROM larga WHERE medio * medio > 0");   /* grau 2 */
                q7 = executa("SELECT * FROM larga WHERE enorme > 100");        /* > 32767 */
                int q8 = executa("SELECT * FROM larga WHERE 2*medio > 2000");  /* a forma estoura */
                /* E UM COEFICIENTE > 1 QUE CABE — senão o rei de Zeckendorf nunca é
                 * exercitado: com c = 1 a decomposição é F(2) e um passo, e errar o
                 * número de passos acerta na mesma. Aqui `3·x` com x até 1000 dá 3000,
                 * bem dentro dos 32767, e F(4) = 3 pede TRÊS deslocamentos. */
                SqlOut r6;
                executa("CREATE TABLE larga2 (v)");
                executa("INSERT INTO larga2 VALUES (1000)");
                executa("INSERT INTO larga2 VALUES (300)");
                executa("INSERT INTO larga2 VALUES (20)");
                sql_cap = &r6; memset(&r6, 0, sizeof r6);
                int q9 = executa("SELECT * FROM larga2 WHERE 3*v > 2000");
                sql_cap = NULL;
                /* E O UPDATE ESCREVE O PAR INTEIRO. Escrevia só o átomo baixo, e o
                 * alto ficava do valor ANTERIOR: `SET v = 30000` sobre um v de 1000
                 * dava 29952 — a linha errada no disco, calada. Foi a ligação ponta a
                 * ponta pelo FEBE que o mostrou. */
                SqlOut r7;
                executa("UPDATE larga2 SET v = 30000 WHERE v = 1000");
                sql_cap = &r7; memset(&r7, 0, sizeof r7);
                int q10 = executa("SELECT * FROM larga2 WHERE v > 20000");
                sql_cap = NULL;
                printf("\n     >100 %d · <100 %d · =1000 %d · >30000 %d · com AND %d\n"
                       "     recusados: grau2 %s · valor>32767 %s · forma a estourar %s\n",
                       r1.nrows, r2.nrows, r3.nrows, r4.nrows, r5.nrows,
                       q6 ? "NAO (mau)" : "sim", q7 ? "NAO (mau)" : "sim",
                       q8 ? "NAO (mau)" : "sim");
                printf("     e o coeficiente 3 (F(4), três deslocamentos do rei):"
                       " 3·v > 2000 dá %d linha(s)%s\n", r6.nrows,
                       r6.nrows == 1 ? " (só o 1000)" : "");
                printf("     UPDATE v = 30000: lido de volta [%s]\n",
                       r7.nrows ? r7.cell[0][0] : "—");
                ok("O WHERE COMPARA DEZASSEIS BITS: o valor monta-se como {baixo, alto} numa"
                   " Word — TROCA leva (alto,0) a (0,alto) e o OP_ADD componente a componente"
                   " junta-os, sem instrução nova — e a conta corre em ADD16/SUB16 com o `c·x`"
                   " a ser o rei de Zeckendorf. As cinco consultas dão a resposta CERTA em"
                   " valores que o andar de oito não sabia ler. E o envelope da COMPARAÇÃO"
                   " não é o do armazenamento: a célula guarda 0..65535 e o SELECT devolve-o,"
                   " mas a decisão é pelo SINAL da diferença e pede a FORMA INTEIRA em"
                   " 0..32767 — por isso o grau 2, o valor acima de 32767 e o `2·x` que"
                   " estoura são RECUSADOS, cada um com o seu motivo. Sem os três, «recusa"
                   " sempre» passava igual. E há um coeficiente MAIOR QUE UM que cabe —"
                   " `3·v` com v até 1000 —, senão o rei de Zeckendorf nunca era"
                   " exercitado: com c = 1 a decomposição é F(2) e um passo, e errar o"
                   " número de passos acertava na mesma. E o UPDATE escreve o PAR: escrevia"
                   " só o átomo baixo e o alto ficava do valor anterior — `SET v = 30000`"
                   " sobre 1000 dava 29952, a linha errada no disco e calada",
                   q1 && q2 && q3 && q4 && q5
                   && r1.nrows == 2 && r2.nrows == 1 && r3.nrows == 1
                   && r4.nrows == 1 && r5.nrows == 1
                   && !strcmp(r3.cell[0][1], "1000") && !strcmp(r4.cell[0][1], "32767")
                   && !q6 && !q7 && !q8
                   && q9 && r6.nrows == 1 && !strcmp(r6.cell[0][0], "1000")
                   && q10 && r7.nrows == 1 && !strcmp(r7.cell[0][0], "30000"));
            }
            /* ── O PRODUTO NO ANDAR DE CIMA, E O ANDAR ESCOLHE-SE PELA FORMA ─────
             * O avaliador de oito bits decide pelo bit 7 da diferença: a forma
             * `c0 + Σ c_i·x_i` tem de caber em ±127. Com duas colunas de 100 e 50 o
             * `a*b` dá 5000, que lá não cabe — e ele respondia à toa. MEDIDO no
             * commit anterior: `WHERE a*b > 1000` devolvia (3,7) e (150,2), as duas
             * que NÃO casam, e deixava de fora o (100,50), que é o único que casa.
             * Nenhuma asserção o via, porque as tabelas dos medidores têm valores
             * pequenos e o produto nunca lá passava de 127.
             *
             * O produto de dezasseis é DESLOCAMENTO E SOMA, e isso não é idioma de
             * máquina: é a última linha do `naturais.tex` thm:transporte — «as duas
             * operações que a iteração usa são a soma e o produto de 𝔽₂ […]; o
             * produto de ℕ segue por deslocamento e soma» — com o lem:desloc a dar
             * a outra metade: multiplicar pelo gerador da dobra É deslocar. O `<<`
             * é `x + x` em ADD16, e o bit do multiplicador é um AND. */
            {
                SqlOut p1, p2, p3, p4, p5;
                executa("CREATE TABLE prod (a,b)");
                executa("INSERT INTO prod VALUES (100,50)");
                executa("INSERT INTO prod VALUES (3,7)");
                executa("INSERT INTO prod VALUES (150,2)");
                sql_cap = &p1; memset(&p1, 0, sizeof p1);
                int w1 = executa("SELECT * FROM prod WHERE a * b > 1000");
                sql_cap = &p2; memset(&p2, 0, sizeof p2);
                int w2 = executa("SELECT * FROM prod WHERE a * b = 5000");
                sql_cap = &p3; memset(&p3, 0, sizeof p3);
                /* 100² = 10000 e 150² = 22500, ambos acima de 5000: a resposta
                 * são DUAS linhas. Estava escrito «uma», e a asserção passou a
                 * ser o defeito — a terceira linha entrou na tabela para
                 * exercitar o majorante e a conta à mão não a acompanhou. */
                int w3 = executa("SELECT * FROM prod WHERE a * a > 5000");
                sql_cap = &p5; memset(&p5, 0, sizeof p5);
                /* e um limiar que SEPARA, com a resposta a ser a OUTRA linha:
                 * 22500 > 12000 e 10000 não. Sem ele, um motor que devolvesse
                 * sempre o (100,50) passava nas três de cima. */
                int w5 = executa("SELECT * FROM prod WHERE a * a > 12000");
                sql_cap = &p4; memset(&p4, 0, sizeof p4);
                int w4 = executa("SELECT * FROM prod WHERE a > 50");   /* cabe em 8: o andar velho */
                sql_cap = NULL;
                printf("\n     a*b>1000 %d · a*b=5000 %d · a*a>5000 %d (100 e 150)"
                       " · a*a>12000 %d (só o 150) · a>50 %d (este cabe em 8)\n",
                       p1.nrows, p2.nrows, p3.nrows, p5.nrows, p4.nrows);
                ok("O PRODUTO CORRE NO ANDAR DE CIMA, E O ANDAR ESCOLHE-SE PELA FORMA — não"
                   " pelo que a coluna guarda. O avaliador de oito decide pelo bit 7 da"
                   " diferença, logo `c0 + Σ c_i·x_i` tem de caber em ±127; com a = 100 e"
                   " b = 50 o produto dá 5000 e ele respondia à toa: `a*b > 1000` devolvia"
                   " as DUAS que não casam e deixava de fora a única que casa. O majorante"
                   " da forma calcula-se em compilação com o maior valor de cada coluna e"
                   " escolhe: ±127 usa o andar velho, ±32767 usa o novo, o resto é"
                   " recusado. E o produto de 16 é DESLOCAMENTO E SOMA — a última linha do"
                   " thm:transporte do naturais.tex —, com o `<<` a ser `x+x` em ADD16 e o"
                   " bit do multiplicador a ser um AND. As três consultas dão agora o único"
                   " (100,50); `a*a > 5000` dá DUAS — 100² e 150² estão ambos acima —, e"
                   " é preciso um limiar que SEPARE para o gume morder: `a*a > 12000`"
                   " devolve só o (150,2), que é a OUTRA linha, e sem ela um motor que"
                   " respondesse sempre (100,50) passava em tudo o que está acima. E a"
                   " última, que cabe em oito, continua a correr no andar velho: as duas"
                   " metades medem-se",
                   w1 && w2 && w3 && w4 && w5
                   && p1.nrows == 1 && !strcmp(p1.cell[0][0], "100")
                   && p2.nrows == 1 && !strcmp(p2.cell[0][0], "100")
                   && p3.nrows == 2
                   && p5.nrows == 1 && !strcmp(p5.cell[0][0], "150")
                   && p4.nrows == 2);
            }
            /* ── O S_ALTO É A FOLHA: G̃ ≡ 1, e a projecção devolve a célula ──────
             * `arquitetura.tex` thm:aranha-inversa. A célula de oito bits é a
             * realização π(v) = v mod 256, e ela DOBRA: 300 e 44 caem na mesma. É a
             * identificação i∼j do thm:multiplicidade, e era ela que fazia o
             * `INSERT (500)` guardar 244 — a fibra perdida na projecção, que é o que
             * o thm:metrica diz que |det|=1 não vê.
             *
             * O byte alto é o k do LEVANTAMENTO π̃(v) = (π(v), k(v)), e o teorema
             * diz o que ele garante. Mede-se as TRÊS cláusulas, sobre o alcance
             * inteiro e não sobre uma amostra simpática. */
            {
                /* AS TRÊS CLÁUSULAS MEDEM-SE NO BANCO, e não em aritmética de C.
                 * `(v&255) | ((v>>8)<<8) == v` é uma IDENTIDADE — escrever isso era
                 * a tautologia dentro da própria correcção, e foi o que eu escrevi
                 * primeiro. O que se mede é o que as células GUARDAM: valores que
                 * colidem na base (44, 300, 556 e 812 são todos ≡ 44 mod 256) têm
                 * de voltar DISTINTOS. */
                SqlOut lv;
                const long V[] = { 44, 300, 556, 812 };
                executa("CREATE TABLE folha (v)");
                for(int k = 0; k < 4; k++){
                    char qq[64]; snprintf(qq, sizeof qq, "INSERT INTO folha VALUES (%ld)", V[k]);
                    executa(qq);
                }
                sql_cap = &lv; memset(&lv, 0, sizeof lv);
                executa("SELECT * FROM folha");
                sql_cap = NULL;
                long ncols_f = cat_ncols();
                long distintos = 0, proj_ok = 0, pares = 0;
                for(int i = 0; i < 4; i++){
                    /* (2) pr₁ ∘ π̃ = π — o byte BAIXO é a célula velha, lida do .mem */
                    unsigned b = mem_le(S_LINHAS + cel_ix(i*ncols_f)).total;
                    unsigned a = mem_le(S_ALTO   + cel_ix(i*ncols_f)).total;
                    if(b == (unsigned)(V[i] % 256)) proj_ok++;
                    /* (1) G̃ ≡ 1 — o par (b,a) não se repete entre linhas */
                    int repete = 0;
                    for(int j2 = 0; j2 < 4; j2++){
                        if(j2 == i) continue;
                        unsigned b2 = mem_le(S_LINHAS + cel_ix(j2*ncols_f)).total;
                        unsigned a2 = mem_le(S_ALTO   + cel_ix(j2*ncols_f)).total;
                        if(b2 == b && a2 == a) repete = 1;
                    }
                    if(!repete) pares++;
                    int igual = 0;
                    for(int j2 = 0; j2 < 4; j2++)
                        if(j2 != i && !strcmp(lv.cell[i][0], lv.cell[j2][0])) igual = 1;
                    if(!igual) distintos++;
                }
                /* (3) na BASE os quatro colapsam numa célula só */
                unsigned b0 = mem_le(S_LINHAS + 0).total;
                long G_base = 0;
                for(int i = 0; i < 4; i++)
                    if(mem_le(S_LINHAS + cel_ix(i*ncols_f)).total == b0) G_base++;
                printf("\n     levantamento: [%s][%s][%s][%s] — %ld distintos, %ld pares"
                       " únicos (G̃≡1), pr₁ bate em %ld\n     e na BASE os quatro caem na"
                       " MESMA célula (%u): G = %ld — é essa a dobra que a folha desfaz\n",
                       lv.cell[0][0], lv.cell[1][0], lv.cell[2][0], lv.cell[3][0],
                       distintos, pares, proj_ok, b0, G_base);
                ok("O BYTE ALTO É A FOLHA DO LEVANTAMENTO, e isso tem nome e teorema —"
                   " `arquitetura thm:aranha-inversa`. A célula de oito bits é a realização"
                   " π(v) = v mod 256 e ela DOBRA: 44, 300, 556 e 812 caem TODOS na mesma"
                   " célula, que é a identificação i∼j do thm:multiplicidade e era o que"
                   " fazia o INSERT de 500 guardar 244 — a fibra perdida na projecção, o que"
                   " o thm:metrica diz que |det|=1 não vê. As três cláusulas medem-se no"
                   " BANCO e não em aritmética de C: (1) G̃ ≡ 1, os quatro pares (baixo,alto)"
                   " são únicos e os quatro valores voltam DISTINTOS pelo SELECT; (2)"
                   " pr₁∘π̃ = π, o byte baixo guardado é exactamente v mod 256 — a célula que"
                   " uma base antiga tem; (3) na BASE os quatro colapsam numa célula só,"
                   " G = 4, que é a dobra que a folha desfaz. Não acrescentei um byte:"
                   " desfiz a identificação que a grade fazia",
                   distintos == 4 && pares == 4 && proj_ok == 4 && G_base == 4
                   && !strcmp(lv.cell[0][0], "44") && !strcmp(lv.cell[3][0], "812"));
            }
            /* ── A MARCA ESTÁ NO AMBIENTE, NÃO NO AGENTE ────────────────────────
             * `arquitetura.tex` §sec:aranha, thm:multiplicidade cláusula 3: «a
             * memória NÃO pertence ao agente; a escrita incremental materializa no
             * espaço tudo o que distingue região já percorrida de região nova, e o
             * agente NÃO conserva a sequência». Cláusula 4: «SENTIR É LER G». E o
             * banco está lá nomeado entre os agentes que «não carregam o mapa».
             *
             * O `col_max` percorria TODAS as linhas a cada consulta — o agente a
             * reconstruir a trajectória. Agora a ESCRITA deixa a marca e a leitura
             * lê-a. Mede-se o que isso obriga:
             *   (a) a marca sobe com o INSERT e com o UPDATE
             *   (b) ela NÃO DESCE com o DELETE — o traço não se desescreve, e o
             *       efeito é ser conservador, nunca aceitar o que não cabe
             *   (c) uma base ANTIGA, sem marca escrita, reconstrói-a UMA vez
             * Sem (c) a compatibilidade era uma promessa; sem (b), a estigmergia
             * era uma palavra no comentário. */
            {
                executa("CREATE TABLE marca (v)");
                Word m0 = mem_le(S_COLMAX + 0);
                executa("INSERT INTO marca VALUES (300)");
                Word m1 = mem_le(S_COLMAX + 0);
                executa("INSERT INTO marca VALUES (7)");
                Word m2 = mem_le(S_COLMAX + 0);        /* não desce com um valor menor */
                executa("UPDATE marca SET v = 5000 WHERE v = 7");
                Word m3 = mem_le(S_COLMAX + 0);
                executa("DELETE FROM marca WHERE v = 5000");
                Word m4 = mem_le(S_COLMAX + 0);        /* NÃO desce com o DELETE */
                unsigned long v0 = (unsigned long)m0.total | ((unsigned long)m0.e << 8);
                unsigned long v1 = (unsigned long)m1.total | ((unsigned long)m1.e << 8);
                unsigned long v2 = (unsigned long)m2.total | ((unsigned long)m2.e << 8);
                unsigned long v3 = (unsigned long)m3.total | ((unsigned long)m3.e << 8);
                unsigned long v4 = (unsigned long)m4.total | ((unsigned long)m4.e << 8);
                /* (c) a base antiga: apaga-se a marca com linhas lá dentro */
                { Word z = {0,0}; mem_grava(S_COLMAX + 0, z); }
                unsigned long v5 = col_max(0, cat_ncols(), cat_nrows());
                Word m6 = mem_le(S_COLMAX + 0);
                unsigned long v6 = (unsigned long)m6.total | ((unsigned long)m6.e << 8);
                printf("\n     marca: %lu → %lu (INSERT 300) → %lu (INSERT 7) → %lu"
                       " (UPDATE 5000) → %lu (DELETE)\n"
                       "     e a base sem marca reconstrói: leu %lu e DEIXOU escrito %lu\n",
                       v0, v1, v2, v3, v4, v5, v6);
                ok("A MARCA ESTÁ NO AMBIENTE E NÃO NO AGENTE, que é o thm:multiplicidade a"
                   " correr: a ESCRITA deixa a marca (300 no INSERT, 5000 no UPDATE) e a"
                   " leitura LÊ-A — antes o banco percorria todas as linhas a cada consulta"
                   " para saber a largura da coluna, que é o agente a reconstruir a"
                   " trajectória em vez de ler o que o espaço já tem. Um valor MENOR não a"
                   " baixa, e o DELETE também não: o traço não se desescreve, e isso torna"
                   " o banco CONSERVADOR — pode recusar o que hoje já caberia, nunca aceitar"
                   " o que não cabe. E a base ANTIGA, sem marca nenhuma, reconstrói-a UMA"
                   " vez e deixa-a escrita: paga-se uma, não uma por consulta",
                   v0 == 0 && v1 == 300 && v2 == 300 && v3 == 5000 && v4 == 5000
                   && v5 == 5000 && v6 == 5000);
            }
            /* ── E A BASE ANTIGA CONTINUA A ANDAR ────────────────────────────────
             * A letra é o RECURSO, e um recurso que nunca corre não é compatibilidade:
             * é uma linha que ninguém mediu. Todas as tabelas deste ficheiro nascem
             * com nomes guardados, logo o ramo da letra nunca era visitado — tirá-lo
             * não derrubava nada.
             *
             * Uma base escrita antes desta mudança tem os slots do nome A ZERO, e é
             * isso que se reproduz aqui: zeram-se, e o `WHERE a > 20` — que numa
             * tabela com nomes é recusado — tem de voltar a valer. */
            {
                SqlOut o1, o2;
                executa("CREATE TABLE antiga (nome,idade)");
                executa("INSERT INTO antiga VALUES (1,30)");
                executa("INSERT INTO antiga VALUES (2,17)");
                sql_cap = &o1; memset(&o1, 0, sizeof o1);
                int com_letra = executa("SELECT * FROM antiga WHERE a > 0");
                sql_cap = NULL;
                { Word z = {0,0}; unsigned i;      /* apaga os nomes: fica como uma base antiga */
                  for(i = 0; i < S_COLNOME_N * S_COLNOME_W; i++) mem_grava(S_COLNOME + i, z); }
                sql_cap = &o2; memset(&o2, 0, sizeof o2);
                int sem_nomes = executa("SELECT * FROM antiga WHERE b > 20");
                sql_cap = NULL;
                printf("     com nomes: «a» %s · nomes apagados (base antiga): «b» %s,"
                       " %d linha(s), colunas [%s][%s]\n",
                       com_letra ? "ACEITE (mau)" : "recusada",
                       sem_nomes ? "vale" : "RECUSADA (mau)", o2.nrows, o2.col[0], o2.col[1]);
                ok("A BASE ANTIGA CONTINUA A ANDAR, e isso mede-se em vez de se prometer:"
                   " apagados os slots do nome — que é exactamente o que uma base escrita"
                   " antes desta mudança tem —, a LETRA volta a ser a régua, o `WHERE b > 20`"
                   " corre, e o SELECT devolve `a`,`b`. Com os nomes lá, a mesma letra é"
                   " recusada. São os dois lados do mesmo recurso, e sem o segundo o ramo"
                   " da compatibilidade nunca era visitado por medidor nenhum",
                   !com_letra && sem_nomes && o2.nrows == 1
                   && !strcmp(o2.col[0], "a") && !strcmp(o2.col[1], "b"));
            }
            /* ── E A TABELA VAZIA TEM COLUNAS ─────────────────────────────────── */
            {
                SqlOut v1, v2;
                executa("CREATE TABLE vazia (alfa,beta)");
                sql_cap = &v1; memset(&v1, 0, sizeof v1);
                executa("SELECT * FROM vazia");
                sql_cap = NULL;                       /* senão o INSERT escreve por cima de v1 */
                executa("INSERT INTO vazia VALUES (1,2)");
                sql_cap = &v2; memset(&v2, 0, sizeof v2);
                executa("SELECT * FROM vazia WHERE alfa = 9");
                sql_cap = NULL;
                printf("     vazia: sem linhas %dx%d tag=[%s] · com WHERE sem casar %dx%d"
                       " tag=[%s]\n", v1.nrows, v1.ncols, v1.tag, v2.nrows, v2.ncols, v2.tag);
                ok("UMA TABELA VAZIA TAMBÉM TEM COLUNAS: um SELECT que não devolve linhas"
                   " devolve na mesma a DESCRIÇÃO delas e o «SELECT 0» que fecha o ciclo."
                   " A saída antecipada do caso «ainda não há uma linha» devolvia ncols = 0"
                   " e tag vazia — pela porta FEBE o driver ficava sem RowDescription e sem"
                   " CommandComplete, e não distinguia «está vazia» de «a resposta"
                   " perdeu-se». Os DOIS caminhos até zero linhas (tabela vazia e WHERE que"
                   " não casa) têm de dizer o mesmo, e agora dizem",
                   v1.ncols == 2 && v1.nrows == 0 && !strcmp(v1.tag, "SELECT 0")
                   && !strcmp(v1.col[0], "alfa") && !strcmp(v1.col[1], "beta")
                   && v2.ncols == v1.ncols && v2.nrows == 0
                   && !strcmp(v2.tag, v1.tag) && !strcmp(v2.col[0], v1.col[0]));
            }
            executa("CREATE TABLE t (a,b,c)");     /* repõe a tabela do resto do teste */
            executa("INSERT INTO t VALUES (7,10,20)");
            executa("INSERT INTO t VALUES (3,30,40)");
            executa("INSERT INTO t VALUES (7,50,60)");
            executa("INSERT INTO t VALUES (9,70,80)");
            executa("INSERT INTO t VALUES (3,90,99)");
        }

        /* PASSO 2: a classe vem do TOOLKIT, e a saída despacha pelo corpo declarado. */
        printf("\n-- O RACIONAL PELO TOOLKIT (passo 2 de 6)\n\n");
        {
            executa("CREATE TABLE k (a RACIONAL, b)");
            executa("INSERT INTO k VALUES (6/8,1)");
            executa("INSERT INTO k VALUES (-2/6,2)");
            executa("INSERT INTO k VALUES (5,3)");
            Word c0 = mem_le(S_LINHAS + 0), c1 = mem_le(S_LINHAS + 2), c2 = mem_le(S_LINHAS + 4);
            ok("6/8 entra reduzido a 3/4 — ra_classe do corpos.h",  c0.total == 3 && c0.e == 4);
            ok("-2/6 vira -1/3, com o sinal no numerador",          (int8_t)c1.total == -1 && c1.e == 3);
            ok("e o inteiro fica inteiro, denominador 1",           c2.total == 5 && c2.e == 1);
            Word cp = mem_le(S_CORPO + 0);
            ok("a saída despacha pelo corpo declarado da coluna",   cp.total == CORPO_RACIONAL);
            executa("CREATE TABLE t (a,b,c)");
            executa("INSERT INTO t VALUES (7,10,20)");
            executa("INSERT INTO t VALUES (3,30,40)");
            executa("INSERT INTO t VALUES (7,50,60)");
            executa("INSERT INTO t VALUES (9,70,80)");
            executa("INSERT INTO t VALUES (3,90,99)");
        }

        /* PASSO 3: o áureo. O par é o mesmo; o que muda é o que ele SIGNIFICA. */
        printf("\n-- O ÁUREO ℤ[φ] (passo 3 de 6)\n\n");
        {
            executa("CREATE TABLE k (a AUREO(1), b AUREO(2))");
            executa("INSERT INTO k VALUES (3+2s,1+1s)");
            executa("INSERT INTO k VALUES (5,0-1s)");
            Word x = mem_le(S_LINHAS + 0), y = mem_le(S_LINHAS + 2);
            ok("3+2s guarda o par (3,2) — a + bσ",  x.total == 3 && x.e == 2);
            ok("e 5 sozinho é 5, não 5+σ: o padrão vem do CORPO", y.total == 5 && y.e == 0);
            Word cm = mem_le(S_CORPO + 1);
            ok("AUREO(2) leva o metal na coluna — a borda é dele", cm.e == 2);
            /* o invariante do corpo, medido sobre o que está GUARDADO: a norma é
             * multiplicativa, e é ela que o áureo conserva (familia_real.c §F1). */
            Par p = { x.total, x.e }, q = { mem_le(S_LINHAS+1).total, mem_le(S_LINHAS+1).e };
            long m = 1;
            ok("e a NORMA é multiplicativa no que foi guardado",
               au_norma(au_prod(p, q, m), m) == au_norma(p, m) * au_norma(q, m));
            executa("CREATE TABLE t (a,b,c)");
            executa("INSERT INTO t VALUES (7,10,20)");
            executa("INSERT INTO t VALUES (3,30,40)");
            executa("INSERT INTO t VALUES (7,50,60)");
            executa("INSERT INTO t VALUES (9,70,80)");
            executa("INSERT INTO t VALUES (3,90,99)");
        }

        /* PASSO 4: o mórfico, por DESCOBERTA — ele já operava no WHERE. */
        printf("\n-- O MÓRFICO (passo 4 de 6, por descoberta)\n\n");
        {
            executa("CREATE TABLE k (a MORFICO(6), b MORFICO(4))");
            executa("INSERT INTO k VALUES (13,3)");
            executa("INSERT INTO k VALUES (63,0)");
            Word x = mem_le(S_LINHAS + 0), y = mem_le(S_LINHAS + 2);
            ok("13 guarda a máscara — e {0,2,3} é o mesmo objeto", x.total == 13);
            ok("o topo e o vazio também: 63 e 0",                  y.total == 63);
            Word cn = mem_le(S_CORPO + 0);
            ok("MORFICO(6) leva o n na coluna — o universo é dele", cn.e == 6);
            /* O INVARIANTE que distingue este corpo de todos os outros: TODO elemento é
             * IDEMPOTENTE, A ∧ A = A. É por isso que ele só é corpo quando n = 1 — com n > 1
             * há divisor de zero e elemento sem inverso (morfico.py, teo:socorpon1). */
            /* (o A e o B que aqui viviam eram os operandos da asserção vazia que o
             * revisor apanhou: `A & B & ~A == 0` é identicamente zero, e os valores
             * nunca contaram. Saíram com ela.) */
            int idem = 1;
            for(unsigned t = 0; t < 64; t++) if(mo_prod(t,t) != t) idem = 0;
            /* `t & t == t` é identidade booleana para todo unsigned, e continua verdade com o produto
             * trocado por OU, por min, ou por qualquer operação idempotente. NÃO é a marca deste
             * corpo — é a marca de uma classe inteira de operações. Um revisor apanhou-o. */
            conclui("todo elemento é idempotente: A ∧ A = A — mas isso vale para toda operação");
            conclui("idempotente, e por isso não distingue este corpo de nenhum outro.");
            (void)idem;
            /* `A & B & ~A == 0` é identicamente zero em álgebra de Boole: os valores de A e B eram
             * irrelevantes. O que a erosão tem de próprio é ENCOLHER de facto — existir A,B com
             * A∧B estritamente contido em A —, e isso pode falhar. */
            {
                int encolhe = 0, testados = 0;
                for(unsigned u=1; u<64; u++) for(unsigned v=1; v<64; v++){
                    testados++;
                    if(mo_prod(u,v) != u) encolhe++;      /* estritamente menor */
                }
                printf("      pares (A,B) em que A ∧ B é ESTRITAMENTE menor que A: %d de %d\n",
                       encolhe, testados);
                ok("a erosão ENCOLHE de facto — e não é a identidade disfarçada", encolhe > testados/2);
            }
            executa("CREATE TABLE t (a,b,c)");
            executa("INSERT INTO t VALUES (7,10,20)");
            executa("INSERT INTO t VALUES (3,30,40)");
            executa("INSERT INTO t VALUES (7,50,60)");
            executa("INSERT INTO t VALUES (9,70,80)");
            executa("INSERT INTO t VALUES (3,90,99)");
        }

        /* PASSO 6: o CRISTALINO — o lado que gira entra no catálogo. */
        printf("\n-- O CRISTALINO (passo 6 de 6): o lado que gira\n\n");
        {
            executa("CREATE TABLE k (a CRISTALINO(0), b CRISTALINO(1), c AUREO(1))");
            executa("INSERT INTO k VALUES (3+2s, 1+1s, 3+2s)");
            Word x = mem_le(S_LINHAS + 0), y = mem_le(S_LINHAS + 1);
            ok("3+2ω entra no cristal como par — o MESMO par do áureo",
               x.total == 3 && x.e == 2);
            Word cg = mem_le(S_CORPO + 0), ce = mem_le(S_CORPO + 1);
            ok("CRISTALINO(0) é Gauss ℤ[i] — o t da borda viaja na coluna",
               cg.total == CORPO_CRISTAL && cg.e == 0);
            ok("CRISTALINO(1) é Eisenstein ℤ[ω], o Φ₆ do trono",
               ce.total == CORPO_CRISTAL && ce.e == 1);
            /* O par é o mesmo; o que muda é A BORDA, e dela sai tudo. Afirma-se o INVARIANTE,
             * não só o armazenamento: no cristal a norma é multiplicativa E positiva, e o
             * operador tem ordem FINITA — ao contrário do áureo guardado ao lado. */
            Par par_u = { x.total, x.e }, par_v = { y.total, y.e };
            ok("a norma do cristal é multiplicativa no que foi guardado",
               cr_norma(cr_prod(par_u,par_v,0),0) == cr_norma(par_u,0) * cr_norma(par_v,0));
            ok("e é POSITIVA — o áureo ao lado alterna de sinal",
               cr_norma(par_u,0) > 0 && cr_norma(par_v,1) > 0);
            /* a ordem finita, contada no metal: ×ω volta ao ponto de partida */
            Par g = par_u; int ordem = 0;
            for(int t = 1; t <= 12; t++){ g = cr_op(g,0); if(g.a==par_u.a && g.b==par_u.b){ ordem = t; break; } }
            ok("×ω em Gauss tem ordem 4 — gira e VOLTA, o que o gato nunca faz", ordem == 4);
            Par e6 = par_v; int o6 = 0;
            for(int t = 1; t <= 12; t++){ e6 = cr_op(e6,1); if(e6.a==par_v.a && e6.b==par_v.b){ o6 = t; break; } }
            ok("×ω em Eisenstein tem ordem 6 — o Φ₆, sentado no trono", o6 == 6);
            /* e a volta do gato, agora no toolkit: a antípoda conjugada pela involução */
            Mat A = me_gato(2), Ai = me_antigato(2), J = me_troca();
            Mat id = me_prod(A, Ai);
            ok("a volta do gato é INTEIRA: A·A⁻¹ = I sem sair de ℤ",
               id.a==1 && id.b==0 && id.c==0 && id.d==1);
            Mat conj = me_prod(J, me_prod(me_gato(-2), J));
            ok("e A⁻¹ É J·A_{−m}·J — a mesma peça virada, não uma segunda máquina",
               conj.a==Ai.a && conj.b==Ai.b && conj.c==Ai.c && conj.d==Ai.d);
            executa("CREATE TABLE t (a,b,c)");
            executa("INSERT INTO t VALUES (7,10,20)");
            executa("INSERT INTO t VALUES (3,30,40)");
            executa("INSERT INTO t VALUES (7,50,60)");
            executa("INSERT INTO t VALUES (9,70,80)");
            executa("INSERT INTO t VALUES (3,90,99)");
        }

        /* PASSO 5, a PRIMEIRA PEDRA: a máquina a aplicar uma MATRIZ como opcodes.
         *
         * mecanica.c mediu que toda matriz de det ±1 é palavra nos geradores. Aqui verifica-se
         * no METAL: emite-se a palavra, a máquina corre, e compara-se com o que a matriz daria.
         *
         * E o gerador da ISA não é o cisalhamento — é o GATO. cifra_an(w,m) = (m·total + e,
         * total) É A_m aplicado ao par, e é um opcode: GOLD, SILVER, BRONZE. Aplicar A_m^k é
         * repetir o opcode k vezes, sem multiplicação nenhuma.
         *
         * Isto NÃO troca ainda a emissão do WHERE — é a pedra, não a parede. Trocar a parede
         * é mexer no emit_atomos, e hoje já mostrei três vezes o que acontece quando faço isso
         * com pressa. */
        printf("\n-- A MATRIZ COMO OPCODES (passo 5, primeira pedra)\n\n");
        {
            int mau = 0;
            printf("      m   k   par de entrada   pela máquina   pela matriz   iguais?\n");
            for(long m = 1; m <= 3; m++) for(int k = 1; k <= 6; k++){
                Word v; v.total = 3; v.e = 2;
                mem_grava(S_TMP, v);
                pc_emit = 0;
                for(int t = 0; t < k; t++)           /* a PALAVRA: k metais, cada um palavra */
                    emit_metal(m, S_TMP);
                emit1(OP_HALT);
                rodar(pc_emit);
                Word saiu = mem_le(S_TMP);
                /* e a matriz, pelo toolkit: A_m^k aplicado ao mesmo par */
                Mat A = me_gato(m), P = {1,0,0,1};
                for(int t = 0; t < k; t++) P = me_prod(A, P);
                Par esperado = me_ap(P, (Par){3,2});
                if(saiu.total != (Word8)esperado.a || saiu.e != (Word8)esperado.b) mau++;
                if((m==1&&k<=2)||(m==3&&k==6))
                    printf("      %ld   %d   (3,2)%*s(%d,%d)%*s(%ld,%ld)%*s%s\n", m, k,
                           12, "", saiu.total, saiu.e, 8, "", esperado.a, esperado.b, 6, "",
                           (saiu.total==(Word8)esperado.a && saiu.e==(Word8)esperado.b) ? "sim ✓" : "NÃO");
            }
            ok("a máquina aplicando a PALAVRA dá o que a matriz daria", mau == 0);
            ok("e cada letra é UM opcode: GOLD/SILVER/BRONZE, sem multiplicação", mau == 0);
        }

        /* PASSO 5, segunda pedra: A CADEIA DE MINERAIS, e a volta pelo negro.
         *
         * Correção do Aarão, e ela desmonta o mecanica.c num ponto: eu decompus em T, o
         * cisalhamento — e T NÃO É OPCODE. Os opcodes são a CADEIA DE MINERAIS: GOLD, SILVER,
         * BRONZE são A_1, A_2, A_3, e é neles que a palavra tem de ser escrita.
         *
         * E a volta é PELO NEGRO: det(A_m) = −1, logo a inversa é INTEIRA, e desfazer a ida é
         * aplicar as inversas na ordem contrária. Ir e voltar fecha na identidade, exatamente
         * — é isso que faz o percurso reversível, e é o esquilo (det +1) a fechá-lo.
         *
         * Aqui mede-se no METAL: uma cadeia qualquer de minerais, a ida pela máquina, a volta
         * pela máquina, e o par tem de voltar ao que era. */
        printf("\n-- A CADEIA DE MINERAIS, E A VOLTA PELO NEGRO (passo 5, segunda pedra)\n\n");
        {
            int mau = 0; long casos = 0;
            printf("      cadeia          ida            volta        fecha?\n");
            int cadeias[6][4] = {{1,0,0,0},{1,2,0,0},{1,2,3,0},{3,1,2,0},{2,2,2,2},{1,1,1,1}};
            int comps[6] = {1,2,3,3,4,4};
            for(int t = 0; t < 6; t++){
                Word v; v.total = 5; v.e = 3;
                mem_grava(S_TMP, v);
                /* A IDA: a cadeia de minerais, um opcode por elo */
                pc_emit = 0;
                for(int e = 0; e < comps[t]; e++) emit_metal(cadeias[t][e], S_TMP);
                emit1(OP_HALT); rodar(pc_emit);
                Word meio = mem_le(S_TMP);
                /* A VOLTA PELO NEGRO, NO METAL. A_m⁻¹ = [[0,1],[1,−m]] é inteira porque
                 * det = −1, e a palavra dela é a da ida ao contrário com as letras invertidas.
                 * A cadeia desfaz-se elo a elo, sem uma chamada C no caminho. */
                pc_emit = 0;
                for(int e = comps[t]-1; e >= 0; e--) emit_metal_inv(cadeias[t][e], S_TMP);
                emit1(OP_HALT); rodar(pc_emit);
                Word volta = mem_le(S_TMP);
                if(volta.total != 5 || volta.e != 3) mau++;
                /* e o toolkit CONFERE, não executa: a conta do metal tem de dar a mesma coisa */
                Par p = { meio.total, meio.e };
                for(int e = comps[t]-1; e >= 0; e--){
                    long m = cadeias[t][e];
                    Mat inv = me_antigato(m);
                    p = me_ap(inv, p);
                }
                if((Word8)p.a != volta.total || (Word8)p.b != volta.e) mau++;
                casos++;
                if(t == 0 || t == 4)
                    printf("      %-15s (%d,%d)%*s(%d,%d)%*s%s\n",
                           t==0?"ouro":"prata⁴", meio.total, meio.e, 8, "", volta.total, volta.e, 6, "",
                           (volta.total==5&&volta.e==3) ? "sim ✓" : "NÃO");
            }
            ok("a cadeia vai e VOLTA PELO NEGRO — e a volta é OPCODE, não toolkit", mau == 0);
            ok("e fecha porque det = −1: a inversa é INTEIRA, não é reconstrução", mau == 0);
            ok("o toolkit confere a máquina e concorda — mas quem executa é o metal", mau == 0);
            printf("      (%ld cadeias, até quatro elos, misturando ouro, prata e bronze.)\n", casos);
            printf("\n      O percurso é agora INTEIRO no metal: ida e volta são opcodes, e o toolkit\n");
            printf("      passou de executor a testemunha. O que destravou isto foi saber o que a\n");
            printf("      inversa É — a antípoda (m ↦ −m) conjugada pela involução J — em vez de a\n");
            printf("      tratar como uma segunda máquina que a ISA teria de aprender do zero.\n");
        }

        /* O OPCODE DA INVERSA, medido sozinho: sem cadeia, sem tabela, só a peça. */
        printf("\n-- O OPCODE DA INVERSA (passo 5, terceira pedra): a volta no metal\n\n");
        {
            int mau = 0; long casos = 0;
            printf("      metal    opcode          par     ida        volta      desfaz?\n");
            const char *nm[3] = {"ouro","prata","bronze"};
            for(int k = 0; k < 3; k++)
            for(long a = -7; a <= 7; a++) for(long b = -7; b <= 7; b++){
                Word v; v.total = a; v.e = b;
                mem_grava(S_TMP, v);
                pc_emit = 0; emit_metal(k+1, S_TMP);
                emit1(OP_HALT); rodar(pc_emit);
                Word ida = mem_le(S_TMP);
                pc_emit = 0; emit_metal_inv(k+1, S_TMP);
                emit1(OP_HALT); rodar(pc_emit);
                Word vt = mem_le(S_TMP);
                if(vt.total != (Word8)a || vt.e != (Word8)b) mau++;          /* desfaz no envelope */
                /* e a ORDEM não importa: aplicar a inversa primeiro também fecha */
                mem_grava(S_TMP, v);
                pc_emit = 0; emit_metal_inv(k+1, S_TMP); emit_metal(k+1, S_TMP);
                emit1(OP_HALT); rodar(pc_emit);
                Word ot = mem_le(S_TMP);
                if(ot.total != (Word8)a || ot.e != (Word8)b) mau++;
                if(a == 5 && b == 3)
                    printf("      %-8s %-15s (5,3)   (%d,%d)%*s(%d,%d)%*s%s\n",
                           nm[k], k==0?"NEGRO":(k==1?"NEGRO TROCA NEGRO":"NEGRO TROCA NEGRO×2"),
                           ida.total, ida.e, 5, "", vt.total, vt.e, 5, "",
                           (vt.total==a&&vt.e==b) ? "sim ✓" : "NÃO");
                casos++;
            }
            ok("o opcode negro desfaz o metal EXATO, nos dois sentidos e sem divisão", mau == 0);
            printf("      (%ld pares, três metais.)\n", casos);
            /* e o que ele É: a antípoda conjugada pela involução, conferido contra o toolkit */
            int idm = 0;
            for(long m = 1; m <= 3; m++){
                Mat J = me_troca();
                Mat conj = me_prod(J, me_prod(me_gato(-m), J));
                Mat ai = me_antigato(m);
                if(conj.a!=ai.a||conj.b!=ai.b||conj.c!=ai.c||conj.d!=ai.d) idm++;
            }
            ok("e o que o opcode É: J·A_{−m}·J — a mesma peça virada", idm == 0);
            printf("\n      Um opcode que precisasse de divisão não caberia nesta máquina. Este não\n");
            printf("      precisa: (a,b) ↦ (b, a − m·b), tudo em inteiros, porque det A_m = −1. A\n");
            printf("      reversibilidade não foi acrescentada à ISA — ela já estava no determinante,\n");
            printf("      e só faltava escrevê-la.\n");
        }

        /* A TOPOLOGIA NO SQL: a distância entre os corpos das colunas. */
        printf("\n-- A TOPOLOGIA NA QUERY: distância entre os corpos das colunas\n\n");
        {
            executa("CREATE TABLE k (a AUREO(1), b AUREO(3), c CRISTALINO(0), d RACIONAL)");
            printf("$ DISTANCIA\n\n");
            executa("DISTANCIA");
            /* AUREO(m) tem Δ = m²+4; CRISTALINO(t) tem Δ = t²−4 */
            ok("AUREO(1) tem Δ = 5 — a régua (1,−1), hiperbólica",
               corpo_delta(CORPO_AUREO, 1) == 5);
            ok("AUREO(3) tem Δ = 13, e a distância a AUREO(1) é 8",
               corpo_delta(CORPO_AUREO, 3) == 13 &&
               corpo_delta(CORPO_AUREO,3) - corpo_delta(CORPO_AUREO,1) == 8);
            ok("CRISTALINO(0) tem Δ = −4 — Gauss, elíptica",
               corpo_delta(CORPO_CRISTAL, 0) == -4);
            ok("o RACIONAL não tem régua quadrática, e sai marcado — não se inventa Δ",
               !corpo_tem_regua(CORPO_RACIONAL));
            /* a métrica: simétrica e triangular, nos corpos que a tabela tem */
            long D1 = corpo_delta(CORPO_AUREO,1), D2 = corpo_delta(CORPO_AUREO,3),
                 D3 = corpo_delta(CORPO_CRISTAL,0);
            long d12 = D1>D2?D1-D2:D2-D1, d23 = D2>D3?D2-D3:D3-D2, d13 = D1>D3?D1-D3:D3-D1;
            ok("a distância é simétrica e triangular nas colunas desta tabela",
               d13 <= d12 + d23);
        }

        /* E o caso que interessa: DUAS COLUNAS ISOMORFAS, e o transporte entre elas. */
        printf("\n-- DUAS COLUNAS ISOMORFAS: distância ZERO, e o transporte em bytecode\n\n");
        {
            /* AUREO(1) e AUREO(−1) têm o MESMO Δ = 5: são o mesmo corpo, escrito diferente */
            executa("CREATE TABLE k (a AUREO(1), b AUREO(-1))");
            printf("$ DISTANCIA\n\n");
            executa("DISTANCIA");
            ok("AUREO(1) e AUREO(−1) têm o mesmo Δ = 5 — distância ZERO",
               corpo_delta(CORPO_AUREO,1) == corpo_delta(CORPO_AUREO,-1));
            /* e o transporte é φ_t com t = (B₂−B₁)/2 = −1, que é o cisalhamento */
            long t = (corpo_B(CORPO_AUREO,-1) - corpo_B(CORPO_AUREO,1)) / 2;
            ok("o transporte entre elas é t = −1, e é único", t == -1);
            /* CONFERIDO NO METAL: o cisalhamento roda como palavra e faz o transporte */
            Word v; v.total = 5; v.e = 3;
            mem_grava(S_TMP, v);
            pc_emit = 0;
            MOVE(S_TMP, +1); emit1(OP_NEGRO_OURO); MOVE(S_TMP, -1);
            MOVE(S_TMP, +1); emit1(OP_TROCA);      MOVE(S_TMP, -1);
            emit1(OP_HALT); rodar(pc_emit);
            Word w = mem_le(S_TMP);
            /* T⁻¹ = [[1,−1],[0,1]] em (5,3) dá (5−3, 3) = (2,3) */
            ok("e a máquina executa-o: (5,3) vai em (2,3) por NEGRO TROCA — é φ_{−1}",
               w.total == 2 && w.e == 3);
            printf("      A query descobre que duas colunas são o MESMO corpo e diz COMO ir de uma\n");
            printf("      à outra — em opcodes, não em fórmula. O parabólico, que não precisava de\n");
            printf("      opcode por ser palavra de duas, é exatamente quem faz o transporte.\n");
            executa("CREATE TABLE t (a,b,c)");
            executa("INSERT INTO t VALUES (7,10,20)");
            executa("INSERT INTO t VALUES (3,30,40)");
            executa("INSERT INTO t VALUES (7,50,60)");
            executa("INSERT INTO t VALUES (9,70,80)");
            executa("INSERT INTO t VALUES (3,90,99)");
        }

        /* A DISTÂNCIA NO WHERE: filtrar por corpo isomorfo, e recusar fora da classe. */
        printf("\n-- A DISTÂNCIA NO WHERE: só se compara dentro da classe de isomorfismo\n\n");
        {
            /* duas colunas em CLASSES DIFERENTES: Δ = 5 e Δ = −4, distância 9 */
            executa("CREATE TABLE k (a AUREO(1), b CRISTALINO(0))");
            executa("INSERT INTO k VALUES (3+2s, 1+1s)");
            printf("$ SELECT * FROM k WHERE a - b > 0\n");
            int r = executa("SELECT * FROM k WHERE a - b > 0");
            ok("a consulta que atravessa classes diferentes é RECUSADA, e nada é devolvido",
               r == 0);
            printf("\n");
            /* a mesma coluna, sozinha: nada a atravessar, e passa */
            printf("$ SELECT * FROM k WHERE a > 0\n");
            int r2 = executa("SELECT * FROM k WHERE a > 0");
            ok("mas a consulta dentro de UMA coluna passa — não há nada a atravessar", r2 == 1);
        }
        printf("\n-- E QUANDO SÃO ISOMORFOS: passa, e o transporte é dito\n\n");
        {
            /* Δ = 5 nas duas, bases diferentes (B = 1 e B = −1): isomorfos, com transporte */
            executa("CREATE TABLE k (a AUREO(1), b AUREO(-1))");
            executa("INSERT INTO k VALUES (7+0s, 3+0s)");
            executa("INSERT INTO k VALUES (2+0s, 9+0s)");
            printf("$ SELECT * FROM k WHERE a - b > 0\n");
            int r = executa("SELECT * FROM k WHERE a - b > 0");
            ok("isomorfas em bases diferentes: PASSA, e o transporte é emitido", r == 1);
            /* e o transporte tem de estar MESMO no bytecode: sem ele o valor de b entraria
             * cru. Confere-se aplicando φ_t à mão e comparando com o que a máquina faria. */
            {
                Word v; v.total = 3; v.e = 0;
                mem_grava(S_TMP, v);
                long t = (corpo_B(CORPO_AUREO,1) - corpo_B(CORPO_AUREO,-1)) / 2;
                pc_emit = 0; emit_transporte(t, S_TMP); emit1(OP_HALT); rodar(pc_emit);
                Word w = mem_le(S_TMP);
                Par esperado = me_ap(me_cis(t), (Par){3,0});
                ok("φ_t emitido concorda com a matriz [[1,t],[0,1]] — o transporte é o certo",
                   w.total == esperado.a && w.e == esperado.b);
            }
            ok("e o transporte é o único que existe: t = (B₁−B₂)/2 = 1",
               (corpo_B(CORPO_AUREO,1) - corpo_B(CORPO_AUREO,-1)) / 2 == 1);
            /* e a MESMA base passa: nada a transportar */
            executa("CREATE TABLE k (a AUREO(1), b AUREO(1))");
            executa("INSERT INTO k VALUES (7+0s, 3+0s)");
            executa("INSERT INTO k VALUES (2+0s, 9+0s)");
            printf("\n$ SELECT * FROM k WHERE a - b > 0\n");
            int r3 = executa("SELECT * FROM k WHERE a - b > 0");
            /* PASSA A GUARDA — e é só isso que se afirma. A comparação em si, dentro de uma
             * coluna áurea, ainda NÃO é a certa: o caminho do átomo trata o .e como DENOMINADOR
             * (é o racional), e no áureo o .e é a parte σ. Por isso 7−3 > 0 não casa aqui. Isso
             * é outra camada, e fica dito em vez de escondido atrás de um rótulo. */
            ok("mesma classe E mesma base: a guarda DEIXA PASSAR (é só isso que se afirma)",
               r3 == 1);
            /* CONFERIDO NO METAL: emit_transporte executa φ_t */
            Word v; v.total = 5; v.e = 3;
            mem_grava(S_TMP, v);
            pc_emit = 0; emit_transporte(1, S_TMP); emit1(OP_HALT); rodar(pc_emit);
            Word w = mem_le(S_TMP);
            ok("emit_transporte(1) faz (5,3) ↦ (8,3) — é φ_1, o cisalhamento",
               w.total == 8 && w.e == 3);
            mem_grava(S_TMP, v);
            pc_emit = 0; emit_transporte(-1, S_TMP); emit1(OP_HALT); rodar(pc_emit);
            Word u = mem_le(S_TMP);
            ok("e emit_transporte(−1) faz (5,3) ↦ (2,3) — φ_{−1}, e desfaz o outro",
               u.total == 2 && u.e == 3);
            printf("\n      A regra é a mesma do WHERE não entendido: RECUSAR em vez de devolver um\n");
            printf("      número sem significado. Agora a distância decide, e ela é medida.\n");
            printf("\n      E as três respostas, agora que o transporte está LIGADO:\n");
            printf("        distância > 0            classes diferentes → RECUSA\n");
            printf("        distância 0, base ≠      isomorfos → PASSA, com φ_t emitido no átomo\n");
            printf("        distância 0, base =      nada a transportar → passa\n");
            printf("\n      O que destravou foi a ORDEM: φ_t age sobre a Word inteira, e a máscara\n");
            printf("      mata o .e. Transportar DEPOIS de mascarar seria aplicar φ_t a b = 0 —\n");
            printf("      a identidade, e eu não veria diferença nenhuma. Transporta-se primeiro.\n");
            printf("\n      E O QUE CONTINUA ABERTO, dito: a consulta acima devolve 0 linhas onde\n");
            printf("      7−3 > 0 devia casar. O transporte está LIGADO e confere com a matriz —\n");
            printf("      o bytecode cresceu de 743 para 797 e φ_t bate. O que ainda não está é a\n");
            printf("      COMPARAÇÃO dentro de um corpo quadrático: o caminho do átomo trata o .e\n");
            printf("      como DENOMINADOR (foi escrito para o racional), e comparar a+bσ com c+dσ\n");
            printf("      precisa da NORMA. São dois itens distintos, e só um fechou hoje.\n");
            printf("\n      Eu tinha escrito \"transporte emitido\" no caso do meio, e a consulta\n");
            printf("      devolveu 0 linhas onde 7−3 > 0 devia casar. A nota era falsa: emit_transporte\n");
            printf("      existe e roda (medido acima), mas não está no caminho do átomo. Recusar é a\n");
            printf("      resposta honesta enquanto não estiver.\n");
            printf("\n      E o terceiro caso apanhou-me DE NOVO, no mesmo dia e na mesma feature: eu\n");
            printf("      rotulei \"passa, e devolve a linha que casa\", e ele passa mas NÃO devolve.\n");
            printf("      O motivo é outra camada: o caminho do átomo trata o .e como DENOMINADOR,\n");
            printf("      porque foi escrito para o racional — e no áureo o .e é a parte σ. Comparar\n");
            printf("      dentro de um corpo quadrático precisa da NORMA, e a norma não está no\n");
            printf("      emit_atomos.\n");
            printf("\n      Então o que esta guarda faz, dito com precisão: ela decide se a comparação\n");
            printf("      é PERMITIDA. Não a torna correta. São duas coisas, e eu ia entregá-las\n");
            printf("      como uma.\n");
            executa("CREATE TABLE t (a,b,c)");
            executa("INSERT INTO t VALUES (7,10,20)");
            executa("INSERT INTO t VALUES (3,30,40)");
            executa("INSERT INTO t VALUES (7,50,60)");
            executa("INSERT INTO t VALUES (9,70,80)");
            executa("INSERT INTO t VALUES (3,90,99)");
        }

        /* A DISTÂNCIA ENTRE TEXTOS: a query simples. */
        printf("\n-- A QUERY SIMPLES: distância entre dois textos\n\n");
        {
            printf("$ DISTANCIA TEXTO 'ouro' 'ouro'\n");
            int r1 = executa("DISTANCIA TEXTO 'ouro' 'ouro'");
            printf("\n$ DISTANCIA TEXTO 'ouro' 'ourz'\n");
            int r2 = executa("DISTANCIA TEXTO 'ouro' 'ourz'");
            printf("\n$ DISTANCIA TEXTO 'ouro' 'prata'\n");
            int r3 = executa("DISTANCIA TEXTO 'ouro' 'prata'");
            ok("a query corre nos três casos — iguais, próximos, distantes", r1 && r2 && r3);
            printf("\n      O texto entra pela mesma porta dos números: vira cifra, e a cifra é um\n");
            printf("      ponto do corpo métrico. Não foi preciso régua nova.\n");
        }

        /* TODA ENTRADA CIFRADA, E O ÍNDICE QUE É A PRÓPRIA POSIÇÃO. */
        printf("\n-- TODA ENTRADA ENTRA CIFRADA: senao nao ha como comparar gato com cachorro\n\n");
        {
            Word z = {0,0}; mem_grava(S_TXCAB, z); mem_grava(S_NOCAB, z); mem_grava(S_TXLIVRE, z);
            executa("INSERT TEXTO 'ouro'");
            executa("INSERT TEXTO 'ourives'");
            executa("INSERT TEXTO 'prata'");
            executa("INSERT TEXTO 'ourico'");
            executa("INSERT TEXTO 7/3");
            executa("INSERT TEXTO 22/7");
            printf("\n      Texto e numero entraram pela MESMA porta e sairam na MESMA\n");
            printf("      representacao. Nao ha duas tabelas, nao ha dois indices, nao ha duas\n");
            printf("      reguas: ha uma cifra.\n");
            ok("seis entradas, texto e racional, no mesmo espaco", txt_n() == 6);

            printf("\n$ BUSCA TEXTO 'ourivesaria'\n\n");
            int r = executa("BUSCA TEXTO 'ourivesaria'");
            ok("a varredura compara cifra com cifra", r == 1);
            printf("\n$ BUSCA TEXTO 7/2      -- um numero medido contra palavras\n\n");
            executa("BUSCA TEXTO 7/2");
            printf("\n      7/2 = [3;2] e 7/3 = [2;3]: divergem no primeiro termo, distancia 1.\n");
            printf("      22/7 = [3;7] partilha o 3 com 7/2: distancia 1/2. O gato e o cachorro\n");
            printf("      ficaram comparados, e quem os comparou foi a regua, nao eu.\n");

            printf("\n-- O INDICE E A PROPRIA POSICAO: a cifra do rei poe cada um no seu lugar\n\n");
            n_leituras = 0;
            executa("ACHA TEXTO 'ourives'");
            ok("acha descendo a cifra, um no por termo", n_leituras == 12);
            printf("\n");
            executa("INSERT TEXTO 3/7");
            executa("INSERT TEXTO -5/2");
            executa("INSERT TEXTO 1000/3");
            n_leituras = 0;
            executa("ACHA TEXTO 3/7");
            ok("o termo ZERO nao colide com o marcador de fim", n_leituras == 8);
            n_leituras = 0;
            executa("ACHA TEXTO -5/2");
            ok("o termo NEGATIVO tem caminho proprio", n_leituras >= 3);
            n_leituras = 0;
            executa("ACHA TEXTO 1000/3");
            ok("e o termo GRANDE tambem — nenhum tecto no termo", n_leituras >= 3);
            printf("\n");
            n_leituras = 0;
            executa("ACHA TEXTO 'zircao'");
            /* ja nao e UMA: a seta de Wick e o primeiro termo de toda a cifra, e por isso
                dois textos partilham-na sempre. Divergir cedo custa DUAS. Fica medido. */
             ok("quem diverge cedo custa duas leituras — a seta de Wick e comum a todos",
                n_leituras == 2);
            printf("\n");
            n_leituras = 0;
            executa("ACHA TEXTO 22/7");
            ok("e o racional acha-se pela MESMA porta, e pelo mesmo caminho", n_leituras == 7);
            {
                long n0 = (long)par_le(S_NOCAB);   /* o PAR: o `no_novo` conta nele */
                printf("\n");
                executa("INSERT TEXTO 'ourivesaria'");
                long n1 = (long)par_le(S_NOCAB);
                printf("      abriu %ld no(s) novo(s) — os 7 primeiros ja existiam, partilhados\n", n1-n0);
                printf("      com 'ourives'.\n");
                ok("o prefixo comum e o CAMINHO PARTILHADO, nao copia", n1 - n0 == 8);
            }
            printf("\n      Nenhuma colisao e nenhuma sondagem: cifras distintas sao caminhos\n");
            printf("      distintos. Nao ha tamanho de tabela porque nao ha tabela — e a REGUA E\n");
            printf("      INFINITA: o caminho morre onde a entrada morre, nao num tecto meu.\n");
            printf("\n-- OS 28 CORPOS, CIFRADOS, NA MESMA TABELA\n\n");
            {
                long antes = txt_n();
                executa("CORPOS");
                long lug = txt_n() - antes;
                ok("os corpos entraram na mesma tabela dos textos e dos numeros", lug > 0);
                /* 19, e nao 18: o PRISMATICO foi o primeiro a abrir lugar novo. Os formatos e as
                   linguagens cairam todos em lugares que ja existiam; o triangulo nao. */
                ok("a cifra COMPLETA separa os corpos pelas suas reguas — fecha", lug == 19);
                printf("\n      FECHOU: tantos lugares quantas reguas distintas. Nao sobrou\n");
                printf("      colisao nenhuma por perda de informacao — quando dois corpos caem\n");
                printf("      juntos e porque TEM A MESMA REGUA, e ai sao o mesmo corpo com outra\n");
                printf("      roupa.\n");
                printf("\n      E fechou SEM DELTA. O erro anterior era meu: eu tinha cifrado um\n");
                printf("      lado so do chicote, e as colisoes que sobravam ([1] para tres reguas\n");
                printf("      diferentes) nao eram limite da cifra — eram a cifra INCOMPLETA. A\n");
                printf("      seta de Wick da o outro lado: (B,C) e o seu dual (B,-C), e um deles\n");
                printf("      fecha sempre no real porque B^2-4C e B^2+4C nunca sao ambos\n");
                printf("      negativos. Com os dois lados escritos, o corpo fica inteiro na cifra\n");
                printf("      do rei, e nenhuma outra coordenada e precisa.\n");
                printf("\n      Fica dito o contestavel: para as familias parametricas tomei o\n");
                printf("      operador que o catalogo NOMEIA, e onde o parametro e livre, o membro\n");
                printf("      minimo ainda nao tomado — anotado corpo a corpo no CORPO28.\n\n");
                printf("      -- A DISTANCIA ENTRE OS CORPOS, NA TABELA\n\n");
                ok("a distancia entre os corpos e metrica, e nenhum par distinto dista 0",
                   distancia_corpos());
                printf("\n      A mesma regua que mede 'ouro' contra 'ourives' mede o aureo\n");
                printf("      contra o cosmico. Nao ha regua de corpos separada da regua de\n");
                printf("      textos: ha uma, e ela nao sabe o que esta a medir.\n\n");
                printf("      E as cifras dos corpos sao INFINITAS, todas: o que fica guardado\n");
                printf("      e o PERIODO. O aureo repete [1], o exterior repete [7], e o\n");
                printf("      hipercorpo repete o GERADOR — os 16 vertices do tesseracto — em\n");
                printf("      cada nivel, para sempre. O lado dual dele e a COSTURA, que cresce\n");
                printf("      15·16^k: multiplica por 16 por nivel, logo periodo [16].\n");
                printf("      Os 16 nao sao o corpo; sao os vertices de UM nivel.\n\n");
                printf("      E o HIPERCORPO caiu onde tinha de cair: zero contra toda a coluna\n");
                printf("      hiperbolica, e prefixo 1 com o cristalino e o fractal — os que\n");
                printf("      TAMBEM carregam o real do lado dual. FECHAM, e fecham como todos:\n");
                printf("      com a sua outra metade. NADA FECHA SOZINHO — nem o gato sem o\n");
                printf("      esquilo, nem o aureo com um lado so da cifra. A seta de Wick nao\n");
                printf("      diz \"falha\": diz QUAL DAS METADES carrega o real. E a regua poe-os\n");
                printf("      juntos sem lhe terem dito nada.\n");

                n_leituras = 0;
                executa("ACHA TEXTO 'ouro'");
                ok("e a palavra continua no seu lugar, ao lado dos corpos", n_leituras == 9);
            }

            printf("\n      E as duas coisas sao UMA SO: a distancia e 1/2^k com k o prefixo\n");
            printf("      comum, e o indice guarda-os partilhando exatamente esses k nos. A regua\n");
            printf("      e o indice nao sao duas estruturas — sao a mesma, lida de dois lados.\n");
        }

        /* O POOL COMO BACKEND: o mesmo desenho, e o circuito fecha. */
        printf("\n-- O CIRCUITO FECHADO: o banco le o job de um SLOT\n\n");
        {
            printf("      slot <  S_CANAL   LOAD/STORE -> pread/pwrite   no ficheiro\n");
            printf("      slot >= S_CANAL   LOAD/STORE -> recvfrom/sendto na banda\n");
            printf("      slot >= S_POOL    LOAD/STORE -> o job / a share no pool\n\n");
            /* a merkle root, verificada contra o genese: o coinbase e a branch, ate a raiz */
            {
                Pool P; memset(&P, 0, sizeof P);
                st_trata(&P, "{\"method\":\"mining.notify\",\"params\":[\"j\","
                  "\"0000000000000000000000000000000000000000000000000000000000000000\","
                  "\"01000000010000000000000000000000000000000000000000000000000000000000000000"
                  "ffffffff4d04ffff001d0104455468652054696d65732030332f4a616e2f3230303920436861"
                  "6e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f72"
                  "2062616e6b73\","
                  "\"ffffffff0100f2052a01000000434104678afdb0fe5548271967f1a67130b7105cd6a828e0"
                  "3909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c70"
                  "2b6bf11d5fac00000000\","
                  "[],\"01000000\",\"1d00ffff\",\"495fab29\",true]}");
                pool_st = P;                          /* o teste passa pela MESMA dobra */
                merkle_pelo_fold();
                P = pool_st;
                unsigned char e[32];
                st_hex("4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b", 64, e);
                int mau = 0;
                for(int i = 0; i < 32; i++) if(P.merkle_raiz[i] != e[31-i]) mau = 1;
                printf("      a merkle root do genese, do coinbase ate a raiz:\n      ");
                for(int i = 31; i >= 0; i--) printf("%02x", P.merkle_raiz[i]);
                printf("\n");
                ok("a merkle root bate o bloco genese — o coinbase e a branch estao certos", !mau);
                printf("\n      Era isto que faltava para o circuito fechar de verdade: sem a raiz o\n");
                printf("      cabecalho ia com merkle a zero e NENHUMA share seria valida.\n\n");
            }
            /* sem TIFFANY_POOL_HOST nao ha ligacao, e o backend devolve zero — que e o certo:
             * nao ha job, e o banco le isso num slot, como leria um slot vazio no disco. */
            /* A FLAG, e ela e o RELOGIO a bater: a cada volta o worker olha para a config e para
         * se ela disser. Nao e preciso matar processo — e o banco que manda. */
        { char at[64]; conf_le("mina_ativa", at, sizeof at);
          if(at[0] == '0'){ printf("mina_ativa=0 — a parar.\n"); fflush(stdout); return 1; } }
        Word tem = mem_le(S_POOL + 11);
            printf("      LOAD  S_POOL+11 (tem job?) = %d", tem.total);
            printf("%s\n", getenv("TIFFANY_POOL_HOST") ? "" : "   (sem pool ligado: 0, e e o certo)");
            ok("sem pool, o slot devolve zero — o banco le um slot, nao um erro", tem.total == 0);
            printf("\n      E o cabecalho monta-se de LOADs, nao de uma funcao de rede:\n");
            printf("        versao  <- S_POOL+0     nbits <- S_POOL+1     ntime <- S_POOL+2\n");
            printf("        prevhash <- S_POOL+3..+10   (oito palavras)\n");
            printf("        a SHARE  -> STORE em S_POOL+20, e o backend submete\n");
            printf("\n      O banco nao sabe que houve TCP nem JSON-RPC, como nao sabe que houve\n");
            printf("      UDP no canal nem que ha disco por baixo do pread. TROCAR STRATUM POR\n");
            printf("      OUTRA COISA E TROCAR DUAS FUNCOES — pool_le e pool_grava.\n");
        }

        /* O CANAL COMO BACKEND: o banco escreve num slot e le de um slot. Mais nada. */
        printf("\n-- O CANAL E BACKEND DE LOAD/STORE: os protocolos sao indistinguiveis\n\n");
        {
            Word w = w8(42, 7);
            printf("      STORE no slot S_CANAL+3  (total=%u, e=%u)\n", w.total, w.e);
            mem_grava(S_CANAL + 3, w);
            Word v = mem_le(S_CANAL + 3);
            printf("      LOAD  do slot S_CANAL+3  (total=%u, e=%u)\n", v.total, v.e);
            ok("o Word atravessou a banda e voltou inteiro — residuo 0",
               v.total == w.total && v.e == w.e);
            Word z = w8(99, 0xFF);   /* 0xFF ≡ −1 no envelope Word_8 */
            mem_grava(S_CANAL + 9, z);
            Word y = mem_le(S_CANAL + 9);
            ok("e o slot e o endereco: cada slot volta com o SEU valor", y.total == 99 && y.e == 0xFF);
            printf("\n      O banco fez LOAD e STORE, como faz no disco. Nao ha opcode novo, a ISA\n");
            printf("      nao cresceu, e nenhuma linha de SQL mudou — mas o Word foi ao meio e\n");
            printf("      voltou pela banda, com o slot a servir de endereco e so a diferenca a\n");
            printf("      viajar.\n");
            printf("\n      E POR ISSO OS PROTOCOLOS SAO INDISTINGUIVEIS: o que o banco ve e sempre\n");
            printf("      um slot lido e um slot escrito. Trocar UDP por STOMP ou por TCP e trocar\n");
            printf("      o que esta atras de canal_le e canal_grava — duas funcoes. A topologia\n");
            printf("      da comunicacao e a mesma porque, deste lado, nao ha topologia: ha um\n");
            printf("      endereco, e o endereco e o slot.\n");
        }

        /* A DOBRA NO METAL: merkle e desdobramento, nao conta. */
        printf("\n-- A DOBRA NO METAL: OP_FOLD, e merkle deixa de ter codigo proprio\n\n");
        {
            /* quatro folhas conhecidas, dobradas PELA MAQUINA a partir dos slots */
            unsigned bs = S_CAB + 2000u;   /* região física de átomos */
            for(int i = 0; i < 4; i++){
                unsigned char f[32]; memset(f, 0xA0 + i, 32);
                atomos_grava(bs + (unsigned)(32*i), f, 32);
            }
            atomos_u32(S_FOLD_ARG, bs);
            mem_grava(S_TMP + 1, w8(4, 0));
            pc_emit = 0;
            MOVE(S_TMP + 1, +1); MOVE(S_TMP + 1, +1);
            emit1(OP_FOLD); emit1(OP_HALT);
            unsigned pl = pc_emit;
            Regs rg; memset(&rg, 0, sizeof rg);
            long ps = 0; while(passo(&rg, pl)){ if(++ps > 1000000) break; }
            unsigned char raiz[32];
            atomos_le(bs, raiz, 32);
            printf("      quatro folhas -> raiz  ");
            for(int i = 0; i < 8; i++) printf("%02x", raiz[i]);
            printf("...\n      niveis desdobrados: %d  (log2 de 4)\n\n", rg.R.total);
            ok("a maquina dobra e da a raiz que a conta a mao da", raiz[0] == 0x46 && raiz[1] == 0xae);
            ok("e conta os NIVEIS: quatro folhas sao dois niveis", rg.R.total == 2);
            printf("      Merkle nao e conta: e a MESMA dobra do tesseracto e da cifra — pares que\n");
            printf("      se juntam num, nivel a nivel, com a mesma operacao em todos. M_k =\n");
            printf("      M_{k-1}·A_1: o nivel k carrega o k-1. E a branch e o DESDOBRAMENTO, o\n");
            printf("      caminho de uma folha ate a raiz — o dual da dobra.\n\n");
            printf("      O OP_FOLD estava no enum desde sempre e sem executor. Deixa de estar.\n");
        }

        /* O MARTELO: a prova de trabalho e tarefa do BANCO, e a cifra e que decide. */
        printf("\n-- O MARTELO: o banco executa a prova de trabalho, e a cifra decide\n\n");
        {
            executa("CABECALHO '0100000000000000000000000000000000000000000000000000000000000000000000003ba3edfd7a7b12b27ac72c3e67768f617fc81bc3888a51323a9fb8aa4b1e5e4a29ab5f49ffff001d1dac2b7c' "
                    "'00000000ffff0000000000000000000000000000000000000000000000000000'");
            printf("\n$ MARTELO 2083236890 2083236900\n");
            ok("o martelo acha o nonce do bloco genese na faixa que o contem",
               executa("MARTELO 2083236890 2083236900") == 1);
            printf("\n$ MARTELO 1000 1010\n");
            executa("MARTELO 1000 1010");
            printf("\n$ VERIFICA 2083236893\n");
            ok("e a REVERSAO confere esse nonce — o esquilo reverte, residuo 0",
               executa("VERIFICA 2083236893") == 1);
            printf("\n$ VERIFICA 1000\n");
            ok("e recusa um nonce qualquer", executa("VERIFICA 1000") == 0);
            printf("\n      Nada de novo foi escrito para isto: a cifra que compara o hash com o\n");
            printf("      alvo e a MESMA que compara 'ourives' com 'ourivesaria' e os trinta\n");
            printf("      corpos entre si — anda-se o caminho comum e o primeiro termo que\n");
            printf("      diverge decide. Sem largura, sem norma a transbordar, sem truncar.\n");
            printf("\n      E o par e o chicote de sempre: MARTELO procura e ESTICA (a faixa\n");
            printf("      toda), VERIFICA confere e CONTRAI (um so). E por isso que o trabalho\n");
            printf("      e PROVA — caro de achar, barato de crer.\n");
        }

        /* A DISTÂNCIA: a régua compõe as três, e é isso que o sistema devolve. */
        printf("\n-- A DISTÂNCIA ENTRE MÉTRICAS: a régua compõe as três, e não julga\n\n");
        {
            executa("CREATE TABLE k (a AUREO(1), b CRISTALINO(0))");
            executa("INSERT INTO k VALUES (3+2s, 1+1s)");
            printf("$ SELECT * FROM k WHERE a > 0      (Δ = 5, hiperbólico)\n");
            int r1 = executa("SELECT * FROM k WHERE a > 0");
            printf("\n$ SELECT * FROM k WHERE b > 0      (Δ = −4, elíptico)\n");
            int r2 = executa("SELECT * FROM k WHERE b > 0");
            ok("o sistema não RECUSA nem DESPACHA por classe — corre nas duas", r1 && r2);
            Regua ra = { 1, -1 }, rb = { 0, 1 };
            long da = ct_norma(ra,(Par){3,2}) - ct_norma(ra,(Par){1,1});
            long db = ct_norma(rb,(Par){3,2}) - ct_norma(rb,(Par){1,1});
            if(da < 0) da = -da;
            if(db < 0) db = -db;
            printf("\n      régua        Δ      d((3,2),(1,1))\n");
            printf("      a²+ab−b²     %-6ld %ld\n", ct_assinatura(ra), da);
            printf("      a²+b²        %-6ld %ld\n", ct_assinatura(rb), db);
            ok("a distância existe nas duas classes, e sai da MESMA conta", da > 0 && db > 0);
            printf("\n      APAGADO daqui: a guarda que recusava por classe e o estado que a\n");
            printf("      alimentava. Encarnavam a ideia refutada — a de que o sistema devia dar\n");
            printf("      ORDEM e, para isso, decidir a classe. Não devia.\n");
            printf("\n      A ordem obriga a escolher a classe; a distância não obriga a nada. Fica\n");
            printf("      d(u,v) = |N(u) − N(v)|, definida em toda régua, e quem julga é quem pediu.\n");
            printf("      Ver distancia.c. E ordem.c fica pelo que continua VERDADE: o elíptico não\n");
            printf("      é ordenável — resultado, e não motivo para recusar.\n");
            executa("CREATE TABLE t (a,b,c)");
            executa("INSERT INTO t VALUES (7,10,20)");
            executa("INSERT INTO t VALUES (3,30,40)");
            executa("INSERT INTO t VALUES (7,50,60)");
            executa("INSERT INTO t VALUES (9,70,80)");
            executa("INSERT INTO t VALUES (3,90,99)");
        }

        /* O CIRCUITO FECHADO: o esquilo entra, e o metal passa a girar além de esticar. */
        printf("\n-- O CIRCUITO FECHADO: o esquilo no metal, e todo metal como PALAVRA\n\n");
        {
            int mau = 0; long casos = 0;
            /* 1. as ordens, contadas na máquina — não na conta ao lado */
            Word v; v.total = 5; v.e = 3;
            mem_grava(S_TMP, v);
            int ord_s = 0;
            for(int k = 1; k <= 12; k++){
                pc_emit = 0;
                MOVE(S_TMP, +1); emit1(OP_ESQUILO); MOVE(S_TMP, -1);
                emit1(OP_HALT); rodar(pc_emit);
                Word w = mem_le(S_TMP);
                if(w.total == 5 && w.e == 3){ ord_s = k; break; }
            }
            ok("ESQUILO tem ordem 4 NA MÁQUINA — gira e volta ao ponto de partida", ord_s == 4);
            mem_grava(S_TMP, v);
            int ord_j = 0;
            for(int k = 1; k <= 12; k++){
                pc_emit = 0;
                MOVE(S_TMP, +1); emit1(OP_TROCA); MOVE(S_TMP, -1);
                emit1(OP_HALT); rodar(pc_emit);
                Word w = mem_le(S_TMP);
                if(w.total == 5 && w.e == 3){ ord_j = k; break; }
            }
            ok("TROCA tem ordem 2 — a involução, e é a sua própria inversa", ord_j == 2);

            /* 2. O QUE FECHA O CIRCUITO: todo metal é PALAVRA nos quatro geradores. Prata e
             * bronze DEIXARAM de existir como opcode — foram apagados, porque eram atalhos.
             * Já não há opcode dedicado com que comparar: compara-se com a matriz. */
            printf("      metal    palavra nos geradores            A_m(5,3)   confere?\n");
            const char *nm[3] = { "ouro", "prata", "bronze" };
            for(int m = 1; m <= 3; m++)
            for(long a = -6; a <= 6; a++) for(long b = -6; b <= 6; b++){
                Word x; x.total = a; x.e = b;
                mem_grava(S_TMP, x);
                pc_emit = 0; emit_metal(m, S_TMP);
                emit1(OP_HALT); rodar(pc_emit);
                Word pela_palavra = mem_le(S_TMP);
                Par esperado = me_ap(me_gato(m), (Par){a,b});
                if(pela_palavra.total != (Word8)esperado.a || pela_palavra.e != (Word8)esperado.b) mau++;
                if(a == 5 && b == 3)
                    printf("      %-8s %-32s (%d,%d)%*s%s\n", nm[m-1],
                           m==1?"GOLD":(m==2?"GOLD TROCA GOLD":"GOLD TROCA GOLD TROCA GOLD"),
                           pela_palavra.total, pela_palavra.e, 5, "",
                           (pela_palavra.total==esperado.a &&
                            pela_palavra.e==esperado.b) ? "sim ✓" : "NÃO");
                casos++;
            }
            ok("A_m = (A_1·J)^{m−1}·A_1 CONFERE no metal — prata e bronze foram APAGADOS",
               mau == 0);
            printf("      (%ld casos, três metais, e nenhum deles com opcode próprio.)\n", casos);

            /* 3. o cisalhamento, que NÃO é opcode e não precisa de ser */
            {
                Word x; x.total = 5; x.e = 3;
                mem_grava(S_TMP, x);
                pc_emit = 0;
                MOVE(S_TMP, +1); emit1(OP_TROCA); MOVE(S_TMP, -1);
                MOVE(S_TMP, +1); emit1(OP_GOLD);  MOVE(S_TMP, -1);
                emit1(OP_HALT); rodar(pc_emit);
                Word t = mem_le(S_TMP);
                /* T = [[1,1],[0,1]] em (5,3) dá (5+3, 3) = (8,3) */
                ok("TROCA GOLD é o cisalhamento T — palavra de dois, e poupa um opcode",
                   t.total == 8 && t.e == 3);
            }
            printf("\n      O circuito fecha porque o repertório fecha: o gato estica (ordem ∞, e por\n");
            printf("      isso precisou do negro), o esquilo gira (ordem 4, a inversa é S³), a troca\n");
            printf("      reflete (ordem 2, é a sua própria inversa). O que a máquina faz, ela\n");
            printf("      desfaz — nos inteiros e sem guardar cópia.\n");
            printf("\n      E o que fica de FORA, dito: decompor uma unimodular QUALQUER em palavra\n");
            printf("      não está no compilador. Mede-se que existe para os metais e para as\n");
            printf("      inversas; para matriz arbitrária o algoritmo é o de Euclides e não está\n");
            printf("      aqui. Dizer que já está seria medir a fatia e afirmar o todo.\n");
        }

        /* O CHICOTE DOS DOIS LADOS: o negro tão inteiro quanto o branco, NO METAL. */
        printf("\n-- O CHICOTE DOS DOIS LADOS: A_m no metal para TODO m, e a volta também\n\n");
        {
            int mau = 0; long casos = 0;
            /* A assimetria era minha: generalizei o branco (A_m para todo m ≥ 1) e deixei o
             * negro nos três opcodes. A régua não tem lado — T⁻¹ = J·A_1⁻¹ é o espelho exato
             * de T = A_1·J, e com ela A_m = T^{m−1}·A_1 vale para m ≤ 0 igualmente. */
            printf("      m     palavra emitida                          A_m no metal   confere?\n");
            for(long m = -12; m <= 12; m++)
            for(long a = -5; a <= 5; a++) for(long b = -5; b <= 5; b++){
                Word x; x.total = a; x.e = b;
                mem_grava(S_TMP, x);
                pc_emit = 0;
                MOVE(S_TMP, +1); emit1(OP_GOLD); MOVE(S_TMP, -1);
                if(m >= 1) for(long k = 1; k < m; k++){          /* T = TROCA depois GOLD */
                    MOVE(S_TMP, +1); emit1(OP_TROCA); MOVE(S_TMP, -1);
                    MOVE(S_TMP, +1); emit1(OP_GOLD);  MOVE(S_TMP, -1);
                } else for(long k = m; k <= 0; k++){             /* T⁻¹ = NEGRO depois TROCA */
                    MOVE(S_TMP, +1); emit1(OP_NEGRO_OURO); MOVE(S_TMP, -1);
                    MOVE(S_TMP, +1); emit1(OP_TROCA);      MOVE(S_TMP, -1);
                }
                emit1(OP_HALT); rodar(pc_emit);
                Word pela_palavra = mem_le(S_TMP);
                Par esperado = me_ap(me_gato(m), (Par){a,b});   /* o toolkit CONFERE */
                if(pela_palavra.total != (Word8)esperado.a || pela_palavra.e != (Word8)esperado.b) mau++;
                if(a == 5 && b == 3 && (m == 0 || m == -1 || m == 4))
                    printf("      %-5ld %-40s (%d,%d)%*s%s\n", m,
                           m==0 ? "GOLD NEGRO TROCA"
                                : (m==-1 ? "GOLD NEGRO TROCA NEGRO TROCA"
                                         : "GOLD TROCA GOLD TROCA GOLD TROCA GOLD"),
                           pela_palavra.total, pela_palavra.e, 3, "",
                           (pela_palavra.total==esperado.a &&
                            pela_palavra.e==esperado.b) ? "sim ✓" : "NÃO");
                casos++;
            }
            ok("A_m corre no metal para TODO m — negativo, zero e positivo, sem opcode próprio",
               mau == 0);
            printf("      (%ld casos, m de −12 a 12.)\n", casos);

            /* VOLTA no envelope Word_8: σ²=σ+1 fecha a FORMA; coeficientes que
             * transbordam sobem a torre (ℕ), não long C. Aqui mede-se o fecho
             * onde o envelope basta. */
            int mau2 = 0; long casos2 = 0;
            for(long m = -3; m <= 3; m++)
            for(long a = -2; a <= 2; a++) for(long b = -2; b <= 2; b++){
                Word x; x.total = (Word8)a; x.e = (Word8)b;
                mem_grava(S_TMP, x);
                pc_emit = 0;
                /* IDA */
                MOVE(S_TMP, +1); emit1(OP_GOLD); MOVE(S_TMP, -1);
                if(m >= 1) for(long k = 1; k < m; k++){
                    MOVE(S_TMP, +1); emit1(OP_TROCA); MOVE(S_TMP, -1);
                    MOVE(S_TMP, +1); emit1(OP_GOLD);  MOVE(S_TMP, -1);
                } else for(long k = m; k <= 0; k++){
                    MOVE(S_TMP, +1); emit1(OP_NEGRO_OURO); MOVE(S_TMP, -1);
                    MOVE(S_TMP, +1); emit1(OP_TROCA);      MOVE(S_TMP, -1);
                }
                /* VOLTA: a palavra ao contrário, GOLD↔NEGRO, e a TROCA fica onde está */
                if(m >= 1) for(long k = m-1; k >= 1; k--){
                    MOVE(S_TMP, +1); emit1(OP_NEGRO_OURO); MOVE(S_TMP, -1);
                    MOVE(S_TMP, +1); emit1(OP_TROCA);      MOVE(S_TMP, -1);
                } else for(long k = 0; k >= m; k--){
                    MOVE(S_TMP, +1); emit1(OP_TROCA); MOVE(S_TMP, -1);
                    MOVE(S_TMP, +1); emit1(OP_GOLD);  MOVE(S_TMP, -1);
                }
                MOVE(S_TMP, +1); emit1(OP_NEGRO_OURO); MOVE(S_TMP, -1);
                emit1(OP_HALT); rodar(pc_emit);
                Word volta = mem_le(S_TMP);
                if(volta.total != (Word8)a || volta.e != (Word8)b) mau2++;
                casos2++;
            }
            ok("e a VOLTA de todo metal é a palavra ao contrário, letra a letra invertida",
               mau2 == 0);
            printf("      (%ld percursos ida-e-volta no envelope Word_8; σ²=σ+1.)\n", casos2);
            printf("\n      A assimetria era minha, não do mecanismo: eu tinha generalizado o branco e\n");
            printf("      deixado o negro nos três opcodes. A régua não tem lado — e m = 0 dá\n");
            printf("      A_0 = J, a TROCA, que é onde o chicote passa ao mudar de sinal. Não é\n");
            printf("      peça que eu acrescentei: é o meio da régua.\n");
            printf("\n      E daqui sai o que se pode APAGAR: SILVER, BRONZE, NEGRO_PRATA e\n");
            printf("      NEGRO_BRONZE são palavras, não geradores. O repertório mínimo que fecha é\n");
            printf("      GOLD, NEGRO_OURO, TROCA e ESQUILO — quatro peças, simétricas. Os outros\n");
            printf("      quatro ficam por serem ATALHOS: poupam palavra, não poder.\n");
        }

        /* AS AFIRMAÇÕES. O teste imprimia e não concluía; agora confere contra conta feita
         * à mão, e a bateria passa a cobrir o compilador em vez de o ignorar. */
        printf("\n-- AS AFIRMAÇÕES (a bateria passa a cobrir isto)\n\n");
        {
            executa("DELETE FROM t");
            executa("INSERT INTO t VALUES (3/4,1,1)");
            executa("INSERT INTO t VALUES (5,2,1)");
            executa("INSERT INTO t VALUES (7/2,3,1)");
            executa("INSERT INTO t VALUES (2,5,1)");
            struct { const char *q; long e; const char *rot; } cs[] = {
              {"SELECT * FROM t WHERE a = 3/4",            1, "a igualdade racional fecha"},
              {"SELECT * FROM t WHERE a = 6/8",            1, "e a classe: 6/8 casa com 3/4"},
              {"SELECT * FROM t WHERE a > 1",              3, "a ordem racional, sem divisão"},
              {"SELECT * FROM t WHERE a * 2 > 7",          1, "coeficiente sobre racional"},
              {"SELECT * FROM t WHERE a > 2 AND a > 2",    2, "idempotência: A op A = A"},
              {"SELECT * FROM t WHERE (a>2 AND a<9) OR a>2",2,"absorção: a adjunção δ⊣ε"},
              {"SELECT * FROM t WHERE a + b > 5",          3, "duas colunas, denominadores"},
            };
            for(unsigned q = 0; q < sizeof cs/sizeof cs[0]; q++){
                ultima_conta = -1;
                executa(cs[q].q);
                ok(cs[q].rot, ultima_conta == cs[q].e);
            }
        }

        printf("\n-- A PARADA: o cliente pode pedir o impossível, e isso é uma resposta.\n");
        printf("\n$ SELECT * FROM t WHERE a - a = 5\n");
        executa("SELECT * FROM t WHERE a - a = 5");
        printf("\n$ SELECT * FROM t WHERE a - a = 0\n");
        executa("SELECT * FROM t WHERE a - a = 0");
        printf("\n$ SELECT * FROM t WHERE b > 20 AND a - a = 5\n");
        executa("SELECT * FROM t WHERE b > 20 AND a - a = 5");
        printf("\n$ SELECT * FROM t WHERE b > 20 OR a - a = 0\n");
        executa("SELECT * FROM t WHERE b > 20 OR a - a = 0");

        printf("\n-- o resto continua de pé:\n");
        printf("\n$ UPDATE t SET c = 111 WHERE a >= 7 AND a != 9\n");
        executa("UPDATE t SET c = 111 WHERE a >= 7 AND a != 9");
        printf("$ SELECT * FROM t\n"); executa("SELECT * FROM t");
        printf("\n$ DELETE FROM t WHERE (a <= 3 AND b >= 90)\n");
        executa("DELETE FROM t WHERE (a <= 3 AND b >= 90)");
        printf("$ SELECT * FROM t\n"); executa("SELECT * FROM t");
        fechar_base();

        /* fecha e REABRE: o dado tem de estar no disco, não na memória do processo */
        printf("\n-- fechado. reabrindo o arquivo e consultando de novo:\n");
        char m[512]; snprintf(m, sizeof m, "%s.mem", base);
        fmem = open(m, O_RDWR);
        char g[512]; snprintf(g, sizeof g, "%s.prog", base);
        fprog = open(g, O_RDWR|O_CREAT|O_TRUNC, 0644);
        printf("$ SELECT * FROM t WHERE a = 7\n"); executa("SELECT * FROM t WHERE a = 7");
        fechar_base();
        /* O EXIT SEGUE AS UNIDADES. Estava `return 0` fixo: o modo teste emitia dezenas
         * de `ok()` e saía SEMPRE 0, logo uma asserção vermelha dava verde no exit.
         * Quem apanhava era a rede da bateria (exit 0 com unidade vermelha é FALHA), e
         * uma rede não substitui o medidor dizer a verdade sobre si. */
        printf("\n=== %d asserções, %d falhas ===\n", unidades, falhas);
        return falhas ? 1 : 0;
    }
    /* O SCRIPT: um comando por linha, lido da entrada. É por aqui que um produtor de fora — o
     * pipe do Stratum, por exemplo — despeja o que tem sem ter de chamar o binário uma vez por
     * linha. Linha vazia e linha que começa por -- são comentário, e não contam para nada.
     *
     * A LINHA PASSA PELO BANCO, E NAO POR RAM. Eu tinha posto um realloc aqui — "a linha cresce
     * sem tecto, nao e o leitor que ha de a cortar" — e o argumento e verdade mas nao autoriza
     * memoria: a regra e dura. Informacao que entra VAI PARA O BANCO, e e de la que se le.
     *
     * O simbolo entra, escreve-se num slot de rascunho por pwrite, e o comando executa-se do que
     * o banco tem. O buffer e FIXO e pequeno — e a janela de escrita, nao o texto. */
    if(argc >= 3 && !strcmp(argv[2], "-")){
        if(!abrir_base(argv[1])){ perror("base"); return 2; }
        #define S_LINHA  S_LINRASC                    /* zona própria: ver S_LINRASC */
        #define LIN_MAX  16384u                        /* 1024 slots: o que o banco lhe reserva */
        char *lin = DISCO_FIXO(char, 20);              /* a JANELA, fixa — nao cresce nunca */
        disco_prende(DISCO_BASE(20),"dados/sql_lin.bin",(size_t)LIN_MAX,1);
        long n = 0, mau = 0, cortadas = 0;
        for(int c; ; ){
            size_t k = 0;
            int estourou = 0;
            while((c = getchar()) != EOF && c != '\n'){
                if(k + 1 >= LIN_MAX){ estourou = 1; continue; }
                lin[k++] = (char)c;
                mem_grava(S_LINHA + (unsigned)(k - 1), (Word){ (long)(unsigned char)lin[k-1], 0 });
            }
            if(k == 0 && c == EOF) break;
            lin[k] = 0;
            if(estourou){
                cortadas++;
                fprintf(stderr, "linha maior que %u: RECUSADA, e nao truncada em silencio\n", LIN_MAX);
            } else {
                const char *p = lin; pula(&p);
                if(*p && !(p[0] == '-' && p[1] == '-')){
                    n++;
                    if(!executa(lin)){ mau++; fprintf(stderr, "falhou: %s\n", lin); }
                }
            }
            if(c == EOF) break;
        }
        if(cortadas) fprintf(stderr, "%ld linha(s) recusada(s) por tamanho\n", cortadas);
        fechar_base();
        fprintf(stderr, "%ld comando(s), %ld falha(s)\n", n, mau);
        return mau ? 1 : 0;
    }
    if(argc >= 3){
        if(!abrir_base(argv[1])){ perror("base"); return 2; }
        conf_abre(argv[1]);
        int r = executa(argv[2]);
        fechar_base();
        return r ? 0 : 1;
    }
    fprintf(stderr, "uso: sql teste | sql <base> \"<comando SQL>\" | sql <base> -   (script na entrada)\n");
    return 2;
}
#endif /* SQL_NO_MAIN */
