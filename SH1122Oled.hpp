#pragma once

#include <stdio.h>
#include <vector>
#include <math.h>
#include <stdio.h>
#include <cstring>
#include <cstdarg>

#include <driver/gpio.h>
#include <driver/spi_common.h>
#include <driver/spi_master.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

typedef struct sh1122_oled_cfg_t
{
        spi_host_device_t spi_peripheral;
        gpio_num_t io_mosi;
        gpio_num_t io_sclk;
        gpio_num_t io_cs;
        gpio_num_t io_rst;
        gpio_num_t io_dc;
        spi_dma_chan_t dma_cha;

        sh1122_oled_cfg_t()
            : spi_peripheral(SPI3_HOST)
            , io_mosi(GPIO_NUM_4)
            , io_sclk(GPIO_NUM_18)
            , io_cs(GPIO_NUM_21)
            , io_rst(GPIO_NUM_22)
            , io_dc(GPIO_NUM_23)
            , dma_cha(SPI_DMA_CH_AUTO)
        {
        }

        sh1122_oled_cfg_t(spi_host_device_t spi_peripheral, gpio_num_t io_mosi, gpio_num_t io_sclk, gpio_num_t io_cs, gpio_num_t io_rst,
                gpio_num_t io_dc, spi_dma_chan_t dma_cha = SPI_DMA_CH_AUTO)
            : spi_peripheral(spi_peripheral)
            , io_mosi(io_mosi)
            , io_sclk(io_sclk)
            , io_cs(io_cs)
            , io_rst(io_rst)
            , io_dc(io_dc)
            , dma_cha(dma_cha)
        {
        }

} sh1122_oled_cfg_t;

class SH1122Oled
{
    public:
        enum class FontDirection
        {
            left_to_right,
            top_to_bottom,
            right_to_left,
            bottom_to_top
        };

        enum class PixelIntensity
        {
            level_0 = 0x00,
            level_1 = 0x11,
            level_2 = 0x22,
            level_3 = 0x33,
            level_4 = 0x44,
            level_5 = 0x55,
            level_6 = 0x66,
            level_7 = 0x77,
            level_8 = 0x88,
            level_9 = 0x99,
            level_10 = 0xAA,
            level_11 = 0xBB,
            level_12 = 0xCC,
            level_13 = 0xDD,
            level_14 = 0xEE,
            level_15 = 0xFF
        };

        SH1122Oled(sh1122_oled_cfg_t oled_cfg = default_oled_cfg);

        void update_screen();
        void clear_buffer();
        void set_pixel(uint16_t x, uint16_t y, PixelIntensity intensity);
        void draw_line(int16_t x_1, int16_t y_1, int16_t x_2, int16_t y_2, PixelIntensity intensity);
        void draw_rectangle_frame(int16_t x_1, int16_t y_1, int16_t width, int16_t height, int16_t thickness, PixelIntensity intensity);
        void draw_rectangle(int16_t x_1, int16_t y_1, int16_t width, int16_t height, PixelIntensity intensity);
        uint16_t draw_glyph(uint16_t x, uint16_t y, PixelIntensity intensity, uint16_t encoding);
        uint16_t draw_string(uint16_t x, uint16_t y, PixelIntensity intensity, const char* format, ...);
        uint16_t font_get_string_width(const char* format, ...);
        uint16_t font_get_string_height(const char* format, ...);
        uint16_t font_get_glyph_width(uint16_t encoding);
        uint16_t font_get_glyph_height(uint16_t encoding);
        uint16_t font_get_string_center_x(const char* str);
        uint16_t font_get_string_center_y(const char* str);
        static void load_font(const uint8_t* font);
        void set_font_direction(FontDirection dir);

        void reset();
        void power_off();
        void power_on();
        void set_contrast(uint8_t contrast_reg_val);
        void set_multiplex_ratio(uint8_t multiplex_ratio_reg_val);
        void set_dc_dc_control_mod(uint8_t mod);
        void set_oscillator_freq(uint8_t freq_reg_val);
        void set_display_offset_mod(uint8_t mod);
        void set_precharge_period(uint8_t period_reg_val);
        void set_vcom(uint8_t vcom_reg_val);
        void set_vseg(uint8_t vseg_reg_val);
        void set_row_addr(uint8_t row_addr);
        void set_start_line(uint8_t start_line);
        void set_vseg_discharge_level(uint8_t discharge_level);
        void set_high_column_address(uint8_t high_column_addr);
        void set_low_column_address(uint8_t low_column_addr);
        void set_rev_display(bool rev_dir);
        void set_segment_remap(bool rev_dir);
        void set_orientation(bool flipped);

        static const constexpr uint16_t WIDTH = 256U;
        static const constexpr uint16_t HEIGHT = 64U;
    private:
        enum
        {
            COMMAND,
            DATA
        };

        typedef struct sh1122_oled_font_info_t
        {
                const uint8_t* font;
                /* offset 0 */
                uint8_t glyph_cnt;
                uint8_t bbx_mode;
                uint8_t bits_per_0;
                uint8_t bits_per_1;

                /* offset 4 */
                uint8_t bits_per_char_width;
                uint8_t bits_per_char_height;
                uint8_t bits_per_char_x;
                uint8_t bits_per_char_y;
                uint8_t bits_per_delta_x;

                /* offset 9 */
                int8_t max_char_width;
                int8_t max_char_height; /* overall height, NOT ascent. Instead ascent = max_char_height + y_offset */
                int8_t x_offset;
                int8_t y_offset;

                /* offset 13 */
                int8_t ascent_A;
                int8_t descent_g; /* usually a negative value */
                int8_t ascent_para;
                int8_t descent_para;

                /* offset 17 */
                uint16_t start_pos_upper_A;
                uint16_t start_pos_lower_a;

                uint16_t start_pos_unicode;

        } sh1122_oled_font_info_t;

        typedef struct sh1122_oled_font_decode_t
        {
                const uint8_t* decode_ptr; // pointer to the glyph being decoded
                uint8_t bit_pos;
                int8_t glyph_width;    // glyph width
                int8_t glyph_height;   // glyph height
                int8_t x;              // current x position to be drawn at
                int8_t y;              // current y position to be drawn at
                uint16_t target_x;     // target x position of the glyph
                uint16_t target_y;     // target y position of the glyph
                int8_t glyph_x_offset; // glyph x offset used for string width calculations only
                uint8_t fg_intensity;  // foreground intensity to draw
        } sh1122_oled_font_decode_t;

        void default_init();
        void send_commands(uint8_t* cmds, uint16_t length);
        void send_data(uint8_t* data, uint16_t length);
        uint16_t get_ascii_next(uint8_t b);
        const uint8_t* font_get_glyph_data(uint16_t encoding);
        uint16_t font_get_glyph_width(sh1122_oled_font_decode_t* decode, uint16_t encoding);
        void font_setup_glyph_decode(sh1122_oled_font_decode_t* decode, const uint8_t* glyph_data);
        int8_t font_decode_glyph(sh1122_oled_font_decode_t* decode, const uint8_t* glyph_data);
        uint8_t font_decode_get_unsigned_bits(sh1122_oled_font_decode_t* decode, uint8_t cnt);
        int8_t font_decode_get_signed_bits(sh1122_oled_font_decode_t* decode, uint8_t cnt);
        uint16_t font_apply_direction_y(uint16_t dy, int8_t x, int8_t y, FontDirection dir);
        uint16_t font_apply_direction_x(uint16_t dx, int8_t x, int8_t y, FontDirection dir);
        static uint8_t font_lookup_table_read_char(const uint8_t* font, uint8_t offset);
        static uint16_t font_lookup_table_read_word(const uint8_t* font, uint8_t offset);
        void font_draw_lines(sh1122_oled_font_decode_t* decode, uint8_t len, uint8_t is_foreground);
        void draw_hv_line(sh1122_oled_font_decode_t* decode, int16_t x, int16_t y, uint16_t length, PixelIntensity intensity);

        sh1122_oled_cfg_t oled_cfg;
        spi_bus_config_t spi_bus_cfg;
        spi_device_interface_config_t oled_interface_cfg;
        spi_device_handle_t spi_hdl;

        static FontDirection font_dir;
        static sh1122_oled_cfg_t default_oled_cfg; ///< default oled config settings
        static sh1122_oled_font_info_t font_info;

        static const constexpr uint16_t FRAME_BUFFER_LENGTH = WIDTH * HEIGHT / 2;
        static uint8_t frame_buffer[FRAME_BUFFER_LENGTH];
        static const constexpr uint16_t FRAME_CHUNK_1_LENGTH = FRAME_BUFFER_LENGTH / 3;
        static const constexpr uint16_t FRAME_CHUNK_2_LENGTH = FRAME_CHUNK_1_LENGTH;
        static const constexpr uint16_t FRAME_CHUNK_3_LENGTH = FRAME_BUFFER_LENGTH - FRAME_CHUNK_1_LENGTH - FRAME_CHUNK_2_LENGTH;
        static const constexpr uint16_t FRAME_CHUNK_LENGTHS[3] = {FRAME_CHUNK_1_LENGTH, FRAME_CHUNK_2_LENGTH, FRAME_CHUNK_3_LENGTH};

        // spi
        static const constexpr uint64_t SPI_CLK_SPEED_HZ = 12000000ULL;
        static const constexpr uint64_t SPI_TRANS_TIMEOUT_MS = 10ULL;
        static const constexpr uint8_t SPI_INTERRUPT_MODE_QUEUE_SIZE = 1;

        // commands
        static const constexpr uint8_t OLED_CMD_POWER_ON = 0xAF;
        static const constexpr uint8_t OLED_CMD_POWER_OFF = 0xAE;
        static const constexpr uint8_t OLED_CMD_SET_ROW_ADDR = 0xB0;
        static const constexpr uint8_t OLED_CMD_SCAN_0_TO_N = 0xC0;  // scan rows from bottom to top
        static const constexpr uint8_t OLED_CMD_SCAN_N_TO_0 = 0xC8;  // scan rows from top to bottom
        static const constexpr uint8_t OLED_CMD_NORM_SEG_MAP = 0xA0; // regular segment driver output pad assignment
        static const constexpr uint8_t OLED_CMD_REV_SEG_MAP = 0xA1;  // reversed segment driver output pads
        static const constexpr uint8_t OLED_CMD_SET_MULTIPLEX_RATION = 0xA8;
        static const constexpr uint8_t OLED_CMD_SET_DC_DC_CONTROL_MOD = 0xAD; // onboard oled DC-DC voltage converter status and switch frequency
        static const constexpr uint8_t OLED_CMD_SET_OSCILLATOR_FREQ = 0xD5;
        static const constexpr uint8_t OLED_CMD_SET_DISP_START_LINE = 0x40;
        static const constexpr uint8_t OLED_CMD_SET_DISP_CONTRAST = 0x81;
        static const constexpr uint8_t OLED_CMD_SET_DISP_OFFSET_MOD = 0xD3;
        static const constexpr uint8_t OLED_CMD_SET_PRE_CHARGE_PERIOD = 0xD9;
        static const constexpr uint8_t OLED_CMD_SET_VCOM = 0xDB;
        static const constexpr uint8_t OLED_CMD_SET_VSEG = 0xDC;
        static const constexpr uint8_t OLED_CMD_SET_DISCHARGE_LEVEL = 0x30;
        static const constexpr uint8_t OLED_CMD_SET_NORMAL_DISPLAY = 0xA6;
        static const constexpr uint8_t OLED_CMD_SET_REV_DISPLAY = 0xA7;
        static const constexpr uint8_t OLED_CMD_SET_HIGH_COLUMN_ADDR = 0x10;
        static const constexpr uint8_t OLED_CMD_SET_LOW_COLUMN_ADDR = 0x00;

        static const constexpr char* TAG = "SH1122Oled";
};