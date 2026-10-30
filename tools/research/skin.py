from gh3 import *
def u32(b,o): return struct.unpack_from('>I',b,o)[0]
def u16(b,o): return struct.unpack_from('>H',b,o)[0]
def f32(b,o): return struct.unpack_from('>f',b,o)[0]
def read_scene(scn):
    matEnd=0x20+u32(scn,0x24)
    mats={}
    p=0x20+u32(scn,0x28)
    while p+0x100<=matEnd:
        size=u32(scn,p+0xc4)
        if size<0x100 or p+size>matEnd: break
        nt=min(8,u32(scn,p+0xb8)); tOff=p+u32(scn,p+0xbc)
        tex=[u32(scn,tOff+i*4) for i in range(nt)]
        mats[u32(scn,p)]=dict(template=u32(scn,p+0xa0),tex=tex,flags=u32(scn,p+0xcc),blend=u32(scn,p+0xd8),glow=u32(scn,p+0xdc),alphaRef=scn[p+0xe8],clamp=list(scn[p+0xea:p+0xee]))
        p+=size
    assert u32(scn,matEnd)==0xBABEFACE
    base=matEnd+8+u32(scn,matEnd+4)
    nMesh=u32(scn,base+0x4c); meshAt=base+u32(scn,base+0x80)
    meshes=[]
    for i in range(nMesh):
        m=meshAt+i*0x80
        nIdx=u16(scn,m+0x24); nVert=u16(scn,m+0x26)
        o40=u32(scn,m+0x40); o5c=u32(scn,m+0x5c); o60=u32(scn,m+0x60)
        if not nVert or not nIdx or o40==0xFFFFFFFF or o5c==0xFFFFFFFF: continue
        sAt=-1 if o60==0xFFFFFFFF else base+o60
        skinned= sAt>=0 and sAt+0x20<=len(scn) and u32(scn,sAt)==0xCAFEBAB4
        vAt=base+o40+0x20; iAt=base+o5c+0x20
        d=dict(n=i,nIdx=nIdx,nVert=nVert,mat=u32(scn,m+0x14),skinned=skinned,vSize=u32(scn,m+0x4c),flags=u32(scn,m+0x10))
        if skinned:
            groups=[u32(scn,sAt+0x10+w*4) for w in range(4)]
            d['groups']=groups
            p=sAt+0x20; v=0; used=set(); wts=[]
            for w in range(4):
                for g in range(groups[w]):
                    num=u32(scn,p); b0,b1,b2=scn[p+4],scn[p+5],scn[p+6]; p+=16
                    used.add(b0)
                    if w>=2: used.add(b1)
                    if w>=3: used.add(b2)
                    wts.append((w,num,b0,b1,b2))
                    v+=num; p+=num*56
            d['nv_read']=v; d['bones']=used; d['wts']=wts
            # triangles from strip
            tri=0;a=b=-1
            for k in range(nIdx):
                c=u16(scn,iAt+k*2)
                if c in(0x7fff,0xffff) or c>=nVert: a=b=-1; continue
                if a>=0 and b>=0 and a!=b and b!=c and a!=c: tri+=1
                a=b;b=c
            d['tri']=tri
        meshes.append(d)
    return mats,meshes
if __name__=='__main__':
    import sys
    for n in sys.argv[1:]:
        b=load(n); ix=pakindex(b)
        sk=max((e for e in ix if e['type']==qbkey('.skin')),key=lambda e:e['len'])
        scn=b[sk['off']:sk['off']+sk['len']]
        mats,meshes=read_scene(scn)
        sm=[m for m in meshes if m['skinned']]
        print(n,'skin bytes',sk['len'],'meshes',len(meshes),'skinned',len(sm),'verts',sum(m['nVert'] for m in sm),'tris',sum(m['tri'] for m in sm),'bones used',len(set().union(*[m['bones'] for m in sm])) if sm else 0,'maxbone',max([max(m['bones']) for m in sm] or [0]),'mats',len(mats))
        for m in sm: print('   mesh',m['n'],'v',m['nVert'],'tri',m['tri'],'groups(1,2,3,4 w idx)',m['groups'],'mat',hex(m['mat']),mats[m['mat']]['tex'] and [hex(t) for t in mats[m['mat']]['tex']],'blend',mats[m['mat']]['blend'],'aref',mats[m['mat']]['alphaRef'],'tmpl',hex(mats[m['mat']]['template']))
