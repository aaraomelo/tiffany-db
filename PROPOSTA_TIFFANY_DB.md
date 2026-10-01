# PROPOSTA TIFFANY DB — produto autocontido, instalável, distribuível via APT (Ubuntu)

- **Estado**: proposta documental fechada. Nada foi implementado, movido, renomeado ou refactorado.
- **Data**: 2026-09-30
- **Fonte**: inventário factual da árvore `C:\Users\jssim\aarao\tiffany` (comandos, listagens, leituras e relatórios de exploração).
- **Regra da tarefa**: sem alteração de código, sem movimentos de ficheiros, sem renomes, sem criação da biblioteca, sem refactor preventivo. Ambigüidades registadas em §14, com alternativas, **sem escolha silenciosa**.
- **Convenção de marcação**: `[OBSERVADO]` = facto verificado no repo; `[PROPOSTA]` = desenho do produto; `[PENDENTE]` = decisão/verificação em aberto, registada sem voto.

---

## 0. Inventário factual

### 0.1 O que é o "Tiffany DB"

O motor de base de dados do projecto vive hoje embebido no repositório tiffany em duas zonas:

- `banco/` — motor SQL em C + programas-harness (demos), script de geração `sql.py`, serviços e shell scripts.
- `lib/` — biblioteca de headers compartilhadas C; o subconjunto de armazenamento/wire é o núcleo do motor (`[OBSERVADO]`).

A "base de dados" é composta por quatro formas de armazenamento `[OBSERVADO]` (nomes deduzidos dos artefactos e do código SQL):

| Formato | Ficheiro(s) | Papel |
|---|---|---|
| ficheiro de base | `<base>.mem` | índice/dados da base |
| tabela | `<base>__<nome>.mem` | uma tabela por ficheiro |
| bytecode | `<base>.prog` | programa/tranche compilada |
| blobs | `<base>_corpo/` | conteúdo por coluna/corpo |

### 0.2 Motor SQL — `banco/sql.c`

- `[OBSERVADO]` ≈ 18.538 linhas; SQL compilado para ISA bytecode (arquitetura própria, `lib/isa.h`, `lib/isa_disk.h`, `lib/word_isa.h`, `lib/pgm.h`); "memória = disco" (persistência directa em `.mem`).
- `[OBSERVADO]` quando compilado como unidade: `SQL_NO_MAIN` nas linhas 16726–18538 — serve para linkar como biblioteca sem `main`.
- `[OBSERVADO]` tipos/corpos: 8 corpos (INTEIRO, RACIONAL com 6 famílias de magnitude, BOOLEANO, TEXTO, DATA). **UUID ausente** (JSON → TEXTO; RLS = isolamento SELECT-only; FKs CASCADE/SET NULL/RESTRICT + ON UPDATE CASCADE; UNIQUE via constraint; índice composto usa só a 1.ª coluna) — capacidades registadas nos contratos da raiz.
- `[OBSERVADO]` transacções: diário de undo idempotente (marcador de registo tolerante a replay).
- `[OBSERVADO]` acompanhante: `banco/sql.py` (gerador; ligado ao script node "gera:nucleo" do `package.json`).

### 0.3 Wire PostgreSQL — `banco/pgwire.c`

- `[OBSERVADO]` implementa o protocolo wire PostgreSQL v3 (FEBE), Simple + Extended query.
- `[OBSERVADO]` porta base 5432 → 55432 (compatível com o padrão de porta alternativa adiantada); `PGWIRE_NO_MAIN` nas linhas 145–191 (linkável sem `main`).
- `[OBSERVADO]` API própria: `pgwire_send_all`, `pgwire_recv_n`, `pgwire_listen`, `pgwire_serve_conn` (% confirmado na leitura de interfaces; verificação integral de assinaturas: `[PENDENTE]`).
- `[PENDENTE]` autenticação/SSL do pgwire não confirmada no inventário — a registar antes de prometer na API pública.

### 0.4 Programas-harness em `banco/`

`[OBSERVADO]` ficheiros-fonte C na raiz de `banco/`: `banco.c`, `erg.c`, `conversa.c`, `traduz.c`, `fita.c`, `pinos.c`, `agentes.c`, `mmu.c`, `node.c`, `liquida.c`, `barramento.c`, `torres.c`, `canal_patria.c`, `fala.c`, `bash.c`, `powershell.c` + `parse_ficheiro.h`.

`[OBSERVADO]` **`banco/apps/` não é código**: contém apenas dados ERG (`conta.erg`, `dual.erg`, `gato.erg`, `soma.erg`, `soma.erg.fita.tmp`) — resolve a ambigüidade anterior; não há um subdirectório de fontes C.

`[OBSERVADO]` serviços/scripts: `Dockerfile.volume`, `grava_saber.sh`, `memoria_banco.sh`, `prova.sh`, `run_sigma_test.sh`, `torre.sh`, `vigilia.sh`, `traz_grok.sh`, `torre-vigilia.service`, `torre-vigilia.timer` (systemd).

### 0.5 `lib/` relevantes do núcleo

`[OBSERVADO]` subset de armazenamento/wire: `banco.h`, `slot_mem.h`, `slot_map.h`, `disco.h`, `pgmsg.h`, `pgwire.h`, `isa.h`, `isa_disk.h`, `word_isa.h`, `pgm.h`, `corpo.h`, `corpos.h`, `corpos_fmt.h`, `global.h`, `contrato.h`, `tiffany.h`, `no_pgwire_stub.h`, `stratum.h`, `stratum_stub.h`. Ficheiros C em `lib/`: `pread_posix.c`, `so_cristal.c` `[PENDENTE]` papel a confirmar.

O resto de `lib/` (122 entradas) é matemática/domínio (alg, edos, física, quântica…) — **fora** do produto; só entra o que o `cc -MM` de cada fonte do núcleo alcançar (§1).

### 0.6 CI existente — `.github/workflows/`

`[OBSERVADO]` (glob omite dot-dirs; listado por verificação directa):

| Workflow | Gátilho | Papel | Relevância |
|---|---|---|---|
| `bateria.yml` | `workflow_dispatch` (só, deliberado) | corre a bateria atestada em `ubuntu-latest` | **§9 núcleo de regressão** |
| `publica.yml` | push + dispatch | publação do site (Vite/WASM/scp) | fora do produto DB |
| `publica-local.yml` | — | variante local | fora |
| `acesso.yml` | dispatch | gestão de chaves do servidor | fora (infra do repo) |
| `erp.yml`, `conecthus.yml` | — | infra ERP/Conecthus | fora do produto DB |

Fatos-chave do `bateria.yml` `[OBSERVADO]`:
- A bateria corre **no runner POSIX**, não na máquina do autor — incidente documentado (26/08, Windows/MinGW reescreveu `tools/atestados.txt` com falhas por falta de `sys/mman.h` e de node no PATH; revertido).
- Ferramentas instaladas no runner: `gcc libc6-dev python3 fonts-liberation fonts-dejavu-core`, node 22, garantia de `cc`.
- Não commita atestados: sobe como artefacto e mostra o diff; adoptar é decisão humana.
- Guarda anti-verde-vazio: exige a linha `^total [0-9]+ :` no relatório.

### 0.7 Testes / bateria

- `[OBSERVADO]` `tests/` tem 630 ficheiros (458 `.c`, 155 `.js`, 13 `.exe`, 3 `.txt`, 1 `.py`).
- `[OBSERVADO]` a bateria (`tools/bateria.sh`) descobre **medidores** por grep em `teoria.tex`, `catalogo.tex`, `enredo.tex` (+ `conecthus/backends/manifesto.json`): 340 + 44 = 369 medidores. **Nota crítica**: descobrir por grep nos `.tex` acopla a bateria ao corpo literário — isso tem de ser cortado ou parametrizado para o produto (§9, §14).
- `[OBSERVADO]` `tools/atestados.txt` é um **ledger por assinatura de conteúdo** (fonte, DEPENDE-DE, headers via `cc -MM`, argumentos; o compilador não entra na assinatura) — 560 linhas, **201 obsoletas**; não é um relatório de bateria (§0.8).
- `[OBSERVADO]` compilação modelo da bateria pgwire (verificação antropológica do agent): `cc -O2 -std=c99 -w -I. -I../tools -I../lib -I../banco -I../tests -DSQL_NO_MAIN -DPGWIRE_NO_MAIN tests/pgwire.c banco/sql.c banco/pgwire.c -lm`.
- `[OBSERVADO]` medidores `fita/pinos/agentes` declar**DEPENDE-DE** numa fixture `origem` **ausente** do repo → falham em máquina limpa sem a fixture (a registar no produto).

### 0.8 Documentos-âncora existentes (não gerados por esta proposta; já na raiz)

`[OBSERVADO]` `AUDITORIA_DISCREPANCIA_VALIDACAO_BANCO.md` (atestados.txt ≠ relatório; discrepância classificada B — baterias/estados diferentes), `PROTOCOLO_AUDITORIA_PRE_MIGRACAO_BANCO.md`, `CONTRATOS_ESQUEMA_BANCO_1..6`, `CONTRATO_VERTICAL_Tenant_Product_Order_Payment_Cash_v2.md`, `mapa_integracao_jev_banco.md`, `DIAGNOSTICO_BUILD_VALIDACAO.md`, `RESULTADO_VALIDACAO_VERTICAL.md`, `VALIDACAO_CAPACIDADES_VERTICAL.md`. `package.json` nome do pacote `tiffany-mvp` (privado), scripts `mvp`, `mvp:bench`, `mvp:test`, `mvp:metal`, `gera:nucleo`, `serve`. `LICENSE` e `NOTICE` existem na raiz.

### 0.9 O que NÃO entra no produto (decisão corroborada, pode confirmar-se)

`[OBSERVADO]` artefactos de autor/composição na raiz: `.tex/.pdf/.aux/.log`, `corpus/`, `cristal/`, `dados/`, `medidor/`, `memoria/`, `papers/`, `partitura/`, `posts/`, `app/` (site Vite), `can/`, `gifs/`, `segredo/`, `psi/`, `redes/`, `curriculo/`, `conecthus/` — incl. `*.exe` compilados avulsos (`erg_new.exe`, etc.). Estes não entram na biblioteca nem no pacote.

---

## 1. Mapa de dependências (observado e exigido)

Grafo `[OBSERVADO]` (deduzido do relatório de exploração; validar até ao `cc -MM` na H do plano):

```
banco/sql.c
 ├─ lib/slot_mem.h, lib/banco.h, lib/disco.h, lib/eda/isa*.h, lib/pgm.h, lib/corpo*.h, lib/global.h…
 ├─ <sys/mman.h>, <unistd.h>, <fcntl.h>   (POSIX; falha com MinGW — incidente 26/08)
 ├─ banco/sql_api.h, banco/sql.py (gerador; precisa Python 3 só em build-time)
 └─ banco/pgwire.c → lib/pgmsg.h, lib/pgwire.h, lib/no_pgwire_stub.h
tests/pgwire.c (medidor) → banco/sql.c + banco/pgwire.c (-DSQL_NO_MAIN -DPGWIRE_NO_MAIN)
```

- **Runtime do motor**: libc POSIX apenas. `[PENDENTE]` `pread_posix.c`/`so_cristal.c` compilam-se em Linux puro? (confirmar com `cc -MM`).
- **Build do motor**: `cc` (gcc), C99, `-lm`.
- **Build da bateria/corrida**: gcc, python3, node 22, `fonts-liberation` + `fonts-dejavu-core` (medidores de glifos), runner POSIX.
- **Não-dependências** `[OBSERVADO]`: nenhuma dependência Node/Python/Java/PostgreSQL no próprio motor; o projecto consome um **PostgreSQL real** só nas camadas Prisma/Postgres (`plataforma/tiffany-erp`, `plataforma/conecthus/tiffany-api`) que estão **fora** do produto DB.

---

## 2. Fronteira do produto

Proposta de fronteira (alt.2 recomendada; alt.1 e alt.3 registadas em §14):

- **Núcleo (entra)** — biblioteca C estática/partilhada:
  - `banco/sql.c`, `banco/pgwire.c`, headers `banco/sql_api.h`, `banco/pgwire_api.h`, `banco/pgwire_sess.h`, `banco/pgcat.h`, `banco/pgfunc.h`, `parse_ficheiro.h`;
  - subset `lib/`: as headers alcançadas por `cc -MM` (`slot_mem.h`, `banco.h`, `disco.h`, `pgmsg.h`, `pgwire.h`, `isa.h`, `isa_disk.h`, `word_isa.h`, `pgm.h`, `corpo*.h`, `global.h`, `contrato.h`, `tiffany.h`, `no_pgwire_stub.h`, `stratum*.h`, `pread_posix.c`, `so_cristal.c` sobre verificação);
  - **não** entram os harnesses dependentes de rede/fixtures ausentes (ex.: `traz_grok.sh`, `torre.sh`, `fita/pinos/agentes` com fixture `origem` `[PENDENTE]` — ou entram a título de exemplos, ver §14).
- **Testes (entra como suíte de regressão)**: `tests/*.c` (modelo `tests/pgwire.c`), um subconjunto de medidores não-acoplado ao corpo literário.
- **Fora (consumidores/target)**: `plataforma/tiffany-erp`, `plataforma/conecthus` (Prisma/Postgres); todo o corpo literário e artefactos §0.9.

A fronteira é **invisível** nesta fase: o produto nasce como documentação + plano; a separação física fica para a migração (§10) sem mover/renomear nada hoje.

---

## 3. Autocontenção (perguntas do produto, respostas propostas)

| Pergunta | Resposta de desenho |
|---|---|
| Precisa de Node? | Motor: não. Bateria/corrida: sim (medidores `.js` da 3B/jevs e outros). Pacote `-dev`: não. |
| Precisa de Python? | Só build-time (gerador `sql.py`). Runtime: não. |
| Precisa de Java? | Não. |
| Precisa de PostgreSQL servidor? | Não. O motor emula o wire PG; destina-se a ligações a partir de drivers PG. |
| Precisa de WASM/compilador Web? | Tooling WASM existe (`tools/*.wasm*.mjs`, `lib/wasm_erg.mjs`) mas fica **fora** do produto v1 (`[PENDENTE]` se se quer em v2). |
| Bibliotecedora/vendor? | `[PENDENTE]`: vendora Unicode/fsync/corpos auxiliares; a decidir com o utilizador. |
| Sistema-alvo | Ubuntu (APT). Portabilidade posix: preservada; a bateria corre onde o repo diz (CI), nunca na máquina do autor Windows `[OBSERVADO]`. |

---

## 4. Linguagem e ABI

- `[PROPOSTA]` C99, compilado com `-O2 -std=c99`.
- `[OBSERVADO]` símbolos de biblioteca já existentes no código: `sql_abrir`, `sql_fechar`, `sql_executa`, `sql_cols_de`, `sql_tx_*`; `pgwire_send_all`, `pgwire_recv_n`, `pgwire_listen`, `pgwire_serve_conn`; guardas `SQL_NO_MAIN`/`PGWIRE_NO_MAIN` já separam biblioteca de executável.
- `[PROPOSTA]` dois ficheiros de artefacto: `libtiffanydb` (shared) e `libtiffanydb-dev` (headers) (§7). A ABI versão segue `semver` sobre os símbolos §5 (§11).

---

## 5. API pública (derivada do código existente)

1. **Ligadura SQL** (já observada): `sql_abrir`, `sql_fechar`, `sql_executa`, `sql_cols_de`, `sql_tx_*` (begin/commit/rollback com diário undo).
2. **Wire PG**: `pgwire_send_all/recv_n/listen/serve_conn` (FEBE v3, Simple+Extended).
3. **Inicialização**: abrir base (`<base>.mem`/`<base>.prog`), caminho por tabela `<base>__<nome>.mem`, blobs em `<base>_corpo/` — formatos constituem a API de compatibilidade de disco (§11).
4. **Não prometer** (verificar antes): autenticação SSL do pgwire `[PENDENTE]`.

Contrato de compatibilidade: os símbolos §5.1–5.2 e os formatos §0.1 são as superfícies a preservar.

---

## 6. CLI

`[OBSERVADO]` não existe hoje um binário único "banco": existem harnesses (`banco.c`, `erg.c`, `node.c`, `conversa.c`…) e scripts (`memoria_banco.sh`, `prova.sh`). `[PROPOSTA]` o pacote oferece um executável `tiffany-db` com subcomandos:

```
tiffany-db serve [--port 55432] <base>      # wire PG (pgwire_listen/serve_conn)
tiffany-db exec <base> [--sql '...'|-]     # SQL unário, saída texto/tabular
tiffany-db info <base>                      # corpo, tabelas, estado pgm
tiffany-db check <base>                     # verificação diário/undo + formato
```
`[PENDENTE]` aceitar/suportar também um subcomando inspirado em `erg` (composição de tranches) como ferramenta de exemplo.

---

## 7. Ubuntu / Debian / APT

- `[PROPOSTA]` árvore de empacotamento (Debian) com 3 pacotes: `tiffany-db` (executável+service), `libtiffanydb` (shared), `libtiffanydb-dev` (headers + `.pc` pkg-config).
- `[PROPOSTA]` meta de instalação: `sudo apt install tiffany-db` (ou `libtiffanydb-dev` para ligar).
- `[PROPOSTA]` convenção: só se entrega na versão-candidata Ubuntu LTS; o repo do projecto alimenta um APT local (ou PPAlocal) primeiro.
- `[OBSERVADO]` não há hoje ficheiros `.deb`/`control`/`pkg-config`/Cmake/Makefile no repo — o packaging é criação nova, não herda nada (a confirmar se há versões anteriores esquecidas `[PENDENTE]`).
- `[PENDENTE]` política de assinatura (chave de pacote) e hosting (LAN proprio vs PPAlocal vs GitHub Reena) — decisão com o utilizador.

---

## 8. Build

- Alvo do núcleo (biblioteca): partir do comando já observado e comprovar com `cc -MM`:
  `cc -O2 -std=c99 -fPIC -Ilib -Ibanco -c banco/sql.c -o sql.o` + `... banco/pgwire.c …`; link em `libtiffanydb.so` e `libtiffanydb.a`.
- Executável `tiffany-db`: link núcleo + `main` leve (novo, não existe `[PROPOSTA]`).
- `[PROPOSTA]` `make` mínimo (sem Cmake novo): alvos `all`, `lib`, `bin`, `test`, `bateria`, `deb`, `clean`.
- `[OBSERVADO]` o gerador `sql.py` roda em build-time (Python 3) — entra no alvo `gera:nucleo`.

---

## 9. Testes e regressão

- Núcleo de regressão = bateria atestada (assinatura por conteúdo, `cc -MM`, sem o compilador na assinatura) **no runner POSIX via CI** `[OBSERVADO]`; nunca na máquina do autor.
- Problema de acoplamento a resolver no plano (sem tocar agora): a descoberta dos medidores por grep nos `.tex` deve ser **parametrizável** para o produto (anúncio de lista própria), senão a suíte herda o corpo literário. `[PENDENTE]`; a bateria actual fica intacta.
- `[OBSERVADO]` fixture `origem` ausente p/ medidores fita/pinos/agentes; `[PENDENTE]` se entra e como se invoca no produto (a alternativa é excluir desses testes, sem os mudar).
- Regressão APT: após empacotar, re-correr os mesmos medidores contra a biblioteca instalada (sanity smoke test do pacote).

---

## 10. Migração (plano em fases, sem mover nada hoje)

1. **Fase 0 (documental)** — esta proposta + inventário fechado (ficar).
2. **Fase 1 (referenciar)** — criar estrutura `debian/` + scripts de build que referenciam os ficheiros actuais **sem mover/copiar**: `fonte` = mesma árvore.
3. **Fase 2 (estático de teste)** — produzir o `.deb` por construção-link (não por cópia de fonte) e correr smoke.
4. **Fase 3 (separação discricionária, SÓ após decisão do utilizador)** — propor cópia simbólica/limpa de `banco/`+subset `lib/` para uma localização canónica, mantendo a actual árvore como upstream; decisão explícita em §14.
5. **Fase 4** — APT público quando a validação vertical dos contratos 1–6 e a regressão ficarem verdes num único comando.

---

## 11. Versionamento e compatibilidade

- `[PROPOSTA]` semver 1.0.0 no primeiro packaging; **3 superfícies independentes**: (a) ABI C (símbolos §5.1–5.2), (b) formato de disco/peristência (§0.1), (c) wire PG v3.
- `[PENDENTE]` fichário `soname` (nome simbólico da .so), política de transições APT, e se a versão do formato de disco entra em `libtiffanydb` ou separada — decisão.
- `[OBSERVADO]` `LICENSE`/`NOTICE` existem na raiz — o pacote herda as mesmas licenças (identificadores: `[PENDENTE]` confirmar SPDX nos ficheiros).

---

## 12. Segurança e operabilidade

- `[OBSERVADO]` RLS é isolamento SELECT-only; FKs com CASCADE; UUID ausente; JSON→TEXT — capacidades que **não se prometem** como Postgres completo (destacar em docs do pacote para não criar expectativa de paridade PG).
- `[OBSERVADO]` porta padrão 55432 (não roda em 5432 de um PG real no mesmo host sem conflito — documento de operação).
- `[PENDENTE]` autenticação no wire; política de ligações (bind localhost? TLS?) a definir antes do primeiro serviço exposto (§5.4).
- `[PENDENTE]` actualização de formato de disco (migração de esquema) — o diário undo/estado é crítico; smoke de backup/restauro a planejar.
- Segredos: `[OBSERVADO]` a árvore tem `segredo/` e `.env`/chaves referidas — **nenhum segredo entra** no pacote nem é impresso; a árvore de packaging só pega fontes.

---

## 13. Proposta final (A–N)

- **A — Estado actual**: motor C embebido em `banco/`+subset `lib/`, wire PG v3, formatos `.mem/.prog/_corpo`, bateria POSIX atestada no CI, sem packaging Debian, sem binário único, fixtures parciais ausentes.
- **B — Dependências**: runtime só libc/glibc POSIX (`-lm`); build `cc`+python3; teste node22+python+gcc+fontes; **necunha dependência Java/PG servidor/WASM** no motor.
- **C — Fronteira**: núcleo `sql.c+pgwire.c+lib{storage,wire}`; harnesses externos; `plataforma/*` = consumidores fora; corpo literário fora.
- **D — Arquitectura**: biblioteca C + executável `tiffany-db` separado; separação `SQL_NO_MAIN`/`PGWIRE_NO_MAIN` já existe; memória=disco inalterado.
- **E — API pública**: `sql_abrir/fechar/executa/cols_de/tx_*`, `pgwire_{send_all,recv_n,listen,serve_conn}`.
- **F — CLI**: `tiffany-db serve [--port] / exec / info / check`.
- **G — Estrutura do repositório**: raiz `banco/`, `lib/`, `tests/`, `tools/`, novo `debian/` + `Makefile` (fase 1 referencial, sem mover).
- **H — Build**: `cc -O2 -std=c99 -fPIC -Ilib -Ibanco` → `.so`/`.a`; `make {lib,bin,test,bateria,deb}`.
- **I — Ubuntu/Debian/APT**: `tiffany-db`, `libtiffanydb`, `libtiffanydb-dev` (+ `.pc`); repo/PPAlocal; LTS.
- **J — Migração**: fases documental→referencial→build-link→separação (só após decisão)→APT.
- **K — Compatibilidade**: **cinco superfícies independentes**: (1) versão do produto, (2) versão da API, (3) ABI/SONAME da `.so`, (4) formato de disco, (5) protocolo wire. SemVer versiona **produto/API**; a ABI tem política própria de **compatibilidade de símbolos + SONAME** (ex.: `libtiffanydb.so.1` com nome soname estável, quebrada apenas por remoção/renome de símbolo público); o formato de disco e o wire têm a sua própria lei. Ver §17 «N1» (correcção registada).
- **L — Riscos**: (1) bateria acoplada ao grep dos `.tex`; (2) fixture `origem` ausente; (3) prometer paridade PG sem UUID/RLS parcial/JSON-to-TEXT → crise de expectativa; (4) runner Windows corrompe `atestados.txt`; (5) descoberta de formato de disco sem migrador.
- **M — Questões abertas**: ver §14.
- **N — Primeira implementação**: ver §15.

---

## 14. Questões abertas e ambigüidades (registadas; escolha com o utilizador)

1. **Fronteira de harnesses**: `erg.c/node.c/conversa.c/fita.c/pinos.c/agentes.c…` entram como exemplos no pacote, ou ficam só na suíte? (Alt.1 = só núcleo; Alt.2 = + exemplos; Alt.3 = só CLI `tiffany-db`.)
2. **Fixture `origem`** (fita/pinos/agentes): repor fixture vs excluir esses medidores do pacote vs excluir agora e documentar.
3. **Descoberta de medidores**: manter grep nos `.tex` (acoplado) vs lista parametrizável própria do produto (recomendado) — **sem alterar hoje**.
4. **Autenticação do pgwire**: N1 resolve o **boundary factual** — `pgwire_serve_conn` consome `Startup` **trust** (sem password), responde **N** a SSL, bind em **127.0.0.1** com fallback 5432→55432. Resta a **decisão** de expor com TLS/password (fora do code actual).
5. **Vendor vs system** de dependências auxiliares.
6. **Hosting APT**: PPAlocal, repo local LAN, GitHub Release; e assinatura de chave.
7. **`so_cristal.c`/`pread_posix.c`**: N1 **resolve pelo grafo** — ver §17: `so_cristal.c` = demo autónoma (compila-sozinha, referenciada só em comentários); `pread_posix.c` = shim Windows (consumidores `erg.c`, `slot_mem.h`, `banco.h`; **não** é preciso em Ubuntu/glibc). Ambas **fora** da biblioteca.
8. **Licença SPDX** do pacote (o repo tem LICENSE/NOTICE).
9. **`soname`/estratégia de transição** (existe consumer ativo em produção? — ver `plataforma/`).
10. `banco/apps/*.erg` (`soma.erg`…) — dados de demonstração; entram como exemplos?

## 15. Primeira implementação (passos, sem mover nada)

1. **Exec** `cc -MM` nos fontes do núcleo → fechar o grafo de dependências real (§1) e resolver as Q4/Q7 e a inclusão de `pread_posix.c`/`so_cristal.c`.
2. **Confirmar** assinaturas exactas de `sql_*` e `pgwire_*` (subir o §5 a contrato) e a porta/autenticação pgwire.
3. **Criar** `debian/` + `Makefile` referencial (build-link sobre a árvore actual) + `tiffany-db` CLI (novo ficheiro, não toca nos existentes).
4. **Empacotar** os 3 artefactos `.deb` e correr smoke local.
5. **Regredir** no CI (workflow novo, dispatch, POSIX) sobre os mesmos medidores + smoke do pacote instalado.
6. **Confiar** no contrato vertical 1–6 para a mensagem de paridade (sem prometer PG completo).

## 17. N1 — fechamento factual do núcleo (2026-09-30)

Execução de N1 conforme pedido. **Zero alterações de código**, ficheiros, renomes, `debian/`, `Makefile`, CLI. Todos os itens abaixo são `[OBSERVADO]` (medição directa), excepto onde marcado.

### 17.1 `cc -MM` real (transitivo, gcc, `cwd = tests`, sinais da bateria)

**`sql.o` ← `banco/sql.c`** (headers próprios + lib + banco):
- **lib/**: `disco.h`, `slot_mem.h`, `palavra8.h`, `umbit.h`, `binario.h`, `largura.h`, `word_isa.h`, `slot_map.h`, `reta.h`, `inteiros.h`, `simbolos.h`, `incidencia.h`, `racionais.h`, `dual16.h`, `dual32.h`, `i128.h`, `serie.h`, `fatorial.h`, `linear.h`, `cifra.h`, `forma.h`, `exterior.h`, `banda.h`, `stratum.h`, `caminho.h`, `corpos.h`, `unidade.h`, `contrato.h`, `pgmsg.h`, `isa.h`.
- **banco/**: `sql_api.h`, `pgcat.h`, `pgfunc.h`, `tiffany_node.h`, `tiffany_shell.h`, `parse_ficheiro.h`.
- **system** (não listados por `-MM`): `stdio stdlib string stdint inttypes unistd time fcntl sys/socket netinet/in arpa/inet sys/time ctype strings sys/wait sys/stat dirent`.

**`pgwire.o` ← `banco/pgwire.c`**: `pgwire.h` (lib), `pgmsg.h` (lib), `sql_api.h`, `pgwire_sess.h` + system `stdio stdlib string errno unistd fcntl signal sys/socket netinet/in arpa/inet`.

**Nota de host**: `cc` não existe na máquina do autor; `gcc` (MinGW WinLibs POSIX UCRT) compila `-MM` OK (grafo POSIX intocado) **mas não compila** os objectos por incompatibilidades Windows/winsock (ver 17.4). O grafo acima é idêntico ao que `cc -MM` daria no runner POSIX (a própria bateria usa estas sinais).

### 17.2 `pread_posix.c` e `so_cristal.c` (pelo grafo, não pelo nome)

- **`lib/pread_posix.c`**: shim **Windows** de `pread/pwrite`; `banco.h` (L87-88) e `slot_mem.h` (L22-23) declaram `extern ssize_t pread/pwrite`. Consumidores: `erg.c`, `slot_mem.h`, `banco.h`, `expr.h`. Em **Ubuntu/glibc** `pread/pwrite` vêm do `unistd.h`/`libc` — o shim **não** entra no link Linux. `[PENDENTE]` apenas se houver produto Windows futuro.
- **`lib/so_cristal.c`**: demo **autónoma** («sistema operacional em analógico», compila `cc -O2 -I. so_cristal.c -lm`); referenciada **só em comentários** de `can/micro.c`. Zero relação com o grafo de `sql.c`/`pgwire.c`. **Fora** da biblioteca.
- **Conclusão**: nenhum dos dois faz parte de `libtiffanydb` em Ubuntu.

### 17.3 Assinaturas reais da API (fonte: `sql_api.h`, `pgwire_api.h`, corpos)

| Símbolo | Assinatura C exacta | Header | Público? | Estado/contexto | Ownership/memória | Retorno/erro |
|---|---|---|---|---|---|---|
| `sql_abrir` | `int sql_abrir(const char *base)` | sql_api.h | público | disco global uma vez (pools `dados/sql_pool.bin`, `sql_rel.bin`); chama `abrir_base`; fixa a **base corrente** global | base global `fmem/fprog`; não reentrante | 1=ok, 0=falha (via `abrir_base`) |
| `sql_fechar` | `void sql_fechar(void)` | sql_api.h | público | `fechar_base(); sql_cap=NULL` | — | — |
| `sql_executa` | `int sql_executa(const char *sql, SqlOut *out)` | sql_api.h | público | catálogo pgcat responde 1.º; se `out!=NULL` captura; linha única e lotes (`sql_executa_lote`) | `out` preenchido (células ≤64×64); `sql_cap` é **static** | 0=falha (ou `out->ok`); `err[160]`, `tag`, `ncols/nrows`, `col`, `tipo`, `cell`, `nulo` |
| `sql_cols_de` | `int sql_cols_de(const char *tabela, char nomes[][32], int cap)` | sql_api.h | público | troca de tabela com restauro (`g_tabela`) | preenche `nomes` | `lidas` ≥0; 0 se tabela inválida |
| `sql_histograma` | `int sql_histograma(const char *tabela, const char *coluna, long *hist, int n, long *fora)` | sql_api.h | público | restauro da tabela anterior | preenche `hist` | nº de dentro; −1 erro; `fora` conta fora do domínio |
| `sql_tx_abre/fecha` | `void sql_tx_abre(void)` / `void sql_tx_fecha(void)` | sql_api.h | público | diário undo global | — | — |
| `sql_tx_desfaz` | `int sql_tx_desfaz(void)` | sql_api.h | público | pilha undo | — | 0 se a pilha encheu (não desfaz) |
| `sql_tx_cheia/escritas` | `int sql_tx_cheia(void)` / `long sql_tx_escritas(void)` | sql_api.h | público | undo | — | estado da pilha |
| `sql_tx_ultra` | `void sql_tx_ultra(long*,long*,long*,long*)` | sql_api.h | público | 4 saídas ultramétrica | — | — |
| `sql_tx_fibra` | `void sql_tx_fibra(long*,long*,long*,long*)` | sql_api.h | público | histograma G | — | — |
| `sql_corpo28_n/corpo28` | `int sql_corpo28_n(void)` / `const char *sql_corpo28(int i, long *B, long *C)` | sql_api.h | público | catálogo dos corpos | ptr em catálogo | nome/parâmetros |
| `sql_optica_resumo` | `void sql_optica_resumo(long*,long*,long*,long*,long*,long*,long*)` | sql_api.h | público | resumo do catálogo | 7 saídas | — |
| `sql_bits_fora` | `long sql_bits_fora(void)` | sql_api.h | público | contador global de bits | — | contagem (medidor exige 0) |
| `sql_bits_fora_zera` | `void sql_bits_fora_zera(void)` | sql_api.h | público | — | — | — |
| `sql_bit_fora_de_proposito` | `int sql_bit_fora_de_proposito(long i)` | sql_api.h | público | bit | — | 0/1 |
| `sql_restauros_falhados` | `long sql_restauros_falhados(void)` | sql_api.h | público | contador global | — | contagem |
| `sql_ord_perdidos` | `long sql_ord_perdidos(void)` | sql_api.h | público | contador global | — | contagem |
| `sql_cifra_para` | `int sql_cifra_para(long B, long C)` | sql_api.h | público | cifra.h | — | 0/1 |
| `sql_raizi` | `long sql_raizi(long n)` | sql_api.h | público | cifra.h | — | raiz inteira |
| `sql_lin_tecto/cel_tecto` | `long sql_lin_tecto(void)` / `long sql_cel_tecto(void)` | sql_api.h | público | limites do mapa de slots | — | tecto |
| `sql_cels_fora` | `long sql_cels_fora(void)` | sql_api.h | público | contador | — | contagem |
| `sql_esp_dist/prof` | `long sql_esp_dist(long,long)` / `int sql_esp_prof(long,long)` | sql_api.h | público | espaço espectral | — | —
| `sql_corpo_aureo/cristal` | `int sql_corpo_aureo(void)` / `int sql_corpo_cristal(void)` | sql_api.h | público | — | — | bool |
| `sql_au_cmp/cr_cmp` | `int sql_au_cmp(long,long,long,long,long)` / `int sql_cr_cmp(long,long,long,long,long)` | sql_api.h | público | comparadores dos corpos | — | −/0/+ |
| `sql_ultimos_passos/nos/prog` | `extern long sql_ultimos_passos; extern long sql_ultimos_nos; extern long sql_ultimo_prog;` | sql_api.h | público (dados) | variáveis **globais mutáveis** (medidas da última execução) | **não** reentrantes; ler após chamada | —
| `pgwire_send_all` | `int pgwire_send_all(int fd, const void *buf, int n)` | pgwire_api.h | público | fd aberto | buf não retido | 0 ok; −1 I/O |
| `pgwire_recv_n` | `int pgwire_recv_n(int fd, void *buf, int n)` | pgwire_api.h | público | fd aberto | preenche buf | 0 ok; −1 I/O |
| `pgwire_listen` | `int pgwire_listen(int want, int *got)` | pgwire_api.h | público | bind 127.0.0.1; want≤0 ⇒ 5432 senão 55432 | fd próprio | listen-fd; −1 falhou |
| `pgwire_serve_conn` | `int pgwire_serve_conn(int cfd, int32_t pid, int32_t key)` | pgwire_api.h | público | sessão local (`PgSess` no stack); usa `sql_executa`; Startup **trust**, SSL→N | sessão por conexão | 0=Terminate; −1 erro fatal; bloqueia |

**Internos relevantes**: `sql_executa_1`, `sql_executa_lote`, `uniao_ha`, `acha_relativo`, `ponte_node_mjs`, `shell_popen_run`, `shell_corre` (static em `sql.c`); `pg_buf_*`, `pg_put_*`, `pg_reply_*`, `pg_peek_head8`, `pg_parse_startup`, `pg_sess_fe` (static em `lib/pgwire.h`/`pgmsg.h`/`pgwire_sess.h`).

### 17.4 `SQL_NO_MAIN` / `PGWIRE_NO_MAIN`

- `SQL_NO_MAIN` (sql.c L16726–18538): guarda **apenas `main()`** — o único símbolo `col0` nesse intervalo é `int main(...)`. Nenhuma função/global auxiliar vive dentro da guarda.
- `PGWIRE_NO_MAIN` (pgwire.c L145–191): guarda **apenas `main()`**.
- **Estado global** (não reentrante, uma base por processo): `fmem/fprog` (base corrente), `g_tabela` (L3057), `sql_cap` (static, L180), `disco_ok`, diário undo, contadores `bits_fora/cels_fora/restauros/ord_perdidos`, `sql_ultimos_*`.
- **Limpeza da compilação como biblioteca**: **limpa** por guarda, com ressalvas factuais:
  - superfície de símbolos `sql.c`: **468 linhas `static`**; os únicos externs não-`sql_*` observados são **0** (todo o interno é static) — só a face `sql_*` (32 funções) + 3 globals de dados. Em `pgwire.c`: exactamente as 4 funções da API + `main` guardado.
  - **símbolos inesperados para o consumidor**: `pgwire.c`/`sql.c` emitem `printf/fprintf(stdout)` no motor (497 linhas em `sql.c` fora do `main`), incluindo as pontes `popen` (ver 17.6). A biblioteca **escreve no stdout** em caminhos internos — tem de ser documentado ou silenciado (decisão de produto).
  - **colisões sql↔pgwire**: nehuma — espaços de nomes disjuntos; os headers partilhados (`pgmsg.h`, `pgwire.h`, `pgcat.h`, `tiffany_shell.h`, `parse_ficheiro.h`) são 100% `static` (link interno), sem lução externo.

### 17.5 Link mínimo (conceptual)

```
sql.o   (banco/sql.c)         pgwire.o   (banco/pgwire.c)
   │                                │
   └──────────────┬─────────────────┘
                  ▼
     libtiffanydb.{so,a}
      «-lm» (não observado necessário: nenhuma função libm chamada
            nos headers transitivos nem no sql.c; apenas libc/glibc
            — confirmar com nm no runner POSIX; a bateria usa -lm)
```
- Bibliotecas: **libc** (implicita). `-lm` observado na bateria mas **não** referenciado pelo grafo; confirmar com `nm` no runner POSIX. Sem `-pthread`, `-lrt`, `-ldl`, sem `-lws2_32` (isso é Windows).
- **Não há terceiros**: nenhum ficheiro `.c` extra a linkar; `pread_posix.c`/`so_cristal.c` fora (§17.2).
- Fontes `.c` transitivos: **nenhum** (grafo de headers apenas).

### 17.6 Runtime e fronteira (correção à proposta)

- O motor `sql.c` contém **pontes de shell** (`ponte_node_mjs` L14425, `shell_popen_run` L14592, verbos `MOVE`) que invocam `node`/`bash`/`powershell` por `popen` quando certas instruções SQL as usam. **Isto é motor, não harness** (corrige a §2). Para o produto: (a) documentar como funcionalidade opcional que **exige** o binário externo presente no runtime; (b) ou gating por flag de compilação — decisão de produto (§18).
- Testes/harness exigem: node 22, python3, gcc, fontes (bateria). O motor em si: só libc.

### 17.7 `DECISÕES QUE AGORA PODEM SER TOMADAS`

**Factualmente resolvida** (o código decide; sem voto):
1. **Q7** — `so_cristal.c` e `pread_posix.c` **ficam fora** da biblioteca (resp. demo autónoma; shim Windows).
2. **Q4 (facto)** — pgwire é **trust**, SSL→N, bind em 127.0.0.1, fallback 5432→55432; API `pgwire_*` ≠ autenticada. A pergunta remanescente é expor TLS/password = **decisão de produto**.
3. **Surface de ABI** — a biblioteca exporta só `sql_*` (32) + 3 dados + `pgwire_*` (4); internos 100% `static`. A ABI pública é **exactamente** os dois headers.
4. **Link** — só libc; `-lm` improvável (confirmar nm no POSIX); sem transitivity `.c`.
5. **Guardas** — `SQL_NO_MAIN`/`PGWIRE_NO_MAIN` removem só `main`; compilação de biblioteca limpa.
6. **POSIX-only** — o núcleo não compila em MinGW (mkdir/fsync/fork/send/recv/SOCKET); o repositório declara-se POSIX e a validação é CI/ubuntu (coerente com `bateria.yml`).

**Decisão de produto** (precisam do teu voto; 2–3 alternativas concretas):
- **D1 — Pontes shell (node/bash/pwsh no SQL)**: (a) manter e documentar "runtime opcional node/bash/pwsh" — zero mudança de código; (b) gate por macro (ex.: `TIFFANY_DB_NO_SHELL`) — pequena alteração, muda ABI? não, só comportamiento condicional; (c) manter só node, remover bash/pwsh — restringe o produto.
- **D2 — stdio do motor**: (a) documentar como diagnóstico actual; (b) flag global de silêncio (novo símbolo? não, variável estática + setter novo = **altera ABI**); (c) reencaminhar via `SqlOut` — alteração interna sem mudar ABI pública.
- **D3 — pgwire no produto**: (a) serviço loopback tal como está (0 segurança além de trust); (b) adicionar password/TLS (mudança de código, fora do N1); (c) só biblioteca, serviço CLI fica adiado.
- **D4 — Autenticação de ligação**: ver D3.
- **D5 — Fixture `origem`** (Q2 da proposta): (a) repor fixture no produto; (b) excluir os medidores fita/pinos/agentes do pacote; (c) manter fora e documentar.
- **D6 — Versões do produto**: ver K (§11) — semver para produto/API; SONAME próprio (ex.: `libtiffanydb.so.1`), disco e wire com regras próprias. A escolha do esquema SONAME depende de existir consumer ativo (Q9).

**Ainda precisa de experimento** (não se fecha só a ler):
1. **`nm` no runner POSIX/CI** — confirmar a lista definitiva de símbolos indefinidos de `sql.o`/`pgwire.o` e a necessidade ou não de `-lm`.
2. **Build real no ubuntu** — confirmar que `cc -O2 -std=c99 -fPIC -Ilib -Ibanco` compila e linka `.so`/`.a` (o N1 só mediu no MinGW, que não é o alvo).
3. **Smoke de `libtiffanydb.so` num programa externo** (link + execução de `sql_abrir/executa` numa base real).
4. **Verificação de formato de disco** — abrir uma base real criada hoje num build POSIX e byte-a-byte antes/depois (migrador v1 depende disso).

### 18. Resultado esperado desta etapa (N1) — resumo

```
Tiffany DB
├── fontes necessárias ......... banco/sql.c, banco/pgwire.c (nenhum .c transitivo)
├── headers necessários ........ §17.1 (29 lib + 6 banco + sistema)
├── dependências de link ....... libc (glibc); -lm provável não; sem -pthread/-lrt/-ldl
├── API pública real ........... sql_api.h (32 funções + 3 globals) + pgwire_api.h (4)
├── API interna ................ 100% static nos headers (pgmsg/pgwire/pgcat/tiffany_shell/parse_ficheiro) e em sql.c
├── runtime .................... libc POSIX; opcional node/bash/pwsh para verbos MOVE/bridges; sem servidor PG, sem Java, sem WASM
└── testes ..................... medidores POSIX (bateria, CI), testes/pgwire.c (SQL_NO_MAIN+PGWIRE_NO_MAIN); fixtures fita/pinos/agentes pendentes
```

A execução de `debian/`/`Makefile`/CLI fica **adiada** até às decisões D1–D6.

## 19. Fontes do inventário

Listagens de `banco/`, `banco/apps/`, `lib/`; leitura de `.github/workflows/bateria.yml`, `publica.yml`, `acesso.yml`; head `sql.c`; `package.json`; relatórios de exploração do grafo de dependências (`banco`→`lib`) e de `tests/`/bateria (630 ficheiros, 369 medidores, `cc` de `tests/pgwire.c`, atestados 560 linhas/201 obsoletas); documentos-âncora da raiz (§0.8). Medições N1: `gcc -MM` real (grafo transitivo), regressões de guardas, contagens `static`/col0, leitura directa de `sql_api.h`/`pgwire_api.h`/`pgwire.c`/corpos. Lemas `[PENDENTE]` indicam o que falta confirmar antes de qualquer implementação.

## 20. Decisões de produto D1–D6 (registadas 2026-09-30)

### D1 — pontes shell: manter o código, capacidade explicitamente opcional/gateável (sem refactor agora)

Meta: **o núcleo funciona sem Node/bash/PowerShell**; as capacidades auxiliares só se ativam quando disponíveis/configuradas. Matriz de capacidades (despacho em `sql.c`: `executa` L15799→15875):

| Vérbio SQL | Caminho de funções | Ponte | Binário externo | Ficheiros/dados externos | Ausência do binário hoje |
|---|---|---|---|---|---|
| `BASH <script> MOVE …` | `cmd_bash`→`cmd_shell`→`shell_escreve`/`shell_corre`/`shell_le` | `popen` | `bash` (`tiffany_bash_bin`, env `TIFFANY_BASH`) | workspace `<base>_bash/` (`in`, `out`) | `popen` falha; `sql_cap->err` preenchido |
| `POWERSHELL … MOVE …` | `cmd_powershell`→`cmd_shell` | `popen` | `powershell` (`tiffany_pwsh_bin`) | `<base>_powershell/` + `in.ps1` | idem |
| `NODE … MOVE …` | `cmd_node`→`cmd_shell` | `popen` | `node` (`tiffany_node_bin`, env `TIFFANY_NODE`) | `<base>_node/` + `in.js` | idem |
| `MANIFESTO PARA U` | `cmd_manifesto_para_u`→`ponte_manifesto_u`→`ponte_node_mjs` | `popen(node)` | `node` | `tools/manifesto_u.mjs`, `conecthus/backends/manifesto.json` (via `acha_relativo`/CWD) | idem |
| `U PARA MANIFESTO` | `cmd_u_para_manifesto`→`ponte_manifesto_u`(modo `para-m`) | `popen(node)` | `node` | `tools/manifesto_u.mjs` | idem |
| `PAGINA PARA U` / `U PARA PAGINA` | `cmd_pagina_para_u` / `cmd_u_para_pagina`→`ponte_node_mjs`(modos `para-u`/`para-p`) | `popen(node)` | `node` | `tools/pagina_u.mjs`, `app/banco/pagina.{html,css,js}` | idem |
| `IMPORT MANIFESTO` | `import_manifesto` | **sem shell** | — | cópia `conecthus/backends/manifesto.json` → disco | falha com mensagem (não chama shell) |

Notas factuais:
- Legacy: `bash_*` (L14719–14722) mantidos "para grep/tests" — reencaminham para o mesmo `cmd_shell`.
- O motor também usa `st_*` (**stratum**, 6 usos em sql.c; `lib/stratum.h` inclui sockets) — **outra** capacidade de rede, distinta das pontes shell; entra no mesmo desenho de gating (§14 Q nova).
- Gate futuro proposto (sem alterar agora): macro `TIFFANY_DB_SEM_SHELL`-like com despacho neutro; hoje a ausência do binário já vira `popen` falho + `sql_cap->err` (comportamento definido, não silencioso).

### D2 — stdout: documentar o comportamento actual (sem nova API/ABI agora)

Medição (`sql.c` linhas 1..16725, `printf`/`fprintf`):

| Categoria | Ocorrências | Exemplo/verbo | Relevância p/ consumidor |
|---|---|---|---|
| Diagnóstico de erro/recusa | 226 | `printf("erro: …")`, `printf("nao …")` | média — sai no stdout, não no `SqlOut` |
| Status indentado do motor | 114 | `      IMPORT MANIFESTO: …`, `MOVE -1 …` | alta nas pontes/imports |
| Medição/selo (medidores) | 70 | `=== …`, `ok(`, `unid` | baixa (via corpus do medidor) |
| `fprintf(stderr, …)` | 8 | erros de processo | baixa |
| Dentro das pontes (14405–14723) | 24 | cópia do stdout do filho | **alta** — `ponte_node_mjs` reencaminha stdout do node para o stdout do processo |

Conclusão: a biblioteca **escreve no stdout** em caminhos internos; documentar no manual do pacote. `SqlOut` não muda; evolução futura (saída controlável) será **nova API = mudança de ABI**, a separar (§D6).

### D3 — pgwire: protocolo na lib, serviço v1 só loopback

- Bind padrão **127.0.0.1**; porta **55432** (fallback 5432→55432 já implementado). **Sem bind público.**
- `trust` aceitável no primeiro modo local; documentar explicitamente **pgwire local ≠ servidor PostgreSQL exposto à rede**.
- Não implementar TLS/password agora. Antes de qualquer exposição externa, abrir etapa própria: autenticação, autorização, TLS, política de bind, limites de conexão, timeouts.

### D5 — suíte v1: excluir `fita/pinos/agentes`

- Excluir da suíte v1 os medidores que dependem da fixture `origem` ausente; registar **não executado ≠ falhou**; manter a dependência documentada para reintegração futura. **Não fabricar fixture artificial** para gerar verde.

### D6 — versionamento (aprovado)

```
produto/API      → SemVer
ABI              → SONAME próprio  (libtiffanydb.so.1)
formato de disco → versão independente
PostgreSQL wire  → protocolo independente
```
As quatro superfícies não se misturam (§11 atualizado).

## 21. E1–E3 (pendente de ambiente POSIX) — comandos prontos

**Blocker verificado 2026-09-30**: host é Windows; `wsl` não instalado; sem docker/podman; sem alvo ssh configurado; `gcc` MinGW faz `-MM` mas não produz `.so`/`ldd`/`file` de Ubuntu. A bateria do projecto já declara que a validação POSIX vive no runner (CI).

Comandos prontos para o runner `ubuntu-latest`:

```bash
# E1 — nm
cc -O2 -std=c99 -w -I. -I../tools -I../lib -I../banco -I../tests -c ../banco/sql.c -o /tmp/sql.o   # cwd: tests
cc -O2 -std=c99 -w -I. -I../tools -I../lib -I../banco -I../tests -c ../banco/pgwire.c -o /tmp/pgwire.o
nm -u /tmp/sql.o; nm -u /tmp/pgwire.o; nm -D --defined-only /tmp/sql.o | rg 'sql_'
# E2 — build mínimo
cc -O2 -std=c99 -w -fPIC -I. -I../lib -I../banco -c ../banco/sql.c -o sql.o
cc -O2 -std=c99 -w -fPIC -I. -I../lib -I../banco -c ../banco/pgwire.c -o pgwire.o
ar rcs libtiffanydb.a sql.o pgwire.o
cc -shared -o libtiffanydb.so.1.0.0 -Wl,-soname,libtiffanydb.so.1 sql.o pgwire.o
ln -sf libtiffanydb.so.1.0.0 libtiffanydb.so.1; ln -sf libtiffanydb.so.1 libtiffanydb.so
nm -D --defined-only libtiffanydb.so; ldd libtiffanydb.so; file libtiffanydb.a libtiffanydb.so.1.0.0
# E3 — consumidor externo mínimo (dir fora do repo)
#   tmp.c: #include "sql_api.h" ; sql_abrir(base) ; sql_executa("CREATE TABLE t (a,b,c)") ;
#   sql_executa("INSERT INTO t VALUES (1,2,3)") ; sql_executa("SELECT * FROM t",&out) ; sql_fechar()
cc -O2 -std=c99 -I<libtiffanydb-dev>/include tmp.c -L<libdir> -ltiffanydb -o /tmp/consumidor
LD_LIBRARY_PATH=<libdir> /tmp/consumidor /tmp/base
rm -f /tmp/consumidor /tmp/base.*
```

Via escolhida: **servidor Pátria** (Ubuntu 24.04, `srv1559444.hstgr.cloud`, chave `segredo/id_ed25519_patria`, host/utilizador das variáveis do repo — padrão `tools/experimentos-patria.sh`). Resultados em §22.

## 22. Resultados E1–E3 (executado 2026-09-30, Pátria Ubuntu 24.04 — gcc 13.3, ar/nm/ldd/file GNU)

### E1 — nm (dependências e símbolos)
- Compilação de objectos com guardas: `cc -O2 -std=c99 -w -fPIC -DSQL_NO_MAIN -DPGWIRE_NO_MAIN -Ilib -Ibanco -c banco/sql.c|pgwire.c` → **limpo**.
- `nm -u sql.o` = **79 não-resolvidos, 100% libc** (printf/fopen/pread/pwrite/mkdir/…). **Nenhum** `sin cos tan log pow sqrt exp floor ceil fmod` → **`-lm` confirmadamente não referenciado** (o claim do §17 era certo). Ponte shell + stratum presentes ao nivel de link: `popen pclose socket bind connect sendto recvfrom`.
- **Zero símbolos definidos (T) fora do espaço `sql_` em sql.o** → 0 fuga de implementação (confirma §17.3).
- `pgwire.o`: só libc + sockets; `sql.c` com `-Wall -Wextra`: **41 avisos** (o relevante: `buf` não-usado em `banco/tiffany_shell.h:24`); `pgwire.c`: **0 avisos**.

### E2 — build real do pacote
- `ar rcs libtiffanydb.a sql.o pgwire.o` → **581.220 bytes**.
- `cc -shared -Wl,-soname,libtiffanydb.so.1` **sem `-lm`: linka → OK**.
- `libtiffanydb.so.1.0.0`: ELF 64-bit x86-64; `readelf`: NEEDED só **libc.so.6**, **SONAME `libtiffanydb.so.1`**; `ldd`: libc + ld-linux (sem libm).
- Exportações (`nm -D --defined-only`): **exactamente 32 `T sql_*` + 4 `T pgwire_*` + 3 `B` dados** (`sql_ultimos_passos/nos/prog`) — nada fora destes espaços.

### E3 — consumidor externo mínimo (prova do §17)
- `consumidor/` isolado sobre `/root/tiffany-e1e3`: cópia de `sql_api.h` (o "header instalado") + `e3.c`; link `cc -Iconsumidor e3 e3.c -L. -ltiffanydb -Wl,-rpath`; execução num `mktemp -d` (base em `/tmp`, **sem ficheiros internos do repo**).
- **Descoberta de API**: `sql_abrir`/`sql_executa` devolvem **`1` em sucesso e `0` em falha** (convenção contrária ao costume C). Primeira leitura do consumidor assumiu 0=ok e falhou; corrigida e confirmada.
- **Run 1**: abrir rc=1 → `CREATE TABLE t (a,b,c)` → 2×`INSERT` → `SELECT a,b,c FROM t` = **2 linhas [1|2|3] [4|5|6]**, `TAG=SELECT 2`, `sql_fechar`, exit 0.
- **Run 2 (novo processo, mesma base)**: `SELECT` devolve as mesmas 2 linhas → **persistência entre processos confirmada**.
- **Evidência D2 no POSIX**: o motor imprimiu no stdout do processo consumidor `tabela t criada: 3 colunas — INTEIRO INTEIRO INTEIRO` — a lib **escreve no stdout mesmo pela porta C**.
- Limpeza: `/root/tiffany-e1e3` apagado; `/root/tiffany-exp` (da casa, 27/08) intocado.

### O que fica comprovado (pronto a virar pacote — sem tocar no núcleo)
Núcleo POSIX compila como biblioteca (`libtiffanydb.a`/`.so` com SONAME próprio), dependência só `libc`, símbolos estáveis (32+4+3), consumidor externo funcional com header + `.so` em minutos, persistência ok.

### O que ainda bloqueia o empacotamento/produto (nenhum é falha — são regulamentos)
1. **Convenção de retorno da API** (1=ok/0=falha) sem constantes nem documentação no header — normalizar (`SQL_OK`/`SQL_ERR`) antes de expor devs.
2. **stdout do motor** misturado com o do processo (D2) — documentar no manual; a futura porta de saída é nova API (muda ABI, §D6).
3. **41 avisos `-Wall`** em sql.c (ex.: `buf` estático não-usado em `tiffany_shell.h`) — limpar ou assumir.
4. Artefactos no fonte a decidir para o pacote-dev: `lib/isa.h.gch` (PCH gerado), `lib/*stub.h`, `lib/pread_posix.c` (só Windows), `lib/so_cristal.c` (demo), `banco/erg_new.exe` (win32, fora do build POSIX).
5. Flags de build oficiais: `-DSQL_NO_MAIN -DPGWIRE_NO_MAIN` (sem elas há dois `main` na lib) — documentar.
6. pgwire: serviço v1 só **loopback** (D3); auth/TLS/limites ainda não existem.
7. Sem CLI (não pedido nesta fase).

Próximo passo natural (só por ordem): pacote `libtiffanydb1` + `libtiffanydb-dev` e bateria nativa completa (369+ medidores) no mesmo servidor.

## 23. CONTRATO CONGELADO DA BIBLIOTECA V1

> Este contrato regista o **comportamento observado** (E1–E3 + probes 2026-09-30). **Nenhuma alteração ao núcleo C**, **nenhuma alteração de assinaturas**, **nenhuma correção da convenção de retorno**. O objetivo é impedir que o empacotamento redefina silenciosamente o que foi validado.

### F1 — ABI pública (observada)

Verificado em E2 (`nm -D --defined-only libtiffanydb.so`):

| Grupo | Quantidade | Símbolos |
|---|---|---|
| `sql_*` (código T) | **32** | `sql_abrir`, `sql_au_cmp`, `sql_bit_fora_de_proposito`, `sql_bits_fora`, `sql_bits_fora_zera`, `sql_cel_tecto`, `sql_cels_fora`, `sql_cifra_para`, `sql_cols_de`, `sql_corpo28`, `sql_corpo28_n`, `sql_corpo_aureo`, `sql_corpo_cifra`, `sql_corpo_cristal`, `sql_cr_cmp`, `sql_esp_dist`, `sql_esp_prof`, `sql_executa`, `sql_fechar`, `sql_histograma`, `sql_lin_tecto`, `sql_optica_resumo`, `sql_ord_perdidos`, `sql_raizi`, `sql_restauros_falhados`, `sql_tx_abre`, `sql_tx_cheia`, `sql_tx_desfaz`, `sql_tx_escritas`, `sql_tx_fecha`, `sql_tx_fibra`, `sql_tx_ultra` |
| `pgwire_*` (código T) | **4** | `pgwire_listen`, `pgwire_recv_n`, `pgwire_send_all`, `pgwire_serve_conn` |
| Dados públicos (B) | **3** | `sql_ultimo_prog`, `sql_ultimos_nos`, `sql_ultimos_passos` |

**Confirmação de assinaturas (headers públicos):**

| Função | Assinatura (sql_api.h) | Retorno (observado/semântico) | Sucesso/Falha | Ownership args/retornos | Estado global afetado | Pode chamar antes de `sql_abrir`? | Observações |
|---|---|---|---|---|---|---|---|
| `sql_lin_tecto` | `long sql_lin_tecto(void)` | `4096L` (constante em runtime) | — (consulta) | sem alocações | nenhum | **Sim** (puro/constante) | Retorna `LIN_TECTO`. |
| `sql_cel_tecto` | `long sql_cel_tecto(void)` | `16384L` | — | nenhum | nenhum | **Sim** | |
| `sql_bits_fora` | `long sql_bits_fora(void)` | contador (>=0) | — | nenhum | contador global | **Sim** (0 antes de uso) | Instrumento. |
| `sql_bits_fora_zera` | `void sql_bits_fora_zera(void)` | — | — | nenhum | zera contador | **Sim** | |
| `sql_bit_fora_de_proposito` | `int sql_bit_fora_de_proposito(long i)` | `0`/`1` (bitmap S_MATCH) | — | nenhum | leitura | **Sim** | Instrumento (não corrige). |
| `sql_cels_fora` | `long sql_cels_fora(void)` | `0` (inicial) | — | nenhum | contador | **Sim** | |
| `sql_restauros_falhados` | `long sql_restauros_falhados(void)` | `>=0` | — | nenhum | contador | **Sim** | |
| `sql_ord_perdidos` | `long sql_ord_perdidos(void)` | `>=0` | — | nenhum | contador | **Sim** | |
| `sql_optica_resumo` | `void sql_optica_resumo(long *fora,*prop,*el,*par,*hip,*borda,*viola)` | — | — | ponteiros OUT (podem ser NULL) | leitura | **Sim** (lê catálogo atual) | Lê estado do catálogo aberto. |
| `sql_corpo28_n` | `int sql_corpo28_n(void)` | `44` | — | nenhum | nenhum | **Sim** | |
| `sql_corpo28` | `const char *sql_corpo28(int i,long *B,long *C)` | ptr nome ou `NULL` se i fora | válido se !=NULL | retorna string estática (não alocada pelo chamador); B,C OUT (opcionais) | nenhum | **Sim** | Fora de intervalo → **NULL** (probe `i=999` → NULL). |
| `sql_cifra_para` | `int sql_cifra_para(long B,long C)` | `0`/`1` (quadrado perfeito de |Δ|) | — | nenhum | **Sim** | |
| `sql_raizi` | `long sql_raizi(long n)` | raiz inteira | — | nenhum | nenhum | **Sim** | |
| `sql_corpo_cifra` | `void sql_corpo_cifra(long corpo,long parm,long *B,long *C,long *D)` | — | — | OUT opcionais | nenhum | **Sim** | |
| `sql_corpo_aureo` | `int sql_corpo_aureo(void)` | `2` | — | nenhum | nenhum | **Sim** | Constante. |
| `sql_corpo_cristal` | `int sql_corpo_cristal(void)` | `4` | — | nenhum | nenhum | **Sim** | Constante. |
| `sql_au_cmp` | `int sql_au_cmp(long ua,ub,va,vb,m)` | `-1/0/1` (ordem) | — | nenhum | nenhum | **Sim** | Comparador (puro). |
| `sql_cr_cmp` | `int sql_cr_cmp(long ua,ub,va,vb,t)` | `-1/0/1` (pela norma) | — | nenhum | nenhum | **Sim** | Comparador (puro). |
| `sql_esp_dist` | `long sql_esp_dist(long Di,long Dj)` | `|Di-Dj|` | — | nenhum | nenhum | **Sim** | |
| `sql_esp_prof` | `int sql_esp_prof(long Di,long Dj)` | profundidade divergência (bits) | — | nenhum | nenhum | **Sim** | `iguais → 64` (sizeof(long)*8), caso contrário índice de 1º bit diferente (0-based no cálculo). |
| `sql_abrir` | `int sql_abrir(const char *base)` | **1 = sucesso**, **0 = falha** | ver convenção F2 | `base` não copiado além de nomes internos; ficheiros abertos | inicializa disco/pool (uma vez), abre `.mem/.prog/.undo` da base, muda `fmem/fprog/fundo`, define `g_base/g_tabela` | **Sim** | Probe: `abrir(m2a)=1`, `abrir` após `abrir` sem fechar **troca de base** (não fecha a anterior implicitamente). `abrir` idempotente em termos de abrir outra base. |
| `sql_fechar` | `void sql_fechar(void)` | — | — | nenhum | fecha `fmem/fprog` (fsync+close); `sql_cap = NULL` | **Sim** | **Idempotente** (probe `fechar2` OK, segundo `sql_fechar()` não crasha). Não fecha `fundo` explicitamente neste caminho (ver implementação). |
| `sql_executa` | `int sql_executa(const char *sql, SqlOut *out)` | **1 = sucesso**, **0 = falha** | F2 | `sql` NUL-terminated (lido); `out` pode ser `NULL` (preenchido só se !=NULL) | executa lote/UNION, pode mudar tabela aberta, escrever disco, emitir stdout, atualizar contadores/`sql_ultimos_*` | **Sim** | **Observado:** pode ser chamado **antes de `sql_abrir`** (probe `antes`: rc=1,ok=1, err vazio) — nesse caso executa com estado “sem base” (SELECT sem tabela produz resultado vazio com mensagem no stdout). Com `out==NULL` retorna **1** em casos bem-sucedidos (probe `select-NULL rc=1`). |
| `sql_cols_de` | `int sql_cols_de(const char *tabela,char nomes[][32],int cap)` | **número de colunas lidas (>=0)** | `==0` significa tabela inexistente OU sem colunas OU parâmetros inválidos | `nomes` OUT (preenchido até cap); strings truncadas a 32 (snprintf). | **restaura** tabela anteriormente aberta (usa_tabela + restaura `g_tabela`) — não altera sessão permanentemente | **Sim** (requer base aberta no contexto, mas função gere restauração) | Probe: `cols_de('t')=3`, `cols_de('x')=0`. Não devolve código de erro separado. |
| `sql_tx_abre` | `void sql_tx_abre(void)` | — | — | nenhum | `undo_em_tx=1`, `undo_n=0`, `undo_cheio=0` | **Sim** | Fora de base aberta: estado lógico apenas (sem efeitos de I/O observáveis). |
| `sql_tx_desfaz` | `int sql_tx_desfaz(void)` | **1 = desfez com sucesso**, **0 = não desfez (pilha cheia OU sem fmem)** | F2 | nenhum | reescreve slots de undo no `fmem` (LIFO), zera `undo_n` | **Sim** | Probe: **fora de base/tx** → `0` (`desfaz-sem-base=0`, `ante_tx` antes de abrir). Dentro de tx com escritas → `1` e reverteu linhas (m9). Comentário header: “0 se a pilha encheu: então NÃO se desfaz nada”. Também retorna 0 se `fmem<0`. |
| `sql_tx_fecha` | `void sql_tx_fecha(void)` | — | — | nenhum | `undo_em_tx=0`, `undo_n=0`, `undo_cheio=0` | **Sim** | Idempotente. |
| `sql_tx_cheia` | `int sql_tx_cheia(void)` | `0`/`1` | — (flag) | nenhum | leitura | **Sim** | Probe: 0 na maioria dos casos testados. |
| `sql_tx_escritas` | `long sql_tx_escritas(void)` | nº escritas na pilha undo (`undo_n`) | — | nenhum | leitura | **Sim** | Probe: cresce dentro de tx (m9 `dentro-tx: escritas=28`). |
| `sql_tx_ultra` | `void sql_tx_ultra(long *prof_min,*prof_ponta,*absorve,*passos)` | — | — | OUT opcionais | leitura da pilha undo | **Sim** | Só significativo com transacção ativa/com histórico de escritas. |
| `sql_tx_fibra` | `void sql_tx_fibra(long *escritas,*slots_distintos,*maior_G,*soma_G)` | — | — | OUT opcionais | leitura de undo | **Sim** | Idem. |
| `sql_histograma` | `int sql_histograma(const char *tabela,const char *coluna,long *hist,int n,long *fora)` | **>=0 = nº de entradas dentro do domínio**, **-1 = falha** | `retorno >= 0` = ok; `retorno == -1` = falha | `hist` OUT (zerado até n), `fora` OUT (opcional). | restaura tabela anterior (não altera sessão) | **Sim** (contexto de base) | Probe: `hist('t','b') dentro=3 fora=0 → retorna 3` (>=0 = sucesso). `hist('t','zz')=-1`, `hist('x','a')=-1` → falha. |
| `sql_ultimo_prog` | `extern long sql_ultimo_prog` | FNV do bytecode última varredura | — (dado) | leitura | atualizado por `sql_executa` | **Sim** (0/valor anterior) | Probe: `S9 prog=2915738222`, m9 `prog=663349981`. |
| `sql_ultimos_nos` | `extern long sql_ultimos_nos` | nós visitados última descida | — | leitura | atualizado | **Sim** | 0 em muitos casos (sem índice/árvore). |
| `sql_ultimos_passos` | `extern long sql_ultimos_passos` | passos última varredura | — | leitura | atualizado | **Sim** | m9 `passos=172`. |

**ABI pgwire (4):**

| Função | Assinatura (pgwire_api.h) | Retorno | Significado | Notas |
|---|---|---|---|---|
| `pgwire_send_all` | `int pgwire_send_all(int fd,const void *buf,int n)` | `0 = enviado completo`, **`-1 = erro`** | I/O | loop send() até n; w<=0 → -1. |
| `pgwire_recv_n` | `int pgwire_recv_n(int fd,void *buf,int n)` | `0 = lido completo`, **`-1 = erro`** | I/O | loop recv() até n; r<=0 → -1. |
| `pgwire_listen` | `int pgwire_listen(int want,int *got)` | `fd >= 0` (listen) ou **`-1 = falha`** | bind/listen | `want<=0` tenta 5432, senão 55432 (fallback). `got` recebe porto efectivo. Bind **127.0.0.1** (loopback). |
| `pgwire_serve_conn` | `int pgwire_serve_conn(int cfd,int32_t pid,int32_t key)` | `0 = Terminate/fecho normal`, **`-1 = erro/fatal`**, `1` retornado internamente (fluxo) — **valor de retorno da API**: 0 ou -1 | sessão PG | SSL→N (recusa SSL), Startup trust, Simple+Extended Query. Usa `sql_executa` (Query/Execute). |

> `pgwire_sess.h` contém apenas helpers `static` (não exportados). Não fazem parte da ABI pública.

### F2 — Convenção de retorno (CONTRATO EXPLÍCITO)

**Regra congelada (não corrigir):**

- `sql_abrir(...) == 1` → **sucesso**
- `sql_abrir(...) == 0` → **falha**

- `sql_executa(...) == 1` → **sucesso**
- `sql_executa(...) == 0` → **falha**

**Demais funções públicas (inspeção):**

| Função | Convenção observada | Observação |
|---|---|---|
| `sql_histograma` | **>= 0 = sucesso** (nº dentro domínio), **== -1 = falha** | Probe confirma (-1 em coluna inexistente/tabela inexistente). |
| `sql_cols_de` | **>= 0 = contagem lida** (0 = sem colunas OU tabela inexistente). **Sem código de erro separado.** | Falha por “não existe” não distingue de “vazia”. |
| `sql_tx_desfaz` | **1 = desfez**, **0 = não desfez** (pilha cheia **OU** sem `fmem`/fora de base). | Header documenta “0 se a pilha encheu…”; implementação também devolve 0 sem base. |
| `sql_corpo28` | **ptr != NULL = válido**, **NULL = fora de intervalo** | Sem boolean de “sucesso”. |
| `sql_bit_fora_de_proposito` | **0/1** (flag/resultado) | Não é “sucesso/falha”. |
| `sql_tx_cheia` | **0/1** (flag) | |
| `pgwire_*` (I/O) | **0 = ok**, **-1 = erro** | Padrão POSIX-like (send/recv). Diferente de `sql_*`. |
| Funções `void` | sem retorno | |
| Funções que devolvem contadores/constantes (`sql_lin_tecto`, `sql_bits_fora`, …) | sem semântica sucesso/falha | Valores >=0. |

**Regra:** se ambígua → **NÃO DETERMINADO**. Não inferido. A convenção acima de `sql_abrir`/`sql_executa` está **explicitamente congelada** como observada (1=sucesso).

### F3 — stdout (comportamento observado)

E3 e probes mostram que o **motor escreve no stdout mesmo quando chamado pela porta C** (`sql_executa` com `SqlOut*`).

| Operação/Contexto | Mensagens no stdout (exemplos observados) | Normal/Diag | Ocorre durante `sql_executa`? |
|---|---|---|---|
| `CREATE TABLE …` | `tabela t criada: 3 colunas — INTEIRO INTEIRO INTEIRO` | **Normal** (informativo do motor) | **Sim** |
| `INSERT …` | `1 linha inserida (3 colunas) — … bytes de ISA, … passos; agora N linhas` | Normal | **Sim** |
| `SELECT …` | `-- … bytes de ISA […], … átomo(s), … passos, … linha(s) lida(s)` + linhas formatadas `   a | b | c` | **Normal** (resultado impresso) | **Sim** |
| `UPDATE …` | `-- … bytes de ISA […], … átomo(s), … passos, … linha(s) atualizada(s)` | Normal | **Sim** |
| `DELETE …` | `-- … bytes de ISA […], … átomo(s), … passos, … linha(s) apagada(s)` | Normal | **Sim** |
| Lote/UNION (casos com múltiplos comandos) | `-- lote: %d comando(s), %d recusado(s) --- o lote continuou…` (emitido internamente quando recusados>0) | Diag/aviso | **Sim** |
| Estados sem tabela/base | `-- 1 linha, sem tabela` (SELECT antes de abrir) | Normal/observado | **Sim** |
| Erros/recusas | `erro: … RECUSADA.` (ex.: UNION encadeada, lados com colunas diferentes, lado falhou) | **Diagnóstico/erro** | **Sim** |

**Silenciamento/redirecionamento:** **não existe forma já existente no API público para silenciar/redirecionar este stdout.** O motor usa `printf/fprintf(stdout,…)` diretamente (ver D2). Não há callbacks/log API nem flags públicas. Qualquer redirecionamento seria externo (shell/`dup2`) ao processo consumidor.

**Conclusão F3:** **stdout faz parte do comportamento observado da V1.** Não é ruído acidental — aparece em caminhos de execução normais (`SELECT`, `CREATE`, `INSERT`, `UPDATE`, `DELETE`, lote). Deve ser **documentado** antes de qualquer mudança de ABI (D6). Não criado `SqlOut`/callbacks nesta fase.

### F4 — Dependências reais

Matriz por fronteira:

| Dependência | Link `.so` (observado E2) | Runtime (caminhos do motor) | Estado V1 |
|---|---|---|---|
| `libc.so.6` | **Obrigatória** (NEEDED único) | Obrigatória | **Obrigatória** |
| `libm.so.*` | **Não necessária** | **Não referenciada** (nm -u sem sin/cos/log/pow…). Link sem `-lm` OK. | **Não necessária** |
| Node.js (`node`) | Não | **Opcional/condicional** — usado apenas por **pontes shell**: `BASH/NODE/POWERSHELL`, `MANIFESTO PARA U/U PARA MANIFESTO`, `PAGINA PARA U/U PARA PAGINA` (via `popen(node)` + `tools/manifesto_u.mjs`, `tools/pagina_u.mjs`). | **Não requisito de todas as operações**. Núcleo SQL/pgwire funciona sem Node. |
| `bash` | Não | **Opcional/condicional** — verbo `BASH` via `popen(bash)` | Opcional (caminhos de ponte). |
| PowerShell (`powershell`) | Não | **Opcional/condicional** — verbo `POWERSHELL` | Opcional. |
| Filesystem POSIX | Não (link) | **Obrigatório** (`open/mkdir/pread/pwrite/fsync/ftruncate/access/opendir/readdir/unlink`, etc.) | **Obrigatório** (POSIX-only). |
| Windows/MinGW | Fora | Fora | **Fora do alvo V1** (POSIX/Ubuntu). |
| Sockets BSD (IPv4/TCP/UDP) | Não (link libc) | Usados por **pgwire** (`socket/bind/listen/connect/send/recv/setsockopt`) e por **canal/banda** (stratum) — capacidades distintas das pontes shell. | Obrigatório para pgwire/canal (quando usados). SQL puro não exige sockets. |

**Distinção crítica:** **dependência de link do `.so` = somente `libc`**. **Dependências externas em runtime = opcionais por caminho** (pontes shell exigem binários externos quando invocadas). O núcleo não exige Node/bash/PowerShell para abrir base/SQL/pgwire.

### F5 — Modelo de estado

Com base em N1 + probes:

| Aspeto | Observado (contrato) | Notas |
|---|---|---|
| Estado global | **Sim** (não encapsulado por instância). | `fmem/fprog/fundo`, `g_base`, `g_tabela`, `sql_cap`, contadores (`bits_fora`, `cels_fora`, …), pilha `undo_*`, `trava_em`. |
| Não-reentrância | **Sim**. | Estado global único por processo. Chamadas concorrentes de threads não sincronizadas → comportamento indefinido (fora de escopo V1). |
| Banco/base por processo | **Um ficheiro-base ativo por processo**. | `sql_abrir(base)` troca base ativa (fecha descritores anteriores). Várias bases distintas podem ser abertas sequencialmente no mesmo processo. |
| Abrir/fechar | `sql_abrir` inicializa pool/disco uma única vez (`disco_ok`), abre `.mem/.prog/.undo` da base. `sql_fechar` fecha `fmem/fprog` (fsync+close), põe `sql_cap=NULL`. **Idempotente** (segundo `sql_fechar()` seguro). | Troca de base via `sql_abrir` **não fecha** explicitamente a base anterior antes de abrir nova (comportamento observado: descritores mudam). |
| Duas instâncias simultâneas no mesmo processo | **Não suportado (não-reentrante)**. | Estado global partilhado. Abrir segunda base sobrescreve estado da primeira. |
| Persistência entre processos | **Comprovada (E3)**: 2º processo lê linhas inseridas pelo 1º (`m2a` reaberto). | Ficheiros `.mem/.prog/.undo` no filesystem. |
| Comportamento antes de `sql_abrir` | Permitido (consulta). `sql_executa` com `out` pode devolver **rc=1, ok=1** e emitir stdout mesmo sem base aberta (SELECT “sem tabela”). | Não é erro — comportamento observado (probe `antes`). |
| Reabrir mesma base | **Suporta** (lê estado persistente). | Probe `reabrir(m2a)` lê linha `(1,2,3)`. |
| Transacção (undo) | `sql_tx_abre/fecha/desfaz`. `sql_tx_desfaz()` retorna **0** fora de contexto/base (não desfez). | Undo por escritas em `fmem`, LIFO. `undo_cheio` sinaliza pilha cheia (retorno 0 em `sql_tx_desfaz`). |
| `sql_cols_de` | Restaura tabela anteriormente aberta (não altera sessão permanentemente). | Contrato de não-efeito lateral na sessão chamadora. |
| `sql_histograma` | Idem (restaura tabela anterior). | |

### F6 — Fronteira Pública/Interna V1

**Público v1 (consumidor externo não precisa do repositório Tiffany):**

- **Headers necessários:** `banco/sql_api.h`, `banco/pgwire_api.h` (+ `stdint.h` transitivo via pgwire_api.h). `banco/pgwire_sess.h` **não é público** (helpers estáticos, uso interno a pgwire.c).
- **ABI `sql_*`:** 32 funções + 3 dados globais (listados F1). Assinaturas conforme headers.
- **ABI `pgwire_*`:** 4 funções (listados F1).
- **Artefactos de link:** `libtiffanydb.so.1` (SONAME) + `libtiffanydb.a` (estático). Consumidor linka com `-ltiffanydb` e inclui headers públicos.

**Interno v1 (não exposto):**

- Demais headers `lib/*.h`, `banco/*.h` não listados acima (ex.: `pgcat.h`, `pgfunc.h`, `tiffany_shell.h`, `tiffany_node.h`, `parse_ficheiro.h`, `pgwire_sess.h`, `pgwire.h`, `pgmsg.h`, `slot_mem.h`, `disco.h`, ISA, etc.)
- Estruturas internas (`SqlCap`, `UndoEnt`, `PgStmt/PgPortal`, tabelas/catalogo, bytecode ISA)
- Funções `static` (468+ linhas static em `sql.c`, 100% internos)
- Detalhes de ISA/armazenamento (.mem/.prog/.undo, slots, endereçamento)
- Harnesses, corpus, medidores do Tiffany geral (`tests/*`, `.erg`, `.fita*`, `.wasm`)
- Arquivos de desenvolvimento/experimentos (`bench_3c_enredo/`, `.torre/`, `segredo/`, scripts SSH, `.tex/.pdf`, etc.)

**Regra:** consumidor externo compila apenas com headers públicos + `.so/.a`, sem conhecer o repositório completo.

### F7 — Teste de consumidor externo (especificação mínima)

Transformado de E3 em especificação reproduzível (independente do restante do Tiffany):

```text
1. Incluir headers públicos: sql_api.h (e opcional pgwire_api.h)
2. Linkar com libtiffanydb (.so com rpath ou LD_LIBRARY_PATH)
3. sql_abrir("base_teste")  → deve retornar 1
4. sql_executa("CREATE TABLE t (a,b,c)", &out) → rc==1 e out.ok==1
5. sql_executa("INSERT INTO t VALUES (1,2,3)", &out) → rc==1 e out.ok==1
6. sql_executa("INSERT INTO t VALUES (4,5,6)", &out) → rc==1 e out.ok==1
7. sql_executa("SELECT a,b,c FROM t", &out) → rc==1, out.ok==1, ncols==3, nrows==2, tag contém "SELECT", células [1,2,3],[4,5,6]
8. sql_fechar()
9. Novo processo (independente)
10. sql_abrir("base_teste") → 1
11. sql_executa("SELECT a,b,c FROM t", &out) → rc==1, out.ok==1, nrows==2 (persistência confirmada)
12. sql_fechar()
```

**Critério de aceitação:** todos os passos com sucesso. **Não depende** de tests/, corpus, .erg, Node, bash, nem do repositório Tiffany.

### F8 — Lixo de distribuição (classificação)

**NÃO deve entrar no pacote (libtiffanydb-dev/libtiffanydb1):**

| Artefato | Localização (exemplos) | Razão |
|---|---|---|
| PCH gerados | `lib/isa.h.gch` | Build artefact, específico de compilação local |
| Executáveis Windows | `banco/erg_new.exe`, outros `.exe` | Fora do alvo POSIX/Linux |
| Objectos temporários | `*.o`, `sql.o`, `pgwire.o`, `libtiffanydb.so.1.0.0`, `libtiffanydb.so`, `libtiffanydb.a` (build tree) | Gerados no build |
| Experimentos | `bench_3c_enredo/` | Não é runtime da biblioteca |
| Corpus/dados de desenvolvimento | `.torre/` (67 entradas), `dados/`, `banco/apps/*.erg`, `banco/apps/*.tmp`, `*.bin`, `*.erg` diversos | Não necessários à instalação da lib |
| Documentação fonte/teórica | `*.tex`, `*.pdf`, `redes/`, `docs/` (muitos) | Podem ir em -doc separado (não obrigatório) |
| Scripts de infra/SSH | `tools/chave-patria.sh`, `tools/experimentos-patria.sh`, `tools/publica*.sh`, `tools/painel.sh`, `plataforma/infra/*`, `.github/workflows/*` | Específicos do repo/infra |
| Credenciais/segredos | `segredo/` (`id_ed25519_patria*`), `.env*` | Proibido em pacote |
| Stubs/demos não necessários | `lib/pread_posix.c` (shim Windows), `lib/so_cristal.c` (demo), `lib/*stub.h` (stratum_stub.h, no_pgwire_stub.h) | Dependem do alvo (Windows vs POSIX); pacote POSIX não precisa de shims Windows |
| Artefatos específicos Pátria | referências/nomes Pátria em scripts (não código núcleo) | Infra específica |

**Regra:** apenas headers públicos (`sql_api.h`, `pgwire_api.h`), bibliotecas compiladas (`.so.1*`, `.a`) e eventualmente `LICENSE/NOTICE/README` (se existirem). **Não apagar nada do repositório nesta etapa** — apenas classificação.

### F9 — Versionamento (proposto, sem implementar)

Quatro superfícies **independentes** (D6 aprovado):

| Superfície | Escopo | Sugestão SemVer | Regra de bump |
|---|---|---|---|
| **1. Produto/API (pública)** | API C exposta por `sql_api.h` + `pgwire_api.h` (assinaturas, semântica documentada, convenção de retorno congelada) | `MAJOR.MINOR.PATCH` (SemVer) | MAJOR = quebra ABI/API (remoção/semântica incompatível), MINOR = adições compatíveis (novas funções), PATCH = correções internas compatíveis. |
| **2. ABI/SONAME** | Símbolos exportados (32+4+3), layout de structs usados na API, calling convention | `libtiffanydb.so.1` (SONAME estável) | Bump SONAME (`.so.2`) **apenas** se ABI quebrada (símbolos removidos/trocados, structs mudam). Adições compatíveis podem manter `.so.1` + MINOR do produto. |
| **3. Formato persistente em disco** | `.mem`, `.prog`, `.undo`, formatos de catálogo/slots | Versão de formato independente | Mudança que torne ficheiros incompatíveis entre versões → versão de disco própria (não implica bump de API/SONAME). |
| **4. Protocolo PostgreSQL wire** | Mensagens Frontend/Backend, Startup/trust, Simple+Extended, tipos anunciados | Versão de protocolo independente | Mudança de protocolo (features/auth/TLS) não implica API/ABI/disco. |

**Nota:** não assumir propagação automática entre 1–4.

### F10 — Status F1–F10

| Item | Estado | Evidência |
|---|---|---|
| **F1 — ABI pública** | **FECHADO** | E2: 32 T sql_*, 4 T pgwire_*, 3 B; assinaturas confirmadas em headers; tabela completa acima. |
| **F2 — Convenção de retorno** | **FECHADO** | `sql_abrir/executa` = **1=sucesso, 0=falha** (congelado). Demais classificadas por inspeção (sem inferência). |
| **F3 — stdout** | **FECHADO** | Observado em E3/probes (CREATE/INSERT/SELECT/UPDATE/DELETE/lote/erros). Sem mecanismo de silenciamento público. Comportamento documentado como observado. |
| **F4 — Dependências reais** | **FECHADO** | Link `.so` = **só libc**. Runtime Node/bash/PowerShell = **opcionais por caminhos de ponte** (não requisito geral). Filesystem POSIX obrigatório, Windows fora. |
| **F5 — Modelo de estado** | **FECHADO** | Não-reentrante, global, 1 base/processo, abrir troca base, fechar idempotente, persistência entre processos comprovada, comportamento antes de abrir documentado, restauração em `cols_de/histograma`. |
| **F6 — Fronteira público/interno** | **FECHADO** | Público = `sql_api.h`, `pgwire_api.h`, 32+4+3. Interno delimitado (static 100%, demais headers não necessários ao consumidor). |
| **F7 — Teste externo mínimo** | **FECHADO** | Especificação reproduzível (12 passos), independente do restante do Tiffany. |
| **F8 — Lixo de distribuição** | **FECHADO** | Classificação por categorias (sem apagar repo). |
| **F9 — Versionamento** | **FECHADO (proposto)** | 4 superfícies independentes, SemVer vs SONAME vs disco vs wire. |
| **F10 — Documento** | **FECHADO** | `§23` completo nesta proposta. |

**Status global:** **F1–F10: FECHADO**

### Observações finais (sem tocar no núcleo)

- Descoberta de contrato (não defeito): `1=sucesso/0=falha` em `sql_abrir/sql_executa` **mantém-se** (congelada). Não corrigida.
- `sql_executa` com `out==NULL` é válido (sucesso devolve 1). `sql_executa` antes de `sql_abrir` é válido com comportamento observado.
- stdout misturado é **comportamento observado da V1** (D2) — documentado, não alterado.
- Nenhum `-lm` no link; ABI estável e mínima (só libc). SONAME `libtiffanydb.so.1`.
- Limpeza do experimento: `/root/tiffany-e1e3` removido, `/root/tiffany-exp` intocado (conforme executado).

**Próxima etapa (após este congelamento):** **G — árvore de instalação + build reproduzível + `.deb`** (sem CLI/APT ainda), sobre esta base especificada.

---

## 24. G — ÁRVORE DE INSTALAÇÃO E BUILD V1 (executado 2026-09-30, Pátria Ubuntu 24.04)

Âmbito exacto do que foi feito: **construir fora do repositório, instalar com `DESTDIR`, publicar `pkg-config`, provar um consumidor externo, separar runtime/dev, e medir reprodutibilidade**. **Nada** foi movido no repositório, **nenhum** `.deb` foi construído, **nenhuma** CLI foi criada, **nenhuma** linha do núcleo foi alterada.

### G0/G2 — Ambiente e convenções de layout (observado)

| Item | Valor observado |
|---|---|
| Distro | Ubuntu 24.04.4 LTS |
| Kernel | `Linux 6.8.0-106-generic x86_64` |
| Compilador | `cc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0` |
| Arquitectura | `amd64` / multiarch `x86_64-linux-gnu` |
| Headers (Debian Policy 10.3.3) | `/usr/include/<nome-do-pacote>` — *"o caminho não deve incluir o nome do pacote"* → usa-se `/usr/include/tiffanydb/` (o nome do projecto, **não** o do pacote binário `libtiffanydb-dev`) |
| Bibliotecas | `/usr/lib/x86_64-linux-gnu/` |
| `pkg-config` | `/usr/lib/x86_64-linux-gnu/pkgconfig` e `/usr/share/pkgconfig` (existe) |
| Ferramentas presentes | `dpkg-dev` **instalado**, `fakeroot` **instalado**, `pkg-config` **1.8.1 (instalado por esta etapa)**, `debhelper` **ausente**, `makeinfo` **ausente** |

**Advertências de ambiente que afectam a fase H (não são bloqueios de G):**

1. **O Pátria não é um POSIX limpo.** Corre **PostgreSQL 16 com 5 clusters** (`main`, `erp`, `erphomolog`, `homolog`, `dev`) em `127.0.0.1:5432`, mais portas `5433/5434/5436/5437/5438` (contentores), Redis (`6379`), Docker e serviços web (`5173`, `8083`, `8088`). Consequência concreta: `pgwire_listen(0, &got)` cai em `5432` → **colide com o PostgreSQL real**. Todo oGPPG tem de usar **portas explícitas altas**. (Foi exactamente este conflito que produziu, numa primeira tentativa, o erro `expected SASL resp` — resposta do PostgreSQL real, não da biblioteca.)
2. `debhelper` e `makeinfo` **não instalados** → a fase H tem de os instalar antes de `dpkg-buildpackage`.
3. O `tar` do Windows não passa pelo *pipe* binário do PowerShell (`gzip: stdin: not in gzip format`); a via fiável é `tar -czf` local + `scp` + extracção remota.
4. Piping de scripts para `ssh … bash` injecta **CRLF** e parte o `bash` (`$'echo\r': command not found`); via fiável = `scp` + `sed -i 's/\r$//'` remoto.

### G1 — Conjunto mínimo de fontes (fechado)

O produto compila a partir de **2 unidades de tradução** + **2 headers públicos** + **37 headers privados**, e **nada mais**:

```text
src/core.c        <- banco/sql.c      (motor)
src/pgwire.c      <- banco/pgwire.c   (wire PostgreSQL)

include/tiffanydb/sql_api.h     (público, 32 funções + SqlOut)
include/tiffanydb/pgwire_api.h  (público, 4 funções)

orig_headers/  (37 privados, não instalados)
  banda binario caminho cifra contrato corpos disco dual16 dual32
  exterior fatorial forma i128 incidencia inteiros isa largura linear
  palavra8 parse_ficheiro pgcat pgfunc pgmsg pgwire pgwire_sess
  racionais reta serie simbolos slot_map slot_mem stratum
  tiffany_node tiffany_shell umbit unidade word_isa
```

Verificação de fecho (executada): a varredura de todos os `#include "…"` de `orig_headers/*.h` **e** `src/*.c` contra os ficheiros presentes devolveu **`(nenhum — o conjunto de headers está fechado)`**.

Correção de percurso (registada porque muda a lista): `pgwire.h` e `pgmsg.h` estão em **`lib/`**, não em `banco/`; e `pgcat.h`, `pgfunc.h`, `tiffany_node.h`, `tiffany_shell.h`, `parse_ficheiro.h`, `pgwire_sess.h` vivem em **`banco/`**. No E1/E2 esta separação estava mascarada por `-Ilib -Ibanco`; com a árvore de produtominimalista cada header tem de ser copiado explicitamente, senão falha.

**Excluído (confirmado por grafo, não por nome):** `lib/pread_posix.c` (só Windows), `lib/so_cristal.c` (demo), toda a infra de Node, corpus, `.erg`, `.tex`, programas-harness de `banco/`, testes e medidores.

### G3/G4 — Build fora do repositório, flags fixas

Comandos exactos (executados em `/root/tiffany-g`, ficheiros de produto copiados, repo nunca tocado):

```bash
FUNC="-std=c99 -D_POSIX_C_SOURCE=200809L -DSQL_NO_MAIN -DPGWIRE_NO_MAIN"
DIST="-O2 -g0 -fPIC -fno-common -Wall -Wextra"
INCS="-I$R/include/tiffanydb -I$R/orig_headers"

cc $FUNC $DIST $INCS -c src/core.c   -o sql.o
cc $FUNC $DIST $INCS -c src/pgwire.c -o pgwire.o
ar rcs libtiffanydb.a sql.o pgwire.o
cc -shared -Wl,-soname,libtiffanydb.so.1 \
   -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -Wl,--build-id=none \
   -o libtiffanydb.so.1.0.0 sql.o pgwire.o
ln -sf libtiffanydb.so.1.0.0 libtiffanydb.so.1
ln -sf libtiffanydb.so.1     libtiffanydb.so
```

**Descoberta de flags (medida, não assumida):**

- **Sem** `-D_POSIX_C_SOURCE=200809L` a compilação **falha**: `pread`, `pwrite`, `mkdir`, `fsync`, `ftruncate`, `access` ficam ocultos. Com a macro, aparecem como `U` em `nm` e resolvem de `libc`. Confirmado: `-D_POSIX_C_SOURCE=200809L` é **obrigatório na linha de comando**.
- **Não** passar `-D_DEFAULT_SOURCE`: `banco/sql.c:76` já faz `#define _DEFAULT_SOURCE` ele próprio, e a duplicação gera `warning: "_DEFAULT_SOURCE" redefined` (341 → **342** avisos, sem qualquer ganho funcional).
- Consoante disto, `cc -std=c99 -D_POSIX_C_SOURCE=200809L …` dá **341 avisos** e o mesmo ABI. **Este é o conjunto oficial.**

**Avisos com as flags oficiais (`-O2 -g0 -fPIC -fno-common -Wall -Wextra`):**

| Ficheiro | Total | Repartição |
|---|---|---|
| `core.c` (`banco/sql.c`) | **341** | 288 `-Wunused-function`, 11 `-Wunused-result`, 7 `-Wstringop-truncation`, 3 `-Wcomment`, 2 `-Wmisleading-indentation`, 1 `-Wunused-variable` (`buf` em `tiffany_shell.h:24`), 1 `-Wrestrict`, restantes `-Wformat` |
| `pgwire.c` | **14** | 14 `-Wunused-function` |

**Divergência a reconciliar (honesta):** o §22 (E1) registou «41 avisos» em `sql.c` e «0 avisos» em `pgwire.c`. Esses números **não reproduzem** com as flags oficiais acima (341/14); o E1 compilou os objectos com `-w`. O que fica valido é que os `-Wunused-function` correspondem a helpers `static` de headers privados que `sql.c` não chama — ou seja, **lixo de linkage interno**, não defeitos de API. Há **um** aviso material a registar: `core.c:4119` usa `%d` com argumento `long int` (`-Wformat=`) — benigno em LP64, mas a corrigir antes de expor a terceiros. **Nenhuma correcção foi feita** (sem tocar no núcleo).

### G5 — Verificação do artefacto

| Comprovação | Resultado |
|---|---|
| `file` | `libtiffanydb.a: current ar archive`; `libtiffanydb.so.1.0.0: ELF 64-bit LSB shared object, x86-64, dynamically linked, not stripped` |
| `readelf -d` NEEDED | **só `libc.so.6`** |
| SONAME | **`libtiffanydb.so.1`** |
| FLAGS / FLAGS_1 | `BIND_NOW` / `NOW` (relro+now activos) |
| `ldd` | `linux-vdso`, `libc.so.6`, `ld-linux-x86-64.so.2` — **sem `libm`** |
| `nm -D --defined-only` | **T=36, D/B=3** → **32 `sql_*` + 4 `pgwire_*` + 0 fora** |
| libm | `nm -D -u`: **NENHUM** (`sin cos tan log pow sqrt exp floor ceil fmod`) |
| `main`/`_init`/`_fini` exportados | **nenhum** |
| Secções | `.text` 269 442 · `.rodata` 52 996 · `.data.rel.ro` 3 352 · `.data` 3 776 · **`.bss` 3 091 416** |
| Exportados vs. `.a` | 36 `T` públicos; **233** `t` internos disponíveis só no link estático |

**Nota de produto (nova, relevante para o consumidor):** `.bss` = **≈2,95 MiB** de buffers estáticos. O `.so` em disco é pequeno (394 432 B) mas o **RSS** de qualquer processo que use a biblioteca cresce ~3 MB. Convém documentar no manual do `-dev`.

**G5 vs E2:** `32 + 4 + 3`, NEEDED só `libc.so.6`, SONAME `libtiffanydb.so.1`, sem `libm` → **idêntico**. A construção como produto não alterou a superfície pública.

### G6 — Instalação com `DESTDIR` (receita final)

`DESTDIR=/tmp/tiffany-root`, multiarch `/usr/lib/x86_64-linux-gnu`, tamanho total **1,1 MB**:

```text
/usr/include/tiffanydb/pgwire_api.h
/usr/include/tiffanydb/sql_api.h
/usr/lib/x86_64-linux-gnu/libtiffanydb.so        -> libtiffanydb.so.1
/usr/lib/x86_64-linux-gnu/libtiffanydb.so.1      -> libtiffanydb.so.1.0.0
/usr/lib/x86_64-linux-gnu/libtiffanydb.so.1.0.0
/usr/lib/x86_64-linux-gnu/libtiffanydb.a
/usr/lib/x86_64-linux-gnu/pkgconfig/tiffanydb.pc
/usr/share/doc/libtiffanydb-dev/LICENSE
/usr/share/doc/libtiffanydb-dev/NOTICE
```

Nenhum caminho fora do prefixo; nenhum binário; nenhum serviço; nenhum ficheiro em `/etc`.

### G7 — `pkg-config`

`pkgconfig/tiffanydb.pc.in` do produto:

```text
prefix=@PREFIX@
libdir=@LIBDIR@
includedir=${prefix}/include

Name: tiffanydb
Description: Tiffany DB - motor SQL embebido (biblioteca C)
Version: 1.0.0
Libs: -L${libdir} -ltiffanydb
Cflags: -I${includedir}
```

Saída real (com `PKG_CONFIG_PATH`/`PKG_CONFIG_SYSROOT_DIR` a apontar para o `DESTDIR`):

| Consulta | Resultado |
|---|---|
| `--cflags` | `-I/tmp/tiffany-root/usr/include` |
| `--libs` | `-L/tmp/tiffany-root/usr/lib/x86_64-linux-gnu -ltiffanydb` |
| `--modversion` | `1.0.0` |
| `--static --libs` | `-L… -ltiffanydb` — **sem `-lm`, sem `-lpthread`, sem `-ldl`** (coerente com G5) |

`Cflags` aponta para `/usr/include` (não `/usr/include/tiffanydb`) porque o consumidor escreve `#include <tiffanydb/sql_api.h>`. Sem `Requires`/`Requires.private`: o produto não tem dependências pkg-config.

### G8 — Consumidor externo (a partir **só** da instalação)

Todos os consumidores abaixo compilam com `$(pkg-config --cflags/--libs tiffanydb)` e **nunca** referenciam `banco/` nem `lib/`.

**G8a — API SQL, link dinâmico**
`readelf -d` do consumidor: `NEEDED libtiffanydb.so.1`, `NEEDED libc.so.6`.
Run 1: `sql_abrir`→`1`; `CREATE TABLE t (a,b,c)`; 2×`INSERT`; `SELECT a,b,c FROM t` → `TAG=SELECT 2`, `ROWS=2`, `[1][2][3]` e `[4][5][6]`.
Run 2 (**novo processo**, base nova em `mktemp -d`): mesmas 2 linhas → **persistência entre processos**.
Ficheiros criados apenas no cwd isolado: `cons_g.mem`, `cons_g.prog`, `cons_g.undo`, `cons_g__t.mem`, `dados/`.

**G8b — resolução do SONAME**
`ldd` **sem** `LD_LIBRARY_PATH` → `libtiffanydb.so.1 => not found` (o `DESTDIR` não está no `ld.so.cache`; comportamento **esperado** para staging). Com `LD_LIBRARY_PATH=<DESTDIR>/usr/lib/x86_64-linux-gnu` → resolve para a cópia instalada. Num pacote instalado em `/usr/lib/x86_64-linux-gnu` isto deixa de ser necessário. Nenhum `-rpath` foi Required.

**G8c — link estático**
`cc … $(pkg-config --static --libs tiffanydb) main.c` com `libtiffanydb.a`: `ldd` mostra **0** ligações dinâmicas a `tiffanydb`; o executável corre e faz o mesmo `SELECT` com 2 linhas. O `.a` é utilizável como está.

**G8d — as 4 assinaturas públicas de `pgwire_api.h`**
Servidor e cliente são **processos separados** (ambos compilados só com headers instalados):

```text
[srv] sql_abrir('pgbase') rc=1
[srv] 1 pgwire_listen(15490,&got) fd=6 porto=15490
[cli] 2 pgwire_send_all(startup) rc=0
[cli] 3 pgwire_recv_n  -> R (AuthenticationOk), S, S, S, K, Z
[cli] CREATE   cmd='CREATE TABLE'
[cli] INSERT 1 cmd='INSERT 0 1'
[cli] INSERT 2 cmd='INSERT 0 1'
[cli] SELECT s1 colunas=3 | ROW1 (ncols=3): [1] [2] [3] | ROW2: [4] [5] [6]
[cli] SELECT s1 cmd='SELECT 2'
[srv] 4 pgwire_serve_conn rc=0 (0 = sessão limpa)
[srv] sql_fechar() OK
```

Sessão 2 (**processo novo**, mesma base, porta diferente): `SELECT s2` → `ROW1 [1][2][3]`, `ROW2 [4][5][6]`, `AuthenticationOk=1` → **persistência também através do pgwire**.
Leitura da **mesma** base por `sql_executa` directo, sem pgwire: `tag=SELECT 2`, `nrows=2`, `[1][2][3] [4][5][6]` → as duas superfícies partilham o mesmo estado em disco.

**Descoberta de contrato (nova, importante — vai para o §23/F4):** `sql_abrir`/`sql_fechar` **não fazem parte da biblioteca**. A única ocorrência de `sql_abrir` em `pgwire.c` está dentro do `main()` que `-DPGWIRE_NO_MAIN` exclui (e não existe em lado nenhum em `pgwire_sess.h`/`pgwire.h`/`pgmsg.h`). Consequência: **as 4 funções `pgwire_*` são só transporte**; quem consome tem de abrir a base. Sem `sql_abrir` antes de `pgwire_serve_conn`, o handshake completa e a sessão fecha limpa (`rc=0`), mas o `SELECT` devolve `SELECT 0` e **nada persiste** (só fica o ficheiro `__t.mem`). Isto é um **facto de contrato a documentar**, não um defeito — e não foi corrigido.

### G9 — Divisão runtime vs development (conceptual, sem `.deb`)

| Pacote (futuro) | Conteúdo | Motivo |
|---|---|---|
| `libtiffanydb1` (runtime) | `libtiffanydb.so.1.0.0` + symlinks `.so.1`, `.so` | O `.so` é a unidade de runtime; o `.a` não é preciso para correr. |
| `libtiffanydb-dev` (desenvolvimento) | `/usr/include/tiffanydb/{sql_api.h,pgwire_api.h}`, `libtiffanydb.a`, `libtiffanydb.so`, `pkgconfig/tiffanydb.pc`, `LICENSE`, `NOTICE` | Headers, link dinâmico de desenvolvimento, link estático, descriptor `pkg-config` e documentação legal. |

A divisão é coerente com G5/G6: o consumidor de runtime só precisa de `NEEDED libtiffanydb.so.1` + `libc.so.6`.

### G10 — Fora de âmbito (confirmado)

Nenhuma CLI, nenhum binário, nenhum serviço, nenhum ficheiro de configuração, nenhum `/etc`. O produto G é **exclusivamente** a biblioteca C. `debian/`, `Makefile`, `.deb` e APT **não** foram criados — ficam para a etapa H.

### G11 — Reprodutibilidade (duas construções totalmente limpas)

Três construções em directórios virgens (`build-baseline`, `build-rep1`, `build-rep2`), `SOURCE_DATE_EPOCH=1700000000`, flags de distribuição:

| Artefacto | baseline | rep1 | rep2 | Veredicto |
|---|---|---|---|---|
| `libtiffanydb.a` (581 220 B) | `5274f9b0…` | `5274f9b0…` | `5274f9b0…` | **idênticos byte-a-byte** |
| `libtiffanydb.so.1.0.0` (394 432 B) | `8a915609…` | `8a915609…` | `8a915609…` | **idênticos byte-a-byte** |

```text
5274f9b0808bb6d4a75bf0c7bce935d2c147ed1eaf0a0a5f18c943211dd48f1b  libtiffanydb.a
8a91560924cccecb901ea44dcfd180800fbbb7b8a553c57fdda963cb9e4b52ec  libtiffanydb.so.1.0.0
```

O que torna isto reprodutível: `-g0` (sem debug info), `-Wl,--build-id=none`, e `ar` no modo determinista por omissão do GNU binutils. Os directórios de saída e os de headers entram por caminho **absoluto**, pelo que o path não é gravado no artefacto (verificado: os três `.so` têm o mesmo sha256 apesar de construídos em directórios diferentes). **Nota de âmbito:** a reprodutibilidade foi medida **na mesma máquina, com o mesmo toolchain**. Não é uma compilação cruzada nem uma reconstrução bit-a-bit noutro host.

### G12 — Estado G1–G12

| Item | Estado | Evidência |
|---|---|---|
| **G1 — árvore mínima de fontes** | **FECHADO** | 2 TUs + 2 headers públicos + 37 privados; varredura de includes **fechada**; exclusões confirmadas. |
| **G2 — convenções de caminho** | **FECHADO** | `/usr/include/tiffanydb/`, `/usr/lib/x86_64-linux-gnu/`, `pkgconfig`; conferido contra Debian Policy 10.3.3 e contra o host real. |
| **G3 — build fora do repo** | **FECHADO** | `/root/tiffany-g` a partir de fontes copiadas; repo intocado; sem `-lm`; `.a` e `.so` ambos produzidos. |
| **G4 — flags fixas e sem magia** | **FECHADO** | Conjunto oficial medido (`-D_POSIX_C_SOURCE=200809L` obrigatório, `-D_DEFAULT_SOURCE` proibido na linha de comando); hardened link; avisos contabilizados (341/14). |
| **G5 — `.so`/`.a`/ABI** | **FECHADO** | NEEDED só `libc.so.6`; SONAME `libtiffanydb.so.1`; 32+4+3 e 0 fora; sem libm; sem `main`; **igual ao E2**. |
| **G6 — `DESTDIR` e árvore final** | **FECHADO** | 12 caminhos, 1,1 MB, nada fora do prefixo. |
| **G7 — `pkg-config`** | **FECHADO** | `tiffanydb.pc.in` verificado: cflags/libs/modversion correctos, `--static` sem `-lm`. |
| **G8 — consumidor externo** | **FECHADO** | SQL dinâmico (com persistência), link estático, e as **4** assinaturas `pgwire_*` com DDL/DML/SELECT e persistência; leitura cruzada das duas superfícies. |
| **G9 — runtime vs dev** | **FECHADO** | Divisão conceptual de 2 pacotes, alinhada com os ficheiros instalados. |
| **G10 — sem CLI** | **FECHADO** | Zero binários/serviços/configuração instalados. |
| **G11 — reprodutibilidade** | **FECHADO (no mesmo host)** | 3 construções limpas → **sha256 idêntico** nos dois artefactos. |
| **G12 — documento** | **FECHADO** | `§24` nesta proposta. |

**Estado global da etapa G: G1–G12: FECHADO** (G11 com a ressalva explícita de mesmo host/toolchain).

### Achados que passam para o contrato e para a etapa H

1. **`sql_abrir`/`sql_fechar` não pertencem à biblioteca** (só ao `main` excluído). A API `pgwire_*` é transporte puro; o ciclo de vida da base é do chamador. **A documentar em §23/F4.**
2. **`.bss` ≈ 2,95 MiB** — o RSS de qualquer consumidor cresce ~3 MB. **A documentar no `-dev`.**
3. **`pgwire_listen(0, …)` cai em 5432** — colide com o PostgreSQL real em muitos hosts. **A usar portas explícitas em toda a fase H.**
4. **341 + 14 avisos** com as flags oficiais (§22 não reproduz) e um `-Wformat` material em `core.c:4119`. **Nenhuma correcção feita** — a decidir na fase de desenvolvimento.
5. **`pkg-config`, `debhelper` e `makeinfo`** não estavam instalados no Pátria; `pkg-config` foi instalado por esta etapa; os outros dois são requisito da fase H.
6. `pkg-config --static --libs` **não** precisa de `-lpthread`/`-ldl`/`-lm` (o `.pc` foi escrito sem `Libs.private` depois de verificado o `nm`).

**Limpeza da etapa G (executada e verificada):** removidos `/root/tiffany-g`, `/root/g1.tgz`, `/root/LICENSE`, `/root/NOTICE`, `/tmp/tiffany-root`, `/tmp/tiffany-consumer`, `/tmp/g*.sh`, `/tmp/*.log` e os objectos temporários de link em `/tmp`. Verificado depois: `/root/tiffany-exp` (preexistente, 27/08) **intacto** (39 MB, com `banco`/`dados`/`lib`/`tests`/`tmp_up`/`tools`), **zero** `libtiffanydb*` em `/usr/lib/x86_64-linux-gnu`, `/usr/local/lib` e `ldconfig`, e os **5** clusters PostgreSQL do Pátria ainda a correr (não tocados). `pkg-config` fica instalado (é sistema e é requisito de H). Nada foi escrito no repositório excepto este `§24`; **sem commit e sem push**.

---

## 25. H — EMPACOTAMENTO DEBIAN V1 (executado 2026-09-30, Pátria Ubuntu 24.04.4, contentor Ubuntu 24.04.5)

### H0 — Ambiente de empacotamento (observado)

| Item | Valor medido |
|---|---|
| Build host | Pátria, Ubuntu 24.04 LTS, amd64, 4 CPU |
| Container de teste | `ubuntu:24.04` → `Ubuntu 24.04.5 LTS` |
| `gcc` | `13.3.0-6ubuntu2~24.04.1` |
| `ld` (binutils) | `2.42` |
| `debhelper` | `13.14.1ubuntu5` (compat 13) |
| `dpkg-dev` | `1.22.6ubuntu6.6` |
| `lintian` | `2.117.0ubuntu1.5` |
| Docker | cliente `29.1.3`, servidor `28.2.2` |

`debhelper`, `dpkg-dev` e `lintian` não estavam instalados (exigência de `§24`/achado 5) e foram instalados por esta etapa. `debuild` **não** foi instalado nem usado: `dpkg-buildpackage` chega e evita uma dependência que nada exige.

### H1 — Decisões de forma (fechadas antes de escrever uma linha)

| Decisão | Escolha | Razão |
|---|---|---|
| Source package | `libtiffanydb` | O nome do que se distribui. |
| Binários | `libtiffanydb1` + `libtiffanydb-dev` | Divisão runtime/desenvolvimento de `§23`/F6 e G9. |
| `Multi-Arch` | `same` em ambos | `§23`/F1 fixa o ABI 1. `same` é o correcto para uma biblioteca de C co-instalável. |
| Triplet | **nunca escrito à mão** | `dpkg-architecture -qDEB_HOST_MULTIARCH` nos flags; `usr/lib/*/` (glob do próprio dpkg) nos `.install`. |
| CLI | **não existe** | `§24`/G10. Zero binários, zero serviços, zero configuração. |
| `.a` | **distribuída**, no `-dev` | O `pkg-config --static` devolve `-ltiffanydb`; sem o `.a` isso seria promessa sem conteúdo. 581 KiB, mesmo `.a` que G8 exercitou num link estático real. |
| Headers | **só os dois públicos** | `sql_api.h` e `pgwire_api.h`. Os 37 privados ficam de fora de propósito: quem os inclui usa o interior, que não tem contrato. |
| `debhelper-compat` | 13 | Compatibilidade estável, `dh` com a sequência correcta. |
| Dependências | só `libc6` | Derivada, não escrita à mão (§H4). |

### H2 — O que entra em cada pacote (4 + 10 ficheiros, nada mais)

`libtiffanydb1` (4 ficheiros):

```
/usr/lib/x86_64-linux-gnu/libtiffanydb.so.1.0.0
/usr/lib/x86_64-linux-gnu/libtiffanydb.so.1        -> libtiffanydb.so.1.0.0
/usr/share/doc/libtiffanydb1/changelog.Debian.gz
/usr/share/doc/libtiffanydb1/copyright
```

`libtiffanydb-dev` (10 ficheiros):

```
/usr/include/tiffanydb/sql_api.h
/usr/include/tiffanydb/pgwire_api.h
/usr/lib/x86_64-linux-gnu/libtiffanydb.a
/usr/lib/x86_64-linux-gnu/libtiffanydb.so          -> libtiffanydb.so.1
/usr/lib/x86_64-linux-gnu/pkgconfig/tiffanydb.pc
/usr/share/doc/libtiffanydb-dev/{LICENSE.gz,NOTICE,README.gz,changelog.Debian.gz,copyright}
```

**Nenhum** `postinst`, `prerm`, serviço, directório de configuração, dados, exemplos, testes ou CLI.

### H3 — Artefactos medidos

| Artefacto | Bytes | sha256 |
|---|---|---|
| `libtiffanydb.a` | 584 316 | `89f395c6dc774b549be02a63706f9418cb77e7a9ba3119fa9b1813e886d7d52b` |
| `libtiffanydb.so.1.0.0` | 398 528 | `bea1b9cd28ccec0d92bdc4edb1cfc1e908a8fc633c4dba5b654c761190e70976` |
| `libtiffanydb1_1.0.0-1_amd64.deb` | 175 424 | `3dd557f6a7f3d5671bf5514a689c36e77c89491b9fe3e7b39b7931e7eae72227` |
| `libtiffanydb-dev_1.0.0-1_amd64.deb` | 202 842 | `69df455381bfb18bf6726bc1af74601cb54114e27c5300d51ddcc703bb06d7b4` |

`.so`: `.text` 281 813 B · `.data` 3 776 B · **`.bss` 3 091 416 B** · `SONAME libtiffanydb.so.1` · `NEEDED libc.so.6` · `BIND_NOW` · **39 símbolos exportados** (35 `sql_*` + 4 `pgwire_*`, sendo 3 `sql_*` dados BSS).

**`.bss` = 3 091 416 B, byte a byte o mesmo número medido em G11.** Não é coincidência: com o LTO desligado (§H4), o `.bss` coincide com o build G sem LTO. O `.text` é maior que o de G11 porque o empacotamento acrescenta o hardening da distro (§H4).

> **Ressalva de reprodutibilidade (honesta, e diferente da de G11).** Os hashes de G11 (`5274f9b0…` / `8a915609…`) **não** reaparecem aqui, e não devem: aquele build usou o conjunto de flags congelado de G4; este usa o mesmo conjunto **mais** o hardening que o Ubuntu injecta (`-fstack-protector-strong`, `-fstack-clash-protection`, `-D_FORTIFY_SOURCE=3`, `-fcf-protection`, `-ffile-prefix-map`, `-Wformat -Werror=format-security`). Código diferente, hash diferente — é o comportamento esperado e não é uma regresão. O que **é** verificável, e foi verificado, é a reprodutibilidade *desta receita* (§H5).

### H4 — Quatro defeitos de empacotamento encontrados e corrigidos

Nenhum destes é uma alteração ao núcleo. São todos de receita, e todos teriam chegado ao `.deb` se não tivessem sido medidos.

**(a) `-flto` entrava no `.deb` e o `filter-out` não o tirava.**
O Ubuntu 24.04 injecta `-flto=auto -ffat-lto-objects` em `CFLAGS` e **não há chave para o desligar**: mediram-se todas as formas de `DEB_BUILD_MAINT_OPTIONS` (`hardening=+all`, `lto=-`, `-lto`, `lto=-` isolado) e nenhuma o remove. Filtrar à mão parecia suficiente, e não era: o `dpkg-buildpackage` **exporta `CFLAGS` no ambiente antes de o `make` ler o `rules`**, o `make` importa variáveis do ambiente como suas, e um `CFLAGS +=` só acrescenta — o `-flto=auto` que se filtrava logo abaixo voltava pela porta do fundo e ficava no comando, porque aparecia primeiro. **A correcção é substituição, não acréscimo** (`override CFLAGS := …`), e há agora uma guarda que **falha o build** se `-flto` reaparecer nos flags ou se algum objecto sair com secções `.gnu.lto_`.

Porque é que isto importa, e não é wokenia: com LTO, o `.a` distribuído levava **1 258 400 bytes de IR de LTO** (1 843 716 B em vez de 584 316 B), e a optimização do produto passava a correr **dentro do compilador de quem consome**. O sintoma apareceu na primeira linkage estática a partir do pacote, com avisos do `sql.c` a emergir no build do consumidor — um aviso de `-Wstringop-overflow` em `merkle_pelo_fold` (`sql.c:2831`) que o consumidor nunca escreveu. Um `.a` é o artefacto que se instala para uso de terceiros; não pode conter trabalho por fazer.

**(b) Os symlinks não estavam em nenhum pacote.**
`dh_makeshlibs` **não cria symlinks** — só lê os símbolos para escrever o ficheiro `shlibs`. Com o `dh_install` a correr **antes** de `override_dh_makeshlibs` na sequência do `dh` (confirmado por `dh binary --no-act`), o `debian/tmp` já estava vazio quando o `dh_makeshlibs` procurava a biblioteca: não gerou ficheiro de símbolos, não gerou symlinks, e o pacote saiu **inutilizável** — `libtiffanydb.so.1` simplesmente não existia. Os dois symlinks passaram a ser criados em `install`, nome a nome, e a biblioteca é reposta em `debian/tmp` antes do `dh_makeshlibs`. Isto é *verificar em vez de supor*: nada aqui depende da posição de um passo na sequência.

**(c) O `libdir` do `pkg-config` estava errado.**
Gerava `-L/usr/x86_64-linux-gnu/lib` em vez de `-L/usr/lib/x86_64-linux-gnu`. A linkage só passava porque o `gcc` já procura `/usr/lib/x86_64-linux-gnu` por omissão — o `-L` estava a apontar para lado nenhum e a ser inócuo. Noutro prefixo, ou com `PKG_CONFIG_PATH` a correr, teria sido um `-L` inválido. Corrigido para `${prefix}/lib/$(MULTIARCH)`.

**(d) Faltava o `Maintainer` e o `copyright`.**
`debian/control` não trazia `Maintainer:` (vinha do changelog mas não chegava aos binários), e o `copyright` não era instalado em lado nenhum — o lintian classificava a falta como **erro**, correctamente: Policy 12.4 exige `/usr/share/doc/<pacote>/copyright`. A sequência do `dh` aqui não corre `dh_installdocs`, por isso o ficheiro é instalado à mão, **sem compressão** (como a Policy pede) e declarado nos dois `.install`.

**Sobre a `${shlibs:Depends}` do `-dev`:** removida, e com razão. `dh_shlibdeps` não a gera para o `-dev` porque esse pacote não contém nenhuma biblioteca partilhada real — só o `.a` e um symlink. Quem liga contra o `.a` resolve os símbolos com o seu próprio linker, e a dependência de execução é de `libtiffanydb1`, declarada ao lado. Deixá-la produzia `dpkg-gencontrol: warning: Depends field … substitution variable ${shlibs:Depends} used, but is not defined`.

### H5 — Metadados e verificação do conteúdo

```
libtiffanydb1     Version 1.0.0-1   Depends: libc6 (>= 2.33)              Multi-Arch: same   Installed-Size 388
libtiffanydb-dev  Version 1.0.0-1   Depends: libtiffanydb1 (= 1.0.0-1)    Multi-Arch: same   Installed-Size 614
```

`libc6 (>= 2.33)` **não está escrito à mão em lado nenhum**: vem do ficheiro `shlibs` que o `dh_makeshlibs` escreveu a partir do objecto medido, e o `dh_gencontrol` transformou em `shlibs:Depends`. Confirmação independente: o símbolo de glibc mais alto exigido pelo `.so` é `GLIBC_2.33` (`fstat@GLIBC_2.33`). É a dependência certa porque, se um dia o produto precisar de mais, isto acompanha sem ninguém se lembrar.

**Reprodutibilidade, medida como deve ser.** Duas construções totalmente limpas, do zero:

| Artefacto | Construção 1 | Construção 2 | |
|---|---|---|---|
| `libtiffanydb.a` | `89f395c6…` | `89f395c6…` | idênticos |
| `libtiffanydb.so.1.0.0` | `bea1b9cd…` | `bea1b9cd…` | idênticos |
| `libtiffanydb1_…amd64.deb` | `3dd557f6…` | `3dd557f6…` | idênticos |
| `libtiffanydb-dev_…amd64.deb` | `69df4553…` | `69df4553…` | idênticos |

Isto é mais forte do que a afirmação de G11, porque os **`.deb` completos** são byte-a-byte iguais, carimbos de data inclusive — `SOURCE_DATE_EPOCH` e `dh_strip_nondeterminism` a trabalhar. **Mesma máquina, mesmo toolchain** (a ressalva de G11 mantém-se).

### H6 — Lintian: 0 erros, 3 avisos, todos classificados

Erros encontrados e corrigidos: `no-copyright-file` em ambos os pacotes (×2). Estado final:

| Aviso | Classificação |
|---|---|
| `initial-upload-closes-no-bugs` (×2) | **Ignorável, por política da própria tag.** A documentação da tag diz, literalmente: *"This warning can be ignored if the package is not intended for Debian"*. É exactamente o caso: o produto é proprietário e a etapa I (APT) está bloqueada por acordo escrito. Não há ITP bug para fechar. |
| `distant-prerequisite-in-shlibs` | **Informativo, não é defeito.** O `shlibs` gerado é uma linha — `libtiffanydb 1 libtiffanydb.so.1` — que é exactamente o que Policy 8.6 manda declarar: só a biblioteca incluída neste pacote. O aviso dispara porque a biblioteca satisfaz um pré-requisito distante do `libc6` actual, o que é a consequência honesta de a biblioteca não usar nenhum símbolo de glibc recente. Não se corrige sem mudar o produto, e mudar o produto está fora de âmbito. |

**Zero erros. Nenhum aviso corrigido às cegas** — cada um ou se corrigiu, ou se classificou com a razão escrita.

### H7/H8 — Consumo real a partir do `.deb`, em contentor limpo

Contentor `ubuntu:24.04` (24.04.5) sem nada de Tiffany DB. Instalação por `apt-get install` dos dois `.deb`:

- Ambos instalam; `dpkg -s` confirma `Depends` e `Multi-Arch: same`.
- `dpkg -L` devolve exactamente a lista de H2, com os symlinks correctos: `libtiffanydb.so -> libtiffanydb.so.1 -> libtiffanydb.so.1.0.0`.
- `ldd libtiffanydb.so.1` → `libc.so.6` e mais nada. Sem libm, sem pthread, sem dl.
- `readelf -d` → `SONAME libtiffanydb.so.1`, `NEEDED libc.so.6`, `BIND_NOW`.

Consumidor externo compilado **só** com o que o `pkg-config` devolve:

```console
cc cons.c -o cons $(pkg-config --cflags --libs tiffanydb)     # rc=0
  --modversion   : 1.0.0
  --cflags       : -I/usr/include/tiffanydb
  --libs         : -ltiffanydb
  --libs --static: -ltiffanydb
```

Execução, com os valores de retorno conferidos um a um:

```console
ABRIR rc=1                              <- 1 = sucesso, como manda §23/F2
CREATE rc=1 tag=[CREATE TABLE] err=[]
INSERT rc=1 tag=[INSERT 0 1]    err=[]  (×2)
COUNT  rc=1 ncols=1 nrows=1 val=[2]
```

**Persistência, em processo novo** (a prova que interessa):

```console
# processo 1: escreve          →  2 linhas inseridas, count(*) = 2
# processo 2: NOVO processo, mesmo directório, só a abrir e a ler
SELECT rc=1 ncols=2 nrows=2
  linha 0: [1] [um]
  linha 1: [2] [dois]
```

**Isolamento:** noutro directório, a base é outra — `SELECT a FROM t` é **recusado** com `relation "t" does not exist` e `rc=0`, nada devolvido. Nenhuma base vê a outra.

**Ligação estática** a partir do `.a` instalado: também funciona, mesmo ciclo completo. E, depois da correcção de H4(a), **sem nenhum aviso do `sql.c` a vazar para o build do consumidor** — era o sintoma do LTO, e desapareceu com ele.

**Remoção:** `dpkg -r libtiffanydb-dev` e depois `libtiffanydb1` → **zero ficheiros** `tiffanydb` deixados em `/usr/lib/x86_64-linux-gnu`.

### H9 — Nada da árvore do repositório entrou por acidente

Duas camadas, porque uma só não chega:

1. **A lista é explícita.** Tudo o que entra num `.deb` passa por `override_dh_auto_install`, nome a nome. Não há **nenhuma** wildcard sobre a árvore do repositório — nem `find`, nem `cp -r`, nem wildcard de headers. Os únicos globs são `usr/lib/*/` nos `.install`, e esses resolvem dentro do `debian/tmp` que a própria receita construiu. `banco/sql.c` e `banco/pgwire.c` entram por caminho explícito; mais nada é compilado.
2. **O resultado foi verificado.** Varredura de todo o conteúdo extraído dos dois `.deb` contra `segredo|\.env|\.exe|\.pdf|\.tex$|medidor|mem\.dat|\.ttf|\.otf|\.gch|ed25519|patria|id_rsa`: **zero ocorrências**. Varredura textual por segredos (`BEGIN OPENSSH`, `PASSWORD`, `SECRET`, `TOKEN`, `APIKEY`): **zero**. A chave `segredo/id_ed25519_patria` nunca entrou no pacote nem na máquina de build.

### H10 — Documentação e licença

- `debian/copyright` em formato 1.0, `LicenseRef-Tiffany-License-2026-08`. **Nenhum SPDX inventado** — o produto não declara nenhum. O texto autoritativo é o `LICENSE`, instalado verbatim no `-dev`.
- A redistribuicao não é livre: o `LICENSE` exige acordo escrito separado. O `copyright` diz isso em vez de o esconder.
- O `README` do `-dev` documenta a superfície de 39 símbolos, o `1 = sucesso`, os 3 MiB de `.bss`, o **porto 5432** de `pgwire_listen(0, …)`, onde a base fica em disco, e a ponte opcional (abaixo). **Zero CLIs, zero serviços, zero configuração** — porque não existem.
- **H9/H12 e dados do utilizador:** os ficheiros que a base cria são dados de quem chama, não conteúdo de pacote. Remover o pacote não os toca, e nunca os tocará, porque nunca foram instalados.

### H11 — As quatro versões, e porque não se confundem

| Nome | Valor | O que é |
|---|---|---|
| `libtiffanydb.so.1` | — | A **ABI**. Não se incrementa por se criar um `.deb`. |
| `1.0.0` | produto | O que o `pkg-config --modversion` devolve, e o número real em `sql.c`. |
| `1.0.0-1` | Debian | Pacote = produto + revisão de empacotamento. |
| Formato persistente, protocolo | — | **Não versionados por esta receita**, e não Toques. |

### H12 — Onde a base fica (medido, não suposto)

`sql_abrir("minha_base")` resolve os caminhos **em relação ao directório de trabalho do processo**. Depois de um `CREATE` e dois `INSERT`, aparecem:

```
minha_base.mem         19 922 944 B    um por base
minha_base__t.mem      19 922 944 B    um por tabela
minha_base.prog                41 B    o programa compilado da última varredura
minha_base.undo         1 605 644 B    a pilha de desfazer
dados/sql_pool.bin          9 944 B    o pool de relações
dados/sql_rel.bin           8 192 B
```

Nenhum `.conf`, nenhum directório `<base>_corpo/` — as expectativas iniciais estavam erradas e ficam substituídas por esta medição. Um detalhe que merece atenção: os `.mem` ocupam **~19 MiB cada um**, muito acima dos ~3 MiB de `.bss`. O `.bss` é memória residente; o `.mem` é espaço pré-alocado em disco. São coisas diferentes e não devem ser comparadas.

### H13 — Três afirmações anteriores que a medição refutou

**1. `sql_abrir` e `sql_fechar` NÃO são "deixadas de fora da biblioteca". O `§24` está errado neste ponto.**
`§24` (achado 1) afirmava que só pertencem ao `main` excluído. **Medido:** ambas são declaradas em `sql_api.h`, definidas em `banco/sql.c:16142` e `banco/sql.c:16152`, **antes** do `#ifndef SQL_NO_MAIN` que começa em `banco/sql.c:16726` — e as duas estão entre os 39 símbolos exportados do `.so` entregue. Portanto: `§23`/F1 e este `§25` consideram-nas parte da API pública. *O `§24` não foi alterado* (por instruição); esta secção é a correcção registada.

**2. A ponte para bash/node/powershell NÃO é inalcançável a partir da API pública.**
Também medido, e por duas vias independentes:
- *Leitura do grafo:* `sql_executa` (`sql.c:16353`) → `executa` (`sql.c:15203`, chamada em `16716`) → dispatcher (`sql.c:15799-15801`) → `cmd_bash` / `cmd_powershell` / `cmd_node`.
- *Execução:* `sql_executa("BASH echo X")` devolve `rc=0` com `err=[BASH: use MOVE]` — a API **reconhece** a palavra e recusa a forma errada. Idêntico para `NODE` e `POWERSHELL`. As formas são `BASH MOVE …`, `NODE MOVE …`, `POWERSHELL MOVE …`.
- *O `.so` entregue:* `nm -D --undefined-only` mostra `popen@GLIBC_2.2.5`, `pclose@GLIBC_2.2.5`, `_exit@GLIBC_2.2.5` por resolver. A ponte está no artefacto.

O que continua verdadeiro: **SQL normal não precisa de nada disso.** Medido num contentor **sem node e sem powershell instalados** — a base abre, `CREATE`, `INSERT`, `SELECT` e contagens funcionam todas. Daí a decisão de não declarar dependência de node/bash/powershell: são capacidades opcionais, e quem as quiser traz o interpretador que precisar. Isto está escrito no `README` do `-dev`.

**3. `.bss` de G11 e o de hoje coincidem só porque o LTO ficou desligado.** Antes da correcção de H4(a), o build empacotado media 3 091 496 B; depois, 3 091 416 B — o valor de G11. São 80 bytes de `.bss` que eram do LTO, e a coincidência final é uma confirmação, não uma casualidade.

### H14 — Estado H1–H14

| Item | Estado | Evidência |
|---|---|---|
| **H1 — forma** | **FECHADO** | 2 binários, sem CLI, `Multi-Arch: same`, triplet de `dpkg-architecture`, compat 13. |
| **H2 — divisão runtime/dev** | **FECHADO** | 4 + 10 ficheiros, listados um a um; `.a` e headers no `-dev`, `.so` e SONAME no runtime. |
| **H3 — symlinks e layout** | **FECHADO** | `.so -> .so.1 -> .so.1.0.0` correctos nos pacotes (ver H4(b)). |
| **H4 — flags e LTO** | **FECHADO** | LTO desligado à mão e **verificado por guarda que falha o build**; restante hardening da distro ligado; `Depends` derivada, não escrita. |
| **H5 — conteúdo e reprodutibilidade** | **FECHADO** | `.a`/`.so`/`.deb` byte-a-byte iguais em duas construções limpas. |
| **H6 — lintian** | **FECHADO** | 0 erros; 3 avisos, cada um classificado com a razão. |
| **H7 — instalação limpa** | **FECHADO** | Contentor 24.04.5, `apt-get`, `dpkg -L`, `ldd`, `readelf`. |
| **H8 — consumidor externo** | **FECHADO** | `pkg-config` → compilar → CREATE/INSERT/SELECT → **persistência em processo novo** → isolamento → link estático → `dpkg -r` limpo. |
| **H9 — lixo de distribuição** | **FECHADO** | Lista explícita, zero wildcards sobre a árvore; varredura de ambos os `.deb`: zero ocorrências. |
| **H10 — documentação e licença** | **FECHADO** | `LicenseRef` sem SPDX inventado; restrição de redistribuição declarada; README factual. |
| **H11 — versões** | **FECHADO** | ABI / produto / Debian / formato, separados e explícitos. |
| **H12 — persistência e remoção** | **FECHADO** | Ficheiros medidos; `dpkg -r` não toca dados de quem chama. |
| **H13 — afirmações refutadas** | **FECHADO** | 3 correcções registadas acima. |
| **H14 — documento** | **FECHADO** | Este `§25`. |

**Estado global da etapa H: H1–H14: FECHADO.**

### O que fica para a etapa I, e o que a bloqueia

A biblioteca está empacotada, verificada e consumível de fora. **Não** há APT, nem repositório, nem publicação — e a razão não é técnica.

A licença (`Tiffany-License-2026-08`) **não é livre** e não concede redistribuição. A etapa I fica bloqueada até existir **acordo escrito separado**. Nada nesta etapa H dependeu dessa publicação: construiu, instalou e mediu tudo localmente, sob a franquia de verificação do `LICENSE`. Publicar os `.deb` num repositório é um acto diferente, com outros requisitos, e é uma decisão que não foi tomada aqui.

### Higiene desta etapa

Nada foi escrito no núcleo: `banco/` inalterado (**zero diff** nos 43 ficheiros versionados de `banco/`), nenhuma assinatura tocada, nenhuma ABI mexida, convenção `1 = sucesso` intacta, `stdout` e persistência como estavam. No repositório entraram apenas `debian/` (9 ficheiros) e este `§25`. **Sem commit e sem push.**

Uma nota para quem for versionar isto: o `debian/rules` tem de entrar no índice com o bit de executável (`git update-index --chmod=+x debian/rules`). Num repositório onde `core.autocrlf` está a `true` e não há `.gitattributes`, o Git no Windows não regista esse bit sozinho, e um `debian/rules` sem executável falha logo no `dpkg-buildpackage`. Nos builds desta etapa o bit foi posto à mão no contentor, e o `debian/rules` está por isso verificado como um ficheiro de texto normal — a receita funciona igual nos dois casos.

`core.autocrlf` está a `true` neste repositório e não há `.gitattributes`, por isso o *staging* foi exportado com `git -c core.autocrlf=false archive` para que os `debian/*` entrem em LF — os `.deb` não devem depender da plataforma de quem constrói. Verificado byte a byte nos 9 ficheiros: **zero CRLF**. Oito deles são ASCII puro; o nono, `debian/copyright`, tem quatro bytes acima de 127 que são o nome do autor em UTF-8, e são intencionais — é o nome certo.

---

## 26. I — FECHO DO PRÉ-APT E DA AUDITORIA (executado 2026-09-30, contentor Ubuntu 24.04)

> **Secção histórica. Não descreve o estado actual.**
>
> Este é o fecho do I0 tal como estava **antes** da etapa de higiene. As três
> pendências listadas em «Pendências que este fecho NÃO resolveu» foram todas
> tratadas depois: o número errado do `README` foi corrigido e os pacotes
> reconstruídos (build 4), e os dois ficheiros indevidos foram desversionados.
>
> **Para o estado vigente, ver `§27` e `RELEASE_MANIFEST_TIFFANY_DB_V1.md`.**
> O texto abaixo é preservado tal como foi escrito, para não reescrever o
> histórico depois de o ter medido.

```text
I0: FECHADO

Biblioteca: validada
Debian: validado
APT local: validado
Reprodutibilidade: validada nas builds realizadas
Secrets: auditados
Git/staging: auditados
Redistribuição pública: BLOQUEADA POR LICENÇA
```

### O que a etapa I fez, e o que não fez

Três coisas, e nada mais:

1. **Terceira construção, para fechar a lacuna que H deixou.** O `debian/README`
   mudou depois da medição de reprodutibilidade de H, o que mudou o `SHA-256`
   do pacote `-dev` (`cd92a82c…` → `69df4553…`). Esse valor final só tinha sido
   construído **uma** vez. Esta etapa reconstruiu a partir do `HEAD` versionado
   mais os nove ficheiros de `debian/`, e o resultado reproduziu byte a byte:
   `libtiffanydb1` `3dd557f6…` e `libtiffanydb-dev` `69df4553…`, com as
   bibliotecas da árvore de build idênticas (`.a` `89f395c6…`, `.so.1.0.0`
   `bea1b9cd…`). Nenhum fonte de `banco/` foi tocado: o que entra na build é o
   `HEAD` tal e qual.

2. **Repositório APT local, descartável, sem assinatura.** `apt-ftparchive` sobre
   o `pool/`, `Packages`/`Packages.gz`/`Release` gerados, `SHA256` e `Size`
   conferidos campo a campo contra os `.deb` reais, e depois um contentor
   `ubuntu:24.04` limpo a fazer `apt-get update`, `apt-get install`,
   `pkg-config`, consumidor ligado à partilhada e à estática, persistência em
   processo novo, e `remove --purge`. Não houve servidor web, endpoint público,
   `InRelease` nem `Release.gpg`: **nenhuma chave foi gerada**, e o cliente usou
   `trusted=yes` precisamente por isso. Contentor e repositório removidos no fim;
   os sete contentores de serviço pré-existentes não foram tocados.

3. **Auditoria de Git, staging e segredos.** Detalhada em
   `RELEASE_MANIFEST_TIFFANY_DB_V1.md`, §10.

O que **não** foi feito: nada publicado, nada hospedado, nada disponibilizado a
terceiros, nenhum repositório APT público, nenhuma chave GPG, nenhum commit,
nenhum push. Nenhuma fonte de produto foi alterada.

### Estado da redistribuição

I1 continua bloqueada, e o bloqueio é de licença, não técnico.

```text
STATUS DE DISTRIBUIÇÃO:
BLOQUEADO

MOTIVO:
Tiffany-License-2026-08 não concede redistribuição livre.
Distribuição pública ou redistribuição dos artefactos requer acordo escrito.
```

O `LICENSE` concede, na secção 3, uso gratuito de verificação enquanto o
repositório permanecer público. A secção 4 nega redistribuição fora disso. A
secção 5 diz que a **Via A** (adesão ao `CONTRATO-TIPO`) serve para uso próprio e
**não** concede redistribuir, e que a redistribuição é da **Via B** — acordo
escrito com o titular, que «é a única fonte da autorização». O
`debian/copyright` diz o mesmo em inglês e acrescenta que o acordo «does not yet
exist». O `NOTICE` confirma que não há material de terceiros incorporado, e a
varredura de segredos não encontrou nenhum — o que é consistente com essa
afirmação, sem ser prova de ausência.

### Medidas que este fecho corrigiu ou fixou

- **`.so` e `.a` dentro do `.deb` não são os mesmos ficheiros** que os da árvore
  de build: `dh_strip` e `dh_ar` processam a cópia instalada depois. As duas
  medidas ficaram registadas em separado no manifesto, para que ninguém compare
  um hash com o outro e conclua que a build não é reprodutível. Os hashes que
  identificam o que se distribui são os **de dentro** dos pacotes.
- **ABI reconfirmada sobre o pacote extraído**, não sobre a árvore de build:
  39 exports, 39 declarações nos dois headers instalados, zero de cada lado. Os
  três `extern long` (`sql_ultimos_passos`, `sql_ultimos_nos`, `sql_ultimo_prog`)
  são declarações sem parênteses e um extrator de símbolos ingenuo lê-las como
  se fossem exports sem declaração; não são.
- **`debian/rules` tem o bit de executável registado.** Com `core.autocrlf` a
  `true` e sem `.gitattributes`, o Git no Windows não o regista sozinho. Feito
  com `git update-index --add --chmod=+x`; o índice mostra `100755`. **Sem
  commit.**

### Pendências que este fecho NÃO resolveu

Três coisas ficam registadas e à espera de decisão de quem detém o repositório.
Nenhuma foi corrigida, porque nenhuma delas é segura de corrigir sem decisão:

1. **`debian/README` linha 65 tem um número errado.** Escreve `3091496` bytes
   para `.bss`. O valor medido é **3091416** bytes (`0x2f2bd8`), confirmado por
   `size -A` e por `readelf -S`, na árvore de build e no pacote extraído. É uma
   troca de dígitos. O texto errado está dentro de `libtiffanydb-dev`, em
   `README.gz`. **Não foi corrigido**: corrigir altera o `SHA-256` do pacote e
   obriga a uma quarta construção, e as hashes deste fecho são as medidas e
   confirmadas.

2. **Dois ficheiros que não deveriam estar no controlo de versões**, ambos
   pré-existentes e **nenhum dos dois dentro de qualquer `.deb`** (confirmado por
   `dpkg-deb -c`):
   - `lib/isa.h.gch` — 2242240 bytes, cabeçalho pré-compilado do GCC (magic
     `gpch`). É um binário gerado, versionado como se fosse fonte.
   - `banco/apps/soma.erg.fita.tmp` — 11 bytes, ficheiro temporário.
   Não foram removidos: **remoção é destrutiva** e a decisão é de quem tem o
   repositório. O *staging* desta etapa foi mais largo do que o produto por
   carregar `lib/` inteiro (150 ficheiros, dos quais 28 fontes `.otf`, 10
   `.js`/`.mjs` e 7 `.txt` que não entram na biblioteca), o que é por si só uma
   medida de higiene a melhorar na próxima etapa.

3. **Três avisos de `lintian` por resolver** — dois `initial-upload-closes-no-bugs`
   (resolve-se com um `Closes:` no changelog) e um
   `distant-prerequisite-in-shlibs` (resolve-se com a entrada oficial). Não são
   erros e não bloqueiam o fecho de I0.

### Higiene desta etapa

Nada foi escrito no núcleo: `git diff -- banco/` continua vazio nos 43 ficheiros
versionados, nenhuma assinatura tocada, nenhuma ABI mexida, convenção
`1 = sucesso` intacta, `stdout` e persistência como estavam. No repositório
entraram `RELEASE_MANIFEST_TIFFANY_DB_V1.md`, este `§26`, e o índice de Git com
`debian/rules` a `100755`. **Sem commit e sem push.**

O único ficheiro que mudou de estado fora do repositório foi o directório
temporário de I0 no host de build, removido depois do teste. O `segredo/` e o
`.env` foram verificados como ignorados pelo Git (`.gitignore:163` e `:164`) e
continuam por versionar: **nenhuma chave privada foi tocada, lida para o
manifesto, ou incluída em qualquer pacote.**

---

## 27. I0 PÓS-HIGIENE — BUILD 4 E BASELINE LIMPO (executado 2026-09-30)

```text
I0 pós-higiene: FECHADO

Artefatos: corrigidos
README: factual
Pacotes: reconstruídos
ABI: preservada
Núcleo: zero diff
Segredos: zero
APT público: NÃO PUBLICADO
Redistribuição: BLOQUEADA POR LICENÇA
```

O `§26` fechou o I0 e deixou três pontos reportados. Esta seção tratou os três.
Nenhum é uma pendência técnica aberta.

### 1. O `README` distribuía um número errado — corrigido

`debian/README:65` afirmava `.bss 3091496` bytes. A medição congelada em H,
reconfirmada na build 3 e outra vez na build 4, é **3091416** bytes
(`0x2f2bd8`), por `size -A` e por `readelf -S`, na árvore de build e no pacote
extraído. Uma troca de dígitos. Como o `README` entra em `libtiffanydb-dev` como
`README.gz`, o `.deb` afirmava uma coisa falsa sobre consumo de memória.

Alterado: uma linha, um número. Mais nada.

```diff
-debian/README:65   ... (medido no .deb: 3091496
+debian/README:65   ... (medido no .deb: 3091416
```

**Build 4.** Ambiente idêntico ao das três anteriores: gcc 13.3.0,
dpkg-dev 1.22.6ubuntu6.6, binutils 2.42, Ubuntu 24.04.5.

| Ficheiro | Tamanho | SHA-256 |
|---|---:|---|
| `libtiffanydb1_1.0.0-1_amd64.deb` | 175424 B | `3dd557f6a7f3d5671bf5514a689c36e77c89491b9fe3e7b39b7931e7eae72227` |
| `libtiffanydb-dev_1.0.0-1_amd64.deb` | 202950 B | `aae0c6559ea9063f86c6ed6c709691a7722cbb95524ab9cd67e8216cad96ef79` |

O **runtime é byte a byte idêntico** ao da build 3 — não contém o README, e
nada mais mudou nele. O `-dev` muda porque o `README.gz` muda. É a única
diferença de produto em quatro builds.

**Prova de que o núcleo não se mexeu.** Sete medições independentes:

| Medição | Resultado |
|---|---|
| `.so.1.0.0` dentro do pacote | `e273beb2…` (378408 B) — igual à build 3 |
| `.a` dentro do pacote | `79c19df3…` (584012 B) — igual à build 3 |
| `README.gz` dentro do `-dev` | 1× `3091416`, **0**× `3091496` |
| `.bss` no `.so` distribuído | 3091416 |
| exports / declarações | 39 / 39 |
| `SONAME` | `libtiffanydb.so.1` |
| `Version` | `1.0.0-1` nos dois; `NEEDED`: `libc.so.6` apenas |

### 2. Uma armadilha que vale registar

A primeira execução da build 4 levou `DEB_BUILD_OPTIONS=nostrip`, com a ideia de
manter o `dh_strip` explícito. O efeito é o contrário: `nostrip` **desliga** o
`dh_strip`. O `.deb` runtime cresceu 6068 B, e a `.so` dentro do pacote passou a
ser a versão **não** stripada — cujo `sha256` coincidia com o da árvore de
build. Parecia uma coincidência boa e era o sintoma do defeito.

Só o `lintian` o apanhou, a subir de 0 erros para
`E: libtiffanydb1: unstripped-binary-or-object`. A build foi refeita sem a
variável e voltou a 0 erros, com o runtime byte-idêntico à build 3. Registo
porque a falha se apresenta como sucesso.

### 3. Os dois ficheiros indevidos — desversionados

`lib/isa.h.gch` (2242240 B) e `banco/apps/soma.erg.fita.tmp` (11 B) saíram do
índice com `git rm --cached`, que **retira do controlo de versões e preserva o
ficheiro em disco**. Nada foi apagado. Antes e depois registado:

```
$ git ls-files --stage lib/isa.h.gch banco/apps/soma.erg.fita.tmp
100644 eba216629465c63db0efb70adf4435c010b98dd4 0	banco/apps/soma.erg.fita.tmp
100644 51c7a4995e347725c713cfd050db1fcdbc370640 0	lib/isa.h.gch

$ git ls-files --stage lib/isa.h.gch banco/apps/soma.erg.fita.tmp
(vazio)
```

Entraram no `.gitignore` (`/lib/*.gch`, `/banco/apps/*.tmp`) para não voltarem
com um `git add -A`. Os padrões são ancorados a `/lib/` e `/banco/apps/`, por
isso **não** atingem os quatro `.erg.fita.tmp` já versionados em
`conecthus/backends/` — confirmado.

**Antes de desversionar o `.gch`, medi-se se ele entrava na compilação. Não
entrava**, por duas provas independentes:

1. Os primeiros 8 bytes são `67 70 63 68 43 30 31 34` — `gpch` seguido de
   `C014`, ou seja um cabeçalho pré-compilado do **GCC 14**. A build usa
   **GCC 13.3**. O GCC 13 rejeita um `.gch` de outra versão.
2. `cc -E` com as CPPFLAGS reais do build produz saída **byte-idêntica** com o
   `.gch` presente e ausente (`cea03563d11ca9451d7b7ce38e6b3e87` nos dois casos).

`lib/isa.h` está na closure do produto; o `.gch` era um artefacto de outra
máquina e outra versão de compilador. A build 4 confirma empiricamente: as
bibliotecas saíram byte-idênticas.

### 4. A superfície de staging passou a ser derivada, não contada

Em vez de levar os 150 ficheiros versionados de `lib/` porque sim, a lista sai
do próprio compilador. `debian/rules` compila exactamente `banco/sql.c` e
`banco/pgwire.c` com `-D_POSIX_C_SOURCE=200809L -DSQL_NO_MAIN
-DPGWIRE_NO_MAIN`, e a closure transitiva do preprocessador **é** o que a
biblioteca precisa:

```
cc -D_POSIX_C_SOURCE=200809L -DSQL_NO_MAIN -DPGWIRE_NO_MAIN \
   -MM banco/sql.c banco/pgwire.c -Ibanco -Ilib
```

40 ficheiros: **31 headers de `lib/`**, 9 de `banco/`. `banco/pgwire_api.h` não
está na closure e foi acrescentado à mão — é instalado pelo pacote mas incluído
por ninguém, e um `-MM` cego tê-lo-ia omitido.

`lib/` precisa de 31 headers. **118 dos 149 versionados não são necessários:**
71 `.h` (fontes de domínio e de corpus), 28 `.otf`, 8 `.mjs`, 7 `.txt`, 2 `.js`,
2 `.c`. Nenhum entra no produto. Todos continuam no repositório.

```
staging de release:  52 ficheiros  (era 202)
  banco/   10   (closure + o header público instalado)
  debian/   9
  lib/     31   (closure do compilador)
  LICENSE   1
  NOTICE    1
```

Prova de que o estreitamento é seguro: o `.deb` runtime saiu byte-idêntico ao
da build 3. Se os 119 ficheiros removidos do staging tivessem afectado a
compilação, o hash teria mudado. Não mudou.

Um detalhe que teria partido a build: `debian/` foi enumerado do **disco**, não
de `git ls-files`, porque os nove ficheiros ainda não estão versionados e
`git ls-files debian` devolve só `debian/rules`. Uma lista de staging
construída a partir do índice teria enviado um pacote incompleto.

### 5. Verificações repetidas na build 4

Conteúdo dos dois pacotes enumerado com `dpkg-deb -c`; `lintian` com **0 erros**
e os mesmos 3 avisos; consumidor externo em contentor limpo a `CREATE`/`INSERT`/
`SELECT` ligado à partilhada; **processo novo** a fazer só `SELECT` e a ler o
que o processo anterior escreveu (persistência em disco, confirmada); consumidor
estático ligado à `.a` instalada, com `ldd` a mostrar dependência apenas de
`libc`; `apt-get remove --purge` a deixar o sistema limpo; `sql_abrir` e
`sql_fechar` presentes na API; varredura de segredos sobre o **conteúdo final
dos dois `.deb`** com 0 ocorrências e nenhum nome de ficheiro suspeito.

### 6. Limpeza do host

Removidos `/root/tiffany-i0` (14M), `/root/tiffany-i0-b4` (7.2M), o contentor
de teste e todos os scripts e staging temporários de I0 em `/tmp`.
`/root/tiffany-exp` ficou **intacto** (39M, 6 entradas) e os 7 contentores de
serviço continuaram com o mesmo estado. Nada de `libtiffanydb` instalado no
host.

Os dois `.deb` da build 4 foram copiados para fora do repositório
(`%LOCALAPPDATA%\Temp\opencode\i0-evidencia-b4\`, hashes reconferidos) para
que a evidência do artefacto exista sem que nenhum `.deb` entre no controlo de
versões.

### Higiene desta etapa

`git diff -- banco/` continua **vazio** nos 43 ficheiros versionados de
`banco/`. Nenhum fonte do núcleo alterado, nenhuma ABI mexida, nenhuma
assinatura tocada, convenção `1 = sucesso` intacta, `stdout` e persistência
como estavam. Versionados: `lib/isa.h.gch` e `banco/apps/soma.erg.fita.tmp`
retirados, `.gitignore` com 8 linhas novas em LF, `debian/rules` a `100755` no
índice. Não versionados ainda: os restantes 8 ficheiros de `debian/`,
`PROPOSTA_TIFFANY_DB.md` e `RELEASE_MANIFEST_TIFFANY_DB_V1.md`.

**Sem commit e sem push.**

### O que esta secção NÃO decide

Duas coisas são decisões de governança do repositório, não pendências técnicas,
e nenhuma foi tomada aqui:

1. Se os **118 ficheiros de `lib/` fora da closure** saem do controlo de
   versões. Não entram no produto — isso está feito e medido. Ficarem ou não no
   repositório é decisão de quem o detém.
2. Se os **8 ficheiros de `debian/`** entram num commit, com o `debian/rules`
   que já está preparado a `100755`.

### O que esta secção NÃO abre

**A licença não mudou.** `Tiffany-License-2026-08` continua a não conceder
redistribuição; a Via B continua a exigir acordo escrito que, segundo o
próprio `debian/copyright`, ainda não existe. Nenhuma correcção de documentação,
nenhuma desversionamento e nenhuma limpeza altera isso.

I1 pode ser investigada ou preparada. **Publicar não.**
Nenhuma chave GPG gerada. Nenhum repositório APT público.