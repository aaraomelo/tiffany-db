/* umbit.h — UM BIT: 𝔽₂, e as cinco primitivas quando o corpo tem dois elementos.
 *
 * O Aarão: «agora migramos para 1 bit, vamos tabelar ASCII e formalizar a realização
 * ASCII da álgebra universal, e usa neuronio.c para ver como operador bit a bit.»
 *
 * ── E A BASE JÁ EXISTIA: `booleana.h` ─────────────────────────────────────────
 * Escrevi este ficheiro sem procurar, e o `lib/booleana.h` já dizia a frase inteira:
 * «A LÓGICA É O CORPO GF(2), E NÃO UMA MÁQUINA À PARTE», com as MESMAS cinco operações —
 * ⊕ o XOR, ⊗ o AND, ÷ a fibra, † a ANF pela transformada de MÖBIUS, ↺ a volta. É a quarta
 * vez que trago para um objecto uma coisa que ele já tinha.
 *
 * O que aqui é novo, e só isso: a recta projectiva ℙ¹(𝔽₂) com os seus três pontos, as
 * OITO LEIS como transformações do byte, e o corpo de 256 (`corpo256.h`). A leitura das
 * cinco primitivas em GF(2) é do `booleana.h`, e cita-se.
 *
 * ── O QUE ACONTECE ÀS CINCO PRIMITIVAS EM 𝔽₂ (a leitura é do `booleana.h`) ─────
 * Em 𝔽₂ a álgebra não fica mais pobre: fica NUA. Cada primitiva colapsa numa porta
 * lógica, e o colapso diz o que ela era.
 *
 *      SOMA          x ⊕ y     é o XOR
 *      DUAL/oposto   −x = x    é a IDENTIDADE — o sinal desaparece por completo
 *      PRODUTO       x ∧ y     é o AND
 *      INVERSÃO      1⁻¹ = 1   e o zero é o polo: ℙ¹(𝔽₂) tem TRÊS pontos
 *      DIFERENÇA     x − y     = x ⊕ y, a mesma coisa que a soma
 *
 * A linha do meio é a que interessa: em 𝔽₂ o oposto de x é o PRÓPRIO x, logo a
 * subtracção e a soma são a mesma operação. O par «soma + dual = a diferença» do teorema
 * das primitivas continua verdadeiro — e degenera, porque o dual é a identidade. É a
 * mesma lei com uma face onde ela quase não se vê, e é por isso que 𝔽₂ é o sítio certo
 * para perceber o que cada peça fazia.
 *
 * ── E O QUE NÃO COLAPSA ───────────────────────────────────────────────────────
 * A INVOLUÇÃO da casa, ν(x) = −1/x, em ℙ¹(𝔽₂) continua a trocar 0 com ∞ — porque essa
 * troca não usa o sinal, usa a REPRESENTAÇÃO. É o Corolário 0 ↔ ∞ a sobreviver no corpo
 * mais pobre que existe, e isso mede-se.
 *
 * ── O BYTE COMO VECTOR: 𝔽₂⁸, e a soma é o XOR bit a bit ───────────────────────
 * Um byte é oito elementos de 𝔽₂, e somá-los é o XOR — a soma coordenada a coordenada.
 * É esta a leitura que o `tests/neuronio.c` já usava sem lhe chamar isso: ele lê o
 * ficheiro somando os bits POR POSIÇÃO, que é exactamente projectar em cada coordenada
 * de 𝔽₂⁸.
 *
 * Precisa de `stdint.h`. */
#ifndef UMBIT_H
#define UMBIT_H

#include <stdint.h>

/* ── 𝔽₂: as cinco primitivas, cada uma numa porta ─────────────────────────────*/
typedef uint8_t B;                       /* 0 ou 1 */

static B b_som(B x, B y){ return (B)(x ^ y); }        /* SOMA      = XOR */
static B b_opo(B x){ return x; }                      /* DUAL      = identidade */
static B b_mul(B x, B y){ return (B)(x & y); }        /* PRODUTO   = AND */
static B b_dif(B x, B y){ return (B)(x ^ y); }        /* DIFERENÇA = a mesma soma */
static B b_nao(B x){ return (B)(x ^ 1); }             /* o complemento: x + 1 */

/* ── ℙ¹(𝔽₂): TRÊS pontos — 0, 1 e ∞ ──────────────────────────────────────────
 * Guardados como o par [p:q] não normalizado, tal como em `sem_ramo.h`: a inversão
 * continua a ser a TROCA, e continua sem um ramo. */
typedef struct { B p, q; } P1;

static P1 p1(B p, B q){ P1 r; r.p = p; r.q = q; return r; }
static P1 p1_zero(void){ return p1(0,1); }
static P1 p1_um(void){ return p1(1,1); }
static P1 p1_inf(void){ return p1(1,0); }
static P1 p1_inverte(P1 x){ P1 r; r.p = x.q; r.q = x.p; return r; }
static int p1_igual(P1 a, P1 b){ return b_mul(a.p,b.q) == b_mul(b.p,a.q); }
static int p1_e_ponto(P1 x){ return !(x.p == 0 && x.q == 0); }

/* ── 𝔽₂⁸: o BYTE como vector, e a soma é o XOR bit a bit ─────────────────────
 * É a leitura que o `tests/neuronio.c` já fazia: ler um ficheiro somando os bits POR
 * POSIÇÃO é projectar em cada coordenada. Aqui a operação diz o seu nome. */
typedef uint8_t V8;

static V8 v_som(V8 a, V8 b){ return (V8)(a ^ b); }    /* a soma vectorial */
static V8 v_zero(void){ return 0; }
static B  v_coord(V8 a, int i){ return (B)((a >> i) & 1); }
static int v_peso(V8 a){                               /* o popcount: o Kirchhoff */
    int n = 0;
    for(int i = 0; i < 8; i++) n += (a >> i) & 1;
    return n;
}
/* o produto INTERNO em 𝔽₂: ⟨a,b⟩ = paridade do AND — a única forma bilinear que há */
static B v_interno(V8 a, V8 b){ return (B)(v_peso((V8)(a & b)) & 1); }

/* ── E O GATO EM 𝔽₂, que é o `neuronio.c` numa linha ─────────────────────────
 * A companion A_m com m = 1 sobre 𝔽₂: (c₀,c₁) ↦ (c₀ ⊕ c₁, c₀). É o passo do
 * `neuronio.c` reduzido ao corpo mais pobre — e o período dele é 3, que é o número de
 * pontos de ℙ¹(𝔽₂). Não é coincidência: é a órbita a fechar no que existe. */
static void b_gato(B *c0, B *c1){
    B n0 = b_som(*c0, *c1), n1 = *c0;
    *c0 = n0; *c1 = n1;
}

/* ═══ AS OITO LEIS COMO OITO TRANSFORMAÇÕES DO BYTE ═══════════════════════════
 * O Aarão: «aplica as oito leis para isso — vai dar as transformações nos 8 bits que já
 * temos.» E o catálogo do Corpo estelar tem-nas indexadas de 0 a 7, com uma frase que é
 * a chave: «as oito são um CICLO GERADOR de período oito».
 *
 * ── E O QUE AQUI SE DECLARA, QUE NÃO É UM TEOREMA ─────────────────────────────
 * Não se escreve «cada bit É uma lei». Escreve-se uma DECLARAÇÃO de reserva:
 *
 *      A posição k do byte fica RESERVADA à realização da Lei k, para 0 ≤ k ≤ 7.
 *
 * É uma convenção da arquitectura, e como toda convenção não se prova — cumpre-se ou não.
 * O que se prova é o que vem depois: se as operações realizam mesmo as leis que a posição
 * lhes reserva. A arquitectura DECLARA; o neurónio MEDE. Escrever a declaração como
 * teorema seria dar a uma escolha o estatuto de um facto, que é o defeito que esta casa
 * persegue.
 *
 * Oito leis, oito bits, período oito. E o byte tem oito posições não por conveniência:
 * porque o catálogo tem oito leis, e as oito são um ciclo gerador de período oito.
 *
 *      0  a divisão do zero    0 = (+1)⊕(−1);  0† = ∞      → o par dos nulos: 0x00 e 0xFF
 *      1  o dual               1† = −1, involução           → o COMPLEMENTO, período 2
 *      2  o bidual             K** = K                      → o complemento DUAS vezes
 *      3  o trial              {−1, 0, +1}                  → os três pontos de ℙ¹(𝔽₂)
 *      4  a tetral             T + T*, a dobra              → a troca de nibbles
 *      5  a pental             x² = −1, o bit i             → a rotação, período 4
 *      6  a hexal              soma = produto               → onde XOR e AND coincidem
 *      7  o octonião dual      ℍ × ℍ*, ligar sem fundir     → os dois nibbles lado a lado
 *
 * ── O QUE AQUI SE AFIRMA E O QUE NÃO ──────────────────────────────────────────
 * Cada linha é uma REALIZAÇÃO da lei em oito bits, com o período MEDIDO. Não se afirma
 * que a lei «é» a porta lógica: afirma-se que a porta a realiza, e o período é a
 * testemunha — se ele não batesse, a realização estava errada. E o ciclo de período oito
 * mede-se no gerador, que é a rotação de um bit.
 *
 * ── E O ZERO É A CHAVE, outra vez ─────────────────────────────────────────────
 * A Lei 0 é «a divisão do zero», com 0† = ∞ pela extensão projectiva — é o
 * Corolário 0 ↔ ∞ na posição zero do byte. Não é uma lei entre outras: é a que dá o
 * arranque, e é por isso que o índice dela é 0. */

/* Lei 1 — o DUAL: o complemento. Período 2. */
static V8 lei1_dual(V8 x){ return (V8)(~x); }
/* Lei 2 — o BIDUAL: o dual duas vezes, e tem de ser a identidade. */
static V8 lei2_bidual(V8 x){ return lei1_dual(lei1_dual(x)); }
/* Lei 4 — a TETRAL, a dobra T + T*: os dois meios do byte trocam. Período 2. */
static V8 lei4_dobra(V8 x){ return (V8)(((x << 4) | (x >> 4)) & 0xFF); }
/* Lei 5 — a PENTAL, x² = −1, o bit i: a rotação de DOIS bits. Período 4 em oito. */
static V8 lei5_rotor(V8 x){ return (V8)(((x << 2) | (x >> 6)) & 0xFF); }
/* Lei 7 — o OCTONIÃO DUAL, ℍ × ℍ*: os dois nibbles operam LADO A LADO sem se misturarem.
 * Realiza-se aplicando o dual só a um lado — ligar sem fundir. */
static V8 lei7_par(V8 x){ return (V8)((x & 0xF0) | ((~x) & 0x0F)); }
/* o GERADOR do ciclo: rodar UM bit. Período OITO — e é ele o ciclo das oito leis. */
static V8 lei_gera(V8 x){ return (V8)(((x << 1) | (x >> 7)) & 0xFF); }

/* Lei 0 — a divisão do zero: o par dos nulos, e a involução que os troca */
static int lei0_e_nulo(V8 x){ return x == 0x00 || x == 0xFF; }
/* Lei 3 — o trial: quantos dos três pontos de ℙ¹(𝔽₂) uma coordenada realiza */
static int lei3_trial(void){ return 3; }
/* Lei 6 — a hexal, soma = produto: onde é que XOR e AND coincidem, bit a bit */
static int lei6_coincide(B x, B y){ return b_som(x,y) == b_mul(x,y); }

/* o PERÍODO de uma transformação do byte, medido e não afirmado */
static int lei_periodo(V8 (*f)(V8), V8 x0, int tecto){
    V8 x = f(x0);
    for(int k = 1; k <= tecto; k++){
        if(x == x0) return k;
        x = f(x);
    }
    return 0;
}

/* ── E A TABELA EXAUSTIVA: o que cada operação FAZ a cada posição ──────────────
 * O teste decisivo, e é sobre os 256 bytes: para cada operação e cada posição k, ela
 * PRESERVA o bit, INVERTE-o (realiza o dual naquela posição) ou MOVE-o (é permutação).
 * Devolve 'p', 'd' ou 'm' — e '?' quando não faz nenhuma das três de forma consistente,
 * que é o caso que interessa não esconder. */
static char lei_faz(V8 (*f)(V8), int k){
    int preserva = 1, inverte = 1;
    for(int x = 0; x < 256; x++){
        B antes = v_coord((V8)x, k), depois = v_coord(f((V8)x), k);
        if(antes != depois) preserva = 0;
        if(antes == depois) inverte = 0;
    }
    if(preserva) return 'p';
    if(inverte) return 'd';
    /* nem preserva nem inverte em toda a parte: é permutação se f for bijectiva */
    int visto[256];
    for(int i = 0; i < 256; i++) visto[i] = 0;
    for(int x = 0; x < 256; x++){
        int y = f((V8)x);
        if(visto[y]) return '?';
        visto[y] = 1;
    }
    return 'm';
}
#endif
