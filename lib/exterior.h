/* exterior.h — Λ V, O HODGE, E O CRUZADO DA CASA.
 *
 * Reli as três teorias antes de escrever, e o andar já cá estava em três sítios:
 *
 * 1. `corpo_analitico.tex` §637: «o DIRECTO ⟨a,b⟩ = cos θ é a parte SIMÉTRICA — a potência
 *    ACTIVA; o CRUZADO ‖a∧b‖ = sin θ é a ANTISSIMÉTRICA — a REACTIVA, que roda e volta
 *    (o torque É o produto cruzado)». Isso é exatamente a decomposição de uma bilinear
 *    em simétrica ⊕ antissimétrica, e **Λ² É O CRUZADO DA CASA**.
 *
 * 2. `teoria.tex` §370: as cartas EQUIAREAIS, «o determinante do sector… cada
 *    carta PRESERVA ÁREA», e «sem a inversa-escala o det é 2 ≠ 1». Isso é Λⁿ T = det T,
 *    com det = 1 ⟺ preserva volume — já medido em `tests/arquimedes_area.js` (21:0).
 *
 * 3. E o fator de potência: fp = 1 ⟺ cruzado nulo ⟺ |det| = 1 — «três nomes da mesma
 *    condição». O motor quer fp = 1 (nada roda); o tecido quer fp = 0 (só cruzado).
 *
 * ── E O QUE NÃO É ──────────────────────────────────────────────────────────────
 * O «Hodge» que aparece nas teorias é a CONJECTURA de Hodge (ciclos algébricos, o
 * problema do milénio). O ⋆ deste ficheiro é o OPERADOR estrela de Hodge,
 * ⋆: Λᵏ → Λⁿ⁻ᵏ. São coisas diferentes com o mesmo nome, e não se juntam.
 *
 * ── A DESCOBERTA QUE A LEITURA DEU ─────────────────────────────────────────────
 * O produto cruzado de ℝ³ é ⋆(a ∧ b) — a cunha seguida do Hodge. E ele só é um VETOR
 * em dimensão 3, porque só aí dim Λ² = dim Λ¹ = 3. Noutra dimensão o cruzado é um
 * BIVETOR e não cabe no espaço de partida. É por isso que o «cruzado» da casa vive
 * onde vive.
 *
 * E o ⋆ é uma INVOLUÇÃO a menos de sinal (⋆⋆ = ±id) — o dual † desta casa, outra vez.
 *
 * ── A CONVENÇÃO ────────────────────────────────────────────────────────────────
 * n = 3 fixo (é onde Λ¹, Λ² e Λ³ cabem todos e o Hodge é visível). Λ¹ e Λ² têm 3
 * coordenadas cada, Λ³ tem 1. A base de Λ² é (e₁∧e₂, e₁∧e₃, e₂∧e₃), nesta ordem.
 *
 * Precisa de `racionais.h` e `linear.h`. */
#ifndef EXTERIOR_H
#define EXTERIOR_H

typedef struct { Qz c[3]; } Biv;              /* Λ²(ℚ³): (e₁∧e₂, e₁∧e₃, e₂∧e₃) */

static Biv biv0(void){ Biv b; for(int i = 0; i < 3; i++) b.c[i] = qz(0,1); return b; }
static int biv_igual(Biv a, Biv b){
    for(int i = 0; i < 3; i++) if(!qz_igual(a.c[i], b.c[i])) return 0;
    return 1;
}
static int biv_zero(Biv a){
    for(int i = 0; i < 3; i++) if(a.c[i].p) return 0;
    return 1;
}
static Biv biv_soma(Biv a, Biv b){
    Biv r; for(int i = 0; i < 3; i++) r.c[i] = qz_soma(a.c[i], b.c[i]);
    return r;
}
static Biv biv_esc(Qz l, Biv a){
    Biv r; for(int i = 0; i < 3; i++) r.c[i] = qz_mult(l, a.c[i]);
    return r;
}
/* ── O PRODUTO WEDGE Λ¹ × Λ¹ → Λ² ──────────────────────────────────────────────
 * (u∧v)_{ij} = uᵢvⱼ − uⱼvᵢ — os menores 2×2, que é o que a ALTERNÂNCIA obriga. */
static Biv ex_wedge(Vec u, Vec v){
    Biv r;
    r.c[0] = qz_soma(qz_mult(u.c[0],v.c[1]), qz_oposto(qz_mult(u.c[1],v.c[0])));  /* e₁∧e₂ */
    r.c[1] = qz_soma(qz_mult(u.c[0],v.c[2]), qz_oposto(qz_mult(u.c[2],v.c[0])));  /* e₁∧e₃ */
    r.c[2] = qz_soma(qz_mult(u.c[1],v.c[2]), qz_oposto(qz_mult(u.c[2],v.c[1])));  /* e₂∧e₃ */
    return r;
}
/* Λ¹ × Λ² → Λ³, que é um escalar: o PRODUTO MISTO e o volume */
static Qz ex_wedge3(Vec u, Biv b){
    /* u ∧ (α e₁∧e₂ + β e₁∧e₃ + γ e₂∧e₃) = (u₃α − u₂β + u₁γ) e₁∧e₂∧e₃ */
    Qz t1 = qz_mult(u.c[2], b.c[0]);
    Qz t2 = qz_oposto(qz_mult(u.c[1], b.c[1]));
    Qz t3 = qz_mult(u.c[0], b.c[2]);
    return qz_soma(qz_soma(t1,t2), t3);
}
static Qz ex_misto(Vec u, Vec v, Vec w){ return ex_wedge3(u, ex_wedge(v,w)); }

/* ── O HODGE ⋆ ────────────────────────────────────────────────────────────────
 * Em ℚ³ com a métrica usual: ⋆(e₁∧e₂) = e₃, ⋆(e₁∧e₃) = −e₂, ⋆(e₂∧e₃) = e₁.
 * E no outro sentido, ⋆e₁ = e₂∧e₃, ⋆e₂ = −e₁∧e₃, ⋆e₃ = e₁∧e₂.
 * Em ℝ³ o ⋆ é uma INVOLUÇÃO exata: ⋆⋆ = id nos dois graus. */
static Vec ex_hodge2(Biv b){                  /* Λ² → Λ¹ */
    Vec v = vec0(3);
    v.c[0] = b.c[2];                          /* e₂∧e₃ ↦ e₁ */
    v.c[1] = qz_oposto(b.c[1]);               /* e₁∧e₃ ↦ −e₂ */
    v.c[2] = b.c[0];                          /* e₁∧e₂ ↦ e₃ */
    return v;
}
static Biv ex_hodge1(Vec v){                  /* Λ¹ → Λ² */
    Biv b = biv0();
    b.c[2] = v.c[0];                          /* e₁ ↦ e₂∧e₃ */
    b.c[1] = qz_oposto(v.c[1]);               /* e₂ ↦ −e₁∧e₃ */
    b.c[0] = v.c[2];                          /* e₃ ↦ e₁∧e₂ */
    return b;
}
/* O PRODUTO CRUZADO É ⋆(a ∧ b) — e só é um VETOR porque em dimensão 3
 * dim Λ² = dim Λ¹ = 3. Noutra dimensão o cruzado é um BIVETOR e não cabe. */
static Vec ex_cruzado(Vec u, Vec v){ return ex_hodge2(ex_wedge(u,v)); }

/* ── A CONTRAÇÃO (produto interior) ι_v : Λᵏ → Λᵏ⁻¹ ───────────────────────────
 * ι_v(u∧w) = ⟨v,u⟩w − ⟨v,w⟩u. É a ANTIDERIVAÇÃO, e ι_v ∘ ι_v = 0 — o mesmo zero
 * duplo da alternância. */
static Vec ex_contrai(Vec v, Biv b){
    /* em coordenadas: (ι_v b)_k = Σ_j v_j b_{jk}, com b antissimétrica */
    Qz B[3][3];
    for(int i = 0; i < 3; i++) for(int j = 0; j < 3; j++) B[i][j] = qz(0,1);
    B[0][1] = b.c[0]; B[1][0] = qz_oposto(b.c[0]);
    B[0][2] = b.c[1]; B[2][0] = qz_oposto(b.c[1]);
    B[1][2] = b.c[2]; B[2][1] = qz_oposto(b.c[2]);
    Vec r = vec0(3);
    for(int k = 0; k < 3; k++){
        Qz s = qz(0,1);
        for(int j = 0; j < 3; j++) s = qz_soma(s, qz_mult(v.c[j], B[j][k]));
        r.c[k] = s;
    }
    return r;
}
/* ── A AÇÃO DE UM OPERADOR EM Λ² e Λ³ ─────────────────────────────────────────
 * Λ²T(u∧v) = Tu ∧ Tv, e Λ³T é a multiplicação por det T — «o fator pelo qual T atua
 * no espaço de VOLUME», que é o que ele diz. */
static Biv ex_acao2(Mat T, Biv b){
    /* pelos menores 2×2 de T: a matriz induzida em Λ² */
    Vec e[3];
    for(int i = 0; i < 3; i++){ e[i] = vec0(3); e[i].c[i] = qz(1,1); }
    Biv w12 = ex_wedge(mat_aplica(T,e[0]), mat_aplica(T,e[1]));
    Biv w13 = ex_wedge(mat_aplica(T,e[0]), mat_aplica(T,e[2]));
    Biv w23 = ex_wedge(mat_aplica(T,e[1]), mat_aplica(T,e[2]));
    return biv_soma(biv_soma(biv_esc(b.c[0], w12), biv_esc(b.c[1], w13)),
                    biv_esc(b.c[2], w23));
}
/* ── A ORIENTAÇÃO: o SINAL do determinante ────────────────────────────────────
 * +1 preserva, −1 inverte, 0 esmaga. E é isto a «transformação de orientação». */
static int ex_orientacao(Mat T){
    Qz d = mat_det(T);
    return d.p > 0 ? 1 : (d.p < 0 ? -1 : 0);
}
/* ── DIMENSÃO 4: Λ²(ℚ⁴) tem SEIS coordenadas ──────────────────────────────────
 * Precisa de existir, e não por completude: é em dimensão 4 que aparece o primeiro
 * bivetor NÃO SIMPLES, e sem ele o Pfaffiano é fórmula recitada. A base é
 * (e₁∧e₂, e₁∧e₃, e₁∧e₄, e₂∧e₃, e₂∧e₄, e₃∧e₄), nesta ordem. */
typedef struct { Qz c[6]; } Biv4;
static const int EX_I4[6] = {0,0,0,1,1,2}, EX_J4[6] = {1,2,3,2,3,3};

static Biv4 biv4_0(void){ Biv4 b; for(int i = 0; i < 6; i++) b.c[i] = qz(0,1); return b; }
static int biv4_zero(Biv4 a){
    for(int i = 0; i < 6; i++) if(a.c[i].p) return 0;
    return 1;
}
static int biv4_igual(Biv4 a, Biv4 b){
    for(int i = 0; i < 6; i++) if(!qz_igual(a.c[i], b.c[i])) return 0;
    return 1;
}
static Biv4 biv4_soma(Biv4 a, Biv4 b){
    Biv4 r; for(int i = 0; i < 6; i++) r.c[i] = qz_soma(a.c[i], b.c[i]);
    return r;
}
static Biv4 ex_wedge4(Vec u, Vec v){
    Biv4 r;
    for(int k = 0; k < 6; k++)
        r.c[k] = qz_soma(qz_mult(u.c[EX_I4[k]], v.c[EX_J4[k]]),
                         qz_oposto(qz_mult(u.c[EX_J4[k]], v.c[EX_I4[k]])));
    return r;
}
/* Λ² × Λ² → Λ⁴, o escalar. ω∧ω = 2·Pf(ω) — e é ele que decide a simplicidade. */
static Qz ex_wedge22(Biv4 a, Biv4 b){
    /* (12)(34) − (13)(24) + (14)(23), simetrizado nos dois argumentos */
    Qz t1 = qz_soma(qz_mult(a.c[0],b.c[5]), qz_mult(a.c[5],b.c[0]));
    Qz t2 = qz_soma(qz_mult(a.c[1],b.c[4]), qz_mult(a.c[4],b.c[1]));
    Qz t3 = qz_soma(qz_mult(a.c[2],b.c[3]), qz_mult(a.c[3],b.c[2]));
    return qz_soma(qz_soma(t1, qz_oposto(t2)), t3);
}
/* ── O PFAFFIANO ──────────────────────────────────────────────────────────────
 * Pf(A) = a₁₂a₃₄ − a₁₃a₂₄ + a₁₄a₂₃, e o teorema é Pf² = det. É a RAIZ QUADRADA do
 * determinante que EXISTE em inteiros — e é por isso que interessa a esta casa: onde
 * a raiz costuma trazer o irracional, aqui ela é POLINOMIAL nas entradas, e a régua
 * da casa manda-a normalizar só no fim.
 *
 * E ele é a mesma conta de ω∧ω: Pf(ω) = 0 ⟺ ω é SIMPLES. Um bivetor de dimensão 4
 * não é geralmente u∧v, e o Pfaffiano é exatamente a obstrução. */
static Qz ex_pf_biv(Biv4 w){
    Qz t1 = qz_mult(w.c[0], w.c[5]);
    Qz t2 = qz_oposto(qz_mult(w.c[1], w.c[4]));
    Qz t3 = qz_mult(w.c[2], w.c[3]);
    return qz_soma(qz_soma(t1,t2), t3);
}
static Biv4 ex_biv_de_matriz(Mat A){
    Biv4 w = biv4_0();
    for(int k = 0; k < 6; k++) w.c[k] = A.a[EX_I4[k]][EX_J4[k]];
    return w;
}
static Qz ex_pfaffiano(Mat A){ return ex_pf_biv(ex_biv_de_matriz(A)); }

static int ex_antissimetrica(Mat A){
    if(A.m != A.n) return 0;
    for(int i = 0; i < A.n; i++){
        if(A.a[i][i].p) return 0;
        for(int j = 0; j < A.n; j++)
            if(!qz_igual(A.a[i][j], qz_oposto(A.a[j][i]))) return 0;
    }
    return 1;
}
/* ── A DECOMPOSIÇÃO DA CASA: directo ⊕ cruzado ────────────────────────────────
 * Toda bilinear parte-se em simétrica (o DIRECTO, a activa) e antissimétrica (o
 * CRUZADO, a reactiva). Sobre ℚ a partição é única e exata — e por isso aqui não há
 * ramo de falha nenhum: dividir por 2 em ℚ sempre pode. Onde ela MORRE é noutro
 * corpo, e isso mede-se noutro sítio (ex_gume_car2), não com um `return 0` que nunca
 * corre. */
static void ex_parte(Mat B, Mat *sim, Mat *ant){
    Mat S = mat0(B.n, B.n), A = mat0(B.n, B.n);
    Qz dois = qz_de_inteiro(2);
    for(int i = 0; i < B.n; i++) for(int j = 0; j < B.n; j++){
        qz_divide(qz_soma(B.a[i][j], B.a[j][i]), dois, &S.a[i][j]);
        qz_divide(qz_soma(B.a[i][j], qz_oposto(B.a[j][i])), dois, &A.a[i][j]);
    }
    *sim = S; *ant = A;
}
/* ── O GUME DA CARACTERÍSTICA 2, PROCURADO ────────────────────────────────────
 * A conta dele: para B simétrica, B(v,w) − B(w,v) = 2B(v,w). Se 2 ≠ 0, ser também
 * antissimétrica força B = 0. Em 𝔽₂, 2 = 0 e a implicação DESAPARECE.
 *
 * Isto não se afirma: varre-se 𝔽ₚ à procura de uma B que seja simétrica E
 * antissimétrica E não nula. Para p ímpar a busca volta VAZIA (é o controlo); para
 * p = 2 volta com a testemunha na mão. O gume é o mesmo programa nos dois corpos. */
/* ═══ O FECHO DO DUAL: LAGRANGE ════════════════════════════════════════════════
 *
 * Ele mandou fechar o dual, e tinha razão: eu tinha construído o CRUZADO (Λ²) e
 * deixado o DIRECTO sozinho do outro lado. O par não estava fechado — estava PARTIDO
 * ao meio, que é o defeito que esta casa já catalogou.
 *
 * A equação que os fecha é uma só, e é exata em inteiros:
 *
 *        ⟨u,v⟩²  +  ‖u∧v‖²  =  N(u)·N(v)
 *        directo² + cruzado² = a norma conservada
 *
 * E é isto que a casa tinha em DUAS metades sem a frase que as junta:
 *
 *  · `corpo_analitico.tex` §640 tem fp = cos θ e tan φ = cruzado/directo — o split.
 *  · `tests/hurwitz.c` §H2 tem N(xy) = N(x)N(y) — a conservação.
 *  · e ninguém escreveu que a SEGUNDA É A PRIMEIRA. Lagrange é cos²θ + sin²θ = 1
 *    com a norma por dentro, e sem raiz nenhuma: mede-se ao QUADRADO.
 *
 * Três consequências que saem de graça, e nenhuma é decorativa:
 *
 *  (a) CAUCHY–SCHWARZ deixa de ser desigualdade: ⟨u,v⟩² ≤ N(u)N(v) é Lagrange com
 *      ‖u∧v‖² ≥ 0, e a FOLGA é exatamente o cruzado. A desigualdade é uma igualdade
 *      a que se apagou um termo — e o termo apagado é o que este ficheiro constrói.
 *
 *  (b) A CONSERVAÇÃO DA NORMA OBRIGA A DIMENSÃO 4. Para u,v puros em dim 3 o produto
 *      é uv = −⟨u,v⟩ + u×v: um escalar e um vetor. O escalar NÃO CABE em dim 3, e é
 *      preciso um quarto lugar. Hurwitz não é um teto imposto de fora — é onde o
 *      directo arranja sítio para se sentar ao lado do cruzado.
 *
 *  (c) E o ⋆ de Hodge é a ESTRELA desta casa: ⋆⋆ = id com resíduo 0, que é o
 *      ν∘ν = id da Lei 1. O `thm:central` diz que a bijeção dual Gentil↔Hurwitz É a
 *      estrela; aqui ela aparece outra vez, entre Λᵏ e Λⁿ⁻ᵏ. */

/* ‖u∧v‖² em qualquer dimensão: a soma dos QUADRADOS dos menores 2×2 (Cauchy–Binet).
 * Não é a norma: é o quadrado dela, e é de propósito — a raiz nunca se tira. */
static Qz ex_cruzado2(Vec u, Vec v){
    Qz s = qz(0,1);
    for(int i = 0; i < u.n; i++) for(int j = i+1; j < u.n; j++){
        Qz m = qz_soma(qz_mult(u.c[i],v.c[j]), qz_oposto(qz_mult(u.c[j],v.c[i])));
        s = qz_soma(s, qz_mult(m,m));
    }
    return s;
}
static Qz ex_directo(Vec u, Vec v){
    Qz s = qz(0,1);
    for(int i = 0; i < u.n; i++) s = qz_soma(s, qz_mult(u.c[i], v.c[i]));
    return s;
}
/* os DOIS lados de Lagrange, devolvidos à parte para se COMPARAREM — e não um só com
 * um «igual» já lá dentro, que é a asserção que não pode falhar */
static void ex_lagrange(Vec u, Vec v, Qz *conservada, Qz *partida){
    *conservada = qz_mult(ex_directo(u,u), ex_directo(v,v));
    Qz d = ex_directo(u,v);
    *partida = qz_soma(qz_mult(d,d), ex_cruzado2(u,v));
}
/* a FOLGA de Cauchy–Schwarz, que É o cruzado: N(u)N(v) − ⟨u,v⟩² = ‖u∧v‖² */
static Qz ex_folga_cs(Vec u, Vec v){
    Qz d = ex_directo(u,v);
    return qz_soma(qz_mult(ex_directo(u,u), ex_directo(v,v)), qz_oposto(qz_mult(d,d)));
}
/* ── O QUARTO LUGAR ────────────────────────────────────────────────────────────
 * O produto de dois vetores PUROS de dim 3 é (escalar, vetor) = (−⟨u,v⟩, u×v), que
 * mora em dim 4. Devolve a norma do produto, e ela tem de ser N(u)N(v) — que é
 * Lagrange outra vez, agora a dizer POR QUE É QUE a torre de Hurwitz tem o degrau 4. */
static Qz ex_quaterniao_norma(Vec u, Vec v){
    Qz d = ex_directo(u,v);
    return qz_soma(qz_mult(d,d), ex_cruzado2(u,v));
}
/* ── O GUME DE HURWITZ, PROCURADO EM INTEIROS ──────────────────────────────────
 * «É a soma de k quadrados fechada para o produto?» Para k = 1, 2 e 4 é (quadrados,
 * Brahmagupta, Euler). Para k = 3 NÃO É, e a testemunha acha-se.
 *
 * O LIMITE DESTE GUME, dito à frente e não escondido: para k ≥ 4 todo natural já é
 * soma de k quadrados (Lagrange, o dos quatro quadrados), portanto a tese é SEMPRE
 * VERDADEIRA e a busca não pode achar nada. Correr isto em k = 5, 6, 7 não mede a
 * ausência das álgebras nessas dimensões — mede o vazio. O que exclui 5, 6 e 7 é a
 * BILINEARIDADE, e essa é o teorema de Hurwitz, não um número. */
static int ex_soma_quadrados(long n, int k){
    if(k == 0) return n == 0;
    for(long a = 0; a*a <= n; a++) if(ex_soma_quadrados(n - a*a, k-1)) return 1;
    return 0;
}
static int ex_gume_hurwitz(int k, long lim, long *tx, long *ty){
    for(long x = 1; x <= lim; x++) for(long y = x; y <= lim; y++)
        if(ex_soma_quadrados(x,k) && ex_soma_quadrados(y,k)
           && !ex_soma_quadrados(x*y,k)){ *tx = x; *ty = y; return 1; }
    return 0;
}
static int ex_gume_car2(long p, long b[4]){
    long lim = p*p*p*p;
    for(long k = 1; k < lim; k++){          /* k = 0 é a nula, que não testemunha nada */
        long t = k, m[4];
        for(int i = 0; i < 4; i++){ m[i] = t % p; t /= p; }
        int nula = 1;
        for(int i = 0; i < 4; i++) if(m[i]) nula = 0;
        if(nula) continue;
        /* simétrica: m01 = m10 ; antissimétrica: m01 = −m10 e diagonal nula */
        if(m[1] != m[2]) continue;
        if((m[1] + m[2]) % p) continue;
        if(m[0] % p || m[3] % p) continue;
        for(int i = 0; i < 4; i++) b[i] = m[i];
        return 1;
    }
    return 0;
}
#endif
