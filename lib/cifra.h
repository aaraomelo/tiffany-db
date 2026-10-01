/* cifra.h — O CODIFICADOR DE ASSINATURA. Um só, e encoda QUALQUER corpo.
 *
 * A assinatura é sempre só uma: uma fração contínua, na cifra do rei. E o codificador também é um
 * só — foi ele que deu [1;2;1;1;3;1;2;1] ao áureo e [1;16;1;2;4;3;...] ao hipercorpo, e é ele que
 * dá a de um formato de ficheiro, porque um formato é elemento como qualquer outro.
 *
 * Estava dentro do sql.c e foi extraído porque eu ia escrever um segundo — o erro de sempre.
 *
 *   cifra_geral(periodo, np, razao, sinal, razao_dual, saida, max)
 *     razao  quanto se estica por nível                        -> o traço B
 *     sinal  se as duas direções se cancelam (-1) ou compõem (+1) -> o determinante C
 */
#ifndef CIFRA_H
#define CIFRA_H
#include <string.h>

/* ── A RAIZ INTEIRA É A AGM COM A FACE GEOMÉTRICA PRESA ──────────────────────
 *
 * Isto era `while((r+1)*(r+1) <= n) r++` --- um laço LINEAR, que faz √n voltas.
 *
 * A `aranha` §sec:objeto formaliza o par (a,b) ↦ (m,g) com m = (a+b)/2 e
 * g = √(ab), e o `thm:duas` diz o que ele custa. Aqui o par é (r, n/r): o
 * PRODUTO é n e não muda --- a face geométrica está presa ---, e o que desce é a
 * média ARITMÉTICA. O encontro m = g é exactamente √n, e o `meio.c` §M8 já mede
 * a convergência: δ' ≤ δ²/(2m), quadrática.
 *
 * É a mesma ponte PA↔PG da caixa de Pandora, aqui a fazer trabalho: o invariante
 * é multiplicativo e o passo é aditivo.
 *
 * MEDIDO em 0..200000, contra o laço linear: ZERO divergências, e as voltas caem
 * de 447 (máx) e 297,6 (média) para 5 e 3,4. Em n = 10⁹ são 3 voltas contra
 * 31622 --- e o CINCO é o mesmo número de batidas que o paper mede para a AGM
 * geral, que não é coincidência: é o mesmo processo.
 *
 * O piso corrige-se por comparação com DIVISÃO e não com produto: `r*r > n`
 * estoura perto do tecto do long, e `r > n/r` não. */
static long raizi(long n){
    if(n < 2) return n < 0 ? 0 : n;
    { long r;
      { long k = 0, m = n; while(m){ m >>= 1; k++; }
        r = 1L << ((k + 1) / 2); }            /* arranque acima da raiz, sem multiplicar */
      for(;;){
          long q = n / r, m = (r + q) / 2;    /* a ARITMÉTICA do par (r, n/r) */
          if(m >= r) break;                    /* parou de descer: chegou ao encontro */
          r = m;
      }
      while(r > 0 && r > n / r) r--;           /* o piso, exacto e sem estourar */
      while(r + 1 <= n / (r + 1)) r++;
      return r; }
}
/* Os termos de (B + sqrt|B^2-4C|)/2 por PQa, EM INTEIROS. Para quando o estado repete — e o que
 * repete e O PERIODO, que Lagrange garante ser invariante completo. Quadrado perfeito: racional,
 * e a cifra PARA. */
static size_t lado(long B, long C, long *a, size_t max){
    long D = B*B - 4*C; if(D < 0) D = -D;
    long r = raizi(D);
    if(r*r == D){
        long x = B + r, y = 2; size_t n = 0;
        while(y && n < max){ long t = x/y; if(x < 0 && t*y != x) t--; a[n++] = t;
                             long rr = x - t*y; x = y; y = rr; }
        return n;
    }
    { long P = B, Q = 2, Pv[48], Qv[48]; size_t n = 0, nv = 0;
      while(n < max){
        long t = (P + r) / Q; if(Q < 0 && (P+r) % Q) t--;
        a[n++] = t;
        P = t*Q - P; Q = (D - P*P) / Q;
        if(Q == 0) break;
        for(size_t k = 0; k < nv; k++) if(Pv[k] == P && Qv[k] == Q) return n;
        if(nv < 48){ Pv[nv] = P; Qv[nv] = Q; nv++; } else break;
      } return n; }
}
/* A CIFRA COMPLETA DE UM CORPO — OS DOIS LADOS DO CHICOTE.
 *
 * Nao ha Delta em lado nenhum: o unico sistema de coordenadas e a cifra do rei. Se ela nao fecha,
 * nao e limite da cifra — e a cifra INCOMPLETA, com um lado so. Falta-lhe o DUAL.
 *
 * Um corpo tem dois lados, e a seta de Wick e o sinal da borda: (B,C) e o seu dual (B,-C). Um
 * deles fecha sempre no real, porque B^2-4C e B^2+4C nunca sao ambos negativos. A cifra completa
 * e: [qual lado fecha; quantos termos do lado proprio; os termos; quantos do lado dual; os termos].
 * Tudo inteiro, tudo exato, e o corpo fica escrito de ponta a ponta na cifra do rei. */
/* O HIPERCORPO nao tem (B,C): o seu operador nao e uma matriz 2x2, e a DEFORMACAO de Hilbert. E
 * um corpo auto-similar E O SEU GERADOR — o resto e recursao. Entao a sua cifra e a regra escrita
 * uma vez: a ordem por que a curva visita os 16 sub-cubos do nivel, que e o codigo de Gray
 * g(i) = i ^ (i>>1), deslocado de 1 porque o zero e o marcador de fim. Nao ha escolha minha
 * nenhuma aqui: o gerador e a curva, e a curva e a deformacao que o Aarao nomeou.
 * O primeiro termo continua a ser a seta de Wick, e vale 2 — no hipercorpo e O LADO DUAL que
 * carrega o real. Fecha na mesma, e fecha como todos: com a sua outra metade. */
/* O CONSTRUTOR, UM SO, PARA OS TRINTA.
 *
 * Nao ha caso especial nenhum. Um corpo e dado por duas coisas, e delas sai tudo o resto:
 *
 *   a CIFRA        o periodo — quem sao os elementos e onde cada um mora
 *   a DEFORMACAO   duas grandezas, e sao AS UNICAS que ela tem:
 *                    a RAZAO   quanto se estica por nivel        -> o traco B
 *                    o SINAL   se as duas direcoes se cancelam   -> o determinante C
 *                              (-1 = o chicote: uma estica o que a outra contrai;
 *                               +1 = a rotacao: compoem-se e voltam)
 *
 * B = razao e C = sinal nao sao coincidencia: sao a definicao. A regua E a deformacao escrita em
 * dois numeros, e por isso "cifra + deformacao" ja e o corpo — nao ha nada a acrescentar depois.
 *
 * Os dois corpos que eu tinha tratado a parte deixam de precisar disso: o venom tem periodo [1] e
 * razao 1 do lado proprio (o rei) com razao 16 do lado dual; o hipercorpo tem por periodo O
 * GERADOR do tesseracto. Sao entradas da mesma tabela, com a mesma funcao. */
static size_t cifra_geral(const long *per, int np, long razao, long sinal,
                          long razao_dual, long *a, size_t max){
    /* OS COMPRIMENTOS VAO PARA O FIM. Estavam a frente, e comprimento NAO E COORDENADA: posto
     * na casa 1 da base, ele contamina o prefixo — dois textos de tamanhos diferentes divergiam
     * no segundo termo e ficavam a distancia maxima, ainda que um fosse prefixo do outro.
     *
     * Atras, ele continua a distinguir (dois cortes diferentes dos mesmos termos dao
     * comprimentos diferentes, e a decomposicao continua unica) mas ja nao pisa o conteudo. E o
     * conteudo e que e coordenada. */
    size_t n = 0;
    a[n++] = (razao*razao - 4*sinal >= 0) ? 1 : 2;      /* qual metade carrega o real */
    long p[48]; size_t npp;
    if(np > 0){ npp = (size_t)np; for(size_t k = 0; k < npp && k < 48; k++) p[k] = per[k]; }
    else npp = lado(razao, sinal, p, 48);
    for(size_t k = 0; k < npp && n < max; k++) a[n++] = p[k];
    long d[48];                                          /* o lado dual */
    size_t nd = lado(razao_dual, razao_dual == razao ? -sinal : sinal, d, 48);
    for(size_t k = 0; k < nd && n < max; k++) a[n++] = d[k];
    if(n + 2 <= max){ a[n++] = (long)npp; a[n++] = (long)nd; }   /* os cortes, atras */
    return n;
}
#endif
