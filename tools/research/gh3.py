import struct,sys,os,collections,math
P='F:/games/gh3/DATA/PAK/'
def crc_table():
    t=[]
    for i in range(256):
        c=i
        for _ in range(8): c=(c>>1)^0xEDB88320 if c&1 else c>>1
        t.append(c)
    return t
T=crc_table()
def qbkey(s):
    c=0xFFFFFFFF
    for ch in s.lower().replace('/',chr(92)).encode(): c=(c>>8)^T[(c^ch)&0xFF]
    return c&0xFFFFFFFF
def pakindex(b,ext=False):
    out=[];p=0
    while p+32<=len(b):
        typ,off,ln,pk,fk,nk,_,fl=struct.unpack('>8I',b[p:p+32])
        if typ in (qbkey('.last'),qbkey('last'),0): break
        out.append(dict(type=typ,off=p+off-(len(b) if ext else 0),len=ln,full=fk,name=nk,flags=fl))
        p+=32+(160 if fl&0x20 else 0)
    return out
TY={0x64112e85:'skin',0x745dcd45:'ska',0x7330095c:'ske',0x8bfa5e8e:'tex',0x6613eacd:'rag',0x9bcc234d:'mdl'}
def load(name):
    return open(P+name+'.pak.xen','rb').read()
if __name__=='__main__':
    print(hex(qbkey('.skin')),hex(qbkey('.ska')))
