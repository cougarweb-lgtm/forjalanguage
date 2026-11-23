/* elf.h - escritor do ELF64 estatico x86-64 (ET_EXEC, sem libc, sem PT_INTERP/PT_DYNAMIC) a partir da Imagem neutra; o plano de segmentos e publico para o --debug-code.
 * Versao: 0.3.0
 * Changelog: 0.3.0 - M5c-b P61: PT_TLS (Imagem.tls); ELF_SEGMENTOS_PIE (7) separado de ELF_MAX_SEGMENTOS (8, com o PT_TLS). Sem TLS o plano e o mesmo, byte a byte.
 *            0.2.0 - M5b P41 (--pie): ELF static-pie (ET_DYN, base 0): PT_PHDR, PT_DYNAMIC e um PT_LOAD RW no fim com .rela.dyn (R_X86_64_RELATIVE por ImagemRelativa) e
 *                    .dynamic (DT_RELA, DT_RELASZ, DT_RELAENT, DT_FLAGS_1 = DF_1_PIE, DT_NULL). ELF_MAX_SEGMENTOS 4 -> 7; sem pie o plano e o mesmo (ELF_SEGMENTOS_EXE).
 *            0.1.0 - M5 P27b: ElfPlanejar (3 PT_LOAD por permissao R, RX, RW + PT_GNU_STACK; offset de arquivo congruente ao endereco), ElfEscrever (cabecalhos, secoes, .symtab,
 *                    .strtab, .shstrtab e cabecalhos de secao so para readelf/objdump/perf) e autoconferencia que rele o que gravou (X0905 com o campo).
 * Verificacao: construir.bat && teste_nativo.exe e python -I ..\tests\cli\verificar_cli.py --forja .\forja.exe  (readelf/file no WSL: gaveta\contrato_m5.txt, P27b)
 */
#ifndef FORJA_NATIVO_FMT_ELF_H
#define FORJA_NATIVO_FMT_ELF_H

#include "nativo/fmt/pe.h"

#define ELF_SEGMENTOS_EXE 4 /* ET_EXEC: ate 3 PT_LOAD (R, RX, RW) e o PT_GNU_STACK; a area dos cabecalhos de programa e essa */
#define ELF_SEGMENTOS_PIE 7 /* pie sem TLS: PT_PHDR, 3 PT_LOAD, o PT_LOAD do .rela.dyn/.dynamic, PT_DYNAMIC e PT_GNU_STACK */
#define ELF_MAX_SEGMENTOS 8 /* o maior plano: o pie com TLS (os 7 acima mais o PT_TLS) */
#define ELF_PT_LOAD 1u
#define ELF_PT_TLS 7u
#define ELF_PT_GNU_STACK 0x6474e551u
#define ELF_PF_X 1u
#define ELF_PF_W 2u
#define ELF_PF_R 4u

/* Um cabecalho de programa (Elf64_Phdr) em valores nativos. */
typedef struct {
    uint32_t tipo, flags;
    uint64_t offset, vaddr, filesz, memsz, alinhamento;
} ElfSegmento;

/* Onde cada coisa cai: os segmentos (PT_GNU_STACK por ultimo), o offset de arquivo de cada secao da Imagem e onde acaba o conteudo das secoes. */
typedef struct {
    ElfSegmento seg[ELF_MAX_SEGMENTOS];
    uint32_t qtd;
    uint64_t off_secao[IMAGEM_MAX_SECOES];
    uint64_t fim_secoes;
    bool pie;                  /* ET_DYN: os 4 campos abaixo dizem onde ficam .rela.dyn e .dynamic (o PT_LOAD deles acaba no fim do arquivo) */
    bool tls;                  /* ha PT_TLS (Imagem.tls): o plano tem um segmento a mais, e a area dos cabecalhos tambem */
    uint64_t off_dyn, va_dyn;  /* arquivo e endereco do .rela.dyn (o .dynamic vem logo depois) */
    uint64_t tam_rela, tam_dyn;
} ElfPlano;

/* Inicio (endereco, base 0) do PT_LOAD do .rela.dyn e .dynamic de um pie: a primeira pagina depois da ultima secao da Imagem. O ligador usa o mesmo valor para _DYNAMIC. */
uint64_t ElfInicioDinamico(const Imagem *im);

/* NULL se a Imagem cabe num ELF (secoes em ordem R, RX, RW, com bytes antes das sem bytes dentro de cada permissao); senao a causa. */
const char *ElfPlanejar(const Imagem *im, ElfPlano *plano);

/* Escreve o ELF64 em saida (que tem de vir vazio) e rele o que gravou: X0905 com o campo se a autoconferencia nao bater. */
bool ElfEscrever(const Imagem *im, Buf *saida, Diag *erro);

#endif
