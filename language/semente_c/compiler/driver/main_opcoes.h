/* main_opcoes.h - o que o main.c deriva das opcoes e do programa: pedido ao nativo, pastas de ligacao e cabecalho da DLL
 * (saiu do main.c 0.56.0 para ele ficar abaixo de 800 linhas).
 * Versao: 0.2.0
 * Changelog: 0.2.0 - 0.21.0 F4 passo 2a: sai ModoComTraco, CompilarC e MAIN_TAM_C (backend C aposentado); o include do emit_c.h e do cc.h saem com eles.
 *            0.1.0 - M6 P69: extracao sem mudar comportamento.
 * Verificacao: construir.bat && python tests\cli\verificar_cli.py --forja <forja.exe>
 */
#ifndef FORJA_DRIVER_MAIN_OPCOES_H
#define FORJA_DRIVER_MAIN_OPCOES_H

#include "driver/compilacao.h"
#include "driver/opcoes.h"
#include "nativo/nativo.h"

/* Pastas de biblioteca do programa: a do .fj raiz e, so com vendor:/engine:, vendor\ ao lado dela e %FORJA_ENGINE%\lib (-L e PATH). */
typedef struct { char raiz[600]; char extra[2][620]; const char *extras[2]; uint32_t nextras; } PastasDeLigacao;

/* Pasta absoluta do .fj (sem a barra final): o -L da ligacao e o inicio do PATH do forja run/test. */
void PastaDoFonte(const Compilacao *c, char *destino, size_t tam);

void PastasDoPrograma(const Compilacao *c, PastasDeLigacao *p);

NativoPedido PedidoNativo(const Opcoes *o, Compilacao *c, const char *saida);

/* Cabecalho C da DLL ao lado dela (<saida>.h): um prototipo por 'pub export proc'. */
int GravarCabecalho(const Opcoes *o, const char *arq_dll, const Compilacao *c);

/* DLL sem nenhum 'pub export proc': B0003 (diagnostico, exit 1). */
int DllSemExportados(void);

#endif
