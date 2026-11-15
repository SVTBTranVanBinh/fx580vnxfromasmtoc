-- screen_demo.lua - draw into the screen buffer with the real ROM, then print
-- the 1536 bytes of buffer 1 so they can be compared with the C translation
-- (tools/screen_demo.c) and rendered as a picture.
--
--   ( cd machine && ../machine/emulator/build/emulator \
--         model=../build/models/fx580vnx_emu script=../tools/screen_demo.lua )

local SENT = 0xA1D0
local STR_ADDR = 0xD100
local SEL_ADDR = 0xD139
local FONT_SIZE_ADDR = 0xD137
local BUF1 = 0xDDD4

-- The same call list as the C demo: frame, second frame, then two text lines.
local calls = {
    {addr = 0x08CB8, er = {0x0404, 0xA030}},          -- box_frame_thin
    {addr = 0x08CB8, er = {0x0404, 0x1030}},
    {addr = 0x08F7E, er = {0x0808, STR_ADDR}},        -- line_print
    {addr = 0x08F7E, er = {0x0840, STR_ADDR}},
}

local state = 0
local cur = 1

local function dump()
    for a = 0, 0x0600 - 1 do
        io.write(string.format("%02X%s", data[BUF1 + a],
                               ((a + 1) % 32 == 0) and "\n" or " "))
    end
end

emu:post_tick(function()
    if state == 0 then
        -- wait for the reset tick to have run
        if cpu.pc ~= 0xA1D2 or cpu.sp ~= 0xF000 or cpu.csr ~= 0 then
            return
        end
        for a = 0xD000, 0xEFFF do
            data[a] = 0
        end
        data[SEL_ADDR] = 1
        data[FONT_SIZE_ADDR] = 14
        local s = "HELLO 580VN"
        for i = 1, #s do
            data[STR_ADDR + i - 1] = string.byte(s, i)
        end
        state = 1
    end

    if state == 1 then
        local c = calls[cur]
        for i = 0, 15 do cpu["r" .. i] = 0 end
        for i = 1, #c.er do
            cpu["r" .. (2 * (i - 1))] = c.er[i] & 0xFF
            cpu["r" .. (2 * (i - 1) + 1)] = (c.er[i] >> 8) & 0xFF
        end
        cpu.sp = 0xED00
        cpu.psw = 0
        cpu.dsr = 0
        cpu.ea = 0
        cpu.lr = SENT
        cpu.lcsr = 0
        cpu.pc = c.addr & 0xFFFF
        cpu.csr = (c.addr >> 16) & 0xF
        state = 2
        return
    end

    if state == 2 then
        if cpu.pc == SENT and cpu.csr == 0 then
            cur = cur + 1
            if cur > #calls then
                dump()
                os.exit(0)
            end
            state = 1
        end
    end
end)
