/* alvo.c - tabela de alvos do backend nativo (dados) e a busca por nome.
 * Versao: 0.5.0
 * Changelog: 0.5.0 - M5b P39-B1 (D0654): cada alvo traz os nomes de copia e zera do runtime (Windows memcpy/memset; linux forja_memcpy/forja_memset).
 *            0.4.1 - M5 P27b: o escritor ELF existe (elf_escrever.c): "x86_64-linux" deixa de ter sem_escritor.
 *            0.4.0 - M5 P27a: {"x86_64-linux", X64Gerar, IMAGEM_ELF} (o mesmo gerador, a convencao Win64 por dentro, D-M5-2); AlvoQtd e AlvoPorIndice.
 *            0.3.1 - M4 P24a: X64Gerar recebe a AlvoColheita (alvo.h 0.5.0); a tabela nao muda.
 *            0.3.0 - M2 P11a: AlvoListagemLiberar libera ListaFuncao.slots_agregado.
 *            0.2.0 - backend x64 P8b: AlvoListagemIniciar e AlvoListagemLiberar.
 *            0.1.1 - backend x64 P8: X64Gerar passou a ter o parametro da listagem (mesma tabela).
 *            0.1.0 - backend x64 P7a: "x86_64-windows" -> X64Gerar.
 * Verificacao: construir.bat && teste_nativo.exe x64 <arq.fj> <saida.exe>
 */
#include <stdlib.h>
#include <string.h>

#include "nativo/x64/alvo.h"
#include "nativo/x64/x64_gerar.h"

static const Alvo ALVOS[] = {
    {"x86_64-windows", X64Gerar, IMAGEM_PE, true, NULL, {"memcpy", "memset"}},
    {"x86_64-linux", X64Gerar, IMAGEM_ELF, false, NULL, {"forja_memcpy", "forja_memset"}},
};

void AlvoListagemIniciar(AlvoListagem *lista)
{
    *lista = (AlvoListagem){0};
    VecIniciar(&lista->funcoes, sizeof(ListaFuncao));
    VecIniciar(&lista->instrs, sizeof(ListaInstr));
}

void AlvoListagemLiberar(AlvoListagem *lista)
{
    for (size_t k = 0; k < lista->funcoes.n; k++)
        free(((ListaFuncao *)VecEm(&lista->funcoes, k))->slots_agregado);
    VecLiberar(&lista->funcoes);
    VecLiberar(&lista->instrs);
    BufLiberar(&lista->textos);
}

uint32_t AlvoQtd(void)
{
    return (uint32_t)(sizeof ALVOS / sizeof ALVOS[0]);
}

const Alvo *AlvoPorIndice(uint32_t k)
{
    return k < AlvoQtd() ? &ALVOS[k] : NULL;
}

const Alvo *AlvoPorNome(const char *nome)
{
    for (uint32_t k = 0; k < AlvoQtd(); k++) {
        if (strcmp(ALVOS[k].nome, nome) == 0)
            return &ALVOS[k];
    }
    return NULL;
}
