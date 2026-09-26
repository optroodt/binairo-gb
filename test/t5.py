import sys; sys.path.insert(0,'test')
from common_flow import *
h=H()
print("tutorial hints:", finish_tutorial(h))
h.tick(60)
h.press("right"); h.tick(20); h.press("down"); h.tick(20); h.shot("40_home8h")
h.press("a"); h.tick(90); h.shot("41_game8h")
# place a wrong symbol for mistake hint: move to first empty and set
h.press("a"); h.tick(10); h.press("a"); h.tick(10); h.shot("42_placed")
h.press("select"); h.tick(10); h.shot("43_undo")
n=solve_with_hints(h, maxn=120, shots={0:"44_h0",5:"45_h5",10:"46_h10",20:"47_h20",30:"48_h30"})
print("game hints:", n)
h.tick(60); h.shot("49_result")
f=open('build/after8.state','wb'); h.pb.save_state(f); f.close()
