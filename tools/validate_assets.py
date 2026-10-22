from pathlib import Path
import json
import numpy as np
from PIL import Image
root=Path(__file__).resolve().parents[1];assets=root/'assets'
names=['idle','walk','actions','petting','eating','stretch','yawn','hair','look','swing','think','clap','hop']
atlas=Image.open(assets/'atlas.png').convert('RGBA');assert atlas.size==(1280,5460)
report=[]
for y,name in enumerate(names):
 frames=[];bottoms=[]
 for x in range(3 if name=='actions' else 4):
  frame=Image.open(assets/'frames'/name/f'{x}.png').convert('RGBA');assert frame.size==(320,420)
  assert np.array_equal(np.array(frame),np.array(atlas.crop((x*320,y*420,(x+1)*320,(y+1)*420))))
  a=np.array(frame)[:,:,3];ys,xs=np.where(a>=100);assert len(xs)>3000
  assert xs.min()>1 and xs.max()<318 and ys.min()>1 and ys.max()<418,(name,x,'clipped')
  assert np.all(a[0,:]==0) and np.all(a[-1,:]==0) and np.all(a[:,0]==0) and np.all(a[:,-1]==0)
  frames.append(np.array(frame));bottoms.append(int(ys.max()))
 if y>=5:
  assert len({f.tobytes() for f in frames})==4,(name,'duplicate frame')
 if name=='hop':assert bottoms[2]<bottoms[0]-30 and bottoms[1]<bottoms[0]-15 and abs(bottoms[3]-bottoms[0])<3
 report.append({'name':name,'frames':len(frames),'bottoms':bottoms})
result={'ok':True,'atlas':list(atlas.size),'cell':[320,420],'frame_count':sum(r['frames'] for r in report),'rows':report}
(assets/'frame-quality.json').write_text(json.dumps(result,indent=2))
p=assets/'asset-validation.json';original=json.loads(p.read_text());original.update({'atlas':list(atlas.size),'frame_count':result['frame_count'],'appended_actions':'new-action-validation.json'});p.write_text(json.dumps(original,indent=2))
print('51 atlas frames match source cells, have transparent safe borders, and new action frames are distinct. Hop keeps verified rise/peak/landing displacement.')
