/*
 * rt_processo_espera_linux.c - std:process no Linux x86-64: pid, wait_any, peak_memory e cpu_times (D-G3-L2b); o espelho Linux dos externos de mesmo nome do trecho Windows de forja_rt_processo.h.
 *
 * Versao: 0.1.0
 * Changelog:
 *   0.1.0 - D-G3-L2b: primeira versao (saiu do rt_processo_linux.c, que passaria de 800 linhas). forja_process_pid le o pid do slot; forja_process_wait_any faz poll por wait4 WNOHANG + nanosleep de 1 ms
 *           (sem pidfd_open: exigiria kernel >= 5.3 e poll por fd, e o polling e o mesmo do forja_processo_esperar); peak_memory e cpu_times saem do rusage do wait4 guardado em prl_colher.
 *           Limite de filhos do Linux = PRL_FILHOS_MAXIMO (8; o do Windows e 64): wait_any com mais de 8 e -2 (invalid_input). DIVERGENCIAS do Windows: peak_memory/cpu_times so valem DEPOIS do fim do
 *           filho (antes: 0, e end_ns = 0); o pico e o RSS maximo (ru_maxrss) do filho e dos descendentes que ele colheu, nao o COMMIT do Job.
 *
 * Incluido por rt_linux_es.c depois de rt_processo_linux.c (usa prl_tabela, prl_posicao, prl_colher, prl_agora_ns e prl_dormir_ns); nao compila sozinho.
 */

/* D-G3-L2b: pid do filho (guardado no slot); -1 = handle invalido. */
FORJA_ES_API int64_t forja_process_pid(uint64_t h)
{
    int32_t pos = prl_posicao(h);

    return pos < 0 ? -1 : prl_tabela[pos].pid;
}

/* Cada handle de ids -> posicao da tabela; false se algum e invalido (fechado, de outra geracao ou fora da faixa). */
static bool prl_resolver_ids(const uint64_t *ids, int64_t n, int32_t *posicoes)
{
    for (int64_t k = 0; k < n; k++) {
        posicoes[k] = prl_posicao(ids[k]);
        if (posicoes[k] < 0)
            return false;
    }
    return true;
}

/* Uma passada de wait4 WNOHANG sobre a lista: indice do primeiro filho ja colhido ou que acabou de terminar; -1 = nenhum ainda; PRL_WAIT_ANY_OUTRO = o wait4 falhou. */
static int64_t prl_primeiro_terminado(const int32_t *posicoes, int64_t n)
{
    for (int64_t k = 0; k < n; k++) {
        PrlSlot *s = &prl_tabela[posicoes[k]];
        int64_t r = s->colhido ? 0 : prl_colher(s, PRL_WNOHANG);

        if (s->colhido)
            return k;
        if (r < 0 && r != SYSL_EINTR)
            return PRL_WAIT_ANY_OUTRO;
    }
    return -1;
}

/* Espera o primeiro dos n filhos (1 a PRL_FILHOS_MAXIMO = 8 no Linux) terminar: >= 0 = indice em ids; -1 = o prazo passou (ms < 0 = sem prazo); -2 = lista vazia, acima do limite ou handle fechado; -3 = outro.
 * Mecanismo: poll por wait4(pid, WNOHANG) de cada filho + nanosleep de 1 ms ate o prazo. Custo: ate 1 ms de atraso entre o fim do filho e a resposta; ms = 0 e so uma consulta. */
FORJA_ES_API int64_t forja_process_wait_any(const uint64_t *ids, int64_t n, int64_t ms)
{
    int32_t posicoes[PRL_FILHOS_MAXIMO];
    int64_t limite = ms < 0 ? 0 : prl_agora_ns() + (ms > PRL_MS_MAXIMO ? PRL_MS_MAXIMO : ms) * PRL_NS_POR_MS;

    if (n <= 0 || n > PRL_FILHOS_MAXIMO || !prl_resolver_ids(ids, n, posicoes))
        return PRL_WAIT_ANY_INVALIDO;
    for (;;) {
        int64_t achou = prl_primeiro_terminado(posicoes, n);
        int64_t restante = limite - prl_agora_ns();

        if (achou != -1)
            return achou;
        if (ms >= 0 && restante <= 0)
            return -1;
        prl_dormir_ns(ms >= 0 && restante < PRL_PASSO_ESPERA_NS ? restante : PRL_PASSO_ESPERA_NS);
    }
}

/* Marca o filho como colhido se ele ja terminou (so consulta; o rusage so existe depois do wait4). */
static void prl_colher_se_terminou(PrlSlot *s)
{
    if (!s->colhido)
        prl_colher(s, PRL_WNOHANG);
}

/* >= 0 = pico de memoria residente (ru_maxrss * 1024) do filho e dos descendentes que ele colheu; < 0 = -codigo neutro (4 handle invalido).
 * DIVERGE do Windows (que mede o Job inteiro, em COMMIT, mesmo com o filho rodando): no Linux o rusage so existe depois do fim do filho; ANTES do fim devolve 0. */
FORJA_ES_API int64_t forja_process_peak_memory(uint64_t h)
{
    int32_t pos = prl_posicao(h);

    if (pos < 0)
        return -FORJA_PRC_ERR_ENTRADA;
    prl_colher_se_terminou(&prl_tabela[pos]);
    return prl_tabela[pos].pico_bytes;
}

/* 0 = feito: saida[0..3] = usuario, sistema, inicio, fim (ns; inicio = o spawn, fim = o instante em que o wait4 colheu o filho, ambos desde 1970); senao o codigo neutro (4 handle invalido).
 * DIVERGE do Windows (GetProcessTimes vale com o filho rodando): no Linux usuario e sistema so existem depois do fim (rusage do wait4); ANTES do fim saem 0, inicio vale e fim = 0. */
FORJA_ES_API int32_t forja_process_cpu_times(uint64_t h, int64_t *saida)
{
    int32_t pos = prl_posicao(h);
    PrlSlot *s = &prl_tabela[pos < 0 ? 0 : pos];

    if (pos < 0)
        return FORJA_PRC_ERR_ENTRADA;
    prl_colher_se_terminou(s);
    saida[0] = s->usuario_ns;
    saida[1] = s->sistema_ns;
    saida[2] = s->inicio_ns;
    saida[3] = s->fim_ns;
    return 0;
}
