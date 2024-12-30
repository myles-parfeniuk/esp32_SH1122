#include "SH1122Oled.hpp"

sh1122_oled_cfg_t SH1122Oled::oled_cfg;
uint8_t SH1122Oled::frame_buffer[FRAME_BUFFER_LENGTH];
SH1122Oled::get_next_char_cb_t SH1122Oled::get_next_char_cb = nullptr;
uint8_t SH1122Oled::utf8_state = 0U;
uint16_t SH1122Oled::utf8_encoding = 0U;
SH1122Oled::FontDirection SH1122Oled::font_dir = SH1122Oled::FontDirection::left_to_right;
SH1122Oled::sh1122_oled_font_info_t SH1122Oled::font_info;

/**
 * @brief SH1122Oled constructor.
 *
 * Construct a SH1122Oled object for managing a SH1122Oled driven OLED display.
 * Initializes required GPIO pins and SPI peripheral then sends out several commands
 * to initialize SH1122. Commands can be seen in default_init().
 *
 * @param oled_cfg Configuration settings (optional), default settings can be seen in sh1122_oled_cfg_t and are selectable.
 * @return void, nothing to return
 */
SH1122Oled::SH1122Oled(sh1122_oled_cfg_t settings)
{
    oled_cfg = settings;
    font_info.font = nullptr; // used to check if user has loaded font for string/glyph functions
    // set-up data command pin and rst pin
    gpio_config_t io_dc_rst_cs_cfg;

    io_dc_rst_cs_cfg.pin_bit_mask = (1 << oled_cfg.io_rst) | (1 << oled_cfg.io_dc) | (1 << oled_cfg.io_cs);
    io_dc_rst_cs_cfg.mode = GPIO_MODE_OUTPUT;
    io_dc_rst_cs_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_dc_rst_cs_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    io_dc_rst_cs_cfg.intr_type = GPIO_INTR_DISABLE;

    gpio_config(&io_dc_rst_cs_cfg);

    gpio_set_level(oled_cfg.io_rst, 1); // put rst pin initially high

    spi_bus_cfg.mosi_io_num = oled_cfg.io_mosi;
    spi_bus_cfg.miso_io_num = GPIO_NUM_NC;
    spi_bus_cfg.sclk_io_num = oled_cfg.io_sclk;
    spi_bus_cfg.quadhd_io_num = GPIO_NUM_NC;
    spi_bus_cfg.quadwp_io_num = GPIO_NUM_NC;
    spi_bus_cfg.max_transfer_sz = FRAME_CHUNK_3_LENGTH;
    spi_bus_cfg.flags = SPICOMMON_BUSFLAG_MASTER;
    spi_bus_cfg.isr_cpu_id = ESP_INTR_CPU_AFFINITY_AUTO;

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

    // initialize SPI peripheral
    ESP_ERROR_CHECK(spi_bus_initialize(oled_cfg.spi_peripheral, &spi_bus_cfg, oled_cfg.dma_cha));
    ESP_ERROR_CHECK(spi_bus_add_device(oled_cfg.spi_peripheral, &oled_interface_cfg, &spi_hdl));

    default_init();
}

/**
 * @brief Updates OLED display with current frame buffer.
 *
 * Sends frame buffer to SH1122 over SPI, should be called after performing draw operations.
 *
 * @return void, nothing to return
 */
void SH1122Oled::update_screen()
{
    gpio_set_level(oled_cfg.io_cs, 0);

    // update display in 3 chunks, SPI max DMA size is 4096 bytes at a time, but display buffer is 8192
    send_data(frame_buffer, FRAME_CHUNK_1_LENGTH);
    send_data(frame_buffer + (FRAME_CHUNK_1_LENGTH), FRAME_CHUNK_2_LENGTH);
    send_data(frame_buffer + (FRAME_CHUNK_1_LENGTH + FRAME_CHUNK_2_LENGTH), FRAME_CHUNK_3_LENGTH);

    gpio_set_level(oled_cfg.io_cs, 1);
}

/**
 * @brief Sets respective pixel to specified grayscale intensity.
 *
 * @param x Pixel x location.
 * @param y Pixel y location.
 * @param intensity Grayscale intensity of the drawn pixel.
 * @return void, nothing to return
 */
void SH1122Oled::set_pixel(uint16_t x, uint16_t y, PixelIntensity intensity)
{

    int16_t x_it = 0;
    int16_t y_it = 0;
    int16_t high_byte = 0;

    if (intensity != PixelIntensity::level_transparent)
        if (SH1122_PIXEL_IN_BOUNDS(x, y))
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
                *pixel = (static_cast<uint8_t>(intensity) & 0x0FU) | (*pixel & 0xF0U);
            else
                *pixel = ((static_cast<uint8_t>(intensity) << 4U) & 0xF0U) | (*pixel & 0x0FU);
        }
}

/**
 * @brief Draws a line between two points.
 *
 * @param x_1 Line starting x location.
 * @param y_1 Line starting y location.
 * @param x_2 Line ending x location.
 * @param y_2 Line ending y location.
 * @param intensity Grayscale intensity of the drawn line.
 * @return void, nothing to return
 */
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

/**
 * @brief Draws rectangular frame at the specified location.
 *
 * @param x Frame x location (upper left corner of frame)
 * @param y Frame y location (upper left corner of frame)
 * @param width Frame width.
 * @param height Frame height.
 * @param thickness Frame thickness (drawn towards center of rectangle)
 * @param intensity Grayscale intensity of the drawn frame.
 * @return void, nothing to return
 */
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

/**
 * @brief Draws a filled rectangle at the specified location.
 *
 * @param x Rectangle x location (upper left corner of rectangle)
 * @param y Rectangle y location (upper left corner of rectangle)
 * @param width Rectangle width.
 * @param height Rectangle height.
 * @param intensity Grayscale intensity of the drawn rectangle.
 * @return void, nothing to return
 */
void SH1122Oled::draw_rectangle(int16_t x_1, int16_t y_1, int16_t width, int16_t height, PixelIntensity intensity)
{
    for (uint16_t j = 0; j < height; j++)
    {
        for (uint16_t i = 0; i < width; i++)
            set_pixel((x_1 + i), (y_1 + j), intensity);
    }
}

/**
 * @brief Draws a circular frame at the specified location.
 *
 * @param x_c Circle center x position.
 * @param y_c Circle center y position.
 * @param r Circle radius.
 * @param thickness Frame thickness (drawn towards center of circle)
 * @param intensity Grayscale intensity of the drawn circle.
 * @return void, nothing to return
 */
void SH1122Oled::draw_circle_frame(int16_t x_c, int16_t y_c, int16_t r, int16_t thickness, PixelIntensity intensity)
{
    draw_ellipse_frame(x_c, y_c, r, r, thickness, intensity);
}

/**
 * @brief Draws a filled circle at the specified location.
 *
 * @param x_c Circle center x position.
 * @param y_c Circle center y position.
 * @param r Circle radius.
 * @param intensity Grayscale intensity of the drawn circle.
 * @return void, nothing to return
 */
void SH1122Oled::draw_circle(int16_t x_c, int16_t y_c, int16_t r, PixelIntensity intensity)
{
    draw_circle_frame(x_c, y_c, r, r, intensity);
}

/**
 * @brief Draws an ellipse frame at the specified location.
 *
 * @param x_c Ellipse center x position.
 * @param y_c Ellipse center y position.
 * @param r_x Horizontal radius (ellipse parameter a)
 * @param r_y Vertical radius (ellipse parameter b)
 * @param thickness Frame thickness (drawn towards center of ellipse)
 * @param intensity Grayscale intensity of the drawn ellipse.
 * @return void, nothing to return
 */
void SH1122Oled::draw_ellipse_frame(int16_t x_c, int16_t y_c, int16_t r_x, int16_t r_y, int16_t thickness, PixelIntensity intensity)
{
    int32_t d_x;
    int32_t d_y;
    float d_1;
    float d_2;
    int16_t x = 0;
    int16_t y = 0;
    int32_t r_x_sq = 0;
    int32_t r_y_sq = 0;
    std::vector<std::vector<sh1122_2d_point_t>> quad_1_points(2); // element 0 is outter points, element 1 is inner points
    std::vector<std::vector<sh1122_2d_point_t>> quad_2_points(2); // element 0 is outter points, element 1 is inner points
    std::vector<std::vector<sh1122_2d_point_t>> quad_3_points(2); // element 0 is outter points, element 1 is inner points
    std::vector<std::vector<sh1122_2d_point_t>> quad_4_points(2); // element 0 is outter points, element 1 is inner points

    if (thickness <= 0 || thickness > r_y || thickness > r_x)
        return;

    if (r_y == thickness)
        set_pixel(x_c, y_c, intensity);
    else if (r_x == thickness)
    {
        draw_line(x_c, y_c - r_y, x_c, y_c + r_y, intensity);
    }

    for (int i = 0; (thickness > 1) ? (i < 2) : (i < 1); i++)
    {
        x = 0; // ellipse is centered about origin, assume (0, r_y) as first point and transform by x_c and y_c later
        y = (r_y - i * (thickness - 1));
        r_x_sq = (int32_t) (r_x - i * (thickness - 1)) * (r_x - i * (thickness - 1));
        r_y_sq = (int32_t) (r_y - i * (thickness - 1)) * (r_y - i * (thickness - 1));

        // calculate initial decision parameter for region 1 of quadrants, d_1_0 = r_y^2 + (1/4)*r_x^2 -r_x^2*r_y
        d_1 = (float) r_y_sq + (0.25f * (float) r_x_sq) - (float) (r_x_sq * (r_y - i * (thickness - 1)));

        // next  decision parameter modifiers
        d_x = 2 * r_y_sq * x; // if(d_1[k] < 0): d_1[k+1] = d_1[k] + 2 * r_y^2 * x[k + 1] + r_y^2
        d_y = 2 * r_x_sq * y; // if(d_1[k] >= 0): d_1[k+1] = d_1[k] + 2 * r_y^2 * x[k + 1] - 2 * r_x^2 * y[k] + r_y^2

        while (d_x < d_y)
        {
            // draw region 1 points
            if (SH1122_PIXEL_IN_BOUNDS(x + x_c, y + y_c))
            {
                // quadrant 1
                set_pixel(x + x_c, y + y_c, intensity);
                quad_1_points[i].push_back((sh1122_2d_point_t){(int16_t) (x + x_c), (int16_t) (y + y_c)});
            }

            if (SH1122_PIXEL_IN_BOUNDS(-x + x_c, y + y_c))
            {
                // quadrant 2
                set_pixel(-x + x_c, y + y_c, intensity);
                quad_2_points[i].push_back((sh1122_2d_point_t){(int16_t) (-x + x_c), (int16_t) (y + y_c)});
            }

            if (SH1122_PIXEL_IN_BOUNDS(x + x_c, -y + y_c))
            {
                // quadrant 3
                set_pixel(x + x_c, -y + y_c, intensity);
                quad_3_points[i].push_back((sh1122_2d_point_t){(int16_t) (x + x_c), (int16_t) (-y + y_c)});
            }

            if (SH1122_PIXEL_IN_BOUNDS(-x + x_c, -y + y_c))
            {
                // quadrant 4
                set_pixel(-x + x_c, -y + y_c, intensity);
                quad_4_points[i].push_back((sh1122_2d_point_t){(int16_t) (-x + x_c), (int16_t) (-y + y_c)});
            }

            // find next point and next d_1 parameter
            if (d_1 < 0) // if(d_1[k] < 0)
            {
                x++; // next point is (x[k+1], y[k])
                d_x += 2 * r_y_sq;
                d_1 += d_x + r_y_sq; // d_1[k+1] = d_1[k] + 2 * r_y^2 * x[k+1] + r_y^2
            }
            else // if(d_1[k] >= 0)
            {
                x++; // next point is (x[k+1], y[k-1])
                y--;
                d_x += 2 * r_y_sq;
                d_y += -2 * r_x_sq;
                d_1 += d_x - d_y + r_y_sq; // d_1[k+1] = d_1[k] + 2 * r_y^2 * x[k+1] - 2 * r_x^2 * y[k] + r_y^2
            }
        }

        // calculate initial decision parameter of region 2 for quadrants d_2_0 = r_y^2 * (x_0 + 1/2)^2 + r_x^2 * (y_9 - 1)^2 - r_x^2 *r_y^2
        d_2 = ((float) r_y_sq * ((float) x + 0.5f) * ((float) x + 0.5f)) + (float) (r_x_sq * (y - 1) * (y - 1)) - (float) (r_x_sq * r_y_sq);

        while (y >= 0)
        {
            // draw region 2 points
            if (SH1122_PIXEL_IN_BOUNDS(x + x_c, y + y_c))
            {
                // quadrant 1
                set_pixel(x + x_c, y + y_c, intensity);
                quad_1_points[i].push_back((sh1122_2d_point_t){(int16_t) (x + x_c), (int16_t) (y + y_c)});
            }

            if (SH1122_PIXEL_IN_BOUNDS(-x + x_c, y + y_c))
            {
                // quadrant 2
                set_pixel(-x + x_c, y + y_c, intensity);
                quad_2_points[i].push_back((sh1122_2d_point_t){(int16_t) (-x + x_c), (int16_t) (y + y_c)});
            }

            if (SH1122_PIXEL_IN_BOUNDS(x + x_c, -y + y_c))
            {
                // quadrant 3
                set_pixel(x + x_c, -y + y_c, intensity);
                quad_3_points[i].push_back((sh1122_2d_point_t){(int16_t) (x + x_c), (int16_t) (-y + y_c)});
            }

            if (SH1122_PIXEL_IN_BOUNDS(-x + x_c, -y + y_c))
            {
                // quadrant 4
                set_pixel(-x + x_c, -y + y_c, intensity);
                quad_4_points[i].push_back((sh1122_2d_point_t){(int16_t) (-x + x_c), (int16_t) (-y + y_c)});
            }

            // find next point and next d_2 parameter
            if (d_2 > 0) // if(d_2[k] > 0)
            {
                y--; // next point is (x[k], y[k-1])
                d_y += -2 * r_x_sq;
                d_2 += r_x_sq - d_y; // d_2[k+1] = d_2[k] - 2 * r_x^2 * y[k+1] + r_x^2
            }
            else // if(d_2[k] <= 0)
            {
                // next point is (x[k+1], y[k-1])
                y--;
                x++;
                d_x += 2 * r_y_sq;
                d_y += -2 * r_x_sq;
                d_2 += d_x - d_y + r_x_sq; // d_2[k+1] = d_2[k] + 2 * r_y^2 * x[k+1] - 2 * r_x^2 * y[k+1] + r_x^2
            }
        }
    }

    if (quad_1_points.at(0).size() != 0 && quad_1_points.at(1).size() != 0)
        fill_ellipse_frame_quadrant(quad_1_points.at(0), quad_1_points.at(1), y_c, intensity);

    if (quad_2_points.at(0).size() != 0 && quad_2_points.at(1).size() != 0)
        fill_ellipse_frame_quadrant(quad_2_points.at(0), quad_2_points.at(1), y_c, intensity);

    if (quad_3_points.at(0).size() != 0 && quad_3_points.at(1).size() != 0)
        fill_ellipse_frame_quadrant(quad_3_points.at(0), quad_3_points.at(1), y_c, intensity);

    if (quad_4_points.at(0).size() != 0 && quad_4_points.at(1).size() != 0)
        fill_ellipse_frame_quadrant(quad_4_points.at(0), quad_4_points.at(1), y_c, intensity);
}

/**
 * @brief Draws a filled ellipse at the specified location.
 *
 * @param x_c Ellipse center x position.
 * @param y_c Ellipse center y position.
 * @param r_x Horizontal radius (ellipse parameter a)
 * @param r_y Vertical radius (ellipse parameter b)
 * @param intensity Grayscale intensity of the drawn ellipse.
 * @return void, nothing to return
 */
void SH1122Oled::draw_ellipse(int16_t x_c, int16_t y_c, int16_t r_x, int16_t r_y, PixelIntensity intensity)
{
    int16_t thickness = std::min(r_x, r_y);
    draw_ellipse_frame(x_c, y_c, r_x, r_y, thickness, intensity);
}

/**
 * @brief Draws the selected glyph/character using the currently loaded font.
 *
 * @param x Glyph x location (upper left corner of glyph)
 * @param y Glyph y location (upper left corner of glyph)
 * @param intensity Grayscale intensity of the drawn glyph
 * @param encoding The encoding of the character to be drawn, supports UTF-8 and UTF-16.
 * @return The change in x required to draw the next glyph in string without overlapping.
 */
uint16_t SH1122Oled::draw_glyph(uint16_t x, uint16_t y, PixelIntensity intensity, uint16_t encoding)
{
    const uint8_t* glyph_ptr = NULL;
    sh1122_oled_font_decode_t decode;
    uint16_t dx = 0;

    // must load font before attempting to write glyphs
    if (font_info.font == nullptr)
    {
        ESP_LOGE(TAG, "No font loaded.");
        return 0;
    }

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
    decode.fg_intensity = static_cast<uint8_t>(intensity);

    glyph_ptr = NULL;

    if (encoding != 0x0ffff)
    {
        glyph_ptr = font_get_glyph_data(encoding); // get glyph data from lookup table
        if (glyph_ptr != NULL)
        {
            font_setup_glyph_decode(&decode, glyph_ptr);         // setup decode structure with important values from table
            dx = font_decode_and_draw_glyph(&decode, glyph_ptr); // decode and draw the glyph
        }
    }

    return dx;
}

/**
 * @brief Draws a string at the specified location using the currently loaded font.
 *
 * @param x String x location (upper left corner of string)
 * @param y String y location (upper left corner of string)
 * @param intensity Grayscale intensity of the drawn string
 * @param format The string to be drawn, supports variable arguments (ie printf style formatting)
 * @return The width of the drawn string.
 */
uint16_t SH1122Oled::draw_string(uint16_t x, uint16_t y, PixelIntensity intensity, const char* format, ...)
{
    uint16_t delta = 0;
    uint16_t encoding = 0;
    uint16_t sum = 0;
    char* buffer = nullptr;
    uint8_t* str = nullptr;
    va_list args;
    uint16_t size;

    utf8_encoding = 0U;
    utf8_state = 0U;

    // must load font before attempting to write
    if (font_info.font == nullptr)
    {
        ESP_LOGE(TAG, "No font loaded.");
        return 0;
    }

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
        encoding = get_next_char_cb(*str); // check to ensure character is not null or new line (end of string)

        if (encoding == 0x0ffff)
            break;

        str++;

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
    }

    delete[] buffer;

    return sum;
}

/**
 * @brief Draws a sh1122 custom run-length-encoded bitmap created with sh1122_encode_bitmap.py.
 *
 * @param x Bitmap x location (upper left corner of bitmap)
 * @param y Bitmap y location (upper left corner of bitmap)
 * @param bg_intensity Background intensity (optional, default transparent), fills transparent pixels with bg_intensity if used
 * @return void, nothing to return
 */
void SH1122Oled::draw_bitmap(uint16_t x, uint16_t y, const uint8_t* bitmap, PixelIntensity bg_intensity)
{
    const int16_t bitmap_col_sz = *(bitmap + 2);
    const int16_t bitmap_row_sz = *(bitmap + 3);
    const uint8_t* data_ptr = bitmap + 4;
    int16_t repeated_value_lim = 0;
    PixelIntensity intensity = PixelIntensity::level_transparent;
    int16_t repeated_value_count = 0;

    bitmap_decode_pixel_block(&data_ptr, repeated_value_lim, intensity);

    for (int row = 0; row < bitmap_row_sz; row++)
    {
        for (int col = 0; col < bitmap_col_sz; col++)
        {
            if (intensity == PixelIntensity::level_transparent)
                intensity = bg_intensity;

            set_pixel(x + col, y + row, intensity);
            repeated_value_count++;

            if (repeated_value_count >= repeated_value_lim)
            {
                bitmap_decode_pixel_block(&data_ptr, repeated_value_lim, intensity);
                repeated_value_count = 0;
            }
        }
    }
}

/**
 * @brief Returns the width of specified glyph using the currently loaded font.
 *
 * @param encoding The encoding of the character for which width is desired, supports UTF-8 and UTF-16.
 * @return The width of the specified glyph.
 */
uint16_t SH1122Oled::font_get_glyph_width(uint16_t encoding)
{
    const uint8_t* glyph_data;
    sh1122_oled_font_decode_t decode;

    if (font_info.font == nullptr)
    {
        ESP_LOGE(TAG, "No font loaded.");
        return 0;
    }

    glyph_data = font_get_glyph_data(encoding);

    if (glyph_data == NULL)
        return 0;

    font_setup_glyph_decode(&decode, glyph_data);
    font_decode_get_signed_bits(&decode, font_info.bits_per_char_x);
    font_decode_get_signed_bits(&decode, font_info.bits_per_char_y);

    return font_decode_get_signed_bits(&decode, font_info.bits_per_delta_x);
}

/**
 * @brief Returns the width of specified glyph using the currently loaded font. Overloaded with decode structure for calls to font_get_string_width()
 *
 * @param decode The decode structure to save the glyph x offset in, for use within get_string_width()
 * @param encoding The encoding of the character for which width is desired, supports UTF-8 and UTF-16.
 * @return The width of the specified glyph.
 */
uint16_t SH1122Oled::font_get_glyph_width(sh1122_oled_font_decode_t* decode, uint16_t encoding)
{
    const uint8_t* glyph_data;

    if (font_info.font == nullptr)
    {
        ESP_LOGE(TAG, "No font loaded.");
        return 0;
    }

    glyph_data = font_get_glyph_data(encoding);

    if (glyph_data == NULL)
        return 0;

    font_setup_glyph_decode(decode, glyph_data);
    decode->glyph_x_offset = font_decode_get_signed_bits(decode, font_info.bits_per_char_x);
    font_decode_get_signed_bits(decode, font_info.bits_per_char_y);

    return font_decode_get_signed_bits(decode, font_info.bits_per_delta_x);
}

/**
 * @brief Returns the height of specified glyph using the currently loaded font.
 *
 * @param encoding The encoding of the character for which height is desired, supports UTF-8 and UTF-16.
 * @return The height of the specified glyph.
 */
uint16_t SH1122Oled::font_get_glyph_height(uint16_t encoding)
{
    const uint8_t* glyph_data;
    sh1122_oled_font_decode_t decode;

    if (font_info.font == nullptr)
    {
        ESP_LOGE(TAG, "No font loaded.");
        return 0;
    }

    glyph_data = font_get_glyph_data(encoding);

    if (glyph_data == NULL)
        return 0;

    font_setup_glyph_decode(&decode, glyph_data);

    return decode.glyph_height;
    return 0;
}

/**
 * @brief Returns the width of specified string using the currently loaded font.
 *
 * @param format The string for which width is desired, supports variable arguments (ie printf style formatting)
 * @return The width of the specified string.
 */
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

    utf8_encoding = 0U;
    utf8_state = 0U;

    // cannot get string width without font info
    if (font_info.font == nullptr)
    {
        ESP_LOGE(TAG, "No font loaded.");
        return 0;
    }

    // find length of variable argument string
    va_start(args, format);
    size = vsnprintf(nullptr, 0, format, args) + 1;
    va_end(args);

    buffer = new char[size]; // allocate correct amount of memory for string

    if (!buffer)
        return 0;

    va_start(args, format);
    vsnprintf(buffer, size, format, args); // save variable argument string to buffer
    va_end(args);

    str = (uint8_t*) buffer; // cast char * to uint8_t for use with other member methods

    width = 0;
    dx = 0;

    while (1)
    {
        encoding = get_next_char_cb(*str); // get next character

        if (encoding == 0x0FFFFU)
            break;
        if (encoding != 0x0FFFEU)
        {
            dx = font_get_glyph_width(&decode, encoding); // get the glyph width
            if (initial_x_offset == -64)
                initial_x_offset = decode.glyph_x_offset;

            width += dx; // increment width counter
        }
        str++; // increment string pointer to next glyph
    }

    // if glyph_width is greater than 0, apply the respective glyph x offset.
    if (decode.glyph_width != 0)
    {
        width -= dx;
        width += decode.glyph_width;
        width += decode.glyph_x_offset;
        if (initial_x_offset > 0)
            width += initial_x_offset;
    }

    delete[] buffer; // free allocated buffer memory

    return width;
}

/**
 * @brief Returns the height (tallest character height) of specified string using the currently loaded font.
 *
 * @param format The string for which height is desired, supports variable arguments (ie printf style formatting)
 * @return The width of the specified string.
 */
uint16_t SH1122Oled::font_get_string_height(const char* format, ...)
{
    char* buffer = nullptr;
    uint8_t* str = nullptr;
    va_list args;
    uint16_t size;
    uint16_t current_height = 0;
    uint16_t max_height = 0;

    // cannot get string height without font info
    if (font_info.font == nullptr)
    {
        ESP_LOGE(TAG, "No font loaded.");
        return 0;
    }

    // allocate correct amount of memory and save string from variable argument list
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

    // so long as the current glyph is not NULL (EOL) get its height and increment string pointer to next glyph
    while (*str != '\0')
    {
        current_height = font_get_glyph_height(*str);
        // if the current height is greater than the largest height detected
        if (current_height > max_height)
            max_height = current_height; // overwrite max_height with tallest character height detected

        str++;
    }

    delete[] buffer; // free memory allocated for string
    return max_height;
}

/**
 * @brief Returns the x position required to horizontally center a given string.
 *
 * @param str The string for which a horizontal centering is desired.
 * @return The x position the string should be drawn at to center it horizontally.
 */
uint16_t SH1122Oled::font_get_string_center_x(const char* str)
{
    uint16_t str_width;

    if (font_info.font == nullptr)
    {
        ESP_LOGE(TAG, "No font loaded.");
        return 0;
    }

    str_width = font_get_string_width(str);

    return (WIDTH - str_width) / 2;
}

/**
 * @brief Returns the y position required to vertically center a given string.
 *
 * @param str The string for which a vertical centering is desired.
 * @return The y position the string should be drawn at to center it vertically.
 */
uint16_t SH1122Oled::font_get_string_center_y(const char* str)
{
    uint16_t max_char_height;

    if (font_info.font == nullptr)
    {
        ESP_LOGE(TAG, "No font loaded.");
        return 0;
    }

    max_char_height = font_get_string_height(str);

    return (HEIGHT - max_char_height) / 2;
}

/**
 * @brief Sends frame buffer to sh1122_screenshot.py to create a screen shot.
 *
 * @return void, nothing to return
 */
void SH1122Oled::take_screen_shot()
{
    const constexpr int CHUNK_BUFFER_LENGTH = 512;
    int index = 0;
    char chunk_buffer[CHUNK_BUFFER_LENGTH];
    uint16_t encoded_val = 0;

    printf("SCREENSHOT_S\n\r");
    printf("TIMESTAMP %lld\n\r", esp_timer_get_time());
    for (int i = 0; i < FRAME_BUFFER_LENGTH; i += 2)
    {
        encoded_val = ((static_cast<uint16_t>(frame_buffer[i + 1]) << 8U) & 0xFF00U) | (static_cast<uint16_t>(frame_buffer[i]) & 0x00FFU);
        index += snprintf(chunk_buffer + index, CHUNK_BUFFER_LENGTH - index, " %d,", encoded_val);

        if (index > CHUNK_BUFFER_LENGTH - 50)
        {
            printf(" %s \n\r", chunk_buffer);
            index = 0;
        }
        else if (i == FRAME_BUFFER_LENGTH - 2)
            printf(" %s \n\r", chunk_buffer);
    }
    printf("SCREENSHOT_P\n\r");
}

/**
 * @brief Loads a font for drawing strings and glyphs (will be decoded in ASCII mode).
 *
 * @param font A pointer to the first element of the respective font lookup table, font tables are located in fonts directory.
 * @return void, nothing to return
 */
void SH1122Oled::load_font(const uint8_t* font)
{

    // set get_next_char callback to ASCII
    get_next_char_cb = get_ascii_next;

    set_font_vars(font);
}

/**
 * @brief Loads a font for drawing strings and glyphs (will be decoded in utf-8 mode).
 *
 * @param font A pointer to the first element of the respective font lookup table, font tables are located in fonts directory.
 * @return void, nothing to return
 */
void SH1122Oled::load_font_utf8(const uint8_t* font)
{

    // set get_next_char callback to ASCII
    get_next_char_cb = get_utf8_next;

    set_font_vars(font);
}

/**
 * @brief Sets the draw direction for strings and glyphs, default is left to right.
 *
 * @param dir The direction strings and glyphs should be drawn in, see FontDirection definition.
 * @return void, nothing to return
 */
void SH1122Oled::set_font_direction(FontDirection dir)
{
    font_dir = dir;
}

/**
 * @brief Clears the buffer containing the pixel data sent to SH1122.
 *
 * @return void, nothing to return
 */
void SH1122Oled::clear_buffer()
{
    memset(frame_buffer, 0, sizeof(frame_buffer));
}

/**
 * @brief Hard resets the SH1122 using the RST pin.
 *
 * @return void, nothing to return
 */
void SH1122Oled::reset()
{
    gpio_set_level(oled_cfg.io_rst, 0);   // bring oled into reset (rst low)
    vTaskDelay(200 / portTICK_PERIOD_MS); // wait 200ms
    gpio_set_level(oled_cfg.io_rst, 1);   // bring oled out of rest (rst high)
    vTaskDelay(200 / portTICK_PERIOD_MS); // wait 200ms to boot
}

/**
 * @brief Sends power off command.
 *
 * @return void, nothing to return
 */
void SH1122Oled::power_off()
{
    uint8_t cmd = OLED_CMD_POWER_OFF;
    send_commands(&cmd, 1);
}

/**
 * @brief Sends power on command.
 *
 * @return void, nothing to return
 */
void SH1122Oled::power_on()
{
    uint8_t cmd = OLED_CMD_POWER_ON;
    send_commands(&cmd, 1);
}

/**
 * @brief Sets the contrast of the display. Display must be powered off before calling.
 *
 * @param contrast_reg_val The desired contrast, SH1122 has 256 contrast steps from 0x00 to 0xFF.
 * @return void, nothing to return
 */
void SH1122Oled::set_contrast(uint8_t contrast_reg_val)
{
    uint8_t cmds[2] = {OLED_CMD_SET_DISP_CONTRAST, contrast_reg_val};
    send_commands(cmds, 2);
}

/**
 * @brief Sets the multiplex ratio of the display. Display must be powered off before calling.
 *
 * @param multiplex_ratio_reg_val Desired multiplex ratio step, from 1 to 64
 * @return void, nothing to return
 */
void SH1122Oled::set_multiplex_ratio(uint8_t multiplex_ratio_reg_val)
{
    uint8_t cmds[2] = {OLED_CMD_SET_MULTIPLEX_RATION, multiplex_ratio_reg_val};
    send_commands(cmds, 2);
}

/**
 * @brief Sets the DC-DC voltage converter status and switch frequency. Display must be powered off before calling.
 *
 * @param mod DC-DC register value, see section 12 of commands in SH1122 datasheet.
 * @return void, nothing to return
 */
void SH1122Oled::set_dc_dc_control_mod(uint8_t mod)
{
    uint8_t cmds[2] = {OLED_CMD_SET_DC_DC_CONTROL_MOD, mod};
    send_commands(cmds, 2);
}

/**
 * @brief Sets clock divide ratio/oscillator frequency of internal display clocks. Display must be powered off before calling.
 *
 * @param freq_reg_val Clock divide ratio register value, see section 17 of commands in SH1122 datasheet.
 * @return void, nothing to return
 */
void SH1122Oled::set_oscillator_freq(uint8_t freq_reg_val)
{
    uint8_t cmds[2] = {OLED_CMD_SET_OSCILLATOR_FREQ, freq_reg_val};
    send_commands(cmds, 2);
}

/**
 * @brief Sets display offset modifier. Display must be powered off before calling.
 *
 * @param mod Offset modifier value, see section 16 of commands in SH1122 datasheet.
 * @return void, nothing to return
 */
void SH1122Oled::set_display_offset_mod(uint8_t mod)
{
    uint8_t cmds[2] = {OLED_CMD_SET_DISP_OFFSET_MOD, mod};
    send_commands(cmds, 2);
}

/**
 * @brief Sets duration of precharge/discharge period of display. Display must be powered off before calling.
 *
 * @param period_reg_val Period register value, see section 18 of commands in SH1122 datasheet.
 * @return void, nothing to return
 */
void SH1122Oled::set_precharge_period(uint8_t period_reg_val)
{
    uint8_t cmds[2] = {OLED_CMD_SET_PRE_CHARGE_PERIOD, period_reg_val};
    send_commands(cmds, 2);
}

/**
 * @brief Sets common pad output voltage of display at deselect stage. Display must be powered off before calling.
 *
 * @param vcom_reg_val VCOM deselect level register value, see section 19 of commands in SH1122 datasheet.
 * @return void, nothing to return
 */
void SH1122Oled::set_vcom(uint8_t vcom_reg_val)
{
    uint8_t cmds[2] = {OLED_CMD_SET_VCOM, vcom_reg_val};
    send_commands(cmds, 2);
}

/**
 * @brief Sets segment pad output voltage level at precharge stage. Display must be powered off before calling.
 *
 * @param vseg_reg_val VSEGM precharge level register value, see section 20 of commands in SH1122 datasheet.
 * @return void, nothing to return
 */
void SH1122Oled::set_vseg(uint8_t vseg_reg_val)
{
    uint8_t cmds[2] = {OLED_CMD_SET_VSEG, vseg_reg_val};
    send_commands(cmds, 2);
}

/**
 * @brief Sets the current row address in display internal RAM. Display must be powered off before calling.
 *
 * @param row_addr Desired row address from 0x00 (POR) to 0x3F.
 * @return void, nothing to return
 */
void SH1122Oled::set_row_addr(uint8_t row_addr)
{
    uint8_t cmds[2] = {OLED_CMD_SET_ROW_ADDR, row_addr};
    send_commands(cmds, 2);
}

/**
 * @brief Sets the row address to be used as initial display line/COM0. Display must be powered off before calling.
 *
 * @param start_line Desired starting row address, from 0x00 (POR) to 0x3F.
 * @return void, nothing to return
 */
void SH1122Oled::set_start_line(uint8_t start_line)
{
    uint8_t cmd = OLED_CMD_SET_DISP_START_LINE | (start_line & 0x3F);

    send_commands(&cmd, 1);
}

/**
 * @brief Sets segment output discharge voltage level. Display must be powered off before calling.
 *
 * @param discharge_level VSEGM discharge level register value, see section 21 of commands in SH1122 datasheet.
 * @return void, nothing to return
 */
void SH1122Oled::set_vseg_discharge_level(uint8_t discharge_level)
{
    uint8_t cmd = OLED_CMD_SET_DISCHARGE_LEVEL | discharge_level;

    send_commands(&cmd, 1);
}

/**
 * @brief Sets the high column address of display. Display must be powered off before calling.
 *
 * @param high_column_addr High column address desired, from 0x10 to 0x17. (column address is 7 bits, with 3 msbs in higher column address register)
 * @return void, nothing to return
 */
void SH1122Oled::set_high_column_address(uint8_t high_column_addr)
{
    uint8_t cmd = OLED_CMD_SET_HIGH_COLUMN_ADDR | high_column_addr;

    send_commands(&cmd, 1);
}

/**
 * @brief Sets the low column address of display. Display must be powered off before calling.
 *
 * @param low_column_addr Low column address desired, from 0x00 to 0x0F. (column address is 7 bits, with 4 lsbs in lower column address register)
 * @return void, nothing to return
 */
void SH1122Oled::set_low_column_address(uint8_t low_column_addr)
{
    uint8_t cmd = OLED_CMD_SET_LOW_COLUMN_ADDR | low_column_addr;

    send_commands(&cmd, 1);
}

/**
 * @brief Inverts display pixel intensity. Display must be powered off before calling.
 *
 * @param inverted True if an inverted pixel intensity id desired, false if otherwise.
 * @return void, nothing to return
 */
void SH1122Oled::set_inverted_intensity(bool inverted)
{
    uint8_t cmd = 0;

    if (inverted)
    {
        cmd = OLED_CMD_SET_INV_DISPLAY;
    }
    else
    {
        cmd = OLED_CMD_SET_NORMAL_DISPLAY;
    }

    send_commands(&cmd, 1);
}

/**
 * @brief Change relationship between RAM column address and segment driver. Display must be powered off before calling.
 *
 * @param remapped True if remapped segment scheme is desired, false if otherwise. See section 8 of commands in SH1122 datasheet.
 * @return void, nothing to return
 */
void SH1122Oled::set_segment_remap(bool remapped)
{
    uint8_t cmd = 0;

    if (remapped)
        cmd = OLED_CMD_NORM_SEG_MAP | 0x01U;
    else
        cmd = OLED_CMD_NORM_SEG_MAP;

    send_commands(&cmd, 1);
}

/**
 * @brief Changes scan direction of display from 0 to N, to N to 0, thus vertically flipping display. Display must be powered off before calling.
 *
 * @param flipped True if a flipped orientation is desired, false if otherwise.
 * @return void, nothing to return
 */
void SH1122Oled::set_orientation(bool flipped)
{
    uint8_t cmd = 0;

    if (flipped)
        cmd = OLED_CMD_SCAN_N_TO_0;
    else
        cmd = OLED_CMD_SCAN_0_TO_N;

    send_commands(&cmd, 1);
}

/**
 * @brief Checks a passed glyph for EOL conditions.
 *
 * @param b Encoding of the glyph being checked.
 * @return ascii value/encoding of the glyph, 0x0ffff if EOL.
 */
uint16_t SH1122Oled::get_ascii_next(uint8_t b)
{
    if (b == 0U || b == '\n')
        return 0x0FFFFU;
    else
        return b;
}

/**
 * @brief Checks a passed glyph for EOL conditions.
 *
 * @param b Encoding of the glyph being checked.
 * @return utf8 value/encoding of the glyph, 0x0ffff if EOL.
 */
uint16_t SH1122Oled::get_utf8_next(uint8_t b)
{
    if (b == 0U || b == '\n')
        return 0x0FFFFU;

    if (utf8_state == 0U)
    {
        if (b >= 0xFCU) /* 6 byte sequence */
        {
            utf8_state = 5U;
            b &= 1U;
        }
        else if (b >= 0xF8U)
        {
            utf8_state = 4U;
            b &= 3U;
        }
        else if (b >= 0xF0U)
        {
            utf8_state = 3;
            b &= 7U;
        }
        else if (b >= 0xE0U)
        {
            utf8_state = 2U;
            b &= 15U;
        }
        else if (b >= 0xC0U)
        {
            utf8_state = 1U;
            b &= 0x01FU;
        }
        else
        {
            return b;
        }

        utf8_encoding = b;
        return 0x0FFFEU;
    }
    else
    {
        utf8_state--;
        utf8_encoding <<= 6U;
        b &= 0x03FU;
        utf8_encoding |= b;

        if (utf8_state != 0)
            return 0x0FFFEU;
    }

    return utf8_encoding;
}

/**
 * @brief Sets font info from desired font. Font info is to decode strings and glyphs.
 *
 * @param font A pointer to the first element of the respective font lookup table, font tables are located in fonts directory.
 * @return void, nothing to return
 */
void SH1122Oled::set_font_vars(const uint8_t* font)
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

/**
 * @brief Returns pointer to glyph data from current font lookup table.
 *
 * @param encoding Encoding of the glyph data is desired for.
 * @return a pointer to the first element of respective glyph data, NULL if not found.
 */
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

/**
 * @brief Sets up glyph decoding structure with values from glyph data for decoding process.
 *
 * @param encoding Encoding of the glyph being decoded.
 * @return void, nothing to return
 */
void SH1122Oled::font_setup_glyph_decode(sh1122_oled_font_decode_t* decode, const uint8_t* glyph_data)
{
    decode->decode_ptr = glyph_data;
    decode->bit_pos = 0;

    decode->glyph_width = font_decode_get_unsigned_bits(decode, font_info.bits_per_char_width);
    decode->glyph_height = font_decode_get_unsigned_bits(decode, font_info.bits_per_char_height);
}

/**
 * @brief Decodes bit values from font lookup table and returns them as an unsigned integer.
 *
 * @param cnt Amount to increment current bit position by.
 * @return Unsigned decoded values.
 */
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

/**
 * @brief Decodes bit values from font lookup table and returns them as a signed integer.
 *
 * @param cnt Amount to increment current bit position by.
 * @return Signed decoded values.
 */
int8_t SH1122Oled::font_decode_get_signed_bits(sh1122_oled_font_decode_t* decode, uint8_t cnt)
{
    int8_t val;
    int8_t d;
    val = static_cast<int8_t>(font_decode_get_unsigned_bits(decode, cnt));
    d = 1;
    cnt--;
    d <<= cnt;
    val -= d;

    return val;
}

/**
 * @brief Apply rotation to y coordinate of lines being drawn for current glyph such that they match passed direction.
 *
 * @param dy Target y position of the glyph being drawn.
 * @param x Local x position of glyph being drawn.
 * @param y Local y position of glyph being drawn.
 * @param dir The desired drawing direction of the font, default is left to right.
 * @return Rotated y value.
 */
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

/**
 * @brief Apply rotation to x coordinate of lines being drawn for current glyph such that they match passed direction.
 *
 * @param dx Target x position of the glyph being drawn.
 * @param x Local x position of glyph being drawn.
 * @param y Local y position of glyph being drawn.
 * @param dir The desired drawing direction of the font, default is left to right.
 * @return Rotated x value.
 */
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

/**
 * @brief Reads an 8 bit value from specified font lookup table.
 *
 * @param font Pointer to first element of font lookup table.
 * @param offset Offset from initial address of lookup table to element desired to be read.
 * @return Read 8-bit value read from lookup table.
 */
uint8_t SH1122Oled::font_lookup_table_read_char(const uint8_t* font, uint8_t offset)
{
    return *static_cast<const uint8_t*>(font + offset);
}

/**
 * @brief Reads a 16 bit value from specified font lookup table.
 *
 * @param font Pointer to first element of font lookup table.
 * @param offset Offset from initial address of lookup table to element desired to be read.
 * @return Read 16-bit value read from lookup table.
 */
uint16_t SH1122Oled::font_lookup_table_read_word(const uint8_t* font, uint8_t offset)
{
    uint16_t word;

    word = static_cast<uint16_t>(*static_cast<const uint8_t*>(font + offset));
    word <<= 8;
    word += static_cast<uint16_t>(*static_cast<const uint8_t*>(font + offset + 1U));

    return word;
}

/**
 * @brief Decodes a single pixel block (an intensity and the amount of pixels it repeats) from sh1122 RLE bitmap data.
 *
 * @param data_ptr Pointer to current pixel block in bitmap data. Incremented after read is completed to next pixel block.
 * @param r_val_lim Repeated value limit returned from data, total amount of pixels returned intensity repeats for.
 * @param intensity Grayscale intensity value returned from data.
 * @return void, nothing to return
 */
void SH1122Oled::bitmap_decode_pixel_block(const uint8_t** data_ptr, int16_t& r_val_lim, PixelIntensity& intensity)
{

    if (**data_ptr & BITMAP_DECODE_WORD_FLG_BIT)
        bitmap_read_word(data_ptr, r_val_lim, intensity);
    else
        bitmap_read_byte(data_ptr, r_val_lim, intensity);
}

/**
 * @brief Decodes a byte length pixel block from sh1122 RLE bitmap data.
 *
 * @param data_ptr Pointer to current pixel block in bitmap data. Incremented by 1 after read is completed to next pixel block.
 * @param r_val_lim Repeated value limit returned from data, total amount of pixels returned intensity repeats for.
 * @param intensity Grayscale intensity value returned from data.
 * @return void, nothing to return
 */
void SH1122Oled::bitmap_read_byte(const uint8_t** data_ptr, int16_t& r_val_lim, PixelIntensity& intensity)
{
    intensity = static_cast<PixelIntensity>((**data_ptr & BITMAP_DECODE_PIXEL_INTENSITY_MASK));
    r_val_lim = static_cast<int16_t>((**data_ptr & BITMAP_DECODE_R_VAL_B_MASK)) >> 5;
    *data_ptr += 1;
}

/**
 * @brief Decodes a word length pixel block from sh1122 RLE bitmap data.
 *
 * @param data_ptr Pointer to current pixel block in bitmap data. Incremented by 2 after read is completed to next pixel block.
 * @param r_val_lim Repeated value limit returned from data, total amount of pixels returned intensity repeats for.
 * @param intensity Grayscale intensity value returned from data.
 * @return void, nothing to return
 */
void SH1122Oled::bitmap_read_word(const uint8_t** data_ptr, int16_t& r_val_lim, PixelIntensity& intensity)
{
    intensity = static_cast<PixelIntensity>((*(*data_ptr + 1) & BITMAP_DECODE_PIXEL_INTENSITY_MASK));
    r_val_lim = static_cast<int16_t>(((**data_ptr & ~BITMAP_DECODE_WORD_FLG_BIT) << 3)) |
                static_cast<int16_t>((((*(*data_ptr + 1)) & BITMAP_DECODE_R_VAL_LOW_MASK) >> 5));
    *data_ptr += 2;
}

/**
 * @brief Decodes and draws a single glyph/character.
 *
 * @param decode Pointer to decode structure containing information about the glyph to be drawn.
 * @param glyph_data Pointer to the initial element of the data for the glyph to be drawn.
 * @return Amount for which x should be incremented before drawing the next glyph.
 */
int8_t SH1122Oled::font_decode_and_draw_glyph(sh1122_oled_font_decode_t* decode, const uint8_t* glyph_data)
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

/**
 * @brief Draws lines/pixels for glyph being drawn.
 *
 * @param decode Pointer to decode structure containing information about the glyph currently being drawn.
 * @param len Total number of pixels which must be drawn for specified glyph.
 * @param is_foreground 0 if pixels are background/whitespace around the glyph, 1 if the actual glyph content to be drawn.
 * @return void, nothing to return
 */
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
            font_draw_line(decode, x, y, current, static_cast<PixelIntensity>(decode->fg_intensity));
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

/**
 * @brief Draws a single font line, called from font_draw_lines
 *
 * @param decode Pointer to decode structure containing information about the glyph currently being drawn.
 * @param x Starting x position of line.
 * @param y Starting y position of line.
 * @param length Length of line in pixels.
 * @param intensity Grayscale intensity of the line being drawn.
 * @return void, nothing to return
 */
void SH1122Oled::font_draw_line(sh1122_oled_font_decode_t* decode, int16_t x, int16_t y, uint16_t length, PixelIntensity intensity)
{

    if (length != 0)
    {
        if (!SH1122_PIXEL_IN_BOUNDS(x, y))
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

/**
 * @brief Fill quadrants of an ellipse frame.
 *
 * @param outter_points Vector containing all outter ring points for quadrant.
 * @param inner_points Vector containing all inner ring points for quadrant.
 * @param y_c Ellipse center y position.
 * @param intensity Grayscale intensity of the ellipse fill.
 * @return void, nothing to return
 */
void SH1122Oled::fill_ellipse_frame_quadrant(
        std::vector<sh1122_2d_point_t>& outter_points, std::vector<sh1122_2d_point_t>& inner_points, int16_t y_c, PixelIntensity intensity)
{
    int16_t in_x = 0;
    int16_t in_y = 0;
    int16_t out_x = 0;
    int16_t out_y = 0;
    int16_t inner_elem = 0;

    // loop through all inner ring points
    for (int i = 0; i < outter_points.size(); i++)
    {
        inner_elem = -255;
        out_x = outter_points[i].x;
        out_y = outter_points[i].y;

        // check if there is an inner ring point with matching x location
        for (int j = 0; j < inner_points.size(); j++)
        {
            in_x = inner_points[j].x;
            in_y = inner_points[j].y;

            // save element and exit loop if found
            if (out_x == in_x)
            {
                inner_elem = j;
                break;
            }
        }

        if (inner_elem != -255)
        {
            // if matching point was found draw a vertical line between the two
            draw_line(out_x, out_y, out_x, in_y, intensity);
            // remove matching point from inner_points vector to increase search speed
            inner_points.erase(inner_points.begin() + inner_elem);
        }
        else
        {
            // no matching point found, draw line to vertical center of ellipse
            draw_line(out_x, out_y, out_x, y_c, intensity);
        }
    }
}

/**
 * @brief Sends commands to initialize display for use.
 *
 * @return void, nothing to return
 */
void SH1122Oled::default_init()
{
    reset();

    power_off();
    // send all initialization commands
    set_oscillator_freq(0x50U);
    set_multiplex_ratio(HEIGHT - 1U);
    set_display_offset_mod(0x00U);
    set_row_addr(0x00U);
    set_high_column_address(0x00U);
    set_low_column_address(0x00U);
    set_start_line(0x00U);
    set_vseg_discharge_level(0x00U);
    set_dc_dc_control_mod(0x80U);
    set_segment_remap(false);
    set_orientation(false);
    set_contrast(0x90);
    set_precharge_period(0x28U);
    set_vcom(0x30U);
    set_vseg(0x1EU);
    set_inverted_intensity(false);
    power_on(); // power back on oled

    // clear screen of any artifacts
    clear_buffer();
    update_screen();
}

/**
 * @brief Sends commands to SH1122 over SPI.
 *
 * @param cmds Pointer to buffer containing commands to be sent.
 * @param length Total length of command buffer.
 * @return void, nothing to return
 */
void SH1122Oled::send_commands(uint8_t* cmds, uint16_t length)
{
    gpio_set_level(oled_cfg.io_cs, 0);
    gpio_set_level(oled_cfg.io_dc, 0);
    spi_transaction_t t;
    spi_transaction_t* rt;
    t.length = length * 8;
    t.rxlength = 0;
    t.tx_buffer = cmds;
    t.rx_buffer = NULL;
    t.flags = 0;
    spi_device_queue_trans(spi_hdl, &t, portMAX_DELAY);
    spi_device_get_trans_result(spi_hdl, &rt, portMAX_DELAY);
    gpio_set_level(oled_cfg.io_cs, 1);
}

/**
 * @brief Sends data to SH1122 over SPI.
 *
 * @param cmds Pointer to buffer containing data to be sent.
 * @param length Total length of data buffer.
 * @return void, nothing to return
 */
void SH1122Oled::send_data(uint8_t* data, uint16_t length)
{
    gpio_set_level(oled_cfg.io_dc, 1);
    spi_transaction_t t;
    spi_transaction_t* rt;
    t.length = length * 8;
    t.rxlength = 0;
    t.tx_buffer = data;
    t.rx_buffer = NULL;
    t.flags = 0;
    spi_device_queue_trans(spi_hdl, &t, portMAX_DELAY);
    spi_device_get_trans_result(spi_hdl, &rt, portMAX_DELAY);
}
