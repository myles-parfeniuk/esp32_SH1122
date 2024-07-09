#include "SH1122Oled.hpp"

sh1122_oled_cfg_t SH1122Oled::default_oled_cfg;
uint8_t SH1122Oled::frame_buffer[FRAME_BUFFER_LENGTH];
SH1122Oled::sh1122_oled_font_info_t SH1122Oled::font_info;
SH1122Oled::FontDirection SH1122Oled::font_dir = SH1122Oled::FontDirection::left_to_right;

SH1122Oled::SH1122Oled(sh1122_oled_cfg_t oled_cfg)
    : oled_cfg(oled_cfg)
{

    // set-up data command pin and rst pin
    gpio_config_t io_dc_rst_cs_cfg;

    io_dc_rst_cs_cfg.pin_bit_mask = (1 << oled_cfg.io_rst) | (1 << oled_cfg.io_dc) | (1 << oled_cfg.io_cs);
    io_dc_rst_cs_cfg.mode = GPIO_MODE_OUTPUT;
    io_dc_rst_cs_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_dc_rst_cs_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    io_dc_rst_cs_cfg.intr_type = GPIO_INTR_DISABLE;

    gpio_config(&io_dc_rst_cs_cfg);

    gpio_set_level(oled_cfg.io_rst, 1); // put rst pin initially high*/

    spi_bus_cfg.mosi_io_num = oled_cfg.io_mosi;
    spi_bus_cfg.miso_io_num = GPIO_NUM_NC;
    spi_bus_cfg.sclk_io_num = oled_cfg.io_sclk;
    spi_bus_cfg.quadhd_io_num = GPIO_NUM_NC;
    spi_bus_cfg.quadwp_io_num = GPIO_NUM_NC;
    spi_bus_cfg.max_transfer_sz = FRAME_CHUNK_3_LENGTH;
    spi_bus_cfg.flags = SPICOMMON_BUSFLAG_MASTER;

    oled_interface_cfg.address_bits = 0;
    oled_interface_cfg.dummy_bits = 0;
    oled_interface_cfg.command_bits = 0;
    oled_interface_cfg.mode = 0;
    oled_interface_cfg.clock_source = SPI_CLK_SRC_DEFAULT;
    oled_interface_cfg.clock_speed_hz = SPI_CLK_SPEED_HZ;
    oled_interface_cfg.spics_io_num = GPIO_NUM_NC;
    oled_interface_cfg.queue_size = SPI_INTERRUPT_MODE_QUEUE_SIZE;
    oled_interface_cfg.pre_cb = NULL;
    oled_interface_cfg.flags = SPI_DEVICE_3WIRE | SPI_DEVICE_HALFDUPLEX;

    ESP_ERROR_CHECK(spi_bus_initialize(oled_cfg.spi_peripheral, &spi_bus_cfg, oled_cfg.dma_cha));
    ESP_ERROR_CHECK(spi_bus_add_device(oled_cfg.spi_peripheral, &oled_interface_cfg, &spi_hdl));

    default_init();
}

void SH1122Oled::update_screen()
{
    gpio_set_level(oled_cfg.io_cs, 0);

    // update display in 3 chunks, SPI max DMA size is 4096 bytes at a time, but display buffer is 8192
    send_data(frame_buffer, FRAME_CHUNK_1_LENGTH);
    send_data(frame_buffer + (FRAME_CHUNK_1_LENGTH), FRAME_CHUNK_2_LENGTH);
    send_data(frame_buffer + (FRAME_CHUNK_1_LENGTH + FRAME_CHUNK_2_LENGTH), FRAME_CHUNK_3_LENGTH);

    gpio_set_level(oled_cfg.io_cs, 1);
}

void SH1122Oled::set_pixel(uint16_t x, uint16_t y, PixelIntensity intensity)
{

    int16_t x_it = 0;
    int16_t y_it = 0;
    int16_t high_byte = 0;

    if ((x < WIDTH) && (y < HEIGHT))
    {
        if (x != 0)
        {
            x_it = x / 2;
            high_byte = x % 2;
        }

        if (y != 0)
            y_it = (y * WIDTH) / 2;

        uint8_t* pixel = (frame_buffer + x_it + y_it);

        if (high_byte == 1)
            *pixel = ((uint8_t) intensity & 0x0F) | (*pixel & 0xF0);
        else
            *pixel = ((uint8_t) intensity & 0xF0) | (*pixel & 0x0F);
    }
}

void SH1122Oled::draw_line(int16_t x_1, int16_t y_1, int16_t x_2, int16_t y_2, PixelIntensity intensity)
{
    const int16_t delta_x = abs(x_2 - x_1);
    const int16_t delta_y = abs(y_2 - y_1);
    const int16_t sign_x = x_1 < x_2 ? 1 : -1;
    const int16_t sign_y = y_1 < y_2 ? 1 : -1;
    int16_t error = delta_x - delta_y;

    set_pixel(x_2, y_2, intensity);

    while (x_1 != x_2 || y_1 != y_2)
    {
        set_pixel(x_1, y_1, intensity);

        const int16_t error_2 = error * 2;

        if (error_2 > -delta_y)
        {
            error -= delta_y;
            x_1 += sign_x;
        }
        if (error_2 < delta_x)
        {
            error += delta_x;
            y_1 += sign_y;
        }
    }
}

void SH1122Oled::draw_rectangle_frame(int16_t x_1, int16_t y_1, int16_t width, int16_t height, int16_t thickness, PixelIntensity intensity)
{
    for (int i = 0; i < thickness; i++)
        draw_line(x_1 + i, y_1 + thickness, x_1 + i, (y_1 + height - 1) - thickness, intensity);

    for (int i = 0; i < thickness; i++)
        draw_line((x_1 + width - 1) - i, y_1 + thickness, (x_1 + width - 1) - i, (y_1 + height - 1) - thickness, intensity);

    for (int i = 0; i < thickness; i++)
        draw_line(x_1, y_1 + i, (x_1 + width - 1), y_1 + i, intensity);

    for (int i = 0; i < thickness; i++)
        draw_line(x_1, (y_1 + height - 1) - i, (x_1 + width - 1), (y_1 + height - 1) - i, intensity);
}

void SH1122Oled::draw_rectangle(int16_t x_1, int16_t y_1, int16_t width, int16_t height, PixelIntensity intensity)
{
    for (uint16_t j = 0; j < height; j++)
    {
        for (uint16_t i = 0; i < width; i++)
            set_pixel((x_1 + i), (y_1 + j), intensity);
    }
}

uint16_t SH1122Oled::draw_glyph(uint16_t x, uint16_t y, PixelIntensity intensity, uint16_t encoding)
{
    const uint8_t* glyph_ptr = NULL;
    sh1122_oled_font_decode_t decode;
    uint16_t dx = 0;

    // set up the decode structure
    decode.target_x = x;

    switch (font_dir)
    {
    case FontDirection::left_to_right:
        y += font_info.ascent_A;
        break;

    case FontDirection::top_to_bottom:
        break;

    case FontDirection::right_to_left:
        break;

    case FontDirection::bottom_to_top:
        decode.target_x += font_info.ascent_A;
        break;
    }

    decode.target_y = y;
    decode.fg_intensity = (uint8_t) intensity;

    glyph_ptr = NULL;

    if (encoding != 0x0ffff)
    {
        glyph_ptr = font_get_glyph_data(encoding);
        if (glyph_ptr != NULL)
        {
            font_setup_glyph_decode(&decode, glyph_ptr);
            dx = font_decode_glyph(&decode, glyph_ptr);
        }
    }

    return dx;
}

uint16_t SH1122Oled::draw_string(uint16_t x, uint16_t y, PixelIntensity intensity, const char* format, ...)
{
    uint16_t delta = 0;
    uint16_t encoding = 0;
    uint16_t sum = 0;
    char* buffer = nullptr;
    uint8_t* str = nullptr;
    va_list args;
    uint16_t size;

    va_start(args, format);
    size = vsnprintf(nullptr, 0, format, args) + 1;
    va_end(args);

    buffer = new char[size];

    if (!buffer)
        return 0;

    va_start(args, format);
    vsnprintf(buffer, size, format, args);
    va_end(args);

    str = (uint8_t*) buffer;

    while (1)
    {
        encoding = get_ascii_next(*str); // check to ensure character is not null or new line (end of string)

        if (encoding == 0x0ffff)
            break;

        if (encoding != 0x0fffe)
        {
            delta = draw_glyph(x, y, intensity, encoding);

            switch (font_dir)
            {
            case FontDirection::left_to_right:
                x += delta;
                break;

            case FontDirection::top_to_bottom:
                y += delta;
                break;

            case FontDirection::right_to_left:
                x -= delta;
                break;

            case FontDirection::bottom_to_top:
                y -= delta;
                break;
            }

            sum += delta;
        }

        str++;
    }

    delete[] buffer;

    return sum;
}

uint16_t SH1122Oled::font_get_glyph_width(uint16_t encoding)
{
    const uint8_t* glyph_data = font_get_glyph_data(encoding);
    sh1122_oled_font_decode_t decode;

    if (glyph_data == NULL)
        return 0;

    font_setup_glyph_decode(&decode, glyph_data);
    font_decode_get_signed_bits(&decode, font_info.bits_per_char_x);
    font_decode_get_signed_bits(&decode, font_info.bits_per_char_y);

    return font_decode_get_signed_bits(&decode, font_info.bits_per_delta_x);
}

uint16_t SH1122Oled::font_get_glyph_height(uint16_t encoding)
{
    const uint8_t* glyph_data = font_get_glyph_data(encoding);
    sh1122_oled_font_decode_t decode;

    if (glyph_data == NULL)
        return 0;

    font_setup_glyph_decode(&decode, glyph_data);

    return decode.glyph_height;
    return 0;
}

uint16_t SH1122Oled::font_get_glyph_width(sh1122_oled_font_decode_t* decode, uint16_t encoding)
{
    const uint8_t* glyph_data = font_get_glyph_data(encoding);

    if (glyph_data == NULL)
        return 0;

    font_setup_glyph_decode(decode, glyph_data);
    decode->glyph_x_offset = font_decode_get_signed_bits(decode, font_info.bits_per_char_x);
    font_decode_get_signed_bits(decode, font_info.bits_per_char_y);

    return font_decode_get_signed_bits(decode, font_info.bits_per_delta_x);
}

uint16_t SH1122Oled::font_get_string_width(const char* format, ...)
{
    uint16_t encoding;
    uint16_t width;
    uint16_t dx;
    int8_t initial_x_offset = -64;
    sh1122_oled_font_decode_t decode;
    char* buffer = nullptr;
    uint8_t* str = nullptr;
    va_list args;
    uint16_t size;

    va_start(args, format);
    size = vsnprintf(nullptr, 0, format, args) + 1;
    va_end(args);

    buffer = new char[size];

    if (!buffer)
        return 0;

    va_start(args, format);
    vsnprintf(buffer, size, format, args);
    va_end(args);

    str = (uint8_t*) buffer;

    width = 0;
    dx = 0;

    while (1)
    {
        encoding = get_ascii_next(*str); // get next character

        if (encoding == 0x0ffff)
            break;
        if (encoding != 0x0fffe)
        {
            dx = font_get_glyph_width(&decode, encoding);
            if (initial_x_offset == -64)
                initial_x_offset = decode.glyph_x_offset;

            width += dx;
        }
        str++;
    }

    if (decode.glyph_width != 0)
    {
        width -= dx;
        width += decode.glyph_width;
        width += decode.glyph_x_offset;
        if (initial_x_offset > 0)
            width += initial_x_offset;
    }

    delete[] buffer;

    return width;
}

uint16_t SH1122Oled::font_get_string_height(const char* format, ...)
{
    char* buffer = nullptr;
    uint8_t* str = nullptr;
    va_list args;
    uint16_t size;
    uint16_t current_height = 0;
    uint16_t max_height = 0;

    va_start(args, format);
    size = vsnprintf(nullptr, 0, format, args) + 1;
    va_end(args);

    buffer = new char[size];

    if (!buffer)
        return 0;

    va_start(args, format);
    vsnprintf(buffer, size, format, args);
    va_end(args);

    str = (uint8_t*) buffer;

    while (*str != '\0')
    {
        current_height = font_get_glyph_height(*str);
        if (current_height > max_height)
            max_height = current_height;

        str++;
    }

    delete[] buffer;
    return max_height;
}

uint16_t SH1122Oled::font_get_string_center_x(const char* str)
{
    uint16_t str_width = font_get_string_width(str);
    return (WIDTH - str_width) / 2;
}

uint16_t SH1122Oled::font_get_string_center_y(const char* str)
{
    uint16_t max_char_height = font_get_string_height(str);
    return (HEIGHT - max_char_height) / 2;
}

void SH1122Oled::load_font(const uint8_t* font)
{
    font_info.font = font;

    font_info.glyph_cnt = font_lookup_table_read_char(font, 0);
    font_info.bbx_mode = font_lookup_table_read_char(font, 1);
    font_info.bits_per_0 = font_lookup_table_read_char(font, 2);
    font_info.bits_per_1 = font_lookup_table_read_char(font, 3);

    font_info.bits_per_char_width = font_lookup_table_read_char(font, 4);
    font_info.bits_per_char_height = font_lookup_table_read_char(font, 5);
    font_info.bits_per_char_x = font_lookup_table_read_char(font, 6);
    font_info.bits_per_char_y = font_lookup_table_read_char(font, 7);
    font_info.bits_per_delta_x = font_lookup_table_read_char(font, 8);

    font_info.max_char_width = font_lookup_table_read_char(font, 9);
    font_info.max_char_height = font_lookup_table_read_char(font, 10);
    font_info.x_offset = font_lookup_table_read_char(font, 11);
    font_info.y_offset = font_lookup_table_read_char(font, 12);

    font_info.ascent_A = font_lookup_table_read_char(font, 13);  // capital a usually the highest pixels of any characters
    font_info.descent_g = font_lookup_table_read_char(font, 14); // lower case usually has the lowest pixels of any characters
    font_info.ascent_para = font_lookup_table_read_char(font, 15);
    font_info.descent_para = font_lookup_table_read_char(font, 16);

    font_info.start_pos_upper_A = font_lookup_table_read_word(font, 17);
    font_info.start_pos_lower_a = font_lookup_table_read_word(font, 19);
    font_info.start_pos_unicode = font_lookup_table_read_word(font, 21);
}

void SH1122Oled::set_font_direction(FontDirection dir)
{
    font_dir = dir;
}

void SH1122Oled::clear_buffer()
{
    memset(frame_buffer, 0, sizeof(frame_buffer));
}

void SH1122Oled::reset()
{
    gpio_set_level(oled_cfg.io_rst, 0);   // bring oled into reset (rst low)
    vTaskDelay(200 / portTICK_PERIOD_MS); // wait 200ms
    gpio_set_level(oled_cfg.io_rst, 1);   // bring oled out of rest (rst high)
    vTaskDelay(200 / portTICK_PERIOD_MS); // wait 200ms to boot
}

void SH1122Oled::power_off()
{
    uint8_t cmd = OLED_CMD_POWER_OFF;
    send_commands(&cmd, 1);
}

void SH1122Oled::power_on()
{
    uint8_t cmd = OLED_CMD_POWER_ON;
    send_commands(&cmd, 1);
}

void SH1122Oled::set_contrast(uint8_t contrast_reg_val)
{
    uint8_t cmds[2] = {OLED_CMD_SET_DISP_CONTRAST, contrast_reg_val};
    send_commands(cmds, 2);
}

void SH1122Oled::set_multiplex_ratio(uint8_t multiplex_ratio_reg_val)
{
    uint8_t cmds[2] = {OLED_CMD_SET_MULTIPLEX_RATION, multiplex_ratio_reg_val};
    send_commands(cmds, 2);
}

void SH1122Oled::set_dc_dc_control_mod(uint8_t mod)
{
    uint8_t cmds[2] = {OLED_CMD_SET_DC_DC_CONTROL_MOD, mod};
    send_commands(cmds, 2);
}

void SH1122Oled::set_oscillator_freq(uint8_t freq_reg_val)
{
    uint8_t cmds[2] = {OLED_CMD_SET_OSCILLATOR_FREQ, freq_reg_val};
    send_commands(cmds, 2);
}

void SH1122Oled::set_display_offset_mod(uint8_t mod)
{
    uint8_t cmds[2] = {OLED_CMD_SET_DISP_OFFSET_MOD, mod};
    send_commands(cmds, 2);
}

void SH1122Oled::set_precharge_period(uint8_t period_reg_val)
{
    uint8_t cmds[2] = {OLED_CMD_SET_PRE_CHARGE_PERIOD, period_reg_val};
    send_commands(cmds, 2);
}

void SH1122Oled::set_vcom(uint8_t vcom_reg_val)
{
    uint8_t cmds[2] = {OLED_CMD_SET_VCOM, vcom_reg_val};
    send_commands(cmds, 2);
}

void SH1122Oled::set_vseg(uint8_t vseg_reg_val)
{
    uint8_t cmds[2] = {OLED_CMD_SET_VSEG, vseg_reg_val};
    send_commands(cmds, 2);
}

void SH1122Oled::set_row_addr(uint8_t row_addr)
{
    uint8_t cmds[2] = {OLED_CMD_SET_ROW_ADDR, row_addr};
    send_commands(cmds, 2);
}

void SH1122Oled::set_start_line(uint8_t start_line)
{
    uint8_t cmd = OLED_CMD_SET_DISP_START_LINE | start_line;

    send_commands(&cmd, 1);
}

void SH1122Oled::set_vseg_discharge_level(uint8_t discharge_level)
{
    uint8_t cmd = OLED_CMD_SET_DISCHARGE_LEVEL | discharge_level;

    send_commands(&cmd, 1);
}

void SH1122Oled::set_high_column_address(uint8_t high_column_addr)
{
    uint8_t cmd = OLED_CMD_SET_HIGH_COLUMN_ADDR | high_column_addr;

    send_commands(&cmd, 1);
}

void SH1122Oled::set_low_column_address(uint8_t low_column_addr)
{
    uint8_t cmd = OLED_CMD_SET_LOW_COLUMN_ADDR | low_column_addr;

    send_commands(&cmd, 1);
}

void SH1122Oled::set_rev_display(bool rev_dir)
{
    uint8_t cmd = 0;

    if (rev_dir)
    {
        cmd = OLED_CMD_SET_REV_DISPLAY;
    }
    else
    {
        cmd = OLED_CMD_SET_NORMAL_DISPLAY;
    }

    send_commands(&cmd, 1);
}

void SH1122Oled::set_segment_remap(bool rev_dir)
{
    uint8_t cmd = 0;

    if (rev_dir)
        cmd = OLED_CMD_NORM_SEG_MAP | 0x01;
    else
        cmd = OLED_CMD_NORM_SEG_MAP;

    send_commands(&cmd, 1);
}

void SH1122Oled::set_orientation(bool flipped)
{
    uint8_t cmd = 0;

    if (flipped)
        cmd = OLED_CMD_SCAN_N_TO_0;
    else
        cmd = OLED_CMD_SCAN_0_TO_N;

    send_commands(&cmd, 1);
}

uint16_t SH1122Oled::get_ascii_next(uint8_t b)
{
    if (b == 0 || b == '\n')
        return 0x0ffff;
    else
        return b;
}

const uint8_t* SH1122Oled::font_get_glyph_data(uint16_t encoding)
{
    const uint8_t* glyph_ptr = font_info.font;
    const uint8_t* unicode_lookup_table = font_info.font;
    glyph_ptr += 23;
    unicode_lookup_table += 23 + font_info.start_pos_unicode;
    uint16_t unicode = 0;

    if (encoding <= 255)
    {
        if (encoding >= 'a')
            glyph_ptr += font_info.start_pos_lower_a;
        else if (encoding >= 'A')
            glyph_ptr += font_info.start_pos_upper_A;

        while (1)
        {
            if (*(glyph_ptr + 1) == 0)
            {
                glyph_ptr = NULL;
                break; // exit loop, reached end of font data and could not find glyph
            }
            else if (*glyph_ptr == encoding)
            {
                glyph_ptr += 2; // skip encoding and glyph size
                break;
            }

            glyph_ptr += *(glyph_ptr + 1);
        }
    }
    else
    {
        glyph_ptr += font_info.start_pos_unicode;

        do
        {
            glyph_ptr += font_lookup_table_read_word(unicode_lookup_table, 0);
            unicode = font_lookup_table_read_word(unicode_lookup_table, 2);
            unicode_lookup_table += 4;
        } while (unicode < encoding);

        while (1)
        {
            unicode = font_lookup_table_read_char(glyph_ptr, 0);
            unicode <<= 8;
            unicode |= font_lookup_table_read_char(glyph_ptr, 1);

            if (unicode == 0)
            {
                glyph_ptr = NULL;
                break;
            }

            if (unicode == encoding)
            {
                glyph_ptr += 3;
                break;
            }

            glyph_ptr += font_lookup_table_read_char(glyph_ptr, 2);
        }
    }

    return glyph_ptr;
}

void SH1122Oled::font_setup_glyph_decode(sh1122_oled_font_decode_t* decode, const uint8_t* glyph_data)
{
    decode->decode_ptr = glyph_data;
    decode->bit_pos = 0;

    decode->glyph_width = font_decode_get_unsigned_bits(decode, font_info.bits_per_char_width);
    decode->glyph_height = font_decode_get_unsigned_bits(decode, font_info.bits_per_char_height);
}

uint8_t SH1122Oled::font_decode_get_unsigned_bits(sh1122_oled_font_decode_t* decode, uint8_t cnt)
{
    uint8_t val;
    uint8_t bit_pos = decode->bit_pos;
    uint8_t bit_pos_plus_cnt;
    uint8_t s = 8;

    val = *decode->decode_ptr; // value of element in font lookup table currently being decoded
    val >>= bit_pos;           // shift by current bit position such that only bits with positions greater than the current position are decoded

    // find next bit position
    bit_pos_plus_cnt = bit_pos;
    bit_pos_plus_cnt += cnt;

    // if the next bit position falls within next font lookup table element
    if (bit_pos_plus_cnt >= 8)
    {
        s -= bit_pos;         // subtract starting bit position from element width (8 bits) to determine how many bits lay within next element
        decode->decode_ptr++; // increment to next element of lookup table
        val |= *decode->decode_ptr << (s); // set the unoccupied bits of val to bits to be decoded in next element
        bit_pos_plus_cnt -= 8;             // subtract the width of a lookup table element to account for moving to next element
    }

    val &= (1U << cnt) - 1; // clear bits of value that were not used, result is undecoded value

    decode->bit_pos = bit_pos_plus_cnt; // save next bit position to decode structure

    return val; // return the decoded value
}

int8_t SH1122Oled::font_decode_get_signed_bits(sh1122_oled_font_decode_t* decode, uint8_t cnt)
{
    int8_t val;
    int8_t d;
    val = (int8_t) font_decode_get_unsigned_bits(decode, cnt);
    d = 1;
    cnt--;
    d <<= cnt;
    val -= d;

    return val;
}

uint16_t SH1122Oled::font_apply_direction_y(uint16_t dy, int8_t x, int8_t y, FontDirection dir)
{
    switch (dir)
    {
    case FontDirection::left_to_right:
        dy += y;
        break;

    case FontDirection::top_to_bottom:
        dy += x;
        break;

    case FontDirection::right_to_left:
        dy -= y;
        break;

    case FontDirection::bottom_to_top:
        dy -= x;
        break;
    }

    return dy;
}

uint16_t SH1122Oled::font_apply_direction_x(uint16_t dx, int8_t x, int8_t y, FontDirection dir)
{
    switch (dir)
    {
    case FontDirection::left_to_right:
        dx += x;
        break;

    case FontDirection::top_to_bottom:
        dx -= y;
        break;

    case FontDirection::right_to_left:
        dx -= x;
        break;

    case FontDirection::bottom_to_top:
        dx += y;
        break;
    }

    return dx;
}

uint8_t SH1122Oled::font_lookup_table_read_char(const uint8_t* font, uint8_t offset)
{
    return *(const uint8_t*) (font + offset);
}

uint16_t SH1122Oled::font_lookup_table_read_word(const uint8_t* font, uint8_t offset)
{
    uint16_t word;

    word = (uint16_t) * (const uint8_t*) (font + offset);
    word <<= 8;
    word += (uint16_t) * (const uint8_t*) (font + offset + 1);

    return word;
}

void SH1122Oled::font_draw_lines(sh1122_oled_font_decode_t* decode, uint8_t len, uint8_t is_foreground)
{
    uint8_t cnt;     /* total number of remaining pixels, which have to be drawn */
    uint8_t rem;     /* remaining pixel to the right edge of the glyph */
    uint8_t current; /* number of pixels, which need to be drawn for the draw procedure */
                     /* current is either equal to cnt or equal to rem */

    /* local coordinates of the glyph */
    uint8_t lx, ly;

    /* target position on the screen */
    uint16_t x, y;

    cnt = len;

    /*get the local position*/
    lx = decode->x;
    ly = decode->y;

    while (1)
    {
        /*calculate the number of pixels to the right edge of the glyph*/
        rem = decode->glyph_width;
        rem -= lx;

        /*calculate how many pixels to draw*/
        current = rem;
        if (cnt < rem)
            current = cnt;

        x = decode->target_x;
        y = decode->target_y;

        x = font_apply_direction_x(x, lx, ly, font_dir);
        y = font_apply_direction_y(y, lx, ly, font_dir);

        if (is_foreground)
        {
            draw_hv_line(decode, x, y, current, (PixelIntensity) decode->fg_intensity);
        }

        if (cnt < rem)
            break;

        cnt -= rem;
        lx = 0;
        ly++;
    }
    lx += cnt;
    decode->x = lx;
    decode->y = ly;
}

void SH1122Oled::draw_hv_line(sh1122_oled_font_decode_t* decode, int16_t x, int16_t y, uint16_t length, PixelIntensity intensity)
{

    if (length != 0)
    {

        if (y < 0)
            return;
        if (y >= HEIGHT)
            return;

        if (x < 0)
            return;
        if (x >= WIDTH)
            return;

        switch (font_dir)
        {
        case FontDirection::left_to_right:
            draw_line(x, y, x + (length - 1), y, intensity);
            break;

        case FontDirection::top_to_bottom:
            draw_line(x, y, x, y + (length - 1), intensity);
            break;

        case FontDirection::right_to_left:
            draw_line(x - (length - 1), y, x, y, intensity);
            break;

        case FontDirection::bottom_to_top:
            draw_line(x, y, x, y - (length - 1), intensity);
            break;
        }
    }
}

int8_t SH1122Oled::font_decode_glyph(sh1122_oled_font_decode_t* decode, const uint8_t* glyph_data)
{
    uint8_t bg_line_length, fg_line_length;
    int8_t x, y;
    int8_t d;
    int8_t h;

    h = decode->glyph_height;

    x = font_decode_get_signed_bits(decode, font_info.bits_per_char_x);
    y = font_decode_get_signed_bits(decode, font_info.bits_per_char_y);
    d = font_decode_get_signed_bits(decode, font_info.bits_per_delta_x);

    if (decode->glyph_width > 0)
    {
        // decode->target_x += x;
        // decode->target_y -= y + h;
        decode->target_x = font_apply_direction_x(decode->target_x, x, -(h + y), font_dir);
        decode->target_y = font_apply_direction_y(decode->target_y, x, -(h + y), font_dir);
        decode->x = 0;
        decode->y = 0;

        while (1)
        {
            bg_line_length = font_decode_get_unsigned_bits(decode, font_info.bits_per_0); // bits per background line
            fg_line_length = font_decode_get_unsigned_bits(decode, font_info.bits_per_1); // bits per foreground line

            do
            {
                font_draw_lines(decode, bg_line_length, 0);
                font_draw_lines(decode, fg_line_length, 1);
            } while (font_decode_get_unsigned_bits(decode, 1) != 0);

            if (decode->y >= h)
                break;
        }
    }

    return d;
}

void SH1122Oled::send_commands(uint8_t* cmds, uint16_t length)
{
    gpio_set_level(oled_cfg.io_dc, 0);
    spi_transaction_t t;
    spi_transaction_t* rt;
    uint8_t dc = COMMAND;
    t.length = length * 8;
    t.rxlength = 0;
    t.tx_buffer = cmds;
    t.rx_buffer = NULL;
    t.user = (void*) &dc;
    t.flags = 0;
    spi_device_queue_trans(spi_hdl, &t, portMAX_DELAY);
    spi_device_get_trans_result(spi_hdl, &rt, portMAX_DELAY);
}

void SH1122Oled::send_data(uint8_t* data, uint16_t length)
{
    gpio_set_level(oled_cfg.io_dc, 1);
    spi_transaction_t t;
    spi_transaction_t* rt;
    uint8_t dc = DATA;
    t.length = length * 8;
    t.rxlength = 0;
    t.tx_buffer = data;
    t.rx_buffer = NULL;
    t.user = (void*) &dc;
    t.flags = 0;
    spi_device_queue_trans(spi_hdl, &t, portMAX_DELAY);
    spi_device_get_trans_result(spi_hdl, &rt, portMAX_DELAY);
}

void SH1122Oled::default_init()
{
    power_off();
    reset();

    // send all initialization commands
    set_oscillator_freq(0x50);
    set_multiplex_ratio(0x3F);
    set_display_offset_mod(0x00);
    set_row_addr(0x00);
    set_start_line(0x00);
    set_vseg_discharge_level(0x00);
    set_dc_dc_control_mod(0x80);
    set_segment_remap(false);
    set_orientation(false);
    set_contrast(0x90);
    set_precharge_period(0x28);
    set_vcom(0x30);
    set_vseg(0x1E);
    set_rev_display(false);
    set_high_column_address(0x00);
    set_low_column_address(0x00);
    // clear screen of any artifacts
    clear_buffer();
    power_on(); // power back on oled

    update_screen();
}