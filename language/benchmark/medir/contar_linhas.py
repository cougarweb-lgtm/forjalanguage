# contar_linhas.py
# Versão: 1.0.0
# Changelog: 1.0.0 — primeira versão: SLOC por protótipo (C/C++ com comentários de bloco; Python com ast e tokenize).
# Verificação: python contar_linhas.py --selftest

import argparse
import ast
import io
import json
import sys
import tempfile
import tokenize
from pathlib import Path

RAIZ_PADRAO = Path("C:/Users/USUARIO/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark")
PADROES = {
    "c": ("*.c", "*.h"),
    "cpp": ("*.cpp", "*.hpp"),
    "py": ("*.py",),
}
TIPOS_SEM_CODIGO = {
    tokenize.COMMENT, tokenize.NL, tokenize.NEWLINE, tokenize.INDENT,
    tokenize.DEDENT, tokenize.ENDMARKER, tokenize.ENCODING,
}
ARQUIVOS_TESTE = {
    "exemplo.c": [
        "// comentario",
        "int main(void) {   // codigo com comentario",
        '    char *s = "// nao e comentario";',
        "    /* bloco",
        "       ainda bloco */",
        "    int x = 1; /* fim */",
        "",
        "    /* sozinho */",
        "    return 0;",
        "}",
    ],
    "cabecalho.h": [
        "/* licenca",
        " * linha 2",
        " */",
        "#define X 1",
    ],
    "exemplo.cpp": [
        "#include <vector>",
        "// so comentario",
        'std::string s = "/* nao bloco */";',
        "int y = 2; /* comeca",
        "   termina */ int z = 3;",
    ],
    "x.hpp": ["// tudo comentario"],
    "exemplo.py": [
        "#!/usr/bin/env python",
        '"""Docstring de modulo.',
        "",
        "Mais uma linha.",
        '"""',
        "# comentario",
        "import os  # comentario no fim",
        "",
        "class Classe:",
        '    """Doc da classe."""',
        "",
        "    def Metodo(self):",
        '        """Doc do metodo',
        "        multilinha.",
        '        """',
        "        # so comentario",
        "        return os.sep",
        "",
        "",
        "def Funcao():",
        '    texto = """',
        "    isto e string de codigo, nao docstring",
        '    """',
        "    return texto",
    ],
    "_oculto.c": ["int oculto = 1;"],
    "__pycache__/ignorado.py": ["x = 1"],
}
ESPERADO_ARQUIVOS = {
    "c/exemplo.c": 5, "c/cabecalho.h": 1, "cpp/exemplo.cpp": 4, "cpp/x.hpp": 0, "py/exemplo.py": 9,
}


class ErroContagem(RuntimeError):
    """Falha da contagem; a mensagem diz o arquivo e o motivo."""


def FimDaString(Linha: str, Inicio: int) -> int:
    """Posição logo depois da aspa de fechamento, ou fim da linha; barra invertida pula o próximo caractere."""
    Aspa = Linha[Inicio]
    Indice = Inicio + 1
    while Indice < len(Linha):
        if Linha[Indice] == "\\":
            Indice += 2
        elif Linha[Indice] == Aspa:
            return Indice + 1
        else:
            Indice += 1
    return len(Linha)


def ExtrairCodigoC(Linha: str, EmBloco: bool) -> tuple:
    """Devolve o código da linha sem comentários e se um bloco /* continua na linha seguinte."""
    # Strings são puladas inteiras: "//" dentro de aspas não é comentário.
    Partes = []
    Indice = 0
    while Indice < len(Linha):
        if EmBloco:
            Fim = Linha.find("*/", Indice)
            if Fim < 0:
                return "".join(Partes), True
            EmBloco = False
            Indice = Fim + 2
        elif Linha.startswith("//", Indice):
            break
        elif Linha.startswith("/*", Indice):
            EmBloco = True
            Indice += 2
        elif Linha[Indice] in "\"'":
            Fim = FimDaString(Linha, Indice)
            Partes.append(Linha[Indice:Fim])
            Indice = Fim
        else:
            Partes.append(Linha[Indice])
            Indice += 1
    return "".join(Partes), EmBloco


def ContarC(Texto: str) -> int:
    """SLOC de C/C++: linhas com algo além de comentário (// e blocos /* */)."""
    EmBloco = False
    Total = 0
    for Linha in Texto.split("\n"):
        Codigo, EmBloco = ExtrairCodigoC(Linha, EmBloco)
        if Codigo.strip():
            Total += 1
    return Total


def EhDocstring(No: ast.AST) -> bool:
    """Verdadeiro se o nó é uma expressão com string literal, ou seja, uma docstring no início do corpo."""
    return isinstance(No, ast.Expr) and isinstance(No.value, ast.Constant) and isinstance(No.value.value, str)


def LinhasDeDocstring(Arvore: ast.AST) -> set:
    """Linhas ocupadas pela docstring de módulo, classe ou função, achadas pelo ast e não por regex."""
    Linhas = set()
    for No in ast.walk(Arvore):
        if not isinstance(No, (ast.Module, ast.ClassDef, ast.FunctionDef, ast.AsyncFunctionDef)):
            continue
        if not No.body or not EhDocstring(No.body[0]):
            continue
        Linhas.update(range(No.body[0].lineno, No.body[0].end_lineno + 1))
    return Linhas


def LinhasComCodigo(Texto: str) -> set:
    """Linhas (1-based) com algum token que não é comentário nem quebra de linha."""
    Linhas = set()
    for Token in tokenize.generate_tokens(io.StringIO(Texto).readline):
        if Token.type in TIPOS_SEM_CODIGO:
            continue
        Linhas.update(range(Token.start[0], Token.end[0] + 1))
    return Linhas


def ContarPy(Texto: str) -> int:
    """SLOC de Python: sem comentário '#', sem docstring de módulo/classe/função e sem linha vazia."""
    Arvore = ast.parse(Texto)
    Ignoradas = LinhasDeDocstring(Arvore)
    Fonte = Texto.split("\n")
    return sum(
        1 for Numero in LinhasComCodigo(Texto)
        if Numero not in Ignoradas and Fonte[Numero - 1].strip()
    )


def ContarArquivo(Caminho: Path) -> int:
    """SLOC de um arquivo: Python pelo ast e tokenize; os demais pelo varredor de comentários C."""
    Texto = Caminho.read_text(encoding="utf-8-sig", errors="replace")
    if Caminho.suffix.lower() == ".py":
        try:
            return ContarPy(Texto)
        except SyntaxError as Erro:
            raise ErroContagem(f"{Caminho} não parseia como Python: {Erro}") from Erro
    return ContarC(Texto)


def ArquivosDoPrototipo(Raiz: Path, Padroes: tuple) -> list:
    """Arquivos da raiz que casam com os padrões; ignora nomes que começam com '_' e a pasta __pycache__."""
    Achados = set()
    for Padrao in Padroes:
        for Caminho in Raiz.rglob(Padrao):
            if not Caminho.is_file() or Caminho.name.startswith("_"):
                continue
            if "__pycache__" in Caminho.relative_to(Raiz).parts:
                continue
            Achados.add(Caminho)
    return sorted(Achados)


def ColetarContagens(Raiz: Path) -> list:
    """Uma entrada por arquivo: protótipo, caminho relativo à raiz e SLOC."""
    Linhas = []
    for Prototipo, Padroes in PADROES.items():
        for Caminho in ArquivosDoPrototipo(Raiz / Prototipo, Padroes):
            Linhas.append({
                "prototipo": Prototipo,
                "arquivo": Caminho.relative_to(Raiz).as_posix(),
                "sloc": ContarArquivo(Caminho),
            })
    return Linhas


def TotalizarPorPrototipo(Linhas: list) -> dict:
    """Soma SLOC e conta arquivos por protótipo."""
    Totais = {Prototipo: {"sloc": 0, "arquivos": 0} for Prototipo in PADROES}
    for Linha in Linhas:
        Dados = Totais[Linha["prototipo"]]
        Dados["sloc"] += Linha["sloc"]
        Dados["arquivos"] += 1
    return Totais


def ImprimirTabela(Linhas: list, Totais: dict) -> None:
    """Tabela por arquivo e, embaixo, o total de cada protótipo."""
    print(f"{'prototipo':<10}{'sloc':>8}  arquivo")
    for Linha in Linhas:
        print(f"{Linha['prototipo']:<10}{Linha['sloc']:>8}  {Linha['arquivo']}")
    print("-" * 60)
    for Prototipo, Dados in Totais.items():
        print(f"{'TOTAL ' + Prototipo:<10}{Dados['sloc']:>8}  ({Dados['arquivos']} arquivos)")


def GravarJson(Caminho: Path, Dados: dict) -> None:
    """Grava o resultado em JSON, criando a pasta se faltar."""
    Caminho.parent.mkdir(parents=True, exist_ok=True)
    Caminho.write_text(json.dumps(Dados, ensure_ascii=False, indent=2), encoding="utf-8")


def PastaDoTeste(Nome: str) -> str:
    """Pasta do protótipo onde o arquivo de exemplo é gravado, pela extensão."""
    Sufixo = Path(Nome).suffix
    if Sufixo in (".c", ".h"):
        return "c"
    if Sufixo in (".cpp", ".hpp"):
        return "cpp"
    return "py"


def EscreverArquivosTeste(Raiz: Path) -> None:
    """Cria na pasta temporária os arquivos de exemplo com cada tipo de comentário."""
    for Nome, Linhas in ARQUIVOS_TESTE.items():
        Caminho = Raiz / PastaDoTeste(Nome) / Nome
        Caminho.parent.mkdir(parents=True, exist_ok=True)
        Caminho.write_text("\n".join(Linhas) + "\n", encoding="utf-8")


def VerificarArquivos(Linhas: list) -> None:
    """Confere que a lista de arquivos e o SLOC de cada um batem com o esperado."""
    Obtidos = {Linha["arquivo"]: Linha["sloc"] for Linha in Linhas}
    if set(Obtidos) != set(ESPERADO_ARQUIVOS):
        raise ErroContagem(
            f"arquivos contados {sorted(Obtidos)} diferem dos esperados {sorted(ESPERADO_ARQUIVOS)}; "
            f"os que começam com '_' e o __pycache__ devem ficar de fora"
        )
    for Arquivo, Esperado in ESPERADO_ARQUIVOS.items():
        if Obtidos[Arquivo] != Esperado:
            raise ErroContagem(f"{Arquivo}: esperava SLOC {Esperado}, veio {Obtidos[Arquivo]}")


def Autoteste() -> int:
    """Gera exemplos com comentários de cada tipo em pasta temporária e confere as contagens."""
    try:
        with tempfile.TemporaryDirectory() as Tmp:
            Raiz = Path(Tmp)
            EscreverArquivosTeste(Raiz)
            VerificarArquivos(ColetarContagens(Raiz))
    except (ErroContagem, OSError) as Erro:
        print(f"SELFTEST FALHOU: {Erro}")
        return 1
    print("SELFTEST OK")
    return 0


def ConstruirParser() -> argparse.ArgumentParser:
    """Declara as opções da linha de comando."""
    Parser = argparse.ArgumentParser(description="Conta SLOC por protótipo (c, cpp, py) do benchmark Forja.")
    Parser.add_argument("--raiz", dest="Raiz", default=str(RAIZ_PADRAO), help="pasta raiz do benchmark")
    Parser.add_argument("--json", dest="Json", default=None, help="grava o resultado também neste JSON")
    Parser.add_argument("--selftest", dest="Selftest", action="store_true")
    return Parser


def Principal(Argumentos: list) -> int:
    """Ponto de entrada: --selftest ou contagem da raiz; erro vira mensagem e exit 1."""
    Args = ConstruirParser().parse_args(Argumentos)
    if Args.Selftest:
        return Autoteste()
    try:
        Raiz = Path(Args.Raiz)
        if not Raiz.is_dir():
            raise ErroContagem(f"--raiz não é uma pasta existente; valor recebido: {Raiz}")
        Linhas = ColetarContagens(Raiz)
        Totais = TotalizarPorPrototipo(Linhas)
    except (ErroContagem, OSError) as Erro:
        print(f"ERRO: {Erro}", file=sys.stderr)
        return 1
    ImprimirTabela(Linhas, Totais)
    if Args.Json:
        GravarJson(Path(Args.Json), {"raiz": str(Raiz), "arquivos": Linhas, "totais": Totais})
        print(f"JSON gravado em {Args.Json}")
    return 0


if __name__ == "__main__":
    sys.exit(Principal(sys.argv[1:]))
