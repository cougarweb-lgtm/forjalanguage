/* elf_ler.h - leitor de ELF64 relocavel x86-64 (P33, pista L; M5b P35 ganha a ponte para o Objeto neutro).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M5b P35: ElfLerObjeto (preenche um Objeto de objeto.h: secoes, simbolos com WEAK e relocacoes com tipo neutro; .eh_frame, .note*, .comment, .debug* e
 *                    .group descartados; relocacao fora do censo = ELFL_RELOC_FORA_DO_CENSO com o tipo) e ElfListarIndefinidos (os GLOBAL UND, para escolher membros; nao confere o censo).
 *                    ElfLSecao ganha flags e alinhamento. Tipos ElfL* seguem (o leitor cru e o teste de 1349 membros dependem deles).
 *            0.1.0 - P33: ElfLer, ElfLLiberar e ElfLNomeReloc. Tipos locais (prefixo ElfL). Censo de relocacao do link estatico do musl:
 *                    R_X86_64_{64, PC32, PLT32, 32, 32S, GOTPCREL, GOTPCRELX, REX_GOTPCRELX, TPOFF32, GOTTPOFF}.
 * Verificacao: gcc -std=c11 -Wall -Wextra -I compiler compiler/nativo/obj/elf_ler.c compiler/nativo/obj/objeto.c compiler/common/arena.c compiler/common/buf.c compiler/common/intern.c compiler/common/vec.c tests/unidade/teste_elf_ler.c -o teste_elf_ler.exe && teste_elf_ler.exe
 */
#ifndef FORJA_NATIVO_OBJ_ELF_LER_H
#define FORJA_NATIVO_OBJ_ELF_LER_H

#include <stddef.h>
#include <stdint.h>

#include "common/arena.h"
#include "common/vec.h"
#include "nativo/obj/objeto.h"

/* ELFL_OK: lido e dentro do censo. ELFL_FORMATO: cabecalho, tabela ou entrada fora do buffer, ou fora de ELF64 relocavel x86-64.
 * ELFL_RELOC_FORA_DO_CENSO: arquivo valido com relocacao que o link nao trata (tipo em ElfLErro.tipo_reloc).
 * ELFL_MEMORIA: falta de memoria ao alocar as tabelas. */
typedef enum { ELFL_OK, ELFL_FORMATO, ELFL_RELOC_FORA_DO_CENSO, ELFL_MEMORIA } ElfLCodigo;

typedef struct {
    ElfLCodigo codigo;
    uint32_t tipo_reloc;  /* so em ELFL_RELOC_FORA_DO_CENSO; zero nos demais */
    char mensagem[256];   /* vazia em ELFL_OK; sem o nome do membro (quem chama o acrescenta) */
} ElfLErro;

/* nome aponta para dentro do buffer e termina em NUL (conferido). Em SHT_NOBITS (.bss), off e tam vem do cabecalho mas nao sao conferidos contra o buffer: a secao nao ocupa o arquivo. */
typedef struct {
    const char *nome;
    uint32_t tipo;
    uint32_t link;
    uint32_t info;
    uint64_t off;
    uint64_t tam;
    uint64_t entsize;
    uint64_t flags;
    uint64_t alinhamento;
} ElfLSecao;

/* Entrada da symtab. A entrada 0 (nula) e contada e fica no indice 0, como o readelf conta. */
typedef struct {
    const char *nome;
    uint64_t valor;
    uint64_t tam;
    uint8_t info;
    uint16_t secao;
} ElfLSimbolo;

/* addend vale 0 em SHT_REL (o addend fica no byte alvo). simbolo e o indice na symtab. */
typedef struct {
    uint64_t off;
    int64_t addend;
    uint32_t tipo;
    uint32_t simbolo;
} ElfLRelocacao;

typedef struct {
    size_t n_secoes;
    ElfLSecao *secoes;
    size_t n_simbolos;
    ElfLSimbolo *simbolos;
    size_t n_relocacoes;
    ElfLRelocacao *relocacoes;
} ElfLObjeto;

/* Le o ELF64 relocavel de bytes[0..n) para obj (alocado aqui). Em ELFL_RELOC_FORA_DO_CENSO, obj fica inteiro (a contagem vale) e erro->tipo_reloc diz o
 * primeiro tipo fora do censo. Em qualquer outro erro, obj fica parcial. Libere com ElfLLiberar sempre, mesmo em erro. */
ElfLCodigo ElfLer(const uint8_t *bytes, size_t n, ElfLObjeto *obj, ElfLErro *erro);

/* Libera as tabelas de obj e zera o objeto. Seguro em obj parcial. */
void ElfLLiberar(ElfLObjeto *obj);

/* Nome do tipo de relocacao do censo (ex. "R_X86_64_PC32"); "?" para tipo fora do censo. */
const char *ElfLNomeReloc(uint32_t tipo);

/* Preenche o Objeto o (iniciado por ObjIniciar) com o membro bytes[0..n). Mesmos codigos de ElfLer; em ELFL_RELOC_FORA_DO_CENSO o Objeto fica vazio (nada e convertido).
 * Simbolo local ganha o nome ".<nome>#<indice da symtab>" (unico no membro; o ponto poe o simbolo fora do global). Secao de simbolo (STT_SECTION) vira ".secao<k>".
 * Absoluto, comum e simbolo de secao descartada = ELFL_FORMATO com a causa no texto (o ligador nativo ainda nao os trata). */
ElfLCodigo ElfLerObjeto(const uint8_t *bytes, size_t n, Objeto *o, ElfLErro *erro);

/* Para onde vao os nomes de ElfListarIndefinidos: cada um e copiado para arena e o const char * entra em lista (Vec de const char *, iniciado por quem chama). */
typedef struct {
    Arena *arena;
    Vec *lista;
} ElfLDestinoNomes;

/* Acrescenta em destino->lista os simbolos GLOBAL indefinidos do membro. Fraco e local ficam de fora (como o COFF). Nao confere o censo de relocacao: um membro com
 * tipo fora do censo tambem e varrido. */
ElfLCodigo ElfListarIndefinidos(const uint8_t *bytes, size_t n, const ElfLDestinoNomes *destino, ElfLErro *erro);

#endif
