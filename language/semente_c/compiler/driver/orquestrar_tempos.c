/* orquestrar_tempos.c - medicao por etapa do orquestrador de projeto (ver orquestrar_tempos.h).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M4 P24a2: toda linha ganha "ini_us"; TemposIniciar guarda o FILETIME de inicio; vida_do_filho tem ini_us = (criacao_ft - inicio_ft) / 10; TemposAnotarUs sem instante grava -1;
 *            ocioso_do_slot passa por TemposFechar (tem o instante).
 *            0.1.0 - M3 F6: primeira versao.
 * Verificacao: construir.bat && python -I ..\tests\projeto\verificar_projeto.py --forja .\forja.exe
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "driver/erro_sistema.h"
#include "driver/orquestrar_tempos.h"

static void AvisarArquivo(const char *acao, const char *caminho, int err)
{
    char causa[ERRO_TEXTO_MAX];

    ErroCrtTexto(err, causa, sizeof causa);
    fprintf(stderr, "forja: aviso: nao consegui %s '%s' dos tempos do orquestrador (%s)\n", acao, caminho, causa);
}

static bool CriarArquivo(const char *caminho)
{
    FILE *f = fopen(caminho, "wb");

    if (f == NULL) {
        AvisarArquivo("criar o arquivo", caminho, errno);
        return false;
    }
    fclose(f);
    return true;
}

static uint64_t FiletimeAgora(void)
{
    FILETIME agora;

    GetSystemTimePreciseAsFileTime(&agora);
    return ((uint64_t)agora.dwHighDateTime << 32) | agora.dwLowDateTime;
}

void TemposIniciar(Tempos *t)
{
    const char *caminho = getenv(TEMPOS_VARIAVEL);
    LARGE_INTEGER frequencia;

    if (caminho == NULL || caminho[0] == '\0') return;
    if (strlen(caminho) >= sizeof t->caminho) {
        fputs("forja: aviso: " TEMPOS_VARIAVEL " longa demais; sem tempos do orquestrador\n", stderr);
        return;
    }
    snprintf(t->caminho, sizeof t->caminho, "%s", caminho);
    if (!CriarArquivo(t->caminho)) return;
    QueryPerformanceFrequency(&frequencia);
    t->frequencia = (uint64_t)frequencia.QuadPart;
    VecIniciar(&t->linhas, sizeof(TempoLinha));
    t->ativo = true;
    t->inicio_ft = FiletimeAgora();
    t->inicio = TemposAgora(t);
}

uint64_t TemposAgora(const Tempos *t)
{
    LARGE_INTEGER agora;

    if (!t->ativo) return 0;
    QueryPerformanceCounter(&agora);
    return (uint64_t)agora.QuadPart;
}

/* Linha nova ao fim da lista, sem instante (ini_us = -1) e sem "n"; quem chama preenche us (e ini_us, se souber). */
static TempoLinha *NovaLinha(Tempos *t, const char *etapa, const char *produto)
{
    TempoLinha *l = VecEmpurrar(&t->linhas);

    snprintf(l->etapa, sizeof l->etapa, "%s", etapa);
    if (produto != NULL) snprintf(l->produto, sizeof l->produto, "%s", produto);
    l->ini_us = -1;
    l->n = -1;
    return l;
}

void TemposAnotarUs(Tempos *t, const char *etapa, const char *produto, uint64_t us)
{
    if (!t->ativo) return;
    NovaLinha(t, etapa, produto)->us = us;
}

void TemposAnotarTicks(Tempos *t, const char *etapa, const char *produto, uint64_t ticks)
{
    if (!t->ativo) return;
    NovaLinha(t, etapa, produto)->us = ticks * 1000000ull / t->frequencia;
}

void TemposFechar(Tempos *t, const char *etapa, const char *produto, uint64_t ini)
{
    TempoLinha *l;

    if (!t->ativo) return;
    l = NovaLinha(t, etapa, produto);
    l->us = (TemposAgora(t) - ini) * 1000000ull / t->frequencia;
    l->ini_us = (int64_t)((ini > t->inicio ? ini - t->inicio : 0) * 1000000ull / t->frequencia);
}

void TemposAcrescentarN(Tempos *t, int64_t n)
{
    if (!t->ativo || t->linhas.n == 0) return;
    ((TempoLinha *)VecEm(&t->linhas, t->linhas.n - 1))->n = n;
}

void TemposAcordou(Tempos *t)
{
    if (!t->ativo) return;
    t->acordou_ft = FiletimeAgora();
    t->acordou_ticks = TemposAgora(t);
}

void TemposDoFilho(Tempos *t, const char *produto, uint64_t criacao, uint64_t saida)
{
    TempoLinha *vida;

    if (!t->ativo) return;
    vida = NovaLinha(t, "vida_do_filho", produto);
    vida->us = saida > criacao ? (saida - criacao) / 10 : 0;
    vida->ini_us = criacao > t->inicio_ft ? (int64_t)((criacao - t->inicio_ft) / 10) : 0;
    TemposAnotarUs(t, "latencia", produto, t->acordou_ft > saida ? (t->acordou_ft - saida) / 10 : 0);
    if (t->nlivres < TEMPOS_SLOTS_MAX) t->livres[t->nlivres++] = t->acordou_ticks;
}

void TemposSlotOcupado(Tempos *t, const char *produto)
{
    uint64_t desde;

    if (!t->ativo || t->nlivres == 0) return;
    desde = t->livres[0];
    memmove(t->livres, t->livres + 1, (--t->nlivres) * sizeof t->livres[0]);
    TemposFechar(t, "ocioso_do_slot", produto, desde);
}

static void EscreverLinha(FILE *f, const TempoLinha *l)
{
    fprintf(f, "{\"etapa\":\"%s\",\"us\":%llu,\"ini_us\":%lld", l->etapa, (unsigned long long)l->us, (long long)l->ini_us);
    if (l->produto[0] != '\0') fprintf(f, ",\"produto\":\"%s\"", l->produto);
    if (l->n >= 0) fprintf(f, ",\"n\":%lld", (long long)l->n);
    fputs("}\n", f);
}

void TemposGravar(Tempos *t)
{
    FILE *f;
    bool falhou;

    if (!t->ativo) return;
    f = fopen(t->caminho, "ab");
    if (f == NULL) {
        AvisarArquivo("abrir o arquivo", t->caminho, errno);
    } else {
        for (size_t k = 0; k < t->linhas.n; k++) EscreverLinha(f, VecEm(&t->linhas, k));
        falhou = ferror(f) != 0;
        if (fclose(f) != 0 || falhou) AvisarArquivo("gravar o arquivo", t->caminho, errno);
    }
    VecLiberar(&t->linhas);
    t->ativo = false;
}
