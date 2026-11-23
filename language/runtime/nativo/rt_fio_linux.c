/*
 * rt_fio_linux.c - o RtFio por thread no Linux com libc (M5c-b P57, D-R3/D-R7): o fio principal, a ponte C -> Forja da thread de fora e a liberacao do fio que morre.
 *
 * Versao: 0.2.0
 * Changelog:
 *   0.2.0 - M6 P69: com -DFORJA_RASTREIO a ponte grava o topo da pilha .fj no fio da thread nova (panic em thread mostra a pilha da propria thread).
 *   0.1.0- M5c-b P57: primeira versao, o espelho de rt_fio.c (Windows) sobre a pthread_key do musl. Os ganchos rt_fio_obter/rt_fio_gravar (rt_fio.h) sao ligados por
 *           rt_linux_entrada_libc.c; na variante crua ficam NULL e nada daqui roda. A thread de fora (pthread_create de uma lib C chamando um fjt_) cria o fio na ponte e liga
 *           rt_multithread ANTES de alocar (a trava do heap vale a partir dai); o destrutor da chave (forja_fio_liberar) devolve frame, buffer do f"..." e o fio quando ela termina.
 *           Falha aqui NAO passa por forja_falha (ela le o RtFio, que e o que falta): escreve a causa em stderr e sai com 101, como o rt_fio.c do Windows.
 *
 * fjt_-16 (pendencia "P47/P57"): NAO e preciso neste passo. O runtime nao chama proc Forja por ponteiro a partir de codigo Win64: a rotina de thread chega ao pthread_create do musl como o fjt_
 * SysV (o stub que cria o fio na ponte); a entrada fjt_-16 (Win64, jmp fjtm_) so e para comparador/callback chamado pelo PROPRIO runtime, o que o P58 (rt_thread_linux.c) tambem nao faz.
 *
 * Incluido por rt_linux.c depois de rt_aloc.c (usa FORJA_FRAME_BYTES, sys_* e memset de rt_mem.c); nao compila sozinho.
 * Verificacao: construir.bat e python -I tests\nativo\linux\threads\verificar_threads_linux.py --forja <forja.exe>
 */
#ifndef RT_ALVO_LINUX
#error "rt_fio_linux.c so compila para Linux: use -DRT_ALVO_LINUX"
#endif

#define RT_FIO_CODIGO_SAIDA_FALHA 101

static const char rt_fio_sem_ponte[] = "{\"codigo\": \"R0007\", \"tipo\": \"runtime\", \"causa\": \"thread de fora entrou no runtime sem passar pela ponte C -> Forja\"}\n";
static const char rt_fio_sem_memoria[] = "{\"codigo\": \"R0007\", \"tipo\": \"runtime\", \"causa\": \"sem memoria para o fio do runtime\"}\n";

static _Noreturn void rt_fio_morrer(const char *texto, int64_t n)
{
    sys_escrever(2, texto, n);
    sys_sair(RT_FIO_CODIGO_SAIDA_FALHA);
}

/* O fio da thread principal: em .bss, o objeto de entrada o grava na chave no main. */
RtFio *forja_fio_principal(void)
{
    return &rt_fio_principal;
}

/* Thread que morre (destrutor da chave): devolve o que o fio dela guardava. O principal nunca e liberado. */
void forja_fio_liberar(RtFio *fio)
{
    if (fio == NULL || fio == &rt_fio_principal)
        return;
    if (fio->frame.base != NULL)
        sys_paginas_liberar(fio->frame.base, FORJA_FRAME_BYTES); /* no Linux munmap exige o tamanho */
    if (fio->formata.dados != NULL)
        sys_heap_liberar(fio->formata.dados);
    sys_heap_liberar(fio);
}

/* Chave sem valor: so a thread de fora que nao passou pela ponte chega aqui (a principal e as da Forja ja tem fio). */
static RtFio *rt_fio_sem_slot(void)
{
    rt_fio_morrer(rt_fio_sem_ponte, (int64_t)sizeof rt_fio_sem_ponte - 1);
}

/* Prologo da ponte: thread de fora (chave sem valor) ganha um fio zerado no heap. Antes de converter os argumentos. Variante crua: nada a fazer. */
void forja_fio_da_ponte(void)
{
    RtFio *fio;

    if (rt_fio_obter == NULL || rt_fio_obter() != NULL)
        return;
    sys_multithread_ligar();
    fio = (RtFio *)sys_heap_alocar((int64_t)sizeof *fio);
    if (fio == NULL)
        rt_fio_morrer(rt_fio_sem_memoria, (int64_t)sizeof rt_fio_sem_memoria - 1);
    memset(fio, 0, sizeof *fio);
#ifdef FORJA_RASTREIO
    /* M6 P69: o topo da pilha .fj da thread e o quadro de quem chamou a ponte (o wrapper do fjt_): [rbp] deste quadro e o rbp dele; a cadeia dos procs .fj da thread para ali. */
    fio->pilha.topo = *(uintptr_t **)__builtin_frame_address(0);
#endif
    rt_fio_gravar(fio);
}
