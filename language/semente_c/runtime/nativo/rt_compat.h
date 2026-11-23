/*
 * rt_compat.h - a unica extensao do gcc (__attribute__) que o runtime nativo usa, num lugar so (regra 5.4 a do plano).
 *
 * Versao: 0.7.0
 * Changelog:
 *   0.7.0 - M3: RT_PALAVRA_MEM_TIPO (a palavra de 8 bytes de rt_mem.c 0.2.0; conferir_regras proibe __attribute__ fora deste arquivo).
 *   0.6.0 - M6e P93c: RT_SEMPRE_INLINE (o corpo comum de rt_pilha_ras e rt_pilha_ras_off em rt_pilha.c, um por tipo de saida, sem custo no laco).
 *   0.5.1 - M5c-b P63: RT_BARREIRA() passa de asm volatile para __atomic_signal_fence (conferir_regras proibe asm fora de rt_salto.c/rt_legado.c); mesmo efeito de compilador.
 *   0.5.0 - M5c-b P63: RT_NOIPA e RT_BARREIRA() (as travas do runtime Linux: o gcc lia o static protegido antes do sys_trava_pegar_srw e usava o valor velho depois de pegar a trava).
 *   0.4.0 - M5b P39-B3: RT_USADO (used: o main e o sys_sair do objeto de entrada libc nao somem no ligador).
 *   0.3.0 - M5 P26d: RT_ABI_SYSV (a convencao System V, so no tratador de sinal do Linux: o kernel o chama assim; o resto do runtime segue Win64).
 *   0.2.0 - M2 P12b: RT_RETORNA_DUAS_VEZES (forja_rt_salvar volta duas vezes, como setjmp: o gcc nao guarda valores em registrador por cima dele)
 *           e RT_NAO_INLINE (o executor de um teste fica isolado do laco do chamador, como o forja_rt_executar do forja_rt.h).
 *   0.1.0 - M2 P9a: RT_NORETORNO (noreturn de ExitProcess) sai de rt_sys_win.c; o expande para o mesmo atributo.
 *
 * Incluido por rt_sys_win.c (que e incluido por rt_win.c). Sem libc.
 */
#ifndef FORJA_RUNTIME_NATIVO_RT_COMPAT_H
#define FORJA_RUNTIME_NATIVO_RT_COMPAT_H

#define RT_NORETORNO __attribute__((noreturn))
#define RT_RETORNA_DUAS_VEZES __attribute__((returns_twice))
#define RT_NAO_INLINE __attribute__((noinline))
#define RT_SEMPRE_INLINE __attribute__((always_inline))
#define RT_ABI_SYSV __attribute__((sysv_abi))
#define RT_USADO __attribute__((used))
/* noipa: o gcc trata a funcao como se o corpo nao existisse para quem a chama (sem isso, na MESMA unidade, ele conclui que sys_trava_pegar_srw nao mexe em static nenhum e adianta a leitura do estado travado para antes da trava). */
#define RT_NOIPA __attribute__((noipa))
/* Barreira de compilador (nao emite instrucao): nada do estado travado passa para o outro lado dela. Builtin do gcc em vez de asm: a regra 5.4 proibe asm fora de rt_salto.c e rt_legado.c. */
#define RT_BARREIRA() __atomic_signal_fence(__ATOMIC_SEQ_CST)
/* Palavra de 8 bytes para copiar memoria (rt_mem.c): acesso nao alinhado (legal em x86-64) e may_alias (sem UB de aliasing ao reler bytes como palavra). */
#define RT_PALAVRA_MEM_TIPO unsigned long long __attribute__((may_alias, aligned(1)))

#endif
