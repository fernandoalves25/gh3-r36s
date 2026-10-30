from gh3 import *
from skin import *
def texbank(b):
    n=(b[6]<<8)|b[7]; meta=u32(b,8); out={}
    for i in range(n):
        p=meta+i*40
        out[u32(b,p+4)]=(u16(b,p+8),u16(b,p+10),u32(b,p+28),u32(b,p+32))
    return out
def dds(b,off,size):
    d=b[off:off+size]
    if d[:3]!=b'DDS': return None
    h=struct.unpack_from('<I',d,12)[0]; w=struct.unpack_from('<I',d,16)[0]; mips=struct.unpack_from('<I',d,28)[0]
    return w,h,d[84:88].decode('latin1'),mips
if __name__=='__main__':
    import sys
    for n in sys.argv[1:]:
        b=load(n); ix=pakindex(b)
        tx=[e for e in ix if e['type']==qbkey('.tex')]
        sk=max((e for e in ix if e['type']==qbkey('.skin')),key=lambda e:e['len'])
        mats,meshes=read_scene(b[sk['off']:sk['off']+sk['len']])
        used=set(t for m in mats.values() for t in m['tex'])
        for e in tx:
            tb=b[e['off']:e['off']+e['len']]; bank=texbank(tb)
            print(n,'texbank',e['len']//1024,'K',len(bank),'tex')
            for k,(w,h,o,s) in bank.items(): print('    ',hex(k),w,h,s//1024,'K',dds(tb,o,s),'USED' if k in used else '')
