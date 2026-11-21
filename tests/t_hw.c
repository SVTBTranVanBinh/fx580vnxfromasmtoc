/* t_hw.c - differential tests for the hardware-register helpers. */
#include <string.h>

#include "batch.h"
#include "harness.h"

#include "../src/hw.h"
#include "../src/runtime.h"

W0(set_flag_0F810)
W0(set_flag_0F812)
W0(set_flag_0F811)
W0(read_0F050)
W0(screen_state_clear)
W1(screen_mode_select)
W0(key_filter_all)
W0(flags_set_four)
W0(lcd_timer_poke)
W0(display_flags_from_modifiers)
W1(submode_setup_gate)
W1(init_state_buffers)
W1(init_three_var_records)
W1(record_pair_marker)
W1(armed_35_or_zero)
W1(record_nibble_predicate)
W0(parked_word_fc1d_or_fc21)
W0(parked_word_fc1c_or_fc20)
W1(byte_166_167_192)
W1(record_marker_25_27_28)
W1(value_fc3e_fc1e_fc1f)
W1(value_165_or_fb01_fb05)
W1(value_in_fc10_fc1b)
W1(value_in_fc28_fc2c)
W1(digit_or_sign_predicate)
W1(write_marker_2f76e)
W0(display_arm_sequence)
W1(digit_byte_at_pointer)
W3(table_lookup_d0f8)
W1(triangular_from_arg)
W0(mode136_screen_state)
W0(cursor_rows_init)
W0(row_limit_for_mode)
W1(distance_from_0D010)
W1(mode_code_from_flag)
W1(mode12_screen_reset)
W1(mode3_screen_reset)
W1(mode136_row_clear)
W0(editor_buffers_reset)
W0(flag_13A_arm)
W0(flag_13A_bit7)
W0(state_predicate_2B3E2)
W1(byte_high_nibble)
W0(state_high_nibble)
W0(state_low_nibble)
W1(state_store)
W1(offset_minus_cc)
W0(read_0F014)
W0(constant_four)
W1(state_nibble_merge)
W1(value_is_two)
W1(value_is_three)
W1(modifier_bit7)
W0(counter_sum_0D156)
W1(record_10b_0DA40)
W0(state_reset_0D116)
W1(table_byte_10b)

W0(state_swap_nibbles)
W1(state_merge_high)
W1(modifier_clear_bit7)
W0(dot_or_prime)
W0(clear_call_registers)
W1(no_op_0CB0E)
W1(display_enable_set)
W1(record_tail_over_14)

/* 0x1FFFE is a bare `pop pc`.  Called through `bl` it pops the return address
 * the call pushed, so to run it the harness has to put one on the stack - and
 * `pop pc` pops *four* bytes (pc then csr), so the word after the sentinel has
 * to be a zero csr or the core keeps running in segment garbage.  This is why
 * the case cannot go through the batch runner: no shape can describe it. */
static void test_pop_pc_return(void)
{
    static const uint16_t args[] = {0, 1, 0x1234, 0xFFFF};

    for (size_t i = 0; i < sizeof(args) / sizeof(args[0]); i++) {
        random_image((uint32_t)(0xC7000 + i));
        Args a = {.er = {args[i], 0}, .stk = {NX8_SENTINEL, 0}, .nstk = 2};
        uint16_t rom_ret = run_rom(0x1FFFE, &a, "return_immediately_1FFFE");
        begin_c();
        uint16_t c_ret = return_immediately_1FFFE(after_c, args[i]);
        check_ret("return_immediately_1FFFE", rom_ret, c_ret);
        check_mem_all("return_immediately_1FFFE");
    }
}

/* 0x0BEFA and 0x0CA34 test the sixteen-bit field at 0xD10E, so sweep the four
 * codes they name plus neighbours on both sides. */
static void test_d10e_token_tests(void)
{
    static const uint16_t words[] = {0xFC1C, 0xFC1D, 0xFC1E, 0xFC1F, 0xFC20,
                                     0xFC21, 0xFC22, 0xFC23, 0xFC24, 0x0000,
                                     0xFFFF};

    for (size_t w = 0; w < sizeof(words) / sizeof(words[0]); w++) {
        for (unsigned arg = 0; arg < 2; arg++) {
            uint16_t a = arg ? 0x5A00 : 0x0000;

            random_image(0xF8000u + (unsigned)(w * 4 + arg));
            image[0xD10E] = (uint8_t)(words[w] & 0xFF);
            image[0xD10F] = (uint8_t)(words[w] >> 8);
            Args args = {.er = {a, 0}, .nstk = 0};

            uint16_t rom_ret = run_rom(0x0BEFA, &args,
                                       "token_at_d10e_is_1f_23");
            begin_c();
            check_ret("token_at_d10e_is_1f_23", rom_ret,
                      token_at_d10e_is_1f_23(after_c, a));
            check_mem_all("token_at_d10e_is_1f_23");

            rom_ret = run_rom(0x0CA34, &args, "token_at_d10e_is_1e_22");
            begin_c();
            check_ret("token_at_d10e_is_1e_22", rom_ret,
                      token_at_d10e_is_1e_22(after_c, a));
            check_mem_all("token_at_d10e_is_1e_22");
        }
    }
}

/* 0x2AE00 / 0x2B226 name modes 12 and 74, so walk those two plus the values
 * either side of them. */
static void test_more_mode_predicates(void)
{
    static const uint8_t modes[] = {0, 11, 12, 13, 73, 74, 75, 255};
    static const uint8_t subs[] = {0, 1, 2};

    for (size_t m = 0; m < sizeof(modes) / sizeof(modes[0]); m++) {
        for (size_t s = 0; s < sizeof(subs) / sizeof(subs[0]); s++) {
            for (unsigned arg = 0; arg < 2; arg++) {
                uint16_t a = arg ? 0x7700 : 0x0000;

                random_image(0xF9000u + (unsigned)(m * 8 + s + arg));
                image[0xD111] = modes[m];
                image[0xD114] = subs[s];
                Args args = {.er = {a, 0}, .nstk = 0};

                uint16_t rom_ret = run_rom(0x2AE00, &args, "mode_is_12_1");
                begin_c();
                check_ret("mode_is_12_1", rom_ret, mode_is_12_1(after_c, a));
                check_mem_all("mode_is_12_1");

                rom_ret = run_rom(0x2B226, &args, "mode_is_74_1");
                begin_c();
                check_ret("mode_is_74_1", rom_ret, mode_is_74_1(after_c, a));
                check_mem_all("mode_is_74_1");
            }
        }
    }
}

/* 0x2F6FC reads one byte through a pointer and keeps its high nibble. */
static void test_nibble_hi_is_2_or_8(void)
{
    static const uint16_t ptrs[] = {0xD000, 0xD001, 0xDFF0};
    static const uint8_t bytes[] = {0x00, 0x1F, 0x20, 0x2F, 0x30, 0x7F, 0x80,
                                    0x8F, 0x90, 0xFF};

    for (size_t b = 0; b < sizeof(bytes) / sizeof(bytes[0]); b++) {
        for (size_t a = 0; a < sizeof(ptrs) / sizeof(ptrs[0]); a++) {
            random_image(0xFA000u + (unsigned)(b * 8 + a));
            image[ptrs[a]] = bytes[b];
            Args args = {.er = {ptrs[a], 0}, .nstk = 0};
            uint16_t rom_ret = run_rom(0x2F6FC, &args,
                                       "nibble_hi_is_2_or_8");
            begin_c();
            check_ret("nibble_hi_is_2_or_8", rom_ret,
                      nibble_hi_is_2_or_8(after_c, ptrs[a]));
            check_mem_all("nibble_hi_is_2_or_8");
        }
    }
}

/* 0x0AF1A, 0x0C34E, 0x2AF28 and 0x2D4D8 are byte predicates that answer in r0
 * and leave r1 alone, so each one is run over the interesting values of its
 * state byte and a high half that has to survive. */
struct pred_case {
    uint32_t addr;
    const char *name;
    uint16_t (*fn)(uint8_t *, uint16_t);
    uint16_t cell;
};

static void one_predicate(const struct pred_case *c, uint8_t value,
                          uint16_t arg, uint8_t other)
{
    static const uint16_t other_cells[] = {0xD114, 0xD116};

    random_image(0xF6000u + (unsigned)value * 16u + (unsigned)other);
    /* the cell under test gets the sweept value, the other two bytes (0xD114
     * and 0xD116) get a value that is deliberately not always equal to it */
    image[c->cell] = value;
    image[other_cells[0]] = other;
    image[other_cells[1]] = other;
    Args args = {.er = {arg, 0}, .nstk = 0};
    uint16_t rom_ret = run_rom(c->addr, &args, c->name);
    begin_c();
    check_ret(c->name, rom_ret, c->fn(after_c, arg));
    check_mem_all(c->name);
}

static void test_state_predicates(void)
{
    static const uint16_t args[] = {0, 0x0023, 0x2300, 0xFFFF};
    static const uint8_t others[] = {0, 1, 2};
    static const struct pred_case cases[] = {
        {0x0AF1A, "is_zero_or_35", is_zero_or_35, 0xD015},
        {0x0C34E, "submode_is_19_or_20", submode_is_19_or_20, 0xD114},
        {0x2AF28, "state_is_1_or_3", state_is_1_or_3, 0xD015},
        {0x2D4D8, "flag4_and_substate_1", flag4_and_substate_1, 0xD114},
    };
    static const uint8_t values[] = {0, 1, 2, 3, 19, 20, 21, 0x10, 0x11, 0xFF};

    for (size_t k = 0; k < sizeof(cases) / sizeof(cases[0]); k++) {
        for (size_t v = 0; v < sizeof(values) / sizeof(values[0]); v++) {
            for (size_t a = 0; a < sizeof(args) / sizeof(args[0]); a++) {
                for (size_t o = 0; o < sizeof(others); o++)
                    one_predicate(&cases[k], values[v], args[a], others[o]);
            }
        }
    }
}

/* 0x286A0 takes a byte and needs the caller's high half back untouched, so the
 * adapter keeps the word and lets the C body slice it. */
static uint16_t a_mode_state_update(uint8_t *ds, const uint16_t *e,
                                    const uint16_t *stk)
{
    (void)stk;
    return mode_state_update(ds, e[0]);
}

/* 0x2F78C - two or four, with the caller's high half carried through both
 * answers, so the sweep has to vary the high byte too. */
static void test_value_is_two_or_four(void)
{
    static const uint16_t highs[] = {0x0000, 0x0100, 0xD200, 0xFF00};
    static const uint8_t lows[] = {0, 1, 2, 3, 4, 5, 0xFE, 0xFF};

    for (size_t h = 0; h < sizeof(highs) / sizeof(highs[0]); h++) {
        for (size_t l = 0; l < sizeof(lows) / sizeof(lows[0]); l++) {
            uint16_t arg = (uint16_t)(highs[h] | lows[l]);

            random_image(0x2F78Cu + (uint32_t)(h * 31u + l));
            Args a = {.er = {arg}, .nstk = 0};
            uint16_t r = run_rom(0x2F78C, &a, "value_is_two_or_four");
            begin_c();
            uint16_t c = value_is_two_or_four(after_c, arg);
            check_ret("value_is_two_or_four", r, c);
            check_mem_all("value_is_two_or_four");
        }
    }
}

/* 0x0A546 - bit 6 of 0xD116 or an empty string at 0xD180.  The length's high
 * byte survives into the answer, so the strings include one long enough to have
 * a non-zero high half - a 300-byte string is what tells a `mov er0, er0`
 * sixteen-bit test from a byte test. */
static void test_flag_or_empty_0D180(void)
{
    static const uint8_t bits[] = {0x00, 0x40, 0x80, 0xC0};
    static const size_t lengths[] = {0, 1, 2, 5, 300};

    for (size_t b = 0; b < sizeof(bits) / sizeof(bits[0]); b++) {
        for (size_t l = 0; l < sizeof(lengths) / sizeof(lengths[0]); l++) {
            random_image(0x0A546u + (uint32_t)(b * 13u + l));
            image[0xD116] = bits[b];
            for (size_t i = 0; i < lengths[l]; i++)
                image[(uint16_t)(0xD180 + i)] = (uint8_t)(0x41 + i % 26);
            image[(uint16_t)(0xD180 + lengths[l])] = 0;

            Args a = {.nstk = 0};
            uint16_t r = run_rom(0x0A546, &a, "flag_or_empty_0D180");
            begin_c();
            uint16_t c = flag_or_empty_0D180(after_c);
            check_ret("flag_or_empty_0D180", r, c);
            check_mem_all("flag_or_empty_0D180");
        }
    }
}

/* 0x2F710 - the three accepts are a window 65..73, the single code 64 and the
 * single code 75; the sweep walks the whole byte plus a spread of high bytes so
 * the sixteen-bit compares are tested against a non-zero upper half too. */
static void test_code_is_64_to_73_or_75(void)
{
    static const uint16_t highs[] = {0x0000, 0x0100, 0x4100, 0xFF00};
    static const uint8_t lows[] = {0x00, 0x3F, 0x40, 0x41, 0x42, 0x49, 0x4A,
                                   0x4B, 0x4C, 0x4D, 0x80, 0xFF};

    for (size_t h = 0; h < sizeof(highs) / sizeof(highs[0]); h++) {
        for (size_t l = 0; l < sizeof(lows) / sizeof(lows[0]); l++) {
            uint16_t arg = (uint16_t)(highs[h] | lows[l]);

            random_image(0x2F710u + (uint32_t)(h * 29u + l));
            Args a = {.er = {arg}, .nstk = 0};
            uint16_t r = run_rom(0x2F710, &a, "code_is_64_to_73_or_75");
            begin_c();
            uint16_t c = code_is_64_to_73_or_75(after_c, arg);
            check_ret("code_is_64_to_73_or_75", r, c);
            check_mem_all("code_is_64_to_73_or_75");
        }
    }
}

/* 0x20CF2 - mode 9 delegates to the per-submode table byte, mode 6 accepts
 * submodes 1..3 and everything else needs bit 7 plus submode 2.  The sweep
 * covers every mode byte, the interesting submodes, and the argument's high
 * half, since two of the three paths pass it straight through. */
static void test_submode_needs_setup(void)
{
    static const uint8_t modes[] = {0, 1, 6, 7, 9, 10, 0x80, 0x86, 0x89, 0xFF};
    static const uint8_t subs[] = {0, 1, 2, 3, 4, 9, 0xFF};
    static const uint8_t rows[] = {1, 3, 4, 5, 8};
    static const uint8_t skews[] = {0, 0xFF};
    static const uint16_t highs[] = {0x0000, 0x0100, 0xD700};

    for (size_t m = 0; m < sizeof(modes) / sizeof(modes[0]); m++) {
        for (size_t s = 0; s < sizeof(subs) / sizeof(subs[0]); s++) {
            for (size_t r = 0; r < sizeof(rows) / sizeof(rows[0]); r++) {
                for (size_t k = 0; k < sizeof(skews) / sizeof(skews[0]); k++) {
                    for (size_t h = 0; h < sizeof(highs) / sizeof(highs[0]);
                         h++) {
                        uint32_t seed = 0x20CF2u + (uint32_t)(m * 977 + s * 53 +
                                                              r * 7 + k * 3 + h);

                        random_image(seed);
                        image[0xD114] = modes[m];
                        image[0xD115] = subs[s];
                        image[0xD112] = rows[r];
                        image[0xD14F] = skews[k];
                        Args a = {.er = {highs[h]}, .nstk = 0};
                        uint16_t rom = run_rom(0x20CF2, &a,
                                              "submode_needs_setup");
                        begin_c();
                        uint16_t c = submode_needs_setup(after_c, highs[h]);
                        check_ret("submode_needs_setup", rom, c);
                        check_mem_all("submode_needs_setup");
                    }
                }
            }
        }
    }
}

/* 0x09EE8 is the only routine here that does not return: it ends in `b _start`,
 * a jump into the main loop, so the harness runs the ROM up to the address the
 * jump lands on and compares what the body wrote before it (`run_rom_until`).
 *
 * Its callees also read stack slots they never wrote - the 2[sp] fallback in
 * 0x09846 and the holes the frames leave - so a stack full of random bytes
 * sends the ROM off through a `pop pc` that was never pushed (the trace ends in
 * "code fetch from unmapped segment" at an address that is really data).  The
 * test pins 0xEC00..0xEFFF and the SFR block to zero, which is what the machine
 * looks like at boot, and sweeps the rest of the data space. */
static void test_screen_mode_setup(void)
{
    for (unsigned t = 0; t < 8; t++) {
        random_image(0xD1B000u + t);
        memset(image + 0xEC00, 0, 0x0400);
        memset(image + 0xF000, 0, 0x0100);
        image[0xD0F5] = 0;
        Args args = {.nstk = 0};

        if (!run_rom_until(0x09EE8, &args, "screen_mode_setup", 0x0A1CE))
            continue;
        begin_c();
        (void)screen_mode_setup(after_c);
        check_mem_all("screen_mode_setup");
        check_mem("screen_mode_setup", 0xF000, 0xF100);
    }

    /* the 0xD0F5 guard is the only way the routine answers at all */
    {
        static const uint8_t flags[] = {1, 2, 0x80, 0xFF};

        for (size_t f = 0; f < sizeof(flags); f++) {
            random_image(0xD1B100u + (unsigned)f);
            image[0xD0F5] = flags[f];
            Args args = {.nstk = 0};
            uint16_t rom_ret = run_rom(0x09EE8, &args, "screen_mode_setup");
            begin_c();
            uint16_t c_ret = screen_mode_setup(after_c);
            check_ret("screen_mode_setup", rom_ret, c_ret);
            check_mem_all("screen_mode_setup");
        }
    }
}

void test_hw(void)
{
    test_submode_needs_setup();
    test_code_is_64_to_73_or_75();
    test_value_is_two_or_four();
    test_flag_or_empty_0D180();
    test_state_predicates();
    test_d10e_token_tests();
    test_more_mode_predicates();
    test_nibble_hi_is_2_or_8();

    BATCH(
        {"set_flag_0F810", 0x099FC, "", a_set_flag_0F810, 0},
        {"set_flag_0F812", 0x09A04, "", a_set_flag_0F812, 0},
        {"set_flag_0F811", 0x09A0C, "", a_set_flag_0F811, 0},
        {"read_0F050", 0x09A24, "", a_read_0F050, 0},
        {"screen_state_clear", 0x0A128, "", a_screen_state_clear, 0},
        {"screen_mode_select", 0x0A10A, "w", a_screen_mode_select, 0},
        {"key_filter_all", 0x0A152, "", a_key_filter_all, 0},
        {"flags_set_four", 0x0A532, "", a_flags_set_four, 0},
        {"lcd_timer_poke", 0x09FB8, "", a_lcd_timer_poke, 0},
        {"display_flags", 0x094FE, "", a_display_flags_from_modifiers, 0},
        {"mode136_screen_state", 0x0A878, "", a_mode136_screen_state, 0},
        {"cursor_rows_init", 0x0AAD4, "", a_cursor_rows_init, 0},
        {"row_limit_for_mode", 0x0A9DE, "", a_row_limit_for_mode, 0},
        {"distance_from_0D010", 0x0AC40, "b", a_distance_from_0D010, 0},
        {"mode12_screen_reset", 0x0E48C, "w", a_mode12_screen_reset, 0},
        {"mode3_screen_reset", 0x0C3B8, "w", a_mode3_screen_reset, 0},
        {"mode136_row_clear", 0x2980E, "w", a_mode136_row_clear, 0},
        {"editor_buffers_reset", 0x246D4, "", a_editor_buffers_reset, 0},
        {"flag_13A_arm", 0x09FE0, "", a_flag_13A_arm, 0},
        {"flag_13A_bit7", 0x09FFE, "", a_flag_13A_bit7, 0},
        {"state_predicate_2B3E2", 0x2B3E2, "", a_state_predicate_2B3E2, 0},
        {"byte_high_nibble", 0x1D94A, "p", a_byte_high_nibble, 0},
        {"state_high_nibble", 0x2082C, "", a_state_high_nibble, 0},
        {"state_low_nibble", 0x20834, "", a_state_low_nibble, 0},
        {"state_store", 0x20860, "b", a_state_store, 0},
        {"offset_minus_cc", 0x14C9C, "w", a_offset_minus_cc, 0},
        {"read_0F014", 0x09FD8, "", a_read_0F014, 0},
        {"constant_four", 0x29978, "", a_constant_four, 0},
        {"state_nibble_merge", 0x2083C, "b", a_state_nibble_merge, 0},
        {"value_is_two", 0x28FB0, "b", a_value_is_two, 0},
        {"value_is_three", 0x2F622, "b", a_value_is_three, 0},
        {"modifier_bit7", 0x2F9F6, "w", a_modifier_bit7, 0},
        {"counter_sum_0D156", 0x2FA02, "", a_counter_sum_0D156, 0},
        {"record_10b_0DA40", 0x18940, "w", a_record_10b_0DA40, 0},
        {"state_reset_0D116", 0x09D42, "", a_state_reset_0D116, 0},
        {"mode_code_from_flag", 0x2ACBE, "w", a_mode_code_from_flag, 0},
        {"mode_state_update", 0x286A0, "wb", a_mode_state_update, 0},
        {"table_byte_10b", 0x1826E, "w", a_table_byte_10b, 0},
        {"state_swap_nibbles", 0x0E1D2, "", a_state_swap_nibbles, 0},
        {"state_merge_high", 0x2084C, "b", a_state_merge_high, 0},
        {"modifier_clear_bit7", 0x20DD2, "b", a_modifier_clear_bit7, 0},
        {"dot_or_prime", 0x206FA, "", a_dot_or_prime, 0},
        {"clear_call_registers", 0x180D0, "", a_clear_call_registers, 0},
        {"no_op_0CB0E", 0x0CB0E, "w", a_no_op_0CB0E, 0},
        {"display_enable_set", 0x0A146, "w", a_display_enable_set, 0},
        {"record_tail_over_14", 0x1844E, "w", a_record_tail_over_14, 0},
        {"submode_setup_gate", 0x24B10, "w", a_submode_setup_gate, 0},
        {"init_state_buffers", 0x09E4E, "w", a_init_state_buffers, 0},
        {"init_three_var_records", 0x1841E, "w", a_init_three_var_records, 0},
        {"record_pair_marker", 0x24F26, "p", a_record_pair_marker, 0},
        {"armed_35_or_zero", 0x2847C, "w", a_armed_35_or_zero, 0},
        {"record_nibble_predicate", 0x1DD44, "p", a_record_nibble_predicate, 0},
        {"parked_word_fc1d_fc21", 0x24BBE, "", a_parked_word_fc1d_or_fc21, 0},
        {"parked_word_fc1c_fc20", 0x0D64E, "", a_parked_word_fc1c_or_fc20, 0},
        {"byte_166_167_192", 0x0D5E0, "w", a_byte_166_167_192, 0},
        {"record_marker_25_27_28", 0x0C7B0, "p", a_record_marker_25_27_28, 0},
        {"value_fc3e_fc1e_fc1f", 0x25010, "w", a_value_fc3e_fc1e_fc1f, 0},
        {"value_165_or_fb01_fb05", 0x2F7E4, "w", a_value_165_or_fb01_fb05, 0},
        {"value_in_fc10_fc1b", 0x1FFE6, "w", a_value_in_fc10_fc1b, 0},
        {"value_in_fc28_fc2c", 0x2B986, "w", a_value_in_fc28_fc2c, 0},
        {"digit_or_sign_predicate", 0x15E36, "p", a_digit_or_sign_predicate, 0},
        {"write_marker_2f76e", 0x2F76E, "p", a_write_marker_2f76e, 0},
        {"display_arm_sequence", 0x0A0DA, "", a_display_arm_sequence, 0},
        {"digit_byte_at_pointer", 0x1F41E, "p", a_digit_byte_at_pointer, 0},
        {"table_lookup_d0f8", 0x13B5A, "zzw", a_table_lookup_d0f8, 0},
        {"triangular_from_arg", 0x0D80C, "w", a_triangular_from_arg, 0},
    );

    test_pop_pc_return();
    test_screen_mode_setup();
}
