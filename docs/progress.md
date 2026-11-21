# Translation progress

How much of the fx-580VN X firmware has a C body that is verified against the
ROM, and what is left.

## Measuring

```sh
python3 tools/coverage.py        # totals
python3 tools/coverage.py -v     # plus the list of translated routines
python3 tools/frontier.py        # routines that can be translated next
python3 tools/frontier.py -b     # routines that are blocked, and by whom
```

`frontier.py` used to be optimistic by 71 routines, and the reason is worth
keeping in mind: it built its "what does this call" set from `bl` targets that
happen to be *function entries*.  A `b` tail call (`0x0EAC6` is just
`b f_0EF00`) is not an entry at all, and a call into a shared tail
(`bl f_13AB4.l_00E`) lands on an address nobody calls directly, so both were
dropped and the caller was advertised as ready while its target had no C body.
`romdb.dependencies()` now resolves every `b`/`bl` target to the *owning
function* (`romdb.SpanOwners`) and follows `b` as well; the honest frontier is
146 ready instead of 221.  `-b` prints the inverse list, which is what decides
whether the next routine is a three-instruction leaf or a 300-instruction
monster: `0x22400` is 11 instructions and unblocks four routines, `0x1C2CA` is
311 instructions and unblocks two.

Entries that are not code at all live in `romdb.NOT_A_FUNCTION`.  The two at
`0x0A1CE`/`0x0A1D0` are the one that finally proved it: `0x0A1D0` is a
`bc al` branch to itself, which is precisely what `emu/nx8.c` uses as
`NX8_SENTINEL` (the return address a call can never reach).  Counting them in
the denominator would cap coverage below 100% for ever.

`tools/coverage.py` counts a routine as translated when its entry address
appears in an address annotation *directly above a declaration* in `src/*.c` /
`src/*.h` (`/* 0xNNNNN - ... */`, the convention every C function in this
project uses).  The adjacency matters: merely mentioning an address elsewhere -
a TODO note, a prose range such as `0x0AC30-0x0C000`, or a data argument like
`subtract_words(rom, 0x20000, ...)` - does not count, so the percentage cannot
be inflated without writing a function.  Both tools share this rule through
`tools/romdb.py`.

A routine here is a *function*, not a chunk: `f_XXXXX` entries own the
`.l_NNN` blocks that follow them, and only the instructions control can reach
inside that span are counted.  Counting chunks instead (the older rule, which
stopped at the first `b`) hid callees and under-counted bodies - 0x0C362 looked
like a 17-instruction leaf until the walk followed its `bc ne` past the
`b LEAVE` into the `.l_028` block that calls 0x24F26.

Bodies and sizes come from `build/disas.json`, so the two numbers are measured
on the same partition:

| metric | current |
| --- | --- |
| routines | 309 / 761 (40.6%) |
| instructions | 6075 / 32749 (18.6%) |

(Measured with `python3 tools/coverage.py`; the instruction total counts the
ret/rt-delimited bodies the tooling treats as one routine each, not every word
in the ROM.  `python3 tools/audit.py` is what keeps the first number honest: it
fails unless every annotated routine is also run against the ROM by a test, and
it counts an annotation only when the addresses open the comment block - a
prose mention inside a neighbouring block used to claim `0x0A568`, whose body
was never written, and inflated the count by one routine and 26 instructions.)

## Instruction-set traps

Properties of the nX-U8/100 that cost a full debugging cycle each while
translating `0x1D950`/`0x1DCCE`/`0x2E2BC`/`0x08E62`.  They are checked against
the core manual (`resource/nX-U8100 Core Instruction Manual.txt`) or against
`emu/nx8.c`, which is the ground truth the diff-test compares to, not guessed:

| trap | rule | where the manual says so |
| --- | --- | --- |
| `BC GE` / `BC LT` / `BC GT` | **unsigned** - `GE` is `C=0`, `LT` is `C=1`, `GT` is `(C=0)&&(Z=0)`; the signed forms are only `BC GES` / `BC LTS`.  So `cmp r0, 0 ; bc gt` is "nonzero", not "positive", and 0x80..0xFF takes the branch (`0x2EB66`) | §3.3, `Bcond` condition table |
| multi-byte `l`/`st` | the effective address is forced even: address bit 0 is ignored, so an odd pointer rounds *down* | §1.3.5.3 "Word Boundaries" |
| register groups | `erN` = 2 bytes (`rN..rN+1`), `xrN` = 4 (`rN..rN+3`), `qrN` = 8 (`rN..rN+7`) - the number is the *first* register, so `push qr8` saves r8..r15 and `push xr0` saves **r0..r3** | `tools/nx8.py`, instruction formats |
| `push xr0` / `pop xr0` as a save | restoring r0..r3 throws away the callee's `er0` answer. `0x17B16` (`mov er0,0` / `bl f_0AC60` / `mov er14,er0` / `pop xr0`) therefore answers *in er14 only* and returns the caller's er0 untouched | `0x17B16` diff-test, three wrong translations before the register image showed er14 right and er0 zero |
| which high half a predicate returns | the `mov r0, <flag>` idiom writes **only r0**, so everything that already held r1 survives: for a routine that starts with `l er0, [cell]` that is the cell's own high byte (`0x0BEFA`), and for one entered with a pointer in er0 it is the pointer's (`0x2F6FC`). Reading "returns the caller's r1" as a general rule fails 42 checks |
| `mov erN, erN` | not a no-op: the ROM uses it as a **sixteen-bit zero test** to set Z. `0x0AF1A` looks like "is er0 = 35" from the `cmp r0, 35 / cmpc r1, 0` prologue, but its failing branch jumps into `mov er0, er0 / bc eq` and answers 1 for er0 = 0 as well - a full sixteen-bit sweep of the entry is the only way to see the second value |
| `SRLC Rn` vs `SLLC Rn` neighbour | `SLLC` takes its carry in from `r(N-1)`, `SRLC` from `r(N+1)` - opposite directions. So `0x1BB4E`'s `sllc r7,4 … sll r0,4` walk r0..r7 as one big-endian 64-bit value shifted left four, and `0x1BB28`'s `srlc r0,4 … srl r7,4` the same right (same for the nine-byte r1..r9 pair, `0x1BB3A`/`0x1BB60`). Consequence: a run of equal bytes is a poor test vector, because right-shifting it twice maps back onto itself |
| return value of a register-only routine | `rt` writes nothing, so er0 comes back holding whatever the body left in r0/r1 - for the shift runs that is the *shifted* pair, not the caller's. Returning 0 "because it writes no result" is wrong |
| `SLLC` / `SRLC` / `SRA` / `SRL` on a byte register | they are **not** eight-bit shifts: `SLLC Rn`/`SRLC Rn` shift the 16 bits of Rn *and the register below it* (`Rn-1`, wrapping to r15 for r0) and store the upper eight bits back in Rn, so `sllc r11, 4` moves `er10` and the low half is thrown away | Core manual §3, `SLLC Rn,obj`: "shifts the 16 bits in the specified byte-sized register and the register below it ... and stores the upper eight bits in the specified register" |
| immediates | printed in **decimal** in `resource/disas.asm`: `add r2, 22` is +0x16 | `tools/disasm.py` |
| `[ea+]` on a multi-byte access | the bump happens first and the result is rounded down to even *again*, so an odd `ea` stays usable | `load_store()`, `bump_ea()` in `emu/nx8.c` |
| multi-byte `st` | the bytes go out in **descending** address order (`ix = length-1 .. 0`) | `load_store()` in `emu/nx8.c` |
| record byte order | the numeric record at `0xD000` and the display buffer are in different orders. `0x1C674` writes `[9,8,7,6,5,4,3,2,0,1]` and `0x1C63E` reads `[8,9,7,6,5,4,3,2,1,0]` - words 1..4 reversed and byte-swapped, word 0 untouched. Copying straight through passes a casual look and fails the moment the records are compared | `0x1C63E`/`0x1C674`: both directions are each other's inverse, so the tests check them against each other |
| the `ENTER` frame | `push lr` pushes **lcsr and lr** (four bytes), so an `ENTER` frame is 16 bytes and the stacked arguments of a routine are at `16/18/20[er14]`, not `14/16/18` | `H_PUSHL` in `emu/nx8.c` (`mask & 8` pushes both) |
| 16-bit shifts | written as an `sllc`/`sll` or `srlc`/`srl` pair on two adjacent bytes; the high byte first for a left shift and the low byte first for a right one, and the carry chains the halves | `op_sllc` / `op_srlc` in `emu/nx8.c`, confirmed by tracing `0x09834` |
| subtracting a constant | `add rX, 240..255` then `bc lt` is a subtract that branches on the borrow: the branch is taken when the *carry out* is set, i.e. when the old value was ≥ the complement | traced at `0x098D6` in `0x09834` |
| `BC AL` | condition nibble 14 really is unconditional; a backward `bc al` is a plain loop jump and the exit test sits at the loop head | §3.3 condition table (1000..1110 = NE, EQ, NV, OV, PS, NS, AL) |
| `bl` and the stack | `bl` pushes **nothing** - it sets the link registers `lr`/`lcsr` - while `pop pc` pops **four** bytes (pc and csr).  So `push <word> ; push <word> ; bl f_1FFFE` (a callee whose body is a bare `pop pc`) does not return to its caller at all: the two pushed words *become* the new pc and csr and control goes to the address the caller stacked.  `0x2FFC6` is that shape, and it can never be diff-tested | `emu/nx8.c` (`H_PUSHL` covers `push lr` only; `nx8_call` sets `lr`/`lcsr` for `bl`) |
| a status byte reused as the answer's high half | `mov r1, r0 ; and r1, 240 ; cmp r1, 64` destroys the caller's r1 on the way to the compare, so the routine it ends in answers `(status & 0xF0) << 8` and not `arg & 0xFF00` (`0x1DA62`); its neighbour `0x1D9AC` keeps the caller's high byte on the first two exits and answers the record's own byte on the last - four exits, three different high halves | both routines' diff-tests; the trap is invisible to a pointer sweep, so the status byte is swept over all 256 values |
| the glyph buffer past the font's own rows | `0x0904C` keeps its glyph bitmap in its **own stack frame**, so with a font size other than 10 or 14 the rows past the extractor's fill are whatever the data space held at that address - filling `0xEC00..0xECFF` with 0xAA makes the ROM draw 0xAA rows.  The C clears the bitmap and reads zero past its end, which is deterministic but cannot match the ROM there: the ROM's answer depends on the stack pointer, not on the data space alone.  Only 10 and 14 are in domain (`0x099E4` and `0x2086C` are the two writers of `0xD137`) and they fill exactly as many rows as `height` asks for, so the divergence is unreachable from the firmware | `tools/screen_demo.lua` (font size 0) versus `tools/screen_demo.c`; the C side now pins the font at 14 |
| a save window that pops back | `0x1C952` pushes `qr0`, `qr0`, `xr0` - 8 + 8 + 4 bytes from `0xD03C`, `0xD044`, `0xD04C`, because `[ea+]` bumps by the *group* size - and pops all twenty bytes back, so everything the body writes in `0xD03C..0xD04F` is invisible: replayed pop by pop, the three chunks land one slot off and six bytes of the record look wrong.  Snapshot the window instead and model only what the body reads out of it |
| a `bl` into a `.l_NNN` is not a call to the routine it belongs to | `bl 0x0A0D4` enters `0x0A0AA` at its *last two* instructions, `st r0, [0xF032]; rt` - the `mov r0, 20` that the whole routine uses sits one instruction before the label.  Writing 20 to the contrast there is wrong by a whole byte: the store takes whatever r0 the caller happened to hold (`0x09A6A` loads 0xD153 two instructions earlier) |
| a record slid through a staging window | `0x1BCDE`/`0x1BD68`/`0x1BE00` stage the ten-byte record at `0xD03C` behind one zero, then reload from `0xD03C + count/2` (the divide) or `0xD045 - count/2` (the multiply) and shift four more bits when the count is odd.  The index that falls off the front reads a zeroed byte and the one that runs past the end reads the zero the staging put there, so the answer is `record[k + half]` / `record[k - half]` with zeros outside 1..9 - and the multiply also clears the record's top byte while the divide keeps it |
| `sub` + `das` as a five test | `0x1CA30` subtracts five from the low half of `0xD001`, masks the byte again and repeats until the subtraction stops borrowing, then branches on 0, 2 or less.  `DAS` adjusts **both** nibbles (`0x0A` becomes `0x04`), so the answer is not `k % 5` for `k >= 10`: 0..4 loop once and come back as themselves, 5..14 give `k - 5`, 15 gives 4 |
| a stacked argument that is a **pointer**, not a value | `0x09846` shares its rectangle fill with the constant-pattern pair `0x09826`/`0x09834`, but its stacked word is an address: the body reads `l r0, [er8]` **inside** the per-byte loop and steps er8 by one after each written column byte, so the fill consumes a byte stream rather than repeating a pattern.  A null pointer takes the stub at `0x09854` and draws nothing at all - not a clear, which is what `0x09826` is for.  Reading the stacked word as the pattern matches the pair that pass a constant value and diverges from the very first byte of a dotted fill (the firmware's own call from `0x09EE8` reads the ROM table at `0x646E`) | `0x09846` diff-test, swept over five geometries x two buffers x pointers into RAM, into ROM and null |
| a routine that never returns | `0x09EE8` ends in `b _start` (`0x0A1CE`), so the harness cannot wait for a `rt`: `run_rom_until(addr, args, name, stop)` steps the core until pc/csr reach the stop address and copies the image *there*, which is what a body that hands control to the main loop has to be compared against.  Its callees also read stack slots they never wrote - the `2[sp]` fallback inside `0x09846` and the holes the frames leave - so a stack full of random bytes sends the ROM through a `pop pc` that was never pushed (the line reads `code fetch from unmapped segment` at an address that is really data): pin `0xEC00..0xEFFF` to zero, exactly as at boot, and keep the rest of the data space random.  A routine whose *own* exit is `pop pc` (the `0x09F68` tail) needs the opposite: `{NX8_SENTINEL, 0}` in the stacked words, because `pop pc` pops four bytes | `0x09EE8` (eight random data spaces, plus the `0xD0F5` guard through the plain runner) and `0x09F68` (`bc ne`/`bc eq` on `0xF042`, four filter bytes) |
| the answer's high half | `0x08FEE` writes only r0, so `line_print`'s `count` comes back in r0 and whatever r1 held at the end in r1: one more than `char_print`'s row count (0x0E for the 14-pixel font, 0x0A for the 10-pixel one, one less for the wide markers 241/242), the arguments untouched when `char_print` was off screen (`x > 191`, or `y >= 64`), the buffer end 0xE3D3/0xE9D3 when the row loop ran off the bottom, and the caller's y when nothing was drawn at all.  The control character 1 inherits the half from its **own** `char_print`, which draws two pixels to the left - so at x = 0 or 1 it is off screen and r1 stays at y | `char_print`, `char_print_1byte`, `line_print` and `line_print.col_0` diff-tests, all four compared as full words since this row was written |

| `mov r1, 252` before a byte answer | the *whole* high half of er0 becomes 0xFC, not the caller's r1: 0x1FFE6 and 0x2B986 answer `0xFC00 | hit` even when the argument's high byte is random | both routines' diff-tests, swept over all 0xFCxx boundary words |
| a byte load into r1 becomes the answer's high half | `l r1,[ptr]` writes r1, so a later `mov r0,1/0` leaves the loaded byte upstairs: 0x15E36 answers `(ds[ptr] << 8) | hit`, 0x09CC4 answers `(ds[0xD121] << 8) | r0` on its general path | both diff-tests |
| `l r0, 3:[er14]` at 0x10E56 | the disassembler's addressing-mode reading is misleading: the routine answers the literal 3 in the low half for every data space, including one with RAM and framebuffer pre-filled with distinct markers | `table_byte_ffdf` diff-test, shape `"w"` |
| a byte write that destroys the argument's high half | at 0x0D80C `mov r1,0` runs *before* `mov er6,er0`, so the loop bound is `arg & 0xFF` zero-extended, not the whole word; the loop always stops within six iterations | `triangular_from_arg` diff-test over all boundary words, plus a step trace of 0x0D80C |
| `DAA`'s extra 0x90 case and carry | `op_daa` adjusts by 0x60 when the high nibble is exactly 9 and the low nibble > 9 even with no half-carry, and ends with `flags_out |= backup & PSW_C`, so a carry from the adjust (or the original `ADDC`) feeds the next byte.  `DAS` preserves its carry the same way.  The generic `bcd_step` models neither, so 0x1C5E6/0x1C612 use dedicated `addc_daa`/`subc_das` steps copied from the core | both bank add/sub diff-tests, 8-word cross product |

Consequence worth stating once: a block copy must not read and write one byte at
a time.  The ROM loads ten bytes into `er6`+`qr8`, stores them, and only then
loads the next ten, so an overlapping source/destination sees the first store -
the interleaved form is wrong for every overlapping pair.  Store *order* within
one group, on the other hand, is unobservable for a byte copy, because every
byte is written exactly once.

Two more that only show up in return values:

* `mov r0, x` writes the low byte only, so `er0` keeps the caller's `r1` in its
  high half.  That is why `0x2805E` returns 0x0707/0x0505 and `0x2AB3C` returns
  `arg & 0xFF00` rather than 0.
* `DIV ERn, Rm` puts the quotient in `ERn` and the remainder in `Rm`, and its Z
  flag reports the *dividend*.  `MUL ERn, Rm` multiplies the low bytes only.

The arithmetic core at 0x27000-0x31000 contains no `daa`/`das` at all - its
digit work is byte-decrement loops and `addc`/`cmpc` carry chains - so plain C
loops model it; the decimal-adjust instructions live in other regions.

## How a routine gets translated

1. `python3 tools/frontier.py` lists routines whose *dependencies* are already
   done - only those can be diff-tested immediately.  A dependency is not just
   a `bl`: a `b` tail call and a `bl` into a `.l_NNN` shared tail both count, and
   both resolve to the function that owns the target address.  `ENTER`/`LEAVE`
   count as done for this purpose even though they have no C body: the frame
   they build is modelled by the calling convention itself, and `tools/romdb.py`
   lists them in `STRUCTURAL_ENTRIES`.
2. `python3 tools/fdump.py 0xNNNNN` prints the annotated body; every branch
   target, call and data label is resolved.  It stops at the first `rt`/`pop
   pc`, which is not always the end of the function: a branch can jump forward
   into a later block of the same span (0x0C784's first listing ended at
   0x0C7A2 and hid a second threshold at 0x0C7A4).  Dump any branch target
   beyond the printed body before writing the C translation.
3. The C body goes into the module for its subsystem, with the ROM address in
   the comment and a note for every place where the original is surprising
   (8-bit wrap-around, flags left in `er0`, bytes burned down on the stack...).
4. A manifest line in `tests/t_*.c` (`{"name", 0xNNNNN, "shape", adapter, 0}`)
   makes `tests/batch.c` run the ROM routine and the C routine on several random
   data spaces and compare the result word plus all of RAM.
5. `bash tools/test.sh` must stay at `0 failures`.

Some routines do not fit the shape language at all, and then the case is
written by hand:

* the arguments are in registers the shape cannot name (`er12`, `er14`, the
  `r12`/`r13` pair, or `sp` - `Args.sp` places the stack anywhere);
* the answer comes back in a register that is not `er0`, in which case
  `check_regs()` compares the 16-byte register file the ROM left behind against
  an image the test builds (`0x143D6` answers in `r5` and `r2`, `0x16070` in
  `er0` and `er2`);
* the routine writes into the stack window `0xEC00-0xF000` that
  `check_mem_all()` skips, so the range is compared explicitly (`0x23F8A`);
* the routine returns through `pop pc`, which pops four bytes (pc and csr), so
  the harness has to stack a return address and a zero csr (`0x1FFFE`).
* an entry above `0xFFFF` does not fit a `uint16_t` field in a test table:
  `{0x1BCDE, fn}` truncates to `0xBCDE` and the ROM runs into the middle of
  an unrelated routine, which the harness reports as `unrecognized
  instruction` at a plausible-looking `pc` rather than as a bad address
  (`0x1BCDE`'s first test run).

## Where the remaining work is

The call graph is the schedule. `tools/frontier.py` currently reports 113
routines that are ready (every dependency already translated), and the count
grows as routines land, so the work does not run out.  `-a` lists them by size;
the cheapest tier is still ~60 routines of fourteen instructions or fewer.

The cheap tier is now empty: nothing in the ready list is eight instructions or
fewer, and the two remaining two-instruction leaves are *blocked*, not hidden -
`0x1571E` is `bc al, f_1568A.l_02E` into the tail of `0x1568A`, and
`0x23E40` is `mov r2, 170` in front of the shared body of `0x23DE4`.  Neither
owner has a C body yet, so neither wrapper can be diff-tested; translate the
owner and the wrapper falls out for free (`0x23C94` came out this way, as a
`timer_delay` call, once `0x09F3A` was in).  That is why no separate
"label entry" convention was needed: the wrappers that turned out to be
*ordinary* entries (`0x0904C`, `0x09D16`, `0x13B96`, `0x14FC6`, `0x17B16`,
`0x17E52`, `0x23C94`) are `bl` targets with their own address, and they count
like any other routine.

Rough shape of what is left, by ROM region:

| region | contents |
| --- | --- |
| `0x08C00-0x09500` | screen buffers, brushes, brush/blend plot primitives |
| `0x09700-0x0A600` | character/cell grid, editor cursor, key handling glue |
| `0x0AC00-0x13000` | editor command dispatch and text buffer maintenance |
| `0x13000-0x1D000` | mode-specific input (`INPUT`/`EQN`/`MATRIX`/`VECTOR`) |
| `0x1D000-0x22000` | number formatting, converter, table of constants |
| `0x22000-0x27000` | diagnostics, serial/ID, contrast, device test screens |
| `0x27000-0x31000` | the arithmetic core: packed BCD float add/sub/mul/div and
  the transcendental kernels |

The arithmetic core is the largest single block and the most intricate: it is
~160 routines of packed-BCD digit loops that read and write the ten-byte record
at `0xD000`/`0xD010` (see `compare_bcd_records` and `decimal_record_predicate`
in `src/mem.c` for the flavour). Translating it is what will move the
instruction percentage fastest, because those routines are big.

## A note on the 30% target

30% of the instruction total is roughly 9800 instructions; at the
current 15.3% the project is ~5000 instructions in. Reaching 30% therefore means
translating on the order of 300 more routines, and the ones that are left are
bigger and more entangled than the ones done so far (the easy leaf
functions and hardware pokes are largely gone). Work in batches with
`tools/frontier.py` between them and the number stays honest.
