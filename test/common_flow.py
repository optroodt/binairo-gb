import sys; sys.path.insert(0,'test')
from harness import H
import re
_A=None
def scene(h):
    global _A
    if _A is None:
        m=re.search(r'([0-9A-F]{8})  _cur_scen', open('binairo.map').read()); _A=int(m.group(1),16)
    return h.mem(_A)
def sym(name):
    m=re.search(r'([0-9A-F]{8})  %s'%name, open('binairo.map').read()); return int(m.group(1),16)
def cells_full(h):
    a=sym('_cell ')
    n=6 if h.mem(sym('_N '))==6 else 8
    return all(h.mem(a+r*8+c) for r in range(n) for c in range(n))
def in_bitmap(h): return scene(h) in (0,1,3,4)
def finish_tutorial(h):
    h.tick(150); h.press("a"); h.tick(60)
    h.press("a"); h.tick(40)
    h.press("a"); h.tick(10); h.press("right"); h.tick(10); h.press("a"); h.tick(10)
    h.press("right"); h.tick(10); h.press("right"); h.tick(10); h.press("a"); h.tick(100)
    h.press("a"); h.tick(10)
    for i in range(3): h.press("down"); h.tick(6)
    h.press("a"); h.tick(100)
    h.press("a"); h.tick(10); h.press("down"); h.tick(8); h.press("a"); h.tick(8); h.press("a"); h.tick(100)
    h.press("a"); h.tick(40)
    return solve_with_hints(h)
def solve_with_hints(h, maxn=80, shots=None):
    for i in range(maxn):
        h.press("b"); h.tick(45)
        if shots is not None and i in shots: h.shot(shots[i]+"_p1")
        h.press("a"); h.tick(35)
        if shots is not None and i in shots: h.shot(shots[i]+"_p2")
        h.press("a"); h.tick(35)
        if in_bitmap(h): return i
        if cells_full(h):
            for k in range(12):
                h.tick(20)
                if in_bitmap(h): return i
    return -1
