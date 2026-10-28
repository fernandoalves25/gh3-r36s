#!/bin/bash
# Audits the ports on the SD card: every ELF file up to two levels under each port folder is checked for
# its machine type and the newest glibc / libstdc++ symbol versions it needs. ArkOS (Ubuntu 19.10) has
# glibc 2.30 and GLIBCXX 3.4.28 for aarch64 (armhf multiarch the same), so anything newer cannot start.
#   bash tools/audit_ports.sh [/mnt/h/ports]
ROOT=${1:-/mnt/h/ports}
MAXGLIBC="2.30"; MAXCXX="3.4.28"
vergt() { [ "$(printf '%s\n%s\n' "$1" "$2" | sort -V | tail -1)" = "$1" ] && [ "$1" != "$2" ]; }
printf '%-22s %-46s %-8s %-10s %-10s %s\n' PORT FILE ARCH GLIBC GLIBCXX STATUS
for port in "$ROOT"/*/; do
    name=$(basename "$port")
    find "$port" -maxdepth 2 -type f -size +20k 2>/dev/null | while read -r f; do
        magic=$(head -c 4 "$f" 2>/dev/null | od -An -c | tr -d ' \n')
        [ "$magic" = "177ELF" ] || continue
        hdr=$(aarch64-linux-gnu-readelf -h "$f" 2>/dev/null) || continue
        case "$hdr" in *AArch64*) arch=aarch64 ;; *ARM*) arch=armhf ;; *X86-64*|*"Advanced Micro"*) arch=x86_64 ;; *80386*) arch=i386 ;; *) arch=other ;; esac
        echo "$hdr" | grep -q "Type:.*\(EXEC\|DYN\)" || continue
        vers=$(aarch64-linux-gnu-readelf -V "$f" 2>/dev/null | grep -oE 'GLIBC_[0-9.]+|GLIBCXX_[0-9.]+' | sort -u)
        glibc=$(echo "$vers" | grep GLIBC_ | sed 's/GLIBC_//' | sort -V | tail -1)
        cxx=$(echo "$vers" | grep GLIBCXX_ | sed 's/GLIBCXX_//' | sort -V | tail -1)
        status=ok
        [ -n "$glibc" ] && vergt "$glibc" "$MAXGLIBC" && status="GLIBC_TOO_NEW"
        [ -n "$cxx" ] && vergt "$cxx" "$MAXCXX" && status="${status/ok/}${status:+ }GLIBCXX_TOO_NEW"
        [ "$arch" = x86_64 ] || [ "$arch" = i386 ] && status="x86 (box64/box86)"
        rel=${f#"$port"}
        printf '%-22s %-46s %-8s %-10s %-10s %s\n' "$name" "${rel:0:46}" "$arch" "${glibc:--}" "${cxx:--}" "$status"
    done
done
