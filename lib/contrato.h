/* contrato.h — O CONTRATO, COMO FERRAMENTA. O sistema não julga o nome; verifica a álgebra.
 *
 * De `chess/elementares/index.tex`: "Cada estrutura desta série herda o mesmo contrato: uma SOMA,
 * uma MULTIPLICAÇÃO, uma DUALIDADE e um OPERADOR. O que muda é o DICIONÁRIO."
 *
 * São QUATRO cláusulas, não três. Eu vinha dizendo três — ⊕ ⊗ ∏ — e faltava a DUALIDADE, que é
 * justamente a que o Aarão apontou: "deve obedecer à álgebra do corpo E deve ser dual".
 *
 * O ponto desta ferramenta: o cliente declara o corpo que quiser, com o nome que quiser — o corpo
 * dos unicórnios coloridos, o corpo das cores, o que for — e nomeia as funções como lhe apetecer.
 * O sistema NÃO FAZ JUÍZO DE VALOR sobre o nome nem sobre o domínio. Ele verifica o contrato:
 *
 *     A1 ⊕ associa      A2 ⊕ comuta      A3 neutro 0      A4 todo x tem oposto
 *     M1 ⊗ associa      M2 ⊗ comuta      M3 neutro 1≠0    M4 todo x≠0 tem inverso
 *     D  distributiva
 *     ν1 a dualidade é INVOLUÇÃO: ν∘ν = id
 *     ν2 e respeita a estrutura: ν(a⊕b) = ν(a)⊕ν(b), ν(a⊗b) = ν(a)⊗ν(b)
 *     Π  o operador é MORFISMO: ou ∏(a⊕b)=∏(a)⊕∏(b) (endo, o gato/Frobenius), ou
 *        ∏(a⊕b)=∏(a)⊗∏(b) (Pontryagin, o caractere). O catálogo usa as duas formas — exigir
 *        só uma seria eu escolher pelo cliente.
 *
 * REVISÃO (o Aarão: "revisa o contrato pq agora mudou; Pontryagin é sempre o cartesiano, aí
 * precisa fornecer apenas um eixo, uma função, pq a outra é dual e Pontryagin calcula na hora"):
 *
 * NA FORMA DE PONTRYAGIN, O Π NÃO É CLÁUSULA — É CONSEQUÊNCIA. Uma cláusula é algo que o cliente
 * DECLARA e o sistema VERIFICA. Mas o caractere não se declara: dado ⊕, ele calcula-se, e são
 * exatamente tantos quantos o grupo (medido em dif.c §F12: Z/n tem n caracteres, e saem de n).
 * Verificar que ele é morfismo é verificar uma coisa construída para o ser.
 *
 * E do lado das FUNÇÕES o mesmo: o cliente dá UMA, e a do outro eixo é a transformada dela — a
 * volta é exata, logo não se perdeu nem se acrescentou nada. Pedir as duas era pedir a mais.
 *
 * Fica: o cliente declara ⊕, ⊗ e ν, e UMA função. O Π e o segundo eixo saem. O contrato tem
 * menos cláusulas e mais força — o que era verificação passou a construção, que é sempre o sinal
 * de se ter percebido a peça. A cláusula fica escrita acima porque a forma ENDO (o gato,
 * Frobenius) continua a ser declarada e verificada; é só a de Pontryagin que se calcula.
 *
 * Passar tudo é CUMPRIR O CONTRATO. Falhar diz-se pela CLÁUSULA, não por veredito: "falha em M4"
 * é informação; "não é corpo" é juízo, e o juízo não é do sistema.
 *
 * Sem alocação: o cliente dá um domínio FINITO enumerado por índice, e tudo corre na pilha.
 */
#ifndef CONTRATO_H
#define CONTRATO_H
#include <stdio.h>
#include "corpos.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

/* as cláusulas, como bits — para se poder dizer QUAL falhou */
#define C_A1 (1u<<0)
#define C_A2 (1u<<1)
#define C_A3 (1u<<2)
#define C_A4 (1u<<3)
#define C_M1 (1u<<4)
#define C_M2 (1u<<5)
#define C_M3 (1u<<6)
#define C_M4 (1u<<7)
#define C_D  (1u<<8)
#define C_N1 (1u<<9)
#define C_N2 (1u<<10)
#define C_PI (1u<<11)
#define C_TODAS 0xFFFu
#define C_ALGEBRA (C_A1|C_A2|C_A3|C_A4|C_M1|C_M2|C_M3|C_M4|C_D)
#define C_DUAL    (C_N1|C_N2)

static const char *ct_clausula(int i){
    static const char *n[12] = { "A1 ⊕ associa", "A2 ⊕ comuta", "A3 neutro 0", "A4 oposto",
                                 "M1 ⊗ associa", "M2 ⊗ comuta", "M3 neutro 1", "M4 inverso",
                                 "D  distributiva", "ν1 involução", "ν2 estrutura",
                                 "Π  morfismo" };
    return (i >= 0 && i < 12) ? n[i] : "?";
}

/* ---- A RÉGUA COMO FERRAMENTA: dar a NORMA em vez do dual, e o sistema deriva ----
 *
 * As operações são as mesmas em todo corpo — o catálogo di-lo, e está medido. O que distingue um
 * corpo de outro é a RÉGUA. E a régua não só caracteriza: ela COMPLETA O DUAL.
 *
 * Na família quadrática, com a borda x² = m·x + n, o conjugado é x' = m − x, e
 *
 *     N(a + b·x) = (a+b·x)(a+b·x') = a² + m·a·b − n·b²
 *
 * Escrevendo a norma como forma quadrática q(a,b) = a² + B·a·b + C·b², sai B = m e C = −n. Logo
 * a norma DÁ a borda, e a borda dá o dual: ν(a,b) = (a + B·b, −b). Não é escolha — é forçado.
 *
 * Então o cliente pode declarar a RÉGUA em vez da dualidade, e o sistema deriva a outra. */
typedef struct { long B, C; } Regua;                  /* q(a,b) = a² + B·a·b + C·b² */
static long ct_norma(Regua r, Par x){ return x.a*x.a + r.B*x.a*x.b + r.C*x.b*x.b; }
static Par  ct_dual_da_regua(Regua r, Par x){ Par y = { x.a + r.B*x.b, -x.b }; return y; }
static Par  ct_prod_da_regua(Regua r, Par x, Par y){   /* a borda que a régua impõe: n = −C */
    Par z = { x.a*y.a - r.C*x.b*y.b, x.a*y.b + x.b*y.a + r.B*x.b*y.b }; return z; }
static long ct_assinatura(Regua r){ return r.B*r.B - 4*r.C; }   /* >0 indef, =0 degen, <0 def */

/* O QUE O CLIENTE DECLARA. Os nomes das funções são dele; o sistema só as chama. */
typedef struct {
    const char *nome;                  /* o nome que o cliente quiser — não é verificado */
    long n;                            /* tamanho do domínio finito de teste */
    Par (*elem)(long i);               /* o i-ésimo elemento do domínio */
    Par (*soma)(Par, Par);             /* ⊕ */
    Par (*prod)(Par, Par);             /* ⊗ */
    Par (*dual)(Par);                  /* ν — a dualidade */
    Par (*op)(Par);                    /* ∏ — o operador que costura */
    int (*igual)(Par, Par);
    Par zero, um;
    /* A SIMPLIFICAÇÃO: se o cliente dá a RÉGUA, a dualidade vem dela e não se declara.
     * Campos no FIM de propósito — os inicializadores posicionais que já existem continuam
     * válidos, e ficam com tem_regua = 0, isto é, dual declarado à mão. */
    int tem_regua;
    Regua regua;
} Contrato;

/* a dualidade efetiva: da régua se houver régua, senão a declarada */
static Par ct_dual(const Contrato *c, Par x){
    return c->tem_regua ? ct_dual_da_regua(c->regua, x) : c->dual(x);
}

/* devolve a máscara das cláusulas que PASSARAM. O que não está na máscara falhou. */
static unsigned ct_verifica(const Contrato *c){
    unsigned ok_ = 0;
    long n = c->n;
    /* A1, A2, D, M1, M2 — os que precisam de tripla */
    int a1 = 1, a2 = 1, m1 = 1, m2 = 1, dd = 1;
    for(long i = 0; i < n; i++) for(long j = 0; j < n; j++){
        Par x = c->elem(i), y = c->elem(j);
        if(!c->igual(c->soma(x,y), c->soma(y,x))) a2 = 0;
        if(!c->igual(c->prod(x,y), c->prod(y,x))) m2 = 0;
        for(long k = 0; k < n; k++){
            Par z = c->elem(k);
            if(!c->igual(c->soma(c->soma(x,y),z), c->soma(x,c->soma(y,z)))) a1 = 0;
            if(!c->igual(c->prod(c->prod(x,y),z), c->prod(x,c->prod(y,z)))) m1 = 0;
            if(!c->igual(c->prod(x, c->soma(y,z)),
                         c->soma(c->prod(x,y), c->prod(x,z))))              dd = 0;
        }
    }
    if(a1) ok_ |= C_A1;
    if(a2) ok_ |= C_A2;
    if(m1) ok_ |= C_M1;
    if(m2) ok_ |= C_M2;
    if(dd) ok_ |= C_D;
    /* A3, M3 — os neutros */
    int a3 = 1, m3 = 1;
    for(long i = 0; i < n; i++){
        Par x = c->elem(i);
        if(!c->igual(c->soma(x, c->zero), x)) a3 = 0;
        if(!c->igual(c->prod(x, c->um),   x)) m3 = 0;
    }
    if(c->igual(c->um, c->zero)) m3 = 0;              /* 1 ≠ 0 é cláusula, não detalhe */
    if(a3) ok_ |= C_A3;
    if(m3) ok_ |= C_M3;
    /* A4, M4 — os inversos, EXIBIDOS por varredura do domínio */
    int a4 = 1, m4 = 1;
    for(long i = 0; i < n; i++){
        Par x = c->elem(i);
        int tem_op = 0, tem_inv = 0;
        for(long j = 0; j < n; j++){
            Par y = c->elem(j);
            if(c->igual(c->soma(x,y), c->zero)) tem_op = 1;
            if(c->igual(c->prod(x,y), c->um))   tem_inv = 1;
        }
        if(!tem_op) a4 = 0;
        if(!c->igual(x, c->zero) && !tem_inv) m4 = 0;
    }
    if(a4) ok_ |= C_A4;
    if(m4) ok_ |= C_M4;
    /* ν1, ν2 — A DUALIDADE, a cláusula que eu tinha esquecido */
    int n1 = 1, n2 = 1;
    if(!c->tem_regua && !c->dual) n1 = n2 = 0;
    else {
        for(long i = 0; i < n; i++){
            Par x = c->elem(i);
            if(!c->igual(ct_dual(c, ct_dual(c, x)), x)) n1 = 0;
            for(long j = 0; j < n; j++){
                Par y = c->elem(j);
                if(!c->igual(ct_dual(c, c->soma(x,y)),
                             c->soma(ct_dual(c,x), ct_dual(c,y)))) n2 = 0;
                if(!c->igual(ct_dual(c, c->prod(x,y)),
                             c->prod(ct_dual(c,x), ct_dual(c,y)))) n2 = 0;
            }
        }
    }
    if(n1) ok_ |= C_N1;
    if(n2) ok_ |= C_N2;
    /* Π — o operador é MORFISMO, numa das duas formas. O catálogo usa as duas: o gato e o
     * Frobenius levam ⊕ a ⊕; o caractere e o exp/log levam ⊕ a ⊗. Exigir só uma seria eu
     * escolher pelo cliente, e o sistema não escolhe. */
    int pia = 1, pim = 1;
    if(!c->op) pia = pim = 0;
    else for(long i = 0; i < n; i++) for(long j = 0; j < n; j++){
        Par x = c->elem(i), y = c->elem(j);
        if(!c->igual(c->op(c->soma(x,y)), c->soma(c->op(x), c->op(y)))) pia = 0;
        if(!c->igual(c->op(c->soma(x,y)), c->prod(c->op(x), c->op(y)))) pim = 0;
    }
    if(pia || pim) ok_ |= C_PI;
    return ok_;
}

/* imprime o veredito por CLÁUSULA — sem juízo de valor sobre o nome */
static void ct_relata(const Contrato *c, unsigned m){
    printf("      %-26s ", c->nome);
    for(int i = 0; i < 12; i++) printf("%s", (m & (1u<<i)) ? "✓" : "·");
    printf("   %s\n", (m & C_TODAS) == C_TODAS ? "CUMPRE"
                    : ((m & C_ALGEBRA) == C_ALGEBRA ? "álgebra ok, falta dual/Π"
                                                    : "falha em cláusula"));
}
static void ct_faltas(unsigned m){
    int primeiro = 1;
    for(int i = 0; i < 12; i++) if(!(m & (1u<<i))){
        printf("%s%s", primeiro ? "" : ", ", ct_clausula(i)); primeiro = 0;
    }
    if(primeiro) printf("nenhuma");
}

#pragma GCC diagnostic pop
#endif
