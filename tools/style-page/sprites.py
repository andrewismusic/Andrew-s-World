# Writes crisp pixel-art SVGs from text grids: the prototype's sprites plus a few new 8x8 tiles, in two palettes.
import os
NES={'K':'#000000','W':'#fcfcfc','S':'#bcbcbc','s':'#7c7c7c','R':'#d82800','r':'#a81000','O':'#f87800','o':'#fca044',
     'Y':'#f8b800','y':'#fce0a8','G':'#00a800','g':'#58d854','D':'#005800','N':'#ac7c00','n':'#503000','T':'#fcd8a8',
     'P':'#f878f8','B':'#0058f8','b':'#3cbcfc','L':'#b8f818'}
SNES=dict(NES,**{'K':'#1c2c1c','W':'#f8f8f0','R':'#e04830','r':'#a03020','O':'#f09038','o':'#f8c878','Y':'#f8d058',
     'G':'#4ca048','g':'#8cd070','D':'#2a6a34','N':'#946030','n':'#5c3a1c','T':'#e8d098','P':'#f090c8','B':'#3870d0','b':'#78b8f0','L':'#c0e890'})
S={
 'tree':['....KKKKKK....','..KKgGGGgGKK..','.KgGGGGGGGgGK.','KgGGDGGGGDGGgK','KGGGGGGgGGGGGK','KGDGGGGGGGGDGK','KGGGGgGGGDGGGK','KGGDGGGGGGGGGK','.KGGGGGDGGGGK.','.KDGGGGGGGGDK.','..KKDGGGGDKK..','....KKKKKK....','.....KNNK.....','.....KNnK.....','.....KNNK.....','.....KNnK.....','....KNNNNK....','...KKKKKKKK...'],
 'pine':['....KK....','...KGGK...','...KGGK...','..KGGgGK..','..KGDGGK..','.KGGGGgGK.','..KKGGKK..','.KGGgGGGK.','KGGDGGGgGK','.KKGGGGKK.','KGGGgGDGGK','KGDGGGGGGK','.KKKKKKKK.','...KNNK...','...KNnK...','...KNNK...','..KKKKKK..'],
 'pumpkin':['...DD...','.KKDKKK.','KOoOOoOK','KoOOoOOK','KoOOoOOK','KOoOOoOK','.KrOOrK.','..KKKK..'],
 'bush':['..KKKK..','.KgGGgK.','KgGGGGGK','KGGDGGgK','KGgGGDGK','KDGGGGDK','.KDDDDK.','..KKKK..'],
 'flower':['...KKKK...','..KPPPPK..','.KPPKKPPK.','KPPKYYKPPK','KPPKYYKPPK','.KPPKKPPK.','..KPPPPK..','...KKGK...','GGGGGGGGGG','DDDDDDDDDD'],
 'die':['.KKKKKKK.','KWWWWWWWK','KWKWWWWWK','KWWWWWWWK','KWWWKWWWK','KWWWWWWWK','KWWWWWKWK','KWWWWWWWK','.KKKKKKK.'],
 'flag':['KKKKK...','KRRRRKK.','KRRRRRRK','KRRRRKK.','KKKKK...','K.......','K.......','K.......','KK......'],
 'vine':['........','........','........','........','.....KK.','....KgGK','....KGGK','.....KG.','GGGGGGGG','DDDDDDDD'],
}
def svg(rows,pal):
    h=len(rows);w=len(rows[0]);out=[]
    for y,row in enumerate(rows):
        x=0
        while x<w:
            c=row[x];e=x
            while e+1<w and row[e+1]==c: e+=1
            if c in pal: out.append(f'<rect x="{x}" y="{y}" width="{e-x+1}" height="1" fill="{pal[c]}"/>')
            x=e+1
    return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="{w}" height="{h}" shape-rendering="crispEdges">{"".join(out)}</svg>'
for name,rows in S.items():
    assert len(set(map(len,rows)))==1,name
    for tag,pal in (('nes',NES),('snes',SNES)):
        open(f'assets/{name}-{tag}.svg','w').write(svg(rows,pal))
print(sorted(os.listdir('assets')))
