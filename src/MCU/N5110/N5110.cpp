#include "mbed.h"
#include "N5110.h"

// overloaded constructor includes power pin - LCD Vcc connected to GPIO pin
// this constructor works fine with LPC1768 - enough current sourced from GPIO
// to power LCD. Doesn't work well with K64F.
//LCD type is passed to the init and initspi functions to enable SPI mode modification -> functionality added byt Dr Tim Amsdon Feb 2022
N5110::N5110(PinName const pwrPin,
             PinName const scePin,
             PinName const rstPin,
             PinName const dcPin,
             PinName const mosiPin,
             PinName const sclkPin,
             PinName const ledPin)
    :
    _spi(new SPI(mosiPin,NC,sclkPin)), // create new SPI instance and initialise
    _led(new PwmOut(ledPin)),
    _pwr(new DigitalOut(pwrPin)),
    _sce(new DigitalOut(scePin)),
    _rst(new DigitalOut(rstPin)),
    _dc(new DigitalOut(dcPin))
{}

// overloaded constructor does not include power pin - LCD Vcc must be tied to +3V3
// Best to use this with K64F as the GPIO hasn't sufficient output current to reliably
// drive the LCD.
N5110::N5110(PinName const scePin,
             PinName const rstPin,
             PinName const dcPin,
             PinName const mosiPin,
             PinName const sclkPin,
             PinName const ledPin)
    :
    _spi(new SPI(mosiPin,NC,sclkPin)), // create new SPI instance and initialise
    _led(new PwmOut(ledPin)),
    _pwr(NULL), // pwr not needed so null it to be safe
    _sce(new DigitalOut(scePin)),
    _rst(new DigitalOut(rstPin)),
    _dc(new DigitalOut(dcPin))
{}

N5110::~N5110(){
    delete _spi;

    if(_pwr) {
        delete _pwr;
    }

    delete _led;
    delete _sce;
    delete _rst;
    delete _dc;
}

// initialise function - powers up and sends the initialisation commands
//LCD type is passed to enable SPI mode modification -> functionality added byt Dr Tim Amsdon Feb 2022
void N5110::init(LCD_Type const lcd){
    turnOn();               // power up
    reset();                // reset LCD - must be done within 100 ms
    initSPI(lcd);    
    setContrast(0.55);      // this may need tuning (say 0.4 to 0.6)
    setBias(3);             // datasheet - 48:1 mux - don't mess with if you don't know what you're doing! (0 to 7)
    setTempCoefficient(0);  // datasheet - may need increasing (range 0 to 3) at very low temperatures
    normalMode();           // normal video mode by default
    clearRAM();             // RAM is undefined at power-up so clear to be sure
    clear();                // clear buffer
    setBrightness(0.5);
}

// sets normal video mode (black on white)
void N5110::normalMode(){
    sendCommand(0b00100000);    // basic instruction
    sendCommand(0b00001100);    // normal video mode- datasheet
}

// sets normal video mode (white on black)
void N5110::inverseMode(){
    sendCommand(0b00100000);   // basic instruction
    sendCommand(0b00001101);   // inverse video mode - datasheet
}

// function to power up the LCD and backlight - only works when using GPIO to power
void N5110::turnOn(){
    if (_pwr != NULL) {
        _pwr->write(1);  // apply power
    }
}

// function to power down LCD
void N5110::turnOff()
{
    clear();                // clear buffer
    refresh();
    setBrightness(0.0);     // turn backlight off
    clearRAM();             // clear RAM to ensure specified current consumption
    
    // send command to ensure we are in basic mode
    sendCommand(0b00100000); // basic mode
    sendCommand(0b00001000); // clear display
    sendCommand(0b00100001); // extended mode
    sendCommand(0b00100100); // power down
    
    // if we are powering the LCD using the GPIO then make it low to turn off
    if (_pwr != NULL) {
        ThisThread::sleep_for(10ms);    // small delay and then turn off the power pin
        _pwr->write(0);                 // turn off power
    }

}

// function to change LED backlight brightness
void N5110::setBrightness(float brightness){

    // check whether brightness is within range
    if (brightness < 0.0f)
        brightness = 0.0f;
    if (brightness > 1.0f)
        brightness = 1.0f;
    _led->write(brightness);    // set PWM duty cycle
}

void N5110::setContrast(float contrast) {
    
    // enforce limits
    if (contrast > 1.0f)
        contrast = 1.0f;
    else if (contrast < 0.0f)
        contrast = 0.0;
    
    char ic = char(contrast*127.0f);    // convert to char in range 0 to 127 (i.e. 6 bits)
    
    sendCommand(0b00100001);            // extended instruction set
    sendCommand(0b10000000 | ic);       // set Vop (which controls contrast)
    sendCommand(0b00100000);            // back to basic instruction set
}

void N5110::setTempCoefficient(char tc) {
    
    // enforce limits
    if (tc>3) {
        tc=3;
    }
    
    // temperature coefficient may need increasing at low temperatures
    sendCommand(0b00100001);        // extended instruction set
    sendCommand(0b00000100 | tc);
    sendCommand(0b00100000);        // back to basic instruction set
}
    
void N5110::setBias(char bias) {
    
    // from data sheet
    // bias      mux rate
    // 0        1:100
    // 1        1:80
    // 2        1:65
    // 3        1:48   (default)
    // 4        1:40/1:34
    // 5        1:24
    // 6        1:18/1:16
    // 7        1:10/1:9/1:8
    
    // enforce limits
    if (bias>7) {
        bias=7;
    }
        
    sendCommand(0b00100001);            // extended mode instruction
    sendCommand(0b00010000 | bias);  
    sendCommand(0b00100000);            // end of extended mode instruction
}

// pulse the active low reset line
void N5110::reset(){
    _rst->write(0);  // reset the LCD
    _rst->write(1);
}

// function to initialise SPI peripheral
//LCD type is passed to enable SPI mode modification -> functionality added byt Dr Tim Amsdon Feb 2022
void N5110::initSPI(LCD_Type const lcd){
    if (lcd == LPH7366_1) {
    // works with Nokia 5510 with part number LPH7366-1
    _spi->format(8,0);          // 8 bits, Mode 0 - polarity 0, phase 1 - base value of clock is 0, data captured on falling edge/propagated on rising edge
    } else { 
    // works with Nokia 5510 with part number LPH7366-6
    _spi->format(8,1);          // 8 bits, Mode 1 - polarity 1, phase 1 - base value of clock is 0, data captured on falling edge/propagated on rising edge    
    }
    _spi->frequency(4000000);   // maximum of screen is 4 MHz
}

// send a command to the display
void N5110::sendCommand(unsigned char command){
    _dc->write(0);              // set DC low for command
    _sce->write(0);             // set CE low to begin frame
    _spi->write(command);       // send command
    _dc->write(1);              // turn back to data by default
    _sce->write(1);             // set CE high to end frame (expected for transmission of single byte)
}

// send data to the display at the current XY address
// dc is set to 1 (i.e. data) after sending a command and so should
// be the default mode.
void N5110::sendData(unsigned char data){
    _sce->write(0);   // set CE low to begin frame
    _spi->write(data);
    _sce->write(1);  // set CE high to end frame (expected for transmission of single byte)
}

// this function writes 0 to the 504 bytes to clear the RAM
void N5110::clearRAM(){
    _sce->write(0);                             //set CE low to begin frame
    for(int i = 0; i < WIDTH * HEIGHT; i++) {   // 48 x 84 bits = 504 bytes
        _spi->write(0x00);                      // send 0's
    }
    _sce->write(1);                             // set CE high to end frame
}

// function to set the XY address in RAM for subsequenct data write
void N5110::setXYAddress(unsigned int const x, unsigned int const y){
    if (x<WIDTH && y<HEIGHT) {          // check within range
        sendCommand(0b00100000);        // basic instruction
        sendCommand(0b10000000 | x);    // send addresses to display with relevant mask
        sendCommand(0b01000000 | y);
    }
}

// These functions are used to set, clear and get the value of pixels in the display
// Pixels are addressed in the range of 0 to 47 (y) and 0 to 83 (x).  The refresh()
// function must be called after set and clear in order to update the display
void N5110::setPixel(unsigned int const x, unsigned int const y, bool const state){
    if (x<WIDTH && y<HEIGHT) {  // check within range

        // calculate bank and shift 1 to required position in the data byte
        if(state) buffer[x][y/8] |= (1 << y%8);
        else      buffer[x][y/8] &= ~(1 << y%8);
    }
}

void N5110::clearPixel(unsigned int const x, unsigned int const y){
    if (x<WIDTH && y<HEIGHT) {  // check within range

        // calculate bank and shift 1 to required position (using bit clear)
        buffer[x][y/8] &= ~(1 << y%8);
    }
}

int N5110::getPixel(unsigned int const x, unsigned int const y) const{
    if (x<WIDTH && y<HEIGHT) {  // check within range
        
        // return relevant bank and mask required bit
        int pixel = (int) buffer[x][y/8] & (1 << y%8);

        if (pixel)
            return 1;
        else
            return 0;
    }
    return 0;
}

// function to refresh the display
void N5110::refresh(){
    setXYAddress(0,0);  // important to set address back to 0,0 before refreshing display
                        // address auto increments after printing string, so buffer[0][0] will not coincide
                        // with top-left pixel after priting string
    _sce->write(0);     //set CE low to begin frame

    // be careful to use correct order (j,i) for horizontal addressing
    for(int j = 0; j < BANKS; j++) {
        for(int i = 0; i < WIDTH; i++) {
            _spi->write(buffer[i][j]);      // send buffer
        }
    }
    _sce->write(1); // set CE high to end frame
}

// fills the buffer with random bytes.  Can be used to test the display.
// The rand() function isn't seeded so it probably creates the same pattern everytime
void N5110::randomiseBuffer(){
    int i,j;
    for(j = 0; j < BANKS; j++) {        // be careful to use correct order (j,i) for horizontal addressing
        for(i = 0; i < WIDTH; i++) {
            buffer[i][j] = rand()%256;  // generate random byte
        }
    }
}

// function to print 5x7 font
void N5110::printChar(char const c, unsigned int const  x, unsigned int const  y){
    
    // check if printing in range of y banks
    if (y<BANKS) {
        for (int i = 0; i < 5 ; i++ ) {
            int pixel_x = x+i;
            if (pixel_x > WIDTH-1)                          // ensure pixel isn't outside the buffer size (0 - 83)
                break;
            buffer[pixel_x][y] = font5x7[(c - 32)*5 + i];   // array is offset by 32 relative to ASCII, each character is 5 pixels wide
        }
    }
}

// function to print string at specified position
void N5110::printString(const char *str, unsigned int const x, unsigned int const y){
    
    // check if printing in range of y banks
    if (y<BANKS) {
        int n = 0 ; // counter for number of characters in string

        // loop through string and print character
        while(*str) {

            // writes the character bitmap data to the buffer, so that text and pixels can be displayed at the same time
            for (int i = 0; i < 5 ; i++ ) {
                int pixel_x = x+i+n*6;
                if (pixel_x > WIDTH-1)  // ensure pixel isn't outside the buffer size (0 - 83)
                    break;
                buffer[pixel_x][y] = font5x7[(*str - 32)*5 + i];
            }
            str++;  // go to next character in string
            n++;    // increment index
        }
    }
}

// function to clear the screen buffer
void N5110::clear(){
    memset(buffer,0,sizeof(buffer));
}

// function to plot array on display
void N5110::plotArray(float const array[]){
    for (int i=0; i<WIDTH; i++) {  // loop through array
        // elements are normalised from 0.0 to 1.0, so multiply
        // by 47 to convert to pixel range, and subtract from 47
        // since top-left is 0,0 in the display geometry
        setPixel(i,47 - int(array[i]*47.0f),true);
    }
}

// function to draw circle
void N5110:: drawCircle(unsigned int const x0, unsigned int const y0, unsigned int const radius, FillType const fill){
    
    // from http://en.wikipedia.org/wiki/Midpoint_circle_algorithm
    int x = radius;
    int y = 0;
    int radiusError = 1-x;

    while(x >= y) {

        // if transparent, just draw outline
        if (fill == FILL_TRANSPARENT) {
            setPixel( x + x0,  y + y0,true);
            setPixel(-x + x0,  y + y0,true);
            setPixel( y + x0,  x + y0,true);
            setPixel(-y + x0,  x + y0,true);
            setPixel(-y + x0, -x + y0,true);
            setPixel( y + x0, -x + y0,true);
            setPixel( x + x0, -y + y0,true);
            setPixel(-x + x0, -y + y0,true);
        } else {  

            // drawing filled circle, so draw lines between points at same y value
            int type = (fill==FILL_BLACK) ? 1:0;  // black or white fill
            drawLine(x+x0,y+y0,-x+x0,y+y0,type);
            drawLine(y+x0,x+y0,-y+x0,x+y0,type);
            drawLine(y+x0,-x+y0,-y+x0,-x+y0,type);
            drawLine(x+x0,-y+y0,-x+x0,-y+y0,type);
        }

        y++;
        if (radiusError<0) {
            radiusError += 2 * y + 1;
        } else {
            x--;
            radiusError += 2 * (y - x) + 1;
        }
    }
}

void N5110::drawLine(unsigned int const x0, unsigned int const y0, unsigned int const x1, unsigned int const y1, unsigned int const type){
    
    // Note that the ranges can be negative so we have to turn the input values into signed integers first
    int const y_range = static_cast<int>(y1) - static_cast<int>(y0);
    int const x_range = static_cast<int>(x1) - static_cast<int>(x0);

    // if dotted line, set step to 2, else step is 1
    unsigned int const step = (type==2) ? 2:1;

    // make sure we loop over the largest range to get the most pixels on the display
    // for instance, if drawing a vertical line (x_range = 0), we need to loop down the y pixels
    // or else we'll only end up with 1 pixel in the x column
    if ( abs(x_range) > abs(y_range) ) {

        // ensure we loop from smallest to largest or else for-loop won't run as expected
        unsigned int const start = x_range > 0 ? x0:x1;
        unsigned int const stop =  x_range > 0 ? x1:x0;

        // loop between x pixels
        for (unsigned int x = start; x<= stop ; x+=step) {  // do linear interpolation
            int const dx = static_cast<int>(x)-static_cast<int>(x0);
            unsigned int const y = y0 + y_range * dx / x_range;

            // If the line type is '0', this will clear the pixel
            // If it is '1' or '2', the pixel will be set
            setPixel(x,y, type);
        }

    } else {

        // ensure we loop from smallest to largest or else for-loop won't run as expected
        unsigned int const start = y_range > 0 ? y0:y1;
        unsigned int const stop =  y_range > 0 ? y1:y0;

        for (unsigned int y = start; y<= stop ; y+=step) {  // do linear interpolation
            int const dy = static_cast<int>(y)-static_cast<int>(y0);
            unsigned int const x = x0 + x_range * dy / y_range;

            // If the line type is '0', this will clear the pixel
            // If it is '1' or '2', the pixel will be set
            setPixel(x,y, type);
        }
    }
}

void N5110::drawRect(unsigned int const x0, unsigned int const y0, unsigned int const width, unsigned int const height, FillType const fill){
    
    // transparent, just outline
    if (fill == FILL_TRANSPARENT) {
        drawLine(x0,y0,x0+(width-1),y0,1);                          // top
        drawLine(x0,y0+(height-1),x0+(width-1),y0+(height-1),1);    // bottom
        drawLine(x0,y0,x0,y0+(height-1),1);                         // left
        drawLine(x0+(width-1),y0,x0+(width-1),y0+(height-1),1);     // right
    
    } else { // filled rectangle
        int type = (fill==FILL_BLACK) ? 1:0;                        // black or white fill
        for (int y = y0; y<y0+height; y++) {                        // loop through rows of rectangle
            drawLine(x0,y,x0+(width-1),y,type);                     // draw line across screen
        }
    }
}

void N5110::drawSprite(int x0, int y0, int nrows, int ncols, int *sprite){
    for (int i = 0; i < nrows; i++) {
        for (int j = 0 ; j < ncols ; j++) {
            int pixel = *((sprite+i*ncols)+j);
            setPixel(x0+j,y0+i, pixel);
        }
    }
}
