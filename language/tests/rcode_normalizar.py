"""Normaliza as linhas R (R-code de runtime) para comparar backends e execucoes: tira os campos que so o rastreio do x64 dev traz (D-M6-6).

Em dev o backend x64 enriquece a linha R com "coluna", "valor", "pilha" e "quadros" (pilha .fj padrao, M6 P69); o backend C so os traz com --rastreio.
Os verificadores que comparam stdout/stderr entre backends passam o texto por NormalizarLinhasR: este e o UNICO ponto que conhece a lista de campos
(nao espalhe copias). Linha que nao e JSON de R-code passa intacta, byte a byte.

Versao: 0.3.1
Changelog: 0.3.1 - M6e P93c: o R0008 do x64 dev leva tambem bloco, arquivo_alocacao e linha_alocacao (o C nao): saem junto.
           0.3.0 - M6e P93a: o R0008 ganha pilha_alocacao e pilha_delete (D-M6e-4); saem com o rastreio (ponto unico, D-M6-6).
           0.2.0 - M6 P80 (main): o R0009 perde tambem a forma_certa nos dois lados (o driver a reescreve pelo bloco que vazou, P70; o C traz a do catalogo): v111/teste/vaza_um divergia no grupo escopo do verificar_nativo.
           0.1.0 - M6 P69: primeira versao (NormalizarLinhasR, CAMPOS_RASTREIO).
Uso: from rcode_normalizar import NormalizarLinhasR  (tests\\ no sys.path)
Verificacao: python -I rcode_normalizar.py --selftest
"""
import json
import sys

CAMPOS_RASTREIO = ("coluna", "valor", "pilha", "quadros", "pilha_alocacao", "pilha_delete", "bloco", "arquivo_alocacao", "linha_alocacao")


def NormalizarLinha(Linha: str) -> str:
    """Uma linha de texto: se for o JSON de um R-code com campos do rastreio, devolve a mesma linha sem eles (mesma ordem das outras chaves)."""
    Corpo = Linha.strip()
    if not Corpo.startswith("{") or '"codigo": "R' not in Corpo:
        return Linha
    try:
        Dados = json.loads(Corpo)
    except ValueError:
        return Linha
    if not isinstance(Dados, dict) or not str(Dados.get("codigo", "")).startswith("R"):
        return Linha
    EhVazamento = Dados.get("codigo") == "R0009"
    if not EhVazamento and not any(Campo in Dados for Campo in CAMPOS_RASTREIO):
        return Linha
    for Campo in CAMPOS_RASTREIO:
        Dados.pop(Campo, None)
    if EhVazamento:
        Dados.pop("forma_certa", None)  # o driver reescreve a forma_certa do R0009 pelo que vazou (P70); o C traz a do catalogo
    return json.dumps(Dados, ensure_ascii=False)


def NormalizarLinhasR(Texto):
    """str ou bytes (UTF-8): normaliza cada linha; preserva o fim de linha original (\\n ou \\r\\n)."""
    if isinstance(Texto, bytes):
        return NormalizarLinhasR(Texto.decode("utf-8", "replace")).encode("utf-8")
    Linhas = Texto.split("\n")
    Saida = []
    for Linha in Linhas:
        Fim = "\r" if Linha.endswith("\r") else ""
        Saida.append(NormalizarLinha(Linha[: len(Linha) - len(Fim)]) + Fim)
    return "\n".join(Saida)


def Selftest() -> int:
    Com = '{"codigo": "R0006", "arquivo": "a.fj", "linha": 2, "coluna": 13, "causa": "x", "valor": {"indice": 7, "tamanho": 3}, "pilha": [{"proc": "p"}], "explicacao": "e"}'
    Sem = '{"codigo": "R0006", "arquivo": "a.fj", "linha": 2, "causa": "x", "explicacao": "e"}'
    Falhas = 0
    if NormalizarLinhasR(Com + "\r\n") != Sem + "\r\n":
        Falhas += 1
        print("falhou: R com rastreio", NormalizarLinhasR(Com))
    if NormalizarLinhasR(Sem) != Sem or NormalizarLinhasR(b"texto solto\n") != b"texto solto\n":
        Falhas += 1
        print("falhou: sem campos nao muda")
    if NormalizarLinhasR('{"codigo": "E0001", "coluna": 3}') != '{"codigo": "E0001", "coluna": 3}':
        Falhas += 1
        print("falhou: so R-code")
    VazaX64 = '{"codigo": "R0009", "tipo": "memoria", "forma_certa": "delete(p) antes de sair", "pilha": [{"proc": "p"}]}'
    VazaC = '{"codigo": "R0009", "tipo": "memoria", "forma_certa": "delete(x) quando o valor"}'
    if NormalizarLinhasR(VazaX64) != NormalizarLinhasR(VazaC) or "forma_certa" in NormalizarLinhasR(VazaC):
        Falhas += 1
        print("falhou: R0009 x64 e C normalizam para a mesma linha")
    Uso8 = '{"codigo": "R0008", "tipo": "heap", "arquivo": "a.fj", "linha": 4, "causa": "c", "pilha_alocacao": [{"proc": "a"}], "pilha_delete": [{"proc": "d"}], "pilha": [{"proc": "p"}]}'
    if NormalizarLinhasR(Uso8) != '{"codigo": "R0008", "tipo": "heap", "arquivo": "a.fj", "linha": 4, "causa": "c"}':
        Falhas += 1
        print("falhou: R0008 com 3 pilhas perde pilha_alocacao e pilha_delete", NormalizarLinhasR(Uso8))
    print("rcode_normalizar selftest:", "OK" if Falhas == 0 else f"{Falhas} falhas")
    return 1 if Falhas else 0


if __name__ == "__main__":
    sys.exit(Selftest())
