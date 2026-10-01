# PROVENIENCIA — JEV dentro do tiffany-db

Registo do que foi trazido, de onde, e do que se prova. Escrito antes de qualquer
decisão de empacotamento, porque o empacotamento ainda não está autorizado.

## 1. A fonte

| | |
|---|---|
| ficheiro | `conecthus/backends/wasm/jev/marginal.c` |
| blob de origem | `20e64e626ca923d2cddb5d3dcdff20d6b3dbb16b` |
| sha256 do ficheiro | `d1d5bf8538d221bcc486e31d7b8dccfcb047a0c51891d00987b3de85b048b1e4` |
| tamanho | 8224 bytes |
| funções | `proj`, `marginal`, `escore`, `duas`, `marginal_d` — 5 |

### Havia duas fontes, e só uma está publicada

`origin/master` (`bdf56b27`) tem um `marginal.c` **de 5144 bytes, 4 funções** —
sem `marginal_d`. A árvore de trabalho tem a versão de **8224 bytes, 5 funções**,
que acrescenta `marginal_d` (a ponte com `iota_d`, o bloco E7).

A versão de 5 funções **nunca foi commitada nem enviada**: `marginal.c`,
`marginal.wasm` e `tests/jev_backends.js` estão todos entre os ficheiros
modificados da árvore de trabalho. Escolhida a versão de 5 funções — é a que tem o
motor dimensional e a que os ensaios de equivalência mediram.

**Consequência a resolver antes de empacotar:** a proveniência aponta para um
*blob* e um sha256, não para um commit, porque esse conteúdo não existe em
nenhum. Ou o bloco E7 é commitado no `tiffany`, ou esta cópia passa a ser a
única forma de o preservar.

### O `.wasm` usado como oráculo foi validado contra o seu próprio fonte

Rebuilt com o `tools/bin/traduz.exe` do próprio repositório, a partir do mesmo
`marginal.c`:

```
marginal.c (árvore) → traduz.exe → 6712 B, sha256 e2831aa848c57035d0760d0a8ce1408e5d8c7f83e4598bb60a38b40a42f6729f
assets/figuras/wasm/jev/marginal.wasm         = 6712 B, sha256 e2831aa848c57035d0760d0a8ce1408e5d8c7f83e4598bb60a38b40a42f6729f
```

Idênticos byte a byte, e o tradutor reporta «emite: 5 funções». O oráculo
corresponde ao fonte, e não a um artefacto mais antigo.

## 2. A transformação

`jev_core.c` **foi gerado, não escrito à mão**: parte-se do original e
aplicam-se sete alterações, todas em linhas de declaração ou de assinatura.

| # | original | agora |
|---|---|---|
| 1 | — | `#include "jev_core.h"` no topo |
| 2 | `unsigned char arena[65536];` | `static unsigned char arena[65536] __attribute__((aligned(4)));` |
| 3 | `int proj(int b, int c, int x){` | `static int jev_proj_raw(int b, int c, int x){` |
| 4 | `int marginal(int X){` | `static int jev_marginal_raw(int X){` |
| 5 | `int escore(int X){` | `static int jev_escore_raw(int X){` |
| 6 | `int duas(int X){` | `static int jev_duas_raw(int X){` |
| 7 | `int marginal_d(int X){` | `static int jev_marginal_d_raw(int X){` |

Comparado bloco a bloco, linha a linha. Em cada servo, **a única linha que
difere é a da assinatura**; o resto é o original caractere a caractere:

```
servo        linhas do bloco (assinatura + corpo)   corpo        linhas que diferem
proj                 11                              IDENTICO              1
marginal             45                              IDENTICO              1
escore               16                              IDENTICO              1
duas                 24                              IDENTICO              1
marginal_d           47                              IDENTICO              1
```

**Zero diferenças em todas as linhas de corpo**, e uma em cada linha de
assinatura — as sete da tabela acima.

### O porquê do sufixo `_raw`

Um corpo só pode chamar o que o precede. A guarda de `proj` tem de estar *acima*
de `proj` sem estar *dentro* de `proj`. Dar o nome ao invocador em vez do invocado
resolve: os quatro corpos chamadores continuam a escrever `proj(...)`, com nome e
argumentos intactos, e é um invólucro no fim do ficheiro que marca o `-1`.

### O porquê do alinhamento

A arena é `unsigned char[65536]` — alinhamento declarado 1 — e é lida como `int *`.
Isso é UB portátil e **o compilador não o denuncia**: `gcc -Wall -Wextra` compila
isto sem um único aviso. Em x86-64 a linkedação entrega a arena múltipla de 4
(medido: 0 mod 4 e 0 mod 8), e por isso funciona. Pedir 4 bytes na declaração
corrige sem tocar em nenhum corpo e sem mudar um único valor de saída.

## 3. A prova de equivalência

| | |
|---|---|
| casos | 96 — E1, E2, E3, E4, E5, E5-isolado, E7a, E7b |
| o que se compara | os **65 536 bytes** da arena depois da chamada, não os valores de saída |
| comparação | 96 × 65 536 = **6 291 456 bytes** |
| resultado | **0 diferenças** |
| recusas | 0 — as 96 entradas estão bem formadas, e a porta aceita-as |

Comparação feita entre a saída do `marginal.wasm` original e a saída da **porta**
(`jev_arena()` + `jev_*`), não a dos servos crus: prova que o que o utilizador
vê é o mesmo que o motor WASM produzia.

Por que a arena inteira e não os valores: porque a arena é o espaço de trabalho,
e um motor que acerta o valor visível mas escreve algures a mais não está certo.
O `memcmp` apanha isso; a comparação de saídas não.

### Como correr isto

```
$ make test        # compila os dois testes e corre-os
$ make simbolos    # lista os simbolos que entrariam no ABI
```

Os dois testes são independentes entre si e da prova de 96 casos:

| teste | o que mede | resultado |
|---|---|---|
| `test_jev_oraculo` | 244 verificações contra um **oráculo em C escrito à parte**, que não lê o motor nem o `tiffany` | 244 verificações, 0 falhas |
| `test_jev_api` | 27 verificações sobre a **porta**: arena, determinismo, `proj`, recusa, `duas` | 27 verificações, 0 falhas |

O `test_jev_oraculo` é o que fica no produto: recalcula o resultado por um caminho
diferente e compara. Se os dois concordarem, nenhum deles está a partilhar o
mesmo erro.

Compilados com `-std=c99 -pedantic -Wall -Wextra -Werror`: **os três ficheiros
passam sem um único aviso**, e continuam a passar em `-O0`, `-O1`, `-O2` e `-O3`.
O `-Werror` está aqui de propósito — um motor que entra na biblioteca com avisos é
um motor que ninguém vai avisar depois.

O `-O0` não é preciosismo. O `test_jev_oraculo` chegou a dar 6 falhas só a `-O0`
e passar a `-O2`, e a causa eram dois defeitos **do próprio teste**: um buffer de
16 bytes que ia receber até 64, e um literal de 6 bytes lido como `int[2]`. São
dois casos de comportamento indefinido, e o `-O2` tapava-os. Corrigidos os dois, a
diferença desapareceu — que é a forma de se saber que era isso e não outra coisa.
Nenhum dos dois afectava o motor: a prova dos 96 casos passava com eles lá dentro.

## 4. As duas divergências, e são só duas

**O retorno.** Os servos devolvem `0`, como o original. A porta devolve `1` para
sucesso, seguindo a casa: `sql_abrir` devolve `1` (`banco/sql.c:16067`) e `0` na
falha (`:16055`), e `sql_executa` devolve `0` no erro.

**A recusa.** Quando uma projeção cai fora de todos os blocos, o original usava o
`-1` como índice. Medido antes de mudar:

```
fronteiras antes : {10, 20, 0,  1}
fronteiras depois: {10, 20, 0, 11}    <- o -1 escreveu por cima do último valor
rc devolvido     : 0                   <- «correu bem»
campo G [0..X)   : intacto
segfault         : nenhum
```

A porta devolve `JEV_ERR_CLASSE`. O `-1` continua a ser devolvido por `jev_proj` —
ali é a resposta certa, e quem pergunta «a que classe pertence x?» tem direito a
sabê-lo.

### O outro lado, que também é verdade

A recusa acontece **depois** do servo escrever. O campo G sobrevive; **as
fronteiras não**. `test_jev_api.c` mede as duas coisas de propósito, para que
ninguém leia «devolveu 0» como «a arena ficou intacta».

Recusar *antes* de escrever seria melhor, e não foi feito: o servo original não
tem onde recuar, e introduzir uma passagem de validação seria alterar o corpo que
este ficheiro existe para preservar. Fica registado como trabalho futuro, com o
mesmo orçamento de 96 casos para o provar.

## 5. Impacto na ABI

```
antes : 39 exportados  (32 funções sql_*, 4 pgwire_*, 3 extern long)
depois: 45 exportados  (39 + 6)
novos  : jev_arena  jev_marginal  jev_escore  jev_duas  jev_marginal_d  jev_proj
```

Verificado a nível de objecto (`nm` sobre `jev_core.o`): exactamente esses 6 e
nenhum mais. `arena`, `proj` e `jev_bad` são `static` e não entram; os cinco
servos `*_raw` também não, e a primeira versão deste trabalho tinha-os a globais
— 11 símbolos em vez de 6 — o que foi corrigido antes de ser medido outra vez.

`SONAME` mantém-se `libtiffanydb.só.1`: adicionar símbolos não obriga a mudá-lo.
Nenhum dos 39 é tocado. **A versão proposta é `1.1.0-1`.**

## 6. O que esta execução NÃO fez

Por instrução, e não por esquecimento:

- **não** commitar, **não** fazer push, **não** taguear, **não** publicar
- **não** tocar em `debian/rules`, `debian/*.install`, `tiffanydb.pc`,
  `debian/changelog` — o empacotamento continua intacto
- **não** tocar em `LICENSE`, `NOTICE`, `README`, `PROPOSTA_TIFFANY_DB.md`,
  `RELEASE_MANIFEST_TIFFANY_DB_V1.md`
- **não** alterar SQL, `sql.c`, ou o runtime de `pgwire`

Ficam pendentes para uma execução com autorização de empacotamento: mover
`jev_core.c` para `banco/`, decidir o nome do header instalado, `debian/rules`
(`.o` novo na prova anti-LTO, no `ar rcsD` e no `-shared`), `.install`,
`changelog` a `1.1.0-1`, e as quatro builds para confirmar reprodutibilidade.

## 7. Ressalva sobre o ambiente

Tudo o que foi medido correu em **mingw-w64 x86_64 (gcc 16.1.0)**. Não há WSL nem
Docker nesta máquina. O produto é empacotado em Debian/Ubuntu 24.04 amd64.

Portanto: o que está provado é **o algoritmo**, não a build empacotada. A
equivalência dos 6 291 456 bytes é uma propriedade do código e não do toolchain —
mas a reprodutibilidade, os `.deb`, os avisos do `gcc` da Debian e o `nm -D` sobre
o ELF são **por medir**, e nada aqui os dá por medidos.
