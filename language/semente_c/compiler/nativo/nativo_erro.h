/* nativo_erro.h - erros do backend nativo legiveis por agente: campos extras do JSON, pacote de relato dos erros internos e a injecao de teste.
 * Versao: 0.4.0
 * Changelog: 0.4.0 - 0.21.0 F5-a: comentario do ErroOrigem.etapa NULL (erro antes da 1a etapa, sem o backend C); codigo igual.
 *            0.3.0 - IA1-b03: ErroOrigem.etapa NULL = erro do backend C (sem alvo e etapa).
 *            0.2.0 - M3 P19: ErroOrigem.campos; NativoRelatarErro devolve 1 tambem para os X01xx de biblioteca.
 *            0.1.0 - backend x64 P8b: NativoRelatarErro (X0001 e X0901..X0906 por DiagImprimirJsonExtras) e NativoInjetarFalha (FORJA_TESTE_INJETAR).
 * Verificacao: construir.bat && python -I ..\tests\cli\verificar_cli.py --forja .\forja.exe
 */
#ifndef FORJA_NATIVO_NATIVO_ERRO_H
#define FORJA_NATIVO_NATIVO_ERRO_H

#include "nativo/nativo.h"

/* Etapas do pipeline, na ordem (o campo "etapa" do JSON). O X64Gerar funde isel, regs e codificar: o erro dele e "isel", ou "codificar" no X0902. */
#define ETAPA_BIR "bir"
#define ETAPA_LAYOUT "layout"
#define ETAPA_ISEL "isel"
#define ETAPA_CODIFICAR "codificar"
#define ETAPA_LIGAR "ligar"
#define ETAPA_ESCREVER "escrever"

/* De onde veio o erro: a etapa e a BIR que ainda existe (NULL se nao ha), com quantas funcoes dela estao completas.
 * etapa NULL = o erro veio antes da 1a etapa (nenhuma etapa entrou ainda): sem "alvo" e "etapa" no JSON, e 'campos' tem de vir preenchido. */
typedef struct {
    const char *etapa;
    const BirPrograma *bir;
    uint32_t bir_funcoes;
    const char *campos;     /* campos extras do erro, ja em JSON ("chave": valor, ...) sem a virgula inicial; NULL = nenhum (X01xx de biblioteca) */
} ErroOrigem;

/* Imprime o erro em stdout: uma linha JSON com os extras (alvo, etapa, ir, sugestao, interno, relato). Em X09xx grava antes o pacote de relato em
 * %TEMP%\forja_relato_<fnv32(comando + fontes)>\. Devolve o exit do forja: 1 para X0001 (diagnostico) e 3 para os internos. */
int NativoRelatarErro(const NativoPedido *pedido, const Diag *erro, const ErroOrigem *origem);

/* Anexa a b o campo `, "ir": {funcao, instr, op, op_aux, tipo}` da IrInstr que fica em 'off' (a causa do X0001 desempata quando ha varias na mesma
 * posicao). false, sem anexar nada, se nenhuma instrucao da IR esta la. */
bool NativoAnexarIr(Buf *b, const NativoPedido *p, uint32_t off, const char *causa);

/* FORJA_TESTE_INJETAR=X0901..X0906 (so para teste) forca esse codigo na etapa dele: bir -> X0904, layout -> X0906, isel -> X0901,
 * codificar -> X0902, ligar -> X0903, escrever -> X0905. true = o codigo desta etapa foi injetado em *erro (a etapa deve falhar). */
bool NativoInjetarFalha(const NativoPedido *pedido, const char *etapa, Diag *erro);

#endif
