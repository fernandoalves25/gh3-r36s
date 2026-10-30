# GH3 band rendering recipe (for the C / GLES2 port)

Source of truth: `F:\games\gh3\index.html` (JS, line numbers below), checked against the real data in `F:\games\gh3\DATA`.
All numbers below marked (V) were verified by me with a Python re-implementation of the readers
(`tools/research/gh3.py skin.py ska.py tex.py`): 225/225 skinned meshes in every DATA/PAK/*.pak.xen parse with
vertex count == header count; every .ska in the tested paks parses. Those scripts are a usable reference for the C port.
Everything is BIG-ENDIAN (except DDS headers, which are little-endian). Nothing in the repo or in F:\games\gh3 was modified.

Existing C code to reuse: `src/gh3fmt.c` (`gh3_qbkey`, `gh3_pak_parse`, `gh3_qb_parse`, `qb_get`/`qb_getk`, `gh3_library_read`),
`tools/qbgrep.c` (dump any qb section: `qbgrep PAK PAB names.txt key <name-substr>`; needs `.nqb` (0x49875607) added to read node arrays).
qbkey = CRC32 (poly 0xEDB88320, init 0xFFFFFFFF, NO final xor) of lowercase name with '/' -> '\\' (js 435-441).

---------------------------------------------------------------------------------------------------------------------
## 1. Files (DATA/PAK, ls -la sizes in bytes) and what they hold

Entry type keys (qbkey of extension): .skin 0x64112e85, .ska 0x745dcd45, .ske 0x7330095c, .tex 0x8bfa5e8e, .rag 0x6613eacd,
.mdl 0x9bcc234d, .qb 0xa7f505c4, .nqb 0x49875607, .scn 0x2c3b5adc, .img 0xdad5e950, .fnt 0x7e1abc70.
`<char>.pak.xen` = single-file pak (no .pab): 32-byte header per entry {type,off(rel to header start),len,pakKey,fullKey,nameKey,?,flags}
(+160 B filename if flags&0x20) -- js `pakIndex` 904 / `parsePak` 448 / C `gh3_pak_parse`. Entries are matched by `fullKey`.
Every char pak also has a 0-byte `.qb`-typed marker entry (0xa7f505c4, 0 bytes) -- ignore.

| pak | bytes | contents (V) |
|---|---|---|
| Slash_1 | 8871936 | 1 .skin 716K (7442v/10913t/16 meshes), 2 .tex (7016K used + 342K other), .img 128K portrait, 12 .ska (face/hand poses 464K) |
| Slash_1_anims | 5013504 | 112 .ska, 4.9 MB |
| Slash01 | 1298432 | Slash's guitar: 1 .skin 172K (2025v/1626t, 3 meshes) + .tex 1084K |
| Morello_1 | 3358720 | Tom Morello body: .skin 426K (4887v/6064t/6 meshes), 2 .tex (1880K used), 12 .ska |
| Morello_1_anims | 4513792 | 107 .ska |
| Morello | 1343488 | his guitar: .skin 217K (2435v/2248t) + .tex |
| Satan | 8331264 | Lou (the devil boss): .skin 747K (8342v/12676t/18 meshes), 2 .tex (6405K used), 14 .ska |
| Satan_anims | 6238208 | 147 .ska |
| Demonik | 1282048 | Lou's guitar: .skin 684K (7909v/2844t!) + .tex 557K |
| axel_1 / axel_1_anims | 8626176 / 7675904 | Axel: 6218v/9470t/14 meshes; 39 .ska in body pak, 194 in anims |
| Casey_1 / _anims | 8855552 / 11927552 | 8701v/13519t/12 meshes; 14 + 276 .ska |
| Izzy_1 / _anims | 6184960 / 8450048 | 8049v/10618t/13 meshes; 16 + 201 .ska |
| Johnny_1 / _anims | 8183808 / 10371072 | 9195v/14092t/11 meshes; 16 + 204 .ska |
| Judy_1 / _anims | 4833280 / 11046912 | 8691v/13062t/12 meshes; 15 + 247 .ska |
| Lars_1 / _anims | 4038656 / 8515584 | 8810v/11755t/12 meshes; 17 + 234 .ska |
| Midori_1 / _anims | 6631424 / 11468800 | 13156v/11619t/21 meshes (biggest); 13 + 258 .ska |
| Xavier_1 / _anims | 7925760 / 8642560 | 6462v/10410t/15 meshes; 16 + 195 .ska |
| Ripper_1 / _anims | 5189632 / 4513792 | Grim Ripper: 7910v/10873t/6 meshes; 12 + 107 .ska |
| RockGod_1 / _anims | 8093696 / 7675904 | God of Rock: 7601v/11865t/10 meshes; 39 + 194 .ska |
| Bassist / Bassist_anims | 2846720 / 1441792 | 4538v/6492t/4 meshes, tex 2359K; 38 .ska |
| Drummer / Drummer_anims | 2637824 / 905216 | 4675v/6498t/2 meshes, tex 2151K; 41 .ska |
| Singer_Male / Singer_Male_Anims | 5439488 / 5947392 | 8016v/8579t/7 meshes, tex 4578K; 82 .ska |
| Drum_Set001 | 1040384 | kit: 698v/550t, skinned to 11 drummer-skeleton bones (idx <=84) |
| Microphone001 | 344064 | mic: 343v/336t (bone 120 of the singer skeleton) |
| ELP01 | 1265664 | bass guitar: 1608v/1451t |
| les_paul_black 1290240, SG_WornRed 1363968, Explorer_White 1261568, godrock 1306624, ripper 1282048 | | the other guitars, 1.5-2.1k verts each |
| crowd_ped_01..04 | 69632 / 94208 x3 | 473 / 838 / 819 / 829 verts; tex 128x128 DXT1 (10-13K) |
| crowd_pedf_1 / _2 | 155648 / 176128 | 974 / 1231 verts, 6 meshes each |
| crowd_prison_01 / _02 | 266240 each | 939 verts, tex 173K (used by z_prison) |
| perm_anims | 8732672 | 544 .ska SHARED: 263 for the 125-bone body skeletons (finger/chord poses, fret-position poses, extras), 198 for the 28-bone crowd-ped skeleton, 37 for a 75-bone skeleton, ... |
| qb.pak/qb.pab | 16896 / 2561120 | `musician_profiles`, `musician_body`, `musician_instrument`, `crowd_profiles`, `<zone>_crowd_models`, `drummer_anims`, `cymbal_anims`, every anim_set / finger_anims / fret_anims struct |
| ZONES/global.pak+.pab | 24576 / 83193856 | 35 `.ske` skeletons (14 KB each for the 125-bone ones), 27 `.rag` (physics, unused by the browser port), 624 .img |
| SONGS/<song>_song.pak.xen | 0.1-1.5 MB | chart `.qb` + 5 big `.ska` of the singer's mouth (lip-sync, 30 face bones, 80-925 KB each, 4-42 s) |

NOT present on this install: Singer_Brett, Singer_Female (+ their _anims), boneman_*, crowd_biker_* (z_hell asks for them: the browser
port falls back to `crowd_ped_01..04, pedf_1/2` -- `crowdUse` 7573).

### Skeletons (all in ZONES/global.pab, found by `fullKey` == key from the musician profile)  (V)
125 bones: Axel/RockGod 0xe1ac58d7, Casey 0xaf370d49, Izzy 0x871b4e23, Judy 0x4ab3992f, Johnny 0x308753e1, Lars 0x88ba9196,
Midori 0x12160e4f, Xavier 0x0006787a, Slash 0x5437a02d, Morello 0x06f72da2, Satan(Lou) 0x2d27cc8d, Ripper 0xa771d7fe,
Bassist 0xb818efb0, **guitar rest skeleton 0xe5d20452** (`src.guitar`, js 7612). All 125-bone ones share one layout.
121 bones: Singer_Male 0xef982399 (Brett 0x2825681c, Female 0xd686877f absent). 100 bones: Drummer 0x93fec78d.
28 bones: crowd ped `gh3_ped` = 0xbb6e508c. (81/75/28/11/5/2-bone ones: not needed.)

125-bone layout (names found by crc, V): 0 root(0x1be55811), 1 (0xe6aca48e, hips / the only body bone whose translation animates),
2 bone_pelvis, 3/4 thigh_l/knee_l, 5/6 thigh_r/knee_r, 7,8 spine, 9 bone_chest, 10 clavicle_l, 11 bone_bicep_l, 12 bone_forearm_l,
13 bone_palm_l, 14 clavicle_r, 15 bicep_r, 16 forearm_r, 17 palm_r, 18 bone_neck, 19 bone_head, 24/25 ankle/toe_l, 26/27 ankle/toe_r,
30-37 twist/helper, 38-67 fingers (3 bones x 5 fingers x 2 hands; left hand 38-40,44-46,50-52,56-58,62-64; right hand 41-43,...),
68 bone_jaw, 69/70 eye_l/eye_r, 71-97 face bones, 98-101 bone_ik_foot_master/slave_l/r, 102/103 bone_ik_hand_slave_l/r (drum sticks),
104-118 hair/cloth, **119 bone_guitar_body, 120 bone_guitar_neck, 121 (0xd7df90, slides the fretting hand along the neck), 122 bone_ik_hand_guitar_l,
123, 124 bone_ik_hand_guitar_r**. Bone 0 rest quat = (0.5,0.5,0.5,0.5) (axis swap); rest pose is a T-pose, Y-up, left hand at +X,
feet at y=0, head y~1.59, body bbox y 0..1.91 (Slash). Character faces +Z at yaw 0; units are metres.

---------------------------------------------------------------------------------------------------------------------
## 2. Binary formats

### 2.1 .ske  (js readSkeleton 1220)
```
u32 @4 = nBones (1..512)         u32 @0x10 = namesAt   u32 @0x14 = parentsAt   u32 @0x28 = posAt   u32 @0x2c = quatAt
names[i]   = u32 qbkey at namesAt+4i       parents[i] = u32 key of parent bone (0 = root) at parentsAt+4i -> index by searching names[]; parent must be < i
pos[i]     = 3 x f32 at posAt+16i  (rest translation, local, 4th float pad)
quat[i]    = 4 x f32 at quatAt+16i  stored (x,y,z,w); USE (-x,-y,-z,w)  (stored turns run the other way)
```
Local rest TRS only (no scale). World = parent world * local.

### 2.2 .skin  (a ".scn"-style container; js readScene 976 + skinnedMesh 1002)  (V)
```
u32 @0x24 = matSize ; matEnd = 0x20 + matSize ; materials start 0x20 + u32@0x28
 material (size = u32 @p+0xc4, >=0x100): key u32@p ; template u32@p+0xa0 ; nTex u32@p+0xb8 (<=8), texTable = p+u32@p+0xbc : u32 texture keys ;
   blend u32@p+0xd8 (0 solid, 1 additive, 3 alpha); alphaRef u8@p+0xe8 ; clamp[4] u8@p+0xea..ed (bit0 no-repeat U, bit1 no-repeat V) ; flags u32@p+0xcc
 u32 @matEnd == 0xBABEFACE ; base = matEnd + 8 + u32@(matEnd+4)
 nMesh = u32@base+0x4c ; meshAt = base + u32@base+0x80 ; mesh record = 0x80 bytes:
   +0x00 f32 centre.xyz, +0x0c f32 radius (bounding sphere) ; +0x10 flags ; +0x14 material key ; +0x1c..1e fmt bytes
   +0x24 u16 nIdx ; +0x26 u16 nVert ; +0x40 u32 uvOff (vAt = base+uvOff+0x20) ; +0x4c u32 vSize ; +0x5c u32 idxOff (iAt = base+idxOff+0x20)
   +0x60 u32 skinOff (0xFFFFFFFF = none), sAt = base+skinOff ; skinned iff u32@sAt == 0xCAFEBAB4
 indices: u16 triangle STRIP at iAt, restart on 0x7FFF/0xFFFF (or >=nVert); alternate winding flip (emit (a,b,c) / (b,a,c)); drop degenerate. JS draws DoubleSide.
 UV block at vAt: stride = floor(vSize/nVert) (8 = 2xf32 uv, or 12 = 4-byte colour + uv); layout from type bytes at vAt-0x20+2.. (0x0A colour, 0x05 uv pair); uv = 2 x f32 BE; V as stored (JS flips nothing, textures uploaded un-flipped).
 SKIN block at sAt: 4 x u32 @sAt+0x10..0x1c = number of groups of kind w=0,1,2,3. Data from sAt+0x20, groups in order w=0..3:
   group header 16 B: u32 num ; u8 b0,b1,b2 (bone INDEX into the skeleton) ; 12 pad.   then num x 56-byte vertices:
   f32 @0 posX, @12 nrmX, @16 posY, @28 nrmY, @32 posZ, @44 nrmZ   (rows of 4 floats: [pos_r, tangent?, bitangent?, nrm_r]; tangents unused)
   f32 @48 w0, @52 w1 (only for w>=2);  w<=1: single bone b0 weight 1;  w==2: bones b0,b1 weights w0,w1;  w==3: b0,b1,b2 weights w0,w1,1-w0-w1
 vertex order of the skin block == vertex index order used by the index strip and the uv block.
```
Positions/normals are in the skeleton's REST WORLD space (bind pose, Y-up). Inverse bind = inverse(rest world of bone). (three.js default.)
Weight mix (V, summed over all bodies listed): about 63% of vertices 1 bone, 29% 2 bones, 8% 3 bones. Max 3 influences; never 4.
Per-mesh distinct bones (V): Slash 1..36, Morello 7..34, Satan 1..53, Bassist 3..62, Drummer 19/59, Singer 1..79, ped 11..14 => a per-mesh bone
palette of <= 80 entries is always enough. Skin meshes also carry an optional colour (unused by the JS).

### 2.3 .tex bank (js texBank 916, texCount 912)
`b[0]=0xFA b[1]=0xCE`; count = (b[6]<<8)|b[7]; meta = u32@8; entry i at meta+40i: key u32@+4, w u16@+8, h u16@+10, data off u32@+28, size u32@+32.
Data = a full **DDS file** (DXT1/DXT3/DXT5, LE header, 128-byte header, mip chain; a few have 0 mips; 32x32 dummies exist).
Which bank: the largest `.tex` entry of the pak (js `model1` 7603). Materials reference textures by key (nTex list above).
Char textures (V): 1024x1024 DXT5 normal + DXT1/DXT5 diffuse for bodies, 512x1024 / 256x512 for hair+clothes, 128x128 for small parts,
bodies total 2-7 MB of DXT per character.

### 2.4 .ska  (js readSka 1232)  (V, tested on ~1100 clips)
```
u32 @0x20 == 0x48 (version) ; f32 @0x28 duration (seconds) ; u16 @0x2c nBones (== skeleton bone count: 125, 121, 100, 28 ...)
u32 @0x24 flags: (>>28)==1 -> "plain" one-pose clip ; bit 0x100 -> "wide" (long face clips) ; bit 0x200 -> "delta" (pose stored as difference from rest)
u32 @0x18 maskAt ; mask = u32 n, then ceil(n/32) u32 words, LSB-first, 1 bit per bone: bone has tracks (otherwise its streams are empty)
u32 @0x58 qAt (rotation streams, concatenated) ; @0x5c tAt (translation streams) ; @0x60 qSizes[nBones] u16 (byte length of each bone's rotation stream) ; @0x64 tSizes[nBones] u16
```
Per bone i, rotation stream (non-plain, narrow): repeated `u16 h`: frame = (h & 0x7FF) + wrap (add 0x800 to wrap when frame goes backwards; frames are 60 Hz);
 if (h & 0x4000) "packed": if !(h & 0x3800) -> no components (x=y=z=0), skip 2 bytes; else for c=0..2 { if (h & (0x2000>>c)) component = u8 (unsigned) else { align to even; component = s16 } };
 non-packed: all 3 components s16 (2-aligned). After the key, align to even. x,y,z = comp/16384; **q = (-x,-y,-z, sqrt(1-x²-y²-z²))** (w>=0).
 wide: each key = u16 frame + u16 h (frame explicit, no wrap logic), same component rules.
 Plain: 16-byte entries (f32 x,y,z,w) -> use (-x,-y,-z,w); first entry only; translations 16-byte entries (f32 x,y,z,pad), first only; time 0.
Translation stream: 16-byte keys: byte0 & 0x40 ? frame = byte0 & 0x3F : frame = (b1<<8)|b2 ; f32 x,y,z at +4,+8,+12. The first bone (in index order) that has any
 translation stream carries 12 extra bytes after its first key (skip them).
Evaluation: key time = frame/60 s; linear lerp for translation, slerp for rotation. Typical body clip has keys every 2 frames (30 Hz), e.g. 225 keys / 7.47 s.
 Rotation: ABSOLUTE local quaternion replacing the bone's rest rotation, unless "delta": q = qDelta * qRest.
 Translation: stored value is an OFFSET: local pos = rest.pos + stored. The root (bone 0) translation is NOT applied: measure its travel (last-first, xz) instead (walks).
 Bones with no track keep their rest transform.
```
Stats (V): body clips avg 44 KB raw, 5 s, ~3700 rot keys + ~900 trans keys, only ~26 bones driven (see 5.2); crowd clips 3-27 KB (28-bone skeleton);
drum-hit clips 1.5-4 KB / 0.2-0.9 s; singer lip-sync clips 80-925 KB / 4-42 s with 30 face bones (68..97) driven, keys at 60 Hz (wide) or 30 Hz.
Expanded in RAM as float quats+times ~ 90 KB per body clip; a guitarist references ~92-96 clips (Slash 94 = 5.0 MB raw, Axel 193 = 9.0 MB, Satan 133 = 6.3 MB,
Bassist 21 = 1.3 MB, Drummer 13 = 0.63 MB, Singer 78 = 5.5 MB). Recommend an offline baker (tools/) that writes int16 quats @30 Hz.

---------------------------------------------------------------------------------------------------------------------
## 3. Who is who: musician profiles (qb.pak, section `musician_profiles`, js readLibrary 668-685)  (V, dumped with qbgrep)

Profile struct: `NAME, Skeleton(=.ske key), musician_body{desc_id}, musician_instrument{desc_id}, Type($guitarist|bassist|drummer|vocalist), anim_set(key of a top-level
qb section), stance($stance_a default), finger_anims(key), fret_anims(key), scale(vec3, only Judy 0.9)`. desc_id resolves via `musician_body` / `musician_instrument`
lists to {Mesh, PAK "pak\models\...\x.pak", anims_pak}. Resolved table on this install:

| profile | role | skeleton | anim_set | stance | finger_anims | fret_anims | body pak / anims | guitar pak |
|---|---|---|---|---|---|---|---|---|
| Axel | guitarist | e1ac58d7 | a871c540 | stance_a | 21825fea | b0d50638 | axel_1 / axel_1_anims | explorer_white |
| Casey | g | af370d49 | 58131586 | a | f79f6f5f | fd273563 | casey_1 | les_paul_black |
| Izzy | g | 871b4e23 | 815a2469 | a | 4735491e | 2d9db923 | izzy_1 | les_paul_black |
| Judy | g | 4ab3992f | 8852508c | a | 8a9d9e12 | fd273563 | judy_1 (scale 0.9) | sg_wornred |
| Johnny | g | 308753e1 | db5f100a | **0x24e8bc01** | c40b4ed2 | 2d9db923 | johnny_1 | sg_wornred |
| Lars | g | 88ba9196 | 2b63a66f | a | 489496ab | b0d50638 | lars_1 | explorer_white |
| Midori | g | 12160e4f | 5203bfea | a | e69a137c | fd273563 | midori_1 | sg_wornred |
| Xavier | g | 0006787a | 6237d94c | a | f48a6549 | 2d9db923 | xavier_1 | les_paul_black |
| **Slash** (boss) | g | 5437a02d | 46b97941 | a | 0c9fc23b | 2d9db923 | slash_1 | slash01 |
| **Morello** (Tom, boss) | g | 06f72da2 | e411f2fc | a | b9d3c0a9 | 2d9db923 | morello_1 | morello |
| **Satan** (Lou, boss) | g | 2d27cc8d | 63c0ef3d | a | 758fae9b | b0d50638 | satan / satan_anims | demonik |
| RockGod | g | e1ac58d7 | a871c540 | a | 21825fea | b0d50638 | rockgod_1 | godrock |
| Ripper | g | a771d7fe | e411f2fc | a | b9d3c0a9 | 2d9db923 | ripper_1 | ripper |
| Bassist | bassist | b818efb0 | f1e4205e | a | 7c77f55a | 2d9db923 | bassist / bassist_anims | elp01 |
| Drummer | drummer | 93fec78d | a3a0e300 | a | - | - | drummer / drummer_anims | drum_set001 |
| Singer | vocalist | ef982399 | a9bd6b62 | a | - | - | singer_male / singer_male_anims | microphone001 |
(Brett/Female singers, BoneMan_* absent from DATA. `Singer_Alt` = same body, microphone002 absent.)

Boss battle: the boss profile (`boss.character_profile` = qbkey of NAME "Slash"/"Morello"/"Satan") is placed in the **bassist slot** (bassist waypoint) and plays the
bassist role's events/frets (101..110) with its own guitarist anim set (js bandRead 7623-7646 `first`, battleStageName 3739). The player's guitarist = chosen character.
Boss attack/react kinds exist in the sets but the browser port never plays them (grep: no use); no attack animation is wired.

---------------------------------------------------------------------------------------------------------------------
## 4. Building one member (js bandRead 7623, stageFiles 7602, bandMember 7408, charMeshes 7328, charBones 7320)

1. skeleton from global.pab (`fullKey == profile.Skeleton`).  2. body: largest `.skin` + largest `.tex` of `<body>.pak`.
3. instrument: same from `<instrument>.pak`. Guitarist/bassist instrument is **rigid**: all vertices weight 1 on bone 119 (V "bones used 1, maxbone 119"). Its inverse bind
   comes from the *guitar skeleton* 0xe5d20452 rest pose (`charMeshes(..., bind)` uses inverse(world of guitarSke bone i) for the body rig's bone i), so
   skinMatrix = bodyBoneWorld[119] * inverse(guitarSkeRestWorld[119]). Drum kit and mic are skinned onto the drummer / singer skeleton itself (own rest inverse).
4. clips: `where` = union of `<anims pak>`, `<body pak>` and `perm_anims`, by fullKey. Needed keys = every key mentioned anywhere in the anim_set struct + finger_anims +
   fret_anims (+ drummer_anims/cymbal_anims). Js reads the idle set first (stance idle slow/med/fast normal + chord poses) then the rest lazily.
5. scale: `root.scale.setScalar(profile.scale[0])` if != 1 (Judy only).
6. placement: see section 9.

### Materials / textures of characters (js charMeshes 7328-7347)  (V, all printed templates)
Per mesh: one material (draw call). Per material: `tex[]` list. Body materials (templates 0xacd09a98, 0x6f3f03f3 hair/cloth, 0x852105c8, 0x9c2d3c81) list
**[normal-map(DXT5, bluish), DIFFUSE, spec/gloss(unused)]** => draw with **tex[1]**. Instruments / crowd list the picture first (tex[0]) and an optional
normal map second. JS rule: look at tex[0]'s average colour; if it is a normal/blue-ish detail map and the material has >1 textures use tex[1] as colour and tex[0] as normal map
(only then; MeshPhong with a little shine), else tex[0] plain (MeshLambert). Everything DoubleSide. blend==3 or alphaRef>0 (hair/lashes/cloth, alphaRef 16) -> alpha-test cutoff
0.35 (NOT blended, so it sorts); blend==1 additive (+ depthWrite off); blend==0 opaque even though the DXT5 has alpha. Vertex colours unused. Lit by the venue's
"band" lights (scene lights tagged group band/alt_band, animated by the light show) plus ambient 0.06; planar shadow (below). The port can shade with
1 directional + ambient and no normal maps.

---------------------------------------------------------------------------------------------------------------------
## 5. Animation selection (what plays when)  js bandPlan 7391, bandMember 7408-7482, tick 7268-7299, bgBeat 6668

### 5.1 Data structures
`anim_set` (top-level qb struct, key from profile):  `stance_X { kind { mood { speed [ska keys] } } }` and `stance_X { transitions { <toStance> = key } }`.
- stances present (V): stance_a, stance_b (0xbc9f47c7), stance_c (0xcb987751), 0x24e8bc01, 0xdaee973d, intro, intro_smstg (0x0adf7bb6), 0x19fffcb1, win, win_smstg (0xddcd5886),
  lose, lose_smstg (0xebf159d0), `facial_anims`, `walking`.  (stance_b/c used by songs for the guitarist/singer "intense" stances.)
- kinds inside stance_a (guitarist, Slash): idle, solo, jump, kick, special, starpower, attack, react, transitions, + unnamed 0x95790fef/0x879394b9/0x4729e84d/0x04dc2d28/0xbb06996c/0x262b7501. Bass: idle solo special jump kick. Drummer: idle, sticks.
  Singer: idle special jump (+ kick release 0x30a85ebe in stance_b). Intro/win/lose stances only hold `idle` + `transitions`.
- mood keys: `bad | normal | good` (qb spells normal as NORMAL); speed keys `slow | med | fast`. `animKeys(kind,mood,speed)` (6772) = `kind[mood] || kind.normal || kind.good` then `[speed] || med`.
  A list holds 1-3 variations (idle good/med has 3); bad mood often has a single sad pose.
- wanted keys when a lookup misses: `keysFor` falls back to stance_a's list (or the first stance) and drops keys whose clip is not on disk.
- finger_anims struct: `GREEN RED YELLOW BLUE ORANGE` + hashed combos (qbkey of `green_red`, `green_yellow_blue` ... built in bandPlan, `col.filter(mask).join('_')`, mask bit0=green..bit4=orange) -> key of a **delta** clip with 16 bones
  (left-hand fingers 38-40,44-46,50-52,56-58,62-64 and 122 ik_hand_guitar_l) (V). Missing combos fall back to mask 1 (green).
- fret_anims struct: `track_101..110` and `track_118..127` -> key of a 2-bone delta clip (bones 120,121 = slide the left-hand anchor along the neck) (V, these live in perm_anims).
- drummer_anims struct: `track_36..59` -> [key] (hit clip, 0.2-0.9 s, ~21 bones driven); extra `sticktap_l`, `sticktap_r`; `cymbal_anims` = 4 keys used for notes 68,69,67,65/66 as swing clips.
- crowd_profiles: 8 entries keyed by qbkey of crowd NAME -> `anim_set`; crowd sets have only idle / special / starpower x mood x speed (V).

### 5.2 Song-side cues (js readSongPak 743-777, bandPlan, tick)
`<song>_song.pak` `.qb` sections (names are `<song>_` + suffix):
- `_performance` (array of {Time ms, scr, params}) -> events: `band_changestance{NAME:$guitarist|bassist|vocalist|drummer, stance}`, `band_playanim{NAME, ANIM:$solo|$special|$jump..., flags: $cycle, $disable_auto_arms(0xa651ae55 flag listed as `_ = $x`)}`,
  `band_playfacialanim{NAME:$vocalist, ANIM:<fullKey of a .ska stored IN the song pak>}` (lip-sync), `band_walktonode{NAME,node}`, `crowd_startlighters`/`crowd_stoplighters` (+ lightshow_* ignored here). Barracuda has 21 changestance, 6 playanim, 5 facial. Sorted by time, fired as `ms >= t`; events older than 3 s on seek are applied "quiet" (stance only, no clip).
- `_anim_notes` [time, note, len]: note 118..127 -> guitarist.fret(note); note 101..110 -> bassist.fret(note) (fretting-hand position pose, layer crossfade 0.18 s, loops).
- `_drums_notes` [time, note, len]: note < 67 -> drummer.hit(note) started HIT_LEAD=0.13 s early; note==36 also pumps the kick-drum front; note >= 67 -> cymbal swing at hit time.
- `_cameras_notes`, `_lightshow_notes`, `_crowd_notes` (camera/light/crowd, not band).

### 5.3 Per-frame state (bgBeat 6668 -> T.tick 7268)
- speed: smoothed bpm from beat progress; `<105 slow, <150 med, else fast` (bpm += (inst-bpm)*min(1,dt*0.5)).
- mood/level: `health<0.66 -> 0 (bad), <1.33 -> 1 (normal), else 2 (good)` (health range 0..2); practice mode: level 1 and band hidden (`holder.visible=false`).
- frets: the chart note at/just before `now` (within max(sustain, 450 ms)) -> its button mask drives the chord layer; `note` index changes each note (strum toggle).
- star power rising edge (`spMix>0.5`) -> play kind `starpower` once for all but the drummer (not while walking).

### 5.4 bandMember algorithm (stance/cycle state machine)
```
state: stance (profile.stance), cycle=idle, cur action, curLoop, mood, speed, n (variation counter)
play(kind): list = keysFor(kind) (per stance/mood/speed); clip = list[(n++) % len] (first one that loads); begin(clip, fade 0.3 s, once); remember curIdle = (kind==idle)
next(): play(cycle) else idle.  tick: when !cur || (!loop && cur.time >= dur-0.3-0.02) || (curIdle && speed changed && cur.time>1.5) -> next()
intro() (song start, ms<4000 or on reset): keysFor stance 'intro', kind idle, mood/speed -> clip, fade 0.2 s (round robin)
changestance(ev): if new stance exists in the set: old=stance; stance=new; cycle=idle; clip = set[old].transitions[new] if present (play once, fade 0.3) else play(idle)
playanim(ev): play(ev.anim as kind); cycle = ev.flags has 'cycle' ? ev.anim : idle ; noArms = flags has 'disable_auto_arms'
playfacialanim(ev): clip from the song pak (wide face clip) -> face layer from time (late) with fade 0.25; also bring() it into the member's clip table before song start
face layer: otherwise cycles P.face = set.facial_anims.idle (flat list) one at a time (LoopOnce, 0.25 s fade; wait 1 s if none)
finale(win|lose): stance 'win'|'lose' (or *_smstg) via the same changestance path + camera lists '<zone>_cameras_win|lose'
walkTo(node): waypoint `<zone>_trg_waypoint_<node>` ; turn rate 4.5 rad/s then walk 'walking.forward|backward.cycle_anim.med' at root travel/duration speed (default 1.3 m/s); on arrival turn 2.4 rad/s to waypoint yaw
```
Round robin means a long idle clip (7-11 s) plays through fully then the next variation; crossfade is a linear weight blend of the two actions for 0.3 s.

### 5.5 What each body clip actually drives (V, Slash idle 0x09b00bce 7.47 s)
rot-driven bones (225 keys each = 30 Hz): 1,3,4,5,6,7,8,9,10,14,18,19,24,25,26,27,98,99,100,101,102,103,119 (+ 0,2,120 constant 2-key). Translation-driven: 1, 98-103 (tiny), 119 (guitar body). **Arms (11-13,15-17), fingers (38-67), cloth (104-118), face (68-97) are NOT driven by idle/solo/special** (only 17/112 Slash clips drive bone_bicep_l;
 mainly win/lose/taunt-type). Legs are plain FK (thigh/knee/ankle rotations) -- no leg IK is solved anywhere in the JS (ik_foot bones are just animated nodes).
=> the arms must be solved by arm IK every frame whenever the current clip does not drive bone_bicep_l (`driven.has(armBone)`) and `!noArms`.

### 5.6 Layers (blending)
`mixer.clipAction` blends by weight; hand layer (`layer(prev,key,fade)` 7432) = looping delta clip whose mask only covers fingers (+122) or bones 120/121, so it composites over the body
clip. Chord layer: `hand = layer(hand, chords.get(frets) ?? chords.get(1), 0.08)` every frame; fret layer: `fretAct = layer(fretAct, frets.get(note), 0.18)`. For the C port simply
override the masked bones with the layer pose (delta: q = qDelta*qRest, t = rest+delta) after sampling the body clip; JS actually averages (weights 1+1) on shared bones 120/121.

### 5.7 Arm IK ("strum / fret hand")  js charArm 7349-7365 (analytic 2-bone, per arm)
```
S = world pos of bone_bicep_(l|r) ; G = world pos of bone_ik_hand_guitar_(l|r) (children of guitar neck: bones 122 / 124) ; k = world scale (profile scale)
a1 = |forearm.localPos| * k ; a2 = |palm.localPos| * k
right hand only: G += unit(perp-to-neck-in-plane: y = (0,1,0) - xdir.y * xdir (xdir = neck bone world +X)) * stroke * k   (strum)
stroke: strokeTo = ±0.028 flips sign every time S.note changes; stroke += (strokeTo-stroke)*min(1,dt*28)
d = clamp(|G-S|, <= (a1+a2)*0.999); a = (a1²-a2²+d²)/(2d); h = sqrt(max(0,a1²-a²));
hint (world) = rootRot * (±0.45 (l:+, r:-), -0.8, -0.4); p = hint minus its component along u=(G-S)/d, normalized (fallback (0,-1,0)-ish)
E (elbow) = S + u*a + p*h
x = unit(E-S), y = unit(G-E), z = cross(x,y)*(-1) ; Qb = basis(x, cross(z,x), z) ; Qf = basis(y, cross(z,y), z)    (+X is the bone axis)
bicep.local = inv(parentWorldRot) * Qb ; forearm.local = inv(Qb) * Qf ; palm.local = inv(Qf) * worldRot(ik_hand_guitar_*)
```
Order each frame: sample all clips -> FK to world -> IK both arms (needs guitar IK targets' world from FK) -> FK again for hand chain/fingers -> skin.
Fingers/left hand pose come from the chord layer; the right hand has no finger pose. Strum is purely this ±2.8 cm stroke (not an animation).

### 5.8 Drummer (js bandMember hit 7467, charClip `land` 7381-7385, tick weights 7476)
`hit(note,struck)`: clip = drum[note]; start time t0 = max(0, land - 0.13) (land = time at which the lowest of bones ik_hand_slave_l/r (stick tip = hand + 0.35 along its local +X) / ik_foot_master_l/r reaches its lowest world y, within 2%-70% of the clip; default 0.13); cymbal swings (struck=true) start at t0=0.
Weight envelope 16*clamp((t-t0+0.015)/0.05, ..., (d-t)/min(0.16,0.45d)) so the hit "all but replaces" the idle arm pose; several hits can overlap. 36 = kick (also pumps the venue kick front, js 7277).

### 5.9 Singer
idle/special/jump (+stance_b kick/release) body clips as above (arms ARE driven in singer clips: 79/82 drive bicep_l, avg 60 bones, 72 KB); mouth: song-pak lip-sync clips on the 30 face bones (68-97, rot+trans keys 60/30 Hz) layered over the body at 0.25 s fade. Minimal: drive only 68 jaw (+69/70 eyes).

---------------------------------------------------------------------------------------------------------------------
## 6. Where they stand (venueBand js 1071; consumed 7177-7178; yaw = rotation about +Y, radians; y = floor)  (V, extracted from each z_*.pak .nqb)

Node name `<zone>_trg_waypoint_<role>_start` (class waypoint, fields pos / angles(x,y,z), yaw = angles[1]). `holder.position = pos`, `holder.rotation.y = yaw`; the rig root hangs below holder.
| zone | guitarist pos / yaw | bassist | drummer | vocalist |
|---|---|---|---|---|
| z_artdeco | (2.156,-0.010,0.309) / 0.524 | (-2.355,-0.010,-0.412) / 0 | (-0.015,0.394,-3.407) / 0 | (0.198,-0.010,0.626) / 0 |
| z_budokan | (1.349,0.028,3.379) / 0.175 | (-3.318,0.027,-0.198) | (0.249,1.193,-3.299) | (-0.767,0.028,3.081) |
| z_dive | (-0.577,0,-1.307) / 0.436 | (-5.915,0,-4.471) / 0.436 | (-3.701,0.075,-7.470) | (-4.105,0,-3.376) |
| z_hell | (4.002,0,1.232) / 0.175 | (-2.634,0,0.978) | (-0.278,0.984,-3.464) / -0.295 | (0.613,0,2.060) |
| z_party | (2.125,0.035,3.595) / 0 | (-2.135,0.035,2.941) | (0.025,0.139,0.850) | (0.223,0.035,3.941) |
| z_prison | (1.811,0.002,0.647) / 0 | (-2.086,0.002,-0.779) | (-0.015,0.226,-4.126) | (-0.147,0.002,0.489) |
| z_soundcheck | (0,0,0) (no start node) | (-37.53,-8.48,0.611) | (-43.47,-8.07,1.492) | (-30.69,-8.48,0.626) |
| z_video | (1.064,0,3.430) / 0.349 | (-1.356,0,2.081) | (0.090,0.290,0.266) | (-0.619,0,3.915) |
| z_wikker | (5.329,0,3.305) / 0.524 | (-1.684,0,6.595) | (1.333,0.988,-1.206) | (1.753,0,5.701) |
(Drummer is on a riser in several venues: see y. The venue .scn is in the same space as the band: no axis flip anywhere in the JS. Tier -> venue name: gh3_shop.tierZone / tier.zone in the repo.)
Camera shots can be locked to a role (`lockto`), orbiting its holder; not part of the band recipe.

---------------------------------------------------------------------------------------------------------------------
## 7. Crowd (js crowdFigure 7485, sceneVenue panels 7106-7124, crowdRead 7647, crowdUse 7573; qb `<zone>_crowd_models`, `crowd_profiles`)

- NOT 3D people in the room: the venue .scn has flat crowd PANEL meshes whose material references the crowd model's `textureasset` key (`panelOf` map). Each of up to 6-8 crowd model kinds
  (list per zone, V): wikker/dive/artdeco/party/budokan/video = Crowd_Ped_01..04 + crowd_pedF_1/2 (video lists 8 incl. repeats), prison = Crowd_Prison_01/02 x3, hell = Crowd_Biker_01/02 (absent -> stand-ins ped_01..04/pedF), soundcheck none.
  The game renders each ped (skeleton gh3_ped 0xbb6e508c, 28 bones; skins 473-1231 verts, 11-18 bones) into a 128x256 RGBA render target with an orthographic camera (w ~1.15 x h 2.58 m, camera at z=6) and maps that texture on the panel (alpha-test 0.5, DoubleSide). Redrawn every 2nd frame, alternating (`(i+frameN)%2==0`).
- animation: `kind = starpower if sp>0.5 else idle`, mood bad/normal/good from level, speed slow/med/fast from bpm; pick list[n++ % len] from the crowd anim_set (idle: 2-3 variations, 2-3 s each; clip changes when time >= dur-0.32 or when kind/mood changes after 0.6 s; fade 0.3). Clips are in perm_anims (28-bone ones, 3-27 KB).
- lighting of the figures: ambient = sum of the venue's crowd-group ambient lights, key = other crowd-group lights (fallback 0.45 / 0.55).
- lighters: `crowd_startlighters` event shows a lighter mesh group swaying 0.1*sin. Not needed.
- Port tip: ~6 FBO textures of 128x256 re-skinned at 15-30 Hz is cheap (a ped is <=1.2k verts); or pre-bake a few frames per ped.

## 8. Shadows / lights (js bandShadows 6828)
Planar projected shadow: the band's skinned meshes are drawn a second time through a matrix that flattens every vertex along the venue's shadow direction (`venueShadow` 1195: shadow-casting band light dir, else straight down) onto y = holder.y + 0.012 with black, opacity `shadowDark` (0.35 from script `tod_manager_create_perm_lights` setdynamicshadowintensity), stencil NotEqual-1 so overlapping triangles do not double-darken. Cost = a second skinned draw of the whole band (can reuse the skinned vertex buffer on CPU skinning, or run the skin shader with the projection matrix on the GPU).

---------------------------------------------------------------------------------------------------------------------
## 9. Cost estimates for R36S (RK3326: 4x Cortex-A35 ~1.3 GHz, Mali-G31, GLES2; estimates, not measured)

Per-character vertex/tri/bone counts (V):
| member | verts | tris | meshes (draw calls) | bones used by skin | tex (DXT) |
|---|---|---|---|---|---|
| guitarist (range of 13) | 4.9k (Morello) .. 13.2k (Midori); typical 6.2-9.2k | 6.1k-14.1k | 6-21 | 68-108 of 125 | 2-7 MB |
| + guitar (rigid, 1 bone) | 1.5k-2.1k (Demonik 7.9k verts but 2.8k tris) | 1.3-2.8k | 3 | 1 | 0.5-1.1 MB |
| bassist + ELP01 | 4.5k + 1.6k | 6.5k + 1.5k | 4 + 2 | 77 | 2.4 MB |
| drummer + kit | 4.7k + 0.7k | 6.5k + 0.55k | 2 + 1 | 73 | 2.2 MB |
| singer + mic | 8.0k + 0.3k | 8.6k + 0.3k | 7 + 2 | 106 | 4.6 MB |
| boss (Slash 7.4k+2.0k / Morello 4.9k+2.4k / Lou 8.3k+7.9k) | | | | | |
| each crowd ped | 0.5-1.2k | 0.7-1.6k | 1-6 | 11-18 | 10-170 KB |
Full band on stage: ~26-34k skinned verts, ~35-45k tris, ~45-60 draw calls (a wide shot sees all four; close-ups hide most).

CPU skinning (pos+normal, <=3 bones, scalar C, no NEON): ~60-90 cycles/vertex => 34k verts ~ 2-3M cycles ~ 2-2.5 ms on ONE A35 core per frame (about 1 ms with NEON on the 1-bone/2-bone fast paths); plus a
0.7-1 MB/frame vertex upload (pos+normal 24 B or packed 12 B). Bone work per member: sample ~26 driven bones (slerp/lerp) + FK over 125 bones + 2 IK arms = ~0.05 ms. Crowd: 6 peds x 1k verts every 2nd frame ~ 0.2 ms. Face/clip streaming negligible.
Total CPU for the band ~ 3 ms CPU-skin, or ~0.2 ms (matrices only) with **GPU skinning**.
GPU skinning is feasible on Mali-G31 GLES2: per-mesh remapped palette <= 80 bones x 3 vec4 (4x3 affine) = 240 vec4 uniforms (GL_MAX_VERTEX_UNIFORM_VECTORS is normally 1024 on Mali, check at runtime;
fallback = split meshes at 40 bones, or CPU skin); attributes: pos(3 float or 3 snorm16), normal (snorm8x4), uv (half/float x2), bone idx (3 x ubyte), weights (2 x ubyte, third = 1-w0-w1). 63% of verts are rigid (1 bone).
Fill rate at 640x480 and ~45k tris is fine; the risk is overdraw of 1024² alpha-test hair layers and the planar shadow second pass -> consider shadow = 1 blob quad per member.
Textures: S3TC is not guaranteed on libmali: check `GL_EXT_texture_compression_s3tc` else decode DXT on load and keep RGB565 / RGBA4444 (alpha-test parts) at <=512 (half-res of 1024 maps) => ~1-1.5 MB per guitarist
(Slash diffuse set: 2.2 Mpx at full res -> ~0.6 Mpx at half = 1.2 MB). Decode of a 7 MB DXT bank ~ 20-40 ms. Drop normal maps (tex[0]) and spec (tex[2]) entirely.
Anim memory: bake clips to int16 quats @30 Hz, only driven bones, and trans only for bones 1, 98-103, 119 => ~12-20 KB per clip, ~1.5 MB per guitarist (all ~95 clips); idle-only set = 9 clips.
LOD ideas: no face/finger bones (68-97, 38-67) on distant shots; skip normals/lighting per vertex (bake a gouraud with one dir light).

---------------------------------------------------------------------------------------------------------------------
## 10. Recommended build order (first step = one guitarist with an idle loop)

Step 1 (smoke test, ~600 lines of C): pick **Axel** (or Morello: lightest 4.9k verts, 107 clips) and Slash/Satan later.
 a. `tools/`: new reader module (src/gh3char.c) for .ske, .skin (section 2.2), .tex+DXT decode, .ska (2.4) using `gh3_pak_parse` (single pak, no pab) and `global.pab` for the skeleton (`gh3_pak_parse(global.pak, global.pab)`, match fullKey).
 b. load skeleton 0xe1ac58d7 (Axel; Slash 0x5437a02d), body skin+tex of `axel_1.pak` (largest of each type), guitar `explorer_white.pak`; use guitar skeleton 0xe5d20452 for the guitar inverse bind (bone 119).
 c. idle clips: qb.pak section `0xa871c540` (Axel set; Slash `0x46b97941`): stance_a/idle/normal/med = list of 3 keys -> read from `axel_1_anims.pak` (fallback `axel_1.pak`, `perm_anims.pak`).
    (Slash idle med normal: 0x09b00bce, 0x90b95a74, 0xe7be6ae2 - 7.5 s, 8.0 s, 9.1 s, all in Slash_1_anims; good/slow: 0x6e403a06 ...)
 d. per frame: t += dt; sample the 26 driven bones, local TRS = rest (+ offset for translations); FK; arm IK (5.7) to ik_hand_guitar targets; skin; draw 16 meshes with tex[1]; guitar = rigid on bone 119.
    Crossfade 0.3 s between variations, round-robin the 3 idle clips.
 e. place with holder pos/yaw from the section 6 table, camera at e.g. (0,1.4,4) looking at it; verify facing +Z, left hand at +X, guitar neck pointing to +X (V: bone 120 world (0.61,0.43,0) for Slash).
Step 2: speed/mood switching (bpm, health), the stance machine with the song `_performance` events, chord + fret layers from song/live input, intro, star-power clip.
Step 3: bassist + drummer (hits from `_drums_notes`) + singer (jaw from song lip-sync). Step 4: crowd FBO billboards on venue panels, bosses (profile in the bassist slot), shadows, win/lose finale.

## 11. Open questions / gotchas
- Untested on device; all counts V are from the files, timing numbers are estimates.
- Layer blending: JS averages overlapping layer/body weights on bones 120/121 (idle clip holds constant identity keys there); an override is a fine approximation.
- `tex` heuristic for "which slot is the normal map" is colour based in the JS; for body templates it is always slot 0 (V printed for Slash, Bassist, Drummer). Instruments can have DXT5 normal in slot 1.
- Strip winding: JS uses DoubleSide so winding direction was never validated; test with culling off first.
- Unnamed hash keys: stance 0x24e8bc01 (Johnny's default), 0xdaee973d, 0x19fffcb1; kinds 0x4729e84d etc. (names not recovered; just treat them as opaque keys, they are looked up by key).
- `.rag` (ragdoll, 77 KB each) and global `.skin`/`.mdl` are not used for the band by the browser port.
- Reference scripts (Python, tested): tools/research/gh3.py (crc+pak), skin.py (skin parse), ska.py (ska+ske), tex.py.
