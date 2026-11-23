# Oraculo da std:image (vendor\image, 0.1.0) para o exemplo image_basico.fj.
#
# Fonte independente da stb: o Pillow (libpng e libjpeg-turbo) decodifica os bytes que este script grava,
# e a definicao dos pixels (o que o script escreveu) confere a decodificacao. Nada daqui chama a stb.
#
# Uso (a partir de language\exemplos_libs):
#     python oraculos\image.py --gerar                                   grava dados\ (PNG, GIF, JPEG e bytes embutidos)
#     python oraculos\image.py --verificar --forja <forja.exe absoluto>  roda image_basico.fj (sem args e com a pasta dados\) e compara
#
# Pillow vem do Python do sistema (pip install pillow). Os fixtures sao so de teste e ficam em dados\.

import argparse
import hashlib
import io
import math
import os
import subprocess
import sys
import zlib

from PIL import Image

PASTA = os.path.dirname(os.path.abspath(__file__))
DADOS = os.path.join(PASTA, "dados")
EXEMPLO = os.path.join(os.path.dirname(PASTA), "image_basico.fj")
ESPERADO = os.path.join(os.path.dirname(PASTA), "image_basico.esperado")

# Mesma semente e formula do exemplo (aleatorio: LCG de 256 bytes); o oraculo gera os mesmos bytes para checar o Pillow.
SEMENTE = 12345
TAMANHO_ALEATORIO = 256
DIMENSAO_MAXIMA_STB = 16384  # STBI_MAX_DIMENSIONS da cola (1 << 14)
LIMITE_PIXELS_PILLOW = 178956970  # padrao do Pillow, restaurado depois do teste do cabecalho forjado


def BytesAleatorios():
    Estado = SEMENTE
    Saida = bytearray()
    for _ in range(TAMANHO_ALEATORIO):
        Estado = (Estado * 1103515245 + 12345) % 2147483648
        Saida.append((Estado // 65536) % 256)
    return bytes(Saida)


def Salvar(Imagem, Nome, Formato, **Opcoes):
    Caminho = os.path.join(DADOS, Nome)
    Imagem.save(Caminho, Formato, **Opcoes)
    with open(Caminho, "rb") as Arquivo:
        return Arquivo.read()


def FixturePngRgbaFonte():
    Largura, Altura = 16, 12
    Pixels = []
    for Linha in range(Altura):
        for Coluna in range(Largura):
            Pixels.append(((Coluna * 37 + Linha * 11) & 255, (Coluna * 5 + Linha * 71) & 255,
                           (Coluna * Coluna + 3 * Linha) & 255, (Coluna * 29 + Linha * 13 + 7) & 255))
    return (Largura, Altura), Pixels


def FixturePngRgba():
    (Largura, Altura), Pixels = FixturePngRgbaFonte()
    Imagem = Image.new("RGBA", (Largura, Altura))
    Imagem.putdata(Pixels)
    return Salvar(Imagem, "png_rgba.png", "PNG")


def FixturePngPaleta():
    Largura, Altura = 8, 6
    Paleta = [((Indice * 16) & 255, (Indice * 53) & 255, (Indice * 97) & 255) for Indice in range(16)]
    Alfas = bytes([0, 64, 128, 192, 255])  # tRNS: so as 5 primeiras entradas tem alfa; o resto e opaco
    Indices = [(Coluna + 3 * Linha) % 16 for Linha in range(Altura) for Coluna in range(Largura)]
    Imagem = Image.new("P", (Largura, Altura))
    Imagem.putpalette([Canal for Cor in Paleta for Canal in Cor])
    Imagem.putdata(Indices)
    return Salvar(Imagem, "png_paleta.png", "PNG", transparency=Alfas)


def FixturePng16():
    Largura, Altura = 8, 6
    Valores = [(Coluna * 4099 + Linha * 257) & 0xFFFF for Linha in range(Altura) for Coluna in range(Largura)]
    Imagem = Image.new("I;16", (Largura, Altura))
    Imagem.putdata(Valores)
    return Salvar(Imagem, "png16.png", "PNG")


def FixtureGif():
    Largura, Altura = 10, 8
    Paleta = [((Indice * 37 + 11) & 255, (Indice * 91) & 255, (255 - Indice * 29) & 255) for Indice in range(8)]
    Indices = [(Coluna * Coluna + Linha * 3) % 8 for Linha in range(Altura) for Coluna in range(Largura)]
    Imagem = Image.new("P", (Largura, Altura))
    Imagem.putpalette([Canal for Cor in Paleta for Canal in Cor])
    Imagem.putdata(Indices)
    return Salvar(Imagem, "gif_estatico.gif", "GIF")


def FixtureJpeg():
    Largura, Altura = 24, 16
    Pixels = [((Coluna * 10) & 255, (Linha * 15) & 255, ((Coluna + Linha) * 7) & 255)
              for Linha in range(Altura) for Coluna in range(Largura)]
    Imagem = Image.new("RGB", (Largura, Altura))
    Imagem.putdata(Pixels)
    return Salvar(Imagem, "jpeg.jpg", "JPEG", quality=90)


def PngForjado(Png2x2):
    # IHDR: largura em [16:20], altura em [20:24]; CRC do IHDR recalculado (so o cabecalho muda).
    Dados = bytearray(Png2x2)
    Dados[16:20] = (20000).to_bytes(4, "big")
    Dados[20:24] = (20000).to_bytes(4, "big")
    Crc = zlib.crc32(bytes(Dados[12:29])) & 0xFFFFFFFF
    Dados[29:33] = Crc.to_bytes(4, "big")
    return bytes(Dados)


def Png2x2():
    Imagem = Image.new("RGBA", (2, 2))
    Imagem.putdata([(255, 0, 0, 255), (0, 255, 0, 128), (0, 0, 255, 0), (255, 255, 255, 64)])
    Caminho = os.path.join(DADOS, "embutido_png_2x2.png")
    Imagem.save(Caminho, "PNG")
    with open(Caminho, "rb") as Arquivo:
        return Arquivo.read()


def Gif2x2():
    Imagem = Image.new("P", (2, 2))
    Imagem.putpalette([255, 0, 0, 0, 0, 255, 0, 255, 0])
    Imagem.putdata([0, 1, 2, 1])
    Caminho = os.path.join(DADOS, "embutido_gif_2x2.gif")
    Imagem.save(Caminho, "GIF")
    with open(Caminho, "rb") as Arquivo:
        return Arquivo.read()


def Gerar():
    os.makedirs(DADOS, exist_ok=True)
    FixturePngRgba()
    FixturePngPaleta()
    FixturePng16()
    FixtureGif()
    FixtureJpeg()
    P2 = Png2x2()
    G2 = Gif2x2()
    with open(os.path.join(DADOS, "embutido_png_forjado_20000.png"), "wb") as Arquivo:
        Arquivo.write(PngForjado(P2))
    print("fixtures gravadas em", DADOS)
    for Rotulo, Dados in (("PNG 2x2", P2), ("GIF 2x2", G2)):
        print(Rotulo, len(Dados), "bytes; hex para o .fj:")
        print(", ".join("0x%02X" % Byte for Byte in Dados))


def CanaisDaStb(Imagem, Formato):
    # Convencao da stb (stbi_info): GIF sempre devolve comp = 4 (comentario "GIF always returns *comp=4");
    # PNG com paleta devolve 4 so quando ha tRNS. Os demais seguem o numero de bandas do Pillow.
    if Formato == "GIF":
        return 4
    if Imagem.mode == "P":
        return 4 if "transparency" in Imagem.info else 3
    return {"L": 1, "I": 1, "I;16": 1, "LA": 2, "RGB": 3, "RGBA": 4}[Imagem.mode]


def RgbaDoPillow(Imagem):
    if Imagem.mode in ("I", "I;16"):
        # 16 bits -> 8 bits pelo byte de cima (v >> 8), a mesma reducao que a stb faz em stbi__convert_16_to_8
        Saida = bytearray()
        for Valor in Imagem.getdata():
            Cinza = Valor >> 8
            Saida += bytes((Cinza, Cinza, Cinza, 255))
        return bytes(Saida)
    return Imagem.convert("RGBA").tobytes()


def DecodificarPillow(Dados):
    Imagem = Image.open(io.BytesIO(Dados))
    Imagem.load()
    return Imagem


def LinhasDeImagem(Rotulo, Dados):
    # Linhas no formato exato do exemplo: "<rotulo> info w h c" e "<rotulo> rgba w h 4 sha256" (JPEG: "<rotulo> rgb w h 3", sem sha).
    Imagem = DecodificarPillow(Dados)
    Formato = Imagem.format
    Largura, Altura = Imagem.size
    Canais = CanaisDaStb(Imagem, Formato)
    Linhas = [f"{Rotulo} info {Largura} {Altura} {Canais}"]
    if Formato == "JPEG":
        Linhas.append(f"{Rotulo} rgb {Largura} {Altura} 3")
    else:
        Bruto = RgbaDoPillow(Imagem)
        assert len(Bruto) == Largura * Altura * 4, (Rotulo, len(Bruto), Largura, Altura)
        Linhas.append(f"{Rotulo} rgba {Largura} {Altura} 4 {hashlib.sha256(Bruto).hexdigest()}")
    return Linhas


def PillowRecusa(Dados):
    # Verdadeiro se o Pillow nao abre/decodifica os bytes. O erro e o resultado esperado do caso negativo.
    try:
        DecodificarPillow(Dados)
    except Exception:  # lint-ok iaf-erro-engolido: a excecao do Pillow e o sinal de "recusado" que o caso negativo exige
        return True
    return False


def LinhasNegativas():
    # Casos que o exemplo faz sem depender da pasta. Cada um confere com o Pillow (ou com a regra do cabecalho).
    P2 = Png2x2()
    Truncado = P2[: len(P2) // 2]
    if not PillowRecusa(Truncado):
        raise SystemExit("oraculo: PNG truncado foi aceito pelo Pillow")
    if not PillowRecusa(BytesAleatorios()):
        raise SystemExit("oraculo: bytes aleatorios foram aceitos pelo Pillow")
    if not PillowRecusa(b""):
        raise SystemExit("oraculo: vazio foi aceito pelo Pillow")
    Forjado = PngForjado(P2)
    # Pillow le so o cabecalho, com o limite de pixels desligado: confirma que o cabecalho diz 20000x20000.
    Image.MAX_IMAGE_PIXELS = None
    try:
        Cabecalho = Image.open(io.BytesIO(Forjado))
        if Cabecalho.size != (20000, 20000):
            raise SystemExit("oraculo: cabecalho forjado nao diz 20000x20000: %r" % (Cabecalho.size,))
    finally:
        Image.MAX_IMAGE_PIXELS = LIMITE_PIXELS_PILLOW
    if 20000 <= DIMENSAO_MAXIMA_STB:
        raise SystemExit("oraculo: limite da stb deveria ser menor que 20000")
    # Mesma ordem que imprimir_negativos em image_basico.fj.
    return [
        "png_truncado invalid_data",
        "aleatorio invalid_data",
        "vazio invalid_data",
        "png_forjado_info too_large",
        "png_forjado_decode too_large",
        "png_canais_5 unsupported",
    ]


def EsperadoEmbutido():
    return LinhasDeImagem("png", Png2x2()) + LinhasDeImagem("gif", Gif2x2()) + LinhasNegativas()


def EsperadoPasta():
    Linhas = []
    for Nome, Rotulo in (("png_rgba.png", "png_rgba"), ("png_paleta.png", "png_paleta"),
                         ("png16.png", "png16"), ("gif_estatico.gif", "gif_estatico"),
                         ("jpeg.jpg", "jpeg")):
        with open(os.path.join(DADOS, Nome), "rb") as Arquivo:
            Linhas += LinhasDeImagem(Rotulo, Arquivo.read())
    return Linhas + LinhasNegativas()


def ConferirFontesDeOrigem():
    # Sanidade: o que o Pillow decodifica do PNG RGBA tem de bater com o que o oraculo escreveu.
    _, Pixels = FixturePngRgbaFonte()
    with open(os.path.join(DADOS, "png_rgba.png"), "rb") as Arquivo:
        Imagem = DecodificarPillow(Arquivo.read())
    if RgbaDoPillow(Imagem) != bytes(Canal for Pixel in Pixels for Canal in Pixel):
        raise SystemExit("oraculo: PNG RGBA decodificado pelo Pillow difere da fonte")


def PsnrEMaxdiff(Stb, Pillow):
    assert len(Stb) == len(Pillow), (len(Stb), len(Pillow))
    Erros = [ValorStb - ValorPillow for ValorStb, ValorPillow in zip(Stb, Pillow)]
    Maxdiff = max(abs(Erro) for Erro in Erros)
    Mse = sum(Erro * Erro for Erro in Erros) / len(Erros)
    Psnr = math.inf if Mse == 0 else 10 * math.log10(255 * 255 / Mse)
    return Psnr, Maxdiff


def RodarForja(Forja, Args):
    # Argumentos do programa vem depois de `--` (sem ele o forja toma a pasta por 2o arquivo de entrada).
    Cmd = [Forja, "run", "--backend", "x64", EXEMPLO] + (["--"] + Args if Args else [])
    Proc = subprocess.run(Cmd, capture_output=True, text=True, timeout=600)
    if Proc.returncode != 0:
        raise SystemExit("forja run saiu com %d:\n%s\n%s" % (Proc.returncode, Proc.stdout, Proc.stderr))
    return [Linha.rstrip("\r") for Linha in Proc.stdout.split("\n") if Linha.strip() != ""]


def Comparar(Titulo, Obtido, Esperado):
    if Obtido != Esperado:
        print("DIVERGENCIA em", Titulo)
        for Indice in range(max(len(Obtido), len(Esperado))):
            Forja = Obtido[Indice] if Indice < len(Obtido) else "<faltou>"
            Oraculo = Esperado[Indice] if Indice < len(Esperado) else "<sobrou>"
            if Forja != Oraculo:
                print("  linha", Indice + 1, "forja:", Forja)
                print("  linha", Indice + 1, "oraculo:", Oraculo)
        raise SystemExit(1)
    print("ok:", Titulo, "(%d linhas)" % len(Esperado))


def Verificar(Forja):
    ConferirFontesDeOrigem()
    Comparar("sem args (embutido) x oraculo", RodarForja(Forja, []), EsperadoEmbutido())
    with open(ESPERADO, "r", encoding="utf-8") as Arquivo:
        Comparar("sem args x image_basico.esperado", RodarForja(Forja, []),
                 [Linha.rstrip("\r\n") for Linha in Arquivo if Linha.strip() != ""])
    Comparar("pasta dados x oraculo", RodarForja(Forja, [DADOS]), EsperadoPasta())
    # JPEG: stb e libjpeg-turbo divergem na IDCT; so informacao, nao aceite.
    with open(os.path.join(DADOS, "jpeg_stb_rgb.bin"), "rb") as Arquivo:
        RgbStb = Arquivo.read()
    with open(os.path.join(DADOS, "jpeg.jpg"), "rb") as Arquivo:
        RgbPillow = DecodificarPillow(Arquivo.read()).convert("RGB").tobytes()
    Psnr, Maxdiff = PsnrEMaxdiff(RgbStb, RgbPillow)
    print("info jpeg (nao e aceite): PSNR %.2f dB, maxdiff %d (stb x Pillow/libjpeg-turbo)" % (Psnr, Maxdiff))
    print("oraculo: TUDO CONFERE")


def Main():
    Parser = argparse.ArgumentParser(description="Oraculo da std:image (Pillow).")
    Parser.add_argument("--gerar", action="store_true")
    Parser.add_argument("--verificar", action="store_true")
    Parser.add_argument("--forja", default="")
    Opcoes = Parser.parse_args()
    if not Opcoes.gerar and not Opcoes.verificar:
        Parser.print_help()
        return 2
    if Opcoes.gerar:
        Gerar()
    if Opcoes.verificar:
        if not Opcoes.forja:
            raise SystemExit("use --forja <forja.exe absoluto>")
        Verificar(Opcoes.forja)
    return 0


if __name__ == "__main__":
    sys.exit(Main())
