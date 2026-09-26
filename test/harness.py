import sys, os
from pyboy import PyBoy

class H:
    def __init__(self, rom="binairo.gb", cgb=False, outdir="build/shots"):
        self.pb = PyBoy(rom, window="null", cgb=cgb, sound_emulated=False)
        self.pb.set_emulation_speed(0)
        self.out = outdir
        os.makedirs(outdir, exist_ok=True)
        self.n = 0
    def tick(self, n=1):
        self.pb.tick(n, True)
    def press(self, b, hold=3, after=4):
        self.pb.button_press(b)
        self.tick(hold)
        self.pb.button_release(b)
        self.tick(after)
    def shot(self, name, scale=3):
        img = self.pb.screen.image.convert("RGB")
        img = img.resize((160 * scale, 144 * scale), 0)
        p = os.path.join(self.out, name + ".png")
        img.save(p)
        return p
    def mem(self, addr):
        return self.pb.memory[addr]
