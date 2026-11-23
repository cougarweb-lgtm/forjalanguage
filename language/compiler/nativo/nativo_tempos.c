/* nativo_tempos.c - tempos do build: a tabela unica dos campos do registro "tempos" (--tempos do driver e .debug.jsonl), o JSON deles e o fechamento
 * (total e outros).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M4 P24a: NATIVO_TEMPOS_CAMPOS, NativoTemposAnexarJson, NativoTemposValor, NativoTemposFechar e NativoTemposGravar.
 * Verificacao: construir.bat && python -I ..\tests\cli\verificar_cli.py --forja .\forja.exe
 *
 * Campo novo = uma linha na tabela (e o membro em NativoTempos, nativo.h): o --tempos, o registro tempos do .debug.jsonl e o texto do --debug-code leem
 * a tabela, nao tem lista propria.
 */
#include <stddef.h>
#include <string.h>

#include "common/base.h"
#include "common/fonte.h"
#include "nativo/nativo.h"

#define CAMPO_TEMPO(membro) {#membro, offsetof(NativoTempos, membro)}

/* A ordem e a do registro (e a do pipeline nas 10 primeiras). */
const NativoTempoCampo NATIVO_TEMPOS_CAMPOS[NATIVO_TEMPOS_QTD] = {
    CAMPO_TEMPO(lex_ns),     CAMPO_TEMPO(parse_ns), CAMPO_TEMPO(check_ns),     CAMPO_TEMPO(ir_ns),     CAMPO_TEMPO(bir_ns),
    CAMPO_TEMPO(isel_ns),    CAMPO_TEMPO(regs_ns),  CAMPO_TEMPO(codificar_ns), CAMPO_TEMPO(ligar_ns),  CAMPO_TEMPO(escrever_ns),
    CAMPO_TEMPO(outros_ns),  CAMPO_TEMPO(total_ns), CAMPO_TEMPO(linhas),
};

uint64_t NativoTemposValor(const NativoTempos *t, size_t k)
{
    uint64_t valor;

    memcpy(&valor, (const char *)t + NATIVO_TEMPOS_CAMPOS[k].deslocamento, sizeof valor);
    return valor;
}

void NativoTemposAnexarJson(Buf *b, const NativoTempos *t)
{
    for (size_t k = 0; k < NATIVO_TEMPOS_QTD; k++)
        BufFormatar(b, ", \"%s\": %llu", NATIVO_TEMPOS_CAMPOS[k].nome, (unsigned long long)NativoTemposValor(t, k));
}

void NativoTemposFechar(NativoTempos *t)
{
    uint64_t soma = 0;

    for (size_t k = 0; k < NATIVO_TEMPOS_ETAPAS; k++)
        soma += NativoTemposValor(t, k);
    t->total_ns = RelogioNs() - t->inicio_ns;
    if (soma > t->total_ns)
        FalhaInterna("tempos: a soma das etapas (%llu ns) passa do total (%llu ns); inicio_ns=%llu", (unsigned long long)soma, (unsigned long long)t->total_ns,
                     (unsigned long long)t->inicio_ns);
    t->outros_ns = t->total_ns - soma;
}

bool NativoTemposGravar(const char *caminho, uint64_t emit_ns, NativoTempos *t, const Fonte *fonte)
{
    Buf texto = {0};
    bool ok;

    NativoTemposFechar(t);
    t->linhas = FonteContarLinhas(fonte);
    BufFormatar(&texto, "{\"emit_ns\": %llu", (unsigned long long)emit_ns);
    NativoTemposAnexarJson(&texto, t);
    BufAnexar(&texto, "}\n");
    ok = ArquivoGravar(caminho, texto.dados, texto.tam);
    BufLiberar(&texto);
    return ok;
}
