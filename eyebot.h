#ifndef EYEBOT_H
#define EYEBOT_H

#include <stdint.h>
#include <stdarg.h>

typedef uint32_t COLOR;
typedef uint8_t BYTE;

//Fonts
enum {
  HELVETICA, 
  TIMES, 
  COURIER
};

//Font variations
enum {
  NORMAL,
  BOLD
};

//LCD Modes
enum {
  LCD_BGCOL_TRANSPARENT, 
  LCD_BGCOL_NOTRANSPARENT, 
  LCD_BGCOL_INVERSE, 
  LCD_BGCOL_NOINVERSE, 
  LCD_FGCOL_INVERSE,
  LCD_FGCOL_NOINVERSE, 
  LCD_AUTOREFRESH, 
  LCD_NOAUTOREFRESH, 
  LCD_SCROLLING, 
  LCD_NOSCROLLING, 
  LCD_LINEFEED,
  LCD_NOLINEFEED, 
  LCD_SHOWMENU, 
  LCD_HIDEMENU, 
  LCD_LISTMENU, 
  LCD_CLASSICMENU, 
  LCD_FB_ROTATE, 
  LCD_FB_NOROTATION
};

const COLOR RED = 0xFF0000, 
            GREEN = 0x00FF00, 
            BLUE = 0x0000FF, 
            WHITE = 0xFFFFFF, 
            GRAY = 0x808080, 
            BLACK = 0x000000, 
            ORANGE = 0xFF7C00, 
            SILVER = 0xE0E0E0, 
            LIGHTGRAY = 0xBFBFBF, 
            DARKGRAY = 0x515151, 
            NAVY = 0x000E64, 
            CYAN = 0x00FFFF, 
            TEAL = 0x008080, 
            MAGENTA = 0xFF00FF, 
            PURPLE = 0x800080, 
            MAROON = 0x800000, 
            YELLOW = 0xFFFF00, 
            OLIVE = 0x808000;

//Must be called at least once
int EYEBOTInit(); 

// Print string and arguments on LCD
int LCDPrintf(const char *format, ...);

// Printf from given position
int LCDSetPrintf(int row, int column, const char *format, ...);

// Clear the LCD display and display buffers
int LCDClear();

// Set cursor position in pixels for subsequent printf
int LCDSetPos(int row, int column);

// Read current cursor position
int LCDGetPos(int *row, int *column);

// Set color for subsequent printf
int LCDSetColor(COLOR fg = WHITE, COLOR bg = BLACK);

// Set font for subsequent print operation
int LCDSetFont(int font, int variation);

// Set font-size for subsequent print operation
int LCDSetFontSize(int fontsize);

// Set LCD Mode (0=default)
// NOT IMPLEMENTED
int LCDSetMode(int mode);

// Set menu entries for soft buttons
// NOT IMPLEMENTED
int LCDMenu(char *st1, char *st2, char *st3, char *st4);

// Set menu for i-th entry with color [1..4]
// NOT IMPLEMENTED
int LCDMenuI(int pos, char *string, COLOR fg, COLOR bg);

// Get LCD resolution in pixels
int LCDGetSize(int *x, int *y); 

// Set one pixel on LCD
int LCDPixel(int x, int y, COLOR col);

// Read pixel value from LCD
COLOR LCDGetPixel(int x, int y);

// Draw line
int LCDLine(int x1, int y1, int x2, int y2, COLOR col);

// Draw filled/hollow rectangle
int LCDArea(int x1, int y1, int x2, int y2, COLOR col, int fill = 1);

// Draw filled/hollow circle
int LCDCircle(int x1, int y1, int radius, COLOR col, int fill = 1);

// Define image type for LCD
// NOT IMPLEMENTED
int LCDImageSize(int t);

// Define image start position and size
int LCDImageStart(int x = 0, int y = 0, int xs = 160, int ys = 120);

// Print color image at image start position and size
int LCDImage(BYTE *img);

// Print gray image [0...255] black...white
int LCDImageGray(BYTE *g);

// Print binary image [0...1] black/white
int LCDImageBinary(BYTE *b);

// Refresh LCD output
// NOT IMPLEMENTED
int LCDRefresh(void);

enum {
  NOKEY = 0,
  KEY1 = 1 << 0,
  KEY2 = 1 << 1,
  KEY3 = 1 << 2,
  KEY4 = 1 << 3,
  ANYKEY = ~0
};

// Blocking read (and wait) for key press (returns KEY1...KEY4 as a bitmask)
int KEYGet(void);            

// Non-blocking read of key press (returns NOKEY=0 if no key)
int KEYRead(void);           

// Wait until specified key has been pressed (use ANYKEY for any key)
int KEYWait(int key);        

// Blocking read for touch at any position, returns coordinates
int KEYGetXY (int *x, int *y);  

// Non-blocking read for touch at any position, returns coordinates
int KEYReadXY(int *x, int *y);  

typedef BYTE QQVGAcol [120][160][sizeof(COLOR)];    
typedef BYTE QVGAcol [240][320][sizeof(COLOR)];    
typedef BYTE VGAcol [480][640][sizeof(COLOR)];    
typedef BYTE CAM1MPcol [730][1296][sizeof(COLOR)];    
typedef BYTE CAMHDcol [1080][1920][sizeof(COLOR)];    
typedef BYTE CAM5MPcol [1944][2592][sizeof(COLOR)];    
typedef BYTE QQVGAgray [120][160];
typedef BYTE QVGAgray [240][320];
typedef BYTE VGAgray [480][640];
typedef BYTE CAM1MPgray [730][1296];
typedef BYTE CAMHDgray [1080][1920];
typedef BYTE CAM5MPgray [1944][2592];

extern int CAMWIDTH; 
extern int CAMHEIGHT;
extern int CAMPIXELS;
extern int CAMSIZE;

#define QQVGA_PIXELS (120*160)
#define QVGA_PIXELS (240*320)
#define VGA_PIXELS (480*640)
#define CAM1MP_PIXELS (730*1296)
#define CAMHD_PIXELS (1080*1920)
#define CAM5MP_PIXELS (1944*2592)
#define QQVGA_SIZE (QQVGA_PIXELS * sizeof(COLOR))
#define QVGA_SIZE (QVGA_SIZE * sizeof(COLOR))
#define VGA_SIZE (VGA_PIXELS * sizeof(COLOR))
#define CAM1MP_SIZE (CAM1MP_PIXELS * sizeof(COLOR))
#define CAMHD_SIZE (CAMHD_PIXELS * sizeof(COLOR))
#define CAM5MP_SIZE (CAM5MP_PIXELS * sizeof(COLOR))

// Change camera resolution (will also set IP resolution)
// NOT IMPLEMENTED
int CAMInit(int resolution);    

// Stops camera stream
// NOT IMPLEMENTED
int CAMRelease(void);           

// Read one color camera image
int CAMGet(BYTE *buf);          

// Read gray scale camera image
int CAMGetGray(BYTE *buf);      

// Set IP resolution using CAM constants (also automatically set by CAMInit)
// NOT IMPLEMENTED
int IPSetSize(int resolution);                                

// Read PNM file, fill/crop if req.; return 3:color, 2:gray, 1:b/w, -1:error
// NOT IMPLEMENTED
int IPReadFile(char *filename, BYTE* img);                    

// Write color PNM file
// NOT IMPLEMENTED
int IPWriteFile(char *filename, BYTE* img);                   

// Write gray scale PGM file
// NOT IMPLEMENTED
int IPWriteFileGray(char *filename, BYTE* gray);              

// Laplace edge detection on gray image
void IPLaplace(BYTE* grayIn, BYTE* grayOut);                   

// Pseudo-Sobel edge detection on gray image
void IPSobel(BYTE* grayIn, BYTE* grayOut);                     

// Transfer color to gray
void IPCol2Gray(BYTE* imgIn, BYTE* grayOut);                   

// Transfer gray to color              
void IPGray2Col(BYTE* imgIn, BYTE* colOut);                    

// Transform 3*gray to color
void IPRGB2Col (BYTE* r, BYTE* g, BYTE* b, BYTE* imgOut);      

// Transform RGB image to HSI
void IPCol2HSI (BYTE* img, BYTE* h, BYTE* s, BYTE* i);         

// Overlay c2 onto c1, all color images
void IPOverlay(BYTE* c1, BYTE* c2, BYTE* cOut);                

// Overlay gray image g2 onto g1, using col
void IPOverlayGray(BYTE* g1, BYTE* g2, COLOR col, BYTE* cOut); 

// PIXEL: RGB to color
COLOR IPPRGB2Col(BYTE r, BYTE g, BYTE b);                       

// PIXEL: color to RGB
void IPPCol2RGB(COLOR col, BYTE* r, BYTE* g, BYTE* b);         

// PIXEL: RGB to HSI for pixel
void IPPCol2HSI(COLOR c, BYTE* h, BYTE* s, BYTE* i);           

// PIXEL: Convert RGB to hue (0 for gray values)
BYTE IPPRGB2Hue(BYTE r, BYTE g, BYTE b);                       

// PIXEL: Convert RGB to hue, sat, int; hue=0 for gray values
void IPPRGB2HSI(BYTE r, BYTE g, BYTE b, BYTE* h, BYTE* s, BYTE* i); 

// Execute Linux program in background
// NOT IMPLEMENTED
char* OSExecute(char* command);

// RoBIOS Version
// NOT IMPLEMENTED
int OSVersion(char* buf);

// RoBIOS-IO Board Version
// NOT IMPLEMENTED
int OSVersionIO(char* buf);

// Speed in MHz
// NOT IMPLEMENTED
int OSMachineSpeed(void);

// Machine type
// NOT IMPLEMENTED
int OSMachineType(void);

// Machine name
// NOT IMPLEMENTED
int OSMachineName(char* buf);

// Machine ID derived from MAC address
// NOT IMPLEMENTED
int OSMachineID(void);

typedef int TIMER;

// Wait for n/1000 sec
// NOT IMPLEMENTED
int OSWait(int n);

// Add fct to 1000Hz/scale timer
// NOT IMPLEMENTED
TIMER OSAttachTimer(int scale, void (*fct)(void));

// Remove fct from 1000Hz/scale timer
// NOT IMPLEMENTED
int OSDetachTimer(TIMER t);

// Get system time (ticks in 1/1000 sec)
// NOT IMPLEMENTED
int OSGetTime(int *hrs,int *mins,int *secs,int *ticks); 

// Count in 1/1000 sec since system start
// NOT IMPLEMENTED
int OSGetCount(void);

// Init communication (see parameters below), interface number as in HDT file
// NOT IMPLEMENTED
int SERInit(int interface, int baud,int handshake); 

// Send single character
// NOT IMPLEMENTED
int SERSendChar(int interface, char ch);

// Send string (Null terminated)
// NOT IMPLEMENTED
int SERSend(int interface, char *buf);              

// Receive single character
// NOT IMPLEMENTED
char SERReceiveChar(int interface);

// Receive String (Null terminated), returns number of chars received
// NOT IMPLEMENTED
int SERReceive(int interface, char *buf, int size);

// Flush interface buffers
// NOT IMPLEMENTED
int SERFlush(int interface);

// Close Interface
// NOT IMPLEMENTED
int SERClose(int interface);

// Play beep sound
// NOT IMPLEMENTED
int AUBeep(void);

// Play audio sample in background (mp3 or wave)
// NOT IMPLEMENTED
int AUPlay(char* filename);

// Check if AUPlay has finished
// NOT IMPLEMENTED
int AUDone(void);

// Return microphone A-to-D sample value
// NOT IMPLEMENTED
int AUMicrophone(void);

enum {
  PSD_FRONT = 1, 
  PSD_LEFT = 2, 
  PSD_RIGHT = 3, 
  PSD_BACK = 4
};

// Read distance value in mm from PSD sensor [1..6]
int PSDGet(int psd);

// Read raw value from PSD sensor [1..6]
int PSDGetRaw(int psd);

// Measure distances in [mm]; default 360Â° and 360 points
// NOT IMPLEMENTED
int LIDARGet(int distance[]);

// range [1..360Â°], tilt angle down, number of points
// NOT IMPLEMENTED
int LIDARSet(int range, int tilt, int points);  

// Set servo [1..14] position to [1..255] or power down (0)
// NOT IMPLEMENTED
int SERVOSet(int servo, int angle);             

// Set servo [1..14] position bypassing HDT
// NOT IMPLEMENTED
int SERVOSetRaw (int servo, int angle);

// Set servo [1..14] limits in 1/100 sec
// NOT IMPLEMENTED
int SERVORange(int servo, int low, int high);

// Set motor [1..4] speed in percent [-100 ..+100]
// NOT IMPLEMENTED
int MOTORDrive(int motor, int speed);

// Set motor [1..4] speed bypassing HDT
// NOT IMPLEMENTED
int MOTORDriveRaw(int motor, int speed);

// Set motor [1..4] PID controller values [1..255]
// NOT IMPLEMENTED
int MOTORPID(int motor, int p, int i, int d);

// Stop PID control loop
// NOT IMPLEMENTED
int MOTORPIDOff(int motor);

// Set controlled motor speed in ticks/100 sec
// NOT IMPLEMENTED
int MOTORSpeed(int motor, int ticks);           

// Read quadrature encoder [1..4]
// NOT IMPLEMENTED
int ENCODERRead(int quad);

// Set encoder value to 0 [1..4]
// NOT IMPLEMENTED
int ENCODERReset(int quad);

// Set PWM offsets per motor [-255...255]
int VWSetOffsets(int left_offset, int right_offset);

// Set fixed linSpeed  [mm/s] and [degrees/s]
int VWSetSpeed(int linSpeed, int angSpeed);     

// Read current speeds [mm/s] and [degrees/s]
int VWGetSpeed(int *linSpeed, int *angSpeed);  

// Set robot position to x, y [mm], phi [degrees]
int VWSetPosition(int x, int y, int phi);       

// Get robot position as x, y [mm], phi [degrees]
int VWGetPosition(int *x, int *y, int *phi);    

// Drive straight, dist [mm], lin. speed [mm/s]
int VWStraight(int dist, int lin_speed);        

// Turn on spot, angle [degrees], ang. speed [degrees/s]
int VWTurn(int angle, int ang_speed);           

// Drive Curve, dist [mm], angle (orientation change) [degrees], lin. speed [mm/s]
int VWCurve(int dist, int angle, int lin_speed);

// Drive x[mm] straight and y[mm] left, x>|y|
int VWDrive(int dx, int dy, int lin_speed);     

// Return remaining drive distance in [mm]
int VWRemain(void);                             

// Non-blocking check whether drive is finished (1) or not (0)
int VWDone(void);                               

// Suspend current thread until drive operation has finished
int VWWait(void);                               

// Returns number of stalled motor [1..2], 3 if both stalled, 0 if none
// NOT IMPLEMENTED
int VWStalled(void);                            

// Set IO line [1..16] to i-n/o-ut/I-n pull-up/J-n pull-down
// NOT IMPLEMENTED
int DIGITALSetup(int io, char direction);       

// Read and return individual input line [1..16]
// NOT IMPLEMENTED
int DIGITALRead(int io);                        

// Read and return all 16 io lines
// NOT IMPLEMENTED
int DIGITALReadAll(void);                       

// Write individual output [1..16] to 0 or 1
// NOT IMPLEMENTED
int DIGITALWrite(int io, int state);            

// Read analog channel [1..8]
// NOT IMPLEMENTED
int ANALOGRead(int channel);                    

// Read analog supply voltage in [0.01 Volt]
// NOT IMPLEMENTED
int ANALOGVoltage(void);                        

// Record analog data (e.g. 8 for microphone) at 1kHz (non-blocking)
// NOT IMPLEMENTED
int ANALOGRecord(int channel, int iterations);  

// Transfer previously recorded data; returns number of bytes
// NOT IMPLEMENTED
int ANALOGTransfer(BYTE* buffer);               

// Blocking read of IRTV command
// NOT IMPLEMENTED
int IRTVGet(void);                              

// Non-blocking read, return 0 if nothing
// NOT IMPLEMENTED
int IRTVRead(void);                             

// Empty IRTV buffers
// NOT IMPLEMENTED
int IRTVFlush(void);                            

// Checks to see if IRTV is activated (1) or off (0)
// NOT IMPLEMENTED
int IRTVGetStatus(void);                        

// Start radio communication
// NOT IMPLEMENTED
int RADIOInit(void);                            

// Get own radio ID
// NOT IMPLEMENTED
int RADIOGetID(void);                           

// Send string (Null terminated) to ID destination
// NOT IMPLEMENTED
int RADIOSend(int id, char* buf);               

// Wait for message, then fill in sender ID and data, returns number of chars received
// NOT IMPLEMENTED
int RADIOReceive(int *id_no, char* buf, int size); 

// Check if message is waiting: 0 or 1 (non-blocking); -1 if error
// NOT IMPLEMENTED
int RADIOCheck(void);                           

// Returns number of robots (incl. self) and list of IDs in network
// NOT IMPLEMENTED
int RADIOStatus(int IDlist[]);                  

// Terminate radio communication
// NOT IMPLEMENTED
int RADIORelease(void);                         

#endif

