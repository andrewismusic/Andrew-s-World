# Round 2 sprites in the SNES palette: a crop per drum voice, flowers coloured by scale degree, a walking farmer.
import importlib.util, sys
spec = importlib.util.spec_from_file_location('s1', 'sprites.py'); s1 = importlib.util.module_from_spec(spec)
src = open('sprites.py').read().split('for name,rows in S.items():')[0]; exec(src, s1.__dict__)
P = dict(s1.SNES, V='#7850a8', v='#a878d0', T='#f0c8a0')
S = {
 'crop-kick':['...DD...','.KKDKKK.','KOoOOoOK','KoOOoOOK','KoOOoOOK','KOoOOoOK','.KrOOrK.','..KKKK..'],
 'crop-snare':['..KKKK..','.KgLLgK.','KgLgLLgK','KLgLLgLK','KgLLgLgK','KLgLgLLK','.KgLLgK.','..KKKK..'],
 'crop-clap':['...KK...','..KYYK..','.KYyYYK.','.KYYyYK.','.KGYYGK.','KGGYYGGK','.KGGGGK.','..KKKK..'],
 'crop-rim':['.KG.GK..','..KGK...','..KRRK..','.KRRWRK.','.KRRRRK.','..KRRK..','...KK...','...K....'],
 'crop-chat':['.Y..Y..Y','.YK.YK.Y','.KY.KY.K','..KYKYK.','...KYK..','...KGK..','...KGK..','..KKKKK.'],
 'crop-ohat':['..KKKK..','.KYYYYK.','KYYnnYYK','KYnNNnYK','KYYnnYYK','.KYYYYK.','..KGGK..','...KGK..'],
 'crop-tom':['...KGK..','..KGGK..','.KVvVVK.','KVvVVVVK','KVVVVVVK','KVVVVVVK','.KVVVVK.','..KKKK..'],
 'crop-perc':['..GKKG..','.KGGGGK.','KRRyRRRK','KRRRRyRK','KRyRRRRK','.KRRRyK.','..KRRK..','...KK...'],
 'farmer':['..KKKK..','.KYYYYK.','KKKKKKKK','.KTTTTK.','.KTKKTK.','.KTTTTK.','..KRRK..','.KRRRRK.','KTKRRKTK','..KBBK..','..KBBK..','..KK.KK.'],
 'bubble':['.KKKKK.','KWWKWWK','KWWKWWK','KWWKWWK','KWWWWWK','KWWKWWK','.KKKKK.','..KK...','..K....'],
}
FLOWER = s1.S['flower']
DEG = ['#e04830','#f09038','#f8d058','#f8f8f0','#78b8f0','#6870d8','#f090c8']
for i, c in enumerate(DEG): S[f'flower-d{i+1}'] = (FLOWER, dict(P, P=c))
for name, v in S.items():
    rows, pal = (v if isinstance(v, tuple) else (v, P))
    assert len(set(map(len, rows))) == 1, name
    open(f'assets2/{name}.svg', 'w').write(s1.svg(rows, pal))
print(len(S), 'sprites')
