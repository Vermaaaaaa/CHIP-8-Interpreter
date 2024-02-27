#ifndef BITMAP_H
#define BITMAP_H

#include <vector>

// Forward declarations
class N5110;
class Bitmap{
private:
    std::vector<int> _contents;
    unsigned int _height;       // The height of the drawing in pixels
    unsigned int _width;        // The width of the drawing in pixels
    
public:
    Bitmap(int const *contents, unsigned int const height, unsigned int const width);
    int get_pixel(unsigned int const row, unsigned int const column) const;
    void print() const;
    void render(N5110 &lcd, unsigned int const x0, unsigned int const y0) const;
};

#endif