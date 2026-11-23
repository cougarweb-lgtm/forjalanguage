/* pe_recursos.h - secao .rsrc do exe nativo: recurso de versao (RT_VERSION) e manifesto (RT_MANIFEST) que dizem quem gerou o exe.
 * Versao: 0.1.1
 * Changelog: 0.1.1 - M2 P9a fase 2: PeAcrescentarRecursos recebe Diag e devolve bool (a recusa de ImagemSecao chega ao chamador).
 *            0.1.0 - backend x64 P7b: PeAcrescentarRecursos (VERSIONINFO com ProductName "Forja Native" + manifesto asInvoker e Windows 10/11).
 * Verificacao: construir.bat && python -I ..\tests\nativo\pe\verificar_pe.py (item do exe real do 01_ola)
 */
#ifndef FORJA_NATIVO_FMT_PE_RECURSOS_H
#define FORJA_NATIVO_FMT_PE_RECURSOS_H

#include "nativo/fmt/pe.h"

/* Acrescenta a secao .rsrc em im (depois das que ja existem; ocupa 1 das IMAGEM_MAX_SECOES). versao = "maior.menor.patch" do forja.
 * Saida deterministica: sem data, sem contador, mesmos bytes para a mesma versao. Devolve false se ImagemSecao recusar (erro preenchido). */
bool PeAcrescentarRecursos(Imagem *im, const char *versao, Diag *erro);

#endif
