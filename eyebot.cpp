#define TOUCH_MODULES_CST_SELF
#include "eyebot.h"
#include <Arduino.h>
#include <math.h>
#include <driver/spi_master.h>
#include <driver/timer.h>
#include <TFT_eSPI.h>
#include <TouchLib.h>
#include <Wire.h>

//Camera Pins
#define PIN_SPI_MISO 43
#define PIN_SPI_CS 44
#define PIN_SPI_SCLK 1
#define PIN_CAM_SIGNAL 2

//Button Pins
#define PIN_LEFT_BUTTON 0
#define PIN_RIGHT_BUTTON 14

//Motor Pins
#define PIN_LEFT_MOTOR_FORWARD 3
#define PIN_LEFT_MOTOR_BACKWARD 10
#define PIN_RIGHT_MOTOR_FORWARD 12
#define PIN_RIGHT_MOTOR_BACKWARD 11

//PSD Pin
#define PIN_DIST_SENSOR 13

//Touch Pins
#define PIN_IIC_SCL                  17
#define PIN_IIC_SDA                  18
#define PIN_TOUCH_INT                16
#define PIN_TOUCH_RES                21

typedef uint32_t u32;
typedef uint64_t u64;
typedef uint16_t RGB565;
typedef void (*input_callback) ();

#define CAM_NUM_IMAGE_SEGMENTS 8
#define QQVGA_RGB565_BUFFER_SIZE QQVGA_WIDTH*QQVGA_HEIGHT*sizeof(RGB565)

#define MAX_RX_SEGMENT_SIZE 4800

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define INTENSITY_THRESHOLD 10

#define RAW_DISTANCE_PAIR_COUNT 25

#define LCD_WIDTH 170
#define LCD_HEIGHT 320

#define QQVGA_WIDTH 160
#define QQVGA_HEIGHT 120

struct RawDistancePair
{
  int raw;
  int distance;//mm
};

enum vw_op
{
  VW_OP_UNDEFINED,
  VW_OP_STRAIGHT,
  VW_OP_TURN,
  VW_OP_CURVE
};

int CAMWIDTH = QQVGA_WIDTH; 
int CAMHEIGHT = QQVGA_HEIGHT;
int CAMPIXELS = QQVGA_PIXELS;
int CAMSIZE = QQVGA_PIXELS * sizeof(COLOR);

static TFT_eSPI gTFT = TFT_eSPI(LCD_WIDTH, LCD_HEIGHT);
static spi_device_handle_t gCamSPIHandle;
TouchLib gTouch(Wire, PIN_IIC_SDA, PIN_IIC_SCL, CTS820_SLAVE_ADDRESS, PIN_TOUCH_RES);
static bool gTouchEnabled = true;

static RGB565 gLCDBuffer[LCD_HEIGHT*LCD_WIDTH];

static volatile vw_op gCurrentVWOp = VW_OP_UNDEFINED;

static timer_group_t gTimerGroup = TIMER_GROUP_0;
static timer_idx_t gMotorTimerIdx = TIMER_0;

static int gLeftMotorOffset;
static int gRightMotorOffset;
static int gLeftMotorPWM;
static int gRightMotorPWM ;

// mm/s
static int MAX_LIN_SPEED = 340;
// degrees/s
static int MAX_ANG_SPEED = 180;

static int gXPos, gYPos, gAngle;
static volatile int gLinSpeed, gAngSpeed;
static unsigned long gOpTotalTime, gOpStartTime;

static input_callback pLeftButtonCB = NULL;
static input_callback pRightButtonCB = NULL;
static input_callback pTouchCB = NULL;

static int gImgXStart, gImgYStart;

static RGB565 rgb888To565(COLOR col)
{
  return gTFT.color24to16(col);
}

static COLOR rgb565To888(RGB565 col)
{
  return gTFT.color16to24(col);
}

static bool motorKillTimerCB(void *arg)
{
  analogWrite(PIN_LEFT_MOTOR_FORWARD, 0);
  analogWrite(PIN_LEFT_MOTOR_BACKWARD, 0);
  analogWrite(PIN_RIGHT_MOTOR_FORWARD, 0);
  analogWrite(PIN_RIGHT_MOTOR_BACKWARD, 0);

  gCurrentVWOp = VW_OP_UNDEFINED;
  gLinSpeed = 0;
  gAngSpeed = 0;

  return true;
}

static void defaultLeftButtonCB()
{
  if (pLeftButtonCB)
    pLeftButtonCB();
}

static void defaultRightButtonCB()
{
  if (pRightButtonCB)
    pRightButtonCB();
}

static void defaultTouchCB()
{
  if (pTouchCB)
    pTouchCB();
}

static void rgb565SwapEndianess(RGB565 *hue)
{
  if (!hue)
    return;

  uint16_t lower = (*hue & 0xFF00) >> 8;
  *hue <<= 8;
  *hue |= lower;
}

static bool setMotorKillTimer(u64 time_ms)
{
  esp_err_t err;
  if (err = timer_pause(gTimerGroup, gMotorTimerIdx))
  {
    //Serial.printf("Failed to pause timer: %d\n", err);
    return false;
  }

  if (err = timer_set_counter_value(gTimerGroup, gMotorTimerIdx, 0))
  {
    //Serial.printf("Failed to set timer counter to zero: %d\n", err);
    return false;
  }

  //Assumes 2000 increments per s
  if (err = timer_set_alarm_value(gTimerGroup, gMotorTimerIdx, 2*(u64)time_ms))
  {
    //Serial.printf("Failed to set timer alarm value: %d\n", err);
    return false;
  }

  if (err = timer_set_alarm(gTimerGroup, gMotorTimerIdx, TIMER_ALARM_EN))
  {
    //Serial.printf("Failed to enable timer alarm: %d\n", err);
    return false;
  }

  if (err = timer_start(gTimerGroup, gMotorTimerIdx))
  {
    //Serial.printf("Failed to start timer: %d\n", err);
    return false;
  }

  return true;
}

int EYEBOTInit()
{
  /////////////
  //Motor init
  /////////////

  analogWrite(PIN_LEFT_MOTOR_FORWARD, 0);
  analogWrite(PIN_LEFT_MOTOR_BACKWARD, 0);
  analogWrite(PIN_RIGHT_MOTOR_FORWARD, 0);
  analogWrite(PIN_RIGHT_MOTOR_BACKWARD, 0);

  ////////////////////////////////////////////
  // Enable being powered from the power rail
  ////////////////////////////////////////////

  //pinMode(PIN_BATTERY_POWER, OUTPUT);
  //digitalWrite(PIN_BATTERY_POWER, HIGH);

  //////////////////////
  //Initialise T-Display
  //////////////////////
  gTFT.init();
  gTFT.fillScreen(TFT_BLACK);
  
  ////////////////////////////////////////////////////////////
  //SPI Initialisation for communication with the ESP32-Camera
  ////////////////////////////////////////////////////////////

  //Configuration for the SPI bus
  spi_bus_config_t buscfg = {
    .mosi_io_num = -1,
    .miso_io_num = PIN_SPI_MISO,
    .sclk_io_num = PIN_SPI_SCLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = QQVGA_RGB565_BUFFER_SIZE/CAM_NUM_IMAGE_SEGMENTS // < Max SPI transation size
  };

  //Configuration for the SPI device on the other side of the bus
  spi_device_interface_config_t devcfg = {
    .command_bits = 0,
    .address_bits = 0,
    .dummy_bits = 0,
    .mode = 0,
    .duty_cycle_pos = 0,    //50% duty cycle
    .cs_ena_posttrans = 3,  //Keep the CS low 3 cycles after transaction, to stop slave from missing the last bit when CS has less propagation delay than CLK
    .clock_speed_hz = SPI_MASTER_FREQ_10M,
    .spics_io_num = PIN_SPI_CS,
    .queue_size = 1
  };

  pinMode(PIN_SPI_SCLK, OUTPUT);
  pinMode(PIN_SPI_CS, OUTPUT);
  pinMode(PIN_CAM_SIGNAL, INPUT_PULLUP);

  //Initialize the SPI bus and add the ESP32-Camera as a device
  esp_err_t err = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
  assert(err == ESP_OK);
  err = spi_bus_add_device(SPI2_HOST, &devcfg, &gCamSPIHandle);
  assert(err == ESP_OK);
  
  ///////////////////////////////
  //Set default button callbacks
  ///////////////////////////////

  attachInterrupt(digitalPinToInterrupt(PIN_LEFT_BUTTON), defaultLeftButtonCB, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_RIGHT_BUTTON), defaultRightButtonCB, CHANGE);

  ///////////////////
  //Initialise timer
  ///////////////////

  timer_config_t timer_config = {};
  timer_config.alarm_en = TIMER_ALARM_DIS;
  timer_config.counter_en = TIMER_PAUSE;
  timer_config.intr_type = TIMER_INTR_LEVEL;
  timer_config.counter_dir = TIMER_COUNT_UP;
  timer_config.auto_reload = TIMER_AUTORELOAD_DIS;
  timer_config.divider = 40000;//Decrements 80 MHz/40000 = 2000 times a second

  err = timer_init(gTimerGroup, gMotorTimerIdx, &timer_config);
  assert(err == ESP_OK);

  err = timer_isr_callback_add(gTimerGroup, gMotorTimerIdx, motorKillTimerCB, NULL, 0);
  assert(err == ESP_OK);

  //////////////////////////
  //Initialise touch screen
  //////////////////////////

  gpio_hold_dis((gpio_num_t)PIN_TOUCH_RES);
  pinMode(PIN_TOUCH_RES, OUTPUT);
  digitalWrite(PIN_TOUCH_RES, LOW);
  delay(500);
  digitalWrite(PIN_TOUCH_RES, HIGH);
  Wire.begin(PIN_IIC_SDA, PIN_IIC_SCL);

  if (!gTouch.init())
    gTouchEnabled = false;

  attachInterrupt(digitalPinToInterrupt(PIN_TOUCH_INT), defaultTouchCB, CHANGE);

  return 0;
}

int LCDPrintf(const char* format, ...)
{
  if (!format)
    return -1;

  va_list arg_ptr;
  va_start(arg_ptr, format);
  char *str_buf = (char*)gLCDBuffer;

  vsnprintf(str_buf, sizeof(gLCDBuffer), format, arg_ptr);

  gTFT.print(str_buf);

  return 0;
}

int LCDSetPrintf(int row, int column, const char *format, ...)
{
  if (!format)
    return -1;
  
  gTFT.setCursor(column, row);

  va_list arg_ptr;
  va_start(arg_ptr, format);
  char *str_buf = (char*)gLCDBuffer;

  vsnprintf(str_buf, sizeof(gLCDBuffer), format, arg_ptr);

  gTFT.print(str_buf);

  return 0;
}

int LCDClear()
{
  gTFT.fillScreen(TFT_BLACK);

  return 0;
}

int LCDSetPos(int row, int column)
{
  gTFT.setCursor(column, row);

  return 0;
}

int LCDGetPos(int *row, int *column)
{
  if (!row || !column)
    return -1;

  *column = gTFT.getCursorX();
  *row = gTFT.getCursorY();

  return 0;
}

int LCDSetColor(COLOR fg, COLOR bg)
{
  RGB565 fg_hue = rgb888To565(fg);
  RGB565 bg_hue = rgb888To565(bg);
  gTFT.setTextColor(fg_hue, bg_hue);
 
  return 0;
}

int LCDSetFont(int font, int variation)
{
  gTFT.setTextFont(font);

  return 0;
}

int LCDSetFontSize(int fontsize)
{
  gTFT.setTextSize(fontsize);

  return 0;
}

int LCDSetMode(int mode)
{
  return -1;
}

int LCDMenu(char *st1, char *st2, char *st3, char *st4)
{
  return -1;
}

int LCDMenuI(int pos, char *string, COLOR fg, COLOR bg)
{
  return -1;
}

int LCDGetSize(int *x, int *y)
{
  if (!x || !y)
    return -1;

  *x = LCD_WIDTH;
  *y = LCD_HEIGHT;

  return 0;
}

int LCDPixel(int x, int y, COLOR col)
{
  RGB565 fg_hue = rgb888To565(col);
  gTFT.drawPixel(x, y, col);

  return 0;
}

COLOR LCDGetPixel(int x, int y)
{
  RGB565 col = gTFT.readPixel(x, y);
  return rgb565To888(col);
}

int LCDLine(int x1, int y1, int x2, int y2, COLOR col)
{
  RGB565 hue = rgb888To565(col);

  if (x1 - x2 == 0)
    if (y1 < y2)
      gTFT.drawFastVLine(x1, y1, y2-y1, hue);
    else
      gTFT.drawFastVLine(x1, y2, y1-y2, hue);
  else if (y1 - y2 == 0)
    if (x1 < x2)
      gTFT.drawFastHLine(x1, y1, x2-x1, hue);
    else
      gTFT.drawFastHLine(x2, y1, x1-x2, hue);
  else
    gTFT.drawLine(x1, y1, x2, y2, hue);
 
  return 0;
}

int LCDArea(int x1, int y1, int x2, int y2, COLOR col, int fill)
{
  RGB565 hue = rgb888To565(col);

  if (x1 > x2)
  {
    int tmp = x2;
    x2 = x1;
    x1 = tmp;
  }

  if (y1 > y2)
  {
    int tmp = y2;
    y2 = y1;
    y1 = tmp;
  }

  if (fill)
    gTFT.fillRect(x1, y1, x2 - x1, y2 - y1, hue);
  else
    gTFT.drawRect(x1, y1, x2 - x1, y2 - y1, hue);

  return 0;
}

int LCDCircle(int x1, int y1, int radius, COLOR col, int fill)
{
  RGB565 hue = rgb888To565(col);

  if (fill)
    gTFT.fillCircle(x1, y1, radius, hue);
  else 
    gTFT.drawCircle(x1, y1, radius, hue);

  return 0;
}

int LCDImageSize(int t)
{
  return -1;
}

int LCDImageStart(int x, int y, int xs, int ys)
{
  // x = x > 0 ? x : 0;
  // x = x < LCD_WIDTH ? x : LCD_WIDTH - 1;
  // y = y > 0 ? y : 0;
  // y = y < LCD_HEIGHT ? y : LCD_HEIGHT - 1;

  gImgXStart = x;
  gImgYStart = y;

  return 0;
}

int LCDImage(BYTE *img)
{
  if (!img)
    return -1;
  
  COLOR *col_img = (COLOR*)img;

  for (int y = 0; y < QQVGA_HEIGHT; y++)
  {
    for (int x = 0; x < QQVGA_WIDTH; x++)
    {
      int i = y*QQVGA_WIDTH + x;

      gLCDBuffer[i] = rgb888To565(col_img[i]);
      rgb565SwapEndianess(&gLCDBuffer[i]);
    }
  }

  gTFT.pushRect(gImgXStart, gImgYStart, QQVGA_WIDTH, QQVGA_HEIGHT, gLCDBuffer);

  return 0;
}

int LCDImageGray(BYTE *g)
{
  if (!g)
    return -1;

  for (int y = 0; y < QQVGA_HEIGHT; y++)
  {
    for (int x = 0; x < QQVGA_WIDTH; x++)
    {
      int i = y*QQVGA_WIDTH + x;

      COLOR col = IPPRGB2Col(g[i], g[i], g[i]);

      gLCDBuffer[i] = rgb888To565(col);
      rgb565SwapEndianess(&gLCDBuffer[i]);
    }
  }

  gTFT.pushRect(gImgXStart, gImgYStart, QQVGA_WIDTH, QQVGA_HEIGHT, gLCDBuffer);
 
  return 0;
}

int LCDImageBinary(BYTE *b)
{
  if (!b)
    return -1;

  for (int y = 0; y < QQVGA_HEIGHT; y++)
  {
    for (int x = 0; x < QQVGA_WIDTH; x++)
    {
      int i = y*QQVGA_WIDTH + x;
      gLCDBuffer[i] = b[i] ? 0xFFFF : 0;
    }
  }

  gTFT.pushRect(gImgXStart, gImgYStart, QQVGA_WIDTH, QQVGA_HEIGHT, gLCDBuffer);
 
  return 0;
}

int LCDRefresh(void)
{
  return -1;
}

int KEYGet(void)
{
  int key = NOKEY;

  while (!key)
    key = KEYRead();

  return key;
}

int KEYRead(void)
{
  int key = NOKEY;

  if (!digitalRead(PIN_LEFT_BUTTON))
    key |= KEY1;

  if (!digitalRead(PIN_RIGHT_BUTTON))
    key |= KEY2;

  return key;
}

int KEYWait(int key)
{
  if (!key)
    return 0;
  
  int pressed = NOKEY;
  while (!pressed)
  {
    pressed = KEYGet();
    pressed &= key;
  }

  return 0;
}        

int KEYGetXY (int *x, int *y)
{
  *x = -1;
  *y = -1;

  if (!gTouchEnabled)
    return -1;

  while (x < 0 || y < 0)
    KEYReadXY(x, y);
  
  return 0;
}  

int KEYReadXY(int *x, int *y)
{
  *x = -1;
  *y = -1;

  if (!gTouchEnabled)
    return -1;

  if (gTouch.read())
  {
    TP_Point t = gTouch.getPoint(0);
    *x = t.x;
    *y = t.y;
  }

  return 0;
}

int CAMInit(int resolution)
{
  return 0;
}

int CAMRelease(void)
{
  return 0; 
}

int CAMGet(BYTE *buf)
{  
  if (!buf)
    return -1;

  BYTE *rx_buf = (BYTE*)gLCDBuffer;

  for (int i = 0; i < CAM_NUM_IMAGE_SEGMENTS; i++)
  {
    spi_transaction_t t = {};
    t.length = MAX_RX_SEGMENT_SIZE*8;//bit length
    t.tx_buffer = NULL;
    t.rx_buffer = rx_buf + i*MAX_RX_SEGMENT_SIZE;

    while (digitalRead(PIN_CAM_SIGNAL));

    esp_err_t err = spi_device_transmit(gCamSPIHandle, &t);
    assert(err == ESP_OK);
  }

  RGB565 *pixels = (RGB565*)rx_buf;
  COLOR *img = (COLOR*)buf;

  for (int i = 0; i < QQVGA_WIDTH*QQVGA_HEIGHT; i++)
  {
    //rgb565SwapEndianess(&pixels[i]);
    img[i] = rgb565To888(pixels[i]);
  }

  return 0;
}

int CAMGetGray(BYTE *buf)
{
  if (!buf)
    return -1;

  BYTE *rx_buf = (BYTE*)gLCDBuffer;

  for (int i = 0; i < CAM_NUM_IMAGE_SEGMENTS; i++)
  {
    spi_transaction_t t = {};
    t.length = MAX_RX_SEGMENT_SIZE*8;//bit length
    t.tx_buffer = NULL;
    t.rx_buffer = rx_buf + i*MAX_RX_SEGMENT_SIZE;

    while (digitalRead(PIN_CAM_SIGNAL));

    esp_err_t err = spi_device_transmit(gCamSPIHandle, &t);
    assert(err == ESP_OK);
  }

  RGB565 *pixels = (RGB565*)rx_buf;

  for (int i = 0; i < QQVGA_WIDTH*QQVGA_HEIGHT; i++)
  {
    //rgb565SwapEndianess(&pixels[i]);
    COLOR col = rgb565To888(pixels[i]);

    BYTE r, g, b;
    IPPCol2RGB(col, &r, &g, &b);

    buf[i] = (r + g + b)/3;
  }

  return 0;
}

int IPSetSize(int resolution)
{
  return -1;
}

int IPReadFile(char *filename, BYTE* img)
{
  return -1;
}

int IPWriteFile(char *filename, BYTE* img)
{
  return -1;
}

int IPWriteFileGray(char *filename, BYTE* gray)
{
  return -1;
}

void IPLaplace(BYTE* grayIn, BYTE* grayOut)
{ 
  if (!grayIn || !grayOut)
    return;

  for (int y = 1; y < QQVGA_HEIGHT - 1; y++)
  {
    for (int x = 1; x < QQVGA_WIDTH - 1; x++)
    {
      int i = y*QQVGA_WIDTH + x;

      int delta = abs(4*grayIn[i] - grayIn[i-1] - grayIn[i+1] - grayIn[i-QQVGA_WIDTH] - grayIn[i+QQVGA_WIDTH]);

      if (delta > 0xFF) 
        delta = 0xFF;

      grayOut[i] = (BYTE)delta;
    }
  }
}

void IPSobel(BYTE* grayIn, BYTE* grayOut)
{
  if (!grayIn || !grayOut)
    return;

  memset(grayOut, 0, QQVGA_WIDTH); // clear first row

  for (int y = 1; y < QQVGA_HEIGHT-1; y++)
  {
    for (int x = 1; x < QQVGA_WIDTH-1; x++)
    {
      int i = y*QQVGA_WIDTH + x;

      int deltaX = 2*grayIn[i+1] + grayIn[i-QQVGA_WIDTH+1] + grayIn[i+QQVGA_WIDTH+1] - 2*grayIn[i-1] - grayIn[i-QQVGA_WIDTH-1] - grayIn[i+QQVGA_WIDTH-1];
      int deltaY = grayIn[i-QQVGA_WIDTH-1] + 2*grayIn[i-QQVGA_WIDTH] + grayIn[i-QQVGA_WIDTH+1] - grayIn[i+QQVGA_WIDTH-1] - 2*grayIn[i+QQVGA_WIDTH] - grayIn[i+QQVGA_WIDTH+1];

      int grad = (abs(deltaX) + abs(deltaY)) / 3;
      if (grad > 0xFF) 
        grad = 0xFF;

      grayOut[i] = (BYTE)grad;
    }
  }

  memset(grayOut + (QQVGA_HEIGHT-1)*(QQVGA_WIDTH), 0, QQVGA_WIDTH); 
}

void IPCol2Gray(BYTE* imgIn, BYTE* grayOut)
{
  if (!imgIn || !grayOut)
    return;
  
  COLOR *img = (COLOR*)imgIn;

  for (int y = 0; y < QQVGA_HEIGHT; y++)
  {
    for (int x = 0; x < QQVGA_WIDTH; x++)
    {
      int i = y * QQVGA_WIDTH + x;

      BYTE r, g, b;
      IPPCol2RGB(img[i], &r, &g, &b);

      grayOut[i] = (r + g + b)/3;
    }
  }
}

void IPGray2Col(BYTE* imgIn, BYTE* colOut)
{
  if (!imgIn || !colOut)
    return;
  
  COLOR *col = (COLOR*)colOut;

  for (int y = 0; y < QQVGA_HEIGHT; y++)
  {
    for (int x = 0; x < QQVGA_WIDTH; x++)
    {
      int i = y * QQVGA_WIDTH + x;
      col[i] = IPPRGB2Col(imgIn[i], imgIn[i], imgIn[i]);
    }
  }
}

void IPRGB2Col(BYTE* r, BYTE* g, BYTE* b, BYTE* imgOut)
{
  COLOR *img = (COLOR*)imgOut;

  for (int y = 0; y < QQVGA_HEIGHT; y++)
  {
    for (int x = 0; x < QQVGA_WIDTH; x++)
    {
      int i = y * QQVGA_WIDTH + x;

      img[i] = IPPRGB2Col(r[i], g[i], b[i]);
    }
  }
}

void IPCol2HSI(BYTE* img, BYTE* h, BYTE* s, BYTE* i)
{
  if (!h || !s || !i)
    return;

  COLOR *col = (COLOR*)img;

  for (int y = 0; y < QQVGA_HEIGHT; y++)
  {
    for (int x = 0; x < QQVGA_WIDTH; x++)
    {
      int idx = y * QQVGA_WIDTH + x;

      IPPCol2HSI(col[idx], &h[idx], &s[idx], &i[idx]);
    }
  }
}

void IPOverlay(BYTE* c1, BYTE* c2, BYTE* cOut)
{
  if (!c1 || !c2 || !cOut)
    return;

  COLOR *out = (COLOR*)cOut;
  COLOR *img1 = (COLOR*)c1;
  COLOR *img2 = (COLOR*)c2;

  for (int y = 0; y < QQVGA_HEIGHT; y++)
  {
    for (int x = 0; x < QQVGA_WIDTH; x++)
    {
      int i = y*QQVGA_WIDTH + x;
      out[i] = img2[i] ? img2[i] : img1[i];
    }
  }
}

void IPOverlayGray(BYTE* g1, BYTE* g2, COLOR col, BYTE* cOut)
{
  if (!g1 || !g2 || !cOut)
    return;
  
  COLOR *out = (COLOR*)cOut;

  for (int y = 0; y < QQVGA_HEIGHT; y++)
  {
    for (int x = 0; x < QQVGA_WIDTH; x++)
    {
      int i = y*QQVGA_WIDTH + x;
      out[i] = g2[i] ? col : IPPRGB2Col(g1[i], g1[i], g1[i]);
    }
  }
}

COLOR IPPRGB2Col(BYTE r, BYTE g, BYTE b)
{
  RGB565 col = gTFT.color565(r, g, b);
  return rgb565To888(col);
}

void IPPCol2RGB(COLOR col, BYTE* r, BYTE* g, BYTE* b)
{
  if (!r || !g || !b)
    return;
  
  *r = (col >> 16) & 0xFF;
  *g = (col >> 8) & 0xFF;
  *b = (col) & 0xFF;
}

void IPPCol2HSI(COLOR col, BYTE *h, BYTE *s, BYTE *i)
{
  if (!h || !s || !s)
    return;

  BYTE r, g, b;
  IPPCol2RGB(col, &r, &g, &b);
  IPPRGB2HSI(r, g, b, h, s, i);
}

BYTE IPPRGB2Hue(BYTE r, BYTE g, BYTE b)
{
  BYTE max   = MAX(r, MAX(g, b));
  BYTE min   = MIN(r, MIN(g, b));
  BYTE delta = max - min;

  int i = (r + g + b)/3;

  BYTE h = 0;

  if ((2*delta > max) && (i > INTENSITY_THRESHOLD))
  { 
    if (r == max) 
      h = 43 + 42*(g-b)/delta; // +/-42 [  1.. 85]
    else if (g == max) 
      h = 128 + 42*(b-r)/delta; // +/-42 [ 86..170]
    else if (b == max) 
      h = 213 + 42*(r-g)/delta; // +/-42 [171..255]
  }
  else
    h = 0;

  return h;// grayscale, not color
}

void IPPRGB2HSI(BYTE r, BYTE g, BYTE b, BYTE* h, BYTE* s, BYTE* i)
{
  if (!h || !s || !s)
    return;

  BYTE max   = MAX(r, MAX(g, b));
  BYTE min   = MIN(r, MIN(g, b));
  BYTE delta = max - min;

  *i = (r + g + b)/3;

  if (*i > 0) 
    *s = 255 - (255 * min)/(*i);
  else 
    *s = 0;

  if ((2*delta > max) && (*i > INTENSITY_THRESHOLD))
  { 
    if (r == max) 
      *h =  43 + 42*(g-b)/delta; // +/-42 [  1.. 85]
    else if (g == max) 
      *h = 128 + 42*(b-r)/delta; // +/-42 [ 86..170]
    else if (b == max) 
      *h = 213 + 42*(r-g)/delta; // +/-42 [171..255]
  }
  else 
    *h = 0; // grayscale, not color
}

char* OSExecute(char* command)
{
  return NULL;
}

int OSVersion(char* buf)
{
  return -1;
}

int OSVersionIO(char* buf)
{
  return -1;
}

int OSMachineSpeed(void)
{
  return -1;
}

int OSMachineType(void)
{
  return -1;
}

int OSMachineName(char* buf)
{
  return -1;
}

int OSMachineID(void)
{
  return -1;
}

int OSWait(int n)
{
  return -1;
}

TIMER OSAttachTimer(int scale, void (*fct)(void))
{
  return -1;
}

int OSDetachTimer(TIMER t)
{
  return -1;
}

int OSGetTime(int *hrs,int *mins,int *secs,int *ticks)
{
  return -1;
}

int OSGetCount(void)
{
  return -1;
}

int SERInit(int interface, int baud,int handshake)
{
  return -1;
}

int SERSendChar(int interface, char ch)
{
  return -1;
}

int SERSend(int interface, char *buf)
{
  return -1;
}

char SERReceiveChar(int interface)
{
  return -1;
}

int SERReceive(int interface, char *buf, int size)
{
  return -1;
}

int SERFlush(int interface)
{
  return -1;
}

int SERClose(int interface)
{
  return -1;
}

int AUBeep(void)
{
  return -1;
}

int AUPlay(char* filename)
{
  return -1;
}

int AUDone(void)
{
  return -1;
}

int AUMicrophone(void)
{
  return -1;
}

int PSDGet(int psd)
{
  if (psd != PSD_FRONT)
    return -1;
  
  static RawDistancePair pairs[RAW_DISTANCE_PAIR_COUNT] = {
    {4000, 60},
    {3700, 70},
    {3240, 80},
    {2980, 90},
    {2740, 100},
    {2520, 110},
    {2340, 120},
    {2150, 130},
    {2030, 140},
    {1880, 150},
    {1780, 160},
    {1640, 170},
    {1570, 180},
    {1480, 190},
    {1430, 200},
    {1330, 210},
    {1260, 220},
    {1190, 230},
    {1175, 240},
    {1100, 250},
    {1050, 260},
    {1000, 270},
    {970, 280},
    {930, 290},
    {870, 300}
  };

  int val = PSDGetRaw(psd);

  for (int i = 0; i < RAW_DISTANCE_PAIR_COUNT; i++)
  {
    if (val >= pairs[i].raw)
      return pairs[i].distance;
  }

  return 300;//max distance
}

int PSDGetRaw(int psd)
{
  if (psd != PSD_FRONT)
    return -1;

  return analogRead(PIN_DIST_SENSOR);
}

int LIDARGet(int distance[])
{
  return -1;
}

int LIDARSet(int range, int tilt, int points)
{
  return -1;
}

int SERVOSet(int servo, int angle)
{
  return -1;
}

int SERVOSetRaw (int servo, int angle)
{
  return -1;
}

int SERVORange(int servo, int low, int high)
{
  return -1;
}

int MOTORDrive(int motor, int speed)
{
  return -1;
}

int MOTORDriveRaw(int motor, int speed)
{
  return -1;
}

int MOTORPID(int motor, int p, int i, int d)
{
  return -1;
}

int MOTORPIDOff(int motor)
{
  return -1;
}

int MOTORSpeed(int motor, int ticks)
{
  return -1;
}

int ENCODERRead(int quad)
{
  return -1;
}

int ENCODERReset(int quad)
{
  return -1;
}

int VWSetOffsets(int left_offset, int right_offset)
{
  gLeftMotorOffset = left_offset < -255 || left_offset > 255 ? 0 : left_offset;
  gRightMotorOffset = right_offset < -255 || right_offset > 255 ? 0 : right_offset;

  return 0;
}

int VWSetSpeed(int lin_speed, int ang_speed)
{
  if (gCurrentVWOp != VW_OP_UNDEFINED)
  {
    //There is still a background timer for
    //an ongoing drive op that must be stopped
    timer_pause(gTimerGroup, gMotorTimerIdx);
  }

  analogWrite(PIN_LEFT_MOTOR_FORWARD, 0);
  analogWrite(PIN_LEFT_MOTOR_BACKWARD, 0);
  analogWrite(PIN_RIGHT_MOTOR_FORWARD, 0);
  analogWrite(PIN_RIGHT_MOTOR_BACKWARD, 0);

  // Set current eyebot position
  int x, y, angle;
  VWGetPosition(&x, &y, &angle);
  VWSetPosition(x, y, angle);

  if (lin_speed < -1 * MAX_LIN_SPEED)
    lin_speed = -1 * MAX_LIN_SPEED;
  
  if (lin_speed > MAX_LIN_SPEED)
    lin_speed = MAX_LIN_SPEED;
  
  if (ang_speed < -1 * MAX_ANG_SPEED)
    ang_speed = -1 * MAX_ANG_SPEED;
  
  if (ang_speed > MAX_ANG_SPEED)
    ang_speed = MAX_ANG_SPEED;

  gLinSpeed = lin_speed;
  gAngSpeed = ang_speed;

  bool reverse = lin_speed >= 0 ? false : true;
  if (reverse)
    lin_speed *= -1;

  bool clockwise = ang_speed >= 0 ? true : false;
  if (!clockwise)
    ang_speed *= -1;
  
  float ang_speed_percentage = ang_speed / (float)MAX_ANG_SPEED;

  if (lin_speed != 0)
  {
    float lin_speed_percentage = lin_speed / (float)MAX_LIN_SPEED;
    int motor_pwm = 255 * lin_speed_percentage;

    gLeftMotorPWM = motor_pwm + gLeftMotorOffset;
    gRightMotorPWM = motor_pwm + gRightMotorOffset;

    if (gLeftMotorPWM < 0)
      gLeftMotorPWM = 0;
    
    if (gLeftMotorPWM > 255)
      gLeftMotorPWM = 255;
    
    if (gRightMotorPWM < 0)
      gRightMotorPWM = 0;

    if (gRightMotorPWM > 255)
      gRightMotorPWM = 255;

    int ang_pwm_offset = 255 * ang_speed_percentage;

    if (clockwise)
    {
      gRightMotorPWM -= ang_pwm_offset;
      if (gRightMotorPWM < 0)
        gRightMotorPWM = 0;
    }
    else
    {
      gLeftMotorPWM -= ang_pwm_offset;
      if (gLeftMotorPWM < 0)
        gLeftMotorPWM = 0;
    }

    if (reverse)
    {
      analogWrite(PIN_LEFT_MOTOR_FORWARD, 0);
      analogWrite(PIN_LEFT_MOTOR_BACKWARD, gLeftMotorPWM);
      analogWrite(PIN_RIGHT_MOTOR_FORWARD, 0);
      analogWrite(PIN_RIGHT_MOTOR_BACKWARD, gRightMotorPWM);
    }
    else
    {
      analogWrite(PIN_LEFT_MOTOR_FORWARD, gLeftMotorPWM);
      analogWrite(PIN_LEFT_MOTOR_BACKWARD, 0);
      analogWrite(PIN_RIGHT_MOTOR_FORWARD, gRightMotorPWM);
      analogWrite(PIN_RIGHT_MOTOR_BACKWARD, 0);
    }
  }
  else
  {
    // When turning on spot, angular speed is inherently doubled, since
    // it only assumes that it is pivoting around a stationary wheel.
    //ang_speed_percentage /= 2;

    if (clockwise)
    {
      analogWrite(PIN_LEFT_MOTOR_FORWARD, 255 * ang_speed_percentage);
      analogWrite(PIN_LEFT_MOTOR_BACKWARD, 0);
      analogWrite(PIN_RIGHT_MOTOR_FORWARD, 0);
      analogWrite(PIN_RIGHT_MOTOR_BACKWARD, 255 * ang_speed_percentage);
    }
    else
    {
      analogWrite(PIN_LEFT_MOTOR_FORWARD, 0);
      analogWrite(PIN_LEFT_MOTOR_BACKWARD, 255 * ang_speed_percentage);
      analogWrite(PIN_RIGHT_MOTOR_FORWARD, 255 * ang_speed_percentage);
      analogWrite(PIN_RIGHT_MOTOR_BACKWARD, 0);
    }
  }

  gOpStartTime = millis();

  gOpTotalTime = 0;//indefinite by default

  return 0;
}     

int VWGetSpeed(int *linSpeed, int *angSpeed)
{
  if (!linSpeed || !angSpeed)
    return -1;

  *linSpeed = gLinSpeed;
  *angSpeed = gAngSpeed;

  return 0;
}

int VWSetPosition(int x, int y, int phi)
{
  gXPos = x;
  gYPos = y;
  gAngle = phi;

  return 0;
}

int VWGetPosition(int *x, int *y, int *phi)
{  
  if (!x || !y || !phi)
    return -1;
  
  unsigned long delta = millis() - gOpStartTime;

  switch (gCurrentVWOp)
  {
    case VW_OP_UNDEFINED:
    case VW_OP_STRAIGHT:
    case VW_OP_CURVE:
    {
      float delta_arc = gLinSpeed * (delta / 1000.0f);
      float eyebot_angle_rad = gAngle * M_PI / 180.0f;
      float delta_degrees = gAngSpeed * (delta / 1000.0f);

      int dx = 0, dy = 0;
      if (gAngSpeed != 0 && delta != 0)
      {
        float delta_rad = delta_degrees * M_PI / 180.0f;
        float radius = delta_arc / delta_rad;

        dy = radius * sinf(eyebot_angle_rad + delta_rad);
        dx = radius - (radius * cosf(eyebot_angle_rad + delta_rad));
      }
      else
      {
        dy = delta_arc * cosf(eyebot_angle_rad);
        dx = delta_arc * sinf(eyebot_angle_rad);
      }

      *x = gXPos + dx;
      *y = gYPos + dy;
      *phi = gAngle + delta_degrees;

      break;
    }
    case VW_OP_TURN:
    {
      float delta_degrees = gAngSpeed * (delta / 1000.0f);

      *x = gXPos;
      *y = gYPos;
      *phi = gAngle + delta_degrees;

      break;
    }
    default:
      break;
  }

  return 0;
}

int VWStraight(int dist, int lin_speed)
{  
  if (lin_speed <= 0)
    return -1;
  
  if (lin_speed > MAX_LIN_SPEED)
    lin_speed = MAX_LIN_SPEED;

  bool reverse = dist < 0 ? true : false;
  
  if (reverse)
  {
    VWSetSpeed(-1*lin_speed, 0);
    dist *= -1;
  }
  else
    VWSetSpeed(lin_speed, 0);

  //ms
  float time_taken = (dist / (float)lin_speed) * 1000;

  if (!setMotorKillTimer((u64)time_taken))
  {
    //Serial.printf("Failed to start kill motor timer\n");
    return false;
  }
  
  gOpTotalTime = time_taken;

  gCurrentVWOp = VW_OP_STRAIGHT;

  return 0;
}

int VWTurn(int angle, int ang_speed)
{
  if (ang_speed)
    return -1;
  
  if (ang_speed > MAX_ANG_SPEED)
    ang_speed = MAX_ANG_SPEED;
  
  bool clockwise = angle < 0 ? false : true;

  if (clockwise)
    VWSetSpeed(0, ang_speed);
  else
  {
    VWSetSpeed(0, -1*ang_speed);
    angle *= -1;
  }
  
  float time_taken = (angle / (float)ang_speed) * 1000 / 2;

  if (!setMotorKillTimer((u64)time_taken))
    return -1;

  gOpTotalTime = time_taken;

  gCurrentVWOp = VW_OP_TURN;

  return 0;
}

int VWCurve(int dist, int angle, int lin_speed)
{
  if (lin_speed < 0)
    return -1;
  
  if (lin_speed > MAX_LIN_SPEED)
    lin_speed = MAX_LIN_SPEED;
  
  bool reverse = dist < 0 ? true : false;
  bool clockwise = angle >= 0 ? true : false;

  if (reverse)
    dist *= -1;

  float time_taken = dist / (float)lin_speed;
  int ang_speed = angle / time_taken;

  if (clockwise)
  {
    if (ang_speed > MAX_ANG_SPEED)
      ang_speed = MAX_ANG_SPEED;
  }
  else
  {
    if (ang_speed < -1*MAX_ANG_SPEED)
      ang_speed = -1*MAX_ANG_SPEED;
  }

  if (reverse)
    VWSetSpeed(-1*lin_speed, ang_speed);
  else
    VWSetSpeed(lin_speed, ang_speed);
  
  time_taken *= 1000;

  if (!setMotorKillTimer((u64)time_taken))
    return -1;

  gOpTotalTime = time_taken;

  gCurrentVWOp = VW_OP_CURVE;

  return 0;
}

int VWDrive(int dx, int dy, int lin_speed)
{  
  if (lin_speed <= 0 || dy == 0)
    return -1;

  if (dx != 0)
  {
    bool reverse = dy < 0 ? true : false;
    bool right = dx >= 0 ? true : false;

    int abs_dy = abs(dy);
    int abs_dx = abs(dx);

    if (abs_dy < abs_dx)
    {
      if (right)
        dx = abs_dy;
      else
        dx = -1*abs_dy;
    }

    float ratio = abs_dx / (float)abs_dy;
    float rads = asinf(ratio);

    float radius = abs_dy / sinf(rads);

    int arc_l = radius * rads;
    int degrees = (rads * 180/M_PI);

    if (reverse)
      arc_l *= -1;

    if (right)
      VWCurve(arc_l, degrees, lin_speed);
    else 
      VWCurve(arc_l, -1*degrees, lin_speed);
  }
  else
    VWStraight(dy, lin_speed);

  return 0;
}

int VWRemain(void)
{
  int dist = -1;

  switch (gCurrentVWOp)
  {
    case VW_OP_STRAIGHT:
    case VW_OP_CURVE:
    {
      unsigned long op_time_delta = millis() - gOpStartTime;
      int total_dist = gOpTotalTime * abs(gLinSpeed) / 1000;
      dist = total_dist - (op_time_delta * abs(gLinSpeed) / 1000);
      break;
    }
    case VW_OP_TURN:
    {
      dist = 0;
      break;
    }
    default:
      break;
  }

  return dist;
}

int VWDone(void)
{  
  if (gCurrentVWOp != VW_OP_UNDEFINED)
    return 0;
  else
    return 1;
}

int VWWait(void)
{  
  while (gCurrentVWOp != VW_OP_UNDEFINED){}

  return 0;
}

int VWStalled(void)
{
  return -1;
}

int DIGITALSetup(int io, char direction)
{
  return -1;
}

int DIGITALRead(int io)
{
  return -1;
}

int DIGITALReadAll(void)
{
  return -1;
}

int DIGITALWrite(int io, int state)
{
  return -1;
}

int ANALOGRead(int channel)
{
  return -1;
}

int ANALOGVoltage(void)
{
  return -1;
}

int ANALOGRecord(int channel, int iterations)
{
  return -1;
}

int ANALOGTransfer(BYTE* buffer)
{
  return -1;
}

int IRTVGet(void)
{
  return -1;
}

int IRTVRead(void)
{
  return -1;
}

int IRTVFlush(void)
{
  return -1;
}

int IRTVGetStatus(void)
{
  return -1;
}

int RADIOInit(void)
{
  return -1;
}

int RADIOGetID(void)
{
  return -1;
}

int RADIOSend(int id, char* buf)
{
  return -1;
}

int RADIOReceive(int *id_no, char* buf, int size)
{
  return -1;
} 

int RADIOCheck(void)
{
  return -1;
}                           

int RADIOStatus(int IDlist[])
{
  return -1;
}                  

int RADIORelease(void)
{
  return -1;
}