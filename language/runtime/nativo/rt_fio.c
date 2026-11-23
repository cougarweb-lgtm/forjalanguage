/*
 * rt_fio.c - o slot TLS do RtFio no Windows (M5 P43, T0-1): aloca o slot, liga o fio principal e cria o fio de uma thread que o runtime nao conhecia.
 *
 * Versao: 0.4.0
 * Changelog:
 *   0.4.0 - M5 P46c: rt_fio_novo vira rt_fio_sem_slot (so rt_fio_iniciar e o fio principal, sem heap: o 01_ola nao importa HeapAlloc) e a criacao do fio da thread de fora passa para forja_fio_da_ponte, chamada so no prologo da ponte C -> Forja.
 *   0.3.0 - M5 P45: rt_fio_liberar (DLL_THREAD_DETACH libera frame, buffer do f"..." e o fio da thread de fora; o fio principal fica). O flag rt_multithread se liga ANTES do CreateThread por forja_proc_multithread_ligar (rt_os_win.c, std:os multithread_on): a thread que ja esta numa secao quando o flag liga segue sem lock (ressalva do P44).
 *   0.2.0 - M5 P44: rt_fio_novo (thread que o runtime nao conhecia) liga o flag rt_multithread (sys_multithread_ligar).
 *   0.1.0 - M5 P43: primeira versao (rt_fio_iniciar e rt_fio_novo).
 *
 * Incluido por rt_win.c depois de rt_nativo.c (usa sys_*, memset de rt_mem.c e o 101 de falha); nao compila sozinho. So o alvo Windows: no Linux o RtFio e unico (rt_fio.h).
 * Falha de alocacao aqui NAO passa por forja_falha: forja_falha le o RtFio (em_teste), e o fio e o que falta. Escreve a causa em stderr e sai com 101.
 */
#ifndef RT_ALVO_WIN
#error "rt_fio.c so compila para Windows: use -DRT_ALVO_WIN"
#endif

#define RT_FIO_CODIGO_SAIDA_FALHA 101

static const char rt_fio_sem_indice[] = "{\"codigo\": \"R0007\", \"tipo\": \"runtime\", \"causa\": \"sem indice de TLS livre para o fio do runtime\"}\n";
static const char rt_fio_sem_ponte[] = "{\"codigo\": \"R0007\", \"tipo\": \"runtime\", \"causa\": \"thread de fora entrou no runtime sem passar pela ponte C -> Forja\"}\n";
static const char rt_fio_sem_memoria[] = "{\"codigo\": \"R0007\", \"tipo\": \"runtime\", \"causa\": \"sem memoria para o fio do runtime\"}\n";

static _Noreturn void rt_fio_morrer(const char *texto, int64_t n)
{
    sys_escrever(2, texto, n);
    sys_sair(RT_FIO_CODIGO_SAIDA_FALHA);
}

void rt_fio_iniciar(void)
{
    uint32_t indice;

    if (rt_fio_indice != 0)
        return;
    indice = sys_tls_alocar();
    if (indice == SYS_TLS_SEM_INDICE)
        rt_fio_morrer(rt_fio_sem_indice, (int64_t)sizeof rt_fio_sem_indice - 1);
    rt_fio_indice = indice + 1u;
    if (!sys_tls_gravar(indice, &rt_fio_principal))
        rt_fio_morrer(rt_fio_sem_indice, (int64_t)sizeof rt_fio_sem_indice - 1);
}

/* Thread que morre (DLL_THREAD_DETACH): devolve o que o fio dela guardava. Sem fio (a thread nunca chamou o runtime) ou o principal: nada a fazer. */
void rt_fio_liberar(void)
{
    RtFio *fio = rt_fio_indice == 0 ? NULL : (RtFio *)rt_fio_slot(rt_fio_indice - 1u);

    if (fio == NULL || fio == &rt_fio_principal)
        return;
    sys_tls_gravar(rt_fio_indice - 1u, NULL);
    if (fio->frame.base != NULL)
        sys_paginas_liberar(fio->frame.base, 0); /* no Windows MEM_RELEASE ignora o tamanho */
    if (fio->formata.dados != NULL)
        sys_heap_liberar(fio->formata.dados);
    sys_heap_liberar(fio);
}

/* Slot nulo: o runtime ainda nao iniciou (primeira chamada) e o fio principal fica no slot. Sem heap aqui: thread de fora so entra pela ponte (forja_fio_da_ponte). */
static RtFio *rt_fio_sem_slot(void)
{
    RtFio *fio;

    rt_fio_iniciar();
    fio = (RtFio *)rt_fio_slot(rt_fio_indice - 1u);
    if (fio == NULL)
        rt_fio_morrer(rt_fio_sem_ponte, (int64_t)sizeof rt_fio_sem_ponte - 1);
    return fio;
}

/* Prologo da ponte: thread de fora (slot nulo) ganha um fio zerado no heap do processo. Antes de converter os argumentos. */
void forja_fio_da_ponte(void)
{
    RtFio *fio;

    if (rt_fio_indice != 0 && rt_fio_slot(rt_fio_indice - 1u) != NULL)
        return;
    rt_fio_iniciar();
    if (rt_fio_slot(rt_fio_indice - 1u) != NULL)
        return;
    fio = (RtFio *)sys_heap_alocar((int64_t)sizeof *fio);
    if (fio == NULL)
        rt_fio_morrer(rt_fio_sem_memoria, (int64_t)sizeof rt_fio_sem_memoria - 1);
    memset(fio, 0, sizeof *fio);
    if (!sys_tls_gravar(rt_fio_indice - 1u, fio))
        rt_fio_morrer(rt_fio_sem_indice, (int64_t)sizeof rt_fio_sem_indice - 1);
    /* Thread de fora: a partir daqui o estado compartilhado (alocador, rastreio, saida, fs...) passa a ser travado (P44). */
    sys_multithread_ligar();
}
