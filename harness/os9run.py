#!/usr/bin/env python3
"""os9run - run an OS-9 Level 2 program (6809 module) on the host.

A small model of one OS-9 process: the module, its data area and the
system calls a CoCoC-compiled C program makes (files, memory, time,
F$Fork/F$Wait for the dcc driver), answered from a host directory that
stands for the disk. The CPU is harness/cpu6809.py.

It is a debugging aid for phase 7, not the proof: MAME (harness/native.py)
is. It runs in seconds where a MAME round trip takes minutes, and it can
say what the program did (system call trace, memory high-water mark,
instruction counts).

  python3 harness/os9run.py [options] MODULE [ARGS...]

  --root DIR      the disk: /dd/... and /d0/... name files under DIR
                  (default: the current directory)
  --cwd DIR       data directory, relative to --root (default: the root)
  --cmds DIR      where F$Fork finds modules (default: ROOT/CMDS)
  --stdout FILE   path 1 (default: the terminal, CR shown as LF)
  --stderr FILE   path 2 (default: the terminal)
  --mem N         extra data memory in bytes, like OS-9's "#N" modifier
  --trace         print every system call to stderr
  --map FILE      llink's link map (-m) of the module: names for addresses
  --break NAME    each time NAME is entered, print the calls on the stack
                  (NAME+HEX: each time that address inside NAME is reached)
                  (needs --map; repeatable)
  --stats         print instructions executed and memory used at exit

Files are used as they are: text inputs must already have CR line ends,
as on an OS-9 disk (native.py prepares them that way). Names match
case-insensitively, as RBF does.
"""
import argparse
import datetime
import os
import time
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from cpu6809 import CPU6809, Halt, C  # noqa: E402

# OS-9 error codes used here
E_EOF, E_PNNF, E_FNA, E_BPNUM, E_MNF, E_MEMFUL, E_UNKSVC, E_CEF, E_BMODE, E_SEEK = \
    211, 216, 214, 201, 221, 207, 208, 218, 203, 247
E_ICPT_IGNORED = 0

SYSNAMES = {
    0x00: "F$Link", 0x01: "F$Load", 0x02: "F$UnLink", 0x03: "F$Fork", 0x04: "F$Wait",
    0x05: "F$Chain", 0x06: "F$Exit", 0x07: "F$Mem", 0x08: "F$Send", 0x09: "F$Icpt",
    0x0A: "F$Sleep", 0x0C: "F$ID", 0x0D: "F$SPrior", 0x0F: "F$PErr", 0x10: "F$PrsNam",
    0x15: "F$Time", 0x17: "F$CRC", 0x39: "F$AllRAM", 0x4F: "F$MapBlk", 0x50: "F$ClrBlk",
    0x51: "F$DelRAM", 0x80: "I$Attach", 0x82: "I$Dup", 0x83: "I$Create", 0x84: "I$Open",
    0x85: "I$MakDir", 0x86: "I$ChgDir", 0x87: "I$Delete", 0x88: "I$Seek", 0x89: "I$Read",
    0x8A: "I$Write", 0x8B: "I$ReadLn", 0x8C: "I$WritLn", 0x8D: "I$GetStt",
    0x8E: "I$SetStt", 0x8F: "I$Close",
}

PATHCHARS = set(b"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._$/-")


class OS9Error(Exception):
    def __init__(self, code):
        self.code = code


class Exit(Exception):
    def __init__(self, status):
        self.status = status


class TermFile:
    """Path 1 or 2 on the terminal: CR shown as LF."""

    def __init__(self, stream):
        self.stream = stream
        self.scf = True

    def write(self, data):
        self.stream.write(data.replace(b"\r", b"\n").decode("latin-1"))
        self.stream.flush()

    def read(self, n):
        return b""

    def readline(self, n):
        return b""

    def close(self):
        pass


class HostFile:
    def __init__(self, f, scf=False):
        self.f = f
        self.scf = scf
        self.refs = 1

    def write(self, data):
        self.f.write(data)

    def read(self, n):
        return self.f.read(n)

    def readline(self, n):
        out = bytearray()
        while len(out) < n:
            c = self.f.read(1)
            if not c:
                break
            out += c
            if c == b"\r":
                break
        return bytes(out)

    def close(self):
        self.refs -= 1
        if self.refs == 0:
            self.f.close()


class Disk:
    """Host directory standing for /dd (and /d0), names case-insensitive."""

    def __init__(self, root, cwd, cmds):
        self.root = os.path.abspath(root)
        self.cwd = os.path.join(self.root, cwd) if cwd else self.root
        self.cmds = cmds or self.find(os.path.join(self.root, "CMDS"), want=False)

    def find(self, path, want=True):
        """Resolve case-insensitively; with want=False the last part may not exist."""
        parts = os.path.relpath(path, "/").split(os.sep)
        cur = "/"
        for i, p in enumerate(parts):
            cand = os.path.join(cur, p)
            if not os.path.exists(cand) and os.path.isdir(cur):
                for e in os.listdir(cur):
                    if e.lower() == p.lower():
                        cand = os.path.join(cur, e)
                        break
            cur = cand
        return cur

    def host(self, name, exe=False):
        low = name.lower()
        for dev in ("/dd", "/d0", "/h0"):
            if low == dev or low.startswith(dev + "/"):
                return self.find(self.root + name[len(dev):])
        if name.startswith("/"):
            raise OS9Error(E_PNNF)          # another device: not modelled
        return self.find(os.path.join(self.cmds if exe else self.cwd, name))


def crc_byte(acc, b):
    """One byte of OS-9's module CRC (as las module.c crc_byte)."""
    b ^= acc[0]
    acc = [acc[1], acc[2], 0]
    acc[1] ^= b >> 7
    acc[2] = (b << 1) & 0xFF
    acc[1] ^= b >> 2
    acc[2] ^= (b << 6) & 0xFF
    b ^= (b << 1) & 0xFF
    b ^= (b << 2) & 0xFF
    b ^= (b << 4) & 0xFF
    if b & 0x80:
        acc[0] ^= 0x80
        acc[2] ^= 0x21
    return acc


def read_map(path):
    """llink -m -s output -> sorted [(module offset, name)] of code symbols.
    Static functions come from each object's las listing (-l -s), found
    next to its .r file, at the section's code base from the map."""
    import re
    syms = []
    base = os.path.dirname(os.path.dirname(os.path.abspath(path)))
    sect = re.compile(r"^(\S+)\s+([0-9a-f]{4}) [0-9a-f]{4} [0-9a-f]{4} [0-9a-f]+\s+[0-9a-f]+ (\S+\.r)$")
    entry = re.compile(r"(\S+)\s+04 [0-9a-f]{2} ([0-9a-f]{4})\b")
    for line in open(path, encoding="latin-1"):
        t = line.split()
        if len(t) == 3 and t[1] == "code":
            syms.append((int(t[2], 16), t[0]))
            continue
        m = sect.match(line.rstrip())
        if not m:
            continue
        lst = os.path.join(base, m.group(3)[:-2] + ".lst")
        if not os.path.exists(lst):
            continue
        cbase = int(m.group(2), 16)
        for l in open(lst, encoding="latin-1"):
            if "|" not in l and not re.match(r"^\s+\S+\s+04 ", l):
                continue
            for n, v in entry.findall(l):
                if not re.match(r"^_\$?\d+$", n):
                    syms.append((cbase + int(v, 16), n))
    return sorted(set(syms))


# Physical RAM blocks from F$AllRAM: number -> [8K bytearray, owner pid]
RAM = {}
NEXTBLK = [0x40]


class Process:
    def __init__(self, disk, module, params, paths, mem_extra=0, trace=False, parent=None,
                 syms=None, breaks=()):
        self.disk = disk
        self.trace = trace
        self.syms = syms or []
        self.breaks = {}
        self.parent = parent
        self.cpu = CPU6809()
        self.paths = paths                  # list: path number -> file object or None
        self.children = []                  # (id, status) of finished children
        self.instr = 0
        self.pid = (parent.pid + 1) if parent else 2
        m = self.cpu.mem
        if module[0:2] != b"\x87\xcd":
            raise SystemExit("not an OS-9 module")
        size = module[2] << 8 | module[3]
        exe = module[9] << 8 | module[10]
        datasz = module[11] << 8 | module[12]
        # The module sits at the top of the 64K, in 8K blocks; on this
        # NitrOS9 it must end by $FD00 (DESIGN.md D25). The data area
        # grows up from $0000.
        top = 0xFD00
        self.modbase = (top - size) & ~0x1FFF
        m[self.modbase:self.modbase + size] = module[:size]
        params = params + b"\r"
        mem = (datasz + mem_extra + len(params) + 0xFF) & ~0xFF
        # F$Fork won't start a process whose module and initial data fill
        # all 8 slots (D25); F$Mem can grow it later
        if (mem + 0x1FFF) // 0x2000 + (0x10000 - self.modbase) // 0x2000 >= 8:
            raise OS9Error(E_MEMFUL)
        self.mapped = {}                    # slot -> block from F$MapBlk
        self.mapblks = 0
        self.memtop = mem
        self.highwater = mem
        pstart = mem - len(params)
        self.stacktop = self.minsp = pstart
        self.minpc = None
        m[pstart:mem] = params
        c = self.cpu
        c.u = 0
        c.dp = 0
        c.y = mem
        c.x = c.s = pstart
        c.d = len(params)
        c.pc = self.modbase + exe
        c.cc = 0x50
        self.codesize = size
        for off, n in self.syms:
            if n in breaks:
                self.breaks[self.modbase + off] = n
            for br in breaks:             # NAME+HEX: an address inside NAME
                if "+" in br and br.split("+")[0] == n:
                    self.breaks[self.modbase + off + int(br.split("+")[1], 16)] = br

    def where(self, addr):
        """function+offset for a code address"""
        off = addr - self.modbase
        best = None
        for o, n in self.syms:
            if o <= off:
                best = (o, n)
            else:
                break
        return f"{best[1]}+{off - best[0]:X}" if best else f"${addr:04X}"

    def backtrace(self, name):
        """Words on the stack that are return addresses (they follow a JSR or
        LBSR in the module) - a heuristic, but DCC code calls with LBSR."""
        c = self.cpu
        out = self.callers(c.s, c.mem[c.s:min(c.s + 2048, 0xFE00) + 1])
        print(f"break {name}: D=${(c.a << 8) | c.b:04X} X=${c.x:04X} Y=${c.y:04X} S=${c.s:04X} " + " <- ".join(out), file=sys.stderr)

    def callers(self, s, stack):
        """Return addresses in a copy of the stack starting at address s."""
        m = self.cpu.mem
        out = []
        for i in range(len(stack) - 1):
            r = stack[i] << 8 | stack[i + 1]
            if not (self.modbase < r < self.modbase + self.codesize):
                continue
            if m[r - 3] in (0x17, 0xBD) or m[r - 2] == 0x8D:
                out.append(self.where(r))
            if len(out) == 40:
                break
        return out

    # ----------------------------------------------------------- helpers
    def name_at(self, addr):
        m = self.cpu.mem
        out = bytearray()
        while m[addr] in PATHCHARS:
            out.append(m[addr])
            addr += 1
        return out.decode("latin-1"), addr

    def newpath(self, f):
        for i, p in enumerate(self.paths):
            if p is None:
                self.paths[i] = f
                return i
        self.paths.append(f)
        return len(self.paths) - 1

    def path(self, n):
        if n >= len(self.paths) or self.paths[n] is None:
            raise OS9Error(E_BPNUM)
        return self.paths[n]

    def fail(self, code):
        c = self.cpu
        c.b = code
        c.cc |= C

    # ----------------------------------------------------------- run
    def run(self):
        c = self.cpu
        m = c.mem
        try:
            while True:
                pc = c.pc
                if m[pc] == 0x10 and m[pc + 1] == 0x3F:
                    code = m[pc + 2]
                    c.pc = (pc + 3) & 0xFFFF
                    c.cc &= ~C & 0xFF
                    try:
                        self.syscall(code)
                    except OS9Error as e:
                        if self.trace:
                            print(f"    -> error {e.code}", file=sys.stderr)
                        self.fail(e.code)
                    continue
                if pc in self.breaks:
                    self.backtrace(self.breaks[pc])
                c.step()
                self.instr += 1
                if c.s < self.minsp:
                    self.minsp, self.minpc = c.s, c.pc
                    self.minstack = bytes(m[c.s:self.stacktop])
        except Exit as e:
            return e.status
        except Halt as e:
            raise SystemExit(f"CPU halt at {c.pc:04X}: {e}")

    def syscall(self, code):
        c = self.cpu
        m = c.mem
        name = SYSNAMES.get(code, f"svc {code:02X}")
        if self.trace:
            print(f"[{self.pid}] {name} A={c.a:02X} B={c.b:02X} X={c.x:04X} Y={c.y:04X} "
                  f"U={c.u:04X}", file=sys.stderr)
        if code == 0x06:                                   # F$Exit
            raise Exit(c.b)
        elif code == 0x07:                                 # F$Mem
            want = c.d
            if want:
                want = (want + 0xFF) & ~0xFF
                limit = min([self.modbase] + [sl * 0x2000 for sl in self.mapped])
                if want > limit:
                    raise OS9Error(E_MEMFUL)
                if want > self.memtop:
                    m[self.memtop:want] = bytes(want - self.memtop)
                self.memtop = want
                self.highwater = max(self.highwater, want)
            c.d = self.memtop
            c.y = self.memtop
        elif code == 0x09:                                 # F$Icpt
            pass
        elif code == 0x0A:                                 # F$Sleep
            c.x = 0
        elif code == 0x0C:                                 # F$ID
            c.a = self.pid
            c.y = 0
        elif code == 0x0D:                                 # F$SPrior
            pass
        elif code == 0x15:                                 # F$Time
            t = datetime.datetime.now()
            m[c.x:c.x + 6] = bytes([t.year - 1900, t.month, t.day, t.hour, t.minute, t.second])
        elif code == 0x17:                                 # F$CRC
            acc = list(m[c.u:c.u + 3])
            for b in m[c.x:c.x + c.y]:
                acc = crc_byte(acc, b)
            m[c.u:c.u + 3] = bytes(acc)
        elif code == 0x39:                                 # F$AllRAM
            first = NEXTBLK[0]
            NEXTBLK[0] += c.b
            for n in range(first, first + c.b):
                RAM[n] = [bytearray(0x2000), self.pid]
            c.d = first
        elif code == 0x4F:                                 # F$MapBlk (1 block)
            if c.b != 1 or c.x not in RAM:
                raise OS9Error(241)                        # E$IBA
            free = [sl for sl in range(8) if sl * 0x2000 >= self.memtop
                    and sl * 0x2000 < self.modbase and sl not in self.mapped]
            if not free:
                raise OS9Error(E_MEMFUL)
            sl = max(free)                                 # F$FreeHB: the highest
            m[sl * 0x2000:(sl + 1) * 0x2000] = RAM[c.x][0]
            self.mapped[sl] = c.x
            self.mapblks += 1
            c.u = sl * 0x2000
        elif code == 0x50:                                 # F$ClrBlk (1 block)
            sl = c.u // 0x2000
            if c.b != 1 or sl not in self.mapped:
                raise OS9Error(241)
            RAM[self.mapped.pop(sl)][0][:] = m[sl * 0x2000:(sl + 1) * 0x2000]
        elif code == 0x51:                                 # F$DelRAM
            for n in range(c.x, c.x + c.b):
                if n not in RAM or n in self.mapped.values():
                    raise OS9Error(241)
                del RAM[n]
        elif code == 0x00:                                 # F$Link
            raise OS9Error(E_MNF)
        elif code == 0x03:                                 # F$Fork
            self.fork()
        elif code == 0x04:                                 # F$Wait
            if not self.children:
                raise OS9Error(226)                        # E$NoChld
            pid, status = self.children.pop(0)
            c.a = pid
            c.b = status
            if status:
                c.cc |= C
        elif code in (0x83, 0x84, 0x85):                   # I$Create, I$Open, I$MakDir
            self.open(code)
        elif code == 0x86:                                 # I$ChgDir
            name, end = self.name_at(c.x)
            h = self.disk.host(name)
            if not os.path.isdir(h):
                raise OS9Error(E_PNNF)
            if c.a & 0x04:                                 # execution directory
                self.disk.cmds = h
            else:
                self.disk.cwd = h
            c.x = end
        elif code == 0x87:                                 # I$Delete
            name, end = self.name_at(c.x)
            h = self.disk.host(name)
            if not os.path.isfile(h):
                raise OS9Error(E_PNNF)
            os.remove(h)
            c.x = end
        elif code == 0x82:                                 # I$Dup
            f = self.path(c.a)
            if isinstance(f, HostFile):
                f.refs += 1
            c.a = self.newpath(f)
        elif code == 0x8F:                                 # I$Close
            f = self.path(c.a)
            f.close()
            self.paths[c.a] = None
        elif code in (0x89, 0x8B):                         # I$Read, I$ReadLn
            f = self.path(c.a)
            data = f.read(c.y) if code == 0x89 else f.readline(c.y)
            if not data and c.y:
                c.y = 0
                raise OS9Error(E_EOF)
            m[c.x:c.x + len(data)] = data
            c.y = len(data)
        elif code in (0x8A, 0x8C):                         # I$Write, I$WritLn
            f = self.path(c.a)
            data = bytes(m[c.x:c.x + c.y])
            if code == 0x8C and b"\r" in data:
                data = data[:data.index(b"\r") + 1]
            f.write(data)
            c.y = len(data)
        elif code == 0x88:                                 # I$Seek
            f = self.path(c.a)
            if not isinstance(f, HostFile):
                return
            f.f.seek(c.x << 16 | c.u)
        elif code == 0x8D:                                 # I$GetStt
            self.getstt()
        elif code == 0x8E:                                 # I$SetStt
            self.setstt()
        elif code == 0x0F:                                 # F$PErr
            print(f"ERROR #{c.b:03d}", file=sys.stderr)
        else:
            raise OS9Error(E_UNKSVC)

    def open(self, code):
        c = self.cpu
        name, end = self.name_at(c.x)
        mode = c.a
        h = self.disk.host(name, exe=bool(mode & 0x04) and code == 0x84)
        if code == 0x85:
            os.makedirs(h, exist_ok=True)
            c.x = end
            return
        if code == 0x83:
            if os.path.exists(h) and not (mode & 0x80):
                pass                                       # RBF: E$CEF unless rewriting
            f = open(h, "w+b")
        else:
            if os.path.isdir(h):
                if not (mode & 0x80):
                    raise OS9Error(E_FNA)
                raise OS9Error(E_FNA)                      # directory reads not modelled
            if not os.path.isfile(h):
                raise OS9Error(E_PNNF)
            f = open(h, "r+b" if mode & 0x02 else "rb")
        c.a = self.newpath(HostFile(f))
        c.x = end

    def getstt(self):
        c = self.cpu
        m = c.mem
        f = self.path(c.a)
        code = c.b
        if code == 0x00:                                   # SS.Opt
            m[c.x:c.x + 32] = bytes(32)
            m[c.x] = 0 if f.scf else 1                     # device type: SCF / RBF
        elif code == 0x01:                                 # SS.Ready
            if not f.scf:
                raise OS9Error(E_UNKSVC)
        elif code in (0x02, 0x05):                         # SS.Size, SS.Pos
            if f.scf:
                raise OS9Error(E_UNKSVC)
            if code == 0x02:
                f.f.flush()         # else fstat misses buffered writes
                n = os.fstat(f.f.fileno()).st_size
            else:
                n = f.f.tell()
            c.x, c.u = n >> 16, n & 0xFFFF
        elif code == 0x0F:                                 # SS.FD
            if f.scf:
                raise OS9Error(E_UNKSVC)
            f.f.flush()
            st = os.fstat(f.f.fileno())
            t = time.localtime(st.st_mtime)
            fd = bytes([0x1B, 0, 0, t.tm_year - 1900, t.tm_mon, t.tm_mday, t.tm_hour,
                        t.tm_min, 1]) + st.st_size.to_bytes(4, "big") + bytes(3)
            n = min(c.y, len(fd))
            m[c.x:c.x + n] = fd[:n]
        elif code == 0x06:                                 # SS.EOF
            if not f.scf:
                f.f.flush()
                pos = f.f.tell()
                if pos >= os.fstat(f.f.fileno()).st_size:
                    raise OS9Error(E_EOF)
        else:
            raise OS9Error(E_UNKSVC)

    def setstt(self):
        c = self.cpu
        f = self.path(c.a)
        code = c.b
        if code == 0x00:                                   # SS.Opt
            pass
        elif code == 0x02:                                 # SS.Size
            if f.scf:
                raise OS9Error(E_UNKSVC)
            f.f.truncate(c.x << 16 | c.u)
        else:
            raise OS9Error(E_UNKSVC)

    def fork(self):
        """Run the child to completion now; F$Wait collects its status."""
        c = self.cpu
        m = c.mem
        name, _ = self.name_at(c.x)
        params = bytes(m[c.u:c.u + c.y])
        if b"\r" in params:
            params = params[:params.index(b"\r")]
        h = self.disk.host(name, exe=True)
        if not os.path.isfile(h):
            raise OS9Error(E_PNNF)
        module = open(h, "rb").read()
        child_paths = list(self.paths[:3])                 # stdin, stdout, stderr
        for f in child_paths:
            if isinstance(f, HostFile):
                f.refs += 1
        extra = max(0, c.b * 256 - (module[11] << 8 | module[12]))
        child = Process(self.disk, module, params, child_paths, extra, self.trace, self)
        # A mapped block is shared RAM, but this model keeps a copy in the
        # slot: write the parent's copies back before the child runs and
        # read them again after (las's sizing trial shares a block, D38)
        for sl, n in self.mapped.items():
            RAM[n][0][:] = m[sl * 0x2000:(sl + 1) * 0x2000]
        status = child.run()
        for sl, n in self.mapped.items():
            if n in RAM:
                m[sl * 0x2000:(sl + 1) * 0x2000] = RAM[n][0]
        for f in child.paths:
            if f is not None:
                f.close()
        self.instr += child.instr
        self.children.append((child.pid, status))
        c.a = child.pid


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("module")
    ap.add_argument("args", nargs=argparse.REMAINDER)
    ap.add_argument("--root", default=".")
    ap.add_argument("--cwd", default="")
    ap.add_argument("--cmds")
    ap.add_argument("--stdin")
    ap.add_argument("--stdout")
    ap.add_argument("--stderr")
    ap.add_argument("--mem", type=int, default=0)
    ap.add_argument("--trace", action="store_true")
    ap.add_argument("--stats", action="store_true")
    ap.add_argument("--map")
    ap.add_argument("--break", dest="breaks", action="append", default=[])
    a = ap.parse_args()

    disk = Disk(a.root, a.cwd, a.cmds)
    paths = [HostFile(open(a.stdin, "rb")) if a.stdin else TermFile(sys.stdin),
             HostFile(open(a.stdout, "wb"), scf=True) if a.stdout else TermFile(sys.stdout),
             HostFile(open(a.stderr, "wb"), scf=True) if a.stderr else TermFile(sys.stderr)]
    module = open(a.module, "rb").read()
    params = " ".join(a.args).encode("latin-1")
    syms = read_map(a.map) if a.map else []
    try:
        p = Process(disk, module, params, paths, a.mem, a.trace, syms=syms, breaks=a.breaks)
    except OS9Error as e:
        raise SystemExit(f"os9run: F$Fork would fail: error {e.code}")
    status = p.run()
    if RAM:
        print(f"os9run: {len(RAM)} RAM blocks never given back (F$DelRAM)", file=sys.stderr)
        status = status or 1
    for f in p.paths:
        if f is not None:
            f.close()
    if a.stats:
        print(f"os9run: exit {status}, {p.instr} instructions, data area "
              f"{p.highwater} bytes (module at ${p.modbase:04X}), "
              f"{NEXTBLK[0] - 0x40} RAM blocks, {p.mapblks} F$MapBlk", file=sys.stderr)
        # CoCoC's cstart: _mtop is the top of ibrk's memory (static data,
        # then ibrk upwards); the stack grows down from the parameters
        mtop = None
        for line in open(a.map, encoding="latin-1") if a.map else ():
            t = line.split()
            if t[:2] == ["_mtop", "udat"]:
                mtop = int(t[2], 16)
        heap = f", ibrk top ${p.cpu.mem[mtop] << 8 | p.cpu.mem[mtop + 1]:04X}" if mtop else ""
        print(f"os9run: stack {p.stacktop - p.minsp} bytes deep (lowest S ${p.minsp:04X}, "
              f"top ${p.stacktop:04X}){heap}", file=sys.stderr)
        if p.minpc is not None and p.syms:
            print(f"os9run: deepest stack in {p.where(p.minpc)} <- "
                  + " <- ".join(p.callers(p.minsp, p.minstack)), file=sys.stderr)
    sys.exit(status)


if __name__ == "__main__":
    main()
