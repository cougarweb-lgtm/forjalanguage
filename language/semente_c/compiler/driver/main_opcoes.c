/* main_opcoes.c - o que o main.c deriva das opcoes e do programa (ver main_opcoes.h).
 * Versao: 0.7.0
 * Changelog: 0.7.0 - Linguagem v1.13 L1b: PedidoNativo leva .pacotes (as pastas de modulo de pasta, linhas "pasta" do --entradas).
 *            0.6.0 - M8 P2: PedidoNativo leva .perfil (--perfil).
 *            0.5.0 - 0.21.0 F4 passo 2a: sai o backend C (ModoComTraco, TemExternoDeFora, ProgramaUsaNet, LigacaoDoPrograma, CompilarC); include backend/emit_h.h no lugar do emit_c.h; sai o include do cc.h.
 *            0.4.0 - M7a F0: PedidoNativo leva .sem_otim.
 *            0.3.0 - O0 P13: PedidoNativo leva .checagem (OpcoesChecagem).
 *            0.2.0 - M6e P93d: PedidoNativo leva .guarda_memoria (--guarda-memoria).
 *            0.1.0 - M6 P69: extracao de ModoComTraco, PastaDoFonte, PastasDoPrograma, LigacaoDoPrograma, CompilarC, PedidoNativo, GravarCabecalho e DllSemExportados do main.c 0.56.0, sem mudar comportamento.
 * Verificacao: construir.bat && python tests\cli\verificar_cli.py --forja <forja.exe>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backend/emit_h.h"
#include "common/buf.h"
#include "common/diag.h"
#include "driver/main_opcoes.h"
#include "driver/versao.h"
#include "frontend/modulos.h"

/* Pasta absoluta do .fj (sem a barra final): o -L da ligacao e o inicio do PATH do forja run/test. */
void PastaDoFonte(const Compilacao *c, char *destino, size_t tam)
{
    char absoluto[600];
    char *corte;

    if (_fullpath(absoluto, c->fonte.caminho, sizeof absoluto) == NULL) snprintf(absoluto, sizeof absoluto, ".\\x");
    corte = strrchr(absoluto, '\\');
    if (corte != NULL) *corte = '\0';
    snprintf(destino, tam, "%s", absoluto);
}

static void AcrescentarPastaExtra(PastasDeLigacao *p, const char *base, const char *sub)
{
    snprintf(p->extra[p->nextras], sizeof p->extra[0], "%s\\%s", base, sub);
    p->extras[p->nextras] = p->extra[p->nextras];
    p->nextras++;
}

/* A pasta extra da origem entra uma vez so: 'vistos' tem 1 bit por origem ja tratada. */
static void PastaExtraDaOrigem(PastasDeLigacao *p, uint8_t origem, uint32_t *vistos)
{
    if ((origem != MOD_VENDOR && origem != MOD_ENGINE) || (*vistos & (1u << origem)) != 0) return;
    *vistos |= 1u << origem;
    if (origem == MOD_VENDOR) {
        char raiz[600]; /* copia: a origem do texto nao pode ser o proprio PastasDeLigacao que recebe o resultado */
        snprintf(raiz, sizeof raiz, "%s", p->raiz);
        AcrescentarPastaExtra(p, raiz, "vendor");
        return;
    }
    const char *engine = getenv("FORJA_ENGINE");
    if (engine != NULL && engine[0] != '\0') AcrescentarPastaExtra(p, engine, "lib");
}

void PastasDoPrograma(const Compilacao *c, PastasDeLigacao *p)
{
    uint32_t vistos = 0;

    PastaDoFonte(c, p->raiz, sizeof p->raiz);
    p->nextras = 0;
    for (uint32_t k = 1; k < c->modulos.n; k++) PastaExtraDaOrigem(p, c->modulos.itens[k].origem, &vistos);
}

NativoPedido PedidoNativo(const Opcoes *o, Compilacao *c, const char *saida)
{
    return (NativoPedido){.ir = &c->ir, .tipos = &c->sem.tipos, .nomes = &c->nomes, .fonte = &c->fonte,
                          .modo = o->release ? BM_RELEASE : BM_DEV, .saida = saida, .alvo = o->target, .versao = FORJA_VERSAO,
                          .debug_code = o->debug_code, .tempos = &c->tempos, .comando = o->linha, .teste = ComandoEh(o, "test"),
                          .traco = o->traco, .dll = ModoEhDll(o), .lib_pasta = o->lib_pasta, .entradas = o->entradas, .cargas = &c->resolvidos, .pacotes = &c->pastas_carregadas,.rastreio = o->rastreio, .guarda_memoria = o->guarda_memoria, .perfil = o->perfil, .pie = o->pie, .simbolos = OpcoesSimbolos(o), .checagem = OpcoesChecagem(o), .sem_otim = o->sem_otim};
}

/* "x.dll" -> "x.h" (troca a extensao do nome do arquivo; sem extensao, acrescenta ".h"). */
static void CaminhoDoCabecalho(const char *arq_dll, char *destino, size_t tam)
{
    const char *ponto = strrchr(arq_dll, '.');
    const char *barra = strrchr(arq_dll, '/');
    const char *barra_invertida = strrchr(arq_dll, '\\');
    size_t base = strlen(arq_dll);

    if (barra_invertida != NULL && (barra == NULL || barra_invertida > barra)) barra = barra_invertida;
    if (ponto != NULL && (barra == NULL || ponto > barra)) base = (size_t)(ponto - arq_dll);
    snprintf(destino, tam, "%.*s.h", (int)base, arq_dll);
}

/* Cabecalho C da DLL ao lado dela (<saida>.h): um prototipo por 'pub export proc'. No backend x64 a gravacao e atomica, como a da DLL. */
int GravarCabecalho(const Opcoes *o, const char *arq_dll, const Compilacao *c)
{
    Buf texto = {0};
    char arq_h[600];
    int codigo = SAIDA_OK;

    CaminhoDoCabecalho(arq_dll, arq_h, sizeof arq_h);
    EmitirCabecalho(&c->ir, &c->sem.tipos, &c->nomes, &texto);
    if (!(BackendEhX64(o) ? ArquivoGravarAtomicoAviso(arq_h, texto.dados, texto.tam) : ArquivoGravar(arq_h, texto.dados, texto.tam))) codigo = SAIDA_USO;
    BufLiberar(&texto);
    return codigo;
}

/* DLL sem nenhum 'pub export proc' nao teria simbolo para chamar: B0003 (diagnostico, exit 1). */
int DllSemExportados(void)
{
    DiagImprimirJsonSemFonte(B0003, "o build --modo dll nao achou nenhum 'pub export proc' no arquivo", stderr);
    return SAIDA_DIAGNOSTICO;
}
