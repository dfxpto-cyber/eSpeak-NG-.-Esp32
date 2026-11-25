#!/usr/bin/env bash
# Recompila os dados da voz portuguesa do eSpeak NG e gera data/espeak-pt.bin.
# Só é preciso para mudar pronúncias (tools/espeak/pt_extra) ou a versão do
# eSpeak. Requisitos: git, cmake e um compilador de C
# (macOS: brew install cmake e Xcode CLT; Debian/Ubuntu: apt install git cmake build-essential).
# Uso: tools/espeak/build_espeak_data.sh   e depois, no exemplo,   idf.py flash
set -euo pipefail
cd "$(dirname "$0")/../.."
COMMIT=$(cat components/espeak-ng/UPSTREAM_COMMIT)
WORK=${WORK:-/tmp/espeak-ng-build}
[ -d "$WORK/src" ] || git clone https://github.com/espeak-ng/espeak-ng.git "$WORK/src"
git -C "$WORK/src" checkout -q "$COMMIT"
cmake -S "$WORK/src" -B "$WORK/build" -DCMAKE_BUILD_TYPE=Release \
      -DUSE_MBROLA=OFF -DUSE_LIBSONIC=OFF -DUSE_LIBPCAUDIO=OFF -DUSE_ASYNC=OFF \
      -DUSE_SPEECHPLAYER=OFF -DUSE_KLATT=OFF
cmake --build "$WORK/build" -j
# pronúncias acrescentadas (Wi-Fi, iPhone, Bluetooth, ...)
cp tools/espeak/pt_extra "$WORK/src/dictsource/pt_extra"
(cd "$WORK/src/dictsource" && ESPEAK_DATA_PATH="$WORK/build" "$WORK/build/src/espeak-ng" --compile=pt)
# timbres (variantes) incluídos no pacote; acrescente ou retire nomes de espeak-ng-data/voices/!v
VARS="f1 f2 f3 f4 f5 m1 m2 m3 m4 m5 m6 m7 Alicia Andrea Annie anika aunty belinda linda steph steph2 shelby grandma Diogo Henrique Marco Mario pedro miguel sandro max iven john"
EXTRA=""; for v in $VARS; do EXTRA="$EXTRA voices/!v/$v"; done
python3 tools/espeak/make_espeak_pack.py "$WORK/build/espeak-ng-data" data/espeak-pt.bin --extra $EXTRA
# ouvir uma frase no computador:
ESPEAK_DATA_PATH="$WORK/build" "$WORK/build/src/espeak-ng" -v roa/pt -w /tmp/teste.wav \
    "Olá! Ligar o Wi-Fi. Escrever no iPhone." && { afplay /tmp/teste.wav 2>/dev/null || aplay -q /tmp/teste.wav 2>/dev/null || true; }
# ver os fonemas de uma palavra (para acertar o pt_extra):
#   ESPEAK_DATA_PATH="$WORK/build" "$WORK/build/src/espeak-ng" -v roa/pt -q -x "palavra"
