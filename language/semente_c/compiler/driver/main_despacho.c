/* main_despacho.c - tabela de despacho dos subcomandos que nao passam pela leitura de opcoes (ver main_despacho.h).
 * Versao: 0.4.0
 * Changelog: 0.4.0 - F1 P1b: o subcomando 'fmt' sai daqui (RodarFmt vive no cmd_fmt.c, com pastas, --aplicar, --json e o --stdin).
 *            0.3.0 - F1 P1a: o subcomando 'fmt' (RodarFmt, so --stdin).
 *            0.2.0 - IA1-L3d1: o subcomando 'migrar' (RodarMigrar).
 *            0.1.0 - IA1-I4: primeira versao; os subcomandos e as condicoes sao os do main.c 0.49.0 (ligar, corrigir, consultar, mapa, spec e explicar --lista|--exemplos), mais o editar.
 * Verificacao: construir.bat && python tests\cli\verificar_cli.py --forja <forja.exe>
 */
#include <string.h>

#include "driver/cmd_consultar.h"
#include "driver/cmd_corrigir.h"
#include "driver/cmd_editar.h"
#include "driver/cmd_fmt.h"
#include "driver/cmd_mapa.h"
#include "driver/cmd_migrar.h"
#include "driver/cmd_spec.h"
#include "driver/ligar.h"
#include "driver/main_despacho.h"
#include "driver/versao.h"

/* O 'spec' imprime a linha do --versao; a tabela so guarda funcoes (argc, argv). */
static int RodarSpecComVersao(int argc, char **argv)
{
    return RodarSpec(argc, argv, FORJA_VERSAO_TEXTO);
}

/* argc_min = argc total (com "forja" e o nome) a partir do qual o subcomando vale; aplica (opcional) decide pelos argumentos que sobram. */
typedef struct {
    const char *nome;
    int argc_min;
    bool (*aplica)(int argc, char **argv);
    int (*rodar)(int argc, char **argv);
} Subcomando;

static const Subcomando SUBCOMANDOS[] = {
    {"ligar", 2, NULL, RodarLigar},
    {"corrigir", 2, NULL, RodarCorrigir},
    {"migrar", 2, NULL, RodarMigrar},
    {"fmt", 2, NULL, RodarFmt},
    {"editar", 2, NULL, RodarEditar},
    {"consultar", 2, NULL, RodarConsultar},
    {"mapa", 2, NULL, RodarMapa},
    {"spec", 2, NULL, RodarSpecComVersao},
    {"explicar", 3, ExplicarEhExtenso, RodarExplicarExtenso},
};

bool DespacharSubcomando(int argc, char **argv, int *codigo)
{
    for (size_t k = 0; k < sizeof SUBCOMANDOS / sizeof SUBCOMANDOS[0]; k++) {
        const Subcomando *s = &SUBCOMANDOS[k];

        if (argc < s->argc_min || strcmp(argv[1], s->nome) != 0) continue;
        if (s->aplica != NULL && !s->aplica(argc - 2, argv + 2)) continue;
        *codigo = s->rodar(argc - 2, argv + 2);
        return true;
    }
    return false;
}
