#!/usr/bin/env python3
# servidor_http.py 0.2.0 - servidor HTTP/HTTPS de mentira para os testes de rede da Forja (vendor:net/net, marco rede R4/R5). So stdlib.
# 0.2.0 (marco rede R5): --tls <cert> [--chave <key>] liga TLS (ssl da stdlib); sem --tls o modo HTTP fica igual.
# Uso: python tests\rede\servidor_http.py [--prazo 30] [--tls tests\rede\certs\servidor.pem] [--chave tests\rede\certs\servidor.key]
# Escuta SO em 127.0.0.1, porta 0 (efemera); a 1a linha do stdout e a porta (so o numero) e o stdout e descarregado na hora.
# Morre sozinho depois de --prazo segundos (padrao 30), aconteca o que acontecer. Nenhum teste usa internet.
# Rotas: /eco (metodo, caminho, cabecalhos em minusculas e corpo em JSON; aceita qualquer metodo), /status/<n>, /atraso/<ms>,
#        /redir/<n> (302 ate /redir/0 = 200 "fim"), /binario/<n> (bytes i % 256), /grande (5 MiB, bytes i % 256).
# Certificados de teste: tests\rede\certs\ (gerar_certs.sh). Sem --chave, a chave e lida do proprio --tls (PEM unico).
# VERIFICAR: python tests\rede\servidor_http.py --prazo 5   (imprime a porta e fica de pe ate o prazo)
#            python tests\rede\servidor_http.py --prazo 5 --tls tests\rede\certs\servidor.pem --chave tests\rede\certs\servidor.key
import argparse
import json
import os
import ssl
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

TamanhoGrande = 5 * 1024 * 1024
PadraoBytes = bytes(range(256))


def GerarBytes(Quantidade: int) -> bytes:
    Repeticoes, Resto = divmod(Quantidade, 256)
    return PadraoBytes * Repeticoes + PadraoBytes[:Resto]


class Tratador(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"  # lint-ok: nome-fora-do-padrao atributo fixo do http.server

    def log_message(self, *_Args):  # lint-ok: nome-fora-do-padrao sobrescreve o http.server (silencia o log de acesso)
        pass

    def handle(self):  # lint-ok: nome-fora-do-padrao sobrescreve o http.server
        # O handshake TLS roda aqui, na thread do cliente (no accept travaria o servidor inteiro). Cliente que recusa o
        # certificado (CA ou SAN errados, teste de NetError.tls) ou fecha no meio nao tem o que responder: so silencia o erro.
        try:
            if isinstance(self.request, ssl.SSLSocket):
                self.request.do_handshake()
            super().handle()
        except (ssl.SSLError, OSError):  # lint-ok: iaf-erro-engolido cliente recusou o certificado ou fechou no meio; so o servidor de teste, sem resposta a dar
            pass

    def Responder(self, Status: int, Corpo: bytes, Tipo: str = "text/plain", Extras: dict = None):
        # 1xx, 204 e 304 nao levam corpo; HEAD leva so o Content-Length do corpo que existiria.
        SemCorpo = Status < 200 or Status in (204, 304)
        self.send_response(Status)
        self.send_header("Content-Type", Tipo)
        if not SemCorpo:
            self.send_header("Content-Length", str(len(Corpo)))
        for Nome, Valor in (Extras or {}).items():
            self.send_header(Nome, Valor)
        self.send_header("Connection", "close")
        self.end_headers()
        self.close_connection = True
        if self.command != "HEAD" and not SemCorpo:
            self.wfile.write(Corpo)

    def LerCorpo(self) -> bytes:
        Tamanho = int(self.headers.get("Content-Length") or 0)
        return self.rfile.read(Tamanho) if Tamanho > 0 else b""

    def Despachar(self):
        Corpo = self.LerCorpo()
        Caminho = self.path
        try:
            if Caminho.startswith("/eco"):
                Resposta = {"method": self.command, "path": Caminho,
                            "headers": {K.lower(): V for K, V in self.headers.items()},
                            "body": Corpo.decode("utf-8", errors="replace"), "body_len": len(Corpo)}
                self.Responder(200, json.dumps(Resposta).encode("utf-8"), "application/json")
            elif Caminho.startswith("/status/"):
                Numero = int(Caminho.split("/")[2])
                self.Responder(Numero, f"status {Numero}".encode("ascii"))
            elif Caminho.startswith("/atraso/"):
                time.sleep(int(Caminho.split("/")[2]) / 1000.0)
                self.Responder(200, b"atrasado")
            elif Caminho.startswith("/redir/"):
                Restam = int(Caminho.split("/")[2])
                if Restam > 0:
                    self.Responder(302, b"", Extras={"Location": f"/redir/{Restam - 1}"})
                else:
                    self.Responder(200, b"fim")
            elif Caminho.startswith("/binario/"):
                self.Responder(200, GerarBytes(int(Caminho.split("/")[2])), "application/octet-stream")
            elif Caminho == "/grande":
                self.Responder(200, GerarBytes(TamanhoGrande), "application/octet-stream")
            else:
                self.Responder(404, b"nao achou")
        except (BrokenPipeError, ConnectionResetError):
            pass  # o cliente desistiu (timeout dos testes)

    # Nomes fixos do http.server (nao sao CamelCase da casa).
    do_GET = do_POST = do_PUT = do_PATCH = do_DELETE = do_HEAD = Despachar  # lint-ok: nome-fora-do-padrao exigido por http.server


def CriarContextoTls(Certificado: str, Chave: str):
    Contexto = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    Contexto.minimum_version = ssl.TLSVersion.TLSv1_2
    Contexto.load_cert_chain(certfile=Certificado, keyfile=Chave)  # Chave None: le a chave do proprio PEM
    return Contexto


def Principal() -> int:
    Analise = argparse.ArgumentParser()
    Analise.add_argument("--prazo", type=float, default=30.0)
    Analise.add_argument("--tls", default=None)
    Analise.add_argument("--chave", default=None)
    Opcoes = Analise.parse_args()
    Servidor = ThreadingHTTPServer(("127.0.0.1", 0), Tratador)
    Servidor.request_queue_size = 128
    Servidor.daemon_threads = True
    if Opcoes.tls:  # certificado ruim derruba antes de anunciar a porta
        Contexto = CriarContextoTls(Opcoes.tls, Opcoes.chave)
        Servidor.socket = Contexto.wrap_socket(Servidor.socket, server_side=True, do_handshake_on_connect=False)
    print(Servidor.server_address[1], flush=True)
    threading.Timer(Opcoes.prazo, lambda: os._exit(0)).start()
    Servidor.serve_forever()
    return 0


if __name__ == "__main__":
    sys.exit(Principal())
