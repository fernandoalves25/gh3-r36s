#!/bin/bash
# Builds a small glibc 2.35 + libstdc++ 12 runtime (Ubuntu 22.04 arm64 packages) for ports whose binary
# was linked against a newer glibc than ArkOS's 2.30. The port's launcher then starts the binary through
# this runtime's dynamic loader:  runtime/ld-linux-aarch64.so.1 --library-path runtime:... ./binary
#   bash tools/nfsu2_runtime.sh [outdir]      -> outdir/ (default build/nfsu2-rt)
set -eu
cd "$(dirname "$0")/.."
OUT=${1:-build/nfsu2-rt}; TMP=$(mktemp -d)
POOL=https://ports.ubuntu.com/pool/main
pick() { curl -fsSL "$1/" | grep -oE "$2" | sort -V | tail -1; }
LIBC=$(pick "$POOL/g/glibc" 'libc6_2\.35-0ubuntu3[.0-9]*_arm64\.deb')
LIBSTD=$(pick "$POOL/g/gcc-12" 'libstdc\+\+6_12\.[0-9.]+-[0-9]+ubuntu1~22\.04[.0-9]*_arm64\.deb')
LIBGCC=$(pick "$POOL/g/gcc-12" 'libgcc-s1_12\.[0-9.]+-[0-9]+ubuntu1~22\.04[.0-9]*_arm64\.deb')
[ -n "$LIBC" ] && [ -n "$LIBSTD" ] && [ -n "$LIBGCC" ] || { echo "package names not found (libc6=$LIBC libstdc++6=$LIBSTD libgcc-s1=$LIBGCC)"; exit 1; }
echo "packages: $LIBC  $LIBSTD  $LIBGCC"
for p in "g/glibc/$LIBC" "g/gcc-12/$LIBSTD" "g/gcc-12/$LIBGCC"; do curl -fsSL -o "$TMP/$(basename "$p")" "$POOL/$p"; done
mkdir -p "$TMP/x"; for d in "$TMP"/*.deb; do dpkg-deb -x "$d" "$TMP/x"; done
rm -rf "$OUT"; mkdir -p "$OUT"
L="$TMP/x/lib/aarch64-linux-gnu"; U="$TMP/x/usr/lib/aarch64-linux-gnu"
for f in ld-linux-aarch64.so.1 libc.so.6 libm.so.6 libpthread.so.0 libdl.so.2 librt.so.1 libresolv.so.2 libutil.so.1 libnss_files.so.2 libnss_dns.so.2 libanl.so.1; do
    src=$(ls "$L/$f" "$U/$f" 2>/dev/null | head -1); [ -n "$src" ] && cp -L "$src" "$OUT/" || echo "note: no $f"
done
cp -L "$U"/libstdc++.so.6 "$OUT/"; cp -L "$L"/libgcc_s.so.1 "$OUT/" 2>/dev/null || cp -L "$U"/libgcc_s.so.1 "$OUT/"
chmod 755 "$OUT"/*
{ echo "glibc runtime for ArkOS ports linked against glibc > 2.30"; echo "from Ubuntu 22.04 arm64: $LIBC $LIBSTD $LIBGCC"; echo "built $(date -u +%Y-%m-%d)"; } > "$OUT/README.txt"
cp "$TMP/x/usr/share/doc/libc6/copyright" "$OUT/LICENSE-glibc.txt" 2>/dev/null || true
cp "$TMP/x/usr/share/doc/libstdc++6/copyright" "$OUT/LICENSE-libstdc++.txt" 2>/dev/null || true
rm -rf "$TMP"
ls -la "$OUT"; du -sh "$OUT"
echo "newest glibc symbol in runtime libc: $(aarch64-linux-gnu-objdump -T "$OUT/libc.so.6" | grep -oE 'GLIBC_2\.[0-9]+' | sort -V | tail -1)"
