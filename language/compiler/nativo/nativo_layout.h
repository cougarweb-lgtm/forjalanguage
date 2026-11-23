/* nativo_layout.h - etapa ETAPA_LAYOUT do backend nativo: o layout\ do compilador confere consigo mesmo (LayoutConferir) e com o runtime (forja_rt_layout).
 * Versao: 0.2.1
 * Changelog: 0.2.1 - M2 P17b: so comentario (forja_rt_layout versao 3 confere tambem o map e a soa dinamica).
 *            0.2.0 - M2 P14b: NativoRtLayoutConferir recebe os Tipos e confere tambem o [dynamic]T (forja_rt_layout de 10 palavras, versao 2).
 *            0.1.0 - M2 P11b: NativoLayoutConferir e NativoRtLayoutConferir (primeiro produtor real do X0906).
 * Verificacao: construir.bat && teste_nativo.exe
 */
#ifndef FORJA_NATIVO_NATIVO_LAYOUT_H
#define FORJA_NATIVO_NATIVO_LAYOUT_H

#include "ir/layout.h"
#include "nativo/nativo.h"
#include "nativo/obj/objeto.h"

/* forja_rt_layout do runtime (Objeto lido do COFF) contra o layout\ de str, do primeiro [dynamic]T e do primeiro map de tipos (se houver) e de toda soa[dynamic]S: versao 3 conhecida, e tamanho,
 * alinhamento e offsets iguais.
 * false = X0906 em *erro ("layout de 'str': backend 16 B, runtime N B", idem '[dynamic]T', versao desconhecida ou simbolo ausente). */
bool NativoRtLayoutConferir(Objeto *runtime, const Layouts *layouts, const Tipos *tipos, Diag *erro);

/* Layout\ do programa (LayoutCalcular + LayoutConferir) e depois o do runtime. false = X0906 em *erro. */
bool NativoLayoutConferir(const NativoPedido *pedido, Objeto *runtime, Diag *erro);

#endif
