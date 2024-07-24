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
    <li><a href="#creating-bitmaps">Creating Bitmaps</a></li>
    <li><a href="#font-system">Font System</a></li>
    <li><a href="#taking-screenshots">Taking Screenshots</a></li>
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

See the [Creating Bitmaps](#creating-bitmaps) for more info.

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

For more on using fonts, see [Font System](#font-system).

<p align="right">(<a href="#readme-top">back to top</a>)</p>

### Screenshots
This library contains tools for creating screenshots and gifs for use in documentation.  
  
See [Taking Screenshots](#taking-screenshots) for more info.

Below is an example of screen recording gif displaying a splash screen.  

![image](README_images/splash_screen_recording.gif)
![image](README_images/splash_screen_real.gif)


<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Getting Started
<p align="right">(<a href="#readme-top">back to top</a>)</p>

### Wiring
The default wiring is depicted below, it can be changed at driver initialization (see [Example](#example)).
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

    // load font for drawing string
    SH1122Oled::load_font(sh1122_font_inr16_mf); 

    while (1)
    {

        oled.clear_buffer(); // clear buffer of previous frame before drawing

        // draw background
        oled.draw_rectangle(0, 0, SH1122Oled::WIDTH, SH1122Oled::HEIGHT, SH1122Oled::PixelIntensity::level_1);

        // draw screen border
        oled.draw_rectangle_frame(0, 0, SH1122Oled::WIDTH, SH1122Oled::HEIGHT, 2, SH1122Oled::PixelIntensity::level_10);

        // draw the string
        const int x = oled.font_get_string_center_x("esp32_SH1122");  // find the string x position for horizontal centering
        const int y = oled.font_get_string_center_y("esp32_SH1122");  // find the string y position for vertical centering
        oled.draw_string(x, y, SH1122Oled::PixelIntensity::level_15, "esp32_SH1122"); 

        oled.update_screen(); // send the current buffer to the screen

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}
```
<p align="right">(<a href="#readme-top">back to top</a>)</p>

### Creating Bitmaps
To create bitmaps:

1.  Place .png images into the `python_utilities/bitmap_encoder/input` directory.

2.  Run the `sh1122_encode_bitmap.py` script and append any arguments on you see fit.   

    From terminal with python in path:

    ```sh
        python sh1122_encode_bitmap.py
    ```

    To see a full list of argument flags type:
    ```sh
        python sh1122_encode_bitmap.py -help
    ```

3. Copy the .hpp files generated by the script from `python_utilities/bitmap_encoder/output` to `your_project/components/esp32_SH1122/bitmaps`

4. To draw a bitmap include the respective .hpp file at the top of your project and call draw bitmap.
    ```cpp
        #include "SH1122Oled.hpp"
        #include "bitmaps/sh1122_bitmap_your_file.hpp"
        //...
        //somewhere in your code
        oled.draw_bitmap(0, 0, sh1122_bitmap_your_file);
    ```
<p align="right">(<a href="#readme-top">back to top</a>)</p>

### Font System
All the font files are located within the `fonts/` directory. Ensure you load a font before calling functions like
`draw_string()`, `draw_glyph()`, `get_string_width()`, etc...  
   
A full list of included fonts can be found at:  
https://github.com/olikraus/u8g2/wiki/fntlistall

To use a font:

1. Include the respective font's header file at the top of your file:
    ```cpp
    #include "fonts/sh1122_font_inr16_mf.hpp"
    ```
2. Call load_font(), passing it the font look up table:
    ```cpp
    SH1122Oled::load_font(sh1122_font_inr16_mf); 
    ```

3. Call whichever draw functions you like, for example:
    ```cpp
    oled.draw_string(x, y, SH1122Oled::PixelIntensity::level_15, "my string"); 
    ```
    String functions also support variable argument lists like printf:

    ```cpp
    int my_number = 1122;
    oled.draw_string(x, y, SH1122Oled::PixelIntensity::level_15, "my number %d", my_number); 
    ```
<p align="right">(<a href="#readme-top">back to top</a>)</p>

### Taking Screenshots
To take screen shots:

1.  Call the take_screenshot() method somewhere in your firmware, this function dumps the current frame buffer over serial.
    ```cpp
    oled.take_screen_shot()
    ```
2. While your firmware is flashed and executing, run the `sh1122_screenshot.py` script located in `python_utilities/screenshot`.  
    Ensure you close the esp-idf serial monitor if you have it open and append the comport name as well as baud rate onto the script execution call, for example:

    ```sh
    python sh1122_screenshot.py COM3 -b 115200
    ```

3. The script will display how many screen shots it has captured each time a new one is received, press the q key once you are done to save the images.

4. The output images (in png format) can be found in the `python_utilities/screenshot/output` directory.  


**NOTE:**
For minimal latency you should increase your console baud rate in menuconfig to the maximum possible value.  
For more info see the header at the top of sh1122_screenshot.py.  

To create gifs from any captured screenshots simply run `sh1122_gif_creator.py`, it will account for each frame duration according to the time stamps in the filenames of the screenshots.

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