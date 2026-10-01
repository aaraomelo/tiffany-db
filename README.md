# Tiffany DB

Repositório independente do produto **Tiffany DB** v1.0.0.

> **Obra com direitos reservados. Não é software livre.**
> Todo o conteúdo deste repositório é conteúdo protegido e rege-se pela
> [`LICENSE`](LICENSE) — identificador `Tiffany-License-2026-08`, uma licença de
> direito autoral. O endereço canónico da obra é
> <https://github.com/aaraomelo/tiffany>; cópias, espelhos e derivações noutros
> endereços não o substituem. Leia a `LICENSE` integralmente antes de qualquer
> uso. Este README não concede, implicitamente nem explicitamente, nenhuma
> autorização que a `LICENSE` não conceda.

## O que é o Tiffany DB

O Tiffany DB é um **motor SQL embebido escrito em C**. Empacota `banco/sql.c` e
`banco/pgwire.c` numa biblioteca C partilhada e estática, `libtiffanydb`, com
interface pública em C: abre uma base de dados, executa SQL e devolve linhas.

| | | Fonte |
|---|---|---|
| Versão | `1.0.0-1` | `debian/changelog` |
| SONAME | `libtiffanydb.so.1` | medido, build 4 |
| ABI | 39 símbolos exportados / 39 declarações | medido, build 4 |
| Dependências de linkage | `libc.so.6` (glibc `GLIBC_2.2.5`–`GLIBC_2.33`) | medido, build 4 |
| Pacotes | `libtiffanydb1` (runtime), `libtiffanydb-dev` (desenvolvimento) | `debian/control` |
| Ambiente de medição | Ubuntu 24.04.4 LTS `amd64`, `cc` 13.3.0, `debhelper-compat` 13 | `RELEASE_MANIFEST…` §2 |

As linhas marcadas *medido* são resultados de inspecção da build 4 nesse
ambiente. Não são propriedades garantidas noutro toolchain ou noutro host.

## Porque é que este repositório existe

O Tiffany DB nasceu dentro do monorepo/corpus **Tiffany**
(<https://github.com/aaraomelo/tiffany>), onde coexists com um conjunto muito
maior de material: testes, ferramentas, textos, figuras, utilitários e
experimentos.

Este repositório isola **apenas a closure do produto**, para que a biblioteca
possa ser **mantida e construída** como unidade independente, sem o peso nem a
ambiguidade do corpus completo. A distribuição fora das condições da `LICENSE`
exige a via prevista nela.

## Origem e baseline

Todo o conteúdo aqui presente foi extraído **verbatim** do monorepo Tiffany,
do commit:

```
bdf56b27d82162cd86d49f916fda4c8d80b02af8
"gov: governança/empacotamento I0 pós-build 4 (debian, manifesto, proposta, .gitignore, debian/rules)"
```

Nesse commit foi selada a governança e o empacotamento do I0 pós-build 4. O
histórico completo, a proposta e a evidência medida estão em:

- [`PROPOSTA_TIFFANY_DB.md`](PROPOSTA_TIFFANY_DB.md)
- [`RELEASE_MANIFEST_TIFFANY_DB_V1.md`](RELEASE_MANIFEST_TIFFANY_DB_V1.md)

Os ficheiros neste repositório são cópias byte-a-byte desse commit, não
re-escritas. O `README.md` é a única adição documental, e por isso não conta
para o total da closure.

## PRODUCT CLOSURE versus TIFFANY MONOREPO CONTEXT

Esta distinção é o objectivo principal do repositório.

### PRODUCT CLOSURE — o que está aqui

| Área | Ficheiros | Papel |
|---|---:|---|
| `banco/` | 10 | 2 translation units (`sql.c`, `pgwire.c`) e 8 headers |
| `lib/` | 31 | headers required transitivamente para compilar a biblioteca |
| `debian/` | 9 | receita de empacotamento dos dois pacotes |
| raiz | 4 | `LICENSE`, `NOTICE`, `PROPOSTA`, manifesto |

**Total: 54 ficheiros** de produto, a que acresce o `README.md` (55 em disco).

A closure foi medida, não presumida: `gcc -MM -Ibanco -Ilib banco/sql.c
banco/pgwire.c` produz 40 dependências — 2 `.c`, 7 headers em `banco/`, 31
headers em `lib/` — e **zero dependências fora deste conjunto**.

### TIFFANY MONOREPO CONTEXT — o que fica no `tiffany`

Não está aqui, e continua no repositório principal:

- **118 ficheiros de `lib/`** fora da closure (71 `.h`, 28 `.otf`, 8 `.mjs`,
  7 `.txt`, 2 `.js`, 2 `.c`). Continuam versionados no monorepo porque têm
  função no ecossistema maior — 70 dos 71 headers são referenciados por
  `tests/`, `tools/` ou `banco/conversa.c`.
- **`tests/`** (628 ficheiros) — os medidores que a secção 3 da `LICENSE`
  concede executar, e que vivem no monorepo.
- **`tools/`, `assets/`, `conecthus/`, `app/`, `corpus/`, `papers/`, `can/`,
  `memoria/`, `redes/`, `docs/`, `integration/`, `gifs/`** e o restante
  material do corpus.

**A closure do produto não representa todo o corpus Tiffany.** Para os textos,
as medições associadas e o contexto matemático, o repositório a consultar é
<https://github.com/aaraomelo/tiffany>.

### Nota sobre `banco/`

O directório `banco/` no monorepo tem 42 ficheiros, dos quais 10 estão aqui. Os
outros 32 ficam no monorepo: outros consumidores (`agentes.c`, `torres.c`,
`canal_patria.c`, `conversa.c`), scripts e serviço (`*.sh`, `*.service`,
`*.timer`), programas auxiliares (`erg.c`, `traduz.c`, `mmu.c`, `node.c`,
`bash.c`, `powershell.c`), dados de teste (`apps/*.erg`, `bai.sql`) e artefactos
de apoio. Nada disso é necessário para construir nem consumir a biblioteca.

## Build

O empacotamento é Debian. Não há Makefile de topo — a árvore não contém
Makefile nem CMake em lado nenhum — a receita é `debian/rules`, que compila
apenas as duas translation units e monta os dois pacotes com
`dpkg-buildpackage`.

Pré-requisitos: `debhelper`, `gcc`, `dpkg-dev`, `binutils` e as ferramentas
usuais de um sistema POSIX. Nenhum componente de terceiros é redistribuído
aqui — ver `NOTICE`.

```
dpkg-buildpackage -us -uc -b
```

A sequência produz:

- `libtiffanydb1_1.0.0-1_amd64.deb`
- `libtiffanydb-dev_1.0.0-1_amd64.deb`

Para reconstruir o ambiente a partir da closure completa deste repositório
basta esta receita.

## Os pacotes `.deb` não são versionados

Os `.deb` **não** estão neste repositório, por decisão: são artefactos de
build, reproduzíveis a partir da receita, e versionar binários num repositório
de fonte serve a verificação.

Os SHA-256 medidos na **build 4 (2026-09-30)**:

```
libtiffanydb1_1.0.0-1_amd64.deb      175424 B
  3dd557f6a7f3d5671bf5514a689c36e77c89491b9fe3e7b39b7931e7eae72227
libtiffanydb-dev_1.0.0-1_amd64.deb   202950 B
  aae0c6559ea9063f86c6ed6c709691a7722cbb95524ab9cd67e8216cad96ef79
```

O que estes hashes significam, exactamente (`RELEASE_MANIFEST…` §8.2):

- **Runtime `3dd557f6…` é byte-idêntico nas quatro builds.**
- **Development `aae0c655…` mudou na build 4** face à build 3
  (`69df4553…`, 202842 B). A causa foi documental: `debian/README` foi corrigido
  e passou a entrar no pacote. Foi a única alteração em todo o produto, e as
  bibliotecas compiladas ficaram idênticas na árvore e dentro dos pacotes.
- A reprodutibilidade foi **validada em quatro execuções da mesma receita no
  mesmo host**. Não é uma prova absoluta: não foi medida em hosts distintos nem
  em versões distintas de `dpkg`/`binutils`.

Quem quiser **verificar** constrói os pacotes e compara com estes hashes. A
distribuição dos pacotes fora disso exige a via prevista na `LICENSE`.

## Instalação e uso

O header público é `sql_api.h` e `pgwire_api.h`, instalados em
`/usr/include/tiffanydb/`. `debian/README` descreve a instalação e a
utilização com detalhe e acompanha o pacote `-dev`.

```c
#include <tiffanydb/sql_api.h>
```

Ao instalar, ligue com `-ltiffanydb`. O ficheiro de pkg-config
`tiffanydb.pc` é instalado pelo `-dev`.

Consulte `debian/README` para os detalhes de linkagem, os códigos de erro e o
uso da shell.

## Relação entre os dois repositórios

| | |
|---|---|
| `aaraomelo/tiffany` | Repositório principal / corpus. Endereço canónico da obra. |
| `aaraomelo/tiffany-db` | Este repositório. Closure independente do produto. |

Não existe relação de submódulo, e não é pretendida que exista. O produto é
extraído do corpus por cópia, com proveniência declarada, e não por
referência.

## Licença

[`LICENSE`](LICENSE) e [`NOTICE`](NOTICE) acompanham a obra e são
reproduzidos aqui verbatim. Nenhum aviso de copyright, autoria ou origem foi
removido, alterado ou resumido. O `debian/copyright` reproduz a mesma licença
como `LicenseRef-Tiffany-License-2026-08`.

O regime da obra é o da `LICENSE`, **independentemente da visibilidade deste
repositório**. Um repositório visível publicamente não constitui, por si só, um
canal de distribuição autorizado, nem converte esta obra em software livre.

Este README é uma descrição factual do produto e da sua proveniência. Não é uma
concessão, e não amplia, restringe ou substitui os termos da `LICENSE`.
Qualquer uso além do que a `LICENSE` concede exige a via prevista nela — Via A
(adesão ao contrato-tipo) ou Via B (acordo escrito bilateral).

Copyright © 2026 Aarão Melo Lopes.
