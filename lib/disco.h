/* disco.h — COMO SE ESCREVE A MAQUINA SEM MEMORIA: o ficheiro E' o vector.
 *
 * A maquina esta' em code/maquina.c e no catalogo, §"A maquina sem memoria". Isto e' a
 * ferramenta que a torna possivel no resto do sistema.
 *
 * O Aarao: "promove em tudo, testa e garante 0 de memoria em todo o sistema, tudo no
 * disco." E antes: "disco e' barato, RAM e' cara."
 *
 * A RAZAO NAO E' O PRECO, E' QUE NAO HA' O QUE DISSIPAR.
 *
 * O Aarao: "o ssd e' feito de NANDs, o ssd nao dissipa; o bit nao dissipa, nao ha' o que
 * dissipar PORQUE NAO HA' O QUE CONSTRUIR."
 *
 * E' isso, e a diferenca esta' toda no verbo. A DRAM dissipa porque RECONSTROI: o refresh
 * e' ler-e-reescrever, milhares de vezes por segundo, o bit que ja' la' estava. Cada
 * reconstrucao passa por apagar, e apagar custa kT ln2 (~2,9e-21 J, medido em
 * laboratorio). O NAND nao reconstroi — a carga fica na floating gate sem alimentacao
 * nenhuma, e um bit parado nao custa nada porque nao esta' a ser feito.
 *
 * A dissipacao e' o preco de CONSTRUIR E DESTRUIR, nao de guardar. Por isso, nesta
 * dimensao, DISCO > MEMORIA — e nao como troca de custo: como ordem.
 *
 * ── O PADRAO ─────────────────────────────────────────────────────────────────────────
 *
 * Substitui-se
 *
 *     static unsigned buf[N];                  <- N*4 bytes em .bss, para sempre
 *
 * por
 *
 *     unsigned *buf = disco_u32("dados/buf.bin", N);     <- 0 bytes em .bss
 *
 * e o ponteiro vive na PILHA do main e passa-se por parametro. Nao fica global: um
 * ponteiro global sao 8 bytes em .bss, e 8 nao e' 0.
 *
 * O que isto faz e' mmap: o ficheiro no disco E' o vector, sem copia. Le-se e escreve-se
 * com o mesmo `buf[i]` de sempre — o kernel pagina o que for preciso e larga o resto. E
 * MAP_SHARED significa que o que se escreve JA' ESTA' no ficheiro: nao ha' "gravar no
 * fim", nao ha' o que perder se o processo morrer, e nao ha' arranque a carregar nada.
 *
 * ── O CUSTO, MEDIDO E NAO AFIRMADO ───────────────────────────────────────────────────
 *
 * Nesta maquina, com o ficheiro em page cache:
 *
 *     abre/fecha por acesso   5690 ns/passo      175 735 passos/s
 *     descritor aberto         902 ns/passo    1 109 078 passos/s
 *     o calculo em si          0% do tempo
 *
 * mmap nao tem nem uma coisa nem outra: o acesso e' um LOAD normal depois da primeira
 * falta de pagina. Quem disser que "o disco e' lento" que traga o numero — eu disse-o
 * sem numero nenhum e o que estava a medir eram os meus proprios fopen/fclose.
 *
 * ── AVISO ────────────────────────────────────────────────────────────────────────────
 *
 * O ficheiro nao encolhe sozinho: disco_u32 estende-o ao tamanho pedido e nunca o corta.
 * E o conteudo PERSISTE entre corridas — o que e' a vantagem, e e' tambem a armadilha se
 * o programa contava com .bss a zeros. Use disco_zera() quando quiser comecar limpo.
 */
/* ── POR FAZER: ISTO DEVIA ASSENTAR NO BANCO ────────────────────────────────────────
 *
 * O Aarao: "sao so' simbolos, poe no banco — a leitura e' simples, so' colocar no slot."
 *
 * As bases daqui sao um SEGUNDO mecanismo de disco, e o sistema ja' tem o primeiro:
 * banco/banco.c, com 65536 slots de 32 bytes {fp:8, off:8, len:8, crc:8} e escrita
 * atomica — grava o dado, fsync, e SO' ENTAO escreve o slot; um orfao e' inofensivo.
 *
 * O banco tem duas coisas que estas bases nao tem:
 *   - CRC por registo: um slot torto e' tratado como VAZIO, em vez de lido a' toa
 *   - a ordem de escrita pensada contra queda a meio
 *
 * O que falta para trocar: banco.c e' um PROGRAMA, e abrir/gravar/ler/fechar sao static
 * dentro dele. A ponte e' extrai-las para lib/banco.h e por DISCO_FIXO a assentar em
 * slots em vez de mmap. Mexe nos ~20 ficheiros ja' migrados e tem de passar a bateria
 * inteira — nao se faz de passagem, e partir o banco e' pior que ter 8 MB em .bss.
 *
 * E DEPOIS MEDIU-SE DUAS VEZES, e a segunda desfez a primeira. Fica o percurso, porque
 * a conclusao do meio esteve escrita aqui e estava errada:
 *
 *     banco por syscall (ler)     34 191,00 ns/acesso
 *     base  (mmap, um elemento)        1,25 ns/acesso
 *     banco MAPEADO                   11,05 ns/acesso     <- e com crc
 *
 * Com a primeira linha eu concluí que eram coisas diferentes que nao se substituem. Os
 * 34 mil nao eram do banco: eram do open/lseek/read A CADA LEITURA. Mapeado, ele da' AS
 * DUAS COISAS — a estrutura com crc E o acesso em nanossegundos.
 *
 * A ARQUITECTURA, ENTAO, E' UMA SO' E E' O BANCO:
 *
 *     banco_mapa   projecta o .idx e o .dat, uma vez
 *     banco_ver    o ponteiro DIRECTO para o registo, com o crc conferido
 *     banco_byte   o byte, com o gato desfeito SO' no par onde ele esta' — O(1)
 *
 * e o que estas bases fazem — mmap num endereco constante — e' o mesmo movimento sem a
 * estrutura: sem crc, sem comprimento, sem impressao. Servem enquanto os ~20 ficheiros
 * migrados nao passarem para banco_ver, e nao servem para mais nada.
 *
 * O QUE FALTA, e e' so' isto: trocar DISCO_FIXO por banco_ver nesses ficheiros, um a um,
 * com a bateria a correr entre cada um. Nao ha' decisao por tomar — ha' trabalho.
 */
/* ── ISTO NAO E' UM CORPO. E' UM MONTE DE BYTES COM NOME DE FICHEIRO ────────────────
 *
 * O Aarao: "cara, isso e' um corpo? na cadeia, no banco, nos slots?"
 *
 * E' — e por isso ISTO ESTA' A MEIO CAMINHO. As bases daqui tiram o vector do .bss e
 * poem-no num ficheiro mapeado, o que resolve a RAM e mais nada:
 *
 *     base    dados/fw_g.bin          bytes, e mais nada
 *     banco   {fp, off, len, crc}     impressao, lugar, tamanho e integridade
 *
 * Um corpo tem endereco na cadeia de slots, tem impressao que o identifica, tem
 * comprimento declarado e tem crc que recusa o torto. Um dados/fw_g.bin nao tem nada
 * disso — se um byte virar, ninguem sabe; se dois programas o quiserem, nao ha' quem os
 * ordene; e o relogio do banco nao o encontra porque ele nao esta' la'.
 *
 * E JA' NAO HA' DESCULPA: mediu-se hoje o banco MAPEADO a 11,05 ns/acesso COM crc,
 * contra os 34 191 ns por syscall que me tinham feito concluir que eram coisas
 * diferentes. Nao sao. O banco da' as duas.
 *
 * 37 ficheiros usam estas bases. A migracao que falta nao e' tirar mais KB do .bss —
 * e' TRAZER ESTES CORPOS PARA O BANCO: gravar/banco_ver em vez de DISCO_FIXO, com
 * impressao e crc, e a bateria a correr entre cada um.
 */
#ifndef BROCA_DISCO_H
#define BROCA_DISCO_H

/* mmap, ftruncate e o resto sao POSIX, e com -std=c99 estrito a libc esconde-os.
 * A bateria compila com -std=c99, e sem esta linha os tres primeiros ficheiros
 * migrados NAO COMPILARAM la' — eu tinha-os testado com -std=gnu99 e nao dei por
 * nada. Testar com flags diferentes das do sistema nao e' testar. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

/* ── O MESMO MOVIMENTO ONDE NAO HA' mmap ────────────────────────────────────────────
 *
 * O ficheiro E' o vector: e' isso que este cabecalho faz, e nao muda. O que muda entre
 * hospedeiros e' o NOME da chamada. Em POSIX e' mmap(MAP_SHARED); em Windows e'
 * CreateFileMapping + MapViewOfFile(FILE_MAP_ALL_ACCESS) sobre PAGE_READWRITE, que tem
 * a MESMA semantica: o que se escreve JA' ESTA' no ficheiro, sem "gravar" nenhum.
 *
 * Isto nao e' uma segunda implementacao do disco — e' a mesma, com o verbo do sistema.
 * Sem este ramo, os medidores que pedem o vector em disco nao correm fora de POSIX, e
 * ficam por correr — que foi exactamente o que aconteceu com o hopfield.c e o
 * neuronio.c antes de isto existir. */
#if defined(_WIN32)
  #include <windows.h>
  #include <io.h>
  #include <direct.h>
  #define DISCO_WIN 1
#else
  #include <unistd.h>
  #include <sys/mman.h>
#endif

/* ftruncate e POSIX e com -std=c99 ESTRITO a libc esconde-o. Definir
 * _POSIX_C_SOURCE aqui nao chega: quando este cabecalho e lido, a libc ja foi
 * incluida pelo .c e a macro nao tem efeito retroactivo. Declara-se o prototipo,
 * que nao depende de ordem nenhuma.
 *   (a bateria compila com -std=c99; eu testei com -std=gnu99 e os tres primeiros
 *    ficheiros migrados NAO COMPILARAM la. testar com flags diferentes das do
 *    sistema nao e testar.) */
#if !defined(DISCO_WIN) && !defined(__USE_XOPEN2K) && !defined(_GNU_SOURCE)
extern int ftruncate(int, off_t);
#endif

/* mapeia n elementos de `tam` bytes do ficheiro `path`. O ficheiro E' o vector. */
static void *disco_mapa(const char *path, size_t n, size_t tam)
{
    size_t bytes = n * tam;
    const char *b = strrchr(path, '/');
    if (b) {                                   /* garante a pasta, sem guardar nada */
        char dir[512];
        size_t k = (size_t)(b - path);
        if (k >= sizeof dir) k = sizeof dir - 1;
        memcpy(dir, path, k); dir[k] = 0;
#if defined(DISCO_WIN)
        _mkdir(dir);
#else
        mkdir(dir, 0755);
#endif
    }
#if defined(DISCO_WIN)
    /* o mesmo movimento: abrir, esticar ao tamanho, e mapear PARTILHADO */
    HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) { perror(path); exit(1); }
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(h, &sz)) { perror(path); CloseHandle(h); exit(1); }
    if ((size_t)sz.QuadPart < bytes) {         /* esticar: o ftruncate daqui */
        LARGE_INTEGER novo; novo.QuadPart = (LONGLONG)bytes;
        if (!SetFilePointerEx(h, novo, NULL, FILE_BEGIN) || !SetEndOfFile(h)) {
            perror(path); CloseHandle(h); exit(1);
        }
    }
    HANDLE m = CreateFileMappingA(h, NULL, PAGE_READWRITE,
                                  (DWORD)(((uint64_t)bytes) >> 32),
                                  (DWORD)(bytes & 0xFFFFFFFFu), NULL);
    if (!m) { perror(path); CloseHandle(h); exit(1); }
    void *p = MapViewOfFile(m, FILE_MAP_ALL_ACCESS, 0, 0, bytes);
    CloseHandle(m); CloseHandle(h);            /* a vista sobrevive aos handles */
    if (!p) { perror(path); exit(1); }
    return p;
#else
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) { perror(path); exit(1); }
    struct stat st;
    if (fstat(fd, &st) != 0) { perror(path); close(fd); exit(1); }
    if ((size_t)st.st_size < bytes && ftruncate(fd, (off_t)bytes) != 0) {
        perror(path); close(fd); exit(1);
    }
    void *p = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);                                 /* o mapa sobrevive ao descritor */
    if (p == MAP_FAILED) { perror(path); exit(1); }
    return p;
#endif
}

static unsigned      *disco_u32(const char *p, size_t n){ return (unsigned*)      disco_mapa(p,n,sizeof(unsigned)); }
static unsigned char *disco_u8 (const char *p, size_t n){ return (unsigned char*) disco_mapa(p,n,1); }
static uint64_t      *disco_u64(const char *p, size_t n){ return (uint64_t*)      disco_mapa(p,n,sizeof(uint64_t)); }
static int64_t       *disco_i64(const char *p, size_t n){ return (int64_t*)       disco_mapa(p,n,sizeof(int64_t)); }

/* comeca limpo — o disco persiste, e nem sempre e' isso que se quer */
/* ── disco_zera APAGA, E APAGAR E' A UNICA COISA QUE CUSTA ─────────────────────────
 *
 * O Aarao: "ajusta isso no barramento e elimina essa dissipacao de memoria — isso tende
 * a crescer, e muito."
 *
 * E cresce de maneira diferente da RAM: a RAM e' um TECTO (mede-se uma vez e fica), a
 * dissipacao e' um CAUDAL (paga-se em cada corrida, e soma para sempre). Medido no
 * repositorio: 32 chamadas com tamanho legivel apagam 4 505 712 bits POR CORRIDA — e a
 * conta de Landauer da' 1,29e-14 J de cada vez, que nao volta.
 *
 * O QUE ISTO CUSTA E' ZERO SE NAO SE APAGAR. Um vector que se vai escrever por inteiro
 * nao precisa de ser limpo antes: a escrita ja' o define. Limpar antes de escrever e'
 * apagar duas vezes o que se vai apagar uma.
 *
 *   disco_zera   quando o programa LE antes de escrever, e conta com zeros
 *   nada         quando o programa ESCREVE tudo antes de ler — e e' o caso comum
 *
 * Fica aqui a marca para que a escolha seja consciente e nao automatica. O tests/dissipa.c
 * mede o par: a mesma tarefa destrutiva apaga 131 072 bits, e a involutiva apaga ZERO. */
static void disco_zera(void *p, size_t n, size_t tam){ memset(p, 0, n*tam); }

/* a alternativa involutiva, para quando o que se quer e' TROCAR e nao APAGAR: aplicada
 * duas vezes devolve, e por isso nao apaga bit nenhum — nao ha' minimo termodinamico a
 * pagar, e nao e' "quase zero": e' zero exacto. */
static void disco_vira(void *p, size_t n, size_t tam, unsigned char m){
    unsigned char *b = (unsigned char*)p;
    for(size_t i = 0; i < n*tam; i++) b[i] ^= m;
}

/* devolve ao sistema; opcional, o fim do processo tambem o faz */
#if defined(DISCO_WIN)
static void disco_larga(void *p, size_t n, size_t tam){ (void)n; (void)tam; if(p) UnmapViewOfFile(p); }
#else
static void disco_larga(void *p, size_t n, size_t tam){ if(p) munmap(p, n*tam); }
#endif

/* ── ZERO ABSOLUTO: o endereco e' CONSTANTE, nao e' variavel ─────────────────────────
 *
 * `unsigned *buf = disco_u32(...)` guarda um ponteiro. Se for global sao 8 bytes em
 * .bss, e 8 nao e' 0. Passar por parametro resolve, mas obriga a mexer em todas as
 * funcoes que tocam o vector.
 *
 * Ha' uma terceira via, e e' a que respeita a doutrina a' letra: MAPEAR NUM ENDERECO
 * FIXO. O endereco passa a ser uma CONSTANTE DO PROGRAMA — nao ocupa .bss porque nao e'
 * uma variavel, e' um literal que o compilador poe na instrucao. Nada e' guardado, e o
 * sitio e' sabido. E' o mesmo movimento do A_0: nao guarda nada e mesmo assim leva la'.
 *
 *     #define CANVAS  DISCO_FIXO(unsigned, 0)          <- 0 bytes em .bss
 *     ...
 *     disco_prende(DISCO_BASE(0), "dados/canvas.bin", N, sizeof(unsigned));
 *
 * As bases ficam a 256 GiB de distancia umas das outras, bem acima do que o heap e as
 * bibliotecas usam, e MAP_FIXED_NOREPLACE faz o kernel RECUSAR em vez de calcar o que
 * la' estiver — se um dia colidir, falha alto em vez de corromper em silencio. */
#define DISCO_LAJE       0x0000200000000000UL      /* 32 TiB: fora do heap e das libs */
#define DISCO_PASSO      0x0000004000000000UL      /* 256 GiB entre bases             */
/* 256 GiB e nao 2 TiB: com 2 TiB a base 63 caia em 158 TiB, ACIMA do limite de
 * utilizador do x86-64 (128 TiB), e o mmap recusava. com 256 GiB a base 255 ainda
 * cabe. o kernel recusar foi a proteccao a funcionar — falhou alto, como devia. */
/* os parametros levam sufixo _ porque `i`, `T` e `D` sao nomes comuns e ja'
 * colidiram: um #define D 768 no ficheiro que inclui isto rebenta a macro. */
#define DISCO_BASE(i_)   ((void*)(DISCO_LAJE + (unsigned long)(i_)*DISCO_PASSO))
#define DISCO_FIXO(T_,i_)  ((T_*)DISCO_BASE(i_))
/* 2D sem tocar num unico acesso: um PONTEIRO PARA ARRAY mantem a sintaxe V[i][j].
 *     static int64_t V[MAXV][MAXD];   ->   #define V DISCO_FIXO2(int64_t, MAXD, k) */
#define DISCO_FIXO2(T_,cols_,i_)  ((T_(*)[cols_])DISCO_BASE(i_))

static void disco_prende(void *onde, const char *path, size_t n, size_t tam)
{
    size_t bytes = n * tam;
    const char *b = strrchr(path, '/');
    if (b) {
        char dir[512];
        size_t k = (size_t)(b - path);
        if (k >= sizeof dir) k = sizeof dir - 1;
        memcpy(dir, path, k); dir[k] = 0;
#if defined(DISCO_WIN)
        _mkdir(dir);
#else
        mkdir(dir, 0755);
#endif
    }
#if defined(DISCO_WIN)
    /* o mesmo prender, com o verbo do sistema: MapViewOfFileEx aceita o endereco
     * como SUGESTAO OBRIGATORIA — devolve NULL se nao puder por-lo ali. E a mesma
     * idempotencia do ramo POSIX: se ja' esta' prendido aqui, a segunda passagem
     * nao e' erro. */
    {
        HANDLE h = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                               OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) { perror(path); exit(1); }
        LARGE_INTEGER sz;
        if (!GetFileSizeEx(h, &sz)) { perror(path); CloseHandle(h); exit(1); }
        if ((size_t)sz.QuadPart < bytes) {
            LARGE_INTEGER novo; novo.QuadPart = (LONGLONG)bytes;
            if (!SetFilePointerEx(h, novo, NULL, FILE_BEGIN) || !SetEndOfFile(h)) {
                perror(path); CloseHandle(h); exit(1);
            }
        }
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(onde, &mbi, sizeof mbi) && mbi.State != MEM_FREE) {
            CloseHandle(h); return;            /* ja' prendido: segunda passagem */
        }
        HANDLE m = CreateFileMappingA(h, NULL, PAGE_READWRITE,
                                      (DWORD)(((uint64_t)bytes) >> 32),
                                      (DWORD)(bytes & 0xFFFFFFFFu), NULL);
        if (!m) { perror(path); CloseHandle(h); exit(1); }
        void *p = MapViewOfFileEx(m, FILE_MAP_ALL_ACCESS, 0, 0, bytes, onde);
        CloseHandle(m); CloseHandle(h);
        if (p != onde) {
            fprintf(stderr, "disco_prende: nao consegui %s em %p\n", path, onde);
            exit(1);
        }
        return;
    }
#else
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) { perror(path); exit(1); }
    struct stat st;
    if (fstat(fd, &st) != 0) { perror(path); close(fd); exit(1); }
    if ((size_t)st.st_size < bytes && ftruncate(fd, (off_t)bytes) != 0) {
        perror(path); close(fd); exit(1);
    }
#ifdef MAP_FIXED_NOREPLACE
    void *p = mmap(onde, bytes, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_FIXED_NOREPLACE, fd, 0);
    /* IDEMPOTENTE: se ja' esta' prendido aqui, nao e' erro — e' a segunda passagem.
     *
     * Sem isto, uma declaracao dentro de uma funcao CHAMADA EM CICLO rebenta a partir da
     * segunda volta, e a mensagem aponta para o endereco em vez de apontar para a causa.
     * Aconteceu-me duas vezes antes de eu perceber que era sempre a mesma coisa.
     *
     * E' seguro assumir que fomos nos: estas bases sao enderecos que mais ninguem usa, e
     * a alternativa — falhar — obriga quem escreve a lembrar-se de prender fora do ciclo,
     * que e' precisamente o tipo de coisa de que ninguem se lembra. */
    if (p == MAP_FAILED && errno == EEXIST) return;
#else
    void *p = mmap(onde, bytes, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_FIXED, fd, 0);
#endif
    close(fd);
    if (p == MAP_FAILED || p != onde) {
        fprintf(stderr, "disco_prende: nao consegui %s em %p (%s)\n", path, onde, strerror(errno));
        exit(1);
    }
#endif   /* DISCO_WIN */
}

#endif
