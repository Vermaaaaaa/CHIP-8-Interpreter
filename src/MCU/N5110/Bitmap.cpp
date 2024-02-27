#include <iostream>
#include "N5110.h"
#include "Bitmap.h"

Bitmap::Bitmap(int const *contents, unsigned int const height, unsigned int const width): _contents(std::vector<int>(height*width)), _height(height), _width(width){
    
    //Perform a quick sanity check of the dimensions
    if (_contents.size() != height * width) {
        std::cerr << "Contents of bitmap has size " << _contents.size()
                  << " pixels, but its dimensions were specified as "
                  << width << " * " << height << " = " << width * height << std::endl;
    }

    for(unsigned int i = 0; i < height*width; ++i) _contents[i] = contents[i];
}

//returns the value of the pixel at the given position
int Bitmap::get_pixel(unsigned int const row, unsigned int const column) const{
    
    // First check that row and column indices are within bounds
    if(column >= _width || row >= _height){
        std::cerr << "The requested pixel with index " << row << "," << column
                  << "is outside the bitmap dimensions: " << _width << ","
                  << _height << std::endl;
    }

    return _contents[row * _width + column];    // Now return the pixel value, using row-major indexing
}

//Prints the contents of the bitmap to the terminal
void Bitmap::print() const{
    for (unsigned int row = 0; row < _height; ++row){
        
        // Print each element of the row
        for (unsigned int column = 0; column < _width; ++column){
            int pixel = get_pixel(row, column);
            std::cout << pixel;
        }

        // And then terminate with a new-line character
        std::cout << std::endl;
    }
}

/* Renders the contents of the bitmap onto an N5110 screen
 *
 * param[in] lcd The screen to use for rendering
 * param[in] x0  The horizontal position in pixels at which to render the bitmap
 * param[in] y0  The vertical position in pixels at which to render the bitmap
 *
 * Note that x0, y0 gives the location of the top-left of the bitmap on the screen.
 * This function only updates the buffer on the screen. You still need to refresh the 
 * screen in order to actually see the bitmap.
 */
void Bitmap::render(N5110 &lcd, unsigned int const x0, unsigned int const y0) const{
    
    // Loop through each row of the bitmap image
    for (unsigned int bitmap_row = 0; bitmap_row < _height; ++bitmap_row){

        unsigned int screen_row = y0 + bitmap_row;          // Row index on the screen for rendering the row of pixels
                
        // Render each pixel in the row
        for (unsigned int bitmap_col = 0; bitmap_col < _width; ++bitmap_col){
            int screen_col = x0 + bitmap_col;               // Column index on the screen for rendering this pixel

            int pixel = get_pixel(bitmap_row, bitmap_col);  // Find the required value of the pixel at the given location within
            lcd.setPixel(screen_col, screen_row, pixel);    // the bitmap data and then write it to the LCD screen
        }
    }
}