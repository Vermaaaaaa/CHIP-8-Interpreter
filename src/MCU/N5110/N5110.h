/*  N5110 Class
*   Library for interfacing with Nokia 5110 LCD display (https://www.sparkfun.com/products/10168) using the hardware SPI on the mbed.
*   The display is powered from a GPIO pin meaning it can be controlled via software.  The LED backlight is also software-controllable (via PWM pin).
*   Can print characters and strings to the display using the included 5x7 font.
*   The library also implements a screen buffer so that individual pixels on the display (84 x 48) can be set, cleared and read.
*   The library can print primitive shapes (lines, circles, rectangles)
*   Acknowledgements to Chris Yan's Nokia_5110 Library.
*
*   Revision 1.3
*   Author: Craig A. Evans 7th February 2017
*   Editied by: Andrew Knowles 2021
*   Update by: Dr Tim Amsdon Feb 2022 -> Added SPI Mode selection required for different LCD variants
*/

#ifndef N5110_H
#define N5110_H

#include "mbed.h"

// number of pixels on display
#define WIDTH 84
#define HEIGHT 48
#define BANKS 6

/// Fill types for 2D shapes
typedef enum{
    FILL_TRANSPARENT, ///< Transparent with outline
    FILL_BLACK,       ///< Filled black
    FILL_WHITE,       ///< Filled white (no outline)
}FillType;

/// Nokia 5510 LCD types -> Added by Dr Tim Amsdon Feb 2022
enum LCD_Type {
    LPH7366_6, ///< Nokia 5510 part no. LPH7366-6 (check back of LCD module) uses SPI Mode 1
    LPH7366_1, ///< Nokia 5510 part no. LPH7366-1 (check back of LCD module) uses SPI Mode 0
};

class N5110{
private:
// objects
    SPI         *_spi;
    PwmOut      *_led;
    DigitalOut  *_pwr;
    DigitalOut  *_sce;
    DigitalOut  *_rst;
    DigitalOut  *_dc;

// variables
    unsigned char buffer[84][6];  // screen buffer - the 6 is for the banks - each one is 8 bits;

public:
    //Create a N5110 object connected to the specified pins
    N5110(PinName const pwrPin, //Pin connected to Vcc on the LCD display
          PinName const scePin, //Pin connected to chip enable
          PinName const rstPin, //Pin connected to reset
          PinName const dcPin,  //Pin connected to data/command select
          PinName const mosiPin,//Pin connected to data input (MOSI)
          PinName const sclkPin,//Pin connected to serial clock (SCLK)
          PinName const ledPin);//Pin connected to LED backlight (must be PWM)

    //Create a N5110 object connected to the specified pins (Vcc to +3V3)
    N5110(PinName const scePin, //Pin connected to chip enable
          PinName const rstPin, //Pin connected to reset
          PinName const dcPin,  //Pin connected to data/command select
          PinName const mosiPin,//Pin connected to data input (MOSI)
          PinName const sclkPin,//Pin connected to serial clock (SCLK)
          PinName const ledPin);//Pin connected to LED backlight (must be PWM)

    //Free allocated memory when object goes out of scope
    ~N5110();

    /* Initialise display
    *   Powers up the display and turns on backlight (50% brightness default).
    *   Sets the display up in horizontal addressing mode and with normal video mode.
    *   LCD type is passed to enable SPI mode modification -> functionality added byt Dr Tim Amsdon Feb 2022*/
    void init(LCD_Type const lcd);

    /* Turn off
    *   Powers down the display and turns of the backlight.
    *   Needs to be reinitialised before being re-used.*/
    void turnOff();

    /* Clear
    *   Clears the screen buffer.*/
    void clear();

    /* Set screen constrast
       @param constrast - float in range 0.0 to 1.0 (0.40 to 0.60 is usually a good value)*/
    void setContrast(float contrast);
    
    // Turn on normal video mode (default) Black on white
    void normalMode();

    // Turn on inverse video mode (default) White on black
    void inverseMode();

    /* Set Brightness
    *   Sets brightness of LED backlight.
    *   @param brightness - float in range 0.0 to 1.0*/
    void setBrightness(float const brightness);

    /* Print String
    *   Prints a string of characters to the screen buffer. String is cut-off after the 83rd pixel.
    *   @param x - the column number (0 to 83)
    *   @param y - the row number (0 to 5) - the display is split into 6 banks - each bank can be considered a row*/
    void printString(char const *str, unsigned int const  x, unsigned int const  y);

    /* Print Character
    *   Sends a character to the screen buffer.  Printed at the specified location. Character is cut-off after the 83rd pixel.
    *   @param  c - the character to print. Can print ASCII as so printChar('C').
    *   @param x - the column number (0 to 83)
    *   @param y - the row number (0 to 5) - the display is split into 6 banks - each bank can be considered a row*/
    void printChar(char const c, unsigned int const x, unsigned int const y);

    /* Set a Pixel
    * @param x     The x co-ordinate of the pixel (0 to 83)
    * @param y     The y co-ordinate of the pixel (0 to 47)
    * @param state The state of the pixel [true=black (default), false=white]
    * @details This function sets the state of a pixel in the screen buffer.
    *          The third parameter can be omitted*/
    void setPixel(unsigned int const x,
                  unsigned int const y,
                  bool const         state = true);

    /* Clear a Pixel
    *   @param  x - the x co-ordinate of the pixel (0 to 83)
    *   @param  y - the y co-ordinate of the pixel (0 to 47)
    *   @details This function clears pixel in the screen buffer
    *   @deprecated Use setPixel(x, y, false) instead*/
    void clearPixel(unsigned int const x,
                    unsigned int const y)
    __attribute__((deprecated("Use setPixel(x,y,false) instead")));

    /* Get a Pixel
    *   This function gets the status of a pixel in the screen buffer.
    *   @param  x - the x co-ordinate of the pixel (0 to 83)
    *   @param  y - the y co-ordinate of the pixel (0 to 47)
    *   @returns
    *       0           - pixel is clear
    *       1    - pixel is set*/
    int getPixel(unsigned int const x, unsigned int const y) const;

    /* Refresh display
    *   This functions sends the screen buffer to the display.*/
    void refresh();

    /* Randomise buffer
    *   This function fills the buffer with random data.  Can be used to test the display.
    *   A call to refresh() must be made to update the display to reflect the change in pixels.
    *   The seed is not set and so the generated pattern will probably be the same each time.
    *   TODO: Randomise the seed - maybe using the noise on the AnalogIn pins.*/
    void randomiseBuffer();

    /* Plot Array
    *   This function plots a one-dimensional array in the buffer.
    *   @param array[] - y values of the plot. Values should be normalised in the range 0.0 to 1.0. First 84 plotted.*/
    void plotArray(float const array[]);

    /* Draw Circle
    *   This function draws a circle at the specified origin with specified radius in the screen buffer
    *   Uses the midpoint circle algorithm.
    *   @see http://en.wikipedia.org/wiki/Midpoint_circle_algorithm
    *   @param  x0     - x-coordinate of centre
    *   @param  y0     - y-coordinate of centre
    *   @param  radius - radius of circle in pixels
    *   @param  fill   - fill-type for the shape*/
    void drawCircle(unsigned int const x0, unsigned int const y0, unsigned int const radius, FillType const fill);

    /* Draw Line
    *   This function draws a line between the specified points using linear interpolation.
    *   @param  x0 - x-coordinate of first point
    *   @param  y0 - y-coordinate of first point
    *   @param  x1 - x-coordinate of last point
    *   @param  y1 - y-coordinate of last point
    *   @param  type - 0 white,1 black,2 dotted*/
    void drawLine(unsigned int const x0, unsigned int const y0, unsigned int const x1, unsigned int const y1, unsigned int const type);

    /* Draw Rectangle
    *   This function draws a rectangle.
    *   @param  x0 - x-coordinate of origin (top-left)
    *   @param  y0 - y-coordinate of origin (top-left)
    *   @param  width - width of rectangle
    *   @param  height - height of rectangle
    *   @param  fill   - fill-type for the shape*/
    void drawRect(unsigned int const x0, unsigned int const y0, unsigned int const width, unsigned int const height, FillType const fill);

    /* Draw Sprite
    *   This function draws a sprite as defined in a 2D array
    *   @param  x0 - x-coordinate of origin (top-left)
    *   @param  y0 - y-coordinate of origin (top-left)
    *   @param  nrows - number of rows in sprite
    *   @param  ncols - number of columns in sprite
    *   @param  sprite - 2D array representing the sprite*/
    void drawSprite(int x0, int y0, int nrows, int ncols, int *sprite);

private:
// methods
    void setXYAddress(unsigned int const x,
                      unsigned int const y);
    void initSPI(LCD_Type const lcd); // LCD type is passed to enable SPI mode modification -> functionality added byt Dr Tim Amsdon Feb 2022
    void turnOn();
    void reset();
    void clearRAM();
    void sendCommand(unsigned char command);
    void sendData(unsigned char data);
    void setTempCoefficient(char tc);           // 0 to 3
    void setBias(char bias);                    // 0 to 7
};

const unsigned char font5x7[480] = {
    0x00, 0x00, 0x00, 0x00, 0x00,// (space)
    0x00, 0x00, 0x5F, 0x00, 0x00,// !
    0x00, 0x07, 0x00, 0x07, 0x00,// "
    0x14, 0x7F, 0x14, 0x7F, 0x14,// #
    0x24, 0x2A, 0x7F, 0x2A, 0x12,// $
    0x23, 0x13, 0x08, 0x64, 0x62,// %
    0x36, 0x49, 0x55, 0x22, 0x50,// &
    0x00, 0x05, 0x03, 0x00, 0x00,// '
    0x00, 0x1C, 0x22, 0x41, 0x00,// (
    0x00, 0x41, 0x22, 0x1C, 0x00,// )
    0x08, 0x2A, 0x1C, 0x2A, 0x08,// *
    0x08, 0x08, 0x3E, 0x08, 0x08,// +
    0x00, 0x50, 0x30, 0x00, 0x00,// ,
    0x08, 0x08, 0x08, 0x08, 0x08,// -
    0x00, 0x60, 0x60, 0x00, 0x00,// .
    0x20, 0x10, 0x08, 0x04, 0x02,// /
    0x3E, 0x51, 0x49, 0x45, 0x3E,// 0
    0x00, 0x42, 0x7F, 0x40, 0x00,// 1
    0x42, 0x61, 0x51, 0x49, 0x46,// 2
    0x21, 0x41, 0x45, 0x4B, 0x31,// 3
    0x18, 0x14, 0x12, 0x7F, 0x10,// 4
    0x27, 0x45, 0x45, 0x45, 0x39,// 5
    0x3C, 0x4A, 0x49, 0x49, 0x30,// 6
    0x01, 0x71, 0x09, 0x05, 0x03,// 7
    0x36, 0x49, 0x49, 0x49, 0x36,// 8
    0x06, 0x49, 0x49, 0x29, 0x1E,// 9
    0x00, 0x36, 0x36, 0x00, 0x00,// :
    0x00, 0x56, 0x36, 0x00, 0x00,// ;
    0x00, 0x08, 0x14, 0x22, 0x41,// <
    0x14, 0x14, 0x14, 0x14, 0x14,// =
    0x41, 0x22, 0x14, 0x08, 0x00,// >
    0x02, 0x01, 0x51, 0x09, 0x06,// ?
    0x32, 0x49, 0x79, 0x41, 0x3E,// @
    0x7E, 0x11, 0x11, 0x11, 0x7E,// A
    0x7F, 0x49, 0x49, 0x49, 0x36,// B
    0x3E, 0x41, 0x41, 0x41, 0x22,// C
    0x7F, 0x41, 0x41, 0x22, 0x1C,// D
    0x7F, 0x49, 0x49, 0x49, 0x41,// E
    0x7F, 0x09, 0x09, 0x01, 0x01,// F
    0x3E, 0x41, 0x41, 0x51, 0x32,// G
    0x7F, 0x08, 0x08, 0x08, 0x7F,// H
    0x00, 0x41, 0x7F, 0x41, 0x00,// I
    0x20, 0x40, 0x41, 0x3F, 0x01,// J
    0x7F, 0x08, 0x14, 0x22, 0x41,// K
    0x7F, 0x40, 0x40, 0x40, 0x40,// L
    0x7F, 0x02, 0x04, 0x02, 0x7F,// M
    0x7F, 0x04, 0x08, 0x10, 0x7F,// N
    0x3E, 0x41, 0x41, 0x41, 0x3E,// O
    0x7F, 0x09, 0x09, 0x09, 0x06,// P
    0x3E, 0x41, 0x51, 0x21, 0x5E,// Q
    0x7F, 0x09, 0x19, 0x29, 0x46,// R
    0x46, 0x49, 0x49, 0x49, 0x31,// S
    0x01, 0x01, 0x7F, 0x01, 0x01,// T
    0x3F, 0x40, 0x40, 0x40, 0x3F,// U
    0x1F, 0x20, 0x40, 0x20, 0x1F,// V
    0x7F, 0x20, 0x18, 0x20, 0x7F,// W
    0x63, 0x14, 0x08, 0x14, 0x63,// X
    0x03, 0x04, 0x78, 0x04, 0x03,// Y
    0x61, 0x51, 0x49, 0x45, 0x43,// Z
    0x00, 0x00, 0x7F, 0x41, 0x41,// [
    0x02, 0x04, 0x08, 0x10, 0x20,// "\"
    0x41, 0x41, 0x7F, 0x00, 0x00,// ]
    0x04, 0x02, 0x01, 0x02, 0x04,// ^
    0x40, 0x40, 0x40, 0x40, 0x40,// _
    0x00, 0x01, 0x02, 0x04, 0x00,// `
    0x20, 0x54, 0x54, 0x54, 0x78,// a
    0x7F, 0x48, 0x44, 0x44, 0x38,// b
    0x38, 0x44, 0x44, 0x44, 0x20,// c
    0x38, 0x44, 0x44, 0x48, 0x7F,// d
    0x38, 0x54, 0x54, 0x54, 0x18,// e
    0x08, 0x7E, 0x09, 0x01, 0x02,// f
    0x08, 0x14, 0x54, 0x54, 0x3C,// g
    0x7F, 0x08, 0x04, 0x04, 0x78,// h
    0x00, 0x44, 0x7D, 0x40, 0x00,// i
    0x20, 0x40, 0x44, 0x3D, 0x00,// j
    0x00, 0x7F, 0x10, 0x28, 0x44,// k
    0x00, 0x41, 0x7F, 0x40, 0x00,// l
    0x7C, 0x04, 0x18, 0x04, 0x78,// m
    0x7C, 0x08, 0x04, 0x04, 0x78,// n
    0x38, 0x44, 0x44, 0x44, 0x38,// o
    0x7C, 0x14, 0x14, 0x14, 0x08,// p
    0x08, 0x14, 0x14, 0x18, 0x7C,// q
    0x7C, 0x08, 0x04, 0x04, 0x08,// r
    0x48, 0x54, 0x54, 0x54, 0x20,// s
    0x04, 0x3F, 0x44, 0x40, 0x20,// t
    0x3C, 0x40, 0x40, 0x20, 0x7C,// u
    0x1C, 0x20, 0x40, 0x20, 0x1C,// v
    0x3C, 0x40, 0x30, 0x40, 0x3C,// w
    0x44, 0x28, 0x10, 0x28, 0x44,// x
    0x0C, 0x50, 0x50, 0x50, 0x3C,// y
    0x44, 0x64, 0x54, 0x4C, 0x44,// z
    0x00, 0x08, 0x36, 0x41, 0x00,// {
    0x00, 0x00, 0x7F, 0x00, 0x00,// |
    0x00, 0x41, 0x36, 0x08, 0x00,// }
    0x08, 0x08, 0x2A, 0x1C, 0x08,// ->
    0x08, 0x1C, 0x2A, 0x08, 0x08 // <-
};

#endif
