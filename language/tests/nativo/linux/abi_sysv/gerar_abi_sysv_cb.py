# -*- coding: utf-8 -*-
"""gerar_abi_sysv_cb.py - gerador da bateria ABI SysV de CALLBACK (M5b P38-0): as 60 assinaturas de abi_sysv.json ao contrario.

Para cada assinatura NN, gera em casos_cb/:
- cb_sysv.c: cb_call_NN(cb) recebe um ponteiro com a assinatura SysV e o CHAMA com os valores do plano (ponteiros apontam para um
  static local da propria funcao). Devolve o que o callback devolver. Vira a lib libabi_sysv_cb.a.
- cbk_NN_<nome>.fj: declara cb_call_NN em foreign e passa o proc Forja cbk_NN_<nome>, que IMPRIME os argumentos que recebeu e
  devolve o valor do plano (retorno ptr = devolve o proprio ponteiro recebido).
- cb_impl_oraculo.c + oraculo_cb.c: o MESMO callback em C (cbimpl_NN) e o main que roda cb_call_NN(cbimpl_NN) por indice (argv[1]).
  A saida do oraculo e o esperado do cbk_NN.

Os valores saem de PlanejarArg (gerar_abi_sysv): um unico plano por caso, lido pelos dois lados. Diferente da chamada: nenhum
ponteiro e NULL, porque o callback so le o inteiro apontado.

Uso: python -I gerar_abi_sysv_cb.py
Versao: 0.1.0
Changelog: 0.1.0 (15/11/2026) M5b P38-0 - primeira versao (60 cb_call_NN + 60 cbk_NN .fj + oraculo C; reaproveita PlanejarArg, ExprC,
           ExprFj e os formatadores de gerar_abi_sysv 0.1.1).
Verificacao: python -I gerar_abi_sysv_cb.py && python -I verificar_abi_sysv_cb.py --forja <copia do forja.exe>
"""
import itertools
import json
import sys
from pathlib import Path

AQUI = Path(__file__).resolve().parent
sys.path.insert(0, str(AQUI))
import gerar_abi_sysv as Chamada  # noqa: E402

VersaoGerador = "0.1.0"
PASTA_CASOS = AQUI / "casos_cb"
LIB = "abi_sysv_cb"
C_LIB = PASTA_CASOS / "cb_sysv.c"
C_IMPL = PASTA_CASOS / "cb_impl_oraculo.c"
ORACULO_C = PASTA_CASOS / "oraculo_cb.c"


def TipoC(Tipo: str) -> str:
    return Chamada.ESCALAR_PARA_C.get(Tipo, Tipo)


def NomeCaso(Numero: int, Assinatura: dict) -> str:
    """Nome do caso Forja: proc definido em .fj tem de ser snake_case (E0102); o do JSON tem S01 em maiusculo."""
    return f"cbk_{Numero:02d}_{Assinatura['nome'].lower()}"


def IndiceDoUltimoPonteiro(Args: list):
    Indices = [Indice for Indice, Arg in enumerate(Args) if Arg["tipo"] == "ptr"]
    return Indices[-1] if Indices else None


def PlanejarCaso(Assinatura: dict, Structs: dict) -> dict:
    """Plano unico do caso: argumentos que a C passa ao callback e valor que o callback devolve."""
    Ordem = itertools.count()
    Args = [Chamada.PlanejarArg(Tipo, Structs, Ordem, Indice) for Indice, Tipo in enumerate(Assinatura["params"])]
    Ret = Chamada.PlanejarArg(Assinatura["ret"], Structs, Ordem, len(Args))
    if Assinatura["ret"] != "ptr":
        return {"args": Args, "ret": Ret, "ptr_ret": None}
    PtrRet = IndiceDoUltimoPonteiro(Args)
    if PtrRet is None:
        raise ValueError(f"{Assinatura['nome']}: retorno ptr sem parametro ptr para devolver")
    return {"args": Args, "ret": Ret, "ptr_ret": PtrRet}


def ExprRetorno(Plano: dict, Expressao) -> str:
    """Retorno ptr devolve o ponteiro recebido; os demais, o valor do plano na sintaxe da linguagem (Expressao)."""
    if Plano["ptr_ret"] is not None:
        return f"a{Plano['ptr_ret']}"
    return Expressao(Plano["ret"])


def ImpressoesDeExpr(Expressao: str, Tipo: str, Structs: dict, Imprimir) -> list:
    """Escalar: uma linha. Struct: uma linha por campo, como ImpressoesDoRetorno, mas para um nome de argumento."""
    if Tipo in Structs:
        return [Imprimir(f"{Expressao}.{Campo}", TipoCampo) for Campo, TipoCampo in Structs[Tipo]]
    return [Imprimir(Expressao, Tipo)]


def WrapperC(Numero: int, Assinatura: dict, Plano: dict) -> str:
    """cb_call_NN: a C chama o callback com a assinatura SysV e devolve o retorno dele."""
    Ret = TipoC(Assinatura["ret"])
    Tipos = ", ".join(TipoC(Tipo) for Tipo in Assinatura["params"])
    Locais = [f"    static int {Arg['nome']} = {Arg['valor']};" for Arg in Chamada.Locais(Plano["args"])]
    Chamadas = ", ".join(Chamada.ExprC(Arg) for Arg in Plano["args"])
    Cab = f"{Ret} cb_call_{Numero:02d}({Ret} (*cb)({Tipos}))"
    return "\n".join([Cab, "{", *Locais, f"    return cb({Chamadas});", "}", ""])


def ImplC(Numero: int, Assinatura: dict, Plano: dict, Structs: dict) -> str:
    """cbimpl_NN: o callback em C; imprime os argumentos como o cbk_NN .fj e devolve o plano."""
    Params = ", ".join(f"{TipoC(Tipo)} a{Indice}" for Indice, Tipo in enumerate(Assinatura["params"]))
    Corpo = []
    for Indice, Tipo in enumerate(Assinatura["params"]):
        Corpo += ["    " + L for L in ImpressoesDeExpr(f"a{Indice}", Tipo, Structs, Chamada.ImprimirC)]
    Corpo.append(f"    return {ExprRetorno(Plano, Chamada.ExprC)};")
    Cab = f"static {TipoC(Assinatura['ret'])} cbimpl_{Numero:02d}({Params})"
    return "\n".join([Cab, "{", *Corpo, "}", ""])


def ForeignFj(Numero: int, Assinatura: dict, Plano: dict) -> list:
    Ret = Chamada.TipoFj(Assinatura["ret"])
    Tipo = f"proc({Chamada.ParametrosFj(Plano['args'])}) -> {Ret}"
    return ["foreign {", f'    link "{LIB}"', f"    proc cb_call_{Numero:02d}(cb: {Tipo}) -> {Ret}", "}", ""]


def CallbackFj(Numero: int, Assinatura: dict, Plano: dict, Structs: dict) -> list:
    Corpo = []
    for Indice, Tipo in enumerate(Assinatura["params"]):
        Corpo += ["    " + L for L in ImpressoesDeExpr(f"a{Indice}", Tipo, Structs, Chamada.ImprimirFj)]
    Corpo.append(f"    return {ExprRetorno(Plano, Chamada.ExprFj)}")
    Cab = (f"proc {NomeCaso(Numero, Assinatura)}({Chamada.ParametrosFj(Plano['args'])}) "
           f"-> {Chamada.TipoFj(Assinatura['ret'])} {{")
    return [Cab, *Corpo, "}", ""]


def MainFj(Numero: int, Assinatura: dict, Structs: dict) -> list:
    Linhas = ["proc main() {", f"    let r = cb_call_{Numero:02d}({NomeCaso(Numero, Assinatura)})"]
    Linhas += ["    " + L for L in Chamada.ImpressoesDoRetorno(Assinatura["ret"], Structs, Chamada.ImprimirFj)]
    return [*Linhas, "}", ""]


def TextoFj(Numero: int, Assinatura: dict, Plano: dict, Structs: dict) -> str:
    Cab = [f"// {NomeCaso(Numero, Assinatura)}.fj - M5b P38-0: callback SysV da assinatura {Assinatura['nome']} "
           f"(lib {LIB}); gerado por gerar_abi_sysv_cb {VersaoGerador}, nao edite a mao.", ""]
    Linhas = Cab + Chamada.DeclaracoesFj(Assinatura, Structs)
    Linhas += ForeignFj(Numero, Assinatura, Plano) + CallbackFj(Numero, Assinatura, Plano, Structs) + MainFj(Numero, Assinatura, Structs)
    return "\n".join(Linhas)


def TextoLib(Casos: list) -> str:
    Cab = [f"/* cb_sysv.c - lib de callback SysV (M5b P38-0): cb_call_NN chama o callback com a assinatura NN. Gerado por "
           f"gerar_abi_sysv_cb {VersaoGerador}. Nao edite a mao. Build: construir_abi_sysv_cb.sh. */",
           '#include "../assinaturas.c"', ""]
    return "\n".join(Cab + [WrapperC(Numero, Assinatura, Plano) for Numero, Assinatura, Plano, _Structs in Casos])


def TextoImpl(Casos: list) -> str:
    Cab = [f"/* cb_impl_oraculo.c - callbacks em C da bateria de callback (M5b P38-0): cbimpl_NN imprime os argumentos e devolve o "
           f"plano, como cbk_NN .fj. Gerado por gerar_abi_sysv_cb {VersaoGerador}. Nao edite a mao. */", ""]
    return "\n".join(Cab + [ImplC(Numero, Assinatura, Plano, Structs) for Numero, Assinatura, Plano, Structs in Casos])


def CasoOraculo(Numero: int, Assinatura: dict, Structs: dict) -> str:
    Linhas = [f"    case {Numero}: {{", f"        {TipoC(Assinatura['ret'])} r = cb_call_{Numero:02d}(cbimpl_{Numero:02d});"]
    Linhas += ["        " + L for L in Chamada.ImpressoesDoRetorno(Assinatura["ret"], Structs, Chamada.ImprimirC)]
    Linhas += ["        break;", "    }"]
    return "\n".join(Linhas)


def TextoOraculo(Casos: list) -> str:
    """O oraculo roda UM caso por argv[1]; o callback C vem de cb_impl_oraculo.c (incluido)."""
    Cab = ["/* oraculo_cb.c - oraculo C da bateria de callback SysV (M5b P38-0): a mesma chamada de cada cbk_NN_<nome>.fj, "
           "compilada no alpine. Gerado por gerar_abi_sysv_cb " + VersaoGerador + ". Nao edite a mao. Uso: oraculo_cb <indice>.",
           " */", "#include <stdio.h>", "#include <stdlib.h>", '#include "cb_sysv.c"', '#include "cb_impl_oraculo.c"', "",
           "int main(int argc, char **argv)", "{", "    if (argc < 2) return 3;", "    switch (atoi(argv[1])) {"]
    Linhas = [CasoOraculo(Numero, Assinatura, Structs) for Numero, Assinatura, _Plano, Structs in Casos]
    return "\n".join(Cab + Linhas + ["    default:", "        return 2;", "    }", "    return 0;", "}", ""])


def CarregarAssinaturas() -> tuple:
    """(assinaturas do JSON, layout dos structs lido de assinaturas.c)."""
    Assinaturas = json.loads(Chamada.JSON_ABI.read_text(encoding="utf-8"))["assinaturas"]
    Structs = Chamada.CarregarStructs(Chamada.C_ASSINATURAS.read_text(encoding="utf-8"))
    return Assinaturas, Structs


def EscreverTexto(Caminho: Path, Texto: str) -> None:
    Caminho.write_text(Texto, encoding="utf-8", newline="\n")


def Gerar() -> int:
    """Escreve os 60 cbk_NN .fj, cb_sysv.c, cb_impl_oraculo.c e oraculo_cb.c. Devolve a quantidade de assinaturas."""
    Assinaturas, Structs = CarregarAssinaturas()
    PASTA_CASOS.mkdir(parents=True, exist_ok=True)
    Casos = [(Numero, Assinatura, PlanejarCaso(Assinatura, Structs), Structs) for Numero, Assinatura in enumerate(Assinaturas)]
    for Numero, Assinatura, Plano, Structs_ in Casos:
        Arquivo = PASTA_CASOS / f"{NomeCaso(Numero, Assinatura)}.fj"
        EscreverTexto(Arquivo, TextoFj(Numero, Assinatura, Plano, Structs_))
    EscreverTexto(C_LIB, TextoLib(Casos))
    EscreverTexto(C_IMPL, TextoImpl(Casos))
    EscreverTexto(ORACULO_C, TextoOraculo(Casos))
    return len(Assinaturas)


def NomesDosCasos() -> list:
    """Nome de cada caso (sem .fj), na ordem do abi_sysv.json: cbk_NN_<assinatura>."""
    Assinaturas, _Structs = CarregarAssinaturas()
    return [NomeCaso(Numero, Assinatura) for Numero, Assinatura in enumerate(Assinaturas)]


def Principal() -> int:
    Quantidade = Gerar()
    print(f"gerar_abi_sysv_cb: {Quantidade} casos em {PASTA_CASOS} + cb_sysv.c + oraculo_cb.c")
    return 0


if __name__ == "__main__":
    raise SystemExit(Principal())
