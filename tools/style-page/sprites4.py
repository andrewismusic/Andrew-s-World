# Round 4 art: subtle sand and parchment textures, kale, flowers in the revised note colours,
# and corn drawn from four sound settings at once (decay = height, tone = husk, tune = ripeness, drive = pests).
import random
src = open('sprites.py').read().split('for name,rows in S.items():')[0]; ns = {}; exec(src, ns)
svg, SNES, FLOWER = ns['svg'], ns['SNES'], ns['S']['flower']
OUT = {}

def texture(w, h, dots, base_pal, seed):
    r = random.Random(seed); g = [['.'] * w for _ in range(h)]
    for ch, n in dots:
        for _ in range(n):
            x, y = r.randrange(w), r.randrange(h)
            g[y][x] = ch
            if ch == 'a' and r.random() < .5 and x + 1 < w: g[y][x + 1] = ch   # some 2-pixel grains
    return [''.join(row) for row in g], base_pal
OUT['tex-sand'] = texture(24, 24, [('a', 7), ('b', 5)], {'a': '#d9bf86', 'b': '#f3e0b0'}, 3)
OUT['tex-parchment'] = texture(24, 24, [('a', 4), ('b', 4)], {'a': '#eadfc2', 'b': '#fcf7ea'}, 7)

KALE = {'K': '#1c2c1c', 'G': '#2f7a4a', 'g': '#5aa868', 'D': '#1f5030'}
OUT['crop-k-kale'] = (['..K.K.K.', '.KgKgKgK', 'KgGgGgGK', 'KGgGgGgK', '.KGgGgK.', '..KDDK..', '...KDK..', '...KK...'], KALE)

NOTE = {'c': '#ffd81c', 'd': '#98e060', 'e': '#2850d8', 'f': '#ff3fa0', 'g': '#4a4a4a', 'a': '#e04830', 'b': '#946030'}
for n, c in NOTE.items():
    OUT['flower-' + n] = (FLOWER, dict(SNES, P=c, Y='#f8f8f0' if n == 'c' else SNES['Y']))

def mix(a, b, t):
    a = [int(a[i:i + 2], 16) for i in (1, 3, 5)]; b = [int(b[i:i + 2], 16) for i in (1, 3, 5)]
    return '#' + ''.join(f'{round(x + (y - x) * t):02x}' for x, y in zip(a, b))

def kernels(r):
    """ripeness 0..1: pale green-yellow, golden, deep orange"""
    stops = [('#e8ec88', '#f6f8c0'), ('#f8cc30', '#fce680'), ('#e88a18', '#f6bc58')]
    i = 0 if r < .5 else 1; t = (r - .5 * i) * 2
    return mix(stops[i][0], stops[i + 1][0], t), mix(stops[i][1], stops[i + 1][1], t)

def outline(g, h, w):
    out = [row[:] for row in g]
    for y in range(h):
        for x in range(w):
            if g[y][x] == '.' and any(0 <= y + dy < h and 0 <= x + dx < w and g[y + dy][x + dx] != '.'
                                      for dy, dx in ((0, 1), (0, -1), (1, 0), (-1, 0))):
                out[y][x] = 'K'
    return [''.join(r) for r in out]

def corn(decay, tone, tune, drive, seed=1):
    w, h = 16, 28; g = [['.'] * w for _ in range(h)]
    base = 22; L = 10 + round(decay * 10); top = base - L
    for y in range(top, base + 1):
        a, b = (6, 9) if y in (top, top + 1, base) else (5, 10)
        for x in range(a, b + 1): g[y][x] = 'y' if (x + y) % 2 else 'Y'
    rnd = random.Random(seed)
    cells = [(y, x) for y in range(top + 1, base) for x in range(5, 11) if g[y][x] != '.']
    for (y, x) in rnd.sample(cells, min(len(cells), round(drive * 10))): g[y][x] = 'n'
    husk_top = round(top + tone * (L - 2))
    flare = 0 if tone < .35 else 1 if tone < .7 else 2
    for y in range(husk_top, base + 2):
        f = flare if y < husk_top + 3 else 0
        for x in range(4 - f, 8): g[y][x] = 'g' if x <= 4 - f else 'G'
        for x in range(8, 12 + f): g[y][x] = 'G' if x >= 11 + f else 'D'
    if drive > .8:
        for (y, x) in rnd.sample([(y, x) for y in range(husk_top, base + 2) for x in range(4, 12) if g[y][x] in 'GD'], 3):
            g[y][x] = 'n'
    if drive > .5:                                   # a worm pokes out near the top of the cob
        wy = top + 2; g[wy][11] = 'W'; g[wy - 1][11] = 'W'; g[wy - 1][12] = 'w'
    for y in range(base + 2, h):
        for x in (7, 8): g[y][x] = 'D'
    if base + 3 < h: g[base + 3][9] = 'G'; g[base + 3][10] = 'G'
    Y, y = kernels(tune)
    pal = dict(SNES, Y=Y, y=y, n='#3a2410', W='#f0a0b0', w='#c06070')
    return outline(g, h, w), pal

def corn_small(decay, tone, tune, drive):
    w, h = 8, 8; g = [['.'] * w for _ in range(h)]
    L = 3 + round(decay * 3); top = 7 - L
    for y in range(top, 7): g[y][3] = g[y][4] = 'Y'
    if drive > .4: g[top + 1][4] = 'n'
    ht = round(top + tone * (L - 1)) if tone > .1 else top
    for y in range(max(ht, top), 7):
        g[y][2] = 'G'; g[y][5] = 'D'
    if tone > .6 and ht - 0 < 7: g[ht][1] = 'G'; g[ht][6] = 'D'
    g[7][3] = g[7][4] = 'D'
    Y, y = kernels(tune)
    return outline(g, h, w), dict(SNES, Y=Y, y=y, n='#3a2410')

# the note view's current clap, plus a sheet of states
OUT['corn-now'] = corn(.62, .9, .5, .2)
OUT['corn-grid-now'] = corn_small(.62, .9, .5, .2)
for i, d in enumerate((.1, .5, .9)):
    for j, t in enumerate((.1, .5, .9)):
        OUT[f'corn-d{i}t{j}'] = corn(d, t, .5, 0)
for i, tu in enumerate((.1, .5, .9)): OUT[f'corn-tune{i}'] = corn(.5, .5, tu, 0)
for i, dr in enumerate((0, .6, 1)): OUT[f'corn-drive{i}'] = corn(.5, .5, .5, dr, seed=4)
for i, (d, t, tu, dr) in enumerate([(.1, .1, .1, 0), (.5, .5, .5, .5), (.9, .9, .9, 1)]):
    OUT[f'corn-small{i}'] = corn_small(d, t, tu, dr)

for name, (rows, pal) in OUT.items():
    assert len(set(map(len, rows))) == 1, name
    open(f'assets4/{name}.svg', 'w').write(svg(rows, pal))
print(len(OUT), 'files'); print('\n'.join(OUT['corn-now'][0])); print(); print('\n'.join(OUT['corn-drive2'][0]))
