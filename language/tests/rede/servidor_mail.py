#!/usr/bin/env python3
# servidor_mail.py 0.2.0 - servidor SMTP, IMAP e POP3 de mentira para os testes de rede da Forja (vendor:net/mail, marco rede R7 e R8). So stdlib.
# Changelog: 0.2.0 - R8: --imap e --pop3 (caixa fixa tests\rede\caixa.json, --caixa, --log); 0.1.0 - R7: --smtp.
# Uso: python tests\rede\servidor_mail.py --smtp|--imap|--pop3 [--prazo 30] [--tls | --starttls] [--cert tests\rede\certs\servidor.pem --chave tests\rede\certs\servidor.key]
#          [--auth PLAIN,LOGIN] [--usuario usuario] [--senha segredo] [--saida arquivo] [--rcpt-recusa addr] [--mudo] [--caixa caixa.json] [--log arquivo]
# --imap/--pop3: servem a caixa do --caixa (padrao tests\rede\caixa.json ao lado deste arquivo); so --tls (TLS implicito: imaps, pop3s), sem STARTTLS; login por LOGIN (IMAP) e USER/PASS (POP3)
#   com --usuario/--senha. IMAP: CAPABILITY, LOGIN, LIST, SELECT, UID SEARCH (ALL/UNSEEN/SEEN), UID STORE +FLAGS (\Seen|\Deleted), UID FETCH n BODY[], EXPUNGE, NOOP, LOGOUT.
#   POP3: CAPA, USER, PASS, STAT, LIST, RETR, DELE (vale no QUIT), RSET, NOOP, QUIT. O estado (lida, apagada) vive em memoria e vale entre conexoes da mesma execucao.
# --log: grava a conversa (">" o que o servidor recebeu, "<" o que enviou) para depurar o que o cliente pede.
# Escuta SO em 127.0.0.1, porta 0 (efemera); a 1a linha do stdout e a porta (so o numero), descarregada na hora.
# Morre sozinho depois de --prazo segundos (padrao 30). Nenhum teste usa internet.
# --tls = TLS implicito (smtps); --starttls = anuncia STARTTLS e so oferece AUTH depois dele; os dois pedem --cert (e --chave se a chave nao estiver no PEM).
# --auth: mecanismos aceitos (PLAIN, LOGIN); sem --auth nao ha autenticacao. Credencial certa = --usuario/--senha (padrao usuario/segredo).
# --rcpt-recusa addr: RCPT TO desse endereco responde 550 (os demais passam). --mudo: aceita a conexao e nunca fala (prova de timeout do cliente).
# --saida: a cada mensagem aceita grava (sobrescrevendo) o envelope, uma linha por item ("MAIL addr", "RCPT addr"), depois "stuffed=N"
#   (quantas linhas vieram com ponto dobrado, ou seja, o curl fez o dot-stuffing), a linha "--DATA--" e os bytes do DATA sem o stuffing e sem o ".".
# VERIFICAR: python tests\rede\servidor_mail.py --smtp --prazo 5   (imprime a porta e fica de pe ate o prazo)
import argparse
import base64
import json
import os
import shlex
import socketserver
import ssl
import sys
import threading
import time

MarcaDados = b"--DATA--\n"
CaixaPadrao = os.path.join(os.path.dirname(os.path.abspath(__file__)), "caixa.json")
TravaLog = threading.Lock()


def CriarContextoTls(Certificado: str, Chave: str):
    Contexto = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    Contexto.minimum_version = ssl.TLSVersion.TLSv1_2
    Contexto.load_cert_chain(certfile=Certificado, keyfile=Chave)  # Chave None: le a chave do proprio PEM
    return Contexto


def EnderecoDe(Argumento: str) -> str:
    """O que vem entre < e > em 'FROM:<a@b> SIZE=10'."""
    Inicio = Argumento.find("<")
    Fim = Argumento.find(">", Inicio + 1)
    return Argumento[Inicio + 1:Fim] if Inicio >= 0 and Fim > Inicio else Argumento.split(":", 1)[-1].strip()


class Tratador(socketserver.BaseRequestHandler):
    Opcoes = None  # preenchido em Principal
    Contexto = None

    def handle(self):  # lint-ok: nome-fora-do-padrao sobrescreve o socketserver
        # O handshake do TLS implicito roda aqui, na thread do cliente. Cliente que recusa o certificado ou fecha no meio
        # nao tem o que responder: so silencia o erro.
        self.Conexao = self.request
        self.Buffer = b""
        self.TlsAtivo = isinstance(self.Conexao, ssl.SSLSocket)
        self.Autenticado = False
        self.Envelope = []
        try:
            if self.Opcoes.mudo:
                time.sleep(self.Opcoes.prazo)
                return
            if self.TlsAtivo:
                self.Conexao.do_handshake()
            self.Enviar("220 localhost ESMTP servidor_mail de teste")
            self.Laco()
        except (ssl.SSLError, OSError):  # lint-ok: iaf-erro-engolido cliente recusou o certificado ou fechou no meio; so o servidor de teste, sem resposta a dar
            pass

    def Registrar(self, Sentido: str, Texto: str):
        if self.Opcoes.log:
            with TravaLog, open(self.Opcoes.log, "a", encoding="utf-8") as Arquivo:
                Arquivo.write(f"{Sentido} {Texto}\n")

    def Enviar(self, Texto: str):
        self.Registrar("<", Texto)
        self.Conexao.sendall(Texto.encode("utf-8") + b"\r\n")

    def EnviarBruto(self, Dados: bytes):
        self.Registrar("<", f"[{len(Dados)} bytes]")
        self.Conexao.sendall(Dados)

    def LerLinha(self):
        while b"\r\n" not in self.Buffer:
            Parte = self.Conexao.recv(4096)
            if not Parte:
                return None
            self.Buffer += Parte
        Linha, self.Buffer = self.Buffer.split(b"\r\n", 1)
        self.Registrar(">", Linha.decode("utf-8", errors="replace"))
        return Linha

    def Laco(self):
        while True:
            Linha = self.LerLinha()
            if Linha is None:
                return
            Texto = Linha.decode("utf-8", errors="replace")
            Verbo, _, Resto = Texto.partition(" ")
            if Verbo.upper() == "QUIT":
                self.Enviar("221 tchau")
                return
            self.Comando(Verbo.upper(), Resto)

    def Comando(self, Verbo: str, Resto: str):
        Tabela = {"EHLO": self.Ehlo, "HELO": self.Ehlo, "STARTTLS": self.IniciarTls, "AUTH": self.Autenticar,
                  "MAIL": self.Remetente, "RCPT": self.Destinatario, "DATA": self.Dados}
        if Verbo in Tabela:
            Tabela[Verbo](Resto)
        elif Verbo in ("RSET", "NOOP"):
            self.Envelope = []
            self.Enviar("250 ok")
        else:
            self.Enviar("502 comando nao implementado")

    def Ehlo(self, _Resto: str):
        Linhas = ["localhost", "SIZE 10485760", "8BITMIME"]
        if self.Opcoes.starttls and not self.TlsAtivo:
            Linhas.append("STARTTLS")
        if self.Opcoes.auth and (self.TlsAtivo or not self.Opcoes.starttls):
            Linhas.append("AUTH " + " ".join(self.Opcoes.auth))
        for Indice, Linha in enumerate(Linhas):
            self.Enviar(("250-" if Indice < len(Linhas) - 1 else "250 ") + Linha)

    def IniciarTls(self, _Resto: str):
        if not self.Opcoes.starttls or self.TlsAtivo:
            self.Enviar("503 STARTTLS fora de hora")
            return
        self.Enviar("220 pode iniciar o TLS")
        self.Conexao = self.Contexto.wrap_socket(self.Conexao, server_side=True)
        self.TlsAtivo = True
        self.Buffer = b""
        self.Envelope = []

    def LerBase64(self, Prompt: str) -> str:
        self.Enviar("334 " + base64.b64encode(Prompt.encode("ascii")).decode("ascii"))
        Linha = self.LerLinha() or b""
        return base64.b64decode(Linha).decode("utf-8", errors="replace")

    def CredencialPlain(self, Inicial: str) -> bool:
        if not Inicial:
            self.Enviar("334 ")
            Inicial = (self.LerLinha() or b"").decode("ascii", errors="replace")
        Partes = base64.b64decode(Inicial).decode("utf-8", errors="replace").split("\0")
        return len(Partes) == 3 and Partes[1] == self.Opcoes.usuario and Partes[2] == self.Opcoes.senha

    def CredencialLogin(self, Inicial: str) -> bool:
        Usuario = base64.b64decode(Inicial).decode("utf-8", errors="replace") if Inicial else self.LerBase64("Username:")
        Senha = self.LerBase64("Password:")
        return Usuario == self.Opcoes.usuario and Senha == self.Opcoes.senha

    def Autenticar(self, Resto: str):
        Mecanismo, _, Inicial = Resto.partition(" ")
        Mecanismo = Mecanismo.upper()
        if Mecanismo not in self.Opcoes.auth:
            self.Enviar("504 mecanismo nao suportado")
            return
        Ok = self.CredencialPlain(Inicial.strip()) if Mecanismo == "PLAIN" else self.CredencialLogin(Inicial.strip())
        self.Autenticado = Ok
        self.Enviar("235 2.7.0 autenticado" if Ok else "535 5.7.8 credencial invalida")

    def Remetente(self, Resto: str):
        if self.Opcoes.auth and not self.Autenticado:
            self.Enviar("530 5.7.0 autenticacao necessaria")
            return
        self.Envelope = ["MAIL " + EnderecoDe(Resto)]
        self.Enviar("250 ok")

    def Destinatario(self, Resto: str):
        Endereco = EnderecoDe(Resto)
        if Endereco == self.Opcoes.rcpt_recusa:
            self.Enviar("550 5.1.1 destinatario recusado")
            return
        self.Envelope.append("RCPT " + Endereco)
        self.Enviar("250 ok")

    def Dados(self, _Resto: str):
        self.Enviar("354 mande, termine com <CRLF>.<CRLF>")
        Corpo = b""
        Dobrados = 0
        while True:
            Linha = self.LerLinha()
            if Linha is None:
                return
            if Linha == b".":
                break
            if Linha.startswith(b".."):
                Linha = Linha[1:]
                Dobrados += 1
            Corpo += Linha + b"\r\n"
        if self.Opcoes.saida:
            Cabeca = "\n".join(self.Envelope + [f"stuffed={Dobrados}"]) + "\n"
            with open(self.Opcoes.saida, "wb") as Arquivo:
                Arquivo.write(Cabeca.encode("utf-8") + MarcaDados + Corpo)
        self.Envelope = []
        self.Enviar("250 2.0.0 mensagem aceita")


class Caixa:
    """A caixa de correio em memoria (lida de caixa.json), dividida entre as conexoes da mesma execucao do servidor."""

    def __init__(self, Caminho: str):
        with open(Caminho, encoding="utf-8") as Arquivo:
            Dados = json.load(Arquivo)
        self.Pastas = Dados["pastas"]
        self.UidValidity = Dados["uidvalidity"]
        self.Trava = threading.Lock()
        self.Mensagens = [{"uid": M["uid"], "pasta": M["pasta"], "lida": M["lida"], "apagada": False,
                           "bruto": ("\r\n".join(M["linhas"]) + "\r\n").encode("utf-8")} for M in Dados["mensagens"]]

    def DaPasta(self, Pasta: str) -> list:
        return [M for M in self.Mensagens if M["pasta"] == Pasta]


class TratadorImap(Tratador):
    Caixa = None  # preenchido em Principal

    def handle(self):  # lint-ok: nome-fora-do-padrao sobrescreve o socketserver
        self.Conexao = self.request
        self.Buffer = b""
        self.Logado = False
        self.Pasta = None
        try:
            if isinstance(self.Conexao, ssl.SSLSocket):
                self.Conexao.do_handshake()
            self.Enviar("* OK IMAP4rev1 servidor_mail de teste pronto")
            self.LacoImap()
        except (ssl.SSLError, OSError):  # lint-ok: iaf-erro-engolido cliente fechou ou recusou o certificado; so o servidor de teste
            pass

    def LacoImap(self):
        Tabela = {"CAPABILITY": self.Capacidade, "LOGIN": self.Entrar, "LIST": self.Listar, "SELECT": self.Selecionar,
                  "UID": self.ComandoUid, "EXPUNGE": self.Expurgar, "NOOP": self.Nada}
        while True:
            Linha = self.LerLinha()
            if Linha is None:
                return
            Marca, _, Resto = Linha.decode("utf-8", errors="replace").partition(" ")
            Verbo, _, Argumentos = Resto.partition(" ")
            if Verbo.upper() == "LOGOUT":
                self.Enviar("* BYE tchau")
                self.Enviar(f"{Marca} OK LOGOUT completed")
                return
            if Verbo.upper() in Tabela:
                Tabela[Verbo.upper()](Marca, Argumentos)
            else:
                self.Enviar(f"{Marca} BAD comando desconhecido")

    def Capacidade(self, Marca: str, _Argumentos: str):
        self.Enviar("* CAPABILITY IMAP4rev1")
        self.Enviar(f"{Marca} OK CAPABILITY completed")

    def Nada(self, Marca: str, _Argumentos: str):
        self.Enviar(f"{Marca} OK NOOP completed")

    def Entrar(self, Marca: str, Argumentos: str):
        Partes = shlex.split(Argumentos)
        self.Logado = len(Partes) == 2 and Partes[0] == self.Opcoes.usuario and Partes[1] == self.Opcoes.senha
        if self.Logado:
            self.Enviar(f"{Marca} OK LOGIN completed")
        else:
            self.Enviar(f"{Marca} NO [AUTHENTICATIONFAILED] credencial invalida")

    def Listar(self, Marca: str, _Argumentos: str):
        for Nome in self.Caixa.Pastas:
            self.Enviar(f'* LIST (\\HasNoChildren) "/" "{Nome}"')
        self.Enviar(f"{Marca} OK LIST completed")

    def Selecionar(self, Marca: str, Argumentos: str):
        Nome = (shlex.split(Argumentos) or [""])[0]
        if Nome not in self.Caixa.Pastas:
            self.Enviar(f"{Marca} NO pasta inexistente")
            return
        self.Pasta = Nome
        with self.Caixa.Trava:
            Total = len(self.Caixa.DaPasta(Nome))
        self.Enviar("* FLAGS (\\Seen \\Deleted)")
        self.Enviar(f"* {Total} EXISTS")
        self.Enviar("* 0 RECENT")
        self.Enviar(f"* OK [UIDVALIDITY {self.Caixa.UidValidity}] ok")
        self.Enviar(f"{Marca} OK [READ-WRITE] SELECT completed")

    def ComandoUid(self, Marca: str, Argumentos: str):
        Subverbo, _, Resto = Argumentos.partition(" ")
        Tabela = {"SEARCH": self.BuscarUid, "STORE": self.MarcarUid, "FETCH": self.BuscarCorpo}
        if self.Pasta is None or Subverbo.upper() not in Tabela:
            self.Enviar(f"{Marca} BAD UID invalido ou sem pasta selecionada")
            return
        Tabela[Subverbo.upper()](Marca, Resto)

    def BuscarUid(self, Marca: str, Criterio: str):
        Pedido = Criterio.strip().upper()
        if Pedido not in ("ALL", "UNSEEN", "SEEN"):
            self.Enviar(f"{Marca} BAD criterio nao suportado")
            return
        with self.Caixa.Trava:
            Achados = [M["uid"] for M in self.Caixa.DaPasta(self.Pasta)
                       if Pedido == "ALL" or (Pedido == "SEEN") == M["lida"]]
        self.Enviar("* SEARCH" + "".join(f" {U}" for U in Achados))
        self.Enviar(f"{Marca} OK SEARCH completed")

    def MarcarUid(self, Marca: str, Resto: str):
        Uid, _, Marcas = Resto.partition(" ")
        Atributo = "lida" if "\\Seen" in Marcas else "apagada"
        with self.Caixa.Trava:
            Lista = self.Caixa.DaPasta(self.Pasta)
            for Posicao, Mensagem in enumerate(Lista, start=1):
                if str(Mensagem["uid"]) == Uid:
                    Mensagem[Atributo] = True
                    self.Enviar(f"* {Posicao} FETCH (FLAGS ({self.Bandeiras(Mensagem)}) UID {Uid})")
        self.Enviar(f"{Marca} OK STORE completed")

    @staticmethod
    def Bandeiras(Mensagem: dict) -> str:
        return " ".join(N for N, Ligada in (("\\Seen", Mensagem["lida"]), ("\\Deleted", Mensagem["apagada"])) if Ligada)

    def BuscarCorpo(self, Marca: str, Resto: str):
        Uid, _, _Itens = Resto.partition(" ")
        with self.Caixa.Trava:
            for Posicao, Mensagem in enumerate(self.Caixa.DaPasta(self.Pasta), start=1):
                if str(Mensagem["uid"]) == Uid:
                    self.Enviar(f"* {Posicao} FETCH (UID {Uid} BODY[] {{{len(Mensagem['bruto'])}}}")
                    self.EnviarBruto(Mensagem["bruto"] + b")\r\n")
        self.Enviar(f"{Marca} OK FETCH completed")

    def Expurgar(self, Marca: str, _Argumentos: str):
        with self.Caixa.Trava:
            for Posicao, Mensagem in enumerate(self.Caixa.DaPasta(self.Pasta), start=1):
                if Mensagem["apagada"]:
                    self.Enviar(f"* {Posicao} EXPUNGE")
            self.Caixa.Mensagens = [M for M in self.Caixa.Mensagens if not (M["pasta"] == self.Pasta and M["apagada"])]
        self.Enviar(f"{Marca} OK EXPUNGE completed")


class TratadorPop3(Tratador):
    Caixa = None  # preenchido em Principal

    def handle(self):  # lint-ok: nome-fora-do-padrao sobrescreve o socketserver
        self.Conexao = self.request
        self.Buffer = b""
        self.UsuarioInformado = ""
        self.Logado = False
        self.Sessao = []  # as mensagens de INBOX no momento do login; o numero POP3 e a posicao + 1
        self.Marcadas = set()
        try:
            if isinstance(self.Conexao, ssl.SSLSocket):
                self.Conexao.do_handshake()
            self.Enviar("+OK servidor_mail de teste POP3 pronto")
            self.LacoPop3()
        except (ssl.SSLError, OSError):  # lint-ok: iaf-erro-engolido cliente fechou ou recusou o certificado; so o servidor de teste
            pass

    def LacoPop3(self):
        Tabela = {"CAPA": self.Capacidade, "USER": self.ComandoUser, "PASS": self.Senha, "STAT": self.Estado,
                  "LIST": self.Listar, "RETR": self.Recuperar, "DELE": self.Apagar, "RSET": self.Desfazer, "NOOP": self.Nada}
        while True:
            Linha = self.LerLinha()
            if Linha is None:
                return
            Verbo, _, Argumento = Linha.decode("utf-8", errors="replace").partition(" ")
            if Verbo.upper() == "QUIT":
                self.Fechar()
                return
            if Verbo.upper() in Tabela:
                Tabela[Verbo.upper()](Argumento.strip())
            else:
                self.Enviar("-ERR comando desconhecido")

    def Fechar(self):
        Apagadas = [M for I, M in enumerate(self.Sessao, start=1) if I in self.Marcadas]
        with self.Caixa.Trava:
            self.Caixa.Mensagens = [M for M in self.Caixa.Mensagens if not any(M is A for A in Apagadas)]
        self.Enviar("+OK tchau")

    def Capacidade(self, _Argumento: str):
        self.Enviar("+OK lista de capacidades")
        self.Enviar("USER")
        self.Enviar(".")

    def Nada(self, _Argumento: str):
        self.Enviar("+OK")

    def ComandoUser(self, Argumento: str):
        self.UsuarioInformado = Argumento
        self.Enviar("+OK")

    def Senha(self, Argumento: str):
        self.Logado = self.UsuarioInformado == self.Opcoes.usuario and Argumento == self.Opcoes.senha
        if not self.Logado:
            self.Enviar("-ERR [AUTH] credencial invalida")
            return
        with self.Caixa.Trava:
            self.Sessao = list(self.Caixa.DaPasta("INBOX"))
        self.Enviar("+OK logado")

    def Visiveis(self) -> list:
        return [(I, M) for I, M in enumerate(self.Sessao, start=1) if I not in self.Marcadas]

    def Estado(self, _Argumento: str):
        Visiveis = self.Visiveis()
        self.Enviar(f"+OK {len(Visiveis)} {sum(len(M['bruto']) for _, M in Visiveis)}")

    def Listar(self, Argumento: str):
        Visiveis = self.Visiveis()
        if Argumento:
            Achada = [(I, M) for I, M in Visiveis if str(I) == Argumento]
            self.Enviar(f"+OK {Achada[0][0]} {len(Achada[0][1]['bruto'])}" if Achada else "-ERR mensagem inexistente")
            return
        self.Enviar(f"+OK {len(Visiveis)} mensagens")
        for Numero, Mensagem in Visiveis:
            self.Enviar(f"{Numero} {len(Mensagem['bruto'])}")
        self.Enviar(".")

    def Recuperar(self, Argumento: str):
        Achada = [M for I, M in self.Visiveis() if str(I) == Argumento]
        if not Achada:
            self.Enviar("-ERR mensagem inexistente")
            return
        Corpo = b"\r\n".join(b"." + L if L.startswith(b".") else L for L in Achada[0]["bruto"].split(b"\r\n"))
        self.Enviar(f"+OK {len(Achada[0]['bruto'])} octetos")
        self.EnviarBruto(Corpo + b".\r\n")

    def Apagar(self, Argumento: str):
        Achada = [I for I, _ in self.Visiveis() if str(I) == Argumento]
        if not Achada:
            self.Enviar("-ERR mensagem inexistente")
            return
        self.Marcadas.add(Achada[0])
        self.Enviar("+OK apagada")

    def Desfazer(self, _Argumento: str):
        self.Marcadas.clear()
        self.Enviar("+OK")


class ServidorSmtp(socketserver.ThreadingTCPServer):
    allow_reuse_address = True  # lint-ok: nome-fora-do-padrao atributo fixo do socketserver
    daemon_threads = True  # lint-ok: nome-fora-do-padrao atributo fixo do socketserver
    request_queue_size = 64  # lint-ok: nome-fora-do-padrao atributo fixo do socketserver


def Principal() -> int:
    Analise = argparse.ArgumentParser()
    Analise.add_argument("--smtp", action="store_true")
    Analise.add_argument("--prazo", type=float, default=30.0)
    Analise.add_argument("--tls", action="store_true")
    Analise.add_argument("--starttls", action="store_true")
    Analise.add_argument("--cert", default=None)
    Analise.add_argument("--chave", default=None)
    Analise.add_argument("--auth", default="")
    Analise.add_argument("--usuario", default="usuario")
    Analise.add_argument("--senha", default="segredo")
    Analise.add_argument("--saida", default=None)
    Analise.add_argument("--rcpt-recusa", dest="rcpt_recusa", default=None)
    Analise.add_argument("--mudo", action="store_true")
    Analise.add_argument("--imap", action="store_true")
    Analise.add_argument("--pop3", action="store_true")
    Analise.add_argument("--caixa", default=CaixaPadrao)
    Analise.add_argument("--log", default=None)
    Opcoes = Analise.parse_args()
    if not (Opcoes.smtp or Opcoes.imap or Opcoes.pop3):
        print("uso: servidor_mail.py --smtp|--imap|--pop3 [...]", file=sys.stderr)
        return 2
    Opcoes.auth = [M.strip().upper() for M in Opcoes.auth.split(",") if M.strip()]
    Tratador.Opcoes = Opcoes
    Classe = TratadorImap if Opcoes.imap else TratadorPop3 if Opcoes.pop3 else Tratador
    if Opcoes.imap or Opcoes.pop3:
        Classe.Caixa = Caixa(Opcoes.caixa)
    Servidor = ServidorSmtp(("127.0.0.1", 0), Classe)
    if Opcoes.tls or Opcoes.starttls:  # certificado ruim derruba antes de anunciar a porta
        Tratador.Contexto = CriarContextoTls(Opcoes.cert, Opcoes.chave)
    if Opcoes.tls:
        Servidor.socket = Tratador.Contexto.wrap_socket(Servidor.socket, server_side=True, do_handshake_on_connect=False)
    print(Servidor.server_address[1], flush=True)
    threading.Timer(Opcoes.prazo, lambda: os._exit(0)).start()
    Servidor.serve_forever()
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
