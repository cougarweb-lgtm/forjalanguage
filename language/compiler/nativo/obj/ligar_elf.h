/* ligar_elf.h - o lado ELF do ligador do backend nativo (M5b P36): relocacoes dos membros de bibliotecas estaticas, celulas .got, simbolos do ligador
 * (__init_array_start e similares) e a coleta dos simbolos da Imagem. Interno: quem chama e ligar.c.
 * Versao: 0.4.0
 * Changelog: 0.4.0 - M6 P66: LigarElfDebug (secoes de debug do programa para Imagem.dbg, com as relocacoes ADDR64 absolutas).
 *            0.3.0 - M5c-b P61: o bloco TLS (LigarElfPlanejarTls, LigarElfMarcarTls, LigarElfTlsPermitido e LigarElfFormaTls); LigarSaidaElf manda OS_TDATA/OS_TBSS para LIGAR_SAIDA_TLS.
 *            0.2.0 - M5b P37d: LigarSaidaElf e LigarElfSemSaida (a saida do ELF sai do tipo da secao).
 *            0.1.0 - M5b P36: primeira versao (o que era so de ELF saiu de ligar.c: LigarElfColetarSimbolos e LigarElfConferirFormato).
 */
#ifndef FORJA_NATIVO_OBJ_LIGAR_ELF_H
#define FORJA_NATIVO_OBJ_LIGAR_ELF_H

#include "nativo/obj/ligar_estado.h"

/* Antes de montar as secoes: conta as celulas .got (relocacoes GOTPCREL, e GOTPCRELX que nao relaxam) e fixa o tamanho de LIGAR_SAIDA_GOT. No PE nao faz nada. */
void LigarElfPlanejarGot(LigacaoEstado *L);

/* Depois de resolver os simbolos: grava o endereco (ADDR64) de cada celula .got. */
void LigarElfPreencherGot(LigacaoEstado *L);

/* Aplica uma relocacao da secao viva de objeto o (ELF): ABS64, ABS32, ABS32S, REL32, GOTPCREL e GOTPCRELX (relaxa ou cai na celula). TLS = X0111. */
bool LigarElfRelocar(LigacaoEstado *L, int o, const ObjReloc *r);

/* RVA do simbolo do ligador (ELF): __init_array_start/end, __fini_array_start/end (vazios, no fim do .init_array) e __ehdr_start (RVA 0). false se o nome nao e um deles. */
bool LigarElfSimboloDoLigador(const LigacaoEstado *L, const char *nome, uint32_t *rva);

/* M6 P66 (D-M6-4): copia as secoes de debug do programa para Imagem.dbg e aplica os ADDR64 delas (enderecos absolutos). Chamada depois do layout. */
void LigarElfDebug(LigacaoEstado *L);

/* ELF: os simbolos vivos de todos os objetos vao para a Imagem (o .symtab). */
void LigarElfColetarSimbolos(const LigacaoEstado *L);

/* ELF estatico: recusa DLL e tabelas de imports (X0903) antes de ligar. */
bool LigarElfConferirFormato(const LigarPedido *pedido, Diag *erro);

/* ELF: saida (LIGAR_SAIDA_*) de uma secao viva de objeto pelo tipo OS_*; OS_TDATA e OS_TBSS vao para LIGAR_SAIDA_TLS (M5c-b P61). */
int LigarSaidaElf(const ObjSecao *s);

/* ELF: tipo de secao sem saida conhecida: recusa com X0903 em L->erro. Sempre false. */
bool LigarElfSemSaida(const LigacaoEstado *L, const ObjSecao *s);

/* M5c-b P61: secao TLS viva sem musl (ligacao crua): recusa com X0113 em L->erro, com a secao no texto. True com a semente (variante libc). */
bool LigarElfTlsPermitido(const LigacaoEstado *L, const ObjSecao *s);

/* M5c-b P61: monta o bloco TLS: .tdata (bytes) primeiro, depois o .tbss na cauda, cada contribuicao no alinhamento dela. Antes de criar as secoes. */
void LigarElfPlanejarTls(LigacaoEstado *L);

/* M5c-b P61: depois de criar as secoes: a Imagem recebe a secao TLS e o p_align do PT_TLS. */
void LigarElfMarcarTls(LigacaoEstado *L);

/* M5c-b P61: o opcode de um GOTTPOFF que o LE relaxa (8b = mov, 03 = add, com modrm [rip+d]); 0 se a forma nao e essa (X0114). */
int LigarElfFormaTls(const ObjSecao *s, uint32_t off);

#endif
