import sys
from PIL import Image
names=sys.argv[2:]
ims=[Image.open('build/shots/%s.png'%n) for n in names]
cols=3; rows=(len(ims)+cols-1)//cols
im=Image.new('RGB',(490*cols,442*rows),(255,0,255))
for k,i in enumerate(ims): im.paste(i,((k%cols)*490,(k//cols)*442))
im.save('build/shots/%s.png'%sys.argv[1])
