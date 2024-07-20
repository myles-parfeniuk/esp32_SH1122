<a name="readme-top"></a>
![image](README_images/esp32_SH1122_banner.png)
<summary>Table of Contents</summary>
<ol>
<li>
    <a href="#about">About</a>
</li>
<li>
    <a href="#features">Features</a>
    <ul>
    <li><a href="#custom-bitmaps">Custom Bitmaps</a></li>
    <li><a href="#u8g2-font-support">U8G2 Font Support</a></li>
    <li><a href="#screenshots">Screenshots</a></li>
    </ul>
</li>
<li>
    <a href="#getting-started">Getting Started</a>
    <ul>
    <li><a href="#wiring">Wiring</a></li>
    <li><a href="#adding-to-project">Adding to Project</a></li>
    <li><a href="#example">Example</a></li>
    </ul>
</li>
<li><a href="#documentation">Documentation</a></li>
<li><a href="#acknowledgements">Acknowledgements</a></li>  <!-- Added this line -->
<li><a href="#license">License</a></li>
<li><a href="#contact">Contact</a></li>
</ol>

## About

esp32_SH1122 is a C++ esp-idf v5.x component, intended to serve as a driver for SH1122 driven 256x64 OLED displays with 16 shades of grayscale.  
This library contains functions for basic graphics like lines, rectangles, and ellipses. It also contains support
for strings and bitmaps.  

## Features
<p align="right">(<a href="#readme-top">back to top</a>)</p>

### Custom Bitmaps
This library includes tools for encoding images into 16-shade grayscale bitmaps. The bitmaps use a custom run-length encoding
format to minimize memory usage as much as possible.  

To create bitmaps, place .png images into the python_utilities/bitmap_encoder/input directory, then run the sh1122_encode_bitmap.py
script. If you would like to preserve transparency ensure you use the '-t' flag,  for more info see the header at the top of sh1122_encode_bitmap.py.  

The encoded bitmaps will be saved as .hpp files within the python_utilities/bitmap_encoder/output directory. Copy and paste
the created bitmaps into the ./bitmaps directory and include them into your project. To draw, call draw_bitmap and pass it the  
respective bitmap.

Below are a few encoded bitmaps on the OLED next to their original source images.

![image](README_images/custom_bitmaps.jpg)
![image](README_images/pig.png)
![image](README_images/pickle_boat.png)
![image](README_images/alert_robot.png)
<p align="right">(<a href="#readme-top">back to top</a>)</p>

### U8G2 Font Support
This library contains close to 2000 different fonts from the U8G2 display library.  
The fonts names remain the same; however, they have "sh1122_font" appended onto the front instead of u8g2_font.  

A list of the fonts available within this library can be found here:  
https://github.com/olikraus/u8g2/wiki/fntlistall  

To use a font, include the respective font's header file, then call load_font(), passing it the font look up table.  
Ensure you load a font before attempting to draw strings or glyphs.

<p align="right">(<a href="#readme-top">back to top</a>)</p>

### Screenshots
This library contains tools for creating screenshots and gifs for use in documentation.  
  
To take screen shots call the take_screenshot() method, while running the sh1122_screenshot.py  
script, located under python_utilities/screenshot. The take_screenshot() method dumps the current frame buffer  
over serial, which is processed into an image by the script.  
  
For minimal latency you should increase your console baud rate in menuconfig to the maximum possible value.  
For more info see the header at the top of sh1122_screenshot.py.  

Running the sh1122_gif_creator.py script will create a gif from any screenshots located in the output folder generated  
by sh1122_screenshot.py. An example of this is the splash screen recording below this text.  

![image](README_images/splash_screen_recording.gif)
![image](README_images/splash_screen_real.gif)


<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Getting Started
<p align="right">(<a href="#readme-top">back to top</a>)</p>

### Wiring
The default wiring is depicted below, it can be changed at driver initialization (see example section).
![image](README_images/esp32_SH1122_wiring.png)
<p align="right">(<a href="#readme-top">back to top</a>)</p>

### Adding to Project
1. Create a "components" directory in the root workspace directory of your esp-idf project if it does not exist already.  

   In workspace directory:     
   ```sh
   mkdir components
   ```


2. Cd into the components directory and clone the esp32_SH1122 repo.

   ```sh
   cd components
   git clone https://github.com/myles-parfeniuk/esp32_SH1122.git
   ```

3. Ensure you clean your esp-idf project before rebuilding.  
   Within esp-idf enabled terminal:
   ```sh
    idf.py fullclean
   ```
<p align="right">(<a href="#readme-top">back to top</a>)</p>

### Example
This example draws a string on a dark gray background.
```cpp  
#include <stdio.h>
#include "SH1122Oled.hpp"
#include "fonts/sh1122_font_inr16_mf.hpp" //include any fonts you wish to use

extern "C" void app_main(void)
{
    SH1122Oled oled; //using default initialization

    //custom initialization example:
    /*SH1122Oled oled(sh1122_oled_cfg_t(SPI3_HOST, //SPI peripheral
                                        GPIO_NUM_4, //sda GPIO
                                        GPIO_NUM_18, //sclk GPIO
                                        GPIO_NUM_21, //chip select GPIO
                                        GPIO_NUM_22, //reset GPIO
                                        GPIO_NUM_23) //data/!command GPIO
                                        );
    */

    while (1)
    {

        oled.clear_buffer(); // clear buffer of previous frame before drawing

        // draw background
        oled.draw_rectangle(0, 0, SH1122Oled::WIDTH, SH1122Oled::HEIGHT, SH1122Oled::PixelIntensity::level_1);

        // draw screen border
        oled.draw_rectangle_frame(0, 0, SH1122Oled::WIDTH, SH1122Oled::HEIGHT, 2, SH1122Oled::PixelIntensity::level_10);

        // draw the string
        SH1122Oled::load_font(sh1122_font_inr16_mf); // load font for drawing string
        const int x = oled.font_get_string_center_x("esp32_SH1122");  // find the string x position for horizontal centering
        const int y = oled.font_get_string_center_y("esp32_SH1122");  // find the string y position for vertical centering
        oled.draw_string(x, y, SH1122Oled::PixelIntensity::level_15, "esp32_SH1122"); 

        oled.update_screen(); // send the current buffer to the screen

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}
```
<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Documentation
API documentation generated with doxygen can be found in the documentation directory of the master branch.  
<p align="right">(<a href="#readme-top">back to top</a>)</p>


## Acknowledgements
Thanks to Oliver Krause and contributors of u8g2.  
https://github.com/olikraus/u8g2  
This library utilizes the same run-line encoding system used by U8G2 for fonts.  
<p align="right">(<a href="#readme-top">back to top</a>)</p>

## License

Distributed under the MIT License. See `LICENSE.md` for more information.
<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Contact

Myles Parfeniuk - myles.parfenyuk@gmail.com

Project Link: [https://github.com/myles-parfeniuk/esp32_SH1122.git](https://github.com/myles-parfeniuk/esp32_SH1122.git)
<p align="right">(<a href="#readme-top">back to top</a>)</p>