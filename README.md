# fx580vnxfromasmtoc
Compile .asm source code of Casio FX580 VN X (Vietnam Market) to .c source code

## Generate C ROM source

The address-annotated listing contains the complete ROM image. Generate a C
source file that embeds those bytes and exposes `fx580vnx_load_rom()` for the
nX-U8/100 emulator:

```sh
python3 tools/asm2c.py
gcc -std=c11 -Wall -Wextra -Iemu -c build/rom.c -o build/rom.o
```

The generator verifies that the listing has no conflicting or missing words.
This preserves the original machine code in a buildable C representation; it
does not translate the firmware into high-level C logic.

## Translate the firmware to C

Hand-written C reimplementations live in `src/`, one module per subsystem. Every
routine keeps the ROM address it replaces in a comment, uses the Casio calling
convention (`uint16_t f(uint8_t *ds, ...)`, arguments in `er0`/`er2`/`er4`,
result in `er0`) and works on the flat 64 KiB data space (`DSR` is always 0 on
this firmware).

Because the CPU core in `emu/nx8.c` is accurate enough to run the real ROM,
every C routine is checked against the original: the tests load a data space,
run the ROM routine, run the C routine on an identical copy, and compare the
returned `er0` plus **every byte a store can latch** - RAM, the SFRs at
`0xF000` and the LCD framebuffer at `0xF800` - skipping only the stack window at
`0xEC00-0xEFFF` that the harness itself fills with arguments.  Leaving the SFRs
and the framebuffer out used to let a translation drop a hardware poke, or
render nothing at all, and still pass.

Three things keep the comparison honest:

* The data space is filled from a table of boundary bytes (`0x00`, `0x09`,
  `0x0A`, `0x7F`, `0x80`, `0xF0`, `0xFF`, ...) rather than uniformly: the values
  the firmware branches on are then common instead of one-in-256.
* Arguments are drawn from per-kind boundary tables first, and two-argument
  routines are swept over the **whole cross product** of theirs (sixty-four
  pairs, not eight) - a single shared draw is how the box and line routines
  stayed green while being wrong for every line wider than 127 columns.  A lone
  byte argument is simply tried for all 256 values.
* If the ROM itself never returns for some argument the case is counted and
  reported, never silently skipped, and is not compared against a shredded
  image.

`tools/mutation.sh` plants twenty deliberate bugs - the ones this project has
actually made plus one per row of the instruction-set trap table in
`docs/progress.md` - and fails unless the suite catches each of them, so
"0 failures" cannot quietly come from weak checks.  A mutation that *survives*
is treated as a hole in the harness rather than a win: the `memmove_nn` signed
direction test is one that survived, which is why `tests/t_mem.c` now carries
framebuffer cases where `src + n` wraps past 0xFFFF.

`tools/audit.py` closes the other hole: a routine counts as translated because
its address is annotated in `src/`, which says nothing about whether anything
tests it.  The audit cross-checks the annotation set against the tests and
fails when an annotated routine is neither diff-tested directly nor reached
from a routine that is.  It also enforces the annotation convention it relies
on: only addresses on the opening line of a comment block count, so a prose
mention ("the scanner at 0x0A568 rejects ...") cannot inflate the percentage.

```sh
bash tools/test.sh          # audit, differential tests, cross-check
bash tools/test.sh --quick  # audit + differential tests only
bash tools/test.sh --no-xcheck  # same, minus the cross-check, on purpose
./tools/mutation.sh         # prove the suite still catches planted bugs
python3 tools/audit.py -v   # which routines are tested directly
```

The cross-check is the only check that validates `emu/nx8.c` itself, and
`emu/nx8.c` is the oracle every differential test compares against.  Skipping it
silently would make "0 failures" mean "this project agrees with itself", so a
missing reference emulator now **fails** the run; `--no-xcheck` skips it only
when asked, and says so in what it prints.

`machine/` is a submodule pinned at CasioEmu commit `52938b7`, the HEAD of
<https://github.com/user202729/CasioEmu> (the maintained fork of
LBPHacker's original).  To get the cross-check running:

```sh
git submodule update --init machine
cd machine/emulator && meson setup build && ninja -C build && cd ../..
```

That needs `meson`, `ninja`, Lua 5.3 and SDL2/SDL2_image.  The model folder the
emulator loads is built on the fly under `build/models/fx580vnx_emu`: it is a
copy of `machine/models/fx580vnx_emu` with `rom.bin` symlinked to
`resource/rom.bin`, because a ROM must not be committed into the submodule.

### Watching the firmware draw

`tools/screen_demo.lua` runs four drawing calls in CasioEmu and dumps the 1536-byte
screen buffer; `tools/screen_demo.c` runs the same four through the C translations,
so the two pictures can be compared byte for byte:

```sh
gcc -std=c11 -O2 -o build/screen_demo tools/screen_demo.c src/*.c emu/nx8.c -I src
./build/screen_demo > build/fb_c.txt
( cd machine && SDL_VIDEODRIVER=dummy emulator/build/emulator \
    model=../build/models/fx580vnx_emu script=../tools/screen_demo.lua \
    | grep -E '^[0-9A-F]{2} ' > ../build/fb_rom.txt )
cmp build/fb_c.txt build/fb_rom.txt
```

The C side has to load `resource/rom.bin` into `0x0000..0xCFFF` first: below
`0xD000` the data space *is* the ROM window and the font bitmaps live there, so a
zeroed buffer draws no glyphs at all.

### Adding a routine

```sh
python3 tools/frontier.py -n 40      # routines whose callees are already done
python3 tools/fdump.py 0x9CC4        # annotated body, labels resolved
```

Write the C body, then add one manifest line to the matching `tests/t_*.c`:

```c
BATCH({"name", 0x09CC4, "ww", a_name, 0})
```

The shape string describes the arguments: `b` byte, `w` word, `p` RAM pointer,
`s` small count, `z` zero, `|` starts the stack arguments, `!` anywhere means
"do not compare the return value". The runner takes care of the rest; the suite
must stay at `0 failures`.

### Progress

`python3 tools/coverage.py` reports how much of the firmware has a verified C
body (currently 31.3% of routines, 14.6% of instructions). See
[`docs/progress.md`](docs/progress.md) for the details and the plan for the
remaining work.
