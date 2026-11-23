/* bir_construir_afirma.c - IR -> BIR: IR_ASSERT, IR_PANIC e IR_EXPECT (R0003), iguais aos forja_assert, forja_panic e forja_expect do runtime.
 * Versao: 0.1.1
 * Changelog: 0.1.1 - M2 P15a: a mensagem de ASSERT pode ser um cstr em variavel ou parametro (o ponteiro vai direto ao forja_assert), como no emit_c.
 *            0.1.0 - M2 P12a: primeira versao. ASSERT e EXPECT chamam forja_assert / forja_expect do runtime (valem em dev e em release; fora de teste o expect vira panic
 *                    "expect falhou: <texto>"); PANIC e ARMADILHA forja_panic. A mensagem de ASSERT e PANIC e V_TEXTO; outro valor segue X0001 (cstr em variavel e do P13a).
 * Verificacao: construir.bat && teste_nativo.exe bir <arq.fj> e python -I ..\tests\nativo\verificar_nativo.py --forja .\forja.exe --pasta m2/resultado
 */
#include <string.h>

#include "nativo/bir/bir_interno.h"

#define ARGS_AFIRMA 4

/* CHAMA a uma funcao do runtime que volta (o resultado, se houver, e descartado). */
static void ChamarRuntime(BirCtx *c, const char *funcao, const BirOperando *args, uint32_t n)
{
    BirOperando *copia = BirArgs(c, n);
    BirInstr *chama;

    memcpy(copia, args, n * sizeof *copia);
    chama = BirEmitir(c, BIR_CHAMA, BC_NADA);
    chama->simbolo = BirSimboloRuntime(c, funcao);
    chama->args = copia;
    chama->nargs = n;
}

static BirOperando EnderecoDoTexto(BirCtx *c, const char *bytes, uint32_t tam)
{
    return OpVreg(BirVregDoTexto(c, bytes, tam), BC_I64);
}

/* Mensagem literal (V_TEXTO): o endereco dos bytes com NUL; cstr em variavel ou parametro: o proprio ponteiro. */
static bool MensagemLiteral(BirCtx *c, const IrInstr *i, const Valor *v, BirOperando *saida)
{
    if (v->tipo_no != V_TEXTO && v->tipo == TIPO_CSTR)
        return BirOperandoDe(c, i, v, saida) && BirOperandoVreg(c, *saida, saida);
    if (v->tipo_no != V_TEXTO)
        return BirFalhaTipo(c, i, "-", v->tipo);
    *saida = EnderecoDoTexto(c, InternerTexto(c->e->nomes, v->id), InternerTam(c->e->nomes, v->id));
    return true;
}

/* arquivo e linha (locais ao trecho do .fj) da instrucao, nos dois ultimos argumentos. */
static void ArquivoELinha(BirCtx *c, const IrInstr *i, BirOperando *arquivo, BirOperando *linha)
{
    uint32_t numero;

    *arquivo = BirVregDoArquivo(c, i->pos, &numero);
    *linha = OpImed(numero, BC_I32);
}

/* A condicao bool segue estendida com zeros: o parametro `bool` do runtime le so o byte baixo, mas o resto do registrador nao fica sujo. */
static bool Condicao(BirCtx *c, const IrInstr *i, BirOperando *saida)
{
    if (!BirOperandoDe(c, i, &i->a, saida))
        return false;
    *saida = OpComExtensao(*saida, BE_ZERO);
    return true;
}

bool BirRegraAssert(BirCtx *c, const IrInstr *i)
{
    BirOperando args[ARGS_AFIRMA];

    if (!Condicao(c, i, &args[0]) || !MensagemLiteral(c, i, &i->b, &args[1]))
        return false;
    ArquivoELinha(c, i, &args[2], &args[3]);
    ChamarRuntime(c, "forja_assert", args, ARGS_AFIRMA);
    return true;
}

bool BirRegraExpect(BirCtx *c, const IrInstr *i)
{
    BirOperando args[ARGS_AFIRMA];

    if (!Condicao(c, i, &args[0]))
        return false;
    args[1] = EnderecoDoTexto(c, InternerTexto(c->e->nomes, i->nome), InternerTam(c->e->nomes, i->nome));
    ArquivoELinha(c, i, &args[2], &args[3]);
    ChamarRuntime(c, "forja_expect", args, ARGS_AFIRMA);
    return true;
}

bool BirRegraPanic(BirCtx *c, const IrInstr *i)
{
    BirOperando args[ARGS_AFIRMA - 1];

    if (!MensagemLiteral(c, i, &i->a, &args[0]))
        return false;
    ArquivoELinha(c, i, &args[1], &args[2]);
    BirArmadilha(c, BirSimboloRuntime(c, "forja_panic"), args, ARGS_AFIRMA - 1);
    return true;
}
