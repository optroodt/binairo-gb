#!/usr/bin/env python3
"""Generate Binairo puzzles for the GB port.

Easy  = solvable with pairs / trios / counting only.
Hard  = needs line reasoning (impossible combinations / unique rows),
        i.e. NOT solvable with the easy techniques alone.
Every puzzle is solvable by pure logic, so it has a unique solution,
and the in-game hint engine (same techniques) can always find a step.
"""
import random, sys, itertools

def valid_patterns(n):
    pats = []
    for p in range(1 << n):
        if bin(p).count("1") != n // 2:
            continue
        bits = [(p >> i) & 1 for i in range(n)]
        if any(bits[i] == bits[i+1] == bits[i+2] for i in range(n - 2)):
            continue
        pats.append(tuple(bits))
    return pats

PATS = {6: valid_patterns(6), 8: valid_patterns(8)}

def lines(n):
    L = []
    for r in range(n):
        L.append([r * n + c for c in range(n)])
    for c in range(n):
        L.append([r * n + c for r in range(n)])
    return L

LINES = {6: lines(6), 8: lines(8)}

def random_solution(n, rng):
    pats = PATS[n][:]
    rows = []
    def col_ok(rows):
        k = len(rows)
        for c in range(n):
            col = [rows[r][c] for r in range(k)]
            if col.count(1) > n // 2 or col.count(0) > n // 2:
                return False
            if k >= 3 and col[-1] == col[-2] == col[-3]:
                return False
        return True
    def rec():
        if len(rows) == n:
            cols = [tuple(rows[r][c] for r in range(n)) for c in range(n)]
            return len(set(cols)) == n
        cand = pats[:]
        rng.shuffle(cand)
        for p in cand:
            if p in rows:
                continue
            rows.append(p)
            if col_ok(rows) and rec():
                return True
            rows.pop()
        return False
    assert rec()
    return [v for row in rows for v in row]

def step_easy(g, n):
    """One pass of pairs / trios / counts. Returns True if progress."""
    prog = False
    h = n // 2
    for L in LINES[n]:
        a = [g[i] for i in L]
        for i in range(n - 1):
            if a[i] != -1 and a[i] == a[i+1]:
                for j in (i - 1, i + 2):
                    if 0 <= j < n and a[j] == -1:
                        a[j] = 1 - a[i]; prog = True
        for i in range(n - 2):
            if a[i] != -1 and a[i] == a[i+2] and a[i+1] == -1:
                a[i+1] = 1 - a[i]; prog = True
        for s in (0, 1):
            if a.count(s) == h and -1 in a:
                for j in range(n):
                    if a[j] == -1:
                        a[j] = 1 - s; prog = True
        for k, i in enumerate(L):
            g[i] = a[k]
    return prog

def step_hard(g, n, use_unique=True):
    prog = False
    for li, L in enumerate(LINES[n]):
        a = [g[i] for i in L]
        if -1 not in a:
            continue
        excl = set()
        if use_unique:
            same = LINES[n][:n] if li < n else LINES[n][n:]
            for L2 in same:
                b = tuple(g[i] for i in L2)
                if -1 not in b:
                    excl.add(b)
        cands = [p for p in PATS[n]
                 if p not in excl and all(a[k] == -1 or a[k] == p[k] for k in range(n))]
        if not cands:
            return None  # contradiction
        for k in range(n):
            if a[k] == -1:
                vals = {p[k] for p in cands}
                if len(vals) == 1:
                    g[L[k]] = vals.pop(); prog = True
    return prog

def solve(g, n, level):
    g = g[:]
    while -1 in g:
        if step_easy(g, n):
            continue
        if level >= 2:
            r = step_hard(g, n)
            if r:
                continue
        return None
    return g

def make_puzzle(n, level, rng):
    while True:
        sol = random_solution(n, rng)
        g = sol[:]
        cells = list(range(n * n))
        rng.shuffle(cells)
        for c in cells:
            v = g[c]
            g[c] = -1
            if solve(g, n, level) != sol:
                g[c] = v
        if level == 2 and solve(g, n, 1) == sol:
            continue  # too easy for hard
        if level == 1:
            # keep easy puzzles friendly: needs at least a few count steps
            pass
        return sol, [1 if v != -1 else 0 for v in g]

def encode(n, sol, giv):
    out = []
    for r in range(n):
        b = 0
        for c in range(n):
            b |= sol[r * n + c] << c
        out.append(b)
    for r in range(n):
        b = 0
        for c in range(n):
            b |= giv[r * n + c] << c
        out.append(b)
    return out

def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 120
    rng = random.Random(20260926)
    sets = {}
    stats = {}
    for n in (6, 8):
        for level, name in ((1, "easy"), (2, "hard")):
            seen = set()
            lst = []
            while len(lst) < count:
                sol, giv = make_puzzle(n, level, rng)
                key = (tuple(sol), tuple(giv))
                if key in seen:
                    continue
                seen.add(key)
                lst.append(encode(n, sol, giv))
                stats.setdefault((n, name), []).append(sum(giv))
            sets[(n, name)] = lst
    for k, v in stats.items():
        print(k, "givens avg %.1f min %d max %d" % (sum(v)/len(v), min(v), max(v)), file=sys.stderr)

    # tutorial puzzle (taken from the rules demo)
    T = [
        "..0.10",
        "..1.0.",
        "..0...",
        "..1.11",
        "..0.10",
        "..10.1",
    ]
    tg = [-1 if ch == "." else int(ch) for row in T for ch in row]
    ts = solve(tg, 6, 2)
    print("tutorial solution", ts, file=sys.stderr)
    ts1 = solve(tg, 6, 1)
    print("tutorial easy-solvable:", ts1 is not None, file=sys.stderr)
    tut = encode(6, ts, [1 if v != -1 else 0 for v in tg])

    with open(sys.argv[2] if len(sys.argv) > 2 else "src/puzzles.c", "w") as f:
        f.write("// Generated by tools/gen_puzzles.py - do not edit\n")
        f.write("#pragma bank 3\n#include <stdint.h>\n#include \"puzzles.h\"\n\n")
        for (n, name), lst in sets.items():
            f.write("const uint8_t puz_%d_%s[%d][%d] = {\n" % (n, name, len(lst), 2 * n))
            for e in lst:
                f.write("  {" + ",".join("0x%02X" % b for b in e) + "},\n")
            f.write("};\n\n")
        f.write("const uint8_t puz_tutorial[12] = {" + ",".join("0x%02X" % b for b in tut) + "};\n")
    with open("src/puzzles.h", "w") as f:
        f.write("#ifndef PUZZLES_H\n#define PUZZLES_H\n#include <stdint.h>\n")
        f.write("#define PUZZLE_COUNT %d\n" % count)
        for (n, name) in sets:
            f.write("extern const uint8_t puz_%d_%s[%d][%d];\n" % (n, name, count, 2 * n))
        f.write("extern const uint8_t puz_tutorial[12];\n#endif\n")

if __name__ == "__main__":
    main()
