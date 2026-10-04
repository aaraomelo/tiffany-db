/* jev_core.h — o contrato da arena do JEV, e os SERVIÇOS crus.
 *
 * Proveniencia
 *   fonte .... conecthus/backends/wasm/jev/marginal.c
 *              blob de origem 20e64e626ca923d2cddb5d3dcdff20d6b3dbb16b
 *              sha256 do ficheiro d1d5bf8538d221bcc486e31d7b8dccfcb047a0c51891d00987b3de85b048b1e4
 *              8224 bytes, 227 linhas, 5 funcoes: proj, marginal, escore, duas, marginal_d
 *   transporte. As CINCO funcoes entram com o corpo EQUIVALENTE: codigo identico
 *              linha a linha ao da fonte. O ficheiro nao e copia literal — os
 *              comentarios têm a acentuacao restaurada, e ha um bloco de nota
 *              junto de jev_marginal_d_raw. A mudanca FUNCIONAL nas cinco e
 *              so o NOME: cada uma passa a chamar-se jev_*_raw. No produto
 *              as cinco raw sao tambem static — mudanca de LINKAGE, nao de
 *              comportamento, e e o que impede os *_raw de entrarem no ABI.
 *              A guarda de proj (banco/jev.c) precisa de passar por cima
 *              de proj e não por cima delas. Os corpos das quatro
 *              chamadoras continuam a escrever `proj(...)` — nome,
 *              argumentos e ordem intactos.
 *   prova ..... 96 casos E1/E2/E3/E4/E5/E5-isolado/E7a/E7b, 65536 bytes de arena
 *              por caso, 6291456 bytes comparados contra o marginal.wasm original:
 *              ZERO diferencas (medicao historica). Os 96 casos estao versionados
 *              como gerador em tests/jev_casos.c; o golden (tests/jev_golden.bin),
 *              o oraculo marginal.wasm e o harness de comparacao NAO estao
 *              versionados. A reproducao independente nao e possivel a partir do
 *              repositorio actual.
 *
 * A ARENA E INT32 A PARTIR DO BYTE 8
 * O byte 8 é o mesmo contrato do marginal.wasm, e a razão de ser deste número
 * está no tools/traduz.c: e o offset da arena dentro do DISCO. Não é escolha
 * nossa e não se mexe.
 *
 *   marginal(int X) e marginal_d(int X)
 *     [0, X)      G(x)              o campo de contagem, lido
 *     X           |I|               cardinal do I
 *     X+1         n                 dimensoes tratadas (só marginal_d)
 *     P = X+2     c_0 c_1 c_2       classes por dimensão
 *     B = P+3     fronteiras        três blocos contiguos de (c_i+1): b_0..b_c
 *     D1 = B+S    marginais directas, linha p em 16*p
 *     J = D1+48   conjunta em ordem odometro
 *     D2 = J+PROD marginais da conjunta, mesma linha em 16*p
 *     OUT = D2+48 OUT[0]=total do campo; OUT[1]=|I| lido
 *
 *   marginal_d(int X)
 *     X+1         d (1..3)          dimensoes; as extras viram classe de residuo 0
 *     C = P+d     coordenadas       d blocos de X ints, coord_k(x)
 *     B = C+d*X   fronteiras        d blocos de (c_i+1) sobre o VALOR da coordenada
 *     resto       como em marginal, com 16*d em vez de 48
 *
 *   escore(int X)
 *     X+1         m                 niveis do Score
 *     B = X+2     fronteiras do Score (m+1)
 *     W = B+m+1   pesos s_i (m ints)
 *     OUT = W+m   OUT[0]=N; OUT[1]=|I|; OUT[2]=m
 *
 *   duas(int X)
 *     Bn = X+2    fronteiras do Noul (c=2: 0, corte, X)
 *     Bs = X+5    fronteiras do Score (m+1)
 *     W  = Bs+m+1 pesos s_i
 *     OUT = W+m   [0..1] Noul G_q(0), G_q(1); [2..2+m) Score por nivel;
 *                 [2+m] N; [3+m] |I|; [4+m] total recontado
 *
 * NENHUMA FRONTEIRA PODE DEIXAR DE SER FRONTEIRA
 * A projeção devolve -1 quando o valor não está em nenhum bloco. O motor
 * original usava esse -1 COMO INDICE: em marginal/marginal_d escrevia por
 * cima de D1-1, que é a última fronteira de entrada, dentro da arena, sem
 * segfault, sem aviso, e devolvia rc=0. Medido em probe.c:
 *     fronteiras antes {10,20,0,1} -> depois {10,20,0,11}, rc=0.
 * Aqui a projeção continua a devolver -1 — o corpo dela é o do original — mas
 * a guarda regista a ocorrência e a porta devolve JEV_ERR_CLASSE. O campo G é
 * as fronteiras ficam por detificar: quem chamou deve guardar a arena (por
 * jev_arena) antes e repô-la, ou deitar o resultado. É o que está escrito na
 * porta. Um motor de base de dados que corrompe a entrada e diz «correu bem»
 * não serve.
 */
#ifndef JEV_CORE_H
#define JEV_H

#define JEV_ARENA_BYTES 65536

/* O BYTE 8 E UM FACTO DO WASM, NÃO UMA REGRA DE USO EM C.
 * No marginal.wasm a arena vive no offset 8 do DISCO — e o mesmo offset que os
 * ensaios E1-E7 usavam, porque o tradutor põe a arena lá. O contrato int32
 * contava a partir dai. Em C a arena está no ponteiro devolvido por
 * jev_arena() e a célula 0 é arena[0]: não há nada a saltar. Por isso este
 * ficheiro não define JEV_ARENA_BASE. Ver jev_api.h. */

/* A CONVENCAO DE RETORNO E DA CASA, E NÃO DO ORIGINAL.
 * Os corpos crus devolvem 0 — como o original, sem mudança. Quem traduz é a
 * PORTA, e ela segue o que a casa já faz: sql_abrir devolve 1 para sucesso e
 * 0 para falha (banco/sql.c:16067 e :16055), e sql_executa devolve 0 no erro.
 * Portanto, na fronteira, 1 = leu a arena e deu conta; 0 = recusou. */
#define JEV_OK           1
/* alguma projeção devolveu -1: o resultado foi escrito por cima das fronteiras
 * de entrada e NÃO serve. A arena fica por identificar — quem chamou deve
 * repor a cópia que guardou, ou deitar o resultado. */
#define JEV_ERR_CLASSE   0

/* Os cinco SERVIÇOS são internos a banco/jev.c e são static: não entram no ABI.
 * Não há prototipo aqui de proposito — se o header os declarasse, a definição
 * static de banco/jev.c passaria a ser uma declaração nao-static seguida de uma
 * static, e o compilador recusa. O que eles fazem está nos comentários de
 * origem, dentro do próprio ficheiro, onde essa informação é de jeito.
 *
 * Quem chama é a PORTA (jev_api.h), que e quem põe a guarda e traduz o retorno.
 * E a única porta que existe: não há caminho para chegar aos servos sem passar
 * por ela, e é por isso que a guarda não pode ser contornada. */

#endif
