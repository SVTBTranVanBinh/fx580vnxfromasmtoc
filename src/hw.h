/* hw.h - small hardware-register and screen-state routines.
 *
 * The firmware keeps a lot of one-line helpers around the SFR space.  They are
 * not interesting on their own, but the call graph goes through them, so the
 * bottom-up translation needs them before the bigger routines can follow.
 */
#ifndef NX_HW_H
#define NX_HW_H

#include <stdint.h>

/* LCD controller registers (0xF8xx block, 0xF00x block) */
#define SFR_DISP        0xF800u   /* screen.disp   */
#define SFR_DISP1       0xF801u   /* screen.disp.1 */
#define SFR_DISP2       0xF803u
#define SFR_DISP3       0xF815u
#define SFR_SCREEN_RANGE 0xF030u  /* screen.range */
#define SFR_SCREEN_MODE 0xF031u   /* screen.mode  */
#define SFR_SCREEN_CONTRAST  0xF032u
#define SFR_SCREEN_CONTRAST2 0xF033u
#define SFR_0F810 0xF810u
#define SFR_0F811 0xF811u
#define SFR_0F812 0xF812u
#define SFR_0F050 0xF050u
#define RAM_0D114 0xD114u
#define MEM_MODE    0xD111u   /* [mode]     */
#define MEM_SUBMODE 0xD112u   /* [submode]  */

/* 0x099FC / 0x09A04 / 0x09A0C - raise one of the three 0xF81x flags. */
uint16_t set_flag_0F810(uint8_t *ds);
uint16_t set_flag_0F812(uint8_t *ds);
uint16_t set_flag_0F811(uint8_t *ds);

/* 0x09A24 - read 0xF050 (the diagnostic byte). */
uint16_t read_0F050(uint8_t *ds);

/* 0x09EE8 - clear, draw the title box and hand control to `_start`; only the
 * 0xD0F5 guard returns. */
uint16_t screen_mode_setup(uint8_t *ds);

/* 0x0A128 - drop the whole display state: 0xF800, 0xF801, 0xF803, 0xF812,
 * 0xF811 and 0xF815 all go to zero. */
uint16_t screen_state_clear(uint8_t *ds);

/* 0x0A10A - select LCD mode 22 unless the "screen updates" flag (bit 4 of
 * 0xD114) is set, then clear the display state.  The ROM saves the incoming
 * register pair with `push xr0` and restores it on the way out, so er0 comes
 * back untouched. */
uint16_t screen_mode_select(uint8_t *ds, uint16_t arg);

/* 0x0A152 - set the key input filter byte to 255. */
uint16_t key_filter_all(uint8_t *ds);

/* 0x0A532 - mark four "first time through" flags at once. */
uint16_t flags_set_four(uint8_t *ds);

/* 0x09FB8 - stop the timer/LCD interrupt source: bit 2 of 0xF014 is cleared
 * and the 0xF008..0xF00A triple is rewritten.  r1 holds 160 when the routine
 * returns, so the result word is 0xA002. */
uint16_t lcd_timer_poke(uint8_t *ds);

/* 0x094FE - turn the display flags on according to the modifier byte. */
uint16_t display_flags_from_modifiers(uint8_t *ds);

/* 0x0A878 - true in mode 136 with 0xD114 == 1. */
uint16_t mode136_screen_state(uint8_t *ds);

/* 0x09FE0 / 0x09FFE - the 0xD13A flag byte: arm it (0x88) and test bit 7. */
uint16_t flag_13A_arm(uint8_t *ds);
uint16_t flag_13A_bit7(uint8_t *ds);

/* 0x2B3E2 - 0xD0F5 == 0, 0xD113 == 0 and 0xD116 == 1. */
uint16_t state_predicate_2B3E2(uint8_t *ds);

/* 0x0E48C - mode 12 only: clear 0xD0F6, raise the four first-time flags and
 * wipe 0x0640 bytes from 0xD400.  Outside mode 12 the argument word comes back
 * untouched (the ROM tests the mode with `push lr` and returns through
 * `pop pc`, leaving er0 alone). */
uint16_t mode12_screen_reset(uint8_t *ds, uint16_t arg);

/* 0x0C3B8 - the mode 3 equivalent: 0xD0F6, the four flags, 0x0640 bytes at
 * 0xD400, then 0xD142/0xD164/0xD165.  Returns 0 in mode 3. */
uint16_t mode3_screen_reset(uint8_t *ds, uint16_t arg);

/* 0x2980E - mode 136 only: 0xD152 = 0 and 200 bytes of 0xD84C cleared. */
uint16_t mode136_row_clear(uint8_t *ds, uint16_t arg);

/* 0x246D4 - raise the four flags, then wipe 22 bytes at 0xD0F8 and 960 bytes
 * at 0xD392.  The result word is the last `memzero` return, i.e. 0xD392. */
uint16_t editor_buffers_reset(uint8_t *ds);

/* 0x0AAD4 - arm 0xD157 and adjust the two row counters when in mode 2. */
uint16_t cursor_rows_init(uint8_t *ds);

/* 0x0A9DE - the row counter limit chosen by the mode: 200 by default, 50 in
 * mode 13/14, and 49 in the 0xDDB0/0xD115 special case. */
uint16_t row_limit_for_mode(uint8_t *ds);

/* 0x0AC40 - signed distance between the argument and the byte at 0xD010. */
uint16_t distance_from_0D010(uint8_t *ds, uint16_t value);

/* 0x1D94A - high nibble of the byte at `addr`. */
uint16_t byte_high_nibble(uint8_t *ds, uint16_t addr);

/* 0x2082C / 0x20834 - high and low nibble of the state byte at 0xD118. */
uint16_t state_high_nibble(uint8_t *ds);
uint16_t state_low_nibble(uint8_t *ds);

/* 0x20860 - write the state byte at 0xD118, clear 0xD148, return 0. */
uint16_t state_store(uint8_t *ds, uint16_t value);

/* 0x14C9C - add 0xFF34 to the argument. */
uint16_t offset_minus_cc(uint8_t *ds, uint16_t value);

/* 0x09FD8 - byte at 0xF014. */
uint16_t read_0F014(uint8_t *ds);

/* 0x29978 - constant 4. */
uint16_t constant_four(uint8_t *ds);

/* 0x2083C - merge the argument's low nibble into the state byte's high one. */
uint16_t state_nibble_merge(uint8_t *ds, uint16_t value);

/* 0x28FB0 / 0x2F622 - compare the low byte against 2 and 3. */
uint16_t value_is_two(uint8_t *ds, uint16_t value);
uint16_t value_is_three(uint8_t *ds, uint16_t value);

/* 0x2F9F6 - bit 7 of the modifier byte at 0xD110. */
uint16_t modifier_bit7(uint8_t *ds, uint16_t arg);

/* 0x2FA02 - byte at 0xD156 plus the word at 0xD00C. */
uint16_t counter_sum_0D156(uint8_t *ds);

/* 0x18940 - argument * 10, based at 0xDA40. */
uint16_t record_10b_0DA40(uint8_t *ds, uint16_t value);

/* 0x09D42 - zero the five state bytes at 0xD116..0xD119 and 0xD148. */
uint16_t state_reset_0D116(uint8_t *ds);

/* 0x1826E - the ten-byte record byte for this index. */
uint16_t table_byte_10b(uint8_t *ds, uint16_t value);

/* 0x0E1D2 - swap the nibbles of the state byte at 0xD118. */
uint16_t state_swap_nibbles(uint8_t *ds);

/* 0x2084C - merge the argument into the high nibble of 0xD118, clear 0xD148. */
uint16_t state_merge_high(uint8_t *ds, uint16_t value);

/* 0x20DD2 - drop bits 7 and 3 from the modifier byte at 0xD110. */
uint16_t modifier_clear_bit7(uint8_t *ds, uint16_t arg);

/* 0x206FA - ',' or '.' for the current display state. */
uint16_t dot_or_prime(uint8_t *ds);

/* 0x180D0 - zero the result registers and answer 0. */
uint16_t clear_call_registers(uint8_t *ds);

/* 0x0CB0E / 0x1FFFE - bodies that are a bare `rt` / `pop pc`. */
uint16_t no_op_0CB0E(uint8_t *ds, uint16_t arg);
uint16_t return_immediately_1FFFE(uint8_t *ds, uint16_t arg);

/* 0x0A146 - the display enable latch at 0xF815, answered in er0's low half. */
uint16_t display_enable_set(uint8_t *ds, uint16_t arg);

/* 0x1844E - is the byte at +8 of record 0x18940(argument) above 14? */
uint16_t record_tail_over_14(uint8_t *ds, uint16_t value);

/* 0x24B10 - bit 4 of the mode byte or the submode predicate says "no setup". */
uint16_t submode_setup_gate(uint8_t *ds, uint16_t arg);

/* 0x09E4E - clear the 0xD392 state buffer and plant a kind-1 record at 0xD39C. */
uint16_t init_state_buffers(uint8_t *ds, uint16_t arg);

/* 0x1841E - initialise three consecutive variable records (kinds 0, 0, 1). */
uint16_t init_three_var_records(uint8_t *ds, uint16_t arg);

/* small predicates */
uint16_t record_pair_marker(uint8_t *ds, uint16_t ptr);
uint16_t armed_35_or_zero(uint8_t *ds, uint16_t arg);
uint16_t record_nibble_predicate(uint8_t *ds, uint16_t ptr);
uint16_t parked_word_fc1d_or_fc21(uint8_t *ds);
uint16_t parked_word_fc1c_or_fc20(uint8_t *ds);
uint16_t byte_166_167_192(uint8_t *ds, uint16_t arg);
uint16_t record_marker_25_27_28(uint8_t *ds, uint16_t ptr);
uint16_t value_fc3e_fc1e_fc1f(uint8_t *ds, uint16_t arg);
uint16_t value_165_or_fb01_fb05(uint8_t *ds, uint16_t arg);
uint16_t value_in_fc10_fc1b(uint8_t *ds, uint16_t arg);
uint16_t value_in_fc28_fc2c(uint8_t *ds, uint16_t arg);
uint16_t digit_or_sign_predicate(uint8_t *ds, uint16_t ptr);
uint16_t write_marker_2f76e(uint8_t *ds, uint16_t ptr);
uint16_t display_arm_sequence(uint8_t *ds);
uint16_t digit_byte_at_pointer(uint8_t *ds, uint16_t ptr);
uint16_t table_lookup_d0f8(uint8_t *ds, uint16_t a, uint16_t b, uint16_t c);
uint16_t triangular_from_arg(uint8_t *ds, uint16_t arg);
uint16_t code_set_2d59a(uint8_t *ds, uint16_t arg);
uint16_t code_set_25352(uint8_t *ds, uint16_t arg);
uint16_t mode_token_20e4a(uint8_t *ds);
uint16_t all_state_conditions(uint8_t *ds);
uint16_t token_window_09ada(uint8_t *ds, uint16_t arg);
uint16_t store_word_at_d060(uint8_t *ds, uint16_t a, uint16_t b, uint16_t c,
                            uint16_t d, uint16_t e, uint16_t er10);
uint16_t value_64_or_66_73(uint8_t *ds, uint16_t ptr);
uint16_t ranges_16c8e(uint8_t *ds, uint16_t arg);
uint16_t state_d019_ge_10(uint8_t *ds, uint16_t arg);
uint16_t abs_diff_d00e(uint8_t *ds, uint16_t arg);
uint16_t table_byte_ffdf(uint8_t *ds, uint16_t arg);
uint16_t copy_160_to_pointer(uint8_t *ds, uint16_t dst, uint16_t src);
uint16_t store_scaled_nibble(uint8_t *ds, uint16_t arg, uint16_t ptr);
uint16_t string_cell_count(uint8_t *ds, uint16_t ptr);
uint16_t read_word_d05e(uint8_t *ds, uint16_t a, uint16_t b, uint16_t c,
                        uint16_t d, uint16_t e, uint16_t er10);
uint16_t row_count_09cc4(uint8_t *ds);
uint16_t or_all_registers(uint8_t *ds, uint16_t e0, uint16_t e1, uint16_t e2,
                          uint16_t e3, uint16_t e4, uint16_t e5, uint16_t e6,
                          uint16_t e7);
uint16_t token_if_er8_nonzero(uint8_t *ds, uint16_t arg0, uint16_t z1,
                              uint16_t z2, uint16_t z3, uint16_t er8);
uint16_t copy_record_er6_to_er10(uint8_t *ds, uint16_t a, uint16_t b,
                                 uint16_t c, uint16_t er6, uint16_t e,
                                 uint16_t er10);
uint16_t init_record_kind_1d986(uint8_t *ds, uint16_t dst, uint16_t kind);
uint16_t copy_record_by_bytes(uint8_t *ds, uint16_t arg);
uint16_t init_record_2244a(uint8_t *ds, uint16_t arg);
uint16_t copy_table_to_d010(uint8_t *ds, uint16_t unused, uint16_t kind);

#endif
