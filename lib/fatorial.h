/* ═══════════════════════════════════════════════════════════════════════════
 * lib/fatorial.h — A BASE FATORIAL: a exponencial, o π e as raízes em dígitos.
 *
 * O paper (§sec:serie) não importa constante nenhuma. Da única hipótese J²=−1
 * as potências de J ciclam com período quatro, a série da exponencial PARTE-SE
 * pela paridade do índice, e recolhendo cada metade:
 *
 *     exp(tJ) = c(t)·1 + s(t)·J,
 *     c(t) = Σ_k (−1)^k t^{2k}/(2k)!,      s(t) = Σ_k (−1)^k t^{2k+1}/(2k+1)!
 *
 * e daí, sem uma palavra de geometria:
 *
 *     π = min{ t > 0 : exp(tJ)·1 = −1 }   --- o tempo que o fluxo leva a
 *                                             realizar a Lei 1.
 *
 * AQUI NÃO HÁ VÍRGULA. Trabalha-se em escala inteira: um número x é guardado
 * como o inteiro round(x·S), e o `thm:serie` diz porque isto TERMINA --- «os
 * termos anulam-se ao fim de um número finito de parcelas, que é o que a
 * divisão inteira faz». O fatorial cresce mais depressa que qualquer potência,
 * e a partir de certo k o termo é ZERO na escala. Não é truncar: é parar.
 *
 * E o termo calcula-se por RECORRÊNCIA, nunca formando t^n nem n! --- os dois
 * estouram e o quociente deles não:
 *
 *     T_0 = S,   T_{k+1} = −T_k · t² / ((2k+1)(2k+2))     (para c)
 *
 * com t já em escala, pelo que cada passo divide também por S. O sinal
 * alternado é o (−1)^k, que é «o número de voltas lido módulo dois».
 *
 * NOTA: este ficheiro é NOVO. As séries formais em Q --- `sr_exp`, `sr_sin`,
 * `sr_cos`, `sr_log1p` --- vivem em `lib/serie.h` e são outra coisa: lá os
 * coeficientes são exactos em Q, aqui avalia-se em escala inteira e conta-se
 * o custo. Escrevi por cima daquele ficheiro uma vez, e o compilador do banco
 * é que o disse --- por isso o nome mudou.
 *
 * Medido em `tests/fatorial.c`.
 * ═══════════════════════════════════════════════════════════════════════════ */
#ifndef FATORIAL_H
#define FATORIAL_H

#include <stdio.h>       /* snprintf, para a apresentação dos dígitos */

/* a profundidade da primeira divergência, do bit mais significativo --- é a
 * def:arvore da aranha, e escreve-se aqui para o header não depender de outro */
static int tv_prof_local(long x, long y, int bits){
    if(x == y) return bits;
    for(int i = 0; i < bits; i++)
        if(((x >> (bits-1-i)) & 1L) != ((y >> (bits-1-i)) & 1L)) return i;
    return bits;
}

/* o resultado de uma série: o valor em escala, e o CUSTO --- quantos termos
 * até o termo ir a zero, que é o ℓ do thm:serie lido no campo */
typedef struct { long valor; int termos; int parou; } SfSerie;

/* ── c(t) = Σ (−1)^k t^{2k}/(2k)! ──────────────────────────────────────────
 * `t` e o resultado em escala `S`. Devolve também em quantos termos parou. */
static SfSerie sf_c(long t, long S){
    SfSerie r; r.valor = S; r.termos = 1; r.parou = 0;
    long T = S;                                   /* T_0 = 1, em escala */
    for(int k = 0; k < 64; k++){
        /* T_{k+1} = −T_k · t · t / ((2k+1)(2k+2) · S · S) */
        long d1 = (long)(2*k+1), d2 = (long)(2*k+2);
        long num = T / d1;                        /* divide cedo: não estoura */
        num = num * t / S;
        num = num / d2;
        num = num * t / S;
        T = -num;
        if(T == 0){ r.parou = 1; break; }
        r.valor += T;
        r.termos++;
    }
    return r;
}

/* ── s(t) = Σ (−1)^k t^{2k+1}/(2k+1)! ─────────────────────────────────────── */
static SfSerie sf_s(long t, long S){
    SfSerie r; r.valor = t; r.termos = 1; r.parou = 0;
    long T = t;                                   /* T_0 = t */
    for(int k = 0; k < 64; k++){
        long d1 = (long)(2*k+2), d2 = (long)(2*k+3);
        long num = T / d1;
        num = num * t / S;
        num = num / d2;
        num = num * t / S;
        T = -num;
        if(T == 0){ r.parou = 1; break; }
        r.valor += T;
        r.termos++;
    }
    return r;
}

/* ── A CONSERVAÇÃO, que sai por derivação e não se impõe:
 *      (c²+s²)' = 2cc' + 2ss' = −2cs + 2sc = 0,  e vale 1 em t=0.
 * Aqui verifica-se em escala: c²+s² tem de dar S², a menos do grão. */
static long sf_nivel(long t, long S){
    SfSerie c = sf_c(t, S), s = sf_s(t, S);
    return (c.valor/1000)*(c.valor/1000) + (s.valor/1000)*(s.valor/1000);
}

/* ── π = min{t>0 : exp(tJ)·1 = −1}.
 *
 * A condição é sobre o PAR: exp(πJ) = −1 quer dizer c(π) = −1 E s(π) = 0. E as
 * duas metades não são igualmente úteis para PROCURAR --- é preciso dizê-lo,
 * porque a primeira escrita disto usou a errada e deu 3,097 em vez de 3,1415:
 *
 *   · procurar c(t) = −1 é procurar um MÍNIMO, e perto do mínimo a função é
 *     PLANA: c(t) ≈ −1 + (t−π)²/2, pelo que um desvio de 10^-3 no valor dá
 *     4·10^-2 no t. A condição é mal posta e o erro é da CONDIÇÃO, não da série.
 *
 *   · procurar s(t) = 0 é procurar um zero SIMPLES: s'(π) = c(π) = −1 ≠ 0, e o
 *     desvio no t é da ordem do desvio no valor. Bem posta.
 *
 * São a mesma equação lida nas duas faces, e só uma delas se procura. Usa-se a
 * segunda, e CONFERE-SE a primeira no ponto achado --- que é o par a fechar. */
typedef struct { long pi; long c_no_ponto; int termos; int achou; } SfPi;

static SfPi sf_pi(long S, long grao){
    SfPi r; r.pi = 0; r.c_no_ponto = 0; r.termos = 0; r.achou = 0;
    long ant = 0; int primeiro = 1;
    for(long t = grao; t < 8*S; t += grao){
        SfSerie sv = sf_s(t, S);
        r.termos += sv.termos;
        if(!primeiro && ant > 0 && sv.valor <= 0){
            /* o zero está entre t−grão e t: fica-se com o mais próximo */
            long tz = (ant > -sv.valor) ? t : t - grao;
            r.pi = tz;
            r.c_no_ponto = sf_c(tz, S).valor;      /* tem de dar −S: a outra face */
            r.achou = 1;
            break;
        }
        ant = sv.valor; primeiro = 0;
    }
    return r;
}

/* ── E A SOLUÇÃO DA EDO É A EXPONENCIAL, que é esta série.
 * Para y' = λy com λ real, y(t) = e^{λt} = Σ (λt)^k/k! --- a mesma recorrência
 * sem o sinal alternado. É o que o `edo` resolve, avaliado. */
static SfSerie sf_exp(long x, long S){
    SfSerie r; r.valor = S; r.termos = 1; r.parou = 0;
    long T = S;
    for(int k = 1; k <= 64; k++){
        T = (T / k) * x / S;
        if(T == 0){ r.parou = 1; break; }
        r.valor += T;
        r.termos++;
    }
    return r;
}

/* ═══ A EDO RESOLVIDA PELA SÉRIE --- e a raiz NUNCA é precisa ══════════════
 *
 * Este é o ponto: para avaliar a solução de
 *
 *     y^{(n)} + c_{n-1} y^{(n-1)} + ... + c_0 y = 0
 *
 * não é preciso extrair raiz nenhuma. As derivadas em zero saem da PRÓPRIA
 * recorrência --- é ela que as dá ---
 *
 *     y^{(k+n)}(0) = −c_{n-1} y^{(k+n-1)}(0) − ... − c_0 y^{(k)}(0),
 *
 * e a série de Taylor é FATORIAL por construção:
 *
 *     y(t) = Σ_k y^{(k)}(0) · t^k / k!
 *
 * O termo calcula-se por recorrência, T_{k+1} = T_k · t/(k+1) com o T a
 * carregar já a derivada --- nunca se forma t^k nem k!. E os termos ANULAM-SE
 * (thm:serie), pelo que o processo TERMINA e o número de termos é o custo.
 *
 * Isto resolve o irracional sem o nomear: quando as raízes são (−B±√Δ)/2 com Δ
 * não quadrado, a solução avaliada sai na mesma, exacta na escala --- porque a
 * série não precisa de saber quais são as raízes, só dos coeficientes.
 * E quando elas são inteiras, a série tem de bater com a forma fechada: é o
 * controlo. */

#define SF_ORD 8

/* avalia y(t) para a EDO de ordem n com coeficientes `co` (do termo constante
 * para cima, mónico implícito) e condições iniciais `d0` = (y(0), y'(0), ...),
 * tudo em escala S. Devolve o valor e os termos gastos. */
static SfSerie sf_edo(const long *co, int n, const long *d0, long t, long S){
    SfSerie r; r.valor = 0; r.termos = 0; r.parou = 0;
    /* O TERMO CARREGA JÁ O FATORIAL, e é isso que evita a perda: escrevendo
     *
     *     w_k = y^{(k)}(0) · t^k / k!,
     *
     * a recorrência das derivadas transporta-se para os w directamente ---
     *
     *     w_{k+n} = − Σ_j c_j · w_{k+j} · t^{n-j} / ((k+j+1)···(k+n)),
     *
     * porque y^{(k+j)} = w_{k+j}·(k+j)!/t^{k+j}. Assim nunca se forma t^k nem
     * k!, nunca se divide um valor grande por outro, e a soma é dos próprios w.
     *
     * A primeira escrita disto guardava a derivada e o t^k/k! SEPARADOS e
     * multiplicava-os com duas divisões por mil --- e perdia: dava 0,16 onde a
     * forma fechada dá 0,307. O controlo apanhou-a, e é para isso que ele lá
     * está. */
    long w[SF_ORD];
    for(int k = 0; k < n && k < SF_ORD; k++){
        /* w_k = d0[k]·t^k/k!, com o t^k/k! por recorrência */
        long pot = S;
        for(int u = 0; u < k; u++) pot = (pot / (u+1)) * t / S;
        w[k] = (k == 0) ? d0[0] : (d0[k] / 1000) * (pot / 1000);
    }
    for(int k = 0; k < n; k++){ r.valor += w[k]; r.termos++; }
    for(int k = n; k < 64; k++){
        long nova = 0; int estourou = 0;
        for(int j = 0; j < n; j++){
            /* c_j · w_{k-n+j} · t^{n-j} / ((k-n+j+1)···k) */
            long termo = -co[j] * w[j];
            for(int u = 0; u < n - j; u++){
                if(termo > (1L<<50) || termo < -(1L<<50)){ estourou = 1; break; }
                termo = termo * t / S;
            }
            if(estourou) break;
            for(int u = k - n + j + 1; u <= k; u++) termo /= u;
            nova += termo;
        }
        if(estourou) break;
        for(int j = 0; j + 1 < n; j++) w[j] = w[j+1];
        w[n-1] = nova;
        if(nova == 0){ r.parou = 1; break; }
        r.valor += nova;
        r.termos++;
    }
    return r;
}

/* e o CONTROLO: a mesma solução pela forma fechada, quando as raízes são
 * inteiras --- y = Σ A_i e^{λ_i t}. Serve para confrontar, não para resolver. */
static long sf_fechada(const long *lam, const long *amp, int m, long t, long S){
    long v = 0;
    for(int i = 0; i < m; i++){
        SfSerie e = sf_exp(lam[i] * t / S, S);
        v += (amp[i] / 1000) * (e.valor / 1000);
    }
    return v;
}

/* ═══ A RAIZ EM BASE FATORIAL: exacta, em dígitos, sem uma vírgula ═════════
 *
 * A série da aranha escreve tudo sobre 1/k!. A representação que lhe corresponde
 * é a BASE FATORIAL --- um número escreve-se
 *
 *     x = d_0 + Σ_{k≥1} d_k / k!,      com  0 ≤ d_k ≤ k,
 *
 * e ela não é uma escolha de conveniência: é a base em que e = Σ1/k! tem
 * dígitos todos iguais a 1, e em que os termos c(t) e s(t) já estão escritos.
 * Cada nível k tem k+1 dígitos possíveis --- a aridade CRESCE ---, e descer por
 * eles é o «navegar = descer por prefixo onde o prefixo é a bola» do paper: o
 * prefixo de k dígitos fixa uma bola, e escolher d_{k+1} parte-a em k+2.
 *
 * Aqui não há aproximação nem arredondamento: cada dígito é um INTEIRO decidido
 * pelo sinal de P, que se calcula em inteiros --- com x = N/k!, o valor
 * P(N/k!)·(k!)^n é inteiro e tem o mesmo sinal. O processo PÁRA onde o produto
 * deixa de caber no anel, e o nível a que parou é o CUSTO, que se conta.
 *
 * Medido em `tests/serie.c` §S5. */

#define FAT_NIV 12

typedef struct {
    long d[FAT_NIV+1];   /* os dígitos: d[0] inteiro, depois 0 ≤ d[k] ≤ k */
    int  niveis;         /* até onde desceu */
    int  parou_por;      /* 0 = exacto (P deu zero), 1 = o anel encheu */
    long num, den;       /* o valor como fracção N/k!, exacta */
    int  achou;
} FatRaiz;

/* sinal de P(N/D) sem sair dos inteiros: Σ c_j N^j D^{n-j}, com guarda */
static int fat_sinal(const long *co, int n, long N, long D, int *coube){
    long acc = 0; *coube = 1;
    const long T = 1L << 58;
    for(int j = n; j >= 0; j--){
        long cj = (j == n) ? 1 : co[j];
        long termo = cj;
        for(int u = 0; u < n - j; u++){
            if(termo > T/D || termo < -(T/D)){ *coube = 0; return 0; }
            termo *= D;
        }
        if(acc > T/(N?N:1) || acc < -(T/(N?N:1))){ *coube = 0; return 0; }
        acc = acc * N + termo;
    }
    return acc > 0 ? 1 : (acc < 0 ? -1 : 0);
}

/* desce os dígitos factoriais da raiz que vive em [a, a+1] */
static FatRaiz fat_raiz(const long *co, int n, long a){
    FatRaiz r; r.niveis = 0; r.parou_por = 1; r.achou = 0;
    r.num = 0; r.den = 1;
    for(int k = 0; k <= FAT_NIV; k++) r.d[k] = 0;
    int coube;
    int sa = fat_sinal(co, n, a, 1, &coube);
    if(!coube) return r;
    int sb = fat_sinal(co, n, a+1, 1, &coube);
    /* se P(a) ou P(a+1) é zero, a raiz é INTEIRA e não pede base nenhuma ---
     * o caminho das racionais já a dá, e diz-se em vez de se calar */
    if(sa == 0){ r.achou = 1; r.parou_por = 0; r.d[0] = a; r.num = a; r.den = 1;
                 r.niveis = 0; return r; }
    if(sb == 0){ r.achou = 1; r.parou_por = 0; r.d[0] = a+1; r.num = a+1; r.den = 1;
                 r.niveis = 0; return r; }
    if(!coube || sa == sb) return r;                         /* sem raiz cercada */
    r.achou = 1;
    r.d[0] = a;
    long N = a, D = 1;                       /* o prefixo corrente é N/D */
    for(int k = 1; k <= FAT_NIV; k++){
        long Dn = D * k;
        if(Dn / k != D) break;               /* o factorial estourou */
        long Nn = N * k;
        /* procura o maior d ≤ k com P((Nn+d)/Dn) do mesmo sinal que sa */
        long escolhido = 0; int falhou = 0;
        for(long d = 0; d <= k; d++){
            int s2 = fat_sinal(co, n, Nn + d, Dn, &coube);
            if(!coube){ falhou = 1; break; }
            if(s2 == 0){ escolhido = d; r.parou_por = 0; r.d[k] = d;
                         r.num = Nn + d; r.den = Dn; r.niveis = k; return r; }
            if(s2 == sa) escolhido = d; else break;
        }
        if(falhou) break;
        r.d[k] = escolhido;
        N = Nn + escolhido; D = Dn;
        r.niveis = k;
        r.num = N; r.den = D;
    }
    return r;
}

/* ═══ AS TRÊS FACES SAEM DA MESMA SÉRIE: ω² = t, com t ∈ {−1,0,+1} ═══════════
 *
 * O paper faz a cisão por paridade com J² = −1. Com ω² = t as potências ciclam
 * do mesmo modo, mas o que elas devolvem depende de t:
 *
 *     ω^{2k} = t^k,        ω^{2k+1} = t^k·ω,
 *
 * pelo que a exponencial se parte exactamente na mesma:
 *
 *     exp(xω) = c_t(x)·1 + s_t(x)·ω,
 *     c_t(x) = Σ_k t^k x^{2k}/(2k)!,     s_t(x) = Σ_k t^k x^{2k+1}/(2k+1)!
 *
 * e as TRÊS CLASSES do thm:leidisc são os três valores de t --- não três
 * teorias, três leituras da mesma série:
 *
 *   t = −1  ELÍPTICO     c,s = cos,sen    ω⁴ = 1, período 4   RODA
 *   t =  0  PARABÓLICO   c,s = 1, x       ω² = 0, nilpotente  DESLIZA
 *   t = +1  HIPERBÓLICO  c,s = cosh,senh  ω² = 1, período 2   FOGE
 *
 * E A CONSERVAÇÃO É UMA SÓ, que é a NORMA da tríade:
 *
 *     c_t² − t·s_t² = 1     (= N(c + sω), com N(a+bω) = a² − t b²)
 *
 * --- em t=−1 é c²+s²=1, em t=0 é c²=1, em t=+1 é c²−s²=1. A exponencial
 * PRESERVA A NORMA nas três faces, e isso não se impõe: sai da derivação,
 * (c²−ts²)' = 2cc' − 2t s s' = 2c(ts) − 2t s c = 0.
 *
 * E o caso t=0 é o que se vê de imediato: a série TERMINA em dois termos, porque
 * ω²=0 mata tudo o resto. O parabólico é a face onde a série é FINITA. */

typedef struct { long c, s; int termos; int parou; } FtFace;

static FtFace ft_face(long x, int t, long S){
    FtFace r; r.c = S; r.s = x; r.termos = 1; r.parou = 0;
    if(t == 0){ r.parou = 1; r.termos = 2; return r; }   /* ω²=0: a série ACABA */
    long Tc = S, Ts = x;
    for(int k = 0; k < 64; k++){
        /* Tc_{k+1} = t·Tc_k·x²/((2k+1)(2k+2)) */
        long a2 = Tc / (2*k+1); a2 = a2 * x / S; a2 = a2 / (2*k+2); a2 = a2 * x / S;
        long b2 = Ts / (2*k+2); b2 = b2 * x / S; b2 = b2 / (2*k+3); b2 = b2 * x / S;
        Tc = t * a2; Ts = t * b2;
        if(Tc == 0 && Ts == 0){ r.parou = 1; break; }
        r.c += Tc; r.s += Ts; r.termos++;
    }
    return r;
}

/* a norma da tríade avaliada na face: tem de dar S², seja qual for t */
static long ft_norma(FtFace f, int t, long S){
    (void)S;
    return (f.c/1000)*(f.c/1000) - (long)t * (f.s/1000)*(f.s/1000);
}

/* ── A MULTIPLICIDADE DE ω É CONTAGEM: aplica-se ω até voltar, e CONTA-SE.
 *
 * O paper não tabela isto --- «o índice k é o que conta as VOLTAS do ciclo de
 * quatro», «π lê-se contando voltas», e o thm:periodo lê |{G>1}| = p no campo
 * sem seguir a trajectória. A ordem de ω é o mesmo: o menor k>0 com ω^k = 1, e
 * acha-se APLICANDO.
 *
 * Um elemento a+bω escreve-se como o par (a,b); multiplicar por ω dá
 *
 *     (a + bω)·ω = aω + bω² = b·t + a·ω   →   (a,b) ↦ (t·b, a),
 *
 * que é a companheira 2×2 outra vez. Parte-se de 1 = (1,0) e conta-se até
 * voltar. Devolve 0 quando não volta --- o nilpotente, onde a órbita CAI no
 * zero em vez de fechar, e isso também se vê contando. */
static int ft_ordem(int t){
    long a = 1, b = 0;                          /* o elemento 1 = 1 + 0·ω */
    for(int k = 1; k <= 12; k++){               /* o lem:cristal fecha em 6 */
        long na = (long)t * b, nb = a;          /* ×ω */
        a = na; b = nb;
        if(a == 1 && b == 0) return k;          /* voltou: a ordem é k */
        if(a == 0 && b == 0) return 0;          /* caiu no zero: nilpotente */
    }
    return -1;                                   /* não fechou no tecto da rede */
}

/* e a ÓRBITA escrita, que é o que a contagem percorre --- para o cliente ver
 * de onde a ordem veio, em vez de a receber por decreto */
static void ft_orbita(int t, char *out, long lim){
    long a = 1, b = 0, o = 0;
    o += snprintf(out + o, lim - o, "1");
    for(int k = 1; k <= 6 && o < lim - 12; k++){
        long na = (long)t * b, nb = a;
        a = na; b = nb;
        if(a == 0 && b == 0){ o += snprintf(out + o, lim - o, " → 0"); break; }
        o += snprintf(out + o, lim - o, " → ");
        if(a && b)      o += snprintf(out + o, lim - o, "%ld%+ldw", a, b);
        else if(a)      o += snprintf(out + o, lim - o, "%ld", a);
        else            o += snprintf(out + o, lim - o, "%sw", b == 1 ? "" : (b == -1 ? "-" : ""));
        if(a == 1 && b == 0) break;
    }
}

/* ── A APRESENTAÇÃO: os dígitos fatoriais escritos como o cliente os lê,
 * sem o N/k! gigante. `lim` é o tamanho do buffer. */
static void ft_escreve(const FatRaiz *r, char *out, long lim){
    long o = 0;
    o += snprintf(out + o, lim - o, "%ld", r->d[0]);
    if(r->niveis > 0){
        o += snprintf(out + o, lim - o, ";");
        for(int k = 1; k <= r->niveis && o < lim - 8; k++)
            o += snprintf(out + o, lim - o, "%ld%s", r->d[k], k < r->niveis ? "," : "");
    }
    /* e o valor em milionésimos, que é o que se lê de relance --- calculado
     * dos DÍGITOS e não guardado à parte */
    if(r->den){
        long v = (r->num * 1000000L) / r->den;
        snprintf(out + o, lim - o, "  (%ld,%06ld)", v/1000000, v%1000000);
    }
}

/* ═══ A SOLUÇÃO DA EDO EM NOTAÇÃO FATORIAL: só CONTAGEM ═════════════════════
 *
 * Aqui não há operação em vírgula nenhuma, e não é preciso: os coeficientes da
 * solução SÃO inteiros. Escrevendo
 *
 *     y(t) = Σ_k d_k · t^k / k!,     d_k = y^{(k)}(0),
 *
 * a recorrência da equação DÁ os d_k, um a um, em inteiros exactos:
 *
 *     d_{k+n} = −c_{n-1} d_{k+n-1} − ... − c_0 d_k .
 *
 * Nada se avalia, nada se divide, nada se aproxima --- CONTA-SE. E o que o
 * cliente recebe é a lista dos d_k, que é a solução escrita na base em que a
 * série já vive. Avaliar num t é outra pergunta, e faz-se depois se se quiser.
 *
 * É a mesma disciplina do resto: o valor é uma leitura tardia do objecto, e o
 * objecto é a contagem. */

#define FT_COEF 24

typedef struct {
    long d[FT_COEF];     /* os coeficientes fatoriais, inteiros e exactos */
    int  n;              /* quantos cabem antes de o anel encher */
    int  ordem;          /* a ordem da equação */
    int  estourou;       /* 1 se parou por o coeficiente não caber */
} FtSol;

static FtSol ft_solucao(const long *co, int ordem, const long *d0, int quantos){
    FtSol r; r.n = 0; r.ordem = ordem; r.estourou = 0;
    if(quantos > FT_COEF) quantos = FT_COEF;
    for(int k = 0; k < ordem && k < quantos; k++){ r.d[k] = d0[k]; r.n++; }
    const long T = 1L << 55;
    for(int k = ordem; k < quantos; k++){
        long v = 0; int cabe = 1;
        for(int j = 0; j < ordem; j++){
            long termo = -co[j] * r.d[k - ordem + j];
            if(termo > T || termo < -T){ cabe = 0; break; }
            v += termo;
            if(v > T || v < -T){ cabe = 0; break; }
        }
        if(!cabe){ r.estourou = 1; break; }
        r.d[k] = v; r.n++;
    }
    return r;
}

/* ── A APRESENTAÇÃO É A TRAJECTÓRIA, E O CAMPO LÊ-LHE O CUSTO.
 *
 * Duas escritas minhas já caíram aqui: primeiro <1,0,-1,0>! --- uma lista com
 * um sinal que eu inventei ---, depois «1 − t²/2! + t⁴/4! − ...», que é a série
 * clássica copiada. Nenhuma é o que esta casa faz.
 *
 * O paper diz o que é: «a série é uma TRAJECTÓRIA (def:serie), o índice k conta
 * as VOLTAS do ciclo de quatro, e o campo G lê o custo dela SEM A SEGUIR
 * (thm:serie) --- ℓ é o que se gastou, e p=1 diz que a trajetória parou».
 *
 * Então o que se apresenta são as três contagens, e mais nada:
 *
 *     as VOLTAS   o índice k: o termo n=2k deu k voltas completas, o n=2k+1
 *                 deu k voltas mais um quarto; e (−1)^k é a volta módulo dois
 *     ℓ (a cauda) quantas somas parciais DISTINTAS --- o custo da série
 *     p (o período) que tem de dar 1: a trajectória entrou no ponto fixo
 *
 * Isto não é uma escrita alternativa da série: é o que o thm:serie mede nela. */

typedef struct {
    long voltas;      /* quantas voltas completas o ciclo deu */
    long cauda;       /* ℓ: somas parciais distintas --- o CUSTO */
    long periodo;     /* p: tem de ser 1 quando a série pára */
    long parciais[FT_COEF];
    int  n;
} FtTraj;

/* a trajectória das somas parciais em escala S, e o que o campo lê nela */
static FtTraj ft_trajetoria(const FtSol *s, long t, long S){
    FtTraj r; r.voltas = 0; r.cauda = 0; r.periodo = 0; r.n = 0;
    long soma = 0, T = S;                        /* T_k = t^k/k! em escala */
    for(int k = 0; k < s->n && k < FT_COEF; k++){
        /* os d_k são inteiros PUROS (1, 0, −1, 30, ...) e só o T está em
         * escala --- dividi-los por mil dava zero em todos, e a trajectória
         * saía constante. O produto d_k·T cabe folgadamente. */
        /* T já vem em escala S (T = S·t^k/k!), logo d_k·T é o termo NA escala.
         * Dividir por S outra vez tirava-a, e a trajectória saía em unidades. */
        soma += s->d[k] * T;
        r.parciais[r.n++] = soma;
        T = (T / (k+1)) * t / S;
        if(T == 0) break;
    }
    /* AS VOLTAS: o índice k do ciclo de quatro --- n=2k e n=2k+1 dão k voltas */
    r.voltas = (r.n - 1) / 2;
    /* E O CAMPO LÊ O CUSTO, sem seguir a trajectória: G sobre as somas parciais.
     * ℓ = quantas têm G=1 (a cauda), p = quantas têm G>1 (o período). */
    for(int i = 0; i < r.n; i++){
        long g = 0;
        for(int j = 0; j < r.n; j++) if(r.parciais[j] == r.parciais[i]) g++;
        int novo = 1;
        for(int j = 0; j < i; j++) if(r.parciais[j] == r.parciais[i]){ novo = 0; break; }
        if(!novo) continue;
        if(g > 1) r.periodo++; else r.cauda++;
    }
    return r;
}

/* a escrita: as três contagens, que é o que o cliente lê */
static void ft_sol_escreve(const FtSol *s, char *out, long lim){
    long o = 0;
    o += snprintf(out + o, lim - o, "%d termos · ciclo ", s->n);
    /* o PERÍODO dos coeficientes é a ordem de ω, contada na própria lista ---
     * não se calcula, procura-se onde ela se repete */
    int per = 0;
    for(int p2 = 1; p2 <= 6 && !per; p2++){
        int ok2 = 1;
        for(int k = 0; k + p2 < s->n; k++) if(s->d[k] != s->d[k+p2]){ ok2 = 0; break; }
        if(ok2) per = p2;
    }
    if(per) o += snprintf(out + o, lim - o, "%d", per);
    else    o += snprintf(out + o, lim - o, "--");
    o += snprintf(out + o, lim - o, " · d:");
    for(int k = 0; k < s->n && o < lim - 14; k++)
        o += snprintf(out + o, lim - o, " %ld", s->d[k]);
    snprintf(out + o, lim - o, "%s", s->estourou ? " ..." : "");
}

/* ── O SOMATÓRIO SAI DA CISÃO ⊕, que é a primeira das quatro operações.
 *
 * O algoritmo da aranha tem QUATRO operações e nem uma a mais, e a primeira é a
 * cisão: «as duas máscaras são complementares, logo PARTICIONAM o byte --- é a
 * cisão ⊕ em duas fases, as posições de índice PAR e as de índice ÍMPAR».
 *
 * É isso que dá o somatório, e não uma tabela de casos. Cindem-se os d_k em
 * duas fases; dentro de cada uma vê-se se são constantes ou se ALTERNAM --- e o
 * alternar é o (−1)^k, «o número de voltas lido módulo dois». Cada fase
 * não-nula dá um termo, e a soma das duas é a resposta:
 *
 *     fase par constante c   →   c · SOMA t^{2k}/(2k)!
 *     fase par alternada     →   SOMA (−1)^k t^{2k}/(2k)!
 *     idem para a ímpar, com 2k+1
 *
 * Quatro passos, e todos contagem: cindir, ver se alterna, escrever, somar. */
static void ft_sol_somatorio(const FtSol *s, char *out, long lim){
    long o = 0; int escreveu = 0;
    for(int fase = 0; fase < 2; fase++){
        /* a fase: os d_k com k ≡ fase (mod 2) */
        long v[FT_COEF]; int nv = 0;
        for(int k = fase; k < s->n; k += 2) v[nv++] = s->d[k];
        if(nv == 0) continue;
        /* nula? */
        int nula = 1;
        for(int i = 0; i < nv; i++) if(v[i]){ nula = 0; break; }
        if(nula) continue;
        /* constante ou alternada? --- as duas leituras que a cisão admite */
        int constante = 1, alterna = 1;
        for(int i = 1; i < nv; i++){
            if(v[i] != v[0]) constante = 0;
            if(v[i] != -v[i-1]) alterna = 0;
        }
        if(!constante && !alterna) continue;      /* esta fase não fecha */
        long c = v[0], ac = c < 0 ? -c : c;
        if(escreveu) o += snprintf(out + o, lim - o, " + ");
        else         o += snprintf(out + o, lim - o, "y(t) = ");
        if(c < 0) o += snprintf(out + o, lim - o, "-");
        if(ac != 1) o += snprintf(out + o, lim - o, "%ld", ac);
        o += snprintf(out + o, lim - o, "SOMA_k ");
        if(alterna && !constante) o += snprintf(out + o, lim - o, "(-1)^k ");
        if(fase == 0) o += snprintf(out + o, lim - o, "t^(2k)/(2k)!");
        else          o += snprintf(out + o, lim - o, "t^(2k+1)/(2k+1)!");
        escreveu = 1;
    }
    if(!escreveu)
        snprintf(out, lim, "y(t) = SOMA_k d_k t^k/k!, com d pela recorrencia");
}

/* ── E A SÉRIE DE POTÊNCIAS RESULTANTE, escrita.
 *
 * A contagem é o que a casa FAZ; a série é o que o cliente LÊ. As duas saem do
 * mesmo objecto --- os d_k --- e não se substituem uma à outra: só a contagem
 * deixava o cliente sem a equação, e só a equação escondia o que o campo lê.
 *
 *     y(t) = Σ_k d_k t^k/k!   →   1 + t + t²/2! + t³/3! + ...
 *
 * Os termos nulos não se escrevem, o coeficiente um não se escreve, e o sinal
 * entra no operador --- como se escreve à mão. */
static void ft_sol_serie(const FtSol *s, char *out, long lim){
    long o = 0; int primeiro = 1;
    for(int k = 0; k < s->n && o < lim - 28; k++){
        long d = s->d[k];
        if(d == 0) continue;
        long ad = d < 0 ? -d : d;
        if(primeiro){ if(d < 0) o += snprintf(out + o, lim - o, "-"); }
        else o += snprintf(out + o, lim - o, d < 0 ? " - " : " + ");
        if(k == 0)      o += snprintf(out + o, lim - o, "%ld", ad);
        else {
            if(ad != 1) o += snprintf(out + o, lim - o, "%ld", ad);
            if(k == 1)  o += snprintf(out + o, lim - o, "t");
            else        o += snprintf(out + o, lim - o, "t^%d/%d!", k, k);
        }
        primeiro = 0;
    }
    if(primeiro) o += snprintf(out + o, lim - o, "0");
    snprintf(out + o, lim - o, " + ...");
}


/* ── E O GUME QUE ELA PEDE: os coeficientes têm de SATISFAZER a recorrência.
 * Reconfere-se cada um a partir dos n anteriores --- se algum não bater, a
 * lista não é solução de equação nenhuma. Devolve quantos batem. */
static int ft_sol_confere(const FtSol *s, const long *co){
    int batem = 0;
    for(int k = s->ordem; k < s->n; k++){
        long v = 0;
        for(int j = 0; j < s->ordem; j++) v -= co[j] * s->d[k - s->ordem + j];
        if(v == s->d[k]) batem++;
    }
    return batem;
}

/* ═══ A SOLUÇÃO COM J: o par (c,s), e não um escalar ════════════════════════
 *
 * A equação da face é y'' = t·y --- com ω²=t --- e a sua solução vive no SEGUNDO
 * DEGRAU com a face, X_2 + X_2·ω.
 *
 * E o degrau escreve-se assim porque NÃO HÁ Z nesta teoria. O paper define-o no
 * preâmbulo e diz porquê: «o segundo degrau, e NÃO UM CONJUNTO IMPORTADO: este
 * documento CONSTRÓI-O (§escada, X_2 = X_1²/~). Escrever Z seria usar o que se
 * vai construir.» O X_2 é o quociente por (a,b)~(c,d) ⟺ a+d=b+c, que é o que
 * fecha a face do OPOSTO --- e é dele que os coeficientes aqui são.
 *
 *     y(t) = c(t)·1 + s(t)·ω,
 *
 * que é o exp(tJ) do paper com J geral. As duas metades saem da MESMA
 * recorrência com condições iniciais diferentes: (1,0) dá o c, (0,1) dá o s ---
 * e é a cisão ⊕ outra vez, agora nas condições em vez dos índices.
 *
 * E o que as liga é a NORMA, que a exponencial preserva: c² − t·s² = 1. Isso
 * verifica-se nos COEFICIENTES, sem avaliar em t nenhum --- a convolução dos
 * dois pela regra da casa tem de dar a lista <1,0,0,0,...>. */

typedef struct { FtSol c, s; int t; } FtPar;

static FtPar ft_par(int t, int quantos){
    FtPar r; r.t = t;
    long co[2] = {-(long)t, 0};                  /* y'' − t·y = 0 */
    long d1[2] = {1, 0}, d2[2] = {0, 1};
    r.c = ft_solucao(co, 2, d1, quantos);
    r.s = ft_solucao(co, 2, d2, quantos);
    return r;
}

/* ── A NORMA NOS COEFICIENTES, pela CONVOLUÇÃO DESTA CASA.
 *
 * A operação é a do `def:conv` --- o produto que a bilinearidade estende da
 * base --- e é a mesma que o `thm:zeta-mu` corre como ζ a acumular e μ a
 * desacumular, com o gato ⊗ a subir e o esquilo ⊘ a descer. Na base fatorial
 * ela escreve-se com os pesos que a base pede,
 *
 *     (f·g)_n = Σ_k C(n,k) f_k g_{n-k},
 *
 * porque t^k/k! · t^{n-k}/(n-k)! = C(n,k)·t^n/n!. Não é uma regra importada:
 * é a mesma convolução, lida na base em que estes coeficientes vivem.
 *
 * Se o par está no nível, o produto dá 1 no termo 0 e zero em todos os outros. */
static int ft_par_norma(const FtPar *p, int ate){
    int batem = 0;
    for(int n = 0; n < ate && n < FT_COEF; n++){
        /* (fg)_n = Σ_k C(n,k) f_k g_{n-k}, que é o produto na base fatorial */
        long acc = 0, bin = 1;
        for(int k = 0; k <= n; k++){
            if(k >= p->c.n || n-k >= p->c.n) break;
            acc += bin * (p->c.d[k] * p->c.d[n-k] - (long)p->t * p->s.d[k] * p->s.d[n-k]);
            bin = bin * (n - k) / (k + 1);
        }
        if(acc == (n == 0 ? 1 : 0)) batem++;
    }
    return batem;
}

/* ── A RÉGUA SOBRE OS COEFICIENTES: a ultramétrica dos endereços, aplicada à
 * lista dos d_k. Ela desce sempre (lem:ultra), e o que a distingue entre as
 * faces são os ESTRITOS --- onde a régua separa em vez de empatar. */
typedef struct { long triplos, viola, estrito; } FtUltra;

static FtUltra ft_ultra(const FtSol *s, int bits){
    FtUltra u = {0,0,0};
    for(int i = 0; i < s->n; i++) for(int j = 0; j < s->n; j++) for(int k = 0; k < s->n; k++){
        long x = s->d[i] + 4096, y = s->d[j] + 4096, z = s->d[k] + 4096;
        int a = tv_prof_local(x, y, bits), b = tv_prof_local(y, z, bits),
            c = tv_prof_local(x, z, bits);
        int m = a < b ? a : b;
        u.triplos++;
        if(c < m) u.viola++; else if(c > m) u.estrito++;
    }
    return u;
}

#endif /* FATORIAL_H */
