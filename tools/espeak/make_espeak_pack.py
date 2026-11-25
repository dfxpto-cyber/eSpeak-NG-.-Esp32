#!/usr/bin/env python3
"""Gera data/espeak-pt.bin: um "pacote" só de leitura com os ficheiros do
eSpeak NG necessários para o português europeu. O pacote é gravado numa
partição da flash e mapeado em memória pelo firmware (sem cópia para RAM).

Formato (little-endian):
  cabeçalho: magic 'ESPK' | versão u32 (1) | n_ficheiros u32 | reservado u32
  n_ficheiros × entrada: nome[56] (caminho relativo, terminado em \0)
                         | offset u32 | tamanho u32
  dados: cada ficheiro alinhado a 16 bytes.

Uso:
  python3 tools/make_espeak_pack.py <espeak-ng-data compilado> data/espeak-pt.bin [--lang pt]
"""
import argparse, pathlib, struct, sys

NAME_LEN = 56
ALIGN = 16

ap = argparse.ArgumentParser()
ap.add_argument("datadir", type=pathlib.Path)
ap.add_argument("out", type=pathlib.Path)
ap.add_argument("--lang", default="pt", help="código do dicionário (pt)")
ap.add_argument("--voice", default="lang/roa/pt", help="ficheiro de voz")
ap.add_argument("--extra", nargs="*", default=[],
                help="ficheiros extra (por ex. voices/!v/f3)")
a = ap.parse_args()

files = ["phontab", "phonindex", "phondata", "intonations",
         f"{a.lang}_dict", a.voice] + a.extra
blobs = []
for rel in files:
    p = a.datadir / rel
    if not p.is_file():
        sys.exit(f"falta {p} (compilaste os dados do eSpeak NG?)")
    if len(rel.encode()) >= NAME_LEN:
        sys.exit(f"nome demasiado longo: {rel}")
    blobs.append((rel, p.read_bytes()))

hdr_size = 16 + len(blobs) * (NAME_LEN + 8)
off = (hdr_size + ALIGN - 1) // ALIGN * ALIGN
entries, data = [], bytearray()
for rel, b in blobs:
    entries.append((rel, off, len(b)))
    data += b
    pad = (-len(b)) % ALIGN
    data += b"\0" * pad
    off += len(b) + pad

out = bytearray(b"ESPK" + struct.pack("<III", 1, len(entries), 0))
for rel, o, n in entries:
    out += rel.encode().ljust(NAME_LEN, b"\0") + struct.pack("<II", o, n)
out += b"\0" * ((-len(out)) % ALIGN)
out += data
a.out.parent.mkdir(parents=True, exist_ok=True)
a.out.write_bytes(out)
for rel, o, n in entries:
    print(f"  {rel:<20} {n:>8} B  @0x{o:06x}")
print(f"{a.out}: {len(out)} bytes ({len(out)/1024:.0f} KiB)")
