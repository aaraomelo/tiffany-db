/* linear.h — ÁLGEBRA LINEAR EXATA, E O GUME QUE PROCURA A HIPÓTESE QUE CARREGA.
 *
 *  ordem do coordenador acrescenta uma exigência que NÃO é conteúdo, é MECANISMO:
 *
 *     «um gume obrigatório em cada teorema: SE A HIPÓTESE FOR RETIRADA, PROCURAR
 *      AUTOMATICAMENTE UM CONTRA-EXEMPLO. Aí não é só resolver álgebra linear: é fazer o
 *      motor descobrir QUAL HIPÓTESE ESTÁ CARREGANDO CADA TEOREMA.»
 *
 * É o que eu vinha a fazer à mão a sessão toda — ℤ₁₂ é abeliano e não exercitava a
 * normalidade; ℤₘ finito faz corpo e domínio coincidirem. Aqui vira função: `gume_*`
 * varre um espaço de objetos onde a hipótese é FALSA e devolve o primeiro onde a tese
 * TAMBÉM falha. Se não achar, diz-se — e diz-se o espaço varrido, porque «não achei» não
 * é «não existe».
 *
 * ── TUDO SOBRE ℚ, EXATO ────────────────────────────────────────────────────────
 * As entradas são `Qz`. A eliminação é Gauss-Jordan em frações — sem pivô parcial, sem
 * tolerância, sem decimal: o pivô é zero ou não é, e a pergunta tem resposta. É por isso
 * que posto, núcleo e inversa aqui são EXATOS e não «numericamente estáveis».
 *
 * E o dual entra como ele o põe: «o vetor fornece o objeto; o FUNCIONAL fornece a
 * coordenada que o mede» — um funcional é uma matriz 1×n, e a base dual sai da inversa.
 *
 * Precisa de `racionais.h`. */
#ifndef LINEAR_H
#define LINEAR_H

#define LN_MAX 6

typedef struct { int n; Qz c[LN_MAX]; } Vec;
typedef struct { int m, n; Qz a[LN_MAX][LN_MAX]; } Mat;

static Vec vec0(int n){ Vec v; v.n = n; for(int i = 0; i < n; i++) v.c[i] = qz(0,1); return v; }
static Mat mat0(int m, int n){
    Mat A; A.m = m; A.n = n;
    for(int i = 0; i < m; i++) for(int j = 0; j < n; j++) A.a[i][j] = qz(0,1);
    return A;
}
static Mat mat_id(int n){
    Mat A = mat0(n,n);
    for(int i = 0; i < n; i++) A.a[i][i] = qz(1,1);
    return A;
}
static Mat mat_de_inteiros(int m, int n, const long *v){
    Mat A = mat0(m,n);
    for(int i = 0; i < m; i++) for(int j = 0; j < n; j++) A.a[i][j] = qz_de_inteiro(v[i*n+j]);
    return A;
}
static int vec_igual(Vec u, Vec v){
    if(u.n != v.n) return 0;
    for(int i = 0; i < u.n; i++) if(!qz_igual(u.c[i], v.c[i])) return 0;
    return 1;
}
static int vec_zero(Vec v){
    for(int i = 0; i < v.n; i++) if(v.c[i].p) return 0;
    return 1;
}
static Vec vec_soma(Vec u, Vec v){
    Vec r = vec0(u.n);
    for(int i = 0; i < u.n; i++) r.c[i] = qz_soma(u.c[i], v.c[i]);
    return r;
}
static Vec vec_esc(Qz l, Vec v){
    Vec r = vec0(v.n);
    for(int i = 0; i < v.n; i++) r.c[i] = qz_mult(l, v.c[i]);
    return r;
}
static Mat mat_mult(Mat A, Mat B){                /* «convolução com índice interno» */
    Mat C = mat0(A.m, B.n);
    for(int i = 0; i < A.m; i++) for(int j = 0; j < B.n; j++){
        Qz s = qz(0,1);
        for(int k = 0; k < A.n; k++) s = qz_soma(s, qz_mult(A.a[i][k], B.a[k][j]));
        C.a[i][j] = s;
    }
    return C;
}
static Mat mat_soma(Mat A, Mat B){
    Mat C = mat0(A.m, A.n);
    for(int i = 0; i < A.m; i++) for(int j = 0; j < A.n; j++)
        C.a[i][j] = qz_soma(A.a[i][j], B.a[i][j]);
    return C;
}
/* ── ESCALAR (multiplicação por Qz) ────────────────────────────────────── */
static Mat mat_esc(Qz l, Mat A){
    Mat R = mat0(A.m, A.n);
    for(int i = 0; i < A.m; i++) for(int j = 0; j < A.n; j++)
        R.a[i][j] = qz_mult(l, A.a[i][j]);
    return R;
}
static Mat mat_esc_neg(Mat A){                    /* −A, que a Cayley–Hamilton pede */
    Mat R = mat0(A.m, A.n);
    for(int i = 0; i < A.m; i++) for(int j = 0; j < A.n; j++) R.a[i][j] = qz_oposto(A.a[i][j]);
    return R;
}
static Mat mat_transposta(Mat A){
    Mat T = mat0(A.n, A.m);
    for(int i = 0; i < A.m; i++) for(int j = 0; j < A.n; j++) T.a[j][i] = A.a[i][j];
    return T;
}
static int mat_igual(Mat A, Mat B){
    if(A.m != B.m || A.n != B.n) return 0;
    for(int i = 0; i < A.m; i++) for(int j = 0; j < A.n; j++)
        if(!qz_igual(A.a[i][j], B.a[i][j])) return 0;
    return 1;
}
static Vec mat_aplica(Mat A, Vec v){
    Vec r = vec0(A.m);
    for(int i = 0; i < A.m; i++){
        Qz s = qz(0,1);
        for(int j = 0; j < A.n; j++) s = qz_soma(s, qz_mult(A.a[i][j], v.c[j]));
        r.c[i] = s;
    }
    return r;
}
/* o DETERMINANTE por expansão — exato, e para n ≤ 4 o custo não incomoda */
static Qz mat_det(Mat A){
    int n = A.n;
    if(n == 1) return A.a[0][0];
    if(n == 2) return qz_soma(qz_mult(A.a[0][0],A.a[1][1]),
                              qz_oposto(qz_mult(A.a[0][1],A.a[1][0])));
    Qz s = qz(0,1);
    for(int j = 0; j < n; j++){
        if(A.a[0][j].p == 0) continue;
        Mat M = mat0(n-1,n-1);
        for(int i = 1; i < n; i++){
            int cc = 0;
            for(int k = 0; k < n; k++){ if(k == j) continue; M.a[i-1][cc++] = A.a[i][k]; }
        }
        Qz t = qz_mult(A.a[0][j], mat_det(M));
        s = (j % 2) ? qz_soma(s, qz_oposto(t)) : qz_soma(s, t);
    }
    return s;
}
/* ── GAUSS-JORDAN EXATO ────────────────────────────────────────────────────────
 * Sem pivô parcial e sem tolerância: em ℚ o pivô é zero ou não é. Devolve o POSTO e
 * deixa a matriz na forma escalonada reduzida, com `piv[j]` a dizer em que linha está o
 * pivô da coluna j (−1 se ela é livre). É desta rotina que saem posto, núcleo, imagem,
 * inversa e a solução dos sistemas — uma só descida, várias leituras. */
static int mat_reduz(Mat *A, int *piv){
    int lin = 0;
    for(int j = 0; j < A->n; j++) piv[j] = -1;
    for(int j = 0; j < A->n && lin < A->m; j++){
        int p = -1;
        for(int i = lin; i < A->m; i++) if(A->a[i][j].p){ p = i; break; }
        if(p < 0) continue;                        /* coluna sem pivô: variável LIVRE */
        for(int k = 0; k < A->n; k++){             /* troca as linhas */
            Qz t = A->a[lin][k]; A->a[lin][k] = A->a[p][k]; A->a[p][k] = t;
        }
        Qz inv;
        if(!qz_inverso(A->a[lin][j], &inv)) continue;
        for(int k = 0; k < A->n; k++) A->a[lin][k] = qz_mult(inv, A->a[lin][k]);
        for(int i = 0; i < A->m; i++){
            if(i == lin || A->a[i][j].p == 0) continue;
            Qz f = A->a[i][j];
            for(int k = 0; k < A->n; k++)
                A->a[i][k] = qz_soma(A->a[i][k], qz_oposto(qz_mult(f, A->a[lin][k])));
        }
        piv[j] = lin;
        lin++;
    }
    return lin;                                    /* o POSTO */
}
static int mat_posto(Mat A){ int piv[LN_MAX]; return mat_reduz(&A, piv); }

/* o NÚCLEO: uma base, uma coluna por variável LIVRE. «o núcleo é a FIBRA do 0» */
static int mat_nucleo(Mat A, Vec *base){
    int piv[LN_MAX], n = A.n;
    int r = mat_reduz(&A, piv), k = 0;
    for(int j = 0; j < n; j++){
        if(piv[j] >= 0) continue;                  /* coluna com pivô: não é livre */
        Vec v = vec0(n);
        v.c[j] = qz(1,1);
        for(int c = 0; c < n; c++)
            if(piv[c] >= 0) v.c[c] = qz_oposto(A.a[piv[c]][j]);
        base[k++] = v;
    }
    (void)r;
    return k;                                      /* a NULIDADE */
}
/* a IMAGEM: as colunas com pivô da matriz ORIGINAL formam uma base */
static int mat_imagem(Mat A, Vec *base){
    Mat R = A;
    int piv[LN_MAX];
    mat_reduz(&R, piv);
    int k = 0;
    for(int j = 0; j < A.n; j++){
        if(piv[j] < 0) continue;
        Vec v = vec0(A.m);
        for(int i = 0; i < A.m; i++) v.c[i] = A.a[i][j];
        base[k++] = v;
    }
    return k;                                      /* o POSTO */
}
/* a INVERSA, por Gauss-Jordan sobre [A | I] — e devolve 0 quando não existe */
static int mat_inversa(Mat A, Mat *R){
    int n = A.n;
    if(A.m != n) return 0;
    Mat W = mat0(n, 2*n);
    W.n = 2*n;
    for(int i = 0; i < n; i++){
        for(int j = 0; j < n; j++) W.a[i][j] = A.a[i][j];
        W.a[i][n+i] = qz(1,1);
    }
    int piv[LN_MAX];
    (void)piv;
    /* a redução escreve-se aqui porque o LN_MAX limita as colunas a 6 */
    int lin = 0;
    for(int j = 0; j < n && lin < n; j++){
        int p = -1;
        for(int i = lin; i < n; i++) if(W.a[i][j].p){ p = i; break; }
        if(p < 0) return 0;                        /* coluna sem pivô: singular */
        for(int k = 0; k < 2*n; k++){ Qz t = W.a[lin][k]; W.a[lin][k] = W.a[p][k]; W.a[p][k] = t; }
        Qz inv;
        if(!qz_inverso(W.a[lin][j], &inv)) return 0;
        for(int k = 0; k < 2*n; k++) W.a[lin][k] = qz_mult(inv, W.a[lin][k]);
        for(int i = 0; i < n; i++){
            if(i == lin || W.a[i][j].p == 0) continue;
            Qz f = W.a[i][j];
            for(int k = 0; k < 2*n; k++)
                W.a[i][k] = qz_soma(W.a[i][k], qz_oposto(qz_mult(f, W.a[lin][k])));
        }
        lin++;
    }
    if(lin < n) return 0;
    *R = mat0(n,n);
    for(int i = 0; i < n; i++) for(int j = 0; j < n; j++) R->a[i][j] = W.a[i][n+j];
    return 1;
}
/* ── INDEPENDÊNCIA, SPAN E COORDENADAS ─────────────────────────────────────────
 * Um conjunto é LI ⟺ a matriz das colunas tem posto = número de vetores. É a mesma
 * descida a responder outra pergunta. */
static Mat mat_de_colunas(const Vec *v, int k){
    Mat A = mat0(v[0].n, k);
    for(int j = 0; j < k; j++) for(int i = 0; i < v[0].n; i++) A.a[i][j] = v[j].c[i];
    return A;
}
static int vec_li(const Vec *v, int k){
    if(k == 0) return 1;
    return mat_posto(mat_de_colunas(v,k)) == k;
}
static int vec_no_span(const Vec *v, int k, Vec alvo){   /* alvo ∈ span(v)? */
    Mat A = mat_de_colunas(v,k);
    Mat B = A;
    B.n = k + 1;
    for(int i = 0; i < alvo.n; i++) B.a[i][k] = alvo.c[i];
    return mat_posto(A) == mat_posto(B);           /* Rouché–Capelli, exato */
}
/* as COORDENADAS numa base: resolve-se B·x = v, e são únicas por B ser LI */
static int vec_coord(const Vec *base, int k, Vec v, Qz *x){
    Mat B = mat_de_colunas(base,k);
    Mat W = B;
    W.n = k + 1;
    for(int i = 0; i < v.n; i++) W.a[i][k] = v.c[i];
    int piv[LN_MAX];
    int r = mat_reduz(&W, piv);
    if(r != k) return 0;
    for(int j = 0; j < k; j++){
        if(piv[j] < 0) return 0;
        x[j] = W.a[piv[j]][k];
    }
    return 1;
}
/* ── O GUME AUTOMÁTICO: retirar a hipótese e PROCURAR o contra-exemplo ─────────
 * Varre matrizes n×n com entradas inteiras num intervalo, e devolve a primeira onde a
 * HIPÓTESE falha E a TESE também falha — é ela que prova que a hipótese carrega o
 * teorema. Devolve 0 quando não acha, e aí o espaço varrido tem de ser dito. */
static long gume_matriz(int n, long lim,
                        int (*hip)(const Mat*), int (*tese)(const Mat*), Mat *contra){
    long lado = 2*lim + 1, total = 1, vistos = 0;
    for(int i = 0; i < n*n; i++) total *= lado;
    for(long k = 0; k < total; k++){
        Mat A = mat0(n,n);
        long t = k;
        for(int i = 0; i < n; i++) for(int j = 0; j < n; j++){
            A.a[i][j] = qz_de_inteiro(t % lado - lim);
            t /= lado;
        }
        vistos++;
        /* O FILTRO DA HIPÓTESE É SEMÂNTICO, NÃO OPERACIONAL — e isso mediu-se: apagar
         * esta linha não muda um único resultado. A razão é que, se o teorema é
         * VERDADEIRO, hip ⟹ tese, logo NENHUM objeto com a hipótese verdadeira falha a
         * tese: a linha seguinte já os exclui a todos. O filtro está cá para DECLARAR
         * que se está a procurar FORA da hipótese — e passaria a ser operacional no dia
         * em que o «teorema» fosse falso, que é justamente quando queremos saber. */
        if(hip(&A)) continue;                      /* a hipótese VALE: não serve de gume */
        if(tese(&A)) continue;                     /* a tese vale à mesma: não decide */
        if(contra) *contra = A;
        return vistos;                             /* achou, e diz em que passo */
    }
    return 0;                                      /* não achou — e o espaço diz-se */
}
/* ── O DUAL: um funcional é uma linha, e medir é multiplicar ───────────────────
 * «o vetor fornece o objeto; o funcional fornece a coordenada que o mede». */
typedef struct { int n; Qz c[LN_MAX]; } Fun;       /* f(v) = Σ cᵢ vᵢ */

static Qz fun_av(Fun f, Vec v){                    /* a AVALIAÇÃO */
    Qz s = qz(0,1);
    for(int i = 0; i < f.n; i++) s = qz_soma(s, qz_mult(f.c[i], v.c[i]));
    return s;
}
static Fun fun_soma(Fun f, Fun g){
    Fun r; r.n = f.n;
    for(int i = 0; i < f.n; i++) r.c[i] = qz_soma(f.c[i], g.c[i]);
    return r;
}
static Fun fun_esc(Qz l, Fun f){
    Fun r; r.n = f.n;
    for(int i = 0; i < f.n; i++) r.c[i] = qz_mult(l, f.c[i]);
    return r;
}
/* A BASE DUAL: e^i(e_j) = δ^i_j. Com B a matriz das colunas da base, os e^i são as
 * LINHAS de B⁻¹ — e é essa a construção, não uma procura. */
static int fun_base_dual(const Vec *base, int n, Fun *dual){
    Mat B = mat_de_colunas(base, n), Bi;
    if(!mat_inversa(B, &Bi)) return 0;
    for(int i = 0; i < n; i++){
        dual[i].n = n;
        for(int j = 0; j < n; j++) dual[i].c[j] = Bi.a[i][j];
    }
    return 1;
}
/* o ANIQUILADOR de W = span(w₁..w_k): os f com f(w) = 0 para todo w — e é o NÚCLEO da
 * matriz cujas LINHAS são os w. Outra vez a mesma descida. */
static int fun_aniquilador(const Vec *w, int k, int n, Fun *saida){
    Mat A = mat0(k, n);
    for(int i = 0; i < k; i++) for(int j = 0; j < n; j++) A.a[i][j] = w[i].c[j];
    Vec nb[LN_MAX];
    int d = mat_nucleo(A, nb);
    for(int i = 0; i < d; i++){
        saida[i].n = n;
        for(int j = 0; j < n; j++) saida[i].c[j] = nb[i].c[j];
    }
    return d;
}
#endif
