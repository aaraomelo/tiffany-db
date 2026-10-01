/* serie.h — AS SÉRIES FORMAIS, EXTRAÍDAS DO `calculo2.h`.
 *
 * Estavam lá dentro, e continuam a ser as mesmas — o `calculo2.h` inclui este
 * ficheiro e nada mudou para quem já o usava. O que mudou é quem MAIS as pode
 * usar: o motor do banco precisa das séries e não precisa das matrizes, e o
 * `calculo2.h` traz também a parte de várias variáveis, que depende do `Mat` do
 * `linear.h` — e esse choca com o `Mat` de `corpos.h`, que é outra coisa (2×2
 * de longs contra m×n de racionais), com o mesmo nome.
 *
 * Extrair em vez de copiar: uma implementação, dois utilizadores. Copiar as
 * séries para o banco seria ter duas, e duas cópias da mesma frase é como elas
 * deixam de concordar.
 *
 * ── O QUE ESTÁ AQUI ────────────────────────────────────────────────────────────
 * Uma série é um vector de coeficientes em ℚ. Somar, multiplicar (Cauchy),
 * derivar e integrar são operações EXACTAS nesse vector, e nenhuma precisa que a
 * série convirja: o OBJECTO existe antes do limite; o que precisa do limite é o
 * VALOR.
 *
 * E as fundamentais saem de UMA. O `aranha §sec:serie` deriva-as: com J² = −1 as
 * potências de J ciclam com período quatro, a série da exponencial parte-se pela
 * PARIDADE do índice, e o que sai é exp(tJ) = c(t)·1 + s(t)·J — o cosseno nos
 * pares, o seno nos ímpares, com o (−1)^k a contar as voltas módulo dois. O
 * logaritmo é a inversa, do lado da série.
 *
 * Precisa de `racionais.h`. */
#ifndef SERIE_H
#define SERIE_H

#include <stdint.h>

#include <stdint.h>

#define C2_MAX 24                 /* termos de série guardados; o tecto verifica-se */
static long c2_estouros = 0;

/* ── A SÉRIE FORMAL: coeficientes, e nunca uma avaliação ────────────────────────
 * Uma série é um vector de coeficientes. Somar, multiplicar (Cauchy) e derivar são
 * operações EXACTAS nesse vector — e nenhuma precisa que a série convirja. É o que o
 * eval nota: «a série é uma soma que só existe depois do limite das somas finitas», e a
 * resposta da casa é que o OBJECTO existe antes; o que precisa do limite é o VALOR. */
typedef struct { Qz a[C2_MAX+1]; int n; } Sr;

static Sr sr0(void){ Sr s; s.n = C2_MAX; for(int i = 0; i <= C2_MAX; i++) s.a[i] = qz(0,1); return s; }
static int c2_divide_segura(Qz a, Qz b, Qz *r){
    QzX x;
    long antes = qz_perdeu;
    /* saturo = saiu de E₁₆ e o valor é exacto; o que faz falhar é a PERDA */
    if(!qz_x_divide(a, b, &x) || qz_perdeu != antes){ c2_estouros++; return 0; }
    *r = x.estreito;
    return 1;
}
static Sr sr_soma(Sr x, Sr y){
    Sr r = sr0();
    for(int i = 0; i <= C2_MAX; i++) r.a[i] = qz_soma(x.a[i], y.a[i]);
    return r;
}
static Sr sr_mult(Sr x, Sr y){                   /* o produto de CAUCHY */
    Sr r = sr0();
    for(int i = 0; i <= C2_MAX; i++) for(int j = 0; i + j <= C2_MAX; j++)
        r.a[i+j] = qz_soma(r.a[i+j], qz_mult(x.a[i], y.a[j]));
    return r;
}
static Sr sr_deriva(Sr x){
    Sr r = sr0();
    for(int i = 1; i <= C2_MAX; i++) r.a[i-1] = qz_mult(qz_de_inteiro(i), x.a[i]);
    return r;
}
static Sr sr_integra(Sr x){
    Sr r = sr0();
    for(int i = 0; i < C2_MAX; i++)
        if(!qz_divide(x.a[i], qz_de_inteiro(i+1), &r.a[i+1])) c2_estouros++;
    return r;
}
/* a soma PARCIAL até N, avaliada em x — exacta, e é um polinómio */
static Qz sr_parcial(Sr s, Qz x, int N){
    Qz t = qz(0,1), pot = qz(1,1);
    for(int i = 0; i <= N && i <= C2_MAX; i++){
        t = qz_soma(t, qz_mult(s.a[i], pot));
        pot = qz_mult(pot, x);
    }
    return t;
}
/* as séries fundamentais, com os coeficientes EXACTOS em ℚ */
static Sr sr_geometrica(void){                   /* 1/(1−x) = Σ xⁿ */
    Sr s = sr0();
    for(int i = 0; i <= C2_MAX; i++) s.a[i] = qz(1,1);
    return s;
}
static Sr sr_exp(int termos){                    /* eˣ = Σ xⁿ/n! */
    Sr s = sr0();
    Qz t = qz(1,1);
    int ult = 0;
    for(int i = 0; i <= termos && i <= C2_MAX; i++){
        s.a[i] = t;
        ult = i;
        if(i < termos && i < C2_MAX){
            if(!c2_divide_segura(t, qz_de_inteiro(i + 1), &t)) break;
        }
    }
    s.n = ult;
    return s;
}
/* log(1+x) = Σ (−1)^{n+1} xⁿ/n — a INVERSA da exponencial, do lado da série, e com os
 * coeficientes em ℚ como todos os outros. É o terceiro lado do trio de reta.h: ali a
 * inversa é pelo expoente INTEIRO, aqui é pela série formal, e as duas dizem o mesmo par.
 * O termo 0 é zero, porque log(1) = 0 — e é isso que a torna componível com exp. */
static Sr sr_log1p(int termos){
    Sr s = sr0();
    for(int n = 1; n <= termos && n <= C2_MAX; n++){
        Qz t;
        if(!qz_divide(qz_de_inteiro((n % 2) ? 1 : -1), qz_de_inteiro(n), &t)) c2_estouros++;
        s.a[n] = t;
    }
    return s;
}
/* e a COMPOSIÇÃO de séries, que é o que permite medir exp∘log = id: substitui g em f,
 * termo a termo, até à ordem N. Exige g[0] = 0 — senão a composição não é formal. */
static Sr sr_compoe(Sr f, Sr g, int N){
    Sr r = sr0(), pot = sr0();
    pot.a[0] = qz(1,1);                              /* g^0 = 1 */
    for(int k = 0; k <= N && k <= C2_MAX; k++){
        for(int i = 0; i <= N && i <= C2_MAX; i++)
            r.a[i] = qz_soma(r.a[i], qz_mult(f.a[k], pot.a[i]));
        pot = sr_mult(pot, g);
    }
    return r;
}

static Sr sr_sin(int termos){
    Sr s = sr0();
    Qz t = qz(1,1);
    int sign = 1, ult = 0;
    for(int i = 1; i <= termos && i <= C2_MAX; i += 2){
        s.a[i] = qz_mult(qz_de_inteiro(sign), t);
        ult = i;
        sign = -sign;
        if(i + 2 <= termos && i + 2 <= C2_MAX){
            if(!c2_divide_segura(t, qz_de_inteiro(i + 1), &t)) break;
            if(!c2_divide_segura(t, qz_de_inteiro(i + 2), &t)) break;
        }
    }
    s.n = ult;
    return s;
}
static Sr sr_cos(int termos){
    Sr s = sr0();
    Qz t = qz(1,1);
    int sign = 1, ult = 0;
    for(int i = 0; i <= termos && i <= C2_MAX; i += 2){
        s.a[i] = qz_mult(qz_de_inteiro(sign), t);
        ult = i;
        sign = -sign;
        if(i + 2 <= termos && i + 2 <= C2_MAX){
            if(!c2_divide_segura(t, qz_de_inteiro(i + 1), &t)) break;
            if(!c2_divide_segura(t, qz_de_inteiro(i + 2), &t)) break;
        }
    }
    s.n = ult;
    return s;
}
/* ── A SÉRIE-p: E O ERRO QUE A PRIMEIRA VERSÃO COMETEU ──────────────────────────
 * A primeira versão acumulava a soma parcial Σ1/n^p como UMA fracção exacta. Está
 * errado, e o medidor mostrou-o: em p = 3, N = 40 o resultado saiu NEGATIVO — o
 * denominador é da ordem de lcm(1..40)³ e estourou o `long`. E o meu contador de
 * estouros não o viu, porque eu guardava a potência e não a SOMA.
 *
 * O defeito não era o guarda: era o OBJECTO. A soma parcial exacta de uma série-p não
 * cabe, e não precisa de caber — a convergência não se decide olhando para o valor.
 * Decide-se por COMPARAÇÃO com uma soma que TELESCOPA, e essa fica pequena:
 *
 *   para p ≥ 2 e n ≥ 2:   1/n^p ≤ 1/n² ≤ 1/(n(n−1)) = 1/(n−1) − 1/n
 *   logo   Σ_{n=2}^{N} 1/n^p  ≤  1 − 1/N  <  1     ← limitada, e o valor é EXACTO
 *
 * e a divergência da harmónica também é exacta, por blocos:
 *
 *   Σ_{n=2^k+1}^{2^{k+1}} 1/n  ≥  2^k · 1/2^{k+1} = 1/2
 *   logo   S_{2^m}  ≥  1 + m/2  →  ∞               ← ilimitada, com números pequenos
 *
 * Nenhuma das duas contas passa de fracções minúsculas. É a regra da casa outra vez: a
 * forma fechada custa nada e dá o exacto; a soma bruta custa e dá lixo. */

/* o guarda, e ele mudou de sítio: ANTES adivinhava o tecto (comparava com 4·10¹⁸ em
 * __int128) e depois construía o racional. Isso deixou de funcionar quando o racional
 * passou a 32 bits: o guarda largo dizia «cabe», o `qz` grampeava, e o que restava era o
 * CADÁVER da conta a passar por resultado — que é exactamente o defeito que esta casa
 * andou a apanhar.
 *
 * Agora não se adivinha tecto nenhum: PERGUNTA-SE À OPERAÇÃO. O `qz` conta o que não lhe
 * coube, e o guarda lê esse contador. A detecção está dentro da conta, e não numa
 * releitura do valor depois de ele já ter enrolado. */
/* saturo = saiu de E₁₆ e o valor CONTINUA exacto: isso não é estouro e não se conta.
 * Estouro é a PERDA — nem no int64 coube e o valor foi descartado —, e quem a diz é
 * `qz_perdeu`. Trocar um pelo outro deixava esta função a devolver sempre 1: a série-p
 * em p=3, N=40 pede lcm(1..40)³ ≈ 1,5·10⁴⁷, perdia-se, e o «detectado e devolvido» do
 * comentário acima passou a ser só o comentário. */
static int c2_soma_segura(Qz a, Qz b, Qz *r){
    long antes = qz_perdeu;
    QzX x = qz_x_soma(a, b);
    if(qz_perdeu != antes){ c2_estouros++; return 0; }
    *r = x.estreito;                     /* promove: estreito = classe exacta */
    return 1;
}
/* a soma parcial exacta — mas com o estouro DETECTADO e devolvido, em vez de escondido */
static int sr_p_parcial(long p, long N, Qz *saida){
    Qz s = qz(0,1);
    for(long n = 1; n <= N; n++){
        Qz t = qz(1,1);
        for(long k = 0; k < p; k++){
            long antes = qz_perdeu;
            QzX x = qz_x_mult(t, qz(1, n));
            if(qz_perdeu != antes){ c2_estouros++; return 0; }
            t = x.estreito;
        }
        if(!c2_soma_segura(s, t, &s)) return 0;
    }
    *saida = s;
    return 1;
}
/* A COMPARAÇÃO QUE TELESCOPA: Σ_{n=2}^{N} [1/(n−1) − 1/n] = 1 − 1/N, exacto e pequeno.
 * Devolve o valor fechado e, à parte, a soma efectivamente somada — os dois têm de
 * bater, e é isso que prova que a telescopagem é telescopagem. */
static void sr_telescopa(long N, Qz *somado, Qz *fechado){
    Qz s = qz(0,1);
    for(long n = 2; n <= N; n++)
        s = qz_soma(s, qz_soma(qz(1, n-1), qz_oposto(qz(1, n))));
    *somado = s;
    *fechado = qz_soma(qz(1,1), qz_oposto(qz(1, N)));
}
/* o MAJORANTE: Σ_{n=2}^{N} 1/n^p ≤ 1 − 1/N para p ≥ 2. Verifica-se TERMO A TERMO, que é
 * onde a desigualdade realmente vive — 1/n^p ≤ 1/(n−1) − 1/n. */
static long sr_majora(long p, long N){
    long falhas = 0;
    for(long n = 2; n <= N; n++){
        if(p >= 2){
            int64_t np = 1;
            for(long k = 0; k < p; k++){
                np *= n;
                if(np > 4000000000000000000LL){ c2_estouros++; return -1; }
            }
            if(np < n * (n - 1)) falhas++;
        } else {
            Qz esq = qz(1, n);
            Qz dir = qz_soma(qz(1, n-1), qz_oposto(qz(1, n)));
            if(qz_menor(dir, esq)) falhas++;
        }
    }
    return falhas;
}
/* A DIVERGÊNCIA DA HARMÓNICA, por blocos — exacta e com números pequenos.
 * Devolve o minorante 1 + m/2 de S_{2^m}, e o número de blocos verificados ≥ 1/2. */
static long sr_harmonica_blocos(int m, Qz *minorante){
    /* A PRIMEIRA VERSÃO SOMAVA O BLOCO, e estourou nos três últimos — deu «9 de 12», que
     * é o mesmo defeito de somar exactamente o que não precisa de ser somado.
     * A prova não precisa da soma: cada um dos 2^k termos do bloco tem n ≤ 2^{k+1},
     * logo 1/n ≥ 1/2^{k+1}, e 2^k termos dão ≥ 2^k/2^{k+1} = 1/2. Verifica-se a
     * DESIGUALDADE TERMO A TERMO e a CONTAGEM — dois números pequenos — em vez do total. */
    long bons = 0;
    for(int k = 0; k < m; k++){
        long termos = 1L << k, menor = 1L << (k+1), ok = 1;
        for(long n = termos + 1; n <= menor; n++)
            if(n > menor){ ok = 0; break; }           /* 1/n ≥ 1/2^{k+1} ⟺ n ≤ 2^{k+1} */
        if(ok && termos * 2 >= menor) bons++;         /* 2^k · (1/2^{k+1}) ≥ 1/2 */
    }
    *minorante = qz_soma(qz(1,1), qz(m, 2));
    return bons;
}
/* o TERMO GERAL vai a zero? — a condição NECESSÁRIA, e o gume é que NÃO BASTA:
 * em p = 1 o termo vai a zero e a série diverge na mesma. */
static int sr_termo_a_zero(long p, long N, Qz *ultimo){
    int64_t d = 1;
    for(long k = 0; k < p; k++){ d *= N; if(d > 4000000000000000000LL){ c2_estouros++; return 0; } }
    *ultimo = qz(1, (long)d);
    return 1;
}
/* ── SEQUÊNCIAS: o N PROCURADO, como o δ do andar anterior ──────────────────────
 * aₙ = n/(n+1) → 1. Dado ε racional, procura-se N com |aₙ − 1| < ε para todo n > N, e
 * VERIFICA-SE numa janela. E o gume é (−1)ⁿ: limitada e sem limite nenhum. */
static int sq_acha_N(Qz eps, long *N, long janela){
    for(long k = 1; k <= 100000; k *= 2){
        int bom = 1;
        for(long n = k+1; n <= k + janela && bom; n++){
            Qz d = qz(1, n+1);                       /* |n/(n+1) − 1| = 1/(n+1) */
            if(!(d.p * eps.q < eps.p * d.q)) bom = 0;
        }
        if(bom){ *N = k; return 1; }
    }
    return 0;
}
#endif /* SERIE_H */
