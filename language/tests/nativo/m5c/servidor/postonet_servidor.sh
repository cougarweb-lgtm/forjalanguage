#!/bin/sh
# postonet_servidor.sh - lancador POSIX do servidor de prova (M5c-b P62) no Postonet: sobe ./servidor.elf e ./bot.elf em ~/forja_prova.
# Uso: sh postonet_servidor.sh <workers> <segundos> <bots>   (ex.: sh postonet_servidor.sh 6 30 4)
# Versao: 0.1.0
# Changelog: 0.1.0 - M5c-b P62 - primeira versao. POSIX puro (busybox ash/dash): sem timeout e sem sha256sum (so md5sum). Argumentos iguais aos de rodar_servidor.py: servidor = workers bots ticks porta; bot = porta bots ticks semente (12345). Tudo sai de ~/forja_prova (nada em /tmp).
# Verificacao: no WSL alpine-build, com servidor.elf e bot.elf em ~/forja_prova: sh postonet_servidor.sh 1 30 4 e sh postonet_servidor.sh 6 30 4 (hash igual, 0 perdido, rc=0).
# Reversao: apagar este arquivo.
set -u

if [ $# -ne 3 ]; then
    echo "uso: sh postonet_servidor.sh <workers> <segundos> <bots>"
    exit 2
fi
WORKERS=$1
SEGUNDOS=$2
BOTS=$3
TICKS=$((SEGUNDOS * 60))
SEMENTE=12345
PASTA="$HOME/forja_prova"
CPU_LIMITE=$((SEGUNDOS + 60))

cd "$PASTA" || { echo "FALHA sem a pasta $PASTA"; exit 2; }
rm -f porta.txt serv_out.txt serv_err.txt bot_out.txt bot_err.txt
ulimit -t "$CPU_LIMITE" || { echo "FALHA ulimit -t $CPU_LIMITE"; exit 2; }

./servidor.elf "$WORKERS" "$BOTS" "$TICKS" "$PASTA/porta.txt" > serv_out.txt 2> serv_err.txt < /dev/null &
SERV=$!

N=0
while [ ! -s porta.txt ] && [ "$N" -lt 30 ] && kill -0 "$SERV" 2>/dev/null; do
    sleep 1
    N=$((N + 1))
done

if [ ! -s porta.txt ]; then
    kill "$SERV" 2>/dev/null
    wait "$SERV" 2>/dev/null
    echo "FALHA servidor nao anunciou a porta"
    echo "== servidor"
    cat serv_out.txt
    cat serv_err.txt
    exit 1
fi

PORTA=$(cat porta.txt)
PORTA=${PORTA##*:}

./bot.elf "$PORTA" "$BOTS" "$TICKS" "$SEMENTE" > bot_out.txt 2> bot_err.txt < /dev/null
RC_BOT=$?
if [ "$RC_BOT" -ne 0 ]; then
    kill "$SERV" 2>/dev/null
fi
wait "$SERV"
RC_SERV=$?

echo "== servidor (workers=$WORKERS bots=$BOTS ticks=$TICKS)"
cat serv_out.txt
echo "== servidor stderr"
cat serv_err.txt
echo "rc=$RC_SERV"
echo "== bot (porta=$PORTA semente=$SEMENTE)"
cat bot_out.txt
echo "== bot stderr"
cat bot_err.txt
echo "rc=$RC_BOT"

if [ "$RC_SERV" -eq 0 ] && [ "$RC_BOT" -eq 0 ]; then
    exit 0
fi
exit 1
