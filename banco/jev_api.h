/* jev_api.h — porta C do JEV para o produto Tiffany DB.
 *
 * O QUE ISTO E. As cinco leituras do JEV — as marginais da conjunta contra as
 * marginais directas, o escore fraccionário como par racional exacto, o Noul e
 * o Score na mesma varredura, e a ponte com o motor dimensional iota_d — que
 * antes viviam só dentro de um motor WASM experimentally, correm aqui dentro da
 * biblioteca, em C, sem runtime novo e sem dependência nova.
 *
 * O QUE ISTO NÃO É. Não é SQL. Não é um agregado: nenhuma destas leituras
 * recebe colunas, nem GROUP BY, nem produz uma linha. Recebe um CAMPO G que o
 * chamador traz pronto na arena. Encaixa-la no caminho do motor seria trabalho
 * de arquitectura (um valor novo na maquina de células, ou a arena serializada
 * numa coluna), e não cabe numa verscao compatível. A porta C e o caminho que
 * deixa o produto CONSUMIR o JEV sem o motor saber o que é JEV.
 *
 * O CONTRATO. A arena é do CHAMADOR. A biblioteca não guarda estado: quem
 * chama pede o ponteiro, escreve a entrada, chama a leitura, le o resultado da
 * mesma arena. Duas leituras seguidas partilham a arena; quem quiser isolar
 * guarda os 65536 bytes antes (ver jev_arena) e repõe depois. O motor SQL não
 * guarda estado global, e o JEV não passa a guardar o que não tinha.
 *
 * O RETORNO. 1 = a leitura correu; 0 = recusou (JEV_ERR_CLASSE), e nesse caso o
 * resultado NÃO e válido — ver «NENHUMA FRONTEIRA PODE DEIXAR DE SER FRONTEIRA»
 * em jev_core.h. Os corpos internos devolvem 0 como o original; quem traduz é
 * está camada, e ela segue a convenção da casa (sql_abrir).
 *
 * A FIDELIDADE. Os cinco corpos entram verbatim do original. Onde esta porta
 * diverge do que o motor WASM fazia, e só aqui: (a) o retorno é 1/0 e não 0;
 * (b) uma projeção fora de classe é recusada em vez de virar índice. Fora
 * dessas duas, os 65536 bytes da arena batem byte a byte com o marginal.wasm
 * original em 96 casos de prova.
 */
#ifndef JEV_API_H
#define JEV_API_H

#define JEV_ARENA_BYTES 65536

/* NÃO EXISTE AQUI UM "BYTE 8". No marginal.wasm a arena comeca no byte 8 porque
 * e o offset que o tradutor lhe dá dentro do DISCO, é o mesmo contrato que a origem
 * fora. Em C não há DISCO nem offset: a arena ESTA no ponteiro que jev_arena()
 * devolve, e a célula 0 é arena[0]. Um macro de offset na porta seria um
 * convite a saltarem 8 bytes que não existem. O facto do byte 8 fica escrito em
 * jev_core.h, onde é a história da origem e não uma instrução de uso. */

#define JEV_OK           1
#define JEV_ERR_CLASSE   0

/* A arena: 65536 bytes, int32 contados desde a celula 0 do ponteiro. O ponteiro é o
 * mesmo em todas as leituras e não muda. Para uma leitura isolada, guarde-o
 * antes e restaure-o depois. */
unsigned char *jev_arena(void);

/* E1+E2 — as marginais da conjunta (D2) igualam as marginais directas (D1),
 * linha p em 16*p, e OUT[0]=total, OUT[1]=|I|. c_0 c_1 c_2 e as classes por
 * dimensão; as fronteiras são três blocos contiguos de (c_i+1). */
int jev_marginal(int X);

/* E4 — o escore fraccionário como par racional EXACTO (N, |I|) em OUT[0..1], m
 * em OUT[2]. Não há divisão no motor: N = Σ s_i G_q(s_i) sai inteiro e o
 * racional N/|I| e uma LEITURA do par, feita fora. */
int jev_escore(int X);

/* E5 — Noul e Score sobre o MESMO G numa varredura única: a segunda leitura não
 * recria o campo. OUT[0..1] = G_q(0), G_q(1); OUT[2..2+m) = Score por nivel;
 * OUT[2+m] = N; OUT[3+m] = |I|; OUT[4+m] = total recontado. */
int jev_duas(int X);

/* E7 — a MESMA coisa lida como coordenadas de X_d, alinhada com iota_d: as
 * projecoes são coordenadas verdadeiras de X_d dadas na arena, não intervalos do
 * mesmo índice. Com a coordenada nova a zero (o residuo de iota_d) a leitura 2D
 * devolve a 1D. As linhas estão em 16*k e as regiões em 16*d. */
int jev_marginal_d(int X);

/* A projeção: a classe de x dentro dos c blocos b_0..b_c, ou -1 se x não está
 * em nenhum. Exposta porque as quatro leituras a usam, e porque um -1 é a
 * única coisa que a porta recusa. */
int jev_proj(int b, int c, int x);

#endif
