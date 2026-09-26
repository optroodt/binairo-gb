import sys; sys.path.insert(0,'test')
from common_flow import *
h=H()
print("tutorial hints:", finish_tutorial(h))
h.tick(60); h.shot("30_home")
h.press("a"); h.tick(90); h.shot("31_game6e")
h.press("start"); h.tick(60); h.shot("32_pause")
h.press("down"); h.tick(10); h.press("down"); h.tick(10); h.press("a"); h.tick(20); h.shot("33_theme")
h.press("b"); h.tick(80); h.shot("34_game_theme")
n=solve_with_hints(h, shots={0:"35_h0",3:"36_h3"})
print("game hints:", n)
h.tick(60); h.shot("37_result")
h.save=h.pb.save_state
import io
f=open('build/after_first.state','wb'); h.pb.save_state(f); f.close()
