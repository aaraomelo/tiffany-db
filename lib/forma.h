/* forma.h — FORMAS, ADJUNTOS E ESPECTRO. E A RAIZ NUNCA SE TIRA.
 *
 *  ordem do coordenador sobe para a parte estrutural: forma bilinear → simétrica → quadrática →
 * produto interno → Cauchy–Schwarz → ortogonalidade → Gram–Schmidt → adjunto →
 * autoadjunto → espectral → valores singulares → SVD → Cayley–Hamilton → Jordan.
 *
 * ── O QUE JÁ CORRIA NA CASA, COM OUTRO NOME ────────────────────────────────────
 * Procurei antes de escrever, e o andar já cá estava em três sítios:
 *
 *   `mat2.estaca(m)` = mI − A_m, com A·(mI−A) = −I.  Isto É CAYLEY–HAMILTON:
 *      A² − mA − I = 0 é p_A(A) = 0 para a companheira A_m = [[m,1],[1,0]].
 *   `mat2.W(m)` = 2A − mI, com W² = (m²+4)I — o discriminante REALIZADO em matriz,
 *      e é outra vez a mesma relação.
 *   `T = C + C⁻¹` com T² = T + I (o traço-dobra do pentágono, `teoria.tex`)
 *      — a terceira aparição.
 *
 * E «a quadrática do ponto fixo DERIVA-SE da matriz (cx² + (d−a)x − b)» é o polinómio
 * do ponto fixo de Möbius. Para a COMPANHEIRA ele coincide com o característico
 * (λ² − tr·λ + det); em geral NÃO — e é essa coincidência que faz o inversor e o
 * espectro morarem no mesmo sítio nesta casa. Mede-se, não se afirma.
 *
 * E as duas FORMAS QUADRÁTICAS do §3 dele já existiam como «as duas cartas»:
 *   a² + b²        (definida — o círculo, a torção)
 *   a² + mab − b²  (indefinida — a hipérbole, o corpo)
 * O gume «Q(v) = 0 implica v = 0?» é exatamente a diferença entre elas.
 *
 * ── E A RAIZ NUNCA SE TIRA ─────────────────────────────────────────────────────
 * |v| = √⟨v,v⟩ é irracional quase sempre. Mas Cauchy–Schwarz ELEVA-SE ao quadrado:
 *
 *     ⟨u,v⟩² ≤ ⟨u,u⟩·⟨v,v⟩
 *
 * e aí é uma desigualdade entre RACIONAIS, exata. O mesmo com os valores singulares:
 * σᵢ = √λᵢ é irracional, σᵢ² = λᵢ não é. Trabalha-se com os quadrados e a raiz só se
 * escreve no fim, em caracteres — a régua de sempre.
 *
 * Precisa de `racionais.h`, `linear.h` e `cifra.h` (o `raizi`). */
#ifndef FORMA_H
#define FORMA_H

/* ── A FORMA BILINEAR B(u,v) = uᵀAv ────────────────────────────────────────────── */
static Qz fb_av(Mat A, Vec u, Vec v){
    Qz s = qz(0,1);
    for(int i = 0; i < A.m; i++) for(int j = 0; j < A.n; j++)
        s = qz_soma(s, qz_mult(u.c[i], qz_mult(A.a[i][j], v.c[j])));
    return s;
}
static int fb_simetrica(Mat A){ return mat_igual(A, mat_transposta(A)); }
static Qz fb_quadratica(Mat A, Vec v){ return fb_av(A, v, v); }

/* DEFINIDA POSITIVA: Q(v) > 0 para todo v ≠ 0. Varre-se um cubo e DIZ-SE o cubo —
 * em ℚ não há como varrer tudo, e «não achei» não é «não existe». O contra-exemplo,
 * quando aparece, é a testemunha e vale por si. */
static int fb_definida(Mat A, long lim, Vec *contra){
    int n = A.n;
    long lado = 2*lim + 1, total = 1;
    for(int i = 0; i < n; i++) total *= lado;
    for(long k = 0; k < total; k++){
        Vec v = vec0(n);
        long t = k;
        for(int i = 0; i < n; i++){ v.c[i] = qz_de_inteiro(t % lado - lim); t /= lado; }
        if(vec_zero(v)) continue;
        Qz q = fb_quadratica(A, v);
        if(q.p <= 0){ if(contra) *contra = v; return 0; }
    }
    return 1;
}
/* ── O PRODUTO INTERNO usual, e a NORMA AO QUADRADO ────────────────────────────
 * `pi_norma2` devolve ⟨v,v⟩ — o quadrado. A raiz não se tira: é ela que traria o
 * irracional para dentro da conta, e o andar de ℝ já disse porquê. */
static Qz pi(Vec u, Vec v){
    Qz s = qz(0,1);
    for(int i = 0; i < u.n; i++) s = qz_soma(s, qz_mult(u.c[i], v.c[i]));
    return s;
}
static Qz pi_norma2(Vec v){ return pi(v,v); }
static int pi_ortogonais(Vec u, Vec v){ return pi(u,v).p == 0; }

/* CAUCHY–SCHWARZ AO QUADRADO: ⟨u,v⟩² ≤ ⟨u,u⟩⟨v,v⟩ — exato, sem raiz.
 * E o caso de IGUALDADE é o gume: dá-se exatamente quando u e v são colineares. */
/* A DESIGUALDADE É SOBRE A FORMA, não sobre um produto fixo — e escrevê-la assim é o
 * que a torna TESTÁVEL: com o produto euclidiano ela é sempre verdadeira, logo uma
 * implementação que devolvesse sempre «sim» era indistinguível da correta. Dando-lhe a
 * forma INDEFINIDA da casa, ela tem de devolver «não», e aí a asserção morde. */
static int fb_cauchy(Mat A, Vec u, Vec v, int *igualdade){
    Qz e = qz_mult(fb_av(A,u,v), fb_av(A,u,v));
    Qz d = qz_mult(fb_quadratica(A,u), fb_quadratica(A,v));
    if(igualdade) *igualdade = qz_igual(e,d);
    return qz_menor(e,d) || qz_igual(e,d);
}
static Mat pi_gram_matriz(int n){                          /* a forma do produto usual */
    Mat I = mat0(n,n);
    for(int i = 0; i < n; i++) I.a[i][i] = qz(1,1);
    return I;
}
static int pi_cauchy(Vec u, Vec v, int *igualdade){
    return fb_cauchy(pi_gram_matriz(u.n), u, v, igualdade);
}
/* a TRIANGULAR, também ao quadrado: |u+v|² ≤ (|u|+|v|)² pede a raiz do produto…
 * então mede-se na forma que a evita:  (|u+v|² − |u|² − |v|²)² ≤ 4|u|²|v|²  quando o
 * lado esquerdo é positivo — que é Cauchy–Schwarz outra vez, e é exata. */
static int pi_triangular(Vec u, Vec v){
    Qz nu = pi_norma2(u), nv = pi_norma2(v), ns = pi_norma2(vec_soma(u,v));
    Qz meio = qz_soma(ns, qz_oposto(qz_soma(nu,nv)));      /* = 2⟨u,v⟩ */
    if(meio.p <= 0) return 1;                              /* já é ≤ sem esforço */
    Qz esq = qz_mult(meio,meio);
    Qz dir = qz_mult(qz_de_inteiro(4), qz_mult(nu,nv));
    return qz_menor(esq,dir) || qz_igual(esq,dir);
}
/* a PROJEÇÃO ortogonal: proj_u(v) = (⟨v,u⟩/⟨u,u⟩)·u — exata em ℚ */
static int pi_proj(Vec v, Vec u, Vec *par, Vec *perp){
    Qz nu = pi_norma2(u);
    if(nu.p == 0) return 0;                                /* u = 0: sem fibra */
    Qz l;
    if(!qz_divide(pi(v,u), nu, &l)) return 0;
    *par = vec_esc(l, u);
    *perp = vec_soma(v, vec_esc(qz_de_inteiro(-1), *par));
    return 1;
}
/* GRAM–SCHMIDT, exato e SEM NORMALIZAR: normalizar dividiria por √⟨u,u⟩, que é
 * irracional. A base sai ORTOGONAL (não ortonormal), e diz-se — a ortonormal existe,
 * mas vive um andar acima, no corpo das raízes. */
static int pi_gram(const Vec *v, int k, Vec *u){
    int m = 0;
    for(int i = 0; i < k; i++){
        Vec w = v[i];
        for(int j = 0; j < m; j++){
            Qz nu = pi_norma2(u[j]);
            if(nu.p == 0) continue;
            Qz l;
            if(!qz_divide(pi(v[i], u[j]), nu, &l)) return -1;
            w = vec_soma(w, vec_esc(qz_oposto(l), u[j]));
        }
        if(!vec_zero(w)) u[m++] = w;                       /* os dependentes caem */
    }
    return m;
}
/* ── O ESPECTRO 2×2, EXATO QUANDO DÁ ───────────────────────────────────────────
 * p_A(λ) = λ² − tr·λ + det. As raízes são racionais ⟺ o discriminante é quadrado
 * perfeito — e quando não é, escreve-se pela FC, não por decimal. */
static Qz esp_tr(Mat A){ return qz_soma(A.a[0][0], A.a[1][1]); }
static long esp_disc(Mat A){                               /* tr² − 4det, em inteiros */
    Qz tr = esp_tr(A), dt = mat_det(A);
    if(tr.q != 1 || dt.q != 1) return -1;                  /* fora dos inteiros: recusa */
    return tr.p*tr.p - 4*dt.p;
}
static int esp_racional(Mat A, long *l1, long *l2){
    long D = esp_disc(A);
    if(D < 0) return 0;
    long r = raizi(D);                                     /* a raiz inteira da casa */
    if(r*r != D) return 0;                                 /* não é quadrado: as raízes
                                                            * são as folhas do corpo, e
                                                            * escrevem-se pela FC */
    Qz tr = esp_tr(A);
    if((tr.p + r) % 2 || (tr.p - r) % 2) return 0;
    *l1 = (tr.p + r)/2; *l2 = (tr.p - r)/2;
    return 1;
}
/* CAYLEY–HAMILTON: A² − tr·A + det·I = 0. Vale SEMPRE, e é exato. */
static Mat esp_cayley(Mat A){
    Mat A2 = mat_mult(A,A);
    Mat t = mat0(A.n, A.n), d = mat0(A.n, A.n);
    Qz tr = esp_tr(A), dt = mat_det(A);
    for(int i = 0; i < A.n; i++) for(int j = 0; j < A.n; j++)
        t.a[i][j] = qz_mult(tr, A.a[i][j]);
    for(int i = 0; i < A.n; i++) d.a[i][i] = dt;
    return mat_soma(mat_soma(A2, mat_esc_neg(t)), d);
}
/* quantos autovetores INDEPENDENTES — é isto que decide a diagonalizabilidade */
static int esp_autovetores(Mat A, Vec *saida){
    long l1, l2;
    if(!esp_racional(A, &l1, &l2)) return 0;
    int k = 0;
    long ls[2] = { l1, l2 };
    for(int i = 0; i < 2; i++){
        if(i == 1 && l1 == l2) break;
        Mat S = A;
        for(int j = 0; j < A.n; j++)
            S.a[j][j] = qz_soma(S.a[j][j], qz_de_inteiro(-ls[i]));
        Vec nb[LN_MAX];
        int d = mat_nucleo(S, nb);
        for(int t = 0; t < d && k < LN_MAX; t++) saida[k++] = nb[t];
    }
    return k;
}
static int esp_diagonalizavel(Mat A){
    Vec vs[LN_MAX];
    int k = esp_autovetores(A, vs);
    return k == A.n && vec_li(vs, k);
}
#endif
