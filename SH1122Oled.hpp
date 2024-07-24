#pragma once

// standard library includes
#include <stdio.h>
#include <vector>
#include <math.h>
#include <stdio.h>
#include <cstring>
#include <cstdarg>

// esp-idf includes
#include <driver/gpio.h>
#include <driver/spi_common.h>
#include <driver/spi_master.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>

#define SH1122_PIXEL_IN_BOUNDS(x, y) (((x >= 0) && (x < SH1122Oled::WIDTH)) && ((y >= 0) && (y < SH1122Oled::HEIGHT)))

/// @brief OLED configuration settings structure passed into SH1122Oled constructor
typedef struct sh1122_oled_cfg_t
{
        spi_host_device_t spi_peripheral; ///<SPI peripheral/host device to be used
        gpio_num_t io_mosi;               ///<MOSI GPIO pin, connects to display SDA pin
        gpio_num_t io_sclk;               ///<SCLK GPIO pin, connects to display SCL pin
        gpio_num_t io_cs;                 ///< Chip select GPIO pin, connects to display CS pin
        gpio_num_t io_rst;                ///< Reset GPIO pin, connects to display RST pin
        gpio_num_t io_dc;                 ///< Data/Command GPIO pin, connects to display DC pin.
        spi_dma_chan_t dma_cha;           ///<DMA channel to be used for SPI

#ifdef ESP32C3_SH1122_CONFIG
        /// @brief Default display configuration settings constructor for ESP32-C3, add
        /// add_compile_definitions("ESP32C3_SH1122_CONFIG") to CMakeList to use
        sh1122_oled_cfg_t()
            : spi_peripheral(SPI2_HOST)
            , io_mosi(GPIO_NUM_4)
            , io_sclk(GPIO_NUM_18)
            , io_cs(GPIO_NUM_5)
            , io_rst(GPIO_NUM_7)
            , io_dc(GPIO_NUM_6)
            , dma_cha(SPI_DMA_CH_AUTO)
        {
        }
#elif defined(ESP32C6_IMU_CONFIG)
        /// @brief Default display configuration settings constructor for ESP32-C6, add
        /// add_compile_definitions("ESP32C6_SH1122_CONFIG") to CMakeList to use
        sh1122_oled_cfg_t()
            : spi_peripheral(SPI2_HOST)
            , io_mosi(GPIO_NUM_4)
            , io_sclk(GPIO_NUM_18)
            , io_cs(GPIO_NUM_5)
            , io_rst(GPIO_NUM_7)
            , io_dc(GPIO_NUM_6)
            , dma_cha(SPI_DMA_CH_AUTO)
        {
        }
#else
        /// @brief Default display configuration settings constructor for ESP32
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

#endif

        /// @brief Overloaded display configuration settings constructor for custom pin settings
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

/**
 *
 * @brief SH1122 OLED driver class.
 *
 * Controls an SH1122 driven OLED display interfaced via SPI.
 * Contains methods to draw to the display and modify its settings.
 *
 * @author Myles Parfeniuk
 *
 */
class SH1122Oled
{
    public:
        /// @brief Drawing directions for strings, passed to set_font_direction().
        enum class FontDirection
        {
            left_to_right,
            top_to_bottom,
            right_to_left,
            bottom_to_top
        };

        /// @brief Pixel intensity/grayscale level passed to drawing functions, SH1122 supports 16 different shades.
        enum class PixelIntensity
        {
            level_0,
            level_1,
            level_2,
            level_3,
            level_4,
            level_5,
            level_6,
            level_7,
            level_8,
            level_9,
            level_10,
            level_11,
            level_12,
            level_13,
            level_14,
            level_15,
            level_transparent,
            max
        };

        SH1122Oled(sh1122_oled_cfg_t settings = sh1122_oled_cfg_t());
        void update_screen();
        void clear_buffer();

        void set_pixel(uint16_t x, uint16_t y, PixelIntensity intensity);
        void draw_line(int16_t x_1, int16_t y_1, int16_t x_2, int16_t y_2, PixelIntensity intensity);
        void draw_rectangle_frame(int16_t x_1, int16_t y_1, int16_t width, int16_t height, int16_t thickness, PixelIntensity intensity);
        void draw_rectangle(int16_t x_1, int16_t y_1, int16_t width, int16_t height, PixelIntensity intensity);
        void draw_circle_frame(int16_t x_c, int16_t y_c, int16_t r, int16_t thickness, PixelIntensity intensity);
        void draw_circle(int16_t x_c, int16_t y_c, int16_t r, PixelIntensity intensity);
        void draw_ellipse_frame(int16_t x_c, int16_t y_c, int16_t r_x, int16_t r_y, int16_t thickness, PixelIntensity intensity);
        void draw_ellipse(int16_t x_c, int16_t y_c, int16_t r_x, int16_t r_y, PixelIntensity intensity);
        uint16_t draw_glyph(uint16_t x, uint16_t y, PixelIntensity intensity, uint16_t encoding);
        uint16_t draw_string(uint16_t x, uint16_t y, PixelIntensity intensity, const char* format, ...);
        void draw_bitmap(uint16_t x, uint16_t y, const uint8_t* bitmap, PixelIntensity bg_intensity = PixelIntensity::level_transparent);

        static void load_font(const uint8_t* font);
        void set_font_direction(FontDirection dir);
        uint16_t font_get_string_width(const char* format, ...);
        uint16_t font_get_string_height(const char* format, ...);
        uint16_t font_get_glyph_width(uint16_t encoding);
        uint16_t font_get_glyph_height(uint16_t encoding);
        uint16_t font_get_string_center_x(const char* str);
        uint16_t font_get_string_center_y(const char* str);

        void take_screen_shot();

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
        void set_inverted_intensity(bool inverted);
        void set_segment_remap(bool remapped);
        void set_orientation(bool flipped);

        static const constexpr uint16_t WIDTH = 256U; ///<Display width
        static const constexpr uint16_t HEIGHT = 64U; ///<Display height

    private:
        /// @brief Font information structure, used to contain information about the currently loaded font.
        typedef struct sh1122_oled_font_info_t
        {
                const uint8_t* font; ///<Pointer to first element of font lookup table.
                /* offset 0 */
                uint8_t glyph_cnt;  ///<Total amount of glyphs contained within the font.
                uint8_t bbx_mode;   ///<BBX build mode of font 0: proportional, 1: common height, 2: monospace, 3: multiple of 8
                uint8_t bits_per_0; ///< Glyph RLE (run length encoding) parameter, max bits per background line.
                uint8_t bits_per_1; ///< Glyph RLE (run length encoding) parameter, max bits per foreground line.

                /* offset 4 */
                uint8_t bits_per_char_width;  ///< Glyph RLE (run length encoding) parameter, bits per char width data.
                uint8_t bits_per_char_height; ///<Glyph RLE (run length encoding) parameter, bits per char height data.
                uint8_t bits_per_char_x;      ///<Glyph RLE (run length encoding) parameter, bits per local char x position.
                uint8_t bits_per_char_y;      ///<Glyph RLE (run length encoding) parameter, bits per local char y position.
                uint8_t bits_per_delta_x;     ///<Glyph RLE (run length encoding) parameter, bits per change in x position.

                /* offset 9 */
                int8_t max_char_width;  ///<Max glyph width of any glyphs contained within font.
                int8_t max_char_height; ///<Max glyph height of any glyphs contained within font.
                int8_t x_offset;        ///< x offset
                int8_t y_offset;        ///< y offset

                /* offset 13 */
                int8_t ascent_A;     ///< Ascent of capital A (usually glyph with highest ending y position)
                int8_t descent_g;    ///< Descent of lowercase g (usually glyph with lowest starting y position)
                int8_t ascent_para;  ///< Ascent of '(' glyph.
                int8_t descent_para; ///< Descent of ')' glyph.

                /* offset 17 */
                uint16_t start_pos_upper_A; ///< Starting offset for uppercase lookup table.
                uint16_t start_pos_lower_a; ///< Starting offset for lowercase lookup table.

                uint16_t start_pos_unicode; ///< Starting offset for unicode (16 bit encoded glyphs) lookup table.

        } sh1122_oled_font_info_t;

        /// @brief Glyph decode information structure, used to contain information about glyphs being decoded and drawn.
        typedef struct sh1122_oled_font_decode_t
        {
                const uint8_t* decode_ptr; ///< Pointer to the glyph data being decoded
                uint8_t bit_pos;           ///< Current bit position in decoding/drawing process
                int8_t glyph_width;        ///< Glyph width
                int8_t glyph_height;       ///< Glyph height
                int8_t x;                  ///< Current x position to be drawn at
                int8_t y;                  ///< Current y position to be drawn at
                uint16_t target_x;         ///< Target x position of the glyph
                uint16_t target_y;         ///< Target y position of the glyph
                int8_t glyph_x_offset;     ///< Glyph x offset used for string width calculations only
                uint8_t fg_intensity;      ///< Foreground intensity to draw
        } sh1122_oled_font_decode_t;

        /// @brief Represents point on OLED screen, used in drawing functions
        typedef struct sh1122_2d_point_t
        {
                int16_t x;
                int16_t y;
        } sh1122_2d_point_t;

        void default_init();
        void send_commands(uint8_t* cmds, uint16_t length);
        void send_data(uint8_t* data, uint16_t length);
        void fill_ellipse_frame_quadrant(
                std::vector<sh1122_2d_point_t>& outter_points, std::vector<sh1122_2d_point_t>& inner_points, int16_t y_c, PixelIntensity intensity);
        uint16_t get_ascii_next(uint8_t b);
        const uint8_t* font_get_glyph_data(uint16_t encoding);
        uint16_t font_get_glyph_width(sh1122_oled_font_decode_t* decode, uint16_t encoding);
        void font_setup_glyph_decode(sh1122_oled_font_decode_t* decode, const uint8_t* glyph_data);
        int8_t font_decode_and_draw_glyph(sh1122_oled_font_decode_t* decode, const uint8_t* glyph_data);
        uint8_t font_decode_get_unsigned_bits(sh1122_oled_font_decode_t* decode, uint8_t cnt);
        int8_t font_decode_get_signed_bits(sh1122_oled_font_decode_t* decode, uint8_t cnt);
        uint16_t font_apply_direction_y(uint16_t dy, int8_t x, int8_t y, FontDirection dir);
        uint16_t font_apply_direction_x(uint16_t dx, int8_t x, int8_t y, FontDirection dir);
        void font_draw_lines(sh1122_oled_font_decode_t* decode, uint8_t len, uint8_t is_foreground);
        void font_draw_line(sh1122_oled_font_decode_t* decode, int16_t x, int16_t y, uint16_t length, PixelIntensity intensity);
        static uint8_t font_lookup_table_read_char(const uint8_t* font, uint8_t offset);
        static uint16_t font_lookup_table_read_word(const uint8_t* font, uint8_t offset);

        void bitmap_decode_pixel_block(const uint8_t** data_ptr, int16_t& r_val_lim, PixelIntensity& intensity);
        void bitmap_read_byte(const uint8_t** data_ptr, int16_t& r_val_lim, PixelIntensity& intensity);
        void bitmap_read_word(const uint8_t** data_ptr, int16_t& r_val_lim, PixelIntensity& intensity);

        static sh1122_oled_cfg_t oled_cfg;                ///< Holds configuration struct passed to constructor, used for GPIO pins and SPI
        spi_bus_config_t spi_bus_cfg;                     ///< SPI peripheral config struct.
        spi_device_interface_config_t oled_interface_cfg; ///< SPI interface struct for SH1122
        spi_device_handle_t spi_hdl;                      ///< Handle to perform SPI transactions with.

        static FontDirection font_dir;            ///< The currently selected font direction, default is left to right.
        static sh1122_oled_font_info_t font_info; ///< Contains information about the currently loaded font.

        static const constexpr uint16_t FRAME_BUFFER_LENGTH = WIDTH * HEIGHT / 2; ///< Length of frame buffer being sent over SPI.
        static uint8_t frame_buffer[FRAME_BUFFER_LENGTH];                         ///< Frame buffer to contain pixel data being sent over SPI.
        static const constexpr uint16_t FRAME_CHUNK_1_LENGTH =
                FRAME_BUFFER_LENGTH /
                3; ///< Length of first frame chunk being sent over SPI (frame buffer length exceeds esp-idf's SPI max transfer sz)
        static const constexpr uint16_t FRAME_CHUNK_2_LENGTH =
                FRAME_CHUNK_1_LENGTH; ///< Length of second frame chunk being sent over SPI (frame buffer length exceeds esp-idf's SPI max transfer sz)
        static const constexpr uint16_t FRAME_CHUNK_3_LENGTH =
                FRAME_BUFFER_LENGTH - FRAME_CHUNK_1_LENGTH -
                FRAME_CHUNK_2_LENGTH; ///< Length of third frame chunk being sent over SPI (frame buffer length exceeds esp-idf's SPI max transfer sz)
        static const constexpr uint16_t FRAME_CHUNK_LENGTHS[3] = {FRAME_CHUNK_1_LENGTH, FRAME_CHUNK_2_LENGTH,
                FRAME_CHUNK_3_LENGTH}; ///< Length of frame chunks being sent over SPI (frame buffer length exceeds esp-idf's SPI max transfer sz)

        // bitmap decoding
        static const constexpr uint8_t BITMAP_DECODE_WORD_FLG_BIT = BIT7;    ///< Indicates word length pixel block.
        static const constexpr uint16_t BITMAP_DECODE_R_VAL_LOW_BIT_POS = 5; ///< Shift for lower repeated value bits.
        static const constexpr uint16_t BITMAP_DECODE_R_VAL_LOW_MASK =
                (BIT7 | BIT6 | BIT5); ///< Mask for isolating the 3 lsbs of repeated value count with word length pixel block.
        static const constexpr uint16_t BITMAP_DECODE_R_VAL_B_MASK =
                (BIT6 | BIT5); ///< Mask for isolating repeated value count with byte length pixel block.
        static const constexpr uint8_t BITMAP_DECODE_PIXEL_INTENSITY_MASK =
                (BIT4 | BIT3 | BIT2 | BIT1 | BIT0); ///< Mask for isolating grayscale intensity value.

        // spi
        static const constexpr uint64_t SPI_CLK_SPEED_HZ = 8000000ULL;    ///< Serial clockspeed of SPI transactions.
        static const constexpr uint64_t SPI_TRANS_TIMEOUT_MS = 10ULL;     ///< Timeout for SPI transactions.
        static const constexpr uint8_t SPI_INTERRUPT_MODE_QUEUE_SIZE = 1; ///< Maximum amount of queued SPI transactions.

        // commands
        static const constexpr uint8_t OLED_CMD_POWER_ON = 0xAF;             ///< Power on command.
        static const constexpr uint8_t OLED_CMD_POWER_OFF = 0xAE;            ///< Power off command.
        static const constexpr uint8_t OLED_CMD_SET_ROW_ADDR = 0xB0;         ///< Set row address command.
        static const constexpr uint8_t OLED_CMD_SCAN_0_TO_N = 0xC0;          ///< Scan from bottom to top command.
        static const constexpr uint8_t OLED_CMD_SCAN_N_TO_0 = 0xC8;          ///< Scan from top to bottom command.
        static const constexpr uint8_t OLED_CMD_NORM_SEG_MAP = 0xA0;         ///< Regular segment driver output pad assignment command.
        static const constexpr uint8_t OLED_CMD_REV_SEG_MAP = 0xA1;          ///< Reversed segment driver output pads assignment command.
        static const constexpr uint8_t OLED_CMD_SET_MULTIPLEX_RATION = 0xA8; ///< Multiplex ratio set command.
        static const constexpr uint8_t OLED_CMD_SET_DC_DC_CONTROL_MOD =
                0xAD; ///< Set onboard oled DC-DC voltage converter status and switch frequency command.
        static const constexpr uint8_t OLED_CMD_SET_OSCILLATOR_FREQ = 0xD5;   ///< Set display clock frequency command.
        static const constexpr uint8_t OLED_CMD_SET_DISP_START_LINE = 0x40;   ///< Set display starting row address command.
        static const constexpr uint8_t OLED_CMD_SET_DISP_CONTRAST = 0x81;     ///< Set display contrast command.
        static const constexpr uint8_t OLED_CMD_SET_DISP_OFFSET_MOD = 0xD3;   ///< Set display offset command.
        static const constexpr uint8_t OLED_CMD_SET_PRE_CHARGE_PERIOD = 0xD9; ///< Set precharge period command.
        static const constexpr uint8_t OLED_CMD_SET_VCOM = 0xDB;              ///< Set common pad output voltage at deselect command.
        static const constexpr uint8_t OLED_CMD_SET_VSEG = 0xDC;              ///< Set segment pad output voltage at precharge stage.
        static const constexpr uint8_t OLED_CMD_SET_DISCHARGE_LEVEL = 0x30;   ///< Set segment output discharge voltage level command.
        static const constexpr uint8_t OLED_CMD_SET_NORMAL_DISPLAY = 0xA6;    ///< Set non inverted pixel intensity command.
        static const constexpr uint8_t OLED_CMD_SET_INV_DISPLAY = 0xA7;       ///< Set inverted pixel intensity command.
        static const constexpr uint8_t OLED_CMD_SET_HIGH_COLUMN_ADDR = 0x10;  ///< Set high column address command.
        static const constexpr uint8_t OLED_CMD_SET_LOW_COLUMN_ADDR = 0x00;   ///< Set low column address command.

        static const constexpr char* TAG = "SH1122Oled"; ///< Class tag, used in debug statements.
};