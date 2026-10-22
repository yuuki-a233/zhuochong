"""Extract generated pose groups; preserve shared row scale and a fixed foot baseline."""
from pathlib import Path
import argparse, json
import numpy as np
from PIL import Image

CW, CH = 320, 420

def extract(path, expected, target_height=396, preserve_vertical=False):
    image = Image.open(path).convert('RGBA')
    data = np.array(image)
    # Tiny alpha remnants from generation are not part of a readable sprite.
    data[data[:, :, 3] < 25] = 0
    mask = data[:, :, 3] >= 100
    columns = np.flatnonzero(mask.sum(axis=0) > 8)
    cuts = np.flatnonzero(np.diff(columns) > 12)
    ranges = [(int(g[0]), int(g[-1]) + 1) for g in np.split(columns, cuts+1) if len(g) > 20]
    if len(ranges) != expected:
        raise ValueError(f'{path}: expected {expected} pose groups, got {ranges}')
    groups=[];bottoms=[]
    for i,(left,right) in enumerate(ranges):
        # Include antialiasing and detached hair tips near the connected pose.
        left=max(0,left-8); right=min(image.width,right+8)
        crop=Image.fromarray(data[:,left:right,:])
        box=crop.getchannel('A').getbbox()
        if not box: raise ValueError('Empty pose')
        groups.append(crop.crop(box))
        bottoms.append(box[3])
    baseline=max(bottoms)
    extents=[g.height+(baseline-bottom if preserve_vertical else 0) for g,bottom in zip(groups,bottoms)]
    scale=min((CW-16)/max(g.width for g in groups),target_height/max(extents))
    frames=[]
    for group,bottom in zip(groups,bottoms):
        resized=group.resize((round(group.width*scale),round(group.height*scale)),Image.Resampling.LANCZOS)
        frame=Image.new('RGBA',(CW,CH))
        lift=round((baseline-bottom)*scale) if preserve_vertical else 0
        frame.alpha_composite(resized,((CW-resized.width)//2,CH-8-resized.height-lift))
        frames.append(frame)
    return frames, {'source':str(path),'count':len(groups),'scale':scale,'source_sizes':[g.size for g in groups],
                    'preserve_vertical':preserve_vertical,'source_bottoms':bottoms}

def main():
    p=argparse.ArgumentParser()
    for name in ['idle','walk','actions','petting','eating']:p.add_argument('--'+name,required=True)
    p.add_argument('--output',required=True)
    a=p.parse_args();out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
    rows=[];report=[]
    for name,count in [('idle',4),('walk',4),('actions',3),('petting',4),('eating',4)]:
        frames,qa=extract(getattr(a,name),count);rows.append(frames);report.append(qa)
        d=out/'frames'/name;d.mkdir(parents=True,exist_ok=True)
        for i,f in enumerate(frames):f.save(d/f'{i}.png')
    atlas=Image.new('RGBA',(CW*4,CH*5))
    for y,row in enumerate(rows):
        for x,f in enumerate(row):atlas.alpha_composite(f,(x*CW,y*CH))
    atlas.save(out/'atlas.png',optimize=True)
    rows[0][0].save(out/'yuuki.ico',sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])
    report={'ok':True,'cell':[CW,CH],'atlas':list(atlas.size),'rows':report}
    (out/'asset-validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2))
    print(json.dumps(report,ensure_ascii=False))

if __name__=='__main__':main()
