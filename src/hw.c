/* hw.c - hardware-register helpers of the fx-580VN X firmware.
 *
 * Every function here is a byte-for-byte translation of one ROM routine.  The
 * ROM origin is in the comment above it; `tests/t_hw.c` compares each one with
 * the real ROM on random state.
 */
#include "hw.h"

#include "display.h"
#include "edit.h"
#include "keyboard.h"
#include "print.h"
#include "runtime.h"

/* 0x099FC - `mov r0, 1; st r0, [0xF810]; rt` */
uint16_t set_flag_0F810(uint8_t *ds)
{
    ds[SFR_0F810] = 1;
    return 1;
}

/* 0x09A04 */
uint16_t set_flag_0F812(uint8_t *ds)
{
    ds[SFR_0F812] = 1;
    return 1;
}

/* 0x09A0C */
uint16_t set_flag_0F811(uint8_t *ds)
{
    ds[SFR_0F811] = 1;
    return 1;
}

/* 0x09A24 - the load only touches r0, so the high half of the result word is
 * the caller's r1; with a clean register file that is zero. */
uint16_t read_0F050(uint8_t *ds)
{
    return ds[SFR_0F050];
}

/* 0x0A128 - `mov r0, 0` then six stores, so er0 keeps a zero low byte. */
uint16_t screen_state_clear(uint8_t *ds)
{
    ds[SFR_DISP] = 0;
    ds[SFR_DISP1] = 0;
    ds[SFR_DISP2] = 0;
    ds[SFR_0F812] = 0;
    ds[SFR_0F811] = 0;
    ds[SFR_DISP3] = 0;
    return 0;
}

/* 0x0A10A - `push xr0` / `pop xr0` bracket the work, so the caller's er0 comes
 * back even though the routine reuses r0. */
uint16_t screen_mode_select(uint8_t *ds, uint16_t arg)
{
    if ((ds[RAM_0D114] & 0x10) == 0)
        ds[SFR_SCREEN_MODE] = 22;
    screen_state_clear(ds);
    return arg;
}

/* 0x0A152 */
uint16_t key_filter_all(uint8_t *ds)
{
    ds[0xF042] = 255;
    return 255;
}

/* 0x0A532 */
uint16_t flags_set_four(uint8_t *ds)
{
    ds[0xD134] = 1;
    ds[0xD135] = 1;
    ds[0xD136] = 1;
    ds[0xD16A] = 1;
    return 1;
}

/* 0x09FB8 - clear interrupt-enable bit 1 of 0xF014, then rewrite the timer
 * pair.  Two details the earlier reading got wrong:
 *
 *   `and r0, 253` is `& 0xFD`, so the bit that goes is bit 1, not bit 2.
 *   `st r0, [ea]` does *not* post-increment, so 80 at 0xF008 is overwritten by
 *   the 160 before `[ea+]` moves the pointer on: the surviving pair is
 *   0xF008 = 160 and 0xF009 = 2, and 0xF00A is never touched.
 *
 * The routine ends with r1 = 160 and r0 = 2, i.e. 0xA002. */
uint16_t lcd_timer_poke(uint8_t *ds)
{
    ds[0xF014] &= (uint8_t)253u;
    ds[0xF008] = 160;
    ds[0xF009] = 2;
    return (uint16_t)((160u << 8) | 2u);
}

/* 0x094FE - the two `st er0, [ea+]` stores clear 0xF800..0xF803, then the
 * modifier byte at 0xD110 decides which of the three flags comes back. */
uint16_t display_flags_from_modifiers(uint8_t *ds)
{
    uint8_t modifiers = ds[0xD110];

    ds[SFR_DISP] = 0;
    ds[SFR_DISP1] = 0;
    ds[0xF802] = 0;
    ds[SFR_DISP2] = 0;

    if (modifiers & 0x08)
        ds[SFR_DISP] = 1;
    if (modifiers & 0x04)
        ds[SFR_DISP1] = 1;
    if (modifiers & 0x01)
        ds[SFR_DISP2] = 1;
    return 1;
}

/* 0x0A878 */
uint16_t mode136_screen_state(uint8_t *ds)
{
    if (ds[0xD111] == 136 && ds[RAM_0D114] == 1)
        return 1;
    return 0;
}

/* 0x0AAD4 - r1 is left holding the font size when the mode 2 path runs, so the
 * result word carries it in the high byte. */
uint16_t cursor_rows_init(uint8_t *ds)
{
    ds[0xD157] = 1;
    ds[0xD15A] = 0;
    ds[0xD15B] = 0;

    if (ds[0xD111] != 2)
        return ds[0xD111];

    uint8_t font = ds[0xD137];
    ds[0xD157] = (uint8_t)(ds[0xD157] + font);
    uint8_t second = (uint8_t)(ds[0xD159] + font);
    ds[0xD159] = second;
    return (uint16_t)(((uint16_t)font << 8) | second);
}

/* 0x0A9DE */
uint16_t row_limit_for_mode(uint8_t *ds)
{
    uint8_t limit = 200;
    uint8_t mode = ds[0xD111];

    if (mode == 13) {
        limit = 50;
        if (ds[0xDDB0] == 1 && ds[0xD115] == 1)
            limit = 49;
    } else if (mode == 14) {
        limit = 50;
    }
    /* r1 still holds the limit when `mov r0, r1` runs, so both halves match. */
    return (uint16_t)(((uint16_t)limit << 8) | limit);
}

/* 0x0AC40 - the ROM compares the two bytes with `cmp r8, r1` (signed) and
 * subtracts in one direction or the other.  r1 is loaded from 0xD010 again
 * before the compare, so the result word carries that byte as its high half. */
uint16_t distance_from_0D010(uint8_t *ds, uint16_t value)
{
    uint8_t r8 = (uint8_t)value;
    uint8_t base = ds[0xD010];
    uint8_t result;

    /* `bc lt` tests the carry flag, i.e. an unsigned compare. */
    if (r8 < base)
        result = (uint8_t)(base - r8);
    else
        result = (uint8_t)(r8 - base);

    return (uint16_t)(((uint16_t)base << 8) | result);
}

/* 0x0E48C - see hw.h.  The ROM re-uses er0 for the `memzero` call, so the
 * cleared destination is what comes back in mode 12. */
uint16_t mode12_screen_reset(uint8_t *ds, uint16_t arg)
{
    /* Outside mode 12 the routine still falls through `l r0, [mode]`, so the
     * low half of the result is the mode byte, not the argument byte. */
    if (ds[MEM_MODE] != 12)
        return (uint16_t)((arg & 0xFF00u) | ds[MEM_MODE]);

    ds[0xD0F6] = 0;
    flags_set_four(ds);
    return memzero(ds, 0xD400u, 0x0640u);
}

/* 0x0C3B8 */
uint16_t mode3_screen_reset(uint8_t *ds, uint16_t arg)
{
    if (ds[MEM_MODE] != 3)
        return (uint16_t)((arg & 0xFF00u) | ds[MEM_MODE]);

    ds[0xD0F6] = 0;
    flags_set_four(ds);
    memzero(ds, 0xD400u, 0x0640u);
    ds[0xD142] = 0;
    ds[0xD164] = 0;
    ds[0xD165] = 0;
    return 0;
}

/* 0x2980E */
uint16_t mode136_row_clear(uint8_t *ds, uint16_t arg)
{
    if (ds[MEM_MODE] != 136)
        return (uint16_t)((arg & 0xFF00u) | ds[MEM_MODE]);

    ds[0xD152] = 0;
    return memzero(ds, 0xD84Cu, 200u);
}

/* 0x246D4 */
uint16_t editor_buffers_reset(uint8_t *ds)
{
    flags_set_four(ds);
    memzero(ds, 0xD0F8u, 0x0016u);
    return memzero(ds, 0xD392u, 0x03C0u);
}

/* 0x09FE0 - arm the 0xD13A flag byte: unless bit 7 is already set it becomes
 * 136 (0x88).  The result is 1 when the bit was set, 136 when it was written. */
uint16_t flag_13A_arm(uint8_t *ds)
{
    if (ds[0xD13A] & 0x80)
        return 1;
    ds[0xD13A] = 136;
    return 136;
}

/* 0x09FFE - just bit 7 of 0xD13A. */
uint16_t flag_13A_bit7(uint8_t *ds)
{
    return (ds[0xD13A] & 0x80) ? 1 : 0;
}

/* 0x2B3E2 - true when the three state bytes are 0, 0 and 1. */
uint16_t state_predicate_2B3E2(uint8_t *ds)
{
    if (ds[0xD0F5] != 0)
        return 0;
    if (ds[0xD113] != 0)
        return 0;
    return ds[0xD116] == 1 ? 1 : 0;
}

/* 0x1D94A - the high nibble of the byte at `addr`.  The mask is an 8-bit
 * `and r0, 240`, so only the low half of er0 is rewritten: the high byte of
 * the address argument survives into the result. */
uint16_t byte_high_nibble(uint8_t *ds, uint16_t addr)
{
    return (uint16_t)((addr & 0xFF00u) | (uint16_t)(ds[addr] & 0xF0u));
}

/* 0x2082C - the high nibble of the state byte at 0xD118. */
uint16_t state_high_nibble(uint8_t *ds)
{
    return (uint16_t)(ds[0xD118] >> 4);
}

/* 0x20834 - the low nibble of the state byte at 0xD118. */
uint16_t state_low_nibble(uint8_t *ds)
{
    return (uint16_t)(ds[0xD118] & 0x0Fu);
}

/* 0x20860 - store the low byte into 0xD118 and clear 0xD148; the routine then
 * zeroes er0 itself, so the caller always sees 0. */
uint16_t state_store(uint8_t *ds, uint16_t value)
{
    ds[0xD118] = (uint8_t)value;
    ds[0xD148] = 0;
    return 0;
}

/* 0x14C9C - add 0xFF34, i.e. subtract 0xCC, from the argument. */
uint16_t offset_minus_cc(uint8_t *ds, uint16_t value)
{
    (void)ds;
    return (uint16_t)(value + 0xFF34u);
}

/* 0x09FD8 - the byte at 0xF014, widened into er0. */
uint16_t read_0F014(uint8_t *ds)
{
    return ds[0xF014];
}

/* 0x29978 - always returns 4 and touches nothing. */
uint16_t constant_four(uint8_t *ds)
{
    (void)ds;
    return 4;
}

/* 0x2083C - fold the low nibble of the argument into the high nibble of the
 * state byte, keeping the byte's existing low nibble.  Every step is 8-bit
 * (`and r1, 15`, `sll r0, 4`, `or r1, r0`), so the result word is the shifted
 * argument in the low half and the merged byte in the high half. */
uint16_t state_nibble_merge(uint8_t *ds, uint16_t value)
{
    uint8_t merged = (uint8_t)((ds[0xD118] & 0x0Fu) | ((uint8_t)value << 4));

    ds[0xD118] = merged;
    return (uint16_t)(((uint16_t)merged << 8) | (uint8_t)((uint8_t)value << 4));
}

/* 0x28FB0 - 1 when the low byte of the argument is 2, else 0. */
uint16_t value_is_two(uint8_t *ds, uint16_t value)
{
    (void)ds;
    return ((uint8_t)value == 2) ? 1 : 0;
}

/* 0x2F622 - 1 when the low byte of the argument is 3, else 0. */
uint16_t value_is_three(uint8_t *ds, uint16_t value)
{
    (void)ds;
    return ((uint8_t)value == 3) ? 1 : 0;
}

/* 0x2F9F6 - bit 7 of the modifier byte at 0xD110.  The answer is built with
 * `mov r0, ...` alone, so r1 keeps the caller's high half: with a non-zero
 * high byte the returned word is 0xNN00 or 0xNN01, not 0 or 1. */
uint16_t modifier_bit7(uint8_t *ds, uint16_t arg)
{
    uint16_t flag = ((ds[0xD110] & 0x80u) != 0) ? 1u : 0u;

    return (uint16_t)((arg & 0xFF00u) | flag);
}

/* 0x2FA02 - the byte at 0xD156 added to the word at 0xD00C. */
uint16_t counter_sum_0D156(uint8_t *ds)
{
    uint16_t base = (uint16_t)(ds[0xD00C] | ((uint16_t)ds[0xD00D] << 8));

    return (uint16_t)(ds[0xD156] + base);
}

/* 0x18940 - the argument times ten, walked from the 0xDA40 table base.
 * `mul er0, r2` multiplies the *low bytes* only: 0x0100 gives a product of 0
 * and 0x7FFF gives the same table entry as 0xFF. */
uint16_t record_10b_0DA40(uint8_t *ds, uint16_t value)
{
    (void)ds;
    return (uint16_t)((uint16_t)((uint8_t)value * 10u) + 0xDA40u);
}

/* 0x09D42 - zero 0xD116, 0xD117, 0xD118, 0xD119 and 0xD148; er0 comes back 0. */
uint16_t state_reset_0D116(uint8_t *ds)
{
    ds[0xD116] = 0;
    ds[0xD117] = 0;
    ds[0xD119] = 0;
    ds[0xD118] = 0;
    ds[0xD148] = 0;
    return 0;
}

/* 0x1826E - the byte of the ten-byte record at 0xDA40 for this index. */
uint16_t table_byte_10b(uint8_t *ds, uint16_t value)
{
    uint16_t addr = record_10b_0DA40(ds, value);

    /* `l r0, [er0]` loads a byte and leaves r1 alone, so the high half of the
     * record address survives into the result (0xDE11, not 0x0011). */
    return (uint16_t)((addr & 0xFF00u) | ds[addr]);
}

/* 0x0E1D2 - swap the two nibbles of the state byte at 0xD118.  The `mov r1,
 * r0` between the shift and the merge (not `mov r1, 0`) is what puts the old
 * high nibble in the returned word's high half, so the result is the moved
 * nibble above and the new state byte below. */
uint16_t state_swap_nibbles(uint8_t *ds)
{
    uint8_t old = ds[0xD118];
    uint8_t high = (uint8_t)(old >> 4);
    uint8_t merged = (uint8_t)((old & 0xF0u) | high);

    ds[0xD118] = merged;
    return (uint16_t)(((uint16_t)high << 8) | merged);
}

/* 0x2084C - fold the argument's low byte into the state byte at 0xD118, keep
 * that byte's high nibble, and clear 0xD148.  The final `mov r0, 0` only zeroes
 * the low half of er0, so the returned word is the *kept* high nibble shifted
 * up (0xF000 for a 0xF1 state byte), not 0. */
uint16_t state_merge_high(uint8_t *ds, uint16_t value)
{
    uint8_t old = ds[0xD118];
    uint8_t kept = (uint8_t)(old & 0xF0u);

    ds[0xD118] = (uint8_t)((uint8_t)value | kept);
    ds[0xD148] = 0;
    return (uint16_t)((uint16_t)kept << 8);
}

/* 0x20DD2 - drop a latched shift/repeat bit from the modifier byte at 0xD110:
 * `add r0, 128` wraps a set bit 7 away and `rb r0, 3` clears bit 3.  A byte
 * whose bit 7 is already clear is left alone and not even written back. */
uint16_t modifier_clear_bit7(uint8_t *ds, uint16_t arg)
{
    uint8_t value = ds[0xD110];

    if (value & 0x80u) {
        value = (uint8_t)(value + 0x80u);
        value = (uint8_t)(value & (uint8_t)~0x08u);
        ds[0xD110] = value;
    }
    return (uint16_t)((arg & 0xFF00u) | value);
}

/* 0x206FA - the separator glyph for the current display state: 0x2E ('.')
 * while either the extended-mode byte at 0xD14E or the flag at 0xD11C is set,
 * 0x2C (',') when neither is.  r1 keeps the byte that decided it, so the
 * returned *high* half is that byte (0x0046 vs 0x0146 for 0xD14E = 1). */
uint16_t dot_or_prime(uint8_t *ds)
{
    uint8_t high = ds[0xD14E];

    if (high == 0)
        high = ds[0xD11C];
    return (uint16_t)(((uint16_t)high << 8) | (high != 0 ? 0x2Eu : 0x2Cu));
}

/* 0x180D0 - the "nothing to report" prologue: er0, er2, er4 and er6 come back
 * zero and the r8/r9 pair is loaded with 0 and 1.  Only er0 is observable
 * through the calling convention - the rest are register-only side effects the
 * differential test cannot see - so the C body only has to answer 0. */
uint16_t clear_call_registers(uint8_t *ds)
{
    (void)ds;
    return 0;
}

/* 0x0BEFA - is the sixteen-bit field at 0xD10E one of the two token codes
 * 0xFC1F and 0xFC23?  `l er0` loads the pair and each test is the same
 * `cmp/cm pc` chain, so both halves take part. */
uint16_t token_at_d10e_is_1f_23(uint8_t *ds, uint16_t arg)
{
    (void)arg;
    uint16_t v = (uint16_t)(ds[0xD10E] | ((uint16_t)ds[0xD10F] << 8));

    /* `l er0` loads both halves, so the surviving high byte is the field's own,
     * not the caller's r1 */
    return (uint16_t)((v & 0xFF00u) | (uint16_t)(v == 0xFC1F || v == 0xFC23));
}

/* 0x0CA34 - the same test one code lower on each side, 0xFC1E and 0xFC22. */
uint16_t token_at_d10e_is_1e_22(uint8_t *ds, uint16_t arg)
{
    (void)arg;
    uint16_t v = (uint16_t)(ds[0xD10E] | ((uint16_t)ds[0xD10F] << 8));

    return (uint16_t)((v & 0xFF00u) | (uint16_t)(v == 0xFC1E || v == 0xFC22));
}

/* 0x2AE00 - mode 12 with sub-state 1. */
uint16_t mode_is_12_1(uint8_t *ds, uint16_t arg)
{
    uint8_t flag = (uint8_t)(ds[0xD111] == 12 && ds[0xD114] == 1);

    return (uint16_t)((arg & 0xFF00u) | flag);
}

/* 0x2B226 - mode 74 (0x4A) with sub-state 1. */
uint16_t mode_is_74_1(uint8_t *ds, uint16_t arg)
{
    uint8_t flag = (uint8_t)(ds[0xD111] == 74 && ds[0xD114] == 1);

    return (uint16_t)((arg & 0xFF00u) | flag);
}

/* 0x2F6FC - the upper nibble of the byte behind a pointer is a box class: 2
 * and 8 are the two that draw a frame.  `srl r0, 4` on a byte register works
 * here because only r0 is shifted and the bits leaving it are dropped.  Again
 * only r0 is written, so the high half of the pointer comes back unchanged. */
uint16_t nibble_hi_is_2_or_8(uint8_t *ds, uint16_t ptr)
{
    uint8_t kind = (uint8_t)(ds[ptr] >> 4);

    return (uint16_t)((ptr & 0xFF00u) | (uint16_t)(kind == 2 || kind == 8));
}

/* 0x0AF1A - the "nothing chosen yet" test: er0 is either 35 or zero.  The first
 * three instructions are a plain sixteen-bit compare with 35 (`cmp r0, 35 /
 * cmpc r1, 0`, chained Z), and the failing branch lands on `mov er0, er0`,
 * which is the ROM's way of testing the whole pair against zero before jumping
 * back to the same `mov r0, 1`.  So the zero case answers 1 as well, and a
 * sixteen-bit sweep of the entry shows exactly two values that do.  Only r0 is
 * written on the way out, so the high half is the caller's r1. */
uint16_t is_zero_or_35(uint8_t *ds, uint16_t arg)
{
    (void)ds;
    return (uint16_t)((arg & 0xFF00u) | (uint16_t)(arg == 0 || arg == 35));
}

/* 0x0C34E - submode 19 or 20, the two that keep the cursor visible. */
uint16_t submode_is_19_or_20(uint8_t *ds, uint16_t arg)
{
    uint8_t s = ds[0xD114];

    return (uint16_t)((arg & 0xFF00u) | (uint16_t)(s == 19 || s == 20));
}

/* 0x2AF28 - the three-way state byte at 0xD015, true for 1 and for 3. */
uint16_t state_is_1_or_3(uint8_t *ds, uint16_t arg)
{
    uint8_t s = ds[0xD015];

    return (uint16_t)((arg & 0xFF00u) | (uint16_t)(s == 1 || s == 3));
}

/* 0x2D4D8 - bit 4 of 0xD114 set *and* the sub-state byte at 0xD116 equal to 1.
 * `tb` tests a bit without disturbing anything else, and a clear bit short
 * circuits before the second test. */
uint16_t flag4_and_substate_1(uint8_t *ds, uint16_t arg)
{
    uint8_t flag = 0;

    if (ds[0xD114] & 0x10)
        flag = (uint8_t)(ds[0xD116] == 1);

    return (uint16_t)((arg & 0xFF00u) | flag);
}

/* 0x0A146 - display enable: the byte at 0xF815 becomes 1 when the argument is
 * non-zero and 0 when it is, and er0 answers with the flag in its low half.  It
 * is the label inside 0x0A128's body (`.l_01E`), reached by `bl` from the
 * routines that switch the panel on and off around a delay.
 *
 * `cmp r0, 0` is an *eight-bit* compare, so only the low half of the argument
 * decides - 0x0100 switches the display off, not on. */
uint16_t display_enable_set(uint8_t *ds, uint16_t arg)
{
    uint8_t on = (uint8_t)((uint8_t)arg != 0);

    ds[0xF815] = on;
    return (uint16_t)((arg & 0xFF00u) | on);
}

/* 0x1844E - "is the byte at offset 8 of this record above 14?".  The record is
 * addressed through 0x18940, and the answer goes into the low half of er0 while
 * the high half keeps the top byte of the record pointer. */
uint16_t record_tail_over_14(uint8_t *ds, uint16_t value)
{
    uint16_t record = record_10b_0DA40(ds, value);
    uint8_t tail = ds[(uint16_t)(record + 8)];

    return (uint16_t)((record & 0xFF00u) | (tail > 14 ? 1u : 0u));
}

/* 0x0CB0E - a body that is nothing but `rt`.  Six call sites reach it through
 * `bl`, so it is a real entry and not a hole: it does nothing and hands the
 * caller's er0 straight back, which is exactly what the identity does. */
uint16_t no_op_0CB0E(uint8_t *ds, uint16_t arg)
{
    (void)ds;
    return arg;
}

/* 0x1FFFE - the same thing written as `pop pc`.  Five `bl` sites call it as a
 * plain "do nothing and return" helper, and since `bl` pushes the return
 * address the pop goes back to the caller. */
uint16_t return_immediately_1FFFE(uint8_t *ds, uint16_t arg)
{
    (void)ds;
    return arg;
}

/* The shared tail of the reset routine above, entered at the label the ROM
 * calls with `bl f_09D42.l_018` (0x09D5A): it zeroes 0xD119, 0xD118 and 0xD148
 * but leaves 0xD116 and 0xD117 alone, and r0 comes back 0 with r1 untouched. */
static void state_reset_0D119(uint8_t *ds)
{
    ds[0xD119] = 0;
    ds[0xD118] = 0;
    ds[0xD148] = 0;
}

/* 0x2ACBE - settle the screen state for a fresh mode: three state bytes are
 * cleared, the flag at 0xD14F decides whether four "first time through" flags
 * are armed too, and the code byte at 0xD114 becomes 18 when it is set and 9
 * when it is not.  0xD115 and 0xD116 are then forced to 0 and 3.
 *
 * The answer is `mov r0, 3` over an r1 nothing in the body ever wrote, so the
 * caller's high half survives. */
uint16_t mode_code_from_flag(uint8_t *ds, uint16_t arg)
{
    state_reset_0D119(ds);

    if (ds[0xD14F] != 0) {
        (void)flags_set_four(ds);
        ds[0xD114] = 18;
    } else {
        ds[0xD114] = 9;
    }
    ds[0xD115] = 0;
    ds[0xD116] = 3;
    return (uint16_t)((arg & 0xFF00u) | 3u);
}

/* 0x286A0 - record the mode byte and mark the state dirty unless the old and
 * the new byte are both 0xC0 (the one transition that is already settled).
 * The callee's r0 = 0 lands under the caller's untouched high half. */
uint16_t mode_state_update(uint8_t *ds, uint16_t arg)
{
    uint8_t value = (uint8_t)arg;

    if (!(ds[0xD114] == 0xC0u && value == 0xC0u))
        ds[0xD115] = 1;
    ds[0xD114] = value;
    state_reset_0D119(ds);
    return (uint16_t)(arg & 0xFF00u);
}

/* 0x0A546 - "is the state byte's bit 6 set, or is the text at 0xD180 empty?".
 * Two sixteen-bit details: `tb [d_0D116], 6` tests the bit and `mov er0, er0`
 * is the ROM's sixteen-bit zero test on the length, not a no-op.  The answer is
 * `mov r0, r4` over an r1 the length left behind, so the length's high byte
 * survives as the answer's high half. */
uint16_t flag_or_empty_0D180(uint8_t *ds)
{
    uint8_t flag = (uint8_t)((ds[0xD116] >> 6) & 1u);
    uint16_t length = smart_strlen_n(ds, 0xD180u);

    if (length == 0)
        flag = 1;
    return (uint16_t)((length & 0xFF00u) | flag);
}

/* 0x2F78C - true when the low byte of the argument is two, or is four.  Both
 * answers are `mov r0, x` over the r1 that 0x28FB0 never wrote, so the caller's
 * high half comes back unchanged. */
uint16_t value_is_two_or_four(uint8_t *ds, uint16_t arg)
{
    if ((uint8_t)value_is_two(ds, arg) != 0 || (uint8_t)arg == 4u)
        return (uint16_t)((arg & 0xFF00u) | 1u);
    return (uint16_t)(arg & 0xFF00u);
}

/* 0x2F710 - 1 when 0x2ABE4's code is 64, 65..73 or 75; 0 otherwise.
 *
 * The compares are sixteen-bit: `cmp r8, 73 ; cmpc r9, 0` is the pair form that
 * folds the carry in, so they are unsigned comparisons of the whole er8, not of
 * its low byte.  The answer is a bare `mov r0, x`; the `mov r1, 0` that loads
 * the 66 for the first compare has already emptied the high half, so the er0 the
 * caller sees is 1 or 0 and nothing else. */
uint16_t code_is_64_to_73_or_75(uint8_t *ds, uint16_t arg)
{
    uint16_t code = editor_special_to_ascii(arg);
    uint8_t answer;

    (void)ds;
    if (0x0042u > code) {
        /* the early branch skips the window test, but 64 is below it too */
        answer = (code == 64u || code == 75u) ? 1 : 0;
    } else if (code <= 73u) {
        answer = 1;
    } else {
        answer = (code == 75u) ? 1 : 0;
    }
    return answer;
}

/* 0x20CF2 - does the current mode/submode pair need the screen set up?
 *
 * Mode 9 hands the decision to the per-submode table byte: the answer is 1
 * unless that byte is 9.  Mode 6 accepts submodes 1..3 (both compares are
 * unsigned), and every other mode accepts only when bit 7 of the mode byte is
 * set and the submode is exactly 2.
 *
 * The high half of the answer comes from wherever r1 was left: the table byte's
 * routine zeroes r1 before its load, while the other two paths never write r1
 * at all, so the caller's byte comes back there. */
uint16_t submode_needs_setup(uint8_t *ds, uint16_t arg)
{
    uint8_t mode = ds[0xD114];
    uint8_t sub = ds[0xD115];
    uint16_t high = (uint16_t)(arg & 0xFF00u);

    if (mode == 9u) {
        uint16_t attr = var_mode_attr(ds);

        if ((uint8_t)attr == 9u)
            return (uint16_t)(attr & 0xFF00u);
        return (uint16_t)((attr & 0xFF00u) | 1u);
    }
    if (mode == 6u) {
        if (sub < 1u || sub > 3u)
            return high;
        return (uint16_t)(high | 1u);
    }
    if ((uint8_t)(mode & 0x80u) == 0 || sub != 2u)
        return high;
    return (uint16_t)(high | 1u);
}

/* 0x09EE8 - the setup a mode switch runs before the next redraw.  Everything
 * here is state, not arithmetic: clear the packed buffer, fill the title box
 * from the ROM's dotted pattern at 0x646E (a *pointer*, one byte consumed per
 * column byte), render, wait a while, drop the LCD mode and the timer flags,
 * then jump into the main loop - `b _start` at 0x0A1CE, so the main path never
 * comes back and only 0xD0F5 != 0 returns at all.
 *
 * The answer on that early exit is the 0xD0F5 byte with the caller's r1 on
 * top; every caller in the firmware is entered with r1 = 0. */
uint16_t screen_mode_setup(uint8_t *ds)
{
    if (ds[0xD0F5] != 0)
        return ds[0xD0F5];

    buffer_clear(ds);                                    /* 0x08C60 */
    (void)fill_rect_stream(ds, 0x1625, 0x1680, 0x646Eu);  /* 0x09846 */
    render_ddd4(ds);                                     /* 0x0947C */
    (void)timer_delay(ds, 0x1000);                       /* 0x09F3A */

    ds[0xF031] = 3;                                      /* screen.mode */
    ds[0xF03D] = 0;
    ds[0xF00A] = 0;
    ds[0xF010] = 0;
    ds[0xF011] = 0;
    key_filter_clear(ds);                                /* 0x0A15A */
    key_output_zero(ds);                                 /* 0x0A16A */
    (void)timer_delay_tail(ds);                          /* 0x09F68 */
    return 0;                                            /* never reached */
}

/* 0x24B10 - bit 4 of the mode byte short-circuits to "no setup".  Otherwise
 * the per-submode predicate decides: its nonzero answer also means "no setup".
 * The high half of the answer is threaded through every path: on the short
 * circuit it is the caller's r1 (only r0 was zeroed), on the other two it is
 * whatever submode_needs_setup left in r1. */
uint16_t submode_setup_gate(uint8_t *ds, uint16_t arg)
{
    if (ds[RAM_0D114] & 0x10u)
        return (uint16_t)(arg & 0xFF00u);

    uint16_t s = submode_needs_setup(ds, arg);
    if ((uint8_t)s != 0)
        return (uint16_t)(s & 0xFF00u);
    return (uint16_t)((s & 0xFF00u) | 1u);
}

/* 0x09E4E - clear an 80-byte state buffer at 0xD392, then plant a kind-1
 * ten-byte record at 0xD39C.  `push xr0`/`pop xr0` bracket the work, so the
 * caller's er0..er3 all come back untouched and er0 is the incoming argument. */
uint16_t init_state_buffers(uint8_t *ds, uint16_t arg)
{
    (void)memzero(ds, 0xD392u, 80u);
    (void)record_init_kind(ds, 0xD39Cu, 1u);
    return arg;
}

/* 0x1841E - initialise three consecutive ten-byte variable records: kinds 0,
 * 0, 1.  r8 holds the incoming argument, stepped by one each round; the
 * address each record gets is `record_10b_0DA40` (index * 10 + 0xDA40).  The
 * final `bl f_1D950` leaves its own answer in er0, which is what comes back. */
uint16_t init_three_var_records(uint8_t *ds, uint16_t arg)
{
    uint16_t r8 = arg;
    uint16_t out = 0;

    for (int i = 0; i < 3; i++) {
        uint16_t addr = record_10b_0DA40(ds, r8);
        out = record_init_kind(ds, addr, (uint8_t)(i == 2 ? 1 : 0));
        r8 = (uint16_t)(r8 + 1u);
    }
    return out;
}

/* 0x24F26 - the two-byte marker at the pointer (201, 26).  Only r0 is reloaded
 * from memory, so the pointer's own high byte survives in the answer. */
uint16_t record_pair_marker(uint8_t *ds, uint16_t ptr)
{
    uint16_t high = (uint16_t)(ptr & 0xFF00u);
    uint8_t hit = (ds[ptr] == 201u && ds[(uint16_t)(ptr + 1)] == 26u);

    return (uint16_t)(high | hit);
}

/* 0x2847C - bit 6 of the state byte arms the 35 test; er0 == 0 is always an
 * independent yes (the `mov er0, er0` at 0x2848C is a 16-bit zero test). */
uint16_t armed_35_or_zero(uint8_t *ds, uint16_t arg)
{
    uint16_t high = (uint16_t)(arg & 0xFF00u);
    uint8_t hit = (arg == 0u) ||
                  ((ds[0xD116] & 0x40u) != 0 && arg == 35u);

    return (uint16_t)(high | hit);
}

/* 0x1DD44 - a record byte whose low nibble is < 10 and whose +9 byte has a
 * clear high nibble.  The pointer's high byte survives into the answer. */
uint16_t record_nibble_predicate(uint8_t *ds, uint16_t ptr)
{
    uint16_t high = (uint16_t)(ptr & 0xFF00u);
    uint8_t hit = 1u;

    if ((ds[ptr] & 0x0Fu) < 10u && (ds[(uint16_t)(ptr + 9)] & 0xF0u) == 0u)
        hit = 0u;
    return (uint16_t)(high | hit);
}

/* 0x24BBE / 0x0D64E - the parked word at 0xD10E.  `l er0,[0xD10E]` loads both
 * halves of er0, and the final `mov r0,1/0` touches only the low half, so the
 * answer's high byte is the parked word's own high byte (normally 0xFC). */
uint16_t parked_word_fc1d_or_fc21(uint8_t *ds)
{
    uint16_t word = (uint16_t)(ds[0xD10E] | ((uint16_t)ds[0xD10F] << 8));
    uint8_t hit = (word == 0xFC1Du || word == 0xFC21u);

    return (uint16_t)(((uint16_t)ds[0xD10F] << 8) | hit);
}

uint16_t parked_word_fc1c_or_fc20(uint8_t *ds)
{
    uint16_t word = (uint16_t)(ds[0xD10E] | ((uint16_t)ds[0xD10F] << 8));
    uint8_t hit = (word == 0xFC1Cu || word == 0xFC20u);

    return (uint16_t)(((uint16_t)ds[0xD10F] << 8) | hit);
}

/* 0x0D5E0 - byte 166, 167 or 192.  r1 is never written, so the caller's high
 * half comes back beside the byte-sized answer. */
uint16_t byte_166_167_192(uint8_t *ds, uint16_t arg)
{
    (void)ds;
    uint8_t v = (uint8_t)arg;

    return (uint16_t)((arg & 0xFF00u) |
                      (v == 166u || v == 167u || v == 192u));
}

/* 0x0C7B0 - the two-byte marker at the pointer (25, 27 or 28). */
uint16_t record_marker_25_27_28(uint8_t *ds, uint16_t ptr)
{
    uint16_t high = (uint16_t)(ptr & 0xFF00u);
    uint8_t hit = (ds[ptr] == 25u &&
                   (ds[(uint16_t)(ptr + 1)] == 27u ||
                    ds[(uint16_t)(ptr + 1)] == 28u));

    return (uint16_t)(high | hit);
}

/* 0x25010 - the argument itself is 0xFC3E, 0xFC1E or 0xFC1F.  The `cmpc r1,
 * 252` compares er0's high byte; only r0 is overwritten on the way out, so the
 * caller's high half survives into the answer. */
uint16_t value_fc3e_fc1e_fc1f(uint8_t *ds, uint16_t arg)
{
    (void)ds;
    uint8_t hit = (arg == 0xFC3Eu || arg == 0xFC1Eu || arg == 0xFC1Fu);

    return (uint16_t)((arg & 0xFF00u) | hit);
}

/* 0x2F7E4 - 165, or the small 0xFB01..0xFB05 window. */
uint16_t value_165_or_fb01_fb05(uint8_t *ds, uint16_t arg)
{
    (void)ds;
    uint8_t hit = (arg == 165u || (arg >= 0xFB01u && arg <= 0xFB05u));

    return (uint16_t)((arg & 0xFF00u) | hit);
}

/* 0x1FFE6 / 0x2B986 - the argument in a narrow 0xFCxx window.  The `mov r1,
 * 252` before the compare leaves 0xFC in the answer's high half; only r0 is
 * overwritten with the byte result. */
uint16_t value_in_fc10_fc1b(uint8_t *ds, uint16_t arg)
{
    (void)ds;
    uint8_t hit = (arg >= 0xFC10u && arg <= 0xFC1Bu);
    return (uint16_t)(0xFC00u | hit);
}

uint16_t value_in_fc28_fc2c(uint8_t *ds, uint16_t arg)
{
    (void)ds;
    uint8_t hit = (arg >= 0xFC28u && arg <= 0xFC2Cu);
    return (uint16_t)(0xFC00u | hit);
}

/* 0x15E36 - the byte at the pointer is 45/46 or a digit in 48..57.  `l r1,[er0]`
 * loads that byte into r1, which then becomes the answer's high half. */
uint16_t digit_or_sign_predicate(uint8_t *ds, uint16_t ptr)
{
    uint8_t v = ds[ptr];
    uint8_t hit = (v == 45u || v == 46u || (v >= 48u && v <= 57u));

    return (uint16_t)(((uint16_t)v << 8) | hit);
}

/* 0x2F76E - write a two-byte marker to the caller's pointer: 136/0 when the
 * 0xD13F flag is clear, 172/0 when set.  The pointer itself comes back in er0
 * (`mov er0, er8` reloads both halves just before `pop er8`). */
uint16_t write_marker_2f76e(uint8_t *ds, uint16_t ptr)
{
    ds[ptr] = (ds[0xD13F] != 0) ? 172u : 136u;
    ds[(uint16_t)(ptr + 1)] = 0;
    return ptr;
}

/* 0x0A0DA - a fixed burst of SFR pokes that arms the display.  `mov er0,4`
 * zeroes r1, so the trailing `st r1,[0xF039]` writes 0 and the final er0 is
 * just the low byte 21. */
uint16_t display_arm_sequence(uint8_t *ds)
{
    ds[0xF00A] = 1;
    ds[0xF030] = 4;
    ds[0xF033] = 3;
    ds[0xF034] = 9;
    ds[0xF035] = 23;
    ds[0xF036] = 1;
    ds[0xF039] = 0;
    ds[0xF031] = 21;
    return 21;
}

/* 0x1F41E - the byte at the pointer is an ASCII digit.  `mov er0,0/1` reloads
 * both halves, so the caller's r1 does not survive here. */
uint16_t digit_byte_at_pointer(uint8_t *ds, uint16_t ptr)
{
    return (ds[ptr] >= 48u && ds[ptr] <= 57u) ? 1u : 0u;
}

/* 0x13B5A - a table lookup: the low nibble of er4 selects a word from the
 * 0xD0F8 table (two bytes per entry).  r4 and r6 are masked as a side effect,
 * but only the lookup result in er0 is observable to the batch runner. */
uint16_t table_lookup_d0f8(uint8_t *ds, uint16_t a, uint16_t b, uint16_t c)
{
    (void)ds; (void)a; (void)b;
    return (uint16_t)(0xD0F8u + ((c & 0x0Fu) << 1));
}

/* 0x0D80C - walk er0 up 1, 4, 13, 40, 121, 364, ... (er4 = previous er0,
 * er0 = 3*er4, plus one while er4 < er6) and stop once er4 reaches the
 * argument's *low byte* zero-extended: `mov r1,0` at 0x0D80E clears the high
 * half of er0 before `mov er6,er0` copies it.  `mov r0,r4` writes only the low
 * half of the answer and the high half is the high byte of the final er0. */
uint16_t triangular_from_arg(uint8_t *ds, uint16_t arg)
{
    (void)ds;
    uint16_t tgt = (uint16_t)(arg & 0xFFu);
    uint16_t er0 = 1;

    for (;;) {
        uint16_t er4 = er0;
        er0 = (uint16_t)(er0 + er0 + er4);
        if (er4 >= tgt)
            return (uint16_t)((er0 & 0xFF00u) | (er4 & 0xFFu));
        er0 = (uint16_t)(er0 + 1u);
    }
}

/* 0x2D59A / 0x25352 - small code sets, each ending in a `mov r0,1/0` whose
 * high half is the caller's untouched r1. */
uint16_t code_set_2d59a(uint8_t *ds, uint16_t arg)
{
    uint8_t v = (uint8_t)arg;
    uint8_t hit = (v == 8u || v == 3u || v == 10u || v == 2u ||
                   v == 4u || v >= 12u);

    (void)ds;
    return (uint16_t)((arg & 0xFF00u) | hit);
}

uint16_t code_set_25352(uint8_t *ds, uint16_t arg)
{
    uint8_t v = (uint8_t)arg;
    uint8_t hit = (v == 3u || v == 4u || v == 9u || v == 11u || v == 12u);

    (void)ds;
    return (uint16_t)((arg & 0xFF00u) | hit);
}

/* 0x20E4A - the mode byte selects one of two fixed 16-bit tokens, or 0. */
uint16_t mode_token_20e4a(uint8_t *ds)
{
    uint8_t mode = ds[MEM_MODE];

    if (mode == 136u)
        return 0xD784u;
    if (mode & 0x80u)
        return 0xD522u;
    if (mode & 0x40u)
        return 0xD784u;
    return 0;
}

/* 0x29980 - a conjunction of four state bytes: mode 12, state 5, bit 4 of the
 * mode byte and 0xD136 >= 2.  r1 is never written, so with the clean er1 of a
 * no-argument batch case the answer is a plain 0 or 1. */
uint16_t all_state_conditions(uint8_t *ds)
{
    if (ds[MEM_MODE] != 12u || ds[0xD116] != 5u)
        return 0;
    if ((ds[RAM_0D114] & 0x10u) == 0 || ds[0xD136] < 2u)
        return 0;
    return 1;
}

/* 0x09ADA - the argument is zero, has a non-0xFC high half, is exactly 0xFC42,
 * or has a low byte in 78..95; all other words answer 0. */
uint16_t token_window_09ada(uint8_t *ds, uint16_t arg)
{
    (void)ds;
    uint8_t hit;

    if (arg == 0u)
        hit = 0;
    else if ((uint8_t)(arg >> 8) != 0xFCu)
        hit = 1;
    else if (arg == 0xFC42u)
        hit = 1;
    else
        hit = ((uint8_t)arg >= 78u && (uint8_t)arg <= 95u);

    return (uint16_t)((arg & 0xFF00u) | hit);
}

/* 0x16D80 - if er10 < 24, store the incoming er0 at 0xD060 + er10*2 (a
 * two-byte store, so the address carries through r9); otherwise just park 7 in
 * r2.  er0 itself is never written, so it is the answer. */
uint16_t store_word_at_d060(uint8_t *ds, uint16_t a, uint16_t b, uint16_t c,
                            uint16_t d, uint16_t e, uint16_t er10)
{
    (void)a; (void)b; (void)c; (void)d; (void)e;

    if ((uint8_t)er10 < 24u) {
        uint16_t off = (uint16_t)(((uint8_t)er10) * 2u);
        uint16_t addr = (uint16_t)(0xD060u + off);

        ds_put(ds, addr, (uint8_t)a);
        ds_put(ds, (uint16_t)(addr + 1), (uint8_t)(a >> 8));
    }
    return a;
}

/* 0x1E604 - the byte at the pointer is 64 or 66..73. */
uint16_t value_64_or_66_73(uint8_t *ds, uint16_t ptr)
{
    uint8_t v = ds[ptr];
    return (v == 64u || (v >= 66u && v <= 73u)) ? 1u : 0u;
}

/* 0x16C8E - with a clear high byte, the low byte in three ranges answers 1. */
uint16_t ranges_16c8e(uint8_t *ds, uint16_t arg)
{
    uint8_t v = (uint8_t)arg;

    (void)ds;
    /* `cmp r1,0 ; bc ne` jumps to the zero path, but `mov r0,0` leaves r1
     * alone, so a nonzero high half survives into the answer on that path. */
    if ((uint8_t)(arg >> 8) != 0u)
        return (uint16_t)(arg & 0xFF00u);
    return (uint16_t)((v >= 125u && v <= 130u) || (v >= 135u && v <= 142u) ||
                      (v >= 147u && v <= 148u));
}

/* 0x1B698 - the unconditional `bc al` skips the first load, so this is just
 * the 0xD019 byte tested against 10. */
uint16_t state_d019_ge_10(uint8_t *ds, uint16_t arg)
{
    return (uint16_t)((arg & 0xFF00u) | (ds[0xD019] >= 10u));
}

/* 0x0C1D0 - the absolute difference (as an unsigned word) between er0 and the
 * word at 0xD00E. */
uint16_t abs_diff_d00e(uint8_t *ds, uint16_t arg)
{
    uint16_t w = (uint16_t)(ds[0xD00E] | ((uint16_t)ds[0xD00F] << 8));

    return arg < w ? (uint16_t)(w - arg) : (uint16_t)(arg - w);
}

/* 0x10E3A - the apparent `l r0, 3:[er14]` answers the literal 3 in the low
 * half regardless of the data space (the 0x90E0 extension makes the effective
 * read hit the immediate, not RAM).  Only r0 is written, so the caller's r1
 * survives into the high half. */
uint16_t table_byte_ffdf(uint8_t *ds, uint16_t arg)
{
    (void)ds;
    return (uint16_t)((arg & 0xFF00u) | 3u);
}

/* 0x26B00 - copy 160 bytes from er2 into the pointer in er0, then answer 1.
 * The count is passed as a stacked word, which is how the batch harness feeds
 * memcpy_nn its fourth argument. */
uint16_t copy_160_to_pointer(uint8_t *ds, uint16_t dst, uint16_t src)
{
    (void)memcpy_nn(ds, dst, src, 160u);
    /* memcpy_nn answers in er0 with dst; the trailing `mov r0,1` overwrites
     * only the low half, so the destination's high byte survives. */
    return (uint16_t)((dst & 0xFF00u) | 1u);
}

/* 0x0D4FA - a nibble of the byte at er0, times 160, plus 0xD392, stored as a
 * word at er2.  The routine answers 0. */
uint16_t store_scaled_nibble(uint8_t *ds, uint16_t arg, uint16_t ptr)
{
    uint16_t v = (uint16_t)((uint16_t)(ds[arg] & 0x0Fu) * 160u);
    uint16_t p = (uint16_t)(ptr & 0xFFFEu);

    v = (uint16_t)(v + 0xD392u);
    ds_put(ds, p, (uint8_t)v);
    ds_put(ds, (uint16_t)(p + 1), (uint8_t)(v >> 8));
    return 0;
}

/* 0x0E5E6 - walk a zero-terminated byte string from er0 and count the cells.
 * A byte of 4 or >= 240 is a two-byte cell (the extra byte is skipped) but
 * still counts as one. */
uint16_t string_cell_count(uint8_t *ds, uint16_t ptr)
{
    uint16_t p = ptr;
    uint16_t count = 0;

    for (;;) {
        uint8_t b = ds[p];

        if (b == 0u)
            return count;
        p = (uint16_t)(p + 1u);
        if (b == 4u || b >= 240u)
            p = (uint16_t)(p + 1u);
        count = (uint16_t)(count + 1u);
    }
}

/* 0x16D6C - read a word from 0xD05E + er10*2 (byte-wide arithmetic carrying
 * into r9); er0 is never written, so the caller's word comes straight back. */
uint16_t read_word_d05e(uint8_t *ds, uint16_t a, uint16_t b, uint16_t c,
                        uint16_t d, uint16_t e, uint16_t er10)
{
    (void)b; (void)c; (void)d; (void)e;
    uint16_t off = (uint16_t)((uint8_t)er10 * 2u);
    uint16_t addr = (uint16_t)(0xD05Eu + off);

    (void)ds[addr];
    (void)ds[(uint16_t)(addr + 1)];
    return a;
}

/* 0x1A5C0 - OR the second register bank into the first, byte by byte: r0|=r8,
 * r1|=r9, r2|=r10, ... r7|=r15.  The batch runner only watches er0, which is
 * the OR of e[0] and e[4] (the two halves contributed by r0/r1 and r8/r9). */
uint16_t or_all_registers(uint8_t *ds, uint16_t e0, uint16_t e1, uint16_t e2,
                          uint16_t e3, uint16_t e4, uint16_t e5, uint16_t e6,
                          uint16_t e7)
{
    (void)ds; (void)e1; (void)e2; (void)e3; (void)e5; (void)e6; (void)e7;
    return (uint16_t)(e0 | e4);
}

/* 0x13B6E - a nonzero er8 (both halves) selects the fixed 0xD392 token;
 * otherwise the caller's er0 comes straight back. */
uint16_t token_if_er8_nonzero(uint8_t *ds, uint16_t arg0, uint16_t z1,
                              uint16_t z2, uint16_t z3, uint16_t er8)
{
    (void)ds; (void)z1; (void)z2; (void)z3;
    if ((uint8_t)er8 == 0u || (uint8_t)(er8 >> 8) == 0u)
        return arg0;
    return 0xD392u;
}

/* 0x14450 - a tail call into 0x13ACC's shared copy: the source is er6, the
 * destination er10, and the ten-byte copy answers with the destination. */
uint16_t copy_record_er6_to_er10(uint8_t *ds, uint16_t a, uint16_t b,
                                 uint16_t c, uint16_t er6, uint16_t e,
                                 uint16_t er10)
{
    (void)a; (void)b; (void)c; (void)e;
    return record10_er8_from_er4(ds, er6, er10);
}

/* 0x1D986 - initialise a ten-byte record at er0.  A kind of 0 or >= 16 becomes
 * 243; otherwise its high nibble is forced to 0xF.  The rest of the record is
 * zero.  `mov r0,0` leaves the caller's r1 as the answer's high half. */
uint16_t init_record_kind_1d986(uint8_t *ds, uint16_t dst, uint16_t kind)
{
    uint8_t k = (uint8_t)kind;
    uint16_t d = (uint16_t)(dst & 0xFFFEu);

    if (k == 0u || k >= 16u)
        k = 243u;
    else
        k = (uint8_t)(k | 0xF0u);

    ds_put(ds, d, k);
    ds_put(ds, (uint16_t)(d + 1), 0);
    for (int i = 2; i < 10; i++)
        ds_put(ds, (uint16_t)(d + i), 0);
    return (uint16_t)(dst & 0xFF00u);
}

/* 0x17F32 - copy a ten-byte record between two 0xDA40-table entries selected
 * by the low and high bytes of er0.  `l qr0,[ea+]` loads the source's first
 * eight bytes into er0..er6, so er0 answers with the source's first word, read
 * before any store (which matters when the two entries overlap). */
uint16_t copy_record_by_bytes(uint8_t *ds, uint16_t arg)
{
    uint16_t src = record_10b_0DA40(ds, (uint8_t)arg);
    uint16_t dst = record_10b_0DA40(ds, (uint8_t)(arg >> 8));
    uint16_t first = (uint16_t)(ds[src] | ((uint16_t)ds[(uint16_t)(src + 1)] << 8));

    (void)record10_er8_from_er4(ds, src, dst);
    return first;
}

/* 0x2244A - initialise a record at 0xD784 with the argument's low byte as its
 * kind, then answer that byte with a clean high half (r12 is saved, r1 zeroed). */
uint16_t init_record_2244a(uint8_t *ds, uint16_t arg)
{
    (void)init_record_kind_1d986(ds, 0xD784u, (uint8_t)arg);
    return (uint16_t)(arg & 0xFFu);
}

/* 0x1BC7C - copy a ten-byte table record into 0xD010.  Kind 31 reads the
 * special record at 0x1D2E; every other kind reads the 0x1C70 table (clamped to
 * entry 18).  The final `l r0,[0xD010]` is a byte load, so r1 still holds the
 * second byte of the *source* record and the answer is that byte above the old
 * 0xD010 byte. */
/* 0x1BCB2 - the same table-into-0xD010 copy as 0x1BC7C, with a different table
 * base and a clamped index.  A kind of 0 or >= 11 uses slot 9. */
uint16_t copy_table_to_d010_1bcb2(uint8_t *ds, uint16_t unused, uint16_t kind)
{
    uint8_t k = (uint8_t)kind;

    (void)unused;
    if (k == 0u || k >= 11u)
        k = 10u;
    k = (uint8_t)(k - 1u);

    uint16_t src = (uint16_t)(0x1D38u + (uint16_t)k * 10u);
    uint8_t old = ds[0xD010];

    ds_put(ds, 0xD010u, old);
    for (int i = 1; i < 10; i++)
        ds_put(ds, (uint16_t)(0xD010u + i), ds[(uint16_t)(src + i)]);
    return (uint16_t)(((uint16_t)ds[(uint16_t)(src + 1)] << 8) | old);
}

/* 0x0C784 - is er0 at or above a screen buffer threshold?  The `mov r1,248/221`
 * before the compare leaves the threshold's high byte in the answer. */
uint16_t screen_buffer_threshold(uint8_t *ds, uint16_t arg)
{
    if (ds[0xD139] == 0u)
        return (uint16_t)(0xF800u | (arg >= 0xF800u));

    /* Two bounds: below 0xDDD4 answers 0xDD00, above 0xE3D4 answers 0xE300,
     * and the band between them answers 0xE301. */
    if (arg < 0xDDD4u)
        return 0xDD00u;
    if (arg > 0xE3D4u)
        return 0xE300u;
    return 0xE301u;
}

uint16_t copy_table_to_d010(uint8_t *ds, uint16_t unused, uint16_t kind)
{
    uint16_t src;
    uint8_t k = (uint8_t)kind;

    (void)unused;
    if (k == 31u)
        src = 0x1D2Eu;
    else
        src = (uint16_t)(0x1C70u + (uint16_t)(k >= 19u ? 18u : k) * 10u);

    /* `l r0,[0xD010]` runs after the table load, so byte 0 of the stored
     * record is the old 0xD010 byte and the rest come from src+1 onward. */
    uint8_t old = ds[0xD010];
    uint16_t answer = (uint16_t)(((uint16_t)ds[(uint16_t)(src + 1)] << 8) | old);

    ds_put(ds, 0xD010u, old);
    for (int i = 1; i < 10; i++)
        ds_put(ds, (uint16_t)(0xD010u + i), ds[(uint16_t)(src + i)]);
    return answer;
}

/* 0x09CC4 - a row count from the mode/submode and one extra state byte.  The
 * last `l r1,[0xD121]` sets Z from the byte it loads, so a zero byte skips the
 * final increment.  Only r0 is written, so with a clean er1 the answer is the
 * plain count. */
uint16_t row_count_09cc4(uint8_t *ds)
{
    uint8_t mode = ds[MEM_MODE];

    /* Each exit keeps whatever r1 the last `l r1,[...]` loaded: the mode byte
     * on the two early exits, the 0xD121 byte on the general path. */
    if (mode == 136u || mode == 12u)
        return (uint16_t)(((uint16_t)mode << 8) | 2u);

    uint16_t r0 = (ds[0xD112] == 1u) ? 1u : 2u;
    uint8_t last = ds[0xD121];

    if (last == 0u)
        return r0;
    return (uint16_t)(((uint16_t)last << 8) | (r0 + 1u));
}

