/* orquestrar_tempos.h - medicao por etapa do orquestrador de projeto (FORJA_ORQ_TEMPOS=<arquivo.jsonl>).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - M4 P24a2: toda linha ganha "ini_us" (offset do inicio da etapa desde o inicio do orquestrador; -1 = sem instante); Tempos guarda o FILETIME de inicio.
 *            0.1.0 - M3 F6: Tempos (mora dentro do Plano), TemposIniciar/Agora/Fechar/AnotarTicks/AnotarUs/AcrescentarN/Acordou/DoFilho/SlotOcupado/Gravar.
 * Verificacao: construir.bat && python -I ..\tests\projeto\verificar_projeto.py --forja .\forja.exe
 *
 * Sem a variavel nada e criado e cada chamada daqui sai no primeiro if (ativo = false). Com ela, as linhas ficam em memoria e so TemposGravar as escreve,
 * no fim da execucao, para a propria escrita nao entrar nas medidas. Relogio: QueryPerformanceCounter (ticks). Linha: {"etapa":"x","us":N,"ini_us":N[,"produto":"p"][,"n":N]}.
 * ini_us: quando a etapa comecou, em us desde o TemposIniciar (QPC; a vida_do_filho usa o FILETIME de criacao do processo menos o de TemposIniciar); -1 = sem instante.
 */
#ifndef FORJA_DRIVER_ORQUESTRAR_TEMPOS_H
#define FORJA_DRIVER_ORQUESTRAR_TEMPOS_H

#include <stdbool.h>
#include <stdint.h>

#include "common/vec.h"

#define TEMPOS_VARIAVEL "FORJA_ORQ_TEMPOS"
#define TEMPOS_CAMINHO_MAX 1100
#define TEMPOS_ETAPA_MAX 24
#define TEMPOS_PRODUTO_MAX 64
#define TEMPOS_SLOTS_MAX 64

/* Uma etapa medida. n < 0 = sem o campo "n". produto vazio = etapa por execucao. ini_us < 0 = sem instante de inicio. */
typedef struct {
    char etapa[TEMPOS_ETAPA_MAX];
    char produto[TEMPOS_PRODUTO_MAX];
    uint64_t us;
    int64_t ini_us;
    int64_t n;
} TempoLinha;

typedef struct {
    bool ativo;
    char caminho[TEMPOS_CAMINHO_MAX];
    uint64_t frequencia, inicio;            /* ticks por segundo; ticks do TemposIniciar */
    uint64_t inicio_ft;                     /* FILETIME (100 ns) do TemposIniciar */
    uint64_t acordou_ticks, acordou_ft;     /* o instante em que o orquestrador acordou da espera: ticks e FILETIME (100 ns) */
    uint64_t livres[TEMPOS_SLOTS_MAX];      /* ticks em que cada vaga ficou livre e ainda nao foi reocupada (fila) */
    uint32_t nlivres;
    Vec linhas;                             /* TempoLinha */
} Tempos;

/* Le FORJA_ORQ_TEMPOS, cria/trunca o arquivo e arma o relogio. Falha ao criar = aviso no stderr e fica desligado. */
void TemposIniciar(Tempos *t);

/* Ticks de agora; 0 com a medicao desligada. */
uint64_t TemposAgora(const Tempos *t);

/* Registra a etapa que comecou em 'ini' (de TemposAgora) e termina agora. produto NULL = por execucao. */
void TemposFechar(Tempos *t, const char *etapa, const char *produto, uint64_t ini);

void TemposAnotarTicks(Tempos *t, const char *etapa, const char *produto, uint64_t ticks);
void TemposAnotarUs(Tempos *t, const char *etapa, const char *produto, uint64_t us);

/* Poe o campo "n" na ultima linha registrada. */
void TemposAcrescentarN(Tempos *t, int64_t n);

/* O orquestrador acabou de acordar da espera por um filho. */
void TemposAcordou(Tempos *t);

/* Do filho que acordou o orquestrador: vida_do_filho e latencia (FILETIME de criacao e de saida, em 100 ns); a vaga dele fica livre desde o instante em que acordou. */
void TemposDoFilho(Tempos *t, const char *produto, uint64_t criacao, uint64_t saida);

/* Um filho vai ocupar uma vaga: se havia vaga livre registra ocioso_do_slot (do acordar ate agora). */
void TemposSlotOcupado(Tempos *t, const char *produto);

/* Escreve as linhas no arquivo e desliga. Falha = aviso com a causa, nunca derruba o build. */
void TemposGravar(Tempos *t);

#endif
