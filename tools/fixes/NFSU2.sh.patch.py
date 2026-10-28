# Patches NFSU2.sh so the game starts through the bundled glibc runtime (ports/nfsu2/runtime/) when it is
# there: the binary was linked against glibc 2.34 / libstdc++ 12 and ArkOS has glibc 2.30.
#   python -I tools/fixes/NFSU2.sh.patch.py "H:\ports\NFSU2.sh"
import sys, shutil, os
p = sys.argv[1]
if not os.path.exists(p + '.bak'): shutil.copyfile(p, p + '.bak')
s = open(p, encoding='utf-8', newline='').read()
old = '''./nfsu2.aarch64 "$GAMEDIR/gamedata" $CAP_ARG $LANG_ARG > "$GAMEDIR/log.txt" 2>&1
EXIT_CODE=$?
'''
new = '''# ArkOS has glibc 2.30, and this binary was linked against glibc 2.34 / libstdc++ 12 ("version GLIBC_2.34
# not found"). runtime/ holds a newer glibc + libstdc++ (Ubuntu 22.04 arm64), and the game is started
# through that runtime's own dynamic loader, with its libraries first and the firmware's (SDL2, Mali, ALSA)
# after them. Without runtime/ the binary is run directly as before.
RT="$GAMEDIR/runtime"
if [ -f "$RT/ld-linux-aarch64.so.1" ]; then
  "$RT/ld-linux-aarch64.so.1" --library-path "$RT:$GAMEDIR/libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \\
    "$GAMEDIR/nfsu2.aarch64" "$GAMEDIR/gamedata" $CAP_ARG $LANG_ARG > "$GAMEDIR/log.txt" 2>&1
else
  ./nfsu2.aarch64 "$GAMEDIR/gamedata" $CAP_ARG $LANG_ARG > "$GAMEDIR/log.txt" 2>&1
fi
EXIT_CODE=$?
'''
assert old in s, 'launch line not found'
s = s.replace(old, new)
open(p, 'w', encoding='utf-8', newline='').write(s)
print('patched', p)
