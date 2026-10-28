#!/usr/bin/env python3
"""Generates levels.h (5 verified 24x24 levels with keycards). Run: python3 gen_levels.py
Link third value: 0 = open corridor, 1 = normal door, 'R'/'U'/'Y' = red/blue/yellow locked door.
Entities: i imp, d demon, f floater, B baron.  Items: h health, a ammo, s shotgun, g chaingun,
v armor, r/u/y red/blue/yellow key."""
import random, math
N = 24
LOCK = {'R': 'r', 'U': 'u', 'Y': 'y'}
LEVELS = [
 dict(name="HANGAR 1", seed=1, par=90,
  rooms={'A':(2,2,6,6,'#'),'B':(11,3,6,5,'%'),'C':(19,2,4,8,'#'),'D':(2,12,7,7,'#'),'E':(12,13,8,8,'W')},
  start='A', exit='E',
  links=[('A','B',1),('B','C',1),('A','D',0),('D','E',1),('B','E',1)],
  ents={'B':['i'],'C':['i','i'],'D':['i'],'E':['i','i']},
  items={'B':['h'],'C':['a'],'D':['s','a'],'E':['h','a']}),
 dict(name="REACTOR", seed=7, par=150,
  rooms={'A':(2,2,5,5,'#'),'B':(10,2,7,7,'W'),'C':(19,3,4,6,'%'),'D':(2,11,6,8,'%'),'E':(11,12,5,9,'#'),'F':(18,13,5,8,'W')},
  start='A', exit='F',
  links=[('A','B',1),('B','C',1),('A','D',0),('B','E',1),('D','E',1),('E','F','R')],
  ents={'B':['i','d'],'C':['i','i'],'D':['d','i'],'E':['d','i'],'F':['i','d','d']},
  items={'B':['a'],'C':['h','r'],'D':['a','h','v'],'E':['a'],'F':['h','a']}),
 dict(name="CRYO LAB", seed=21, par=180,
  rooms={'A':(2,2,6,5,'#'),'B':(10,2,6,6,'W'),'C':(18,2,5,7,'%'),'D':(2,10,7,6,'W'),'E':(11,11,5,6,'#'),'F':(18,12,5,9,'W'),'G':(3,18,12,4,'%')},
  start='A', exit='F',
  links=[('A','B',1),('B','C',1),('A','D',0),('B','E',1),('D','G',1),('E','G',0),('C','F','U')],
  ents={'B':['i','f'],'C':['d','f'],'D':['i','i'],'E':['f','d'],'G':['d','i'],'F':['f','d','d']},
  items={'B':['a'],'C':['a'],'D':['g','h'],'E':['h','a'],'G':['u','a','v'],'F':['h','a']}),
 dict(name="DEMON WARRENS", seed=33, par=220,
  rooms={'A':(2,2,5,5,'#'),'B':(9,2,6,5,'%'),'C':(17,2,5,6,'W'),'D':(2,9,6,6,'#'),'E':(10,9,5,6,'W'),'F':(17,11,5,6,'%'),'G':(2,18,8,4,'#'),'H':(12,18,10,4,'W')},
  start='A', exit='H',
  links=[('A','B',1),('B','C',1),('A','D',0),('B','E',1),('C','F','R'),('D','G',1),('E','H','Y')],
  ents={'B':['i','i'],'C':['d','f'],'D':['d','i'],'E':['f','f'],'F':['d','d','f'],'G':['i','d'],'H':['f','d','d','i']},
  items={'B':['a'],'C':['h'],'D':['r','a','h'],'E':['a','v'],'F':['y','h','a'],'G':['g','a','a'],'H':['h','a']}),
 dict(name="HELL KEEP", seed=11, par=260,
  rooms={'A':(2,2,5,5,'#'),'B':(9,2,5,5,'%'),'C':(16,2,6,6,'W'),'D':(2,10,8,6,'%'),'E':(13,10,8,5,'#'),'F':(3,18,18,4,'W')},
  start='A', exit='F',
  links=[('A','B',1),('B','C',1),('A','D',0),('C','E',1),('D','E','R'),('E','F','U')],
  ents={'B':['i','d'],'C':['i','f','d'],'D':['d','d','i'],'E':['f','d','i'],'F':['B','d','f']},
  items={'B':['r','a'],'C':['h','a'],'D':['u','h','a','v'],'E':['a','h'],'F':['h','a','a']}),
]

def build(L):
    rnd = random.Random(L['seed'])
    g = [['#']*N for _ in range(N)]
    rooms = L['rooms']; roomof = {}
    for k,(x,y,w,h,s) in rooms.items():
        for yy in range(y,y+h):
            for xx in range(x,x+w):
                g[yy][xx]='.'; roomof[(xx,yy)]=k
    center=lambda k:(rooms[k][0]+rooms[k][2]//2, rooms[k][1]+rooms[k][3]//2)
    doors=[]
    for a,b,d in L['links']:
        (ax,ay),(bx,by)=center(a),center(b)
        path=[]; x,y=ax,ay
        while x!=bx: x+=1 if bx>x else -1; path.append((x,y))
        while y!=by: y+=1 if by>y else -1; path.append((x,y))
        for (x,y) in path:
            if g[y][x]=='#': g[y][x]='.'
        if d:
            ch='D' if d==1 else d
            placed=False
            for (x,y) in path:
                if (x,y) in roomof: continue
                if (g[y-1][x]=='#' and g[y+1][x]=='#') or (g[y][x-1]=='#' and g[y][x+1]=='#'):
                    if not any(abs(x-dx)+abs(y-dy)<2 for dx,dy in doors):
                        g[y][x]=ch; doors.append((x,y)); placed=True; break
            if not placed and ch!='D': raise Exception('locked door not placed %s %s %s in %s'%(a,b,d,L['name']))
    for y in range(N):
        for x in range(N):
            if g[y][x]!='#': continue
            for dy in (-1,0,1):
                for dx in (-1,0,1):
                    q=(x+dx,y+dy)
                    if q in roomof and rooms[roomof[q]][4]!='#': g[y][x]=rooms[roomof[q]][4]
    x,y,w,h,s=rooms[L['exit']]; ex,ey=x+w//2,y+h; g[ey][ex]='E'
    used=set(); sx,sy=center(L['start']); g[sy][sx]='P'; used.add((sx,sy))
    def place(room,ch):
        x,y,w,h,s=rooms[room]
        for _ in range(300):
            xx=rnd.randint(x+1,x+w-2); yy=rnd.randint(y+1,y+h-2)
            if (xx,yy) in used or g[yy][xx]!='.': continue
            if room==L['start'] and abs(xx-sx)+abs(yy-sy)<3: continue
            used.add((xx,yy)); g[yy][xx]=ch; return
        raise Exception('cannot place '+ch)
    for r,l in L['ents'].items():
        for c in l: place(r,c)
    for r,l in L['items'].items():
        for c in l: place(r,c)
    nonstart=[k for k in rooms if k!=L['start']]
    for c in (rnd.choice('xze'),rnd.choice('xze')): place(rnd.choice(nonstart),c)
    if L['name'] in ('REACTOR','CRYO LAB'): place(rnd.choice(nonstart),'b')
    for _ in range(1+len(rooms)//3): place(rnd.choice(nonstart),'c')
    passable='.DihasdBfgvruyPxzebc'
    def bfs(keys):
        seen={(sx,sy)}; q=[(sx,sy)]
        while q:
            x,y=q.pop()
            for dx,dy in((1,0),(-1,0),(0,1),(0,-1)):
                n=(x+dx,y+dy)
                if not(0<=n[0]<N and 0<=n[1]<N) or n in seen: continue
                c=g[n[1]][n[0]]
                if c in passable or (c in LOCK and LOCK[c] in keys): seen.add(n); q.append(n)
        return seen
    def exit_ok(seen): return any((ex+dx,ey+dy) in seen for dx,dy in((1,0),(-1,0),(0,1),(0,-1)))
    assert not exit_ok(bfs(set())) or not any(c in LOCK for r in g for c in r) , 'lock is bypassable (exit open without keys) in '+L['name']
    keys=set()
    while True:
        seen=bfs(keys)
        nk={g[y][x] for (x,y) in seen if g[y][x] in 'ruy'}
        if nk<=keys: break
        keys|=nk
    assert exit_ok(seen), 'exit unreachable in '+L['name']
    for y in range(N):
        for x in range(N):
            if g[y][x] in 'ihasdBfgvruyxzebc' and (x,y) not in seen: raise Exception('unreachable %s at %d,%d in %s'%(g[y][x],x,y,L['name']))
    rows=[''.join(r) for r in g]; assert all(len(r)==N for r in rows) and len(rows)==N
    a,b,_=L['links'][0]; (cx,cy),(tx,ty)=center(L['start']),center(b)
    return rows, math.atan2(ty-cy,tx-cx)

out=['// AUTO-GENERATED by gen_levels.py','#define NLEVELS %d'%len(LEVELS),'static const char *LEVELS[NLEVELS][MH]={']
names=[];angs=[];pars=[]
for L in LEVELS:
    rows,ang=build(L); names.append(L['name']); angs.append(ang); pars.append(L['par'])
    out.append('{'); out+=['"%s",'%r for r in rows]; out.append('},')
    print(L['name']); print('\n'.join(rows)); print()
out.append('};')
out.append('static const char *LEVEL_NAMES[NLEVELS]={%s};'%','.join('"%s"'%n for n in names))
out.append('static const float LEVEL_ANG[NLEVELS]={%s};'%','.join('%.4ff'%a for a in angs))
out.append('static const int LEVEL_PAR[NLEVELS]={%s};'%','.join(str(p) for p in pars))
open('levels.h','w').write('\n'.join(out)+'\n')
print('levels.h written: %d levels verified (keys reachable, exits reachable)'%len(LEVELS))
