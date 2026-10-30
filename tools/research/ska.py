from gh3 import *
def u32(b,o): return struct.unpack_from('>I',b,o)[0]
def u16(b,o): return struct.unpack_from('>H',b,o)[0]
def s16(b,o): return struct.unpack_from('>h',b,o)[0]
def f32(b,o): return struct.unpack_from('>f',b,o)[0]
def read_ske(b):
    n=u32(b,4); names=u32(b,0x10); parents=u32(b,0x14); posAt=u32(b,0x28); quatAt=u32(b,0x2c)
    keys=[u32(b,names+i*4) for i in range(n)]
    bones=[]
    for i in range(n):
        pk=u32(b,parents+i*4); parent=keys.index(pk) if pk else -1
        bones.append(dict(key=keys[i],parent=parent if parent<i else -1,pos=[f32(b,posAt+i*16+k*4) for k in range(3)],quat=[-f32(b,quatAt+i*16),-f32(b,quatAt+i*16+4),-f32(b,quatAt+i*16+8),f32(b,quatAt+i*16+12)]))
    return bones
def read_ska(b):
    if len(b)<0x80 or u32(b,0x20)!=0x48: raise Exception('bad')
    duration=f32(b,0x28); n=u16(b,0x2c); qAt=u32(b,0x58); tAt=u32(b,0x5c); qSizes=u32(b,0x60); tSizes=u32(b,0x64)
    plain=(u32(b,0x24)>>28)==1; wide=(u32(b,0x24)&0x100)!=0
    mAt=u32(b,0x18)
    hasMask= mAt>=0x80 and mAt+4+((n+31)//32)*4<=len(b) and u32(b,mAt)==n
    def uses(i): return (not hasMask) or ((u32(b,mAt+4+(i>>5)*4)>>(i&31))&1)==1
    bones=[]; qp=qAt; tp=tAt; lead=True
    for i in range(n):
        qEnd=qp+u16(b,qSizes+i*2); tEnd=tp+u16(b,tSizes+i*2); qt=[];q=[];tt=[];t=[]
        p=qp; last=0; wrap=0; head=lead and tEnd>tp
        if head: lead=False
        if (not uses(i)) or qEnd>len(b) or tEnd>len(b):
            bones.append(dict(qt=qt,q=q,tt=tt,t=t)); qp=qEnd; tp=tEnd; continue
        if plain:
            while p+16<=qEnd: qt.append(0); q.append((-f32(b,p),-f32(b,p+4),-f32(b,p+8),f32(b,p+12))); p+=16
            p=tp
            while p+16<=tEnd: tt.append(0); t.append((f32(b,p),f32(b,p+4),f32(b,p+8))); p+=16
            bones.append(dict(qt=qt[:1],q=q[:1],tt=tt[:1],t=t[:1])); qp=qEnd; tp=tEnd; continue
        step=4 if wide else 2
        while p+step<=qEnd:
            if wide: frame=u16(b,p); h=u16(b,p+2); p+=4
            else:
                h=u16(b,p); p+=2; frame=(h&0x7FF)+wrap
                if frame<last: wrap+=0x800; frame+=0x800
                last=frame
            v=[0,0,0]; packed=h&0x4000
            if packed and not (h&0x3800): p+=2
            else:
                for c in range(3):
                    if packed and (h&(0x2000>>c)): v[c]=b[p]; p+=1
                    else:
                        if p&1: p+=1
                        v[c]=s16(b,p); p+=2
            if p&1: p+=1
            if p>qEnd: break
            x,y,z=[a/16384 for a in v]
            qt.append(frame/60); q.append((-x,-y,-z,math.sqrt(max(0,1-x*x-y*y-z*z))))
        p=tp; first=True
        while p+16<=tEnd:
            h0=b[p]; frame=(h0&0x3F) if h0&0x40 else (b[p+1]<<8)|b[p+2]
            tt.append(frame/60); t.append((f32(b,p+4),f32(b,p+8),f32(b,p+12))); p+=16
            if first and head: p+=12
            first=False
        bones.append(dict(qt=qt,q=q,tt=tt,t=t)); qp=qEnd; tp=tEnd
    return dict(duration=duration,bones=bones,plain=plain,delta=(u32(b,0x24)&0x200)!=0,wide=wide,n=n,masked=hasMask,flags=u32(b,0x24))
