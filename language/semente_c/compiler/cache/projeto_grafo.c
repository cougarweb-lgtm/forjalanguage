/* projeto_grafo.c - camadas topologicas (Kahn) dos produtos do forja.proj e deteccao de ciclo (P22-O1b do M3).
 * Versao: 0.1.0
 * Changelog: 0.1.0 - M3 P22-O1b: ProjetoCamadas; o ciclo (B0007) sai pela caminhada "1a dependencia ainda sem camada" a partir do 1o produto sem camada, na ordem de nome.
 * Verificacao: construir.bat && teste_common.exe
 */
#include <stdlib.h>

#include "cache/projeto_grafo.h"
#include "common/buf.h"

#define SEM_CAMADA UINT32_MAX

/* Todas as dependencias de k ja tem camada menor que 'rodada'. */
static bool DependenciasProntas(const Produto *pr, const uint32_t *camada, uint32_t rodada)
{
    for (uint32_t j = 0; j < pr->ndep; j++) {
        if (camada[pr->dep[j]] >= rodada) return false;     /* SEM_CAMADA tambem e >= rodada */
    }
    return true;
}

/* Uma rodada de Kahn: quem esta pronto ganha a camada 'rodada'. Devolve quantos ganharam. */
static uint32_t UmaRodada(const Projeto *p, uint32_t *camada, uint32_t rodada)
{
    uint32_t novos = 0;

    for (uint32_t k = 0; k < p->produtos.n; k++) {
        if (camada[k] == SEM_CAMADA && DependenciasProntas(ProjetoProduto(p, k), camada, rodada)) {
            camada[k] = rodada;
            novos++;
        }
    }
    return novos;
}

/* Primeira dependencia de k ainda sem camada (todo produto preso tem uma). */
static uint32_t DependenciaPresa(const Produto *pr, const uint32_t *camada)
{
    for (uint32_t j = 0; j < pr->ndep; j++) {
        if (camada[pr->dep[j]] == SEM_CAMADA) return pr->dep[j];
    }
    return pr->dep[0];
}

static uint32_t PrimeiroSemCamada(const uint32_t *camada)
{
    uint32_t k = 0;

    while (camada[k] != SEM_CAMADA) k++;
    return k;
}

/* Posicao de 'produto' na trilha; tam se ainda nao esta. */
static uint32_t PosicaoNaTrilha(const uint32_t *trilha, uint32_t tam, uint32_t produto)
{
    uint32_t pos = 0;

    while (pos < tam && trilha[pos] != produto) pos++;
    return pos;
}

/* Anda de dependencia presa em dependencia presa ate repetir um produto; o ciclo e o trecho desde a repeticao. */
static void RelatarCiclo(const Projeto *p, const uint32_t *camada, Diag *erro)
{
    uint32_t *trilha = calloc(p->produtos.n + 1, sizeof *trilha);
    uint32_t tam = 0, atual = PrimeiroSemCamada(camada), inicio = PosicaoNaTrilha(trilha, 0, atual);
    Buf texto = {0};

    if (trilha == NULL) FalhaInterna("memoria esgotada na deteccao de ciclo");
    while (inicio == tam) {
        trilha[tam++] = atual;
        atual = DependenciaPresa(ProjetoProduto(p, atual), camada);
        inicio = PosicaoNaTrilha(trilha, tam, atual);
    }
    BufAnexar(&texto, "ciclo de dependencia: ");
    for (uint32_t k = inicio; k < tam; k++) BufFormatar(&texto, "%s -> ", ProjetoProduto(p, trilha[k])->nome);
    BufAnexar(&texto, ProjetoProduto(p, atual)->nome);
    DiagRelatar(erro, B0007, 0, "%s", texto.dados);
    BufLiberar(&texto);
    free(trilha);
}

/* Indices por (camada, indice): o indice ja e a ordem de nome. */
static void OrdenarPorCamada(const Projeto *p, const uint32_t *camada, uint32_t maior, uint32_t *ordem)
{
    uint32_t pos = 0;

    for (uint32_t c = 0; c <= maior; c++) {
        for (uint32_t k = 0; k < p->produtos.n; k++) {
            if (camada[k] == c) ordem[pos++] = k;
        }
    }
}

bool ProjetoCamadas(const Projeto *p, uint32_t *camada, uint32_t *ordem, Diag *erro)
{
    uint32_t total = 0, rodada = 0;

    for (uint32_t k = 0; k < p->produtos.n; k++) camada[k] = SEM_CAMADA;
    while (total < p->produtos.n) {
        uint32_t novos = UmaRodada(p, camada, rodada);

        if (novos == 0) {
            RelatarCiclo(p, camada, erro);
            return false;
        }
        total += novos;
        rodada++;
    }
    OrdenarPorCamada(p, camada, rodada - 1, ordem);
    return true;
}
