/* elf_escrever.c - ELF64 LSB x86-64 do alvo Linux: 3 PT_LOAD por permissao (R com os cabecalhos, RX, RW com memsz > filesz pelo .bss), PT_GNU_STACK RW,
 * e autoconferencia que devolve X0905 com o campo que nao bateu. Com pie (P41): ET_DYN base 0, PT_PHDR, PT_DYNAMIC e um PT_LOAD RW no fim com .rela.dyn (RELATIVE).
 * Versao: 0.4.0
 * Changelog: 0.4.0 - M6 P66: secoes de debug do programa (Imagem.dbg: .debug_abbrev, .debug_info, .debug_line, .debug_frame) sem SHF_ALLOC, depois dos PT_LOAD e antes do
 *                    .symtab (fora de todo PT_LOAD); sem --simbolos nao sai nenhuma secao .debug_* e o ELF sai byte a byte igual.
 *            0.3.0 - M5c-b P61: PT_TLS (p_offset/p_vaddr = inicio da secao TLS, filesz = .tdata, memsz = .tdata mais .tbss, p_align do bloco) e um cabecalho de programa
 *                    a mais so com TLS; a autoconferencia confere o PT_TLS. Sem TLS, o ELF sai byte a byte igual.
 *            0.2.0 - M5b P41 (--pie): ET_DYN com base 0, PT_PHDR, PT_DYNAMIC, PT_LOAD do .rela.dyn/.dynamic (DT_RELA, DT_RELASZ, DT_RELAENT, DT_FLAGS_1 = DF_1_PIE),
 *                    secoes .rela.dyn e .dynamic; a area dos cabecalhos de programa segue ELF_SEGMENTOS_EXE sem pie (o ET_EXEC sai byte a byte igual).
 *            0.1.0 - M5 P27b (D-M5-1): primeiro escritor de ELF. Os campos de Elf64_Ehdr/Phdr/Shdr/Sym ficam em TABELAS CONSTANTES (nome, offset e tamanho; um valor por indice),
 *                    lidas pelo escritor e pela autoconferencia, para uma traducao futura (G3). Offset de arquivo = RVA das secoes com bytes (as secoes ja nascem alinhadas a
 *                    0x1000 no ligador, entao offset congruente a vaddr sai de graca); o .bss sem .data antes dele nao leva bytes e o offset do segmento e a proxima pagina.
 *                    Cabecalhos de secao, .symtab (Imagem.simbolos), .strtab e .shstrtab so existem para readelf -S, objdump -d, gdb e perf: o kernel os ignora (custam
 *                    ~24 B por simbolo; nenhum e lido na carga).
 * Verificacao: construir.bat && python -I ..\tests\cli\verificar_cli.py --forja .\forja.exe; no WSL: readelf -h -l -S e file no ELF do 01_ola (gaveta\contrato_m5.txt, P27b)
 */
#include <stdio.h>
#include <string.h>

#include "nativo/fmt/elf.h"

#define ELF_TAM_EHDR 64u
#define ELF_TAM_PHDR 56u
#define ELF_TAM_SHDR 64u
#define ELF_TAM_SIM 24u
#define ELF_TAM_IDENT 16u
#define ELF_PAGINA 0x1000u
#define ELF_ALINHA_PILHA 16u
#define ELF_ALINHA_SECAO 16u
#define ELF_ALINHA_TABELA 8u
#define ELF_BITS_BYTE 8u
#define ELF_ET_EXEC 2u
#define ELF_ET_DYN 3u
#define ELF_EM_X86_64 62u
#define ELF_EV_CURRENT 1u
#define ELF_SHT_PROGBITS 1u
#define ELF_SHT_SYMTAB 2u
#define ELF_SHT_STRTAB 3u
#define ELF_SHT_NOBITS 8u
#define ELF_SHT_RELA 4u
#define ELF_SHT_DYNAMIC 6u
#define ELF_SHF_WRITE 1u
#define ELF_SHF_ALLOC 2u
#define ELF_SHF_EXEC 4u
#define ELF_STB_GLOBAL 1u
#define ELF_STT_NOTYPE 0u
#define ELF_STT_FUNC 2u
#define ELF_PRIMEIRO_GLOBAL 1u    /* sh_info do .symtab: o simbolo nulo e o unico local */
#define ELF_SECOES_EXTRAS 3u      /* .symtab, .strtab, .shstrtab */
#define ELF_SECOES_DINAMICAS 2u   /* pie: .rela.dyn e .dynamic (antes do .symtab) */
#define ELF_PT_PHDR 6u
#define ELF_PT_DYNAMIC 2u
#define ELF_PF_RW (ELF_PF_R | ELF_PF_W)
#define ELF_R_RELATIVE 8u         /* R_X86_64_RELATIVE: o campo recebe base + adendo */
#define ELF_TAM_RELA 24u          /* Elf64_Rela */
#define ELF_TAM_DT 16u            /* Elf64_Dyn */
#define ELF_TAM_QUAD 8u           /* um Elf64_Addr / Xword */
#define ELF_QTD_DT 5u             /* DT_RELA, DT_RELASZ, DT_RELAENT, DT_FLAGS_1, DT_NULL */
#define ELF_DT_NULL 0u
#define ELF_DT_RELA 7u
#define ELF_DT_RELASZ 8u
#define ELF_DT_RELAENT 9u
#define ELF_DT_FLAGS_1 0x6ffffffbu
#define ELF_DF_1_PIE 0x08000000u
#define ELF_PE_ESCRITA 0x80000000u /* IMAGE_SCN_MEM_WRITE: o ligador descreve as permissoes das secoes com as caracteristicas do PE */
#define ELF_MOTIVO_TAM 160

/* O que a Imagem diz de cada secao vira uma de tres permissoes, na ordem em que os segmentos ficam: so leitura, leitura + execucao, leitura + escrita. */
typedef enum { CLASSE_R, CLASSE_RX, CLASSE_RW, CLASSE_QTD } ElfClasse;

static const uint32_t ELF_FLAGS_SEGMENTO[CLASSE_QTD] = {ELF_PF_R, ELF_PF_R | ELF_PF_X, ELF_PF_R | ELF_PF_W};
static const uint32_t ELF_FLAGS_SECAO[CLASSE_QTD] = {ELF_SHF_ALLOC, ELF_SHF_ALLOC | ELF_SHF_EXEC, ELF_SHF_ALLOC | ELF_SHF_WRITE};

/* ---------- tabelas constantes dos cabecalhos (Elf64_*): nome, offset e tamanho de cada campo ---------- */

typedef struct { const char *nome; uint8_t off, tam; } ElfCampo;
typedef struct { const ElfCampo *campos; uint32_t qtd, tam; } ElfTabela;

enum { EH_TYPE, EH_MACHINE, EH_VERSION, EH_ENTRY, EH_PHOFF, EH_SHOFF, EH_FLAGS, EH_EHSIZE, EH_PHENTSIZE, EH_PHNUM, EH_SHENTSIZE, EH_SHNUM, EH_SHSTRNDX, EH_QTD };
enum { PH_TYPE, PH_FLAGS, PH_OFFSET, PH_VADDR, PH_PADDR, PH_FILESZ, PH_MEMSZ, PH_ALIGN, PH_QTD };
enum { SH_NAME, SH_TYPE, SH_FLAGS, SH_ADDR, SH_OFFSET, SH_SIZE, SH_LINK, SH_INFO, SH_ADDRALIGN, SH_ENTSIZE, SH_QTD };
enum { ST_NAME, ST_INFO, ST_OTHER, ST_SHNDX, ST_VALUE, ST_SIZE, ST_QTD };

static const ElfCampo ELF_EHDR[EH_QTD] = {
    [EH_TYPE] = {"e_type", 16, 2},       [EH_MACHINE] = {"e_machine", 18, 2},     [EH_VERSION] = {"e_version", 20, 4},    [EH_ENTRY] = {"e_entry", 24, 8},
    [EH_PHOFF] = {"e_phoff", 32, 8},     [EH_SHOFF] = {"e_shoff", 40, 8},         [EH_FLAGS] = {"e_flags", 48, 4},        [EH_EHSIZE] = {"e_ehsize", 52, 2},
    [EH_PHENTSIZE] = {"e_phentsize", 54, 2}, [EH_PHNUM] = {"e_phnum", 56, 2},     [EH_SHENTSIZE] = {"e_shentsize", 58, 2}, [EH_SHNUM] = {"e_shnum", 60, 2},
    [EH_SHSTRNDX] = {"e_shstrndx", 62, 2},
};
static const ElfCampo ELF_PHDR[PH_QTD] = {
    [PH_TYPE] = {"p_type", 0, 4},    [PH_FLAGS] = {"p_flags", 4, 4},    [PH_OFFSET] = {"p_offset", 8, 8}, [PH_VADDR] = {"p_vaddr", 16, 8},
    [PH_PADDR] = {"p_paddr", 24, 8}, [PH_FILESZ] = {"p_filesz", 32, 8}, [PH_MEMSZ] = {"p_memsz", 40, 8},  [PH_ALIGN] = {"p_align", 48, 8},
};
static const ElfCampo ELF_SHDR[SH_QTD] = {
    [SH_NAME] = {"sh_name", 0, 4},       [SH_TYPE] = {"sh_type", 4, 4},       [SH_FLAGS] = {"sh_flags", 8, 8},         [SH_ADDR] = {"sh_addr", 16, 8},
    [SH_OFFSET] = {"sh_offset", 24, 8},  [SH_SIZE] = {"sh_size", 32, 8},      [SH_LINK] = {"sh_link", 40, 4},          [SH_INFO] = {"sh_info", 44, 4},
    [SH_ADDRALIGN] = {"sh_addralign", 48, 8}, [SH_ENTSIZE] = {"sh_entsize", 56, 8},
};
static const ElfCampo ELF_SYM[ST_QTD] = {
    [ST_NAME] = {"st_name", 0, 4},   [ST_INFO] = {"st_info", 4, 1},   [ST_OTHER] = {"st_other", 5, 1},
    [ST_SHNDX] = {"st_shndx", 6, 2}, [ST_VALUE] = {"st_value", 8, 8}, [ST_SIZE] = {"st_size", 16, 8},
};

static const ElfTabela ELF_T_EHDR = {ELF_EHDR, EH_QTD, ELF_TAM_EHDR};
static const ElfTabela ELF_T_PHDR = {ELF_PHDR, PH_QTD, ELF_TAM_PHDR};
static const ElfTabela ELF_T_SHDR = {ELF_SHDR, SH_QTD, ELF_TAM_SHDR};
static const ElfTabela ELF_T_SYM = {ELF_SYM, ST_QTD, ELF_TAM_SIM};

/* e_ident: 0x7f "ELF", ELFCLASS64, ELFDATA2LSB, EV_CURRENT, ELFOSABI_SYSV, versao da ABI 0, preenchimento. */
static const uint8_t ELF_IDENT[ELF_TAM_IDENT] = {0x7f, 'E', 'L', 'F', 2, 1, 1, 0};

/* ---------- bytes ---------- */

static uint64_t ElfAlinhar(uint64_t v, uint64_t alinhamento)
{
    return (v + alinhamento - 1) / alinhamento * alinhamento;
}

static void ElfPoe(uint8_t *reg, const ElfCampo *c, uint64_t v)
{
    for (uint32_t k = 0; k < c->tam; k++)
        reg[c->off + k] = (uint8_t)(v >> (k * ELF_BITS_BYTE));
}

static uint64_t ElfLe(const uint8_t *reg, const ElfCampo *c)
{
    uint64_t v = 0;
    for (uint32_t k = c->tam; k > 0; k--)
        v = (v << ELF_BITS_BYTE) | reg[c->off + k - 1];
    return v;
}

/* Um registro da tabela: ident (so no Ehdr) e depois um valor por campo, na ordem dos indices. */
static void ElfAnexarRegistro(Buf *b, const ElfTabela *t, const uint64_t *valores)
{
    uint8_t reg[ELF_TAM_EHDR] = {0};

    if (t == &ELF_T_EHDR)
        memcpy(reg, ELF_IDENT, ELF_TAM_IDENT);
    for (uint32_t k = 0; k < t->qtd; k++)
        ElfPoe(reg, &t->campos[k], valores[k]);
    BufAnexarN(b, (const char *)reg, t->tam);
}

static void ElfPreencher(Buf *b, uint64_t ate)
{
    static const char zeros[ELF_PAGINA] = {0};

    while (b->tam < ate)
        BufAnexarN(b, zeros, ate - b->tam < sizeof zeros ? (size_t)(ate - b->tam) : sizeof zeros);
}

/* ---------- plano: classes, offsets e segmentos ---------- */

/* Inicio do PT_LOAD do .rela.dyn/.dynamic (pie): a pagina seguinte a ultima secao da Imagem. O ligador usa o mesmo valor para _DYNAMIC. */
uint64_t ElfInicioDinamico(const Imagem *im)
{
    uint64_t fim = 0;

    for (uint32_t i = 0; i < im->qtd_secoes; i++) {
        uint64_t f = im->secoes[i].rva + im->secoes[i].tam_virtual;
        if (f > fim)
            fim = f;
    }
    return ElfAlinhar(fim, ELF_PAGINA);
}

static ElfClasse ElfClasseDe(const PeSecao *s)
{
    if ((s->caracteristicas & PE_SECAO_CODIGO) != 0)
        return CLASSE_RX;
    return (s->caracteristicas & ELF_PE_ESCRITA) != 0 ? CLASSE_RW : CLASSE_R;
}

typedef struct {
    ElfClasse anterior;
    bool sem_bytes;    /* a secao anterior da mesma classe nao leva bytes */
} ElfOrdem;

static const char *ElfConferirOrdem(const PeSecao *s, ElfOrdem *o)
{
    ElfClasse c = ElfClasseDe(s);

    if (c < o->anterior)
        return "secoes fora da ordem R, RX, RW";
    if (c != o->anterior)
        o->sem_bytes = false;
    if (s->bytes.tam > 0 && o->sem_bytes)
        return "secao com bytes depois de secao sem bytes";
    o->sem_bytes = o->sem_bytes || s->bytes.tam == 0;
    o->anterior = c;
    return NULL;
}

/* As classes em ordem crescente, e dentro de cada uma as secoes com bytes antes das sem bytes (o .bss e o fim do segmento). */
static const char *ElfOrdenar(const Imagem *im)
{
    ElfOrdem o = {CLASSE_R, false};

    for (uint32_t i = 0; i < im->qtd_secoes; i++) {
        const char *motivo = ElfConferirOrdem(&im->secoes[i], &o);
        if (motivo != NULL)
            return motivo;
    }
    return NULL;
}

typedef struct {
    uint64_t pos;          /* fim do conteudo gravado ate aqui */
    bool seg_tem_bytes;    /* o segmento da secao corrente ja tem bytes no arquivo */
    ElfClasse classe;
} ElfCursor;

/* Offset de arquivo da secao: o RVA se tem bytes; sem bytes, o fim do conteudo (ou a proxima pagina se o segmento ainda nao tem bytes). */
static const char *ElfPosicionar(const PeSecao *s, ElfCursor *cur, uint64_t *off)
{
    ElfClasse c = ElfClasseDe(s);

    if (c != cur->classe) {
        cur->classe = c;
        cur->seg_tem_bytes = false;
    }
    if (s->bytes.tam == 0) {
        *off = cur->seg_tem_bytes ? cur->pos : ElfAlinhar(cur->pos, ELF_PAGINA);
        cur->pos = *off;
        return NULL;
    }
    if (s->rva < cur->pos)
        return "secao sobrepoe os cabecalhos ou a secao anterior";
    *off = s->rva;
    cur->pos = s->rva + s->bytes.tam;
    cur->seg_tem_bytes = true;
    return NULL;
}

/* O .dynamic (5 entradas, primeiro: _DYNAMIC = inicio dele, conhecido antes de contar as relocacoes) e o .rela.dyn (24 B por ADDR64 vivo) num PT_LOAD RW so no fim do arquivo, em pagina propria. */
static void ElfPlanejarDinamico(const Imagem *im, ElfPlano *p)
{
    p->va_dyn = ElfInicioDinamico(im);
    p->off_dyn = ElfAlinhar(p->fim_secoes, ELF_PAGINA);
    p->tam_rela = (uint64_t)im->relativos.n * ELF_TAM_RELA;
    p->tam_dyn = ELF_QTD_DT * ELF_TAM_DT;
    p->fim_secoes = p->off_dyn + p->tam_rela + p->tam_dyn;
}

static const char *ElfDistribuir(const Imagem *im, ElfPlano *p)
{
    uint32_t reserva = (im->pie ? ELF_SEGMENTOS_PIE : ELF_SEGMENTOS_EXE) + (im->tls ? 1u : 0u);
    ElfCursor cur = {.pos = ELF_TAM_EHDR + (uint64_t)ELF_TAM_PHDR * reserva, .classe = CLASSE_R};

    for (uint32_t i = 0; i < im->qtd_secoes; i++) {
        const char *motivo = ElfPosicionar(&im->secoes[i], &cur, &p->off_secao[i]);
        if (motivo != NULL)
            return motivo;
    }
    p->fim_secoes = cur.pos;
    if (im->pie)
        ElfPlanejarDinamico(im, p);
    return NULL;
}

typedef struct { int32_t ini, fim; } ElfFaixa; /* indices em Imagem.secoes; -1 = nenhuma secao da classe */

static void ElfAcharFaixas(const Imagem *im, ElfFaixa faixas[CLASSE_QTD])
{
    for (int c = 0; c < CLASSE_QTD; c++)
        faixas[c] = (ElfFaixa){-1, -1};
    for (uint32_t i = 0; i < im->qtd_secoes; i++) {
        ElfFaixa *f = &faixas[ElfClasseDe(&im->secoes[i])];
        if (f->ini < 0)
            f->ini = (int32_t)i;
        f->fim = (int32_t)i;
    }
}

/* pie (P41): o carregador grava base + adendo nos campos RELATIVE, e eles podem estar no .rodata (ponteiros de tabela de libc): o segmento de
 * leitura do pie ganha escrita (como o RELRO, sem o mprotect). Sem pie, as flags sao as de sempre. */
static uint32_t ElfFlagsDoSegmento(const Imagem *im, ElfClasse c)
{
    if (im->pie && c == CLASSE_R)
        return ELF_PF_R | ELF_PF_W;
    return ELF_FLAGS_SEGMENTO[c];
}

/* O segmento da classe: o de so leitura comeca no offset 0 (leva os cabecalhos); filesz acaba no ultimo byte gravado, memsz no fim da ultima secao. */
static ElfSegmento ElfSegmentoLoad(const Imagem *im, const ElfPlano *p, ElfFaixa f, ElfClasse c)
{
    const PeSecao *ini = &im->secoes[f.ini], *fim = &im->secoes[f.fim];
    uint64_t base_rva = c == CLASSE_R ? 0 : ini->rva;
    uint64_t base_off = c == CLASSE_R ? 0 : p->off_secao[f.ini];
    uint64_t fim_arquivo = base_off;

    for (int32_t i = f.ini; i <= f.fim; i++) {
        if (im->secoes[i].bytes.tam > 0)
            fim_arquivo = p->off_secao[i] + im->secoes[i].bytes.tam;
    }
    return (ElfSegmento){ELF_PT_LOAD, ElfFlagsDoSegmento(im, c), base_off, PeBaseDaImagem(im) + base_rva, fim_arquivo - base_off,
                         fim->rva + fim->tam_virtual - base_rva, ELF_PAGINA};
}

/* pie: o PT_LOAD do .rela.dyn/.dynamic e o PT_DYNAMIC (depois dos PT_LOAD das classes; o PT_PHDR ja abriu a lista e o PT_GNU_STACK fecha). */
static void ElfSegmentosDinamicos(ElfPlano *p)
{
    uint64_t tam = p->tam_rela + p->tam_dyn;

    p->seg[p->qtd++] = (ElfSegmento){ELF_PT_LOAD, ELF_PF_RW, p->off_dyn, p->va_dyn, tam, tam, ELF_PAGINA};
    p->seg[p->qtd++] = (ElfSegmento){ELF_PT_DYNAMIC, ELF_PF_RW, p->off_dyn, p->va_dyn, p->tam_dyn, p->tam_dyn, ELF_ALINHA_TABELA};
}

/* PT_TLS (M5c-b P61): o bloco que o musl copia para cada thread. Comeca na secao TLS (.tdata, com bytes), filesz so os bytes dela; memsz inclui o .tbss,
 * que e a cauda da mesma secao (zeros na memoria, sem bytes no arquivo). p_align = o maior alinhamento dos membros. */
static ElfSegmento ElfSegmentoTls(const Imagem *im, const ElfPlano *p)
{
    const PeSecao *s = &im->secoes[im->tls_secao];

    return (ElfSegmento){ELF_PT_TLS, ELF_PF_R | ELF_PF_W, p->off_secao[im->tls_secao], PeBaseDaImagem(im) + s->rva, s->bytes.tam, s->tam_virtual,
                         im->tls_alinhamento};
}

const char *ElfPlanejar(const Imagem *im, ElfPlano *p)
{
    ElfFaixa faixas[CLASSE_QTD];
    const char *motivo;

    memset(p, 0, sizeof *p);
    p->pie = im->pie;
    p->tls = im->tls;
    if (im->qtd_secoes == 0)
        return "imagem sem secoes";
    motivo = ElfOrdenar(im);
    if (motivo == NULL)
        motivo = ElfDistribuir(im, p);
    if (motivo != NULL)
        return motivo;
    ElfAcharFaixas(im, faixas);
    if (p->pie)
        p->seg[p->qtd++] = (ElfSegmento){ELF_PT_PHDR, ELF_PF_R, ELF_TAM_EHDR, ELF_TAM_EHDR, 0, 0, ELF_ALINHA_TABELA};
    for (int c = 0; c < CLASSE_QTD; c++) {
        if (faixas[c].ini >= 0)
            p->seg[p->qtd++] = ElfSegmentoLoad(im, p, faixas[c], (ElfClasse)c);
    }
    if (p->pie)
        ElfSegmentosDinamicos(p);
    if (p->tls)
        p->seg[p->qtd++] = ElfSegmentoTls(im, p);
    p->seg[p->qtd++] = (ElfSegmento){ELF_PT_GNU_STACK, ELF_PF_R | ELF_PF_W, 0, 0, 0, 0, ELF_ALINHA_PILHA}; /* pilha sem execucao */
    if (p->pie) /* o PT_PHDR tem o tamanho da tabela inteira, ele mesmo e o GNU_STACK incluidos */
        p->seg[0].filesz = p->seg[0].memsz = (uint64_t)p->qtd * ELF_TAM_PHDR;
    return NULL;
}

/* ---------- .symtab, .strtab, .shstrtab e cabecalhos de secao (so para as ferramentas; o kernel nao os le) ---------- */

typedef struct {
    Buf sim, str, shstr;                          /* conteudo de .symtab, .strtab e .shstrtab */
    uint32_t nome_sec[IMAGEM_MAX_SECOES];         /* sh_name de cada secao da Imagem */
    uint32_t nome_sim, nome_str, nome_shstr;
    uint64_t off_sim, off_str, off_shstr, off_sh; /* onde cada um cai no arquivo */
    uint32_t nsh;                                 /* cabecalhos de secao: o nulo, as da Imagem, as 3 extras e (pie) as 2 dinamicas */
    uint32_t ndyn, nome_rela, nome_dyn;           /* pie: quantas secoes dinamicas (0 ou 2) e os sh_name delas */
    uint32_t link_str;                            /* indice do .strtab: o sh_link do .dynamic (o readelf exige uma tabela de strings la) */
    uint64_t off_dbg[IMAGEM_QTD_DEBUG];           /* M6 P66: onde cada secao de debug presente cai no arquivo */
    uint32_t nome_dbg[IMAGEM_QTD_DEBUG];          /* M6 P66: sh_name de cada secao de debug presente */
    uint32_t ndbg;                                /* M6 P66: quantas secoes de debug presentes (entram antes do .symtab) */
} ElfExtras;

static const char *ElfNomeDaSecao(const PeSecao *s)
{
    switch (ElfClasseDe(s)) {
    case CLASSE_R: return ".rodata";
    case CLASSE_RX: return ".text";
    default: return s->bytes.tam > 0 ? ".data" : ".bss";
    }
}

static uint32_t ElfNome(Buf *tab, const char *nome)
{
    uint32_t off = (uint32_t)tab->tam;

    BufAnexarN(tab, nome, strlen(nome) + 1);
    return off;
}

/* ---------- secoes de debug do programa (M6 P66, D-M6-4): nao alocadas; so as presentes (Imagem.dbg[k].tam > 0) ---------- */
static const char *const ELF_NOME_DBG[IMAGEM_QTD_DEBUG] = {".debug_abbrev", ".debug_info", ".debug_line", ".debug_frame"};

static void ElfDbgNomes(const Imagem *im, ElfExtras *x)
{
    for (uint32_t k = 0; k < IMAGEM_QTD_DEBUG; k++) {
        if (im->dbg[k].tam > 0)
            x->nome_dbg[k] = ElfNome(&x->shstr, ELF_NOME_DBG[k]);
    }
}

/* Cada secao de debug presente cai logo depois de fim; devolve o fim delas (o .symtab alinha depois). */
static uint64_t ElfDbgPlanejar(const Imagem *im, uint64_t fim, ElfExtras *x)
{
    x->ndbg = 0;
    for (uint32_t k = 0; k < IMAGEM_QTD_DEBUG; k++) {
        if (im->dbg[k].tam == 0)
            continue;
        x->off_dbg[k] = fim;
        fim += im->dbg[k].tam;
        x->ndbg++;
    }
    return fim;
}

static void ElfEscreverDbg(Buf *b, const Imagem *im, const ElfExtras *x)
{
    for (uint32_t k = 0; k < IMAGEM_QTD_DEBUG; k++) {
        if (im->dbg[k].tam == 0)
            continue;
        ElfPreencher(b, x->off_dbg[k]);
        BufAnexarN(b, im->dbg[k].dados, im->dbg[k].tam);
    }
}

static void ElfAnexarShdrDbg(Buf *b, const Imagem *im, const ElfExtras *x)
{
    for (uint32_t k = 0; k < IMAGEM_QTD_DEBUG; k++) {
        uint64_t sh[SH_QTD] = {[SH_NAME] = x->nome_dbg[k], [SH_TYPE] = ELF_SHT_PROGBITS, [SH_OFFSET] = x->off_dbg[k], [SH_SIZE] = im->dbg[k].tam, [SH_ADDRALIGN] = 1};

        if (im->dbg[k].tam > 0) /* flags 0: sem SHF_ALLOC */
            ElfAnexarRegistro(b, &ELF_T_SHDR, sh);
    }
}

static void ElfMontarNomes(const Imagem *im, ElfExtras *x)
{
    BufAnexarN(&x->shstr, "", 1);
    for (uint32_t i = 0; i < im->qtd_secoes; i++)
        x->nome_sec[i] = ElfNome(&x->shstr, ElfNomeDaSecao(&im->secoes[i]));
    x->nome_sim = ElfNome(&x->shstr, ".symtab");
    x->nome_str = ElfNome(&x->shstr, ".strtab");
    x->nome_shstr = ElfNome(&x->shstr, ".shstrtab");
    ElfDbgNomes(im, x);
    if (im->pie) {
        x->nome_rela = ElfNome(&x->shstr, ".rela.dyn");
        x->nome_dyn = ElfNome(&x->shstr, ".dynamic");
    }
}

static void ElfAnexarSimbolo(const Imagem *im, ElfExtras *x, const ImagemSimbolo *s)
{
    uint32_t tipo = ElfClasseDe(&im->secoes[s->secao]) == CLASSE_RX ? ELF_STT_FUNC : ELF_STT_NOTYPE;
    uint64_t v[ST_QTD] = {[ST_NAME] = ElfNome(&x->str, s->nome), [ST_INFO] = (ELF_STB_GLOBAL << 4) | tipo,
                          [ST_SHNDX] = s->secao + 1, [ST_VALUE] = PeBaseDaImagem(im) + s->rva};

    ElfAnexarRegistro(&x->sim, &ELF_T_SYM, v);
}

static void ElfMontarSimbolos(const Imagem *im, ElfExtras *x)
{
    uint64_t nulo[ST_QTD] = {0};

    ElfAnexarRegistro(&x->sim, &ELF_T_SYM, nulo);
    BufAnexarN(&x->str, "", 1);
    for (size_t k = 0; k < im->simbolos.n; k++)
        ElfAnexarSimbolo(im, x, VecEm(&im->simbolos, k));
}

static void ElfMontarExtras(const Imagem *im, const ElfPlano *p, ElfExtras *x)
{
    ElfMontarNomes(im, x);
    ElfMontarSimbolos(im, x);
    x->off_sim = ElfAlinhar(ElfDbgPlanejar(im, p->fim_secoes, x), ELF_ALINHA_TABELA);
    x->off_str = x->off_sim + x->sim.tam;
    x->off_shstr = x->off_str + x->str.tam;
    x->off_sh = ElfAlinhar(x->off_shstr + x->shstr.tam, ELF_ALINHA_TABELA);
    x->ndyn = im->pie ? ELF_SECOES_DINAMICAS : 0;
    x->link_str = im->qtd_secoes + 2 + x->ndyn + x->ndbg;
    x->nsh = 1 + im->qtd_secoes + x->ndyn + x->ndbg + ELF_SECOES_EXTRAS;
}

static void ElfLiberarExtras(ElfExtras *x)
{
    BufLiberar(&x->sim);
    BufLiberar(&x->str);
    BufLiberar(&x->shstr);
}

/* ---------- escrita ---------- */

static void ElfAnexarEhdr(Buf *b, const Imagem *im, const ElfPlano *p, const ElfExtras *x)
{
    uint64_t v[EH_QTD] = {[EH_TYPE] = im->pie ? ELF_ET_DYN : ELF_ET_EXEC, [EH_MACHINE] = ELF_EM_X86_64, [EH_VERSION] = ELF_EV_CURRENT, [EH_ENTRY] = PeBaseDaImagem(im) + im->entrada_rva,
                          [EH_PHOFF] = ELF_TAM_EHDR, [EH_SHOFF] = x->off_sh, [EH_EHSIZE] = ELF_TAM_EHDR, [EH_PHENTSIZE] = ELF_TAM_PHDR, [EH_PHNUM] = p->qtd,
                          [EH_SHENTSIZE] = ELF_TAM_SHDR, [EH_SHNUM] = x->nsh, [EH_SHSTRNDX] = x->nsh - 1};

    ElfAnexarRegistro(b, &ELF_T_EHDR, v);
}

static void ElfAnexarPhdr(Buf *b, const ElfSegmento *s)
{
    uint64_t v[PH_QTD] = {[PH_TYPE] = s->tipo, [PH_FLAGS] = s->flags, [PH_OFFSET] = s->offset, [PH_VADDR] = s->vaddr, [PH_PADDR] = s->vaddr,
                          [PH_FILESZ] = s->filesz, [PH_MEMSZ] = s->memsz, [PH_ALIGN] = s->alinhamento};

    ElfAnexarRegistro(b, &ELF_T_PHDR, v);
}

static void ElfAnexarShdrSecao(Buf *b, const Imagem *im, const ElfPlano *p, const ElfExtras *x)
{
    for (uint32_t i = 0; i < im->qtd_secoes; i++) {
        const PeSecao *s = &im->secoes[i];
        bool bytes = s->bytes.tam > 0;
        uint64_t v[SH_QTD] = {[SH_NAME] = x->nome_sec[i], [SH_TYPE] = bytes ? ELF_SHT_PROGBITS : ELF_SHT_NOBITS, [SH_FLAGS] = ELF_FLAGS_SECAO[ElfClasseDe(s)],
                              [SH_ADDR] = PeBaseDaImagem(im) + s->rva, [SH_OFFSET] = p->off_secao[i], [SH_SIZE] = bytes ? s->bytes.tam : s->tam_virtual,
                              [SH_ADDRALIGN] = ELF_ALINHA_SECAO};
        ElfAnexarRegistro(b, &ELF_T_SHDR, v);
    }
}

static void ElfAnexarShdrExtras(Buf *b, const Imagem *im, const ElfExtras *x)
{
    uint64_t sim[SH_QTD] = {[SH_NAME] = x->nome_sim, [SH_TYPE] = ELF_SHT_SYMTAB, [SH_OFFSET] = x->off_sim, [SH_SIZE] = x->sim.tam,
                            [SH_LINK] = x->link_str, [SH_INFO] = ELF_PRIMEIRO_GLOBAL, [SH_ADDRALIGN] = ELF_ALINHA_TABELA, [SH_ENTSIZE] = ELF_TAM_SIM};
    uint64_t str[SH_QTD] = {[SH_NAME] = x->nome_str, [SH_TYPE] = ELF_SHT_STRTAB, [SH_OFFSET] = x->off_str, [SH_SIZE] = x->str.tam, [SH_ADDRALIGN] = 1};
    uint64_t shstr[SH_QTD] = {[SH_NAME] = x->nome_shstr, [SH_TYPE] = ELF_SHT_STRTAB, [SH_OFFSET] = x->off_shstr, [SH_SIZE] = x->shstr.tam, [SH_ADDRALIGN] = 1};

    ElfAnexarShdrDbg(b, im, x);
    ElfAnexarRegistro(b, &ELF_T_SHDR, sim);
    ElfAnexarRegistro(b, &ELF_T_SHDR, str);
    ElfAnexarRegistro(b, &ELF_T_SHDR, shstr);
}

/* ---------- pie: .rela.dyn e .dynamic (P41) ---------- */

static void ElfAnexarQuad(Buf *b, uint64_t v)
{
    uint8_t reg[ELF_TAM_QUAD];

    for (uint32_t k = 0; k < ELF_TAM_QUAD; k++)
        reg[k] = (uint8_t)(v >> (k * ELF_BITS_BYTE));
    BufAnexarN(b, (const char *)reg, ELF_TAM_QUAD);
}

/* Uma entrada de .rela.dyn por ADDR64 vivo: offset = endereco do campo (base 0), info = RELATIVE sem simbolo, adendo = o valor que o campo tinha. */
static void ElfAnexarRelativas(Buf *b, const Imagem *im)
{
    for (size_t k = 0; k < im->relativos.n; k++) {
        const ImagemRelativa *r = VecEm(&im->relativos, k);
        ElfAnexarQuad(b, PeBaseDaImagem(im) + r->rva);
        ElfAnexarQuad(b, ELF_R_RELATIVE);
        ElfAnexarQuad(b, r->valor);
    }
}

/* .dynamic: so o que o carregador e o _start precisam (DT_RELA, DT_RELASZ, DT_RELAENT, DT_FLAGS_1 = DF_1_PIE, DT_NULL). */
static void ElfAnexarDinamico(Buf *b, const ElfPlano *p)
{
    const uint64_t pares[ELF_QTD_DT][2] = {{ELF_DT_RELA, p->va_dyn + p->tam_dyn}, {ELF_DT_RELASZ, p->tam_rela}, {ELF_DT_RELAENT, ELF_TAM_RELA},
                                           {ELF_DT_FLAGS_1, ELF_DF_1_PIE}, {ELF_DT_NULL, 0}};

    for (uint32_t k = 0; k < ELF_QTD_DT; k++) {
        ElfAnexarQuad(b, pares[k][0]);
        ElfAnexarQuad(b, pares[k][1]);
    }
}

static void ElfAnexarShdrDinamico(Buf *b, const ElfPlano *p, const ElfExtras *x)
{
    uint64_t rela[SH_QTD] = {[SH_NAME] = x->nome_rela, [SH_TYPE] = ELF_SHT_RELA, [SH_FLAGS] = ELF_SHF_ALLOC, [SH_ADDR] = p->va_dyn + p->tam_dyn,
                             [SH_OFFSET] = p->off_dyn + p->tam_dyn, [SH_SIZE] = p->tam_rela, [SH_ADDRALIGN] = ELF_ALINHA_TABELA, [SH_ENTSIZE] = ELF_TAM_RELA};
    uint64_t dyn[SH_QTD] = {[SH_NAME] = x->nome_dyn, [SH_TYPE] = ELF_SHT_DYNAMIC, [SH_FLAGS] = ELF_SHF_ALLOC | ELF_SHF_WRITE, [SH_LINK] = x->link_str,
                            [SH_ADDR] = p->va_dyn, [SH_OFFSET] = p->off_dyn, [SH_SIZE] = p->tam_dyn,
                            [SH_ADDRALIGN] = ELF_ALINHA_TABELA, [SH_ENTSIZE] = ELF_TAM_DT};

    ElfAnexarRegistro(b, &ELF_T_SHDR, rela);
    ElfAnexarRegistro(b, &ELF_T_SHDR, dyn);
}

static void ElfEscreverTudo(const Imagem *im, const ElfPlano *p, const ElfExtras *x, Buf *b)
{
    uint64_t nulo[SH_QTD] = {0};

    ElfAnexarEhdr(b, im, p, x);
    for (uint32_t k = 0; k < p->qtd; k++)
        ElfAnexarPhdr(b, &p->seg[k]);
    for (uint32_t i = 0; i < im->qtd_secoes; i++) {
        if (im->secoes[i].bytes.tam == 0)
            continue;
        ElfPreencher(b, p->off_secao[i]);
        BufAnexarN(b, im->secoes[i].bytes.dados, im->secoes[i].bytes.tam);
    }
    if (p->pie) {
        ElfPreencher(b, p->off_dyn);
        ElfAnexarDinamico(b, p);
        ElfAnexarRelativas(b, im);
    }
    ElfEscreverDbg(b, im, x);
    ElfPreencher(b, x->off_sim);
    BufAnexarN(b, x->sim.dados, x->sim.tam);
    BufAnexarN(b, x->str.dados, x->str.tam);
    BufAnexarN(b, x->shstr.dados, x->shstr.tam);
    ElfPreencher(b, x->off_sh);
    ElfAnexarRegistro(b, &ELF_T_SHDR, nulo);
    ElfAnexarShdrSecao(b, im, p, x);
    if (p->pie)
        ElfAnexarShdrDinamico(b, p, x);
    ElfAnexarShdrExtras(b, im, x);
}

/* ---------- autoconferencia (X0905): rele os bytes gravados, nao o plano ---------- */

typedef struct {
    const uint8_t *d;
    uint64_t n;
    uint32_t esperado_ph, esperado_sh;  /* quantos cabecalhos de programa e de secao o escritor montou */
    bool pie;                           /* ET_DYN: PT_PHDR e PT_DYNAMIC sao admitidos e o PT_DYNAMIC e obrigatorio */
    bool tls;                           /* ha bloco TLS: exatamente um PT_TLS, e so ele */
    char motivo[ELF_MOTIVO_TAM];
} ElfVisao;

/* true se cond vale; senao guarda "<campo>: <regra>" para o X0905 e devolve false. */
static bool ElfExige(ElfVisao *v, bool cond, const char *campo, const char *regra)
{
    if (!cond && v->motivo[0] == '\0')
        snprintf(v->motivo, sizeof v->motivo, "%s: %s", campo, regra);
    return cond;
}

static bool ElfConferirIdent(ElfVisao *v)
{
    return ElfExige(v, v->n >= ELF_TAM_EHDR, "e_ident", "arquivo menor que o cabecalho ELF") &&
           ElfExige(v, memcmp(v->d, ELF_IDENT, ELF_TAM_IDENT) == 0, "e_ident", "diferente de 7f 45 4c 46, ELFCLASS64, LSB, EV_CURRENT, SYSV");
}

static bool ElfConferirTipos(ElfVisao *v)
{
    const ElfCampo *e = ELF_EHDR;

    return ElfExige(v, ElfLe(v->d, &e[EH_TYPE]) == (v->pie ? ELF_ET_DYN : ELF_ET_EXEC), e[EH_TYPE].nome, v->pie ? "diferente de ET_DYN" : "diferente de ET_EXEC") &&
           ElfExige(v, ElfLe(v->d, &e[EH_MACHINE]) == ELF_EM_X86_64, e[EH_MACHINE].nome, "diferente de EM_X86_64") &&
           ElfExige(v, ElfLe(v->d, &e[EH_EHSIZE]) == ELF_TAM_EHDR && ElfLe(v->d, &e[EH_PHENTSIZE]) == ELF_TAM_PHDR, e[EH_EHSIZE].nome, "ehsize/phentsize errados");
}

static bool ElfConferirTabelas(ElfVisao *v)
{
    const ElfCampo *e = ELF_EHDR;
    uint64_t phoff = ElfLe(v->d, &e[EH_PHOFF]), shoff = ElfLe(v->d, &e[EH_SHOFF]);

    return ElfExige(v, phoff == ELF_TAM_EHDR && ElfLe(v->d, &e[EH_PHNUM]) == v->esperado_ph, e[EH_PHOFF].nome, "diferente de 64 ou e_phnum diferente do montado") &&
           ElfExige(v, phoff + v->esperado_ph * (uint64_t)ELF_TAM_PHDR <= v->n, e[EH_PHNUM].nome, "tabela de cabecalhos de programa fora do arquivo") &&
           ElfExige(v, ElfLe(v->d, &e[EH_SHNUM]) == v->esperado_sh && shoff + v->esperado_sh * (uint64_t)ELF_TAM_SHDR <= v->n, e[EH_SHOFF].nome,
                    "tabela de secoes fora do arquivo ou e_shnum diferente do montado");
}

static bool ElfConferirCabecalho(ElfVisao *v)
{
    return ElfConferirIdent(v) && ElfConferirTipos(v) && ElfConferirTabelas(v);
}

typedef struct { uint64_t fim_pagina_va, fim_off; } ElfAnterior; /* onde o PT_LOAD anterior acaba (memoria em pagina inteira, arquivo em bytes) */

static bool ElfConferirAlinhamento(ElfVisao *v, const uint8_t *ph)
{
    const ElfCampo *p = ELF_PHDR;

    return ElfExige(v, ElfLe(ph, &p[PH_ALIGN]) == ELF_PAGINA, p[PH_ALIGN].nome, "diferente de 0x1000") &&
           ElfExige(v, ElfLe(ph, &p[PH_OFFSET]) % ELF_PAGINA == ElfLe(ph, &p[PH_VADDR]) % ELF_PAGINA, p[PH_OFFSET].nome, "nao e congruente a p_vaddr modulo 0x1000");
}

static bool ElfConferirTamanhos(ElfVisao *v, const uint8_t *ph)
{
    const ElfCampo *p = ELF_PHDR;
    uint64_t offset = ElfLe(ph, &p[PH_OFFSET]), filesz = ElfLe(ph, &p[PH_FILESZ]), flags = ElfLe(ph, &p[PH_FLAGS]);

    return ElfExige(v, filesz <= ElfLe(ph, &p[PH_MEMSZ]), p[PH_FILESZ].nome, "maior que p_memsz") &&
           ElfExige(v, offset <= v->n && filesz <= v->n - offset, p[PH_FILESZ].nome, "passa do fim do arquivo") &&
           ElfExige(v, (flags & ELF_PF_R) != 0 && (flags & (ELF_PF_W | ELF_PF_X)) != (ELF_PF_W | ELF_PF_X), p[PH_FLAGS].nome, "sem leitura ou com escrita e execucao juntas");
}

static bool ElfConferirLoad(ElfVisao *v, const uint8_t *ph, ElfAnterior *ant)
{
    const ElfCampo *p = ELF_PHDR;
    uint64_t offset = ElfLe(ph, &p[PH_OFFSET]), vaddr = ElfLe(ph, &p[PH_VADDR]);
    uint64_t filesz = ElfLe(ph, &p[PH_FILESZ]), memsz = ElfLe(ph, &p[PH_MEMSZ]);
    bool ok = ElfConferirAlinhamento(v, ph) && ElfConferirTamanhos(v, ph) &&
              ElfExige(v, vaddr / ELF_PAGINA * ELF_PAGINA >= ant->fim_pagina_va, p[PH_VADDR].nome, "PT_LOAD sobrepoe o anterior em memoria") &&
              ElfExige(v, offset >= ant->fim_off, p[PH_OFFSET].nome, "PT_LOAD sobrepoe o anterior no arquivo");

    ant->fim_pagina_va = ElfAlinhar(vaddr + memsz, ELF_PAGINA);
    ant->fim_off = offset + filesz;
    return ok;
}

/* PT_TLS (M5c-b P61): alinhamento de potencia de 2 e memsz (o bloco que o musl copia) >= filesz. */
static bool ElfConferirTls(ElfVisao *v, const uint8_t *ph)
{
    const ElfCampo *p = ELF_PHDR;
    uint64_t align = ElfLe(ph, &p[PH_ALIGN]), filesz = ElfLe(ph, &p[PH_FILESZ]), memsz = ElfLe(ph, &p[PH_MEMSZ]);

    return ElfExige(v, align != 0 && (align & (align - 1)) == 0, p[PH_ALIGN].nome, "PT_TLS com alinhamento que nao e potencia de 2") &&
           ElfExige(v, memsz > 0 && filesz <= memsz, p[PH_MEMSZ].nome, "PT_TLS com memsz zero ou menor que filesz");
}

/* Tipos de cabecalho de programa que o escritor monta: PT_LOAD e PT_GNU_STACK; no pie PT_PHDR e PT_DYNAMIC; com TLS, PT_TLS. Nada de PT_INTERP. */
static bool ElfTipoAdmitido(const ElfVisao *v, uint64_t tipo)
{
    if (tipo == ELF_PT_LOAD || tipo == ELF_PT_GNU_STACK)
        return true;
    if (v->pie && (tipo == ELF_PT_PHDR || tipo == ELF_PT_DYNAMIC))
        return true;
    return v->tls && tipo == ELF_PT_TLS;
}

/* So os tipos admitidos; um PT_LOAD confere a faixa; um PT_TLS confere o bloco; no pie um PT_DYNAMIC; com TLS um PT_TLS. */
static bool ElfConferirSegmentos(ElfVisao *v)
{
    ElfAnterior ant = {0, 0};
    uint32_t loads = 0, dinamicos = 0, tlses = 0;

    for (uint32_t k = 0; k < v->esperado_ph; k++) {
        const uint8_t *ph = v->d + ELF_TAM_EHDR + (uint64_t)k * ELF_TAM_PHDR;
        uint64_t tipo = ElfLe(ph, &ELF_PHDR[PH_TYPE]);
        if (!ElfExige(v, ElfTipoAdmitido(v, tipo), ELF_PHDR[PH_TYPE].nome, "tipo fora do que o escritor monta (PT_LOAD, PT_GNU_STACK, no pie PT_PHDR e PT_DYNAMIC, com TLS PT_TLS)"))
            return false;
        if (tipo == ELF_PT_LOAD && !ElfConferirLoad(v, ph, &ant))
            return false;
        if (tipo == ELF_PT_TLS && !ElfConferirTls(v, ph))
            return false;
        loads += tipo == ELF_PT_LOAD;
        dinamicos += tipo == ELF_PT_DYNAMIC;
        tlses += tipo == ELF_PT_TLS;
    }
    return ElfExige(v, loads > 0, ELF_PHDR[PH_TYPE].nome, "nenhum PT_LOAD") && ElfExige(v, !v->pie || dinamicos == 1, ELF_PHDR[PH_TYPE].nome, "pie sem exatamente um PT_DYNAMIC") &&
           ElfExige(v, tlses == (v->tls ? 1u : 0u), ELF_PHDR[PH_TYPE].nome, "PT_TLS faltando ou sobrando em relacao ao plano");
}

static bool ElfExecutaEntrada(const uint8_t *ph, uint64_t entrada)
{
    uint64_t vaddr = ElfLe(ph, &ELF_PHDR[PH_VADDR]);

    return ElfLe(ph, &ELF_PHDR[PH_TYPE]) == ELF_PT_LOAD && (ElfLe(ph, &ELF_PHDR[PH_FLAGS]) & ELF_PF_X) != 0 && entrada >= vaddr &&
           entrada - vaddr < ElfLe(ph, &ELF_PHDR[PH_FILESZ]);
}

/* e_entry cai dentro do arquivo de um PT_LOAD executavel. */
static bool ElfConferirEntrada(ElfVisao *v)
{
    uint64_t entrada = ElfLe(v->d, &ELF_EHDR[EH_ENTRY]);

    for (uint32_t k = 0; k < v->esperado_ph; k++) {
        if (ElfExecutaEntrada(v->d + ELF_TAM_EHDR + (uint64_t)k * ELF_TAM_PHDR, entrada))
            return true;
    }
    return ElfExige(v, false, ELF_EHDR[EH_ENTRY].nome, "fora de qualquer PT_LOAD executavel");
}

/* Cada cabecalho de secao com bytes cabe no arquivo; o indice do .shstrtab e o ultimo. */
static bool ElfConferirSecoes(ElfVisao *v)
{
    uint64_t shoff = ElfLe(v->d, &ELF_EHDR[EH_SHOFF]);

    for (uint32_t k = 1; k < v->esperado_sh; k++) {
        const uint8_t *sh = v->d + shoff + (uint64_t)k * ELF_TAM_SHDR;
        uint64_t off = ElfLe(sh, &ELF_SHDR[SH_OFFSET]), tam = ElfLe(sh, &ELF_SHDR[SH_SIZE]);
        bool nobits = ElfLe(sh, &ELF_SHDR[SH_TYPE]) == ELF_SHT_NOBITS;
        if (!ElfExige(v, nobits || (off <= v->n && tam <= v->n - off), ELF_SHDR[SH_SIZE].nome, "secao passa do fim do arquivo"))
            return false;
    }
    return ElfExige(v, ElfLe(v->d, &ELF_EHDR[EH_SHSTRNDX]) == v->esperado_sh - 1, ELF_EHDR[EH_SHSTRNDX].nome, "diferente do ultimo cabecalho de secao");
}

static bool ElfAutoconferir(const Buf *saida, const ElfPlano *p, uint32_t qtd_sh, Diag *erro)
{
    ElfVisao v = {.d = (const uint8_t *)saida->dados, .n = saida->tam, .esperado_ph = p->qtd, .esperado_sh = qtd_sh, .pie = p->pie, .tls = p->tls};
    bool ok = ElfConferirIdent(&v) && ElfConferirCabecalho(&v) && ElfConferirSegmentos(&v) && ElfConferirEntrada(&v) && ElfConferirSecoes(&v);

    return ok || DiagRelatar(erro, X0905, 0, "autoconferencia do ELF: %s", v.motivo);
}

/* ---------- API ---------- */

bool ElfEscrever(const Imagem *im, Buf *saida, Diag *erro)
{
    ElfPlano plano;
    ElfExtras x = {0};
    const char *motivo;
    bool ok;

    if (saida->tam != 0)
        FalhaInterna("elf: ElfEscrever recebeu uma saida que nao esta vazia");
    if (im->formato != IMAGEM_ELF)
        return DiagRelatar(erro, X0905, 0, "autoconferencia do ELF: a imagem nao e do formato ELF");
    motivo = ElfPlanejar(im, &plano);
    if (motivo != NULL)
        return DiagRelatar(erro, X0905, 0, "autoconferencia do ELF: %s", motivo);
    ElfMontarExtras(im, &plano, &x);
    ElfEscreverTudo(im, &plano, &x, saida);
    ok = ElfAutoconferir(saida, &plano, x.nsh, erro);
    ElfLiberarExtras(&x);
    return ok;
}
