# Round 3 sprites: letter-coded drum crops, pixel logs and fins, flowers in the chosen note colours,
# and corn drawn at three "tone" settings (husk closed, half, peeled) at note-view and grid size.
src = open('sprites.py').read().split('for name,rows in S.items():')[0]; ns = {}; exec(src, ns)
svg, SNES, FLOWER = ns['svg'], ns['SNES'], ns['S']['flower']
P = dict(SNES, V='#7850a8', v='#a878d0', s='#607898', S='#90a8c8', W='#f8f8f0', w='#e8f4ff', o='#f0b860', y='#fce8b8', L='#c8f0a0')
S = {
 'crop-k-kohlrabi':['..KGKGK.','...KGK..','.KKVVKK.','KVvVVVVK','KVVVVvVK','KVvVVVVK','.KVVVVK.','..KKKK..'],
 'crop-sn-snowpea':['........','......KG','..KKKKKK','.KgGgGgK','KgLgLgLK','KGgGgGK.','.KKKKK..','........'],
 'crop-rm-radish':['.KG.GK..','..KGK...','..KRRK..','.KRRWRK.','.KRRRRK.','..KRRK..','...KK...','...K....'],
 'crop-ch-chili':['......KG','.....KGK','....KRRK','...KRRK.','..KRRK..','.KRRK...','KRrK....','KKK.....'],
 'crop-oh-onion':['...G.G..','...GKG..','...KoK..','..KyooK.','.KyooooK','.KooyooK','..KoooK.','...KKK..'],
 'crop-tm-tomato':['...GG...','..GKKG..','.KRRRRK.','KRWRRRRK','KRRRRRRK','KRRRRRrK','.KRRRrK.','..KKKK..'],
 'crop-pc-pumpkin':['...DD...','.KKDKKK.','KOoOOoOK','KoOOoOOK','KoOOoOOK','KOoOOoOK','.KrOOrK.','..KKKK..'],
 'log-cap':['..KKKKKK','.KNNNNNN','KNnNNnNN','KNNNNNNN','KNNnNNNN','KNNNNNnN','.KnnnnnN','..KKKKKK'],
 'log-body':['KKKKKKKK','NNNNNNNN','NnNNNnNN','NNNNNNNN','NNnNNNNn','NNNNNNNN','nnnnnnnn','KKKKKKKK'],
 'log-end':['KKKKKK..','NNNNKyK.','NNNKyyyK','NNNKyNyK','NNNKyNyK','NNNKyyyK','nnnnKyK.','KKKKKK..'],
 'fin':['......KK....','.....KsK....','....KssK....','...KsssK....','..KssssSK...','.KsssssSSK..','KwwwwwwwwwwK','.KKKKKKKKKK.'],
 'grip':['KK','KK'],
}
# note colours: C yellow, D green, E blue, F pink, G black, A red, B brown
NOTE = {'c':'#f8d058','d':'#98e060','e':'#4890e0','f':'#f090c8','g':'#2a2a2a','a':'#e04830','b':'#946030'}
for n, c in NOTE.items(): S['flower-' + n] = (FLOWER, dict(P, P=c, Y='#f8f8f0' if n == 'c' else P['Y']))

def corn(t, w=16, h=24):
    """Corn on the cob; t is tone 0..1: the husk covers the cob up to a line that drops as t rises."""
    g = [['.'] * w for _ in range(h)]
    cob = {y: (6, 9) if y in (2, 3, 19, 20) else (5, 10) for y in range(2, 21)}
    for y, (a, b) in cob.items():
        for x in range(a, b + 1): g[y][x] = 'y' if (x + y) % 2 else 'Y'
    top = round(3 + t * 14)                       # husk top row
    for y in range(top, 22):
        spread = 0 if t < 0.35 else min(3, (y - top) // 3 + 1) if y < top + 6 else 0
        for x in range(4 - spread, 8): 
            if 0 <= x < w: g[y][x] = 'G' if x > 4 - spread else 'g'
        for x in range(8, 12 + spread):
            if 0 <= x < w: g[y][x] = 'D' if x < 11 + spread else 'G'
    for y in range(21, 24):
        for x in (7, 8): g[y][x] = 'D'
    out = [row[:] for row in g]                   # outline every filled pixel's empty neighbours
    for y in range(h):
        for x in range(w):
            if g[y][x] == '.' and any(0 <= y + dy < h and 0 <= x + dx < w and g[y + dy][x + dx] != '.'
                                      for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0))):
                out[y][x] = 'K'
    return [''.join(r) for r in out]

def corn_small(t):
    top = round(1 + t * 4)
    rows = []
    for y in range(8):
        r = ['.'] * 8
        if 1 <= y <= 6:
            for x in (3, 4): r[x] = 'Y'
        if y >= top and y <= 6:
            r[2] = 'G'; r[5] = 'D'
            if t > 0.35 and y == top: r[1] = 'G'; r[6] = 'D'
        if y == 7: r[3] = r[4] = 'D'
        rows.append(r)
    out = [r[:] for r in rows]
    for y in range(8):
        for x in range(8):
            if rows[y][x] == '.' and any(0 <= y + dy < 8 and 0 <= x + dx < 8 and rows[y + dy][x + dx] != '.'
                                         for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0))):
                out[y][x] = 'K'
    return [''.join(r) for r in out]

for tag, t in (('t15', .15), ('t50', .5), ('t90', .9)):
    S['corn-big-' + tag] = corn(t)
    S['crop-cp-corn-' + tag] = corn_small(t)
for name, v in S.items():
    rows, pal = v if isinstance(v, tuple) else (v, P)
    assert len(set(map(len, rows))) == 1, name
    open(f'assets3/{name}.svg', 'w').write(svg(rows, pal))
print('\n'.join(S['corn-big-t90'])); print(); print('\n'.join(S['crop-cp-corn-t50']))
