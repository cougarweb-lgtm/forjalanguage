"""Diagnostico da Forja: excecao unica que carrega codigo, posicao e mensagens.

Versao: 1.0.0
Changelog: 1.0.0 - ForjaErro com posicao por token ou por deslocamento, tipos e textos por codigo.
Verificacao: python C:/Users/USUARIO/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/medir/rodar_suite.py --compilador "python C:/Users/USUARIO/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/py/forja.py" --corpus C:/Users/USUARIO/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/corpus/dourado --registro C:/Users/USUARIO/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language/benchmark/py/_tentativas.jsonl
"""

TIPOS_POR_CODIGO = {
    "E0001": "lexico",
    "E0101": "sintaxe",
    "E0201": "nome",
    "E0301": "tipo",
    "E0302": "tipo",
    "E0412": "unidade",
    "E0420": "faixa",
    "E0501": "argumento",
    "E0502": "argumento",
    "E0503": "argumento",
    "E0504": "argumento",
}

EXPLICACAO_POR_CODIGO = {
    "E0001": "O texto tem um caractere que a linguagem nao conhece ou uma string sem fechar.",
    "E0101": "A estrutura do comando nao bate com a gramatica neste ponto.",
    "E0201": "O nome usado nao foi declarado antes (ou falta o proc main).",
    "E0301": "Os tipos nao combinam: nao ha conversao implicita entre categorias.",
    "E0302": "Um proc com retorno precisa terminar com return.",
    "E0412": "As dimensoes das unidades nao combinam ou nao ha unidade declarada para o resultado.",
    "E0420": "A constante esta fora da faixa declarada para o campo.",
    "E0501": "Os argumentos nomeados precisam seguir a ordem em que os parametros foram declarados.",
    "E0502": "O 1o argumento e posicional e os demais sao nomeados.",
    "E0503": "Falta um argumento obrigatorio (parametro sem padrao).",
    "E0504": "O nome nao existe como parametro do proc nem como campo da struct.",
}

FORMA_CERTA_POR_CODIGO = {
    "E0001": "Remova o caractere ou feche a string na mesma linha.",
    "E0101": "Escreva o comando na forma da gramatica, um por linha.",
    "E0201": "Declare o nome antes de usar, ou crie proc main().",
    "E0301": "Use o mesmo tipo dos dois lados ou converta com 'as'.",
    "E0302": "Termine o corpo do proc com return <valor>.",
    "E0412": "Use unidades de mesma dimensao ou declare a unidade do resultado.",
    "E0420": "Use um valor dentro da faixa 'in a..=b' ou 'in a..<b'.",
    "E0501": "Ponha os argumentos nomeados na ordem dos parametros, sem repetir.",
    "E0502": "Escreva f(valor, nome = valor, ...): so o 1o e posicional.",
    "E0503": "Passe o argumento faltante ou de um padrao ao parametro.",
    "E0504": "Use o nome de um parametro (ou campo) existente.",
}


class ForjaErro(Exception):
    """Diagnostico da linguagem. Posicao: Tok (indice de token) ou Deslocamento (offset no texto)."""

    def __init__(self, Codigo: str, Causa: str, Tok: int = -1, Deslocamento: int = -1) -> None:
        super().__init__(f"{Codigo}: {Causa}")
        self.Codigo = Codigo
        self.Causa = Causa
        self.Tok = Tok
        self.Deslocamento = Deslocamento


class PosicaoFixaErro(ForjaErro):
    """Diagnostico com linha e coluna ja conhecidas (ex.: proc main ausente em 1:1)."""

    def __init__(self, Codigo: str, Causa: str, Linha: int, Coluna: int) -> None:
        super().__init__(Codigo, Causa)
        self.Linha = Linha
        self.Coluna = Coluna


class EntradaSaidaErro(Exception):
    """Falha ao abrir ou gravar arquivo (exit 2)."""
