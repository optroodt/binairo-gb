import sys, random; sys.path.insert(0,'test')
from common_flow import *
h=H()
rng=random.Random(int(sys.argv[1]) if len(sys.argv)>1 else 1)
btns=["a","b","up","down","left","right","start","select"]
weights=[5,2,4,4,4,4,1,1]
h.tick(150)
bad=0; scenes=set()
for i in range(int(sys.argv[2]) if len(sys.argv)>2 else 3000):
    b=rng.choices(btns,weights)[0]
    h.press(b, hold=rng.randint(1,4), after=rng.randint(1,12))
    pc=h.pb.register_file.PC
    sc=scene(h); scenes.add(sc)
    if sc>5 or (0x8000<=pc<0xC000 and not (0xA000<=pc<0xC000)) or (0xA000<=pc<0xC000):
        print("BAD", i, hex(pc), sc); bad+=1; h.shot("fuzz_bad_%d"%i); break
print("done scenes",sorted(scenes),"bad",bad, "frames", h.pb.frame_count)
h.shot("fuzz_end")
