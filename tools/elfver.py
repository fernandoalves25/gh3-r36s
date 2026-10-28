# Audits the ports on the SD card from Windows: every ELF executable or shared object up to two levels
# under each port folder is read for its machine type and the glibc / libstdc++ symbol versions it needs.
# ArkOS (Ubuntu 19.10) ships glibc 2.30 and GLIBCXX 3.4.28, so anything newer cannot start there.
#   python -I tools/elfver.py H:\ports [maxdepth]
import os, struct, sys

MAX_GLIBC = (2, 30); MAX_CXX = (3, 4, 28)
MACHINES = {183: 'aarch64', 40: 'armhf', 62: 'x86_64', 3: 'i386'}

def vtuple(s):
    try: return tuple(int(x) for x in s.split('.'))
    except ValueError: return (0,)

def elf_info(path):
    with open(path, 'rb') as f:
        hdr = f.read(64)
        if len(hdr) < 52 or hdr[:4] != b'\x7fELF': return None
        cls = hdr[4]; E = '<' if hdr[5] == 1 else '>'
        e_type, e_machine = struct.unpack_from(E + 'HH', hdr, 16)
        if e_type not in (2, 3): return None
        if cls == 2:
            e_shoff, = struct.unpack_from(E + 'Q', hdr, 40); e_shentsize, e_shnum, _ = struct.unpack_from(E + 'HHH', hdr, 58)
        else:
            e_shoff, = struct.unpack_from(E + 'I', hdr, 32); e_shentsize, e_shnum, _ = struct.unpack_from(E + 'HHH', hdr, 46)
        if not e_shoff or not e_shnum: return MACHINES.get(e_machine, str(e_machine)), set(), set()
        f.seek(e_shoff); sh = f.read(e_shentsize * e_shnum); secs = []
        for i in range(e_shnum):
            o = i * e_shentsize
            if cls == 2: _, typ, _, _, off, size, link = struct.unpack_from(E + 'IIQQQQI', sh, o)
            else: _, typ, _, _, off, size, link = struct.unpack_from(E + 'IIIIIII', sh, o)
            secs.append((typ, off, size, link))
        vers = set(); needed = set()
        for typ, off, size, link in secs:
            if typ != 0x6ffffffe or link >= len(secs): continue  # SHT_GNU_verneed
            f.seek(off); data = f.read(size); f.seek(secs[link][1]); strtab = f.read(secs[link][2])
            name_at = lambda i: strtab[i:strtab.find(b'\0', i)].decode('latin1')
            p = 0
            while p + 16 <= len(data):
                _, cnt, vn_file, vn_aux, vn_next = struct.unpack_from(E + 'HHIII', data, p); needed.add(name_at(vn_file)); q = p + vn_aux
                for _ in range(cnt):
                    if q + 16 > len(data): break
                    _, _, _, vna_name, vna_next = struct.unpack_from(E + 'IHHII', data, q); vers.add(name_at(vna_name))
                    if not vna_next: break
                    q += vna_next
                if not vn_next: break
                p += vn_next
        return MACHINES.get(e_machine, str(e_machine)), vers, needed

def main():
    root = sys.argv[1]; maxdepth = int(sys.argv[2]) if len(sys.argv) > 2 else 2
    print(f'{"PORT":22} {"FILE":48} {"ARCH":8} {"GLIBC":8} {"GLIBCXX":8} STATUS')
    bad = []
    for port in sorted(os.listdir(root)):
        pdir = os.path.join(root, port)
        if not os.path.isdir(pdir): continue
        for dirpath, dirs, files in os.walk(pdir):
            depth = dirpath[len(pdir):].count(os.sep)
            if depth >= maxdepth: dirs[:] = []
            for fn in files:
                p = os.path.join(dirpath, fn)
                try:
                    if os.path.getsize(p) < 20000: continue
                    info = elf_info(p)
                except OSError: continue
                if not info: continue
                arch, vers, needed = info
                glibc = max((vtuple(v[6:]) for v in vers if v.startswith('GLIBC_')), default=None)
                cxx = max((vtuple(v[8:]) for v in vers if v.startswith('GLIBCXX_')), default=None)
                status = []
                if arch in ('x86_64', 'i386'): status.append('x86: needs box64/box86')
                if glibc and glibc > MAX_GLIBC: status.append('GLIBC too new')
                if cxx and cxx > MAX_CXX: status.append('GLIBCXX too new')
                rel = os.path.relpath(p, pdir)
                line = f'{port[:22]:22} {rel[:48]:48} {arch:8} {".".join(map(str, glibc)) if glibc else "-":8} {".".join(map(str, cxx)) if cxx else "-":8} {", ".join(status) or "ok"}'
                print(line)
                if any('too new' in s for s in status): bad.append(line)
    print(f'\n{len(bad)} file(s) need a newer glibc/libstdc++ than ArkOS has:')
    for b in bad: print('  ' + b)

main()
