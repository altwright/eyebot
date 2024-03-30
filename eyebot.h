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

bool IP888To565(rgb888 in_hue, rgb565 *out_hue);

bool IP565To888(rgb565 in_hue, rgb888 *out_hue);

////////////////
//LCD Functions
////////////////

extern const rgb888 RED;
extern const rgb888 GREEN;
extern const rgb888 BLUE;
extern const rgb888 BLACK;
extern const rgb888 WHITE;
extern const rgb888 YELLOW;
extern const rgb888 MAGENTA;
extern const rgb888 CYAN;

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

bool LCDClear();

bool LCDSetCursor(int x, int y);

bool LCDGetCursor(int *x, int *y);

//1 -> 8
bool LCDSetFont(int font);

bool LCDSetFontColor(rgb888 fg, rgb888 bg = {0, 0, 0, 0xFF});

bool LCDSetFontSize(int size);

bool LCDPrint(const char *str);

bool LCDPrintln(const char *str);

bool LCDPrintAt(int x, int y, const char *str);

bool LCDGetSize(int *lcd_width, int *lcd_height);

bool LCDSetPixel(int x, int y, rgb888 hue);

bool LCDGetPixel(int x, int y, rgb888 *hue);

bool LCDDrawLine(int xs, int ys, int xe, int ye, rgb888 hue);

bool LCDDrawRect(int x, int y, int w, int h, rgb888 hue, bool fill = true);

bool LCDDrawCircle(int x, int y, int radius, rgb888 hue, bool fill = true);

#endif

