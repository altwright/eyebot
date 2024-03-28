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
#define QQVGA_SIZE QQVGA_WIDTH*QQVGA_HEIGHT
#define QQVGA_RGB565_BUFFER_SIZE QQVGA_SIZE*2

typedef uint8_t byte; 
typedef byte gray;
typedef uint16_t rgb565;
typedef struct {
  byte red;
  byte green;
  byte blue;
  byte alpha;
} rgba8888;
typedef rgba8888 rgb888;
typedef rgb888 rgb;

typedef struct {
  //-2 to 2
  int8_t brightness;
  //-2 to 2
  int8_t contrast;  
  //-2 to 2     
  int8_t saturation; 
  // 0 = disable , 1 = enable
  uint8_t hmirror;  
  // 0 = disable , 1 = enable      
  uint8_t vflip;
  // 0 = disable , 1 = enable
  uint8_t colorbar;       
  // IGNORED
  uint16_t _padding; 
} camera_settings;

bool CAMGetImage(rgb565 imgbuf[]);                

bool CAMGetImage(rgb888 imgbuf[]);                

bool CAMChangeSettings(camera_settings settings);

/////////////////////////////
//Image Processing Functions
/////////////////////////////

//Converts a RGB888 colour image to 8-bit grayscale
bool IPColorToGray(int img_width, int img_height, rgb888 col_img[], gray gray_img[]);

//Converts a RGB565 colour image to 8-bit grayscale
bool IPColorToGray(int img_width, int img_height, rgb565 col_img[], gray gray_img[]);

////////////////
//LCD Functions
////////////////

//Push RGB565 image to LCD buffer.
bool LCDPushColorImage(int xpos, int ypos, int img_width, int img_height, rgb565 img[]);

//Push RGB888 image to LCD buffer.
bool LCDPushColorImage(int xpos, int ypos, int img_width, int img_height, rgb888 img[]);

//Push 8-bit grayscale image to LCD buffer.
bool LCDPushGrayImage(int xpos, int ypos, int img_width, int img_height, gray img[]);

//Push binary image to LCD buffer.
bool LCDPushBinaryImage(int xpos, int ypos, int img_width, int img_height, bool img[]);

//Present LCD buffer to display
bool LCDRefresh();

bool LCDClear(int xpos = -1, int ypos = -1, int xs = -1, int ys = -1);

bool LCDSetCursor(int xpos, int ypos);

bool LCDGetCursor(int *xpos, int *ypos);

bool LCDSetFont(int font);

bool LCDSetFontSize(int size);

bool LCDPrintf(const char *format, ...);

bool LCDPrintfAt(int xpos, int ypos, const char *format, ...);

bool LCDGetSize(int *lcd_width, int *lcd_height);

bool LCDSetPixel(int xpos, int ypos, rgb888 hue);

bool LCDGetPixel(int xpos, int ypos, rgb888 *hue);

bool LCDDrawLine(int xstart, int ystart, int xfinish, int yfinish, rgb888 hue);

bool LCDDrawRect(int xstart, int ystart, int xfinish, int yfinish, rgb888 hue, bool fill = true);

bool LCDDrawCircle(int xpos, int ypos, int radius, rgb888 hue, bool fill = true);

#endif

