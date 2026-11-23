/* biblioteca.h - achar a biblioteca de um 'link' do foreign (P19a do M3): a ordem de busca do ld e o tipo decidido pelo CONTEUDO do arquivo.
 * Versao: 0.5.0
 * Changelog: 0.5.0 - M5b P39-B2 (D0654): BibliotecaBusca.semente (pasta do libc.a da semente musl): no alvo ELF, "c", "m", "msvcrt" e "ucrt" acham <semente>\libc.a.
 *            0.4.0 - M5b P36: BibliotecaTipo.BIB_ELF_DINAMICO (ELF ET_DYN = .so, X0003 em bibliotecas.c); BibliotecaBusca.elf (busca <pasta>\vendor\linux\lib<x>.a).
 *            0.3.0 - M3 P19b: so comentario (o tipo do ar vem dos membros lidos do arquivo, nao so do prefixo de 64 KiB; IMPORT_MSVC, IMPORT_GNU e EXE passam a resolver em bibliotecas.c).
 *            0.2.0 - M3 P19c: link "msvcrt" e "ucrt" tambem valem a ucrtbase.dll do System32.
 *            0.1.0 - M3 P19a: BibliotecaAchar (ordem por pasta conferida com o ld 2.46 e --trace; link "m" e "c" = ucrtbase.dll do System32; "procurei_em").
 * Verificacao: construir.bat && teste_nativo.exe (OK: N verificacoes)
 */
#ifndef FORJA_NATIVO_OBJ_BIBLIOTECA_H
#define FORJA_NATIVO_OBJ_BIBLIOTECA_H

#include <stdint.h>

#include "common/buf.h"

#define BIBLIOTECA_TAM_CAMINHO 620

/* Pelo conteudo, nunca pela extensao: "MZ" com o bit de DLL = DLL, sem ele = EXE; "!<arch>\n" cujo 1o membro e um objeto com secoes .idata$ = import GNU
 * (dlltool), cujo 1o membro e (ou logo depois tem) cabecalho de import curto = import MSVC; membro de objeto /GL (IL) = OBJETO_GL; outro membro = ESTATICA_AR. */
typedef enum { BIB_DLL, BIB_EXE, BIB_IMPORT_MSVC, BIB_IMPORT_GNU, BIB_ESTATICA_AR, BIB_OBJETO_GL, BIB_DESCONHECIDO, BIB_ELF_DINAMICO } BibliotecaTipo;

/* Pastas de busca, na ordem: as de PastasDeLigacao do driver (pasta do .fj, extras de vendor: e engine:), depois --lib-pasta. O System32 vem sempre por
 * ultimo e so para .dll. elf (M5b P36, alvo x86_64-linux): cada pasta cede <pasta>\vendor\linux\ e so ha lib<x>.a (estatica) e lib<x>.so (ELF dinamico, X0003); sem System32. */
typedef struct {
    const char *const *pastas;
    uint32_t qtd_pastas;
    bool elf;
    const char *semente; /* ELF: pasta da semente musl (libc.a); NULL = sem semente (o link "c" acha vendor\linux, como antes) */
} BibliotecaBusca;

/* caminho vazio se nao achou. procurei_em: um candidato por linha ('\n'), na ordem tentada; o achado e o ultimo. */
typedef struct {
    char caminho[BIBLIOTECA_TAM_CAMINHO];
    BibliotecaTipo tipo;
    Buf procurei_em;
} BibliotecaAchada;

/* Ordem por pasta (a do ld para -l<x> no PE): lib<x>.dll.a, <x>.dll.a, lib<x>.a, <x>.lib, lib<x>.lib, lib<x>.dll, <x>.dll. O 1o arquivo que existe vence, seja
 * qual for o tipo. link "m", "c", "msvcrt" e "ucrt" valem ucrtbase.dll de %SystemRoot%\System32 (a UCRT do Windows tem a libc e a libm). Devolve false se nada existe
 * (X0102 e do chamador; achada->procurei_em diz o que foi tentado). Chame BibliotecaLiberar depois, ate quando devolver false. */
bool BibliotecaAchar(const BibliotecaBusca *busca, const char *link, BibliotecaAchada *achada);

void BibliotecaLiberar(BibliotecaAchada *achada);

#endif
