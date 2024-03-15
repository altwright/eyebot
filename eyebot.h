#ifndef EYEBOT_H
#define EYEBOT_H

#include <stdint.h>

////////////////
//Core Functions
////////////////

#define LCD_WIDTH 170
#define LCD_HEIGHT 320

//Must be called at setup()
bool EYEBOTInit(); 

///////////////////
//Camera Functions
///////////////////

#define QQVGA_WIDTH 160
#define QQVGA_HEIGHT 120
#define QQVGA QQVGA_WIDTH*QQVGA_HEIGHT
#define QQVGA_RGB565_BUFFER_SIZE QQVGA*2
#define QQVGA_G8_BUFFER_SIZE QQVGA

typedef uint8_t byte; 
//RGB565
typedef uint16_t rgb;

// Set to true for RGB565 images, false for G8 images.
// Can only be called once.
bool CAMInit(bool colour = true);
// Read one camera image
bool CAMGetImage(byte imgbuf[]);                

////////////////
//LCD Functions
////////////////

//Push RGB565 image to LCD buffer.
bool LCDPushImage(int xpos, int ypos, int img_width, int img_height, rgb img[]);
//Present LCD buffer to display
bool LCDRefresh();

#endif

