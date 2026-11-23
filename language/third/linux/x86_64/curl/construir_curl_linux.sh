#!/bin/sh
# construir_curl_linux.sh - gera libcurl.a ESTATICO para o alvo x86_64-linux (R10a do contrato_rede, D-REDE-7).
# Uso (no WSL, distro alpine-build): sh construir_curl_linux.sh [/mnt/c/Users/USUARIO/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language]
# Entrada (so leitura, sha256 conferido): vendor/net/curl/curl-8.23.0.tar.gz, vendor/net/mbedtls/mbedtls-3.6.8.tar.bz2,
#   vendor/net/curl_ponte.c/.h, vendor/net/curl/include.
# Saida: third/linux/x86_64/curl/libcurl.a (libcurl 8.23.0 + mbedcrypto + mbedx509 + mbedtls + curl_ponte.o) e a copia em vendor/linux/libcurl.a.
# Nada de pacote: so gcc, make, tar, ar, readelf, nm, sha256sum do alpine-build (sem cmake: o make do proprio Mbed TLS e o configure do curl bastam).
# Mbed TLS: config padrao + MBEDTLS_THREADING_C/PTHREAD (libcurl usado por varias threads; o entropy e o ctr_drbg do curl carregam a trava).
# Mbed TLS tambem com MBEDTLS_NO_UDBL_DIVISION (opcao oficial: sem divisao de 128 bits, que pediria __udivti3 da libgcc); curl com --without-pic (libtool gerava objetos PIC, que pedem _GLOBAL_OFFSET_TABLE_).
# libgcc_shim.c (nesta pasta) acrescenta __popcountdi2, o unico simbolo de libgcc que sobra (curl uint-bset.c usa __builtin_popcountll).
# Versao: 0.2.0
# Changelog: 0.2.0 (19/11/2026) R10a: NO_UDBL_DIVISION, --without-pic e libgcc_shim.o (o X0101 de __popcountdi2/__udivti3 e os objetos PIC do 0.1.0).
#            0.1.0 (19/11/2026) R10a: primeira versao.
set -eu

RAIZ="${1:-/mnt/c/Users/USUARIO/Documents/TEMPO/DESENVOLVIMENTO/FORJA/language}"
CURL_TAR="$RAIZ/vendor/net/curl/curl-8.23.0.tar.gz"
CURL_SHA="e3de77cfa6367083c776a26647fc9f633b538c06c3baa73d931a252ac8f72b49"
MBED_TAR="$RAIZ/vendor/net/mbedtls/mbedtls-3.6.8.tar.bz2"
MBED_SHA="bc54b818e87f49c0b3281dd85d24adaede0e63b4e822e3fbd49c005a38b9595b"
PONTE_INC="$RAIZ/vendor/net/curl/include"
DESTINO="$RAIZ/third/linux/x86_64/curl"
DESTINO_VENDOR="$RAIZ/vendor/linux"
FLAGS="-O2 -fno-pie -fno-asynchronous-unwind-tables -fno-stack-protector"

conferir() {
    Obtido="$(sha256sum "$1" | cut -d' ' -f1)"
    [ "$Obtido" = "$2" ] || { echo "sha256 de $1 diverge: $Obtido != $2" >&2; exit 1; }
}
conferir "$CURL_TAR" "$CURL_SHA"
conferir "$MBED_TAR" "$MBED_SHA"

TRAB="$(mktemp -d /tmp/r10a_curl.XXXXXX)"
trap 'rm -rf "$TRAB"' EXIT
cd "$TRAB"
tar -xzf "$CURL_TAR"
tar -xjf "$MBED_TAR"

# ---- Mbed TLS: config padrao + threading pthread; make static (3 .a)
MB="$TRAB/mbedtls-3.6.8"
sed -i 's|^//#define MBEDTLS_THREADING_C|#define MBEDTLS_THREADING_C|; s|^//#define MBEDTLS_THREADING_PTHREAD|#define MBEDTLS_THREADING_PTHREAD|' "$MB/include/mbedtls/mbedtls_config.h"
sed -i 's|^//#define MBEDTLS_NO_UDBL_DIVISION|#define MBEDTLS_NO_UDBL_DIVISION|' "$MB/include/mbedtls/mbedtls_config.h"
grep -q '^#define MBEDTLS_NO_UDBL_DIVISION' "$MB/include/mbedtls/mbedtls_config.h"
grep -q '^#define MBEDTLS_THREADING_C' "$MB/include/mbedtls/mbedtls_config.h"
grep -q '^#define MBEDTLS_THREADING_PTHREAD' "$MB/include/mbedtls/mbedtls_config.h"
make -C "$MB/library" -j8 static CC=gcc CFLAGS="$FLAGS"
PREFIXO="$TRAB/prefixo"
mkdir -p "$PREFIXO/include" "$PREFIXO/lib"
cp -r "$MB/include/mbedtls" "$MB/include/psa" "$PREFIXO/include/"
cp "$MB/library/libmbedcrypto.a" "$MB/library/libmbedx509.a" "$MB/library/libmbedtls.a" "$PREFIXO/lib/"

# ---- libcurl: configure fora da arvore, so protocolos pedidos + mbedtls estatico
mkdir "$TRAB/build_curl"
cd "$TRAB/build_curl"
"$TRAB/curl-8.23.0/configure" CC=gcc CFLAGS="$FLAGS" LDFLAGS="-static -no-pie" \
    --disable-shared --enable-static --without-pic --disable-docs --disable-manual \
    --disable-threaded-resolver --disable-ldap --disable-ldaps \
    --with-mbedtls="$PREFIXO" --with-ca-bundle=/etc/ssl/certs/ca-certificates.crt --without-ca-path \
    --without-zlib --without-brotli --without-zstd --without-libidn2 --without-libpsl --without-nghttp2 \
    --without-libssh2 --without-libssh
make -C lib -j8 libcurl.la

# ---- cola (curl_ponte.o) com os mesmos flags
gcc $FLAGS -Wall -DCURL_STATICLIB -I "$PONTE_INC" -c "$RAIZ/vendor/net/curl_ponte.c" -o "$TRAB/curl_ponte.o"

# ---- shim do unico simbolo de libgcc que falta; o .o nao pode pedir __popcountdi2 de volta
gcc $FLAGS -Wall -c "$DESTINO/libgcc_shim.c" -o "$TRAB/libgcc_shim.o"
! nm "$TRAB/libgcc_shim.o" | grep -q ' U __popcountdi2'

# ---- junta tudo em UM .a: cada archive extraido numa pasta propria, membros com prefixo (curl e mbedtls tem version.o)
JUNTA="$TRAB/junta"
mkdir "$JUNTA"
for Arq in "$TRAB/build_curl/lib/.libs/libcurl.a:curl" "$PREFIXO/lib/libmbedcrypto.a:mbedcrypto" "$PREFIXO/lib/libmbedx509.a:mbedx509" "$PREFIXO/lib/libmbedtls.a:mbedtls"; do
    Origem="${Arq%%:*}"
    Prefixo="${Arq##*:}"
    mkdir "$TRAB/x_$Prefixo"
    (cd "$TRAB/x_$Prefixo" && ar x "$Origem")
    for Obj in "$TRAB/x_$Prefixo"/*.o; do
        mv "$Obj" "$JUNTA/${Prefixo}_$(basename "$Obj")"
    done
done
cp "$TRAB/curl_ponte.o" "$JUNTA/curl_ponte.o"
cp "$TRAB/libgcc_shim.o" "$JUNTA/libgcc_shim.o"
cd "$JUNTA"
rm -f "$TRAB/libcurl.a"
ar rcsD "$TRAB/libcurl.a" *.o

mkdir -p "$DESTINO" "$DESTINO_VENDOR"
cp "$TRAB/libcurl.a" "$DESTINO/libcurl.a"
cp "$TRAB/libcurl.a" "$DESTINO_VENDOR/libcurl.a"
( cd "$DESTINO" && sha256sum libcurl.a > SHA256SUMS )
ls -l "$DESTINO/libcurl.a"
cat "$DESTINO/SHA256SUMS"
