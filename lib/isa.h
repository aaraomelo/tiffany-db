/* isa.h — A ISA, NUMA FONTE SÓ.
 *
 * Estava escrita duas vezes: uma no `sql.c` e outra no `erg.c`, com o `erg.c`
 * §E1 a existir precisamente para as confrontar. Um medidor a vigiar uma cópia
 * é melhor do que nada e pior do que não haver cópia --- e ele apanhou-o: o
 * `sql.c` tinha 24 opcodes e o `erg.c` 18, com cinco que a porta não expunha
 * por não os CONHECER, não por dependerem do banco.
 *
 * Os oito primeiros são os do broca-so, número a número. A partir do nono as
 * duas ISAs separam-se: o broca-so tem ali SILVER, GOLD, BRONZE, e esta casa
 * não carregou os três metais nem o UNFOLD, o PROJECT, o LIFT, o SPECT e o
 * COUNT --- pôs o GOLD e acrescentou os seus. O que as duas partilham não é a
 * numeração, é a FORMA: dois registadores, uma ULA componente a componente, e o
 * endereço IMEDIATO na instrução.
 *
 * O §E1 continua a medir --- e passa a medir contra ESTE ficheiro, que é a
 * fonte, em vez de contra uma cópia.
 *
 * A REGRA DE QUEM ACRESCENTA: no FIM. O número de cada opcode antigo não pode
 * mudar, ou um programa compilado antes passa a significar outra coisa. Já
 * aconteceu uma vez, com o ADD16 a entrar no meio e a empurrar quatro casas.
 */
#ifndef TIFFANY_ISA_H
#define TIFFANY_ISA_H

enum { OP_HALT=0, OP_LOAD, OP_STORE, OP_ADD, OP_SUB, OP_AND, OP_OR, OP_XOR,
       OP_GOLD, OP_CMP, OP_JMP, OP_JZ, OP_JNZ,
       OP_FOLD, OP_LOADS,
       /* saíram quatro nomes que estavam só neste enum: sem um único `case`, sem
        * entrada no montador e sem uso. Reservados que nunca correram — e manter
        * redundância custa mais do que a tirar. (Os nomes não se escrevem aqui: o
        * erg.c LÊ este enum do ficheiro, e apanhá-los-ia como opcodes.) */
       /* A VOLTA. Acrescentados no FIM de propósito: o número de cada opcode antigo não
        * muda, e nenhum programa já compilado passa a significar outra coisa. */
       OP_NEGRO_OURO,
       /* O CIRCUITO. O gato estica; faltava quem GIRE, e sem ele a máquina não gera o grupo
        * todo. ESQUILO é ×ω do cristalino (t=0): det +1, ordem 4. TROCA é J, a involução. */
       OP_ESQUILO, OP_TROCA,
       /* O MARTELO. A prova de trabalho é uma TAREFA DO BANCO, não de um processo que fala com
        * ele: o SELECT e o UPDATE já correm nesta máquina, e o martelo corre ao lado deles. Como
        * OP_GOLD, ele não é um programa — é um opcode que a máquina executa. */
       OP_MARTELO,
       /* O ANDAR DE CIMA COMO INSTRUÇÕES PRÓPRIAS. O ADD de oito tem de continuar
        * componente a componente: o catálogo guarda o par numa Word e o `nrows++`
        * soma 1 ao segundo componente — com transporte a atravessar, um segundo
        * componente de 255 a passar a 256 subiria para o primeiro. O par e o
        * número são leituras DIFERENTES da mesma Word, e por isso são instruções
        * diferentes. (E o GOLD de 16 não precisa de opcode: é o ADD de 16 mais
        * uma cópia.)
        *
        * E ESTES ESTAVAM NO MEIO, contra a regra que o parágrafo acima declara.
        * Entraram depois do LOADS e empurraram NEGRO_OURO, ESQUILO, TROCA e
        * MARTELO três casas — de modo que um programa compilado antes, com
        * NEGRO_OURO no 15, passou a executar o ADD de 16. A regra da VOLTA não é
        * decoração: quem acrescenta acrescenta NO FIM. Medido pelo erg.c §E1,
        * que confronta este enum com o montador e dizia «DISCORDAM em 3, o
        * primeiro é TROCA». */
       OP_ADD16, OP_SUB16, OP_CMP16,
       /* e o PRODUTO do andar, que faltava: sem ele o compilador multiplicava
        * CONTANDO — um laço de |Y| voltas —, e um contador do par com o átomo
        * alto fora da conta nunca chegava a zero. No fim, como manda a VOLTA. */
       OP_MUL16,
       /* O ESPALHAMENTO, e é ele que tira o último salto do avaliador. Devolve a
        * máscara INTEIRA se o argumento é não-nulo, e zero se é nulo: o booleano
        * deixa de viver na coordenada 0 e passa a viver em todas. Com ele o teste
        * de uma condição não ramifica — `<` e `>` são o bit de sinal espalhado, e
        * `=` é o complemento disso —, e o molde da linha recebe já a máscara sem
        * ter de a fabricar com `0 − ACC`. No FIM, como manda a VOLTA. */
       OP_ESPALHA,
       /* Endereçamento indireto (pipe wasm→ERG): o operando aponta ao slot que
        * guarda o índice u16 do destino; LOADS lê, STORE_IND grava. No FIM. */
       OP_STORE_IND,
       /* VINCO: SUB face ⊕ + flags←zero(R) — fis:thm:simbolos (4); evita STORE·LOAD·CMP */
       OP_VINCO,
       /* INC slot: slot←slot+1 Word — laço i++/nout++ num passo (fis:algoritmo escrita) */
       OP_INC };

#define FL_ZERO 0x01
#define FL_EQ   0x02
#define FL_LT   0x04

#endif
