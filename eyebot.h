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
typedef byte grayscale;
typedef uint32_t rgb;

typedef struct {
  byte hue;
  byte saturation;
  byte intensity;
} __attribute__((aligned(4))) hsi;

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

// imgbuf must be of length at least QQVGA_SIZE
bool CAMGetImage(rgb imgbuf[]);                

bool CAMChangeSettings(camera_settings settings);

/////////////////////////////
//Image Processing Functions
/////////////////////////////

bool IPGetRGB(rgb col, byte *r, byte *g, byte *b);

bool IPSetRGB(byte r, byte g, byte b, rgb *col);

bool IPRGBToGrayscale(rgb col, grayscale *gray);

bool IPRGBToGrayscale(rgb col, rgb *gray);

//Converts a RGB888 colour image to 8-bit grayscale
bool IPRGBToGrayscale(int img_width, int img_height, const rgb col_img[], grayscale gray_img[]);

bool IPRGBToGrayscale(int img_width, int img_height, const rgb col_img[], rgb gray_img[]);

bool IPGrayscaleToRGB(grayscale gray, rgb *col);

bool IPGrayscaleToRGB(int img_width, int img_height, const grayscale gray_img[], rgb col_img[]);

// Transform RGB pixel to HSI
bool IPRGBToHSI(rgb col, hsi *value);         

bool IPRGBToHSI(int img_width, int img_height, const rgb img[], hsi values[]);

// Laplace edge detection on gray image
bool IPLaplace(int img_width, int img_height, const grayscale *in, grayscale *out);

// Sobel edge detection on gray image
bool IPSobel(int img_width, int img_height, const grayscale *in, grayscale *out);

// intensity_threshold: 0-254
bool IPOverlay(int width, int height, int intensity_threshold, const rgb bg[], const rgb fg[], rgb out[]);

// intensity_threshold: 0-254
bool IPOverlay(int width, int height, int intensity_threshold, rgb overlay_color, const rgb bg[], const grayscale fg[], rgb out[]);

// intensity_threshold: 0-254
bool IPOverlay(int width, int height, int intensity_threshold, rgb overlay_color, const grayscale bg[], const grayscale fg[], rgb out[]);

// intensity_threshold: 0-254
bool IPOverlay(int width, int height, int intensity_threshold, const grayscale bg[], const rgb fg[], rgb out[]);

////////////////
//LCD Functions
////////////////

extern const rgb RED;
extern const rgb GREEN;
extern const rgb BLUE;
extern const rgb BLACK;
extern const rgb WHITE;
extern const rgb YELLOW;
extern const rgb MAGENTA;
extern const rgb CYAN;

//Push RGB888 image to LCD buffer.
bool LCDDrawImage(int xpos, int ypos, int img_width, int img_height, const rgb img[]);

//Push 8-bit grayscale image to LCD buffer.
bool LCDDrawImage(int xpos, int ypos, int img_width, int img_height, const grayscale img[]);

bool LCDRefresh();

bool LCDClear();

bool LCDSetCursor(int x, int y);

bool LCDGetCursor(int *x, int *y);

bool LCDSetFont(int font);

bool LCDSetFontColor(rgb fg, rgb bg = 0);

bool LCDSetFontSize(int size);

bool LCDPrint(const char *str);

bool LCDPrintln(const char *str);

bool LCDPrintAt(int x, int y, const char *str);

bool LCDGetSize(int *lcd_width, int *lcd_height);

bool LCDSetPixel(int x, int y, rgb hue);

bool LCDGetPixel(int x, int y, rgb *hue);

bool LCDDrawLine(int xs, int ys, int xe, int ye, rgb hue);

bool LCDDrawRect(int x, int y, int w, int h, rgb hue, bool fill = true);

bool LCDDrawCircle(int x, int y, int radius, rgb hue, bool fill = true);

#endif

