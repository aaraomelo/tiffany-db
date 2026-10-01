# RELEASE MANIFEST â€” Tiffany DB V1

Documento de fecho da etapa **I0 (PRÃ‰-APT/LICENÃ‡A)**.

Todos os valores abaixo foram **medidos**, nÃ£o estimados. Cada nÃºmero aponta
para a mediÃ§Ã£o que o produziu. Onde nÃ£o houve mediÃ§Ã£o, diz-se "nÃ£o medido" em
vez de se supor.

Este manifesto **nÃ£o publica nada**. Descreve artefactos que ainda nÃ£o podem ser
redistribuÃ­dos (Â§12).

---

## 1. IdentificaÃ§Ã£o

| Campo | Valor |
|---|---|
| Produto | Tiffany DB |
| VersÃ£o do produto (como o `pkg-config` a reporta) | `1.0.0` |
| VersÃ£o do pacote Debian (produto + revisÃ£o) | `1.0.0-1` |
| Arquitectura | `amd64` |
| Formato persistente | versÃ£o 1 (dados) â€” nÃ£o versionado por este empacotamento |
| Formato `pgwire` | versÃ£o 1 (protocolo) â€” nÃ£o versionado por este empacotamento |
| Objecto partilhado | `libtiffanydb.so.1.0.0` |
| SONAME | `libtiffanydb.so.1` |
| ABI | 1 |
| `Multi-Arch` | `same` (ambos os pacotes) |
| Fonte | `Source: libtiffanydb` |

As quatro coisas acima sÃ£o diferentes e nÃ£o se confundem: `1.0.0-1` Ã© a versÃ£o do
pacote, `1.0.0` Ã© a versÃ£o do produto, `.so.1` Ã© a ABI, e `1` na linha de dados
e no protocolo Ã© formato, nÃ£o revisÃ£o.

---

## 2. Ambiente de mediÃ§Ã£o

| Item | Valor |
|---|---|
| DistribuiÃ§Ã£o do host de build | Ubuntu 24.04.4 LTS, `amd64`, 4 CPU |
| Compilador | `cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0` |
| Contentor de validaÃ§Ã£o funcional | Ubuntu 24.04.5 |
| Contentor do teste APT local (I0) | `ubuntu:24.04`, `libc6 2.39-0ubuntu8.9` |
| `debhelper-compat` | 13 |
| `Standards-Version` | 4.7.0 |

Todas as mediÃ§Ãµes de ELF, `lintian` e conteÃºdo de pacote foram feitas no host de
build. O teste de instalaÃ§Ã£o, `pkg-config`, consumidor e persistÃªncia foi feito em
contentor limpo.

---

## 3. Artefactos

### 3.1 Pacotes â€” os ficheiros que seriam distribuÃ­dos

> **Estes sÃ£o os hashes da BUILD 4 (2026-09-30), nÃ£o da build 3.** O `SHA-256` e o
> tamanho do pacote de desenvolvimento mudaram porque `debian/README` distribuiu
> um nÃºmero factualmente errado sobre `.bss` e foi corrigido. O pacote runtime Ã©
> **byte a byte idÃªntico** ao da build 3. Detalhe em Â§8.3.

| Ficheiro | Tamanho | SHA-256 |
|---|---:|---|
| `libtiffanydb1_1.0.0-1_amd64.deb` | 175424 B | `3dd557f6a7f3d5671bf5514a689c36e77c89491b9fe3e7b39b7931e7eae72227` |
| `libtiffanydb-dev_1.0.0-1_amd64.deb` | **202950 B** | **`aae0c6559ea9063f86c6ed6c709691a7722cbb95524ab9cd67e8216cad96ef79`** |

Hashes da build 3, substituÃ­dos por estes:

| Ficheiro | Tamanho (build 3) | SHA-256 (build 3) |
|---|---:|---|
| `libtiffanydb1_1.0.0-1_amd64.deb` | 175424 B | `3dd557f6a7f3d5671bf5514a689c36e77c89491b9fe3e7b39b7931e7eae72227` *(inalterado)* |
| `libtiffanydb-dev_1.0.0-1_amd64.deb` | 202842 B | `69df455381bfb18bf6726bc1af74601cb54114e27c5300d51ddcc703bb06d7b4` *(substituÃ­do)* |

`Installed-Size`: 388 KiB (runtime), 614 KiB (development).

### 3.2 ConteÃºdo exacto do pacote runtime

Enumerado com `dpkg-deb -c`, nÃ£o inferido:

| Caminho | Tipo | Tamanho |
|---|---|---:|
| `usr/lib/x86_64-linux-gnu/libtiffanydb.so.1.0.0` | ficheiro | 378408 B |
| `usr/lib/x86_64-linux-gnu/libtiffanydb.so.1` | symlink â†’ `libtiffanydb.so.1.0.0` | â€” |
| `usr/share/doc/libtiffanydb1/changelog.Debian.gz` | ficheiro | 426 B |
| `usr/share/doc/libtiffanydb1/copyright` | ficheiro, **nÃ£o comprimido** | 5417 B |

Quatro entradas. Nenhum header, nenhuma `.a`, nenhum `.pc`.

### 3.3 ConteÃºdo exacto do pacote development

| Caminho | Tipo | Tamanho |
|---|---|---:|
| `usr/include/tiffanydb/sql_api.h` | ficheiro | 8189 B |
| `usr/include/tiffanydb/pgwire_api.h` | ficheiro | 343 B |
| `usr/lib/x86_64-linux-gnu/libtiffanydb.a` | ficheiro | 584012 B |
| `usr/lib/x86_64-linux-gnu/libtiffanydb.so` | symlink â†’ `libtiffanydb.so.1` | â€” |
| `usr/lib/x86_64-linux-gnu/pkgconfig/tiffanydb.pc` | ficheiro | 257 B |
| `usr/share/doc/libtiffanydb-dev/LICENSE.gz` | ficheiro | 6665 B |
| `usr/share/doc/libtiffanydb-dev/NOTICE` | ficheiro | 3476 B |
| `usr/share/doc/libtiffanydb-dev/README.gz` | ficheiro | 2098 B |
| `usr/share/doc/libtiffanydb-dev/changelog.Debian.gz` | ficheiro | 426 B |
| `usr/share/doc/libtiffanydb-dev/copyright` | ficheiro, **nÃ£o comprimido** | 5417 B |

`copyright` vai sem compressÃ£o nos dois pacotes, como a Policy pede. Os headers
internos **nÃ£o** sÃ£o instalados: hÃ¡ 37 no produto e sÃ³ 2 sÃ£o pÃºblicos.

### 3.4 Bibliotecas compiladas â€” dois valores, e nÃ£o Ã© engano

A biblioteca na Ã¡rvore de build e a biblioteca dentro do `.deb` **nÃ£o sÃ£o o mesmo
ficheiro**: `dh_strip` e `dh_ar` processam a cÃ³pia instalada depois de a ponte de
compilaÃ§Ã£o a ter escrito. As duas mediÃ§Ãµes ficam registadas para que ninguÃ©m
confunda uma com a outra.

| Ficheiro | Onde | Tamanho | SHA-256 |
|---|---|---:|---|
| `libtiffanydb.a` | Ã¡rvore de build | 584316 B | `89f395c6dc774b549be02a63706f9418cb77e7a9ba3119fa9b1813e886d7d52b` |
| `libtiffanydb.so.1.0.0` | Ã¡rvore de build | 398528 B | `bea1b9cd28ccec0d92bdc4edb1cfc1e908a8fc633c4dba5b654c761190e70976` |
| `libtiffanydb.a` | **dentro do `.deb`** | 584012 B | `79c19df31e835d00e73d2bf8afce8f93bd8e2caa8c8dd392f7f34d1677367e76` |
| `libtiffanydb.so.1.0.0` | **dentro do `.deb`** | 378408 B | `e273beb295f70bebe7790bf32a77030c45e8957eb01793e31c9184b399736983` |

O valor que identifica o que se distribui Ã© o da tabela **dentro do `.deb`**
(Â§3.2, Â§3.3, Â§3.1). Os valores da Ã¡rvore de build servem para comparar builds.

### 3.5 SecÃ§Ãµes do objecto expedido

Medido com `size -A` sobre o `.so` extraÃ­do do pacote:

| SecÃ§Ã£o | Tamanho |
|---|---:|
| `.text` | 281813 B |
| `.rodata` | 52996 B |
| `.data.rel.ro` | 3344 B |
| `.data` | 3776 B |
| `.bss` | **3091416 B** (â‰ˆ 3,0 MiB) |
| Total | 3461965 B |

`.bss` = `0x2f2bd8` = 3091416 bytes, confirmado por `readelf -S` e por `size -A`,
na Ã¡rvore de build e no pacote.


> **Este número foi medido, mas estava mal escrito.** O `debian/README`
> afirmava `3091496` — uma troca de dígitos — e esse README era distribuído
> dentro de `libtiffanydb-dev`. A tabela acima e o texto distribuído dizem agora
> o mesmo número. Corrigido na build 4; ver §8.3.
---

## 4. ABI pÃºblica

O objecto exporta **39 sÃ­mbolos**. Os dois headers instalados declaram **os
mesmos 39** â€” verificado por comparaÃ§Ã£o do `nm -D` sobre o `.so` extraÃ­do do
pacote contra os cabeÃ§alhos instalados, nÃ£o contra a Ã¡rvore de build.

- 32 funÃ§Ãµes declaradas com parÃªnteses;
- 4 funÃ§Ãµes `pgwire_*` em `pgwire_api.h`;
- 3 variÃ¡veis `extern long`: `sql_ultimos_passos`, `sql_ultimos_nos`,
  `sql_ultimo_prog`.

*Declarados e nÃ£o exportados: nenhum.*
*Exportados e nÃ£o declarados: nenhum.*

Lista completa dos 39:

```
pgwire_listen  pgwire_recv_n  pgwire_send_all  pgwire_serve_conn
sql_abrir  sql_au_cmp  sql_bit_fora_de_proposito  sql_bits_fora
sql_bits_fora_zera  sql_cel_tecto  sql_cels_fora  sql_cifra_para
sql_cols_de  sql_corpo28  sql_corpo28_n  sql_corpo_aureo  sql_corpo_cifra
sql_corpo_cristal  sql_cr_cmp  sql_esp_dist  sql_esp_prof  sql_executa
sql_fechar  sql_histograma  sql_lin_tecto  sql_optica_resumo
sql_ord_perdidos  sql_raizi  sql_restauros_falhados  sql_tx_abre
sql_tx_cheia  sql_tx_desfaz  sql_tx_escritas  sql_tx_fecha  sql_tx_fibra
sql_tx_ultra  sql_ultimo_prog  sql_ultimos_nos  sql_ultimos_passos
```

### `sql_abrir` e `sql_fechar` fazem parte da API

Confirmado nos dois sentidos, sobre o pacote extraÃ­do:

| SÃ­mbolo | Exportado pelo `.so` | Declarado no header instalado |
|---|---|---|
| `sql_abrir` | sim | sim (`sql_api.h`) |
| `sql_fechar` | sim | sim (`sql_api.h`) |

Assinaturas, tal como instaladas:

```c
int  sql_abrir(const char *base);
void sql_fechar(void);
```

ConvenÃ§Ã£o do produto: **1 = sucesso, 0 = falha**. NÃ£o Ã© errno. `sql_executa`
segue a mesma convenÃ§Ã£o. `sql_abrir` devolveu `1` em todas as aberturas de
sucesso medidas.

---

## 5. DependÃªncias

### 5.1 DependÃªncia ELF observada

`readelf -d` sobre o `.so` expedido:

```
NEEDED   Shared library: [libc.so.6]
SONAME   Library soname:  [libtiffanydb.so.1]
```

Uma Ãºnica `NEEDED`. Todos os sÃ­mbolos indefinidos resolvem para `libc.so.6`
(versÃµes `GLIBC_2.2.5` a `GLIBC_2.33`) mais os sÃ­mbolos de suporte do compilador
que o prÃ³prio `libc` fornece (`__stack_chk_fail`, `__gmon_start__`,
`_ITM_*`, `__cxa_finalize`).

### 5.2 AusÃªncia de `-lm`

**Confirmada.** Nenhum sÃ­mbolo de `libm` aparece na tabela de indefinidos, e nÃ£o
hÃ¡ `-lm` em `Libs:`. O `pkg-config` instalado Ã©:

```
prefix=/usr
exec_prefix=${prefix}
libdir=${prefix}/lib/x86_64-linux-gnu
includedir=${prefix}/include/tiffanydb

Name: tiffanydb
Description: Tiffany DB - motor SQL embebido (biblioteca C)
Version: 1.0.0
Libs: -L${libdir} -ltiffanydb
Cflags: -I${includedir}
```

NÃ£o hÃ¡ `-lm`, `-lpthread` nem `-ldl`.

### 5.3 `Depends` derivada, nÃ£o escrita Ã  mÃ£o

| Pacote | `Depends` | Como foi obtida |
|---|---|---|
| `libtiffanydb1` | `libc6 (>= 2.33)` | `dh_makeshlibs` â†’ `dh_shlibdeps` â†’ `shlibs:Depends` |
| `libtiffanydb-dev` | `libtiffanydb1 (= 1.0.0-1)` | `${binary:Version}` |

O `>= 2.33` vem da versÃ£o mais alta de sÃ­mbolo observada, `fstat@GLIBC_2.33`.
NinguÃ©m escreveu `libc6` Ã  mÃ£o; se um dia o objecto precisar de mais, a cadeia
acompanha sozinha.

O pacote `-dev` **nÃ£o** declara `${shlibs:Depends}`: nÃ£o contÃ©m biblioteca
partilhada real, sÃ³ a biblioteca estÃ¡tica e o symlink de link.

---

## 6. Comportamento observado e documentado

### 6.1 PersistÃªncia entre processos

Medido: `CREATE TABLE` e `INSERT` num processo; um **processo novo**, que nÃ£o
compartilha memÃ³ria com o primeiro, abre a mesma base e lÃª a linha inserida.

| Medida | Resultado |
|---|---|
| Processo 1 cria e insere | `DDL=1`, `INS=1`, `SELECT` devolve 1 linha |
| Processo novo, sÃ³ a ler | `SELECT` devolve a linha; `rc=0` |
| Isolamento entre bases | bases diferentes nÃ£o se misturam |

Ficheiros que aparecem no directÃ³rio de trabalho: `minha_base.mem`,
`minha_base__t.mem`, `minha_base.prog`, `minha_base.undo`, e `dados/sql_pool`,
`dados/sql_rel`. NÃ£o sÃ£o criados `conf` nem `_corpo`.

### 6.2 LigaÃ§Ã£o estÃ¡tica e partilhada

| Modo | Resultado |
|---|---|
| Partilhada (`pkg-config --libs`) | compilado, `ldd` mostra `libtiffanydb.so.1` |
| EstÃ¡tica (`pkg-config --libs --static`) | compilado contra a `.a` distribuÃ­da e executado com sucesso |

O `--static` funciona porque a `.a` **Ã©** distribuÃ­da. Sem ela, o `--static` seria
uma promessa sem conteÃºdo.

### 6.3 `pkg-config`

`pkg-config --modversion tiffanydb` â†’ `1.0.0`.
`pkg-config --libs --cflags tiffanydb` â†’ `-L/usr/lib/x86_64-linux-gnu -ltiffanydb -I/usr/include/tiffanydb`.
Instalado no pacote e resolvido sem variÃ¡vel de ambiente no contentor de teste
(que tinha o caminho multiarch jÃ¡ no `PKG_CONFIG_PATH` por omissÃ£o da distro).

### 6.4 Pontes bash / node / powershell â€” opcionais

A ponte estÃ¡ dentro da biblioteca e Ã© alcanÃ§Ã¡vel pela API pÃºblica:

```c
sql_executa("BASH MOVE ...")        /* chega a cmd_bash        */
sql_executa("NODE MOVE ...")        /* chega a cmd_node        */
sql_executa("POWERSHELL MOVE ...")  /* chega a cmd_powershell  */
```

Formato errado devolve a mensagem `BASH: use MOVE` com `rc = 0`.

**O pacote nÃ£o declara dependÃªncia de node, bash ou powershell.** Medido num
contentor Ubuntu 24.04 **sem** node e **sem** powershell: o pacote instala e a
biblioteca funciona. SÃ£o capacidades opcionais do produto, nÃ£o dependÃªncias do
pacote. `popen`, `pclose` e `_exit` aparecem na tabela de indefinidos do `.so`.

### 6.5 `stdout` Ã© comportamento documentado, nÃ£o defeito

O motor escreve no `stdout` do processo consumidor em caminhos internos, mesmo
quando chamado pela porta C com `SqlOut *`. Observado em `SELECT`, `CREATE`,
`INSERT`, `UPDATE`, `DELETE` e lote.

**NÃ£o existe forma na API pÃºblica V1 de silenciar ou redireccionar este
`stdout`.** O motor usa `printf`/`fprintf(stdout, â€¦)` directamente. Qualquer
silenciamento Ã© externo ao processo (shell, `dup2`).

Isto estÃ¡ documentado em `debian/README` e Ã© comportamento conhecido e aceite da
V1. Uma porta de saÃ­da controlÃ¡vel seria **nova API = mudanÃ§a de ABI**, fora do
Ã¢mbito desta V1.

---

## 7. Empacotamento Debian

### 7.1 Ficheiros

Nove ficheiros em `debian/`, todos com terminaÃ§Ã£o **LF**, nenhum com CRLF:

| Ficheiro | Bytes | Bytes > 127 |
|---|---:|---:|
| `changelog` | 723 | 0 |
| `control` | 1730 | 0 |
| `copyright` | 5417 | 4 |
| `libtiffanydb-dev.install` | 306 | 0 |
| `libtiffanydb1.install` | 97 | 0 |
| `README` | 4492 | 0 |
| `rules` | 11372 | 0 |
| `patches/series` | 0 | 0 |
| `source/format` | 11 | 0 |

Os 4 bytes acima de 127 em `copyright` sÃ£o o nome do autor em UTF-8. SÃ£o
intencionais.

### 7.2 `lintian`

**0 erros. 3 avisos.** Todos os trÃªs sÃ£o conhecidos e classificados:

| Pacote | Aviso | Leitura |
|---|---|---|
| `libtiffanydb1` | `distant-prerequisite-in-shlibs` | O `libtiffanydb.so.1` Ã© um `shlib` novo sem `debian/control` registado. Desaparece quando a biblioteca entrar num repositÃ³rio oficial. |
| `libtiffanydb1` | `initial-upload-closes-no-bugs` | Falta um `Closes:` no changelog. Ã‰ aviso de primeira publicaÃ§Ã£o. |
| `libtiffanydb-dev` | `initial-upload-closes-no-bugs` | Idem. |

Nenhum `no-copyright-file`: o `copyright` Ã© instalado Ã  mÃ£o nos dois pacotes.

### 7.3 DecisÃµes de empacotamento que afectam o binÃ¡rio

- **LTO removido Ã  mÃ£o.** O Ubuntu 24.04 injecta `-flto=auto -ffat-lto-objects`
  e nÃ£o tem interruptor de `dpkg-buildflags` para o tirar. A flag Ã© filtrada, e
  o build **falha de propÃ³sito** se `-flto` voltar aos flags, ou se algum objecto
  tiver secÃ§Ã£o `.gnu.lto_*`. Ã‰ um erro de build, nÃ£o um aviso.
- `-g0` e `--build-id=none`, para reprodutibilidade byte a byte.
- `ar` em modo determinÃ­stico (`rcsD`).
- Todo o hardening da distro fica ligado: fortify, stack protector, relro, now,
  cf-protection.
- Os symlinks sÃ£o criados **Ã  mÃ£o**, nome a nome: `dh_makeshlibs` lÃª os sÃ­mbolos
  mas nÃ£o cria symlinks.
- Nada entra por wildcard. A lista de ficheiros de cada pacote estÃ¡ escrita
  explÃ­cita em `debian/rules` e nos dois `.install`.

---

## 8. Reprodutibilidade

### 8.1 As quatro builds

| Build | Origem | `libtiffanydb1_â€¦deb` | `libtiffanydb-dev_â€¦deb` |
|---|---|---|---|
| 1 | H, mediÃ§Ã£o inicial | `3dd557f6â€¦` | `cd92a82câ€¦` |
| 2 | H, apÃ³s ajuste do `README` | `3dd557f6â€¦` | `69df4553â€¦` |
| 3 | I0, reconstruÃ§Ã£o independente | `3dd557f6â€¦` | `69df4553â€¦` |
| **4** | **I0 pÃ³s-higiene, apÃ³s correcÃ§Ã£o do `README`** | **`3dd557f6â€¦`** | **`aae0c655â€¦`** |

### 8.2 Onde os hashes sÃ£o e nÃ£o sÃ£o idÃªnticos

- **Runtime: idÃªntico nas quatro builds.** `3dd557f6a7f3d5671bf5514a689c36e77c89491b9fe3e7b39b7931e7eae72227`
- **Development: mudou na build 4** para `aae0c655â€¦`, porque `debian/README`
  entrou no pacote. Foi a Ãºnica alteraÃ§Ã£o em todo o produto.
- **Bibliotecas compiladas: idÃªnticas nas quatro builds**, tanto na Ã¡rvore como
  dentro dos pacotes extraÃ­dos:
  - Ã¡rvore: `.a` `89f395c6â€¦`, `.so.1.0.0` `bea1b9cdâ€¦`
  - dentro do `.deb`: `.a` `79c19df3â€¦` (584012 B), `.so.1.0.0` `e273beb2â€¦` (378408 B)

A build 1 produziu `cd92a82câ€¦` no pacote `-dev` porque o `debian/README` ainda
nÃ£o continha o texto final. A divergÃªncia era de conteÃºdo documental, nÃ£o de
compilaÃ§Ã£o. A build 4 voltou a mudar o `-dev` pelo mesmo motivo, pela mesma
razÃ£o, na mesma direcÃ§Ã£o.

**Reprodutibilidade validada nas builds realizadas.** NÃ£o Ã© uma prova absoluta:
foi medida em quatro execuÃ§Ãµes da mesma receita no mesmo host, nÃ£o em hosts
distintos nem em versÃµes distintas de `dpkg`/`binutils`.

### 8.3 Build 4 â€” motivo, alcance e prova de que o nÃºcleo nÃ£o se mexeu

```text
Build 4:
motivo = correÃ§Ã£o factual do README distribuÃ­do
nÃºcleo = inalterado
ABI = inalterada
SONAME = inalterado
```

**Motivo.** `debian/README:65` afirmava `.bss 3091496` bytes. A mediÃ§Ã£o congelada
em H, reconfirmada na build 3 e outra vez na build 4, Ã© **3091416** bytes
(`0x2f2bd8`) â€” obtida por `size -A` e por `readelf -S`, na Ã¡rvore de build e no
pacote extraÃ­do. A linha tinha uma troca de dÃ­gitos. Como o `README` entra em
`libtiffanydb-dev` como `README.gz`, o `.deb` distribuÃ­a informaÃ§Ã£o errada sobre
consumo de memÃ³ria.

**O que foi alterado.** Uma linha, um nÃºmero:

```
debian/README:65
- ... (medido no .deb: 3091496
+ ... (medido no .deb: 3091416
```

**Prova de que nada mais mudou.** TrÃªs mediÃ§Ãµes independentes na build 4:

| MediÃ§Ã£o | Resultado | Leitura |
|---|---|---|
| `.deb` runtime | `3dd557f6â€¦` â€” igual Ã  build 3 | o runtime nÃ£o contÃ©m o README; nada mudou nele |
| `.so.1.0.0` dentro do pacote | `e273beb2â€¦` â€” igual | o binÃ¡rio distribuÃ­do Ã© o mesmo |
| `.a` dentro do pacote | `79c19df3â€¦` â€” igual | a biblioteca estÃ¡tica Ã© a mesma |
| `README.gz` dentro do `-dev` | 1 ocorrÃªncia de `3091416`, **0** de `3091496` | a correcÃ§Ã£o chegou ao artefacto |
| `.bss` no `.so` distribuÃ­do | 3091416 | o nÃºmero agora bate com a documentaÃ§Ã£o |
| exports / declaraÃ§Ãµes | 39 / 39 | ABI inalterada |
| `SONAME` | `libtiffanydb.so.1` | inalterado |
| `Version` | `1.0.0-1` nos dois pacotes | inalterado |
| `NEEDED` | `libc.so.6` apenas | inalterado |

**SuperfÃ­cie de staging mais estreita, sem efeito no produto.** A build 4 nÃ£o
usou a Ã¡rvore larga da build 3. Usou uma Ã¡rvore derivada da closure real do
compilador (Â§10.4): 52 ficheiros em vez de 202. O runtime sairia byte-idÃªntico
mesmo que a Ã¡rvore fosse errada, e saiu â€” o que confirma que os 119 ficheiros de
`lib/` removidos do staging nÃ£o participavam da construÃ§Ã£o.

**VerificaÃ§Ãµes repetidas na build 4.** ConteÃºdo dos dois pacotes enumerado com
`dpkg-deb -c`; `lintian` com 0 erros e os mesmos 3 avisos; consumidor externo a
`CREATE`/`INSERT`/`SELECT` ligado Ã  partilhada; processo novo a fazer sÃ³ `SELECT`
a ler o que o processo anterior escreveu (persistÃªncia em disco); consumidor
estÃ¡tico ligado Ã  `.a` instalada, com `ldd` a mostrar dependÃªncia apenas de
`libc`; `apt-get remove --purge` a deixar o sistema limpo; `sql_abrir`/`sql_fechar`
presentes na API.

### 8.4 `DEB_BUILD_OPTIONS=nostrip` nÃ£o Ã© um no-op

Registo porque apareceu durante esta etapa e porque muda o produto silenciosamente.

A primeira execuÃ§Ã£o da build 4 foi feita com `DEB_BUILD_OPTIONS=nostrip`, com a
intenÃ§Ã£o de manter o `dh_strip` "explÃ­cito". O efeito Ã© o oposto do pretendido:
`nostrip` **desliga** o `dh_strip`. O resultado foi:

- `libtiffanydb1` crescia de 175424 B para 181492 B (+6068 B);
- a `.so.1.0.0` dentro do pacote passava a ser a versÃ£o nÃ£o stripada, e o
  `sha256` passava a coincidir com o da Ã¡rvore de build â€” o que *parecia* uma
  boa notÃ­cia e era o sintoma do defeito;
- o `lintian` subia de 0 erros para **1 erro**:
  `E: libtiffanydb1: unstripped-binary-or-object`.

A build foi refeita sem essa variÃ¡vel e voltou a 0 erros, com o runtime
byte-idÃªntico Ã  build 3. Registado porque o sintoma (hashes mais "bonitos" e
iguais aos da Ã¡rvore) Ã© enganador: sÃ³ o `lintian` o apanhou.

### 8.5 O `.gch` removido nunca entrou na construÃ§Ã£o

`lib/isa.h.gch` foi desversionado. Antes de o remover, mediu-se se ele
participava da compilaÃ§Ã£o. Duas provas independentes, ambas negativas:

1. **VersÃ£o do compilador.** Os primeiros 8 bytes do ficheiro sÃ£o
   `67 70 63 68 43 30 31 34` â€” `gpch` seguido de `C014`, que identifica um
   cabeÃ§alho prÃ©-compilado produzido pelo **GCC 14**. A build usa
   **GCC 13.3.0**. O GCC 13 rejeita um `.gch` de outra versÃ£o.
2. **Preprocessamento.** `cc -E` com as CPPFLAGS reais do build produz saÃ­da
   **byte-idÃªntica** com o `.gch` presente e com o `.gch` ausente
   (`cea03563d11ca9451d7b7ce38e6b3e87` nos dois casos).

`lib/isa.h` estÃ¡ na closure do produto, mas o `.gch` Ã© um artefacto compilado de
outra mÃ¡quina e outra versÃ£o de compilador. DesversionÃ¡-lo nÃ£o pode mudar o
binÃ¡rio â€” e a build 4 confirma-o empiricamente, com as bibliotecas a sair
byte-idÃªnticas.

`banco/apps/soma.erg.fita.tmp` (11 bytes, `01 01 00 01 02 00 03 02 03 00 00`)
tambÃ©m foi desversionado. Ã‰ um ficheiro temporÃ¡rio e nÃ£o binÃ¡rio de texto; nÃ£o
entra em nenhum `#include`, logo nunca teve efeito na construÃ§Ã£o.

---

## 9. Teste de repositÃ³rio APT local (descartÃ¡vel)

Executado em contentor limpo. **Nada foi publicado.** NÃ£o houve servidor web,
endpoint pÃºblico, chave GPG, `InRelease` nem `Release.gpg`. Nenhuma chave foi
gerada.

### 9.1 Metadados gerados

`apt-ftparchive` sobre `pool/`:

```
Suite:       tiffany
Codename:    tiffany
Architectures: amd64
Components:  main
Origin:      Tiffany DB
Label:       Tiffany DB (local disposable test)
Description: I0 local test - NOT FOR DISTRIBUTION
```

Ficheiros: `pool/main/libtiffanydb/*.deb`,
`dists/tiffany/main/binary-amd64/Packages{,.gz}`, `dists/tiffany/Release`.

### 9.2 Metadados contra artefactos

ConferÃªncia campo a campo entre `SHA256`/`Size` do `Packages` e os `.deb` reais:

| Pacote | `Size` | `SHA256` | Resultado |
|---|---:|---|---|
| `libtiffanydb-dev` | 202842 | `69df455381bfb18bf6726bc1af74601cb54114e27c5300d51ddcc703bb06d7b4` | coincide |
| `libtiffanydb1` | 175424 | `3dd557f6a7f3d5671bf5514a689c36e77c89491b9fe3e7b39b7931e7eae72227` | coincide |

O `apt-get download` no contentor devolveu ficheiros com os mesmos `SHA-256`.

### 9.3 Cliente

Fonte usada, com `trusted=yes` **porque nÃ£o existe assinatura**:

```
deb [trusted=yes] file:/repo tiffany main
```

| Passo | Resultado |
|---|---|
| `apt-get update` | ok |
| `apt-cache policy` | candidato `1.0.0-1` para ambos |
| `apt-get install libtiffanydb-dev` | ok, `libtiffanydb1` puxado por dependÃªncia |
| `dpkg-query` | `1.0.0-1 amd64 installed` para ambos |
| `pkg-config` | versÃ£o `1.0.0`, flags correctas |
| Consumidor ligado Ã  partilhada | compila e corre |
| Consumidor ligado Ã  estÃ¡tica | compila e corre |
| PersistÃªncia em processo novo | ok |
| `apt-get remove --purge` | ok, ambos removidos |

### 9.4 Limpeza

Contentor de teste removido. RepositÃ³rio APT local removido. Nenhum contentor
prÃ©-existente foi tocado â€” os 7 contentores de serviÃ§o continuaram a correr com
o mesmo estado.

---

## 10. Auditoria de Git, staging e segredos

### 10.1 Git

| VerificaÃ§Ã£o | Resultado |
|---|---|
| `git diff -- banco/` | vazio â€” **nenhum fonte de `banco/` alterado** |
| `git diff -- debian/` | vazio â€” `debian/` nÃ£o estÃ¡ registado; `git diff` nÃ£o mostra nÃ£o-rastreados |
| `git ls-files --stage debian/rules` | `100755 35652e51143e10af9a7d8ce80c3d462cf6dc5db3 0  debian/rules` |
| `.deb` versionado | nenhum |
| `.so` / `.a` / `.o` versionados | nenhum |
| BinÃ¡rios alguma vez adicionados em qualquer commit | nenhum |
| Bit executÃ¡vel de `debian/rules` | registado como `100755`, blob em LF puro (0 caracteres CR) |
| Ficheiros em `debian/` com CRLF | nenhum |
| `lib/isa.h.gch` versionado | **nÃ£o** â€” desversionado nesta etapa |
| `banco/apps/soma.erg.fita.tmp` versionado | **nÃ£o** â€” desversionado nesta etapa |

O bit executÃ¡vel foi registado com `git update-index --add --chmod=+x`.
**NÃ£o foi feito commit.**

### 10.2 Desversionamento dos dois ficheiros indevidos

Feito com `git rm --cached`, que **retira do Ã­ndice e preserva o ficheiro em
disco**. Nada foi apagado. Antes e depois, registado como pedido:

```
$ git ls-files --stage lib/isa.h.gch banco/apps/soma.erg.fita.tmp
100644 eba216629465c63db0efb70adf4435c010b98dd4 0	banco/apps/soma.erg.fita.tmp
100644 51c7a4995e347725c713cfd050db1fcdbc370640 0	lib/isa.h.gch

$ git ls-files --stage lib/isa.h.gch banco/apps/soma.erg.fita.tmp
(vazio)
```

E, depois:

```
$ git status --short
D  banco/apps/soma.erg.fita.tmp
D  lib/isa.h.gch
```

Para nÃ£o voltarem a aparecer como ruÃ­do, e para nÃ£o regressarem com um
`git add -A`, entraram no `.gitignore`:

```
/lib/*.gch
/banco/apps/*.tmp
```

Confirmado com `git check-ignore -v`. Os padrÃµes sÃ£o ancorados a `/lib/` e
`/banco/apps/`, por isso **nÃ£o** atingem os quatro ficheiros `.erg.fita.tmp` jÃ¡
versionados em `conecthus/backends/` â€” verificado, continuam versionados.
`.gitignore` ficou em LF e o diff Ã© de 8 linhas acrescentadas, 0 removidas.

Nenhum outro ficheiro de `lib/`, `banco/` ou `banco/apps/` foi tocado:
`git diff --stat -- lib banco banco/apps` estÃ¡ vazio.

### 10.3 Censo do staging

**Estes nÃºmeros sÃ£o da build 3.** A build 4 usou outro staging, derivado da
closure real do compilador (Â§10.5). Registados os dois.

204 ficheiros enviados para a build 3:

| Categoria | Quantidade |
|---|---:|
| Total | 204 |
| Empacotamento Debian (`debian/`) | 9 |
| CÃ³digo do produto (`banco/` 43 + `lib/` 150) | 193 |
| DocumentaÃ§Ã£o (raiz: `LICENSE`, `NOTICE`) | 2 |
| Testes | 1 |
| Artefactos binÃ¡rios | **2** â€” ver Â§11 |
| Ficheiros proibidos | **0** |

CorrecÃ§Ã£o ao censo original, que contava 0 binÃ¡rios: o filtro de extensÃ£o nÃ£o
incluÃ­a `.gch`. Existem exactamente dois ficheiros de conteÃºdo gerado dentro do
staging da build 3, nenhum deles de empacotamento â€” `lib/isa.h.gch` (2242240 B) e
`banco/apps/soma.erg.fita.tmp` (11 B). Ambos foram desversionados nesta etapa
(Â§10.2) e nenhum entrava em qualquer `.deb`, como `dpkg-deb -c` confirmou.

Proibidos verificados um a um: `.deb`, `.so`, `.a`, `.o`, chaves
(`id_ed25519`, `id_rsa`, `.pem`, `.key`, `.p12`), `.env`, `credential*`,
`segredo/`, `.pdf`, `.doc*`, `.xls*`, `.gif`, `.zip`, `.tar`, `.gz`, `.exe`, e
os prÃ³prios ficheiros de trabalho (`i0*.ps1`, `i0*.sh`, `stage.tar`, `prod.tar`).

### 10.4 Lista fechada: `LIB NECESSÃRIA` e `LIB NÃƒO NECESSÃRIA`

O objectivo Ã© nÃ£o levar para o produto ficheiros sÃ³ porque estÃ£o versionados.
Em vez de listar por extensÃ£o, a lista sai do prÃ³prio compilador:

```
cc -D_POSIX_C_SOURCE=200809L -DSQL_NO_MAIN -DPGWIRE_NO_MAIN \
   -MM banco/sql.c banco/pgwire.c -Ibanco -Ilib
```

`debian/rules` compila exactamente `banco/sql.c` e `banco/pgwire.c` com essas
mesmas flags, pelo que a closure transitiva do preprocessador **Ã©** o conjunto
de headers de que a biblioteca precisa. 40 ficheiros no total, dos quais:

**LIB NECESSÃRIA â€” 31 headers de 149 ficheiros versionados em `lib/`**

```
lib/banda.h      lib/binario.h    lib/caminho.h    lib/cifra.h
lib/contrato.h   lib/corpos.h     lib/disco.h      lib/dual16.h
lib/dual32.h     lib/exterior.h   lib/fatorial.h   lib/forma.h
lib/i128.h       lib/incidencia.h lib/inteiros.h   lib/isa.h
lib/largura.h    lib/linear.h     lib/palavra8.h   lib/pgmsg.h
lib/pgwire.h     lib/racionais.h  lib/reta.h       lib/serie.h
lib/simbolos.h   lib/slot_map.h   lib/slot_mem.h   lib/stratum.h
lib/umbit.h      lib/unidade.h    lib/word_isa.h
```

**BANCO NECESSÃRIO â€” 10 ficheiros**

```
banco/sql.c              (compilado)
banco/pgwire.c           (compilado)
banco/sql_api.h          (na closure; instalado em /usr/include/tiffanydb/)
banco/pgwire_api.h       (NÃƒO estÃ¡ na closure; instalado explicitamente)
banco/parse_ficheiro.h   banco/pgcat.h        banco/pgfunc.h
banco/pgwire_sess.h      banco/tiffany_node.h banco/tiffany_shell.h
```

`banco/pgwire_api.h` Ã© a razÃ£o de a lista de `banco/` nÃ£o ser sÃ³ a closure: Ã©
instalado pelo pacote mas incluÃ­do por ninguÃ©m. Teria sido omitido por um
`-MM` cego.

**LIB NÃƒO NECESSÃRIA â€” 118 ficheiros, todos permanecem no repositÃ³rio**

| ExtensÃ£o | Quantidade | Exemplo |
|---|---:|---|
| `.h` | 71 | `lib/algebra.h`, `lib/tiffany.h`, `lib/torre_alg.h` |
| `.otf` | 28 | `lib/fontes/d-bx1000.otf` |
| `.mjs` | 8 | `lib/arena_combinacao.mjs`, `lib/peano.js` |
| `.txt` | 7 | `lib/classe/corpus_frases_pt.txt` |
| `.c` | 2 | `lib/pread_posix.c`, `lib/so_cristal.c` |
| `.js` | 2 | `lib/peano.js`, `lib/universal.js` |

Os 71 headers dispensÃ¡veis estÃ£o listados um a um na saÃ­da da mediÃ§Ã£o; nenhum Ã©
alcanÃ§ado a partir de `sql.c` ou `pgwire.c`. Isto inclui fontes de domÃ­nio e
corpus (`corpus_frases_pt.txt`, `corpus_orbitas_pt.txt`), fontes tipogrÃ¡ficas
(`.otf`) e protÃ³tipos JavaScript que nada tem a ver com esta biblioteca.

### 10.5 Staging de release normalizado

```
banco/       10    (closure + o header pÃºblico instalado)
debian/       9
lib/         31    (closure do compilador)
LICENSE       1
NOTICE        1
TOTAL        52    (era 202 na build 3)
```

VerificaÃ§Ãµes sobre a Ã¡rvore jÃ¡ montada:

- zero `.gch`, `.otf`, `.js`, `.mjs`, `.txt`, `.tmp`, `.pdf`, `.png`;
- `banco/sql_api.h` e `banco/pgwire_api.h` presentes;
- os 10 ficheiros de `banco/` necessÃ¡rios existem todos;
- os 9 ficheiros de `debian/` em LF, zero CRLF;
- o `README` recebido contÃ©m `3091416` uma vez e `3091496` zero vezes.

O `debian/` foi enumerado do disco, nÃ£o de `git ls-files` â€” os nove ficheiros
ainda nÃ£o estÃ£o versionados, e `git ls-files debian` devolve sÃ³ `debian/rules`.
Uma lista de staging construÃ­da a partir do Ã­ndice teria enviado um pacote
incompleto e falhado.

**Prova de que o estreitamento Ã© seguro:** o `.deb` runtime saiu byte-idÃªntico ao
da build 3 (Â§8.3). Se os 119 ficheiros removidos do staging tivessem afectado a
compilaÃ§Ã£o, o hash teria mudado.

### 10.6 Segredos

Varrido objectivo, sem imprimir valores, em quatro Ã¢mbitos:

| Ã‚mbito | Ficheiros | OcorrÃªncias |
|---|---:|---:|
| Staging da build 3 (o que foi para a build) | 204 | **0** |
| Staging da build 4 (closure + `debian/`) | 52 | **0** |
| SuperfÃ­cie de release (`banco/`, `lib/`, `debian/`, `LICENSE`, `NOTICE`) | 213 | **0** |
| Rastreados pelo git (repositÃ³rio inteiro) | 2666 | 84 |

A varredura foi repetida sobre os **conteÃºdo final dos dois `.deb` da build 4**,
extraÃ­dos com `dpkg-deb --fsys-tarfile` e varridos com o conjunto completo de
padrÃµes (chaves em todos os formatos `BEGIN â€¦ PRIVATE KEY`, tokens
`ghp_`/`github_pat_`/`xox*`/`sk-`, JWTs `eyJâ€¦`, `AKIAâ€¦`, atribuiÃ§Ãµes
`PASSWORD=`/`SECRET=`/`TOKEN=`/`api_key`): **0 ocorrÃªncias em ambos**. Nenhum
nome de ficheiro suspeito dentro dos pacotes (`.pem`, `.key`, `.p12`, `.env`,
`.secret`, `.tmp`, `.gch`, `.otf`, `.js`, `.mjs`, `.pyc`, ficheiros ocultos).

PadrÃµes procurados: `id_ed25519`, `BEGIN OPENSSH PRIVATE KEY`,
`BEGIN PRIVATE KEY`, `BEGIN RSA PRIVATE KEY`, `password=`, `passwd=`, `secret=`,
`token=`, `api_key`, `AWS_SECRET`, `GITHUB_TOKEN`, e nomes com `segredo`,
`credential`, `.env`.

**Nenhum segredo no Ã¢mbito do I0.** As 84 ocorrÃªncias do repositÃ³rio largo estÃ£o
em `plataforma/` e `tools/`, em cÃ³digo nÃ£o relacionado com `libtiffanydb`: sÃ£o
nomes de variÃ¡veis de ambiente e identificadores (`api_key`, `token=`,
`secret=`), dois ficheiros de exemplo (`.env.exemplo`,
`plataforma/conecthus/tiffany-app/.env.example`) e quatro scripts de
infraestrutura. Nenhum deles embute material de chave privada â€” verificou-se
expressamente a ausÃªncia de `BEGIN â€¦ PRIVATE KEY` em cada um.

Estado do que Ã© sensÃ­vel:

| Item | Versionado? |
|---|---|
| `segredo/` (inclui `segredo/id_ed25519_patria`) | **nÃ£o** â€” `.gitignore:163` |
| `.env` | **nÃ£o** â€” `.gitignore:164` |
| `lib/isa.h.gch` | **nÃ£o** â€” desversionado nesta etapa, `.gitignore:285` |
| `banco/apps/soma.erg.fita.tmp` | **nÃ£o** â€” desversionado nesta etapa, `.gitignore:287` |
| `tools/segredo.sh`, `tools/chave-patria.sh`, `tools/experimentos-patria.sh`, `tools/publica_banco.sh` | sim, mas **sÃ³ referenciam** o caminho da chave; nenhuma chave embutida |

Nenhum caminho `/root/â€¦` do host de build, nenhuma referÃªncia Ã s credenciais da
PÃ¡tria e nenhum artefacto dos experimentos descartÃ¡veis estÃ¡ no conteÃºdo dos
pacotes. O `PROPOSTA_TIFFANY_DB.md` menciona `/root/â€¦` e o host em Â§22â€“Â§25, que
sÃ£o registos histÃ³ricos das mediÃ§Ãµes e nÃ£o conteÃºdo de pacote.

---

## 11. LimitaÃ§Ãµes conhecidas

Registadas como sÃ£o, sem qualified:

1. **Documento com um nÃºmero errado.** `debian/README` linha 65 escreve
   `3091496` bytes para `.bss`. O valor medido Ã© **3091416** bytes
   (`0x2f2bd8`), confirmado por `size -A` e `readelf -S` na Ã¡rvore de build e no
   pacote extraÃ­do. Ã‰ uma troca de dÃ­gitos. O texto errado estÃ¡ dentro de
   `libtiffanydb-dev` como `README.gz`. **NÃ£o foi corrigido**, porque corrigir
   altera o `SHA-256` do pacote e obriga a reconstruir â€” e as hashes deste
   manifesto sÃ£o as medidas e validadas em Â§8.1.
2. **SÃ³ `amd64`.** Nenhum `arm64` foi compilado nem medido. O empacotamento nÃ£o
   escreve o triplet Ã  mÃ£o e resolve o multiarch sozinho, mas isso **nÃ£o** foi
   testado noutra arquitectura.
3. **Reprodutibilidade de Ã¢mbito estreito.** TrÃªs execuÃ§Ãµes da mesma receita no
   mesmo host. NÃ£o houve build em mÃ¡quina limpa, nem em `dpkg`/`binutils`
   diferentes.
4. **`stdout` sem forma de o silenciar.** Ver Â§6.5. Comportamento aceite na V1.
5. **`pgwire_listen(0, &got)` pede a porta 5432**, nÃ£o uma porta efÃ©mera. Em
   mÃ¡quina com PostgreSQL Ã  escuta isso Ã© uma colisÃ£o. Documentado em
   `debian/README`.
6. **`sql_abrir` resolve caminhos em relaÃ§Ã£o ao directÃ³rio de trabalho do
   processo**, nÃ£o a `/usr` nem a `/var`. Documentado.
7. **Teste APT feito com `trusted=yes`, sem assinatura.** NÃ£o prova que um
   cliente com verificaÃ§Ã£o de assinatura aceitaria o repositÃ³rio â€” nenhuma chave
   foi gerada, por instruÃ§Ã£o.
8. **3 avisos de `lintian` por resolver** (Â§7.2). Dois resolvem-se com um
   `Closes:` no changelog; o terceiro com a entrada oficial.
9. **Avisos do compilador por silenciar em work future.** `-Wall -Wextra` mediu
   341 avisos de funÃ§Ãµes `static` nÃ£o usadas e 14 avisos de truncamento em
   `sql.c`. O empacotamento nÃ£o os cala: um `.deb` com `-Werror` nÃ£o Ã©
   reprodutÃ­vel enquanto o nÃºmero nÃ£o desce.
10. ~~**204 ficheiros no staging Ã© mais largo do que o produto.**~~
    **RESOLVIDO nesta etapa.** O `lib/isa.h.gch` (2242240 B, cabeÃ§alho
    prÃ©-compilado GCC, magic `gpchC014`) e o `banco/apps/soma.erg.fita.tmp`
    (11 B) foram desversionados com `git rm --cached`, sem apagar os ficheiros do
    disco (Â§10.2), e protegidos por `.gitignore`. Nenhum dos dois entrava em
    qualquer `.deb` â€” confirmado por `dpkg-deb -c`. Ficou provado que o `.gch`
    nunca participou da compilaÃ§Ã£o (Â§8.5).
11. ~~**Staging largo, com conteÃºdo que nÃ£o pertence Ã  biblioteca.**~~ **RESOLVIDO.**
    O staging da build 4 desce de 202 para 52 ficheiros, com a lista de `lib/`
    derivada da closure do compilador em vez de contada por extensÃ£o (Â§10.4,
    Â§10.5). A prova de que o estreitamento nÃ£o mexeu no produto Ã© o `.deb`
    runtime byte-idÃªntico ao da build 3.
12. **O nÃºmero errado no `README` distribuÃ­do.** **RESOLVIDO nesta etapa.** Era o
    ponto mais sÃ©rio dos trÃªs: o `.deb` afirmava `.bss 3091496` bytes quando o
    valor medido Ã© 3091416. Corrigido em `debian/README:65`, reconstruÃ­do, e o
    pacote de distribuiÃ§Ã£o passou a conter o nÃºmero certo (Â§8.3).
13. **Os 118 ficheiros de `lib/` fora da closure continuam versionados.** NÃ£o
    entram no produto, mas continuam no repositÃ³rio. A remoÃ§Ã£o Ã© decisÃ£o de quem
    o detÃ©m; o que estava ao alcance desta etapa â€” nÃ£o os deixar entrar no
    produto â€” estÃ¡ feito e medido.

---

## 12. Status de distribuiÃ§Ã£o

```text
STATUS DE DISTRIBUIÃ‡ÃƒO:
BLOQUEADO

MOTIVO:
Tiffany-License-2026-08 nÃ£o concede redistribuiÃ§Ã£o livre.
DistribuiÃ§Ã£o pÃºblica ou redistribuiÃ§Ã£o dos artefactos requer acordo escrito.
```

### O que a auditoria encontrou, e o que nÃ£o encontrou

Lido no texto, sem interpretaÃ§Ã£o para alÃ©m disso:

- A licenÃ§a Ã© **proprietÃ¡ria** e identifica-se como `Tiffany-License-2026-08`.
- A secÃ§Ã£o 3 concede a qualquer pessoa, **enquanto o repositÃ³rio permanecer
  pÃºblico**, autorizaÃ§Ã£o gratuita para obter cÃ³pia, ler, estudar, compilar e
  executar os medidores em `tests/`, produzir e divulgar os resultados, e citar
  trechos. Concede-se tambÃ©m Â«compilar e executar as partes do restante
  repositÃ³rio de que esses medidores dependam para compilar e executarÂ».
- A secÃ§Ã£o 4 diz que, fora do concedido nas secÃ§Ãµes 2 e 3, **nÃ£o** Ã© concedida
  licenÃ§a gratuita para uso, modificaÃ§Ã£o, adaptaÃ§Ã£o, **redistribuiÃ§Ã£o**,
  publicaÃ§Ã£o, incorporaÃ§Ã£o noutra obra, sublicenciamento ou exploraÃ§Ã£o
  comercial.
- A secÃ§Ã£o 5 tem duas vias. **Via A**, adesÃ£o ao `CONTRATO-TIPO` por PIX com
  legenda estruturada, serve para uso prÃ³prio â€” usar, copiar, modificar, compilar
  e integrar nos sistemas de quem adere â€” e a licenÃ§a diz expressamente que essa
  via **nÃ£o** concede redistribuir, sublicenciar, republicar ou vender.
  **Via B**, **acordo escrito bilateral** com o titular, Ã© a via indicada para o
  que a Via A nÃ£o concede, e a secÃ§Ã£o 5 diz que esse acordo Â«Ã© a Ãºnica fonte da
  autorizaÃ§Ã£oÂ».
- A secÃ§Ã£o 7 veda usar, copiar, distribuir, vender ou explorar na ausÃªncia de
  autorizaÃ§Ã£o vÃ¡lida.
- A secÃ§Ã£o 10 exige a preservaÃ§Ã£o dos avisos de copyright, autoria e origem.
- `debian/copyright` reproduz isto e acrescenta, no parÃ¡grafo *Packaging*, que
  publicar os pacotes binÃ¡rios resultantes junto de terceiros Â«requires a
  written agreement that does not yet existÂ».
- O bloco *OBRAS DE TERCEIROS* da licenÃ§a e o `NOTICE` dizem que **nenhum
  material de terceiros estÃ¡ incorporado** ao conteÃºdo versionado. A varredura
  (Â§10.3) nÃ£o encontrou nenhum, o que Ã© consistente com essa afirmaÃ§Ã£o â€” sem
  isto ser uma prova de ausÃªncia.

ConclusÃ£o que a auditoria sustenta: **construir, instalar e verificar estes
pacotes na mÃ¡quina que detÃ©m a licenÃ§a** estÃ¡ coberto pela concessÃ£o gratuita de
verificaÃ§Ã£o. **TornÃ¡-los disponÃ­veis a terceiros** cai na redistribuiÃ§Ã£o, que o
texto reserva Ã  Via B e que, segundo o prÃ³prio `debian/copyright`, ainda nÃ£o
existe.

Este documento nÃ£o Ã© parecer jurÃ­dico e nÃ£o acrescenta nada ao texto da licenÃ§a.
Diz o que a auditoria encontrou e onde estÃ¡ escrito.

### Alternativas de distribuiÃ§Ã£o â€” registadas, nÃ£o escolhidas

Nenhuma foi adoptada. Nenhuma pode ser adoptada sem a Via B.

| Alternativa | Nota |
|---|---|
| PPA (Launchpad) | Exige conta Ubuntu e recompressÃ£o; revocation complexa. |
| GitHub Pages / Releases | Distribui via GitHub; estÃ¡ dentro do mesmo problema de autorizaÃ§Ã£o. |
| S3 + CloudFront | Controlo fino de acesso, mas Ã© tornar o produto disponÃ­vel. |
| Servidor na rede local | DistribuiÃ§Ã£o para terceiros; mesmo regime da Via B. |

### Requisitos para desbloquear

1. Acordo escrito com o titular, ao abrigo da Via B, que defina alcance, prazo,
   territÃ³rio, modalidades de uso, nÃºmero de utilizadores, distribuiÃ§Ã£o e
   sublicenciamento.
2. Chave de assinatura do repositÃ³rio APT, gerada e gerida fora deste Ã¢mbito.
   Nenhuma foi criada aqui.
3. ReexecuÃ§Ã£o do `lintian` com a entrada oficial registada (os mesmos 3 avisos).
4. ~~DecisÃ£o sobre os dois ficheiros indevidos de Â§11.10.~~ **Feito:** foram
   desversionados (Â§10.2). O que resta, e nÃ£o Ã© bloqueante para I1, Ã© decidir
   se os 118 ficheiros de `lib/` fora da closure saem do repositÃ³rio (Â§11.13).

Nada disto foi feito, e nada disto serÃ¡ feito sem instruÃ§Ã£o explÃ­cita.

---

## 13. Fecho

Etapa I0 encerrada:

- biblioteca validada;
- empacotamento Debian validado;
- repositÃ³rio APT local, descartÃ¡vel, validado;
- reprodutibilidade validada nas builds realizadas;
- secrets auditados;
- Git e staging auditados;
- **redistribuiÃ§Ã£o pÃºblica bloqueada por licenÃ§a.**

Nenhum commit. Nenhum push. Nenhuma publicaÃ§Ã£o. Nenhuma chave GPG. Nenhum
repositÃ³rio APT pÃºblico.

### Fecho pÃ³s-higiene

Os trÃªs pontos que I0 deixou reportados foram tratados nesta etapa, e todos
mediu-se antes e depois:

```text
I0 pÃ³s-higiene: FECHADO

Artefatos: corrigidos
README: factual
Pacotes: reconstruÃ­dos
ABI: preservada
NÃºcleo: zero diff
Segredos: zero
APT pÃºblico: NÃƒO PUBLICADO
RedistribuiÃ§Ã£o: BLOQUEADA POR LICENÃ‡A
```

| Ponto | Estado | Prova |
|---|---|---|
| `debian/README:65` com `.bss` errado | corrigido | `README.gz` no `.deb` tem 1Ã— `3091416`, 0Ã— `3091496`; `.bss` medido 3091416 |
| `lib/isa.h.gch` versionado | desversionado | `git ls-files` vazio; provado que nunca entrou na build (Â§8.5) |
| `banco/apps/soma.erg.fita.tmp` versionado | desversionado | `git ls-files` vazio |
| staging largo (202 ficheiros) | 52 ficheiros | lista derivada da closure; runtime byte-idÃªntico |

NÃºcleo C: `git diff -- banco/` continua vazio. ABI: 39 exports, 39 declaraÃ§Ãµes,
`SONAME` e `Version` inalterados. Segredos: 0 no staging e 0 dentro dos dois
`.deb`. Sem commit, sem push, sem publicaÃ§Ã£o, sem GPG.

**Os hashes em Â§3.1 sÃ£o os da build 4.** Se alguÃ©m comparar um artefacto com um
hash anterior a esta etapa, o do `-dev` vai divergir â€” Ã© a correcÃ§Ã£o do README,
e Ã© o motivo de a build existir.
