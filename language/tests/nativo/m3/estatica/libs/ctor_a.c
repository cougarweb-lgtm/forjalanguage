/* ctor_a.c - M3b P23f: membro de libfx_ctor.a com construtores. Tres do gcc (.ctors.65434 = prioridade 101, .ctors.65433 = 102 e .ctors sem sufixo = padrao, que rodam nessa ordem: 1, 2, 3) e um
 * do MSVC (ponteiro na secao .CRT$XCU). fx_ctor_ordem devolve 123 se rodaram na ordem certa e fx_ctor_xc devolve 7 se o .CRT$XCU rodou. Sem os construtores devolveriam 0. */
static int g_ordem = 0;
static int g_xc = 0;

__attribute__((constructor(102))) static void iniciar_b(void) { g_ordem = g_ordem * 10 + 2; }
__attribute__((constructor(101))) static void iniciar_a(void) { g_ordem = g_ordem * 10 + 1; }
__attribute__((constructor)) static void iniciar_c(void) { g_ordem = g_ordem * 10 + 3; }

static void iniciar_xc(void) { g_xc = 7; }
__attribute__((section(".CRT$XCU"), used)) static void (*const p_xc)(void) = iniciar_xc;

int fx_ctor_ordem(void) { return g_ordem; }
int fx_ctor_xc(void) { return g_xc; }
