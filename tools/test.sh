#!/bin/sh
# Run the decompilation regression tests.
#
#   ./tools/test.sh              - differential tests, audit, CasioEmu cross-check
#   ./tools/test.sh --quick      - differential tests + audit only (dev loop)
#   ./tools/test.sh --no-xcheck  - everything except the cross-check, on purpose
#   ./tools/test.sh --strict     - treat a missing emulator as fatal (the default)
#
# Why the cross-check is not optional-by-accident: tests/xcheck.c and
# tools/xcheck.lua run the same calls through emu/nx8.c and through CasioEmu.
# That is the only check that validates the C core itself - and the C core is
# the oracle every differential test compares the translations against.  Skip
# it silently and "0 failures" means this project agrees with itself.  So a
# missing emulator fails the run; --no-xcheck has to be asked for out loud, and
# it says so in the summary it prints.
set -u
cd "$(dirname "$0")/.."

CC=${CC:-gcc}
CFLAGS="-std=c11 -Wall -Wextra -O2"

quick=0
xcheck=1
for arg in "$@"; do
    case $arg in
    --quick)     quick=1; xcheck=0 ;;
    --no-xcheck) xcheck=0 ;;
    --strict)    xcheck=1 ;;
    -h|--help)
        sed -n '2,8p' "$0" | sed 's/^# \{0,1\}//'
        exit 0 ;;
    *)
        echo "unknown argument: $arg (see --help)" >&2
        exit 2 ;;
    esac
done

mkdir -p build

echo "== differential tests =="
$CC $CFLAGS -o build/diff_all \
    tests/main.c tests/harness.c tests/batch.c \
    tests/t_string.c tests/t_mem.c tests/t_display.c tests/t_font.c tests/t_print.c tests/t_keyboard.c tests/t_edit.c tests/t_diagnostic.c tests/t_hw.c tests/t_draw.c tests/t_record.c tests/t_arith.c tests/t_emu.c \
    src/string.c src/mem.c src/display.c src/font.c src/print.c src/keyboard.c src/edit.c src/diagnostic.c src/hw.c src/draw.c src/arith.c \
    emu/nx8.c
./build/diff_all resource/rom.bin

echo "== coverage audit =="
python3 tools/audit.py || exit 1

# The C-side trace is built and run before the cross-check decision, so the
# comparison below can never accidentally diff two stale files: xcheck_mine.txt
# is always the output of this run.
$CC $CFLAGS -o build/xcheck tests/xcheck.c emu/nx8.c
./build/xcheck resource/rom.bin > build/xcheck_mine.txt
mine=$(wc -l < build/xcheck_mine.txt)
if [ "$mine" -lt 10 ]; then
    echo "xcheck: FAILED - only $mine cases produced by tests/xcheck.c" >&2
    exit 1
fi

if [ "$xcheck" -eq 0 ]; then
    if [ "$quick" -eq 1 ]; then
        echo "cross-check: not run (--quick)"
    else
        echo "cross-check: NOT RUN (--no-xcheck) - emu/nx8.c is unvalidated"
        echo "             against a reference here; the remaining checks compare"
        echo "             this project with itself."
    fi
    exit 0
fi

echo "== CasioEmu cross-check =="
if [ ! -x machine/emulator/build/emulator ]; then
    echo "cross-check: UNAVAILABLE - machine/emulator/build/emulator is missing" >&2
    if [ -e machine/.git ] || git -C . ls-files -s machine 2>/dev/null | grep -q '^160000'; then
        echo "  machine/ is a submodule that is not checked out (and .gitmodules has" >&2
        echo "  no mapping for it, so 'git submodule update --init machine' cannot" >&2
        echo "  find a URL - restore the mapping first)." >&2
    fi
    echo "  Without the reference emulator there is no independent oracle for" >&2
    echo "  emu/nx8.c, which every differential test compares against." >&2
    echo "  Fix the submodule and build machine/emulator, or rerun with" >&2
    echo "  --no-xcheck to accept the gap explicitly." >&2
    exit 1
fi

# The emulator's model folder expects the ROM next to its model.lua, and the
# ROM cannot live in the submodule (it is not ours to commit), so the run uses a
# copy under build/ with a symlink back to resource/rom.bin.
if [ ! -f build/models/fx580vnx_emu/model.lua ]; then
    mkdir -p build/models/fx580vnx_emu
    cp machine/models/fx580vnx_emu/model.lua \
       machine/models/fx580vnx_emu/interface.png build/models/fx580vnx_emu/
    ln -sf ../../../resource/rom.bin build/models/fx580vnx_emu/rom.bin
fi

( cd machine && SDL_VIDEODRIVER=dummy timeout -s KILL 300 \
    emulator/build/emulator model=../build/models/fx580vnx_emu \
    script=../tools/xcheck.lua ) 2>&1 | grep "^CASE " > build/xcheck_ref.txt
ref=$(wc -l < build/xcheck_ref.txt)
if [ "$ref" -lt 10 ]; then
    echo "cross-check: FAILED - the emulator produced $ref cases (expected $mine)" >&2
    echo "  an empty or truncated trace must not be diffed as if it agreed." >&2
    exit 1
fi
if diff -u build/xcheck_mine.txt build/xcheck_ref.txt; then
    echo "cross-check: OK ($ref cases)"
else
    echo "cross-check: MISMATCH"
    exit 1
fi
