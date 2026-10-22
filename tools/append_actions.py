"""Append approved action strips to existing production frames; never redraw base rows.
Usage: python tools/append_actions.py /path/to/action-sources.json
The source map contains the eight named action PNG paths.
"""
from pathlib import Path
import json,sys
from PIL import Image,ImageDraw
from assemble_assets import extract,CW,CH

ROOT=Path(__file__).resolve().parents[1]
NAMES=['stretch','yawn','hair','look','swing','think','clap','hop']
def main():
    sources=json.loads(Path(sys.argv[1]).read_text());assets=ROOT/'assets'
    rows=[];report=[]
    for name,count in [('idle',4),('walk',4),('actions',3),('petting',4),('eating',4)]:
        rows.append([Image.open(assets/'frames'/name/f'{i}.png').convert('RGBA') for i in range(count)])
    for name in NAMES:
        frames,qa=extract(sources[name],4,target_height=310 if name=='swing' else 396,preserve_vertical=name=='hop')
        folder=assets/'frames'/name;folder.mkdir(exist_ok=True)
        for i,frame in enumerate(frames):frame.save(folder/f'{i}.png',optimize=True)
        rows.append(frames);report.append({'name':name,**qa})
    atlas=Image.new('RGBA',(CW*4,CH*len(rows)))
    for y,row in enumerate(rows):
        for x,frame in enumerate(row):atlas.alpha_composite(frame,(x*CW,y*CH))
    atlas.save(assets/'atlas.png',optimize=True)
    (assets/'new-action-validation.json').write_text(json.dumps({'cell':[CW,CH],'atlas':list(atlas.size),'rows':report},indent=2))
    board=Image.new('RGB',(CW*4,CH*8),(247,243,239))
    for y,row in enumerate(rows[5:]):
        for x,frame in enumerate(row):board.paste(frame,(x*CW,y*CH),frame)
        ImageDraw.Draw(board).text((8,y*CH+5),NAMES[y],fill=(60,40,50))
    board.save(ROOT/'build'/'new-actions-contact.png')
    print(json.dumps(report))
if __name__=='__main__':main()
