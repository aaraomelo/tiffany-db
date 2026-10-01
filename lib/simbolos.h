/* ═══════════════════════════════════════════════════════════════════════════
 * lib/simbolos.h — OS SEIS SINAIS SÃO OITO, E SAEM DE UMA DOBRA SOBRE OS PARES.
 *
 * O `fisica.tex §fis:simbolos` (Teor. `fis:thm:simbolos`) prova que os símbolos
 * de relação NÃO são vocabulário primitivo: a transposição τ(x,y)=(y,x) é uma
 * dobra sobre S×S, a cláusula (3) parte S×S em TRÊS blocos sem resto — o vinco
 * e os dois lados —, e uma relação é uma ESCOLHA de quais blocos se tomam.
 * Escolhas de três coisas há 2³:
 *
 *      três blocos  ⟹  2³ = 8 relações, e nem uma a mais.
 *
 * Este ficheiro é essa contagem, e mais nada. Uma relação é UM BYTE com três
 * bits acesos ou apagados, e o índice dela é o subconjunto de blocos que ela
 * toma, lido em binário — a mesma leitura do `fis:thm:largura` em w = 3.
 *
 *      >  =  <   relação   o que diz
 *      0  0  0   nula      nunca
 *      0  0  1   <         um lado
 *      0  1  0   =         o vinco
 *      0  1  1   ≤         o lado com o vinco
 *      1  0  0   >         o outro lado
 *      1  0  1   ≠         os dois lados, sem o vinco
 *      1  1  0   ≥         o outro lado com o vinco
 *      1  1  1   total     sempre
 *
 * ── E A NULA E A TOTAL NÃO SÃO ENFEITE ────────────────────────────────────────
 * «Sem a nula e a total são seis, e seis não é potência de dois»: é o nulo que
 * faz o quadro fechar. E não é só aritmética de contagem — elas são as duas
 * relações que NÃO PRECISAM DA LINHA, e é por isso que quem compila com este
 * ficheiro decide-as em compilação em vez de emitir código para as testar.
 *
 * ── AS DUAS DOBRAS DA CLÁUSULA (9), E ELAS COMUTAM ────────────────────────────
 * A COMPLEMENTAÇÃO toma os blocos que ficaram de fora: troca ∅ com a total, =
 * com ≠, < com ≥ e > com ≤ — quatro pares, e NENHUM fixo, porque três blocos
 * não se repartem ao meio. A TRANSPOSIÇÃO τ troca < com > e ≤ com ≥, e fixa as
 * outras quatro. As duas comutam — uma mexe em QUAIS blocos, a outra em QUAL
 * lado —, pelo que geram o quarteto.
 *
 * ── A CISÃO EM QUATRO E QUATRO, E ELA É DE CARDINAL ───────────────────────────
 * Uma relação ignora a ordem SE E SÓ SE lê apenas o CARDINAL da intersecção com
 * o par {<,>} — zero ou dois — e não QUAL dos dois tomou:
 *
 *      cardinal PAR   (0 ou 2)  →  reversível  →  ∅, =, ≠, total  →  DIRECTO
 *      cardinal ÍMPAR (1)       →  orientada   →  <, >, ≤, ≥      →  CRUZADO
 *
 * «Contar não orienta — é preciso ESCOLHER, e escolher é o bit da cláusula (5).»
 *
 * ── O QUE ESTE FICHEIRO NÃO FAZ ───────────────────────────────────────────────
 * Não escolhe o bit. A orientação — qual lado se chama `<` — é a cláusula (5),
 * e o `fis:thm:ordem` diz que esta obra a escolhe UMA VEZ, nos dois símbolos, e
 * o resto herda. Aqui herda-se: `0 < 1`, e é a mesma escolha do `tests/ordem.c`.
 * E não faz transitividade: uma orientação não é ainda uma ordem — a coerência
 * entre blocos é o terceiro custo, e paga-se por indução na largura, não aqui.
 *
 * O `∈` também não entra: pelo `fis:cor:pertenca` ele não é um nono sinal, é o
 * `=` da cláusula (4) composto com uma aplicação já definida. E o `∝` fica de
 * fora por outra razão — custa a segunda face (`fis:cor:prop`), e este ficheiro
 * só tem a dobra.
 *
 * Medido em `tests/simbolos.c`: as oito varridas inteiras, e os pares e ternos
 * delas — não há amostra, porque oito cabe todo.
 *
 *   cc -O2 -std=c99 -Ilib -o /tmp/simbolos tests/simbolos.c && /tmp/simbolos
 * ═══════════════════════════════════════════════════════════════════════════ */
#ifndef SIMBOLOS_H
#define SIMBOLOS_H

/* ── (3) OS TRÊS BLOCOS, E O ÍNDICE DE CADA UM ────────────────────────────────
 * A ordem dos bits é a da tabela lida ao contrário — o bloco `<` no bit 0 — para
 * que o índice da relação seja o número da linha da tabela, e não outro. */
enum { SB_B_LT = 0,      /* um lado                    */
       SB_B_EQ = 1,      /* o vinco: Fix(τ) = {(x,x)}  */
       SB_B_GT = 2 };    /* o outro lado               */

/* ── (8) AS OITO, E O ÍNDICE É O SUBCONJUNTO LIDO EM BINÁRIO ──────────────── */
enum {
    SB_NULA  = 0,        /* 000  nunca                          */
    SB_LT    = 1,        /* 001  <                              */
    SB_EQ    = 2,        /* 010  =   o vinco                    */
    SB_LE    = 3,        /* 011  ≤ = < ⊔ =   (cláusula (7))     */
    SB_GT    = 4,        /* 100  >                              */
    SB_NE    = 5,        /* 101  ≠   os dois lados, sem o vinco */
    SB_GE    = 6,        /* 110  ≥ = > ⊔ =   (cláusula (7))     */
    SB_TOTAL = 7         /* 111  sempre                         */
};
#define SB_N       8                             /* e nem uma a mais */
#define SB_MASCARA 7                             /* três blocos      */
#define SB_LADOS   (SB_LT | SB_GT)               /* o tecido: S×S menos o vinco */
#define SB_VINCO   SB_EQ                         /* Fix(τ)                      */

/* ── A ÁRVORE DAS PERGUNTAS, QUE É O QUE A COMPUTAÇÃO CORRE ───────────────────
 * «A relação colapsa? A separação orienta-se? O vinco entra?» — três perguntas,
 * e cada uma só faz sentido depois da anterior. Uma vez partido o par no bloco a
 * que pertence, responder é UM DESLOCAMENTO E UM AND: a relação já traz escritas
 * as três respostas. Não são seis testes soltos; é uma consulta a três bits. */
static int sb_bloco(long d){                     /* em que bloco cai o par */
    return d < 0 ? SB_B_LT : (d == 0 ? SB_B_EQ : SB_B_GT);
}
static int sb_vale(int m, long d){               /* a relação `m` toma esse bloco? */
    return (m >> sb_bloco(d)) & 1;
}

/* ── (9) AS DUAS DOBRAS, E ELAS COMUTAM ──────────────────────────────────────
 * A complementação mexe em QUAIS blocos; a τ mexe em QUAL lado. Agem em coisas
 * distintas, e é por isso — e não por acaso — que comutam. */
static int sb_compl(int m){                      /* os blocos que ficaram de fora */
    return (~m) & SB_MASCARA;
}
static int sb_tau(int m){                        /* troca o lado: (x,y) ↦ (y,x) */
    return (m & SB_VINCO)
         | ((m & SB_LT) << (SB_B_GT - SB_B_LT))
         | ((m & SB_GT) >> (SB_B_GT - SB_B_LT));
}

/* ── A CISÃO, E ELA LÊ-SE NO CARDINAL ─────────────────────────────────────────
 * Cardinal par não orienta; cardinal ímpar orienta, e é só isso que separa as
 * duas metades. Note-se que `sb_orientada` NÃO consulta a tabela dos nomes: ela
 * conta, e a contagem decide — é a mesma leitura do `G` do `fis:thm:medida`,
 * que conta e não distingue quem contou. */
static int sb_pop(int m){                        /* quantos blocos a relação toma */
    return ((m >> SB_B_LT) & 1) + ((m >> SB_B_EQ) & 1) + ((m >> SB_B_GT) & 1);
}
static int sb_orientada(int m){                  /* lê a ordem — o lado CRUZADO   */
    return sb_pop(m & SB_LADOS) == 1;
}
static int sb_reversivel(int m){                 /* ignora a ordem — o DIRECTO    */
    return !sb_orientada(m);
}

/* ── O PAR (op, nega) É DERIVADO, E NÃO ESCRITO ───────────────────────────────
 * Quem emite código para uma máquina que só tem um salto por ZERO não conhece
 * oito relações: conhece os TRÊS blocos únicos e um XOR. E isso não é uma
 * segunda tabela a manter ao lado desta — é a cláusula (9) a agir: toda relação
 * de dois blocos é a COMPLEMENTAR de uma de um bloco só, e negar é complementar.
 *
 *      nega  =  a relação toma dois blocos  ⟺  a complementar toma um
 *      op    =  o bloco único, dela ou da complementar
 *
 * A nula e a total não têm `op`, e é correcto que não tenham: são as duas que
 * `sb_pop` dá 0 e 3, as duas que a complementação troca uma pela outra, e as
 * duas que se decidem sem ler a linha. Quem receber `op == 0` não deve emitir
 * teste nenhum — deve responder `sb_constante`. */
static int sb_nega(int m){
    return sb_pop(m) >= 2;
}
static int sb_op(int m){                         /* '<', '=', '>' — ou 0 */
    int u = sb_nega(m) ? sb_compl(m) : m;
    if(u == SB_LT) return '<';
    if(u == SB_EQ) return '=';
    if(u == SB_GT) return '>';
    return 0;                                    /* a nula e a total */
}
static int sb_decidida(int m){                   /* dispensa a linha?    */
    return m == SB_NULA || m == SB_TOTAL;
}
static int sb_constante(int m){                  /* e vale o quê         */
    return m == SB_TOTAL;
}

/* ── A CANONIZAÇÃO: τ APLICADA AO PAR *E* À RELAÇÃO NÃO MUDA A VERDADE ────────
 * `L op R` escreve-se `(L−R) op 0`, e trocar o par por `(R−L)` é a τ a agir do
 * lado dos pontos. Pela cláusula (5) ela troca `<` com `>`; logo aplicá-la aos
 * DOIS lados ao mesmo tempo — negar a diferença e transpor a relação — deixa a
 * verdade onde estava. É isso que autoriza um compilador a escolher um
 * representante por bloco em vez de emitir dois códigos para o mesmo facto.
 *
 * `sb_transpor_pede` diz quando fazê-lo, e a regra é: leva-se sempre a relação
 * para o lado ESCOLHIDO, que é o `>`. E as τ-fixas — ∅, =, ≠, total — não pedem
 * nada, porque para elas o par e o seu transposto são o mesmo facto: quem quiser
 * um representante ali tem de o escolher por outro critério, e não pela dobra. */
static int sb_transpor_pede(int m){
    return sb_op(m) == '<';
}
static int sb_tau_fixa(int m){
    return sb_tau(m) == m;
}

/* ── O ALCANCE: DUAS RELAÇÕES INTERSECTAM-SE, OU NÃO ──────────────────────────
 * Saber se uma condição PODE ser satisfeita num intervalo `[baixo, alto]` não é
 * um caso por operador: os blocos que o intervalo alcança são eles próprios uma
 * relação, e a pergunta é se as duas se cruzam. Um `&` decide, e decide para as
 * oito — inclusive para aquelas cujo `op` derivado é o do complemento, que é
 * exactamente onde uma tabela de três casos se engana e responde VAZIO a quem
 * tinha resposta. */
static int sb_alcance(long long baixo, long long alto){
    int b = 0;
    if(baixo <  0)               b |= 1 << SB_B_LT;
    if(baixo <= 0 && alto >= 0)  b |= 1 << SB_B_EQ;
    if(alto  >  0)               b |= 1 << SB_B_GT;
    return b;
}
static int sb_possivel(int m, long long baixo, long long alto){
    return (m & sb_alcance(baixo, alto)) != 0;
}

/* ── A ESCRITA, E A LEITURA ───────────────────────────────────────────────────
 * Seis têm sinal em SQL; duas não têm, e diz-se em vez de se fingir. O `<>` e o
 * `!=` leem-se os dois e escrevem-se num só: são a mesma relação, e a contração
 * do WHERE conta com isso. */
static const char *sb_escreve(int m){
    switch(m & SB_MASCARA){
        case SB_NULA:  return "nula";
        case SB_LT:    return "<";
        case SB_EQ:    return "=";
        case SB_LE:    return "<=";
        case SB_GT:    return ">";
        case SB_NE:    return "<>";
        case SB_GE:    return ">=";
        default:       return "total";
    }
}
/* devolve a relação e escreve em `*n` quantos caracteres consumiu; `*n == 0`
 * quer dizer «ali não há sinal nenhum», e não «há um que eu não conheço». */
static int sb_le(const char *s, int *n){
    *n = 0;
    if(!s || !s[0]) return SB_NULA;
    if(s[0] == '!' && s[1] == '=') { *n = 2; return SB_NE; }
    if(s[0] == '<' && s[1] == '>') { *n = 2; return SB_NE; }
    if(s[0] == '<' && s[1] == '=') { *n = 2; return SB_LE; }
    if(s[0] == '>' && s[1] == '=') { *n = 2; return SB_GE; }
    if(s[0] == '=' && s[1] == '=') { *n = 2; return SB_EQ; }   /* tolerado */
    if(s[0] == '=')                { *n = 1; return SB_EQ; }
    if(s[0] == '<')                { *n = 1; return SB_LT; }
    if(s[0] == '>')                { *n = 1; return SB_GT; }
    return SB_NULA;
}

#endif /* SIMBOLOS_H */
