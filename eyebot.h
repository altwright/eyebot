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

typedef uint8_t byte; 
typedef byte grayscale;
typedef uint32_t rgb;

typedef struct {
  byte hue;
  byte saturation;
  byte intensity;
} __attribute__((aligned(4))) hsi;

typedef struct camera_settings {
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

///////////////////
// Input Functions
///////////////////

enum button {
  LEFT_BUTTON,
  RIGHT_BUTTON
};

typedef void (*button_callback) ();

//Non-blocking
bool INReadButton(button b, bool *pressed);

//Blocking
bool INWaitForButtonPress(button b);

//Blocking
bool INWaitForButtonRelease(button b);

/**
 * Triggers whenever the button's state changes, both rising and falling.
 * 
 * Excerpt taken from https://www.arduino.cc/reference/en/language/functions/external-interrupts/attachinterrupt/:
 * 
 * "Generally, an ISR should be as short and fast as possible. If your sketch uses multiple ISRs, 
 * only one can run at a time, other interrupts will be executed after the current one finishes 
 * in an order that depends on the priority they have. millis() relies on interrupts to count, so 
 * it will never increment inside an ISR. Since delay() requires interrupts to work, it will not 
 * work if called inside an ISR. micros() works initially but will start behaving erratically after 
 * 1-2 ms. delayMicroseconds() does not use any counter, so it will work as normal.
 * Typically global variables are used to pass data between an ISR and the main program. 
 * 
 * To make sure variables shared between an ISR and the main program are updated correctly, 
 * declare them as volatile."
*/
bool INSetButtonCallback(button b, button_callback cb);

bool INClearButtonCallback(button b);

//////////////////////////
//Motor-Driving Functions
//////////////////////////

//[-255, 255], applied to the PWM signal for each motor.
bool DRVSetMotorOffsets(int left_offset, int right_offset);

//[0, 340] mm/s
bool DRVSetMaxLinearSpeed(int max_lin_speed);

//[0, 180] degrees/s
bool DRVSetMaxAngularSpeed(int max_ang_speed);

// Set fixed linear speed  [mm/s] and angular speed [degrees/s].
bool DRVSetSpeed(int lin_speed, int ang_speed);     

// Set eyebot position to x, y [mm] and forward-facing angle (negative for counter-clockwise) [degrees]
bool DRVSetPosition(int x, int y, int angle);       

// Get estimate of eyebot position as x and y [mm], and forward-facing angle (negative for counter-clockwise) [degrees]
bool DRVGetPosition(int *x, int *y, int *angle);    

// Drive straight distance (negative for reverse) [mm] at speed > 0 [mm/s].
bool DRVStraight(int dist, int speed);        

// Turn on spot angle (negative for anti-clockwise) [degrees] at speed > 0 [degrees/s].
bool DRVTurn(int angle, int speed);           

// Drive curve for distance (negative for reverse) [mm] at linear speed > 0 [mm/s]. 
// Transition to final orienation angle (negative for anti-clockwise) [degrees].
bool DRVCurve(int dist, int angle, int lin_speed);

// Drive dx [mm] right (negative for left) and dy [mm] forward (negative for reverse), 
// speed > 0 [mm/s]. |y| >= |x|, y != 0.
bool DRVGoTo(int dx, int dy, int speed);     

// Return remaining drive distance of current drive op in [mm]. Returns false if there is no current drive op.
bool DRVRemaining(int *dist);                             

// Returns false if drive operation is not finished
bool DRVDone();                               

// Block until drive operation has finished
bool DRVWait();                               

/////////////////////////////////////////////
// Position Sensitive Device (PSD) Functions
/////////////////////////////////////////////

// Read distance value in mm (20-400) from distance sensor.
// Closer than 20 mm inverts distance reading, greater than 400 mm returns -1 in dist.
bool PSDGet(int *dist);

// Read raw value from distance sensor
bool PSDGetRaw(int *val);                         

#endif

