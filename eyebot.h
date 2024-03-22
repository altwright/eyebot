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

typedef struct {
  //0 - RGB565, 1 - GRAYSCALE
  uint8_t pix_format;
  //0 - No Effect, 1 - Negative, 2 - Grayscale
  uint8_t special_effect;
  //-2 to 2
  int8_t brightness;
  //-2 to 2
  int8_t contrast;  
  //-2 to 2     
  int8_t saturation;     
  //0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home
  uint8_t wb_mode;        
  // 0 = disable , 1 = enable
  uint8_t hmirror;  
  // 0 = disable , 1 = enable      
  uint8_t vflip;
} camera_settings;

// Read one camera image
bool CAMGetImage(byte imgbuf[]);                

bool CAMChangeSettings(camera_settings settings);

////////////////
//LCD Functions
////////////////

//Push RGB565 image to LCD buffer.
bool LCDPushImage(int xpos, int ypos, int img_width, int img_height, rgb img[]);
//Present LCD buffer to display
bool LCDRefresh();

#endif

