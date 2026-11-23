/* tls_helper.c - M5c-b P61: lib C com __thread nos dois modelos de acesso (initial-exec: GOTTPOFF, que o ligador relaxa para LE; local-exec: TPOFF32 direto)
 * e um __thread alinhado a 64 (o p_align do PT_TLS). Dois com inicializador (.tdata) e tres sem (.tbss). Lib: vendor/linux/libtls.a (construir_tls.sh, no alpine-build).
 * Nada disto entra no forja.exe. Simbolos com o prefixo tlst_ (forja_ e reservado para o compilador, E0905).
 * Versao: 0.1.0 - M5c-b P61: primeira versao.
 */

#define TLST_IE_BSS 0
#define TLST_IE_DAT 1
#define TLST_LE_BSS 2
#define TLST_LE_DAT 3
#define TLST_AL_BSS 4

__attribute__((tls_model("initial-exec"))) __thread int tlst_ie_bss;
__attribute__((tls_model("initial-exec"))) __thread int tlst_ie_dat = 1000;
__attribute__((tls_model("local-exec"))) __thread int tlst_le_bss;
__attribute__((tls_model("local-exec"))) __thread int tlst_le_dat = 2000;
__attribute__((tls_model("local-exec"), aligned(64))) __thread int tlst_al_bss;

/* Le a variavel k (TLST_*) da thread atual. */
int tlst_ler(int k)
{
    switch (k) {
    case TLST_IE_BSS: return tlst_ie_bss;
    case TLST_IE_DAT: return tlst_ie_dat;
    case TLST_LE_BSS: return tlst_le_bss;
    case TLST_LE_DAT: return tlst_le_dat;
    default: return tlst_al_bss;
    }
}

/* Grava v na variavel k (TLST_*) da thread atual. */
void tlst_gravar(int k, int v)
{
    switch (k) {
    case TLST_IE_BSS: tlst_ie_bss = v; break;
    case TLST_IE_DAT: tlst_ie_dat = v; break;
    case TLST_LE_BSS: tlst_le_bss = v; break;
    case TLST_LE_DAT: tlst_le_dat = v; break;
    default: tlst_al_bss = v; break;
    }
}
