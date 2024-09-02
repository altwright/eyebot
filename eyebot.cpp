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
#define PIN_LEFT_MOTOR_DIR 3
#define PIN_LEFT_MOTOR_PWM 10
#define PIN_RIGHT_MOTOR_DIR 11
#define PIN_RIGHT_MOTOR_PWM 12

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

#define CAM_NUM_IMAGE_SEGMENTS 8
#define MAX_RX_SEGMENT_SIZE 4800

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define INTENSITY_THRESHOLD 10

#define RAW_DISTANCE_PAIR_COUNT 25

#define LCD_WIDTH 170
#define LCD_HEIGHT 320

#define QQVGA_WIDTH 160
#define QQVGA_HEIGHT 120

#define CAM_TIMEOUT_MS 500 

struct RawDistancePair
{
  int raw;
  int distance;//mm
};

enum VWOperation
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
static TouchLib gTouch(Wire, PIN_IIC_SDA, PIN_IIC_SCL, CTS820_SLAVE_ADDRESS, PIN_TOUCH_RES);
static bool gTouchEnabled = true;

static RGB565 *pLCDBuffer = NULL;

static volatile VWOperation gCurrentVWOp = VW_OP_UNDEFINED;

static timer_group_t gTimerGroup = TIMER_GROUP_0;
static timer_idx_t gMotorTimerIdx = TIMER_0;

static int gLeftMotorOffset;
static int gRightMotorOffset;
static int gLeftMotorPWM;
static int gRightMotorPWM ;

// mm/s
static const int MAX_LIN_SPEED = 340;
// degrees/s
static const int MAX_ANG_SPEED = 180;

static volatile int gXPos, gYPos, gAngle;
static int gFinalXPos, gFinalYPos, gFinalAngle;
static volatile int gLinSpeed = 0, gAngSpeed = 0;
static unsigned long gOpTotalTime = 0, gOpStartTime = 0;

static int gImgXStart = 0;
static int gImgYStart = 0;
static int gImgWidth = QQVGA_WIDTH;
static int gImgHeight = QQVGA_HEIGHT;

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
  analogWrite(PIN_LEFT_MOTOR_PWM, 0);
  analogWrite(PIN_RIGHT_MOTOR_PWM, 0);

  gXPos = gFinalXPos;
  gYPos = gFinalYPos;
  gAngle = gFinalAngle;

  gCurrentVWOp = VW_OP_UNDEFINED;
  gLinSpeed = 0;
  gAngSpeed = 0;

  return true;
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
    return false;

  if (err = timer_set_counter_value(gTimerGroup, gMotorTimerIdx, 0))
    return false;

  //Assumes 2000 increments per s
  if (err = timer_set_alarm_value(gTimerGroup, gMotorTimerIdx, 2*(u64)time_ms))
    return false;

  if (err = timer_set_alarm(gTimerGroup, gMotorTimerIdx, TIMER_ALARM_EN))
    return false;

  if (err = timer_start(gTimerGroup, gMotorTimerIdx))
    return false;

  return true;
}

static void calcDeltaPosition(int delta_ms, int *dx, int *dy, int *dphi)
{
  *dx = 0;
  *dy = 0;
  *dphi = 0;

  switch (gCurrentVWOp)
  {
    case VW_OP_UNDEFINED:
    case VW_OP_STRAIGHT:
    case VW_OP_CURVE:
    {
      float delta_arc = gLinSpeed * (delta_ms / 1000.0f);
      float eyebot_angle_rad = gAngle * M_PI / 180.0f;
      int delta_deg = gAngSpeed * (delta_ms / 1000.0f);

      if (gLinSpeed < 0) delta_deg *= -1;

      if (gAngSpeed != 0 && delta_ms != 0 && abs(delta_deg) > 5)
      {
        float delta_rad = delta_deg * M_PI / 180.0f;
        float radius = delta_arc / delta_rad;

        float x = radius * sinf(delta_rad);
        float y = radius - (radius * cosf(delta_rad));

        *dx = x*sinf(eyebot_angle_rad) + y*cosf(eyebot_angle_rad);
        *dy = x*cosf(eyebot_angle_rad) - y*sinf(eyebot_angle_rad);
      }
      else
      {
        *dy = delta_arc * cosf(eyebot_angle_rad);
        *dx = delta_arc * sinf(eyebot_angle_rad);
      }

      *dphi = delta_deg;

      break;
    }
    case VW_OP_TURN:
    {
      float delta_deg = gAngSpeed * (2*delta_ms / 1000.0f);

      *dphi = delta_deg;

      break;
    }
    default:
      break;
  }
}

int EYEBOTInit()
{
  /////////////
  //Motor init
  /////////////

  analogWrite(PIN_LEFT_MOTOR_PWM, 0);
  analogWrite(PIN_RIGHT_MOTOR_PWM, 0);

  pinMode(PIN_LEFT_MOTOR_DIR, OUTPUT);
  pinMode(PIN_RIGHT_MOTOR_DIR, OUTPUT);
  digitalWrite(PIN_LEFT_MOTOR_DIR, HIGH);
  digitalWrite(PIN_RIGHT_MOTOR_DIR, LOW);

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
    .max_transfer_sz = MAX_RX_SEGMENT_SIZE 
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
  delay(100);
  digitalWrite(PIN_TOUCH_RES, HIGH);
  Wire.begin(PIN_IIC_SDA, PIN_IIC_SCL);

  if (!gTouch.init())
    gTouchEnabled = false;

  pLCDBuffer = (RGB565*)calloc(LCD_WIDTH*LCD_HEIGHT, sizeof(RGB565));
  if (!pLCDBuffer)
    return -1;

  return 0;
}

int LCDPrintf(const char* format, ...)
{
  if (!format)
    return -1;

  va_list arg_ptr;
  va_start(arg_ptr, format);
  char *str_buf = (char*)pLCDBuffer;

  vsnprintf(str_buf, LCD_WIDTH*LCD_HEIGHT*sizeof(RGB565), format, arg_ptr);

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
  char *str_buf = (char*)pLCDBuffer;

  vsnprintf(str_buf, LCD_WIDTH*LCD_HEIGHT*sizeof(RGB565), format, arg_ptr);

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
  {
    if (y1 < y2)
      gTFT.drawFastVLine(x1, y1, y2-y1, hue);
    else
      gTFT.drawFastVLine(x1, y2, y1-y2, hue);
  }
  else if (y1 - y2 == 0)
  {
    if (x1 < x2)
      gTFT.drawFastHLine(x1, y1, x2-x1, hue);
    else
      gTFT.drawFastHLine(x2, y1, x1-x2, hue);
  }
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
  gImgXStart = x;
  gImgYStart = y;
  gImgWidth = xs < 0 || xs > LCD_WIDTH ? LCD_WIDTH : xs;
  gImgHeight = ys < 0 || ys > LCD_HEIGHT ? LCD_HEIGHT : ys;

  return 0;
}

int LCDImage(BYTE *img)
{
  if (!img)
    return -1;
  
  COLOR *col_img = (COLOR*)img;

  for (int y = 0; y < gImgHeight; y++)
  for (int x = 0; x < gImgWidth; x++)
  {
    int i = y*gImgWidth + x;

    pLCDBuffer[i] = rgb888To565(col_img[i]);
    rgb565SwapEndianess(&pLCDBuffer[i]);
  }

  gTFT.pushRect(gImgXStart, gImgYStart, gImgWidth, gImgHeight, pLCDBuffer);

  return 0;
}

int LCDImageGray(BYTE *g)
{
  if (!g)
    return -1;

  for (int y = 0; y < gImgHeight; y++)
  for (int x = 0; x < gImgWidth; x++)
  {
    int i = y*gImgWidth + x;

    COLOR col = IPPRGB2Col(g[i], g[i], g[i]);

    pLCDBuffer[i] = rgb888To565(col);
    rgb565SwapEndianess(&pLCDBuffer[i]);
  }

  gTFT.pushRect(gImgXStart, gImgYStart, gImgWidth, gImgHeight, pLCDBuffer);
 
  return 0;
}

int LCDImageBinary(BYTE *b)
{
  if (!b)
    return -1;

  for (int y = 0; y < gImgHeight; y++)
  for (int x = 0; x < gImgWidth; x++)
  {
    int i = y*gImgWidth + x;
    pLCDBuffer[i] = b[i] ? 0xFFFF : 0;
  }

  gTFT.pushRect(gImgXStart, gImgYStart, gImgWidth, gImgHeight, pLCDBuffer);
 
  return 0;
}

int LCDRefresh(void)
{
  return 0;
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

  BYTE *rx_buf = (BYTE*)pLCDBuffer;

  unsigned long start_time = millis();
  bool timed_out = false;

  for (int i = 0; i < CAM_NUM_IMAGE_SEGMENTS; i++)
  {
    spi_transaction_t t = {};
    t.length = MAX_RX_SEGMENT_SIZE*8;//bit length
    t.tx_buffer = NULL;
    t.rx_buffer = rx_buf + i*MAX_RX_SEGMENT_SIZE;

    while ((millis() - start_time < CAM_TIMEOUT_MS) && digitalRead(PIN_CAM_SIGNAL));

    if (digitalRead(PIN_CAM_SIGNAL))
    {
      timed_out = true;
      break;
    }

    esp_err_t err = spi_device_transmit(gCamSPIHandle, &t);
    assert(err == ESP_OK);
  }

  RGB565 *pixels = (RGB565*)rx_buf;
  COLOR *img = (COLOR*)buf;

  if (timed_out)
  {
    for (int i = 0; i < QQVGA_WIDTH*QQVGA_HEIGHT; i++)
    {
      img[i] = RED;
    }
  }
  else
  {
    for (int i = 0; i < QQVGA_WIDTH*QQVGA_HEIGHT; i++)
    {
      img[i] = rgb565To888(pixels[i]);
    }
  }

  return 0;
}

int CAMGetGray(BYTE *buf)
{
  if (!buf)
    return -1;

  BYTE *rx_buf = (BYTE*)pLCDBuffer;

  unsigned long start_time = millis();
  bool timed_out = false;

  for (int i = 0; i < CAM_NUM_IMAGE_SEGMENTS; i++)
  {
    spi_transaction_t t = {};
    t.length = MAX_RX_SEGMENT_SIZE*8;//bit length
    t.tx_buffer = NULL;
    t.rx_buffer = rx_buf + i*MAX_RX_SEGMENT_SIZE;

    while ((millis() - start_time < CAM_TIMEOUT_MS) && digitalRead(PIN_CAM_SIGNAL));

    if (digitalRead(PIN_CAM_SIGNAL))
    {
      timed_out = true;
      break;
    }

    esp_err_t err = spi_device_transmit(gCamSPIHandle, &t);
    assert(err == ESP_OK);
  }

  RGB565 *pixels = (RGB565*)rx_buf;

  if (timed_out)
  {
    for (int i = 0; i < QQVGA_WIDTH*QQVGA_HEIGHT; i++)
    {
      buf[i] = 0xFF;
    }
  }
  else
  {
    float divisor = 1.0f/3.0f;

    for (int i = 0; i < QQVGA_WIDTH*QQVGA_HEIGHT; i++)
    {
      COLOR col = rgb565To888(pixels[i]);

      BYTE r, g, b;
      IPPCol2RGB(col, &r, &g, &b);

      buf[i] = (r + g + b)*divisor;
    }
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
  for (int x = 1; x < QQVGA_WIDTH - 1; x++)
  {
    int i = y*QQVGA_WIDTH + x;

    int delta = abs(4*grayIn[i] - grayIn[i-1] - grayIn[i+1] - grayIn[i-QQVGA_WIDTH] - grayIn[i+QQVGA_WIDTH]);

    if (delta > 0xFF) 
      delta = 0xFF;

    grayOut[i] = (BYTE)delta;
  }
}

void IPSobel(BYTE* grayIn, BYTE* grayOut)
{
  if (!grayIn || !grayOut)
    return;

  memset(grayOut, 0, QQVGA_WIDTH); // clear first row

  for (int y = 1; y < QQVGA_HEIGHT-1; y++)
  for (int x = 1; x < QQVGA_WIDTH-1; x++)
  {
    int i = y*QQVGA_WIDTH + x;

    int deltaX = 2*grayIn[i+1] + grayIn[i-QQVGA_WIDTH+1] + grayIn[i+QQVGA_WIDTH+1] - 2*grayIn[i-1] - grayIn[i-QQVGA_WIDTH-1] - grayIn[i+QQVGA_WIDTH-1];
    int deltaY = grayIn[i-QQVGA_WIDTH-1] + 2*grayIn[i-QQVGA_WIDTH] + grayIn[i-QQVGA_WIDTH+1] - grayIn[i+QQVGA_WIDTH-1] - 2*grayIn[i+QQVGA_WIDTH] - grayIn[i+QQVGA_WIDTH+1];

    int grad = (abs(deltaX) + abs(deltaY)) / 3;
    if (grad > 255) 
      grad = 255;

    grayOut[i] = (BYTE)grad;
  }

  memset(grayOut + (QQVGA_HEIGHT-1)*(QQVGA_WIDTH), 0, QQVGA_WIDTH); 
}

void IPCol2Gray(BYTE* imgIn, BYTE* grayOut)
{
  if (!imgIn || !grayOut)
    return;
  
  COLOR *img = (COLOR*)imgIn;
  float divisor = 1.0f/3.0f;

  for (int y = 0; y < QQVGA_HEIGHT; y++)
  for (int x = 0; x < QQVGA_WIDTH; x++)
  {
    int i = y * QQVGA_WIDTH + x;

    BYTE r, g, b;
    IPPCol2RGB(img[i], &r, &g, &b);

    grayOut[i] = (r + g + b)*divisor;
  }
}

void IPGray2Col(BYTE* imgIn, BYTE* colOut)
{
  if (!imgIn || !colOut)
    return;
  
  COLOR *col = (COLOR*)colOut;

  for (int y = 0; y < QQVGA_HEIGHT; y++)
  for (int x = 0; x < QQVGA_WIDTH; x++)
  {
    int i = y * QQVGA_WIDTH + x;
    col[i] = IPPRGB2Col(imgIn[i], imgIn[i], imgIn[i]);
  }
}

void IPRGB2Col(BYTE* r, BYTE* g, BYTE* b, BYTE* imgOut)
{
  COLOR *img = (COLOR*)imgOut;

  for (int y = 0; y < QQVGA_HEIGHT; y++)
  for (int x = 0; x < QQVGA_WIDTH; x++)
  {
    int i = y * QQVGA_WIDTH + x;

    img[i] = IPPRGB2Col(r[i], g[i], b[i]);
  }
}

void IPCol2HSI(BYTE* img, BYTE* h, BYTE* s, BYTE* i)
{
  if (!h || !s || !i)
    return;

  COLOR *col = (COLOR*)img;

  for (int y = 0; y < QQVGA_HEIGHT; y++)
  for (int x = 0; x < QQVGA_WIDTH; x++)
  {
    int idx = y * QQVGA_WIDTH + x;

    IPPCol2HSI(col[idx], &h[idx], &s[idx], &i[idx]);
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
  for (int x = 0; x < QQVGA_WIDTH; x++)
  {
    int i = y*QQVGA_WIDTH + x;
    out[i] = img2[i] ? img2[i] : img1[i];
  }
}

void IPOverlayGray(BYTE* g1, BYTE* g2, COLOR col, BYTE* cOut)
{
  if (!g1 || !g2 || !cOut)
    return;
  
  COLOR *out = (COLOR*)cOut;

  for (int y = 0; y < QQVGA_HEIGHT; y++)
  for (int x = 0; x < QQVGA_WIDTH; x++)
  {
    int i = y*QQVGA_WIDTH + x;
    out[i] = g2[i] ? col : IPPRGB2Col(g1[i], g1[i], g1[i]);
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
  
  // Found via experimentation
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

  analogWrite(PIN_LEFT_MOTOR_PWM, 0);
  analogWrite(PIN_RIGHT_MOTOR_PWM, 0);

  // Update current eyebot position
  int x, y, angle;
  VWGetPosition(&x, &y, &angle);
  VWSetPosition(x, y, angle);

  if (lin_speed < -1 * MAX_LIN_SPEED) lin_speed = -1 * MAX_LIN_SPEED;
  else if (lin_speed > MAX_LIN_SPEED) lin_speed = MAX_LIN_SPEED;
  
  if (ang_speed < -1 * MAX_ANG_SPEED) ang_speed = -1 * MAX_ANG_SPEED;
  else if (ang_speed > MAX_ANG_SPEED) ang_speed = MAX_ANG_SPEED;

  gLinSpeed = lin_speed;
  gAngSpeed = ang_speed;

  bool reverse = lin_speed >= 0 ? false : true;
  if (reverse) lin_speed *= -1;

  bool clockwise = ang_speed >= 0 ? true : false;
  if (!clockwise) ang_speed *= -1;
  
  float ang_speed_percentage = ang_speed / (float)MAX_ANG_SPEED;

  if (lin_speed != 0)
  {
    float lin_speed_percentage = lin_speed / (float)MAX_LIN_SPEED;
    int motor_pwm = 255 * lin_speed_percentage;

    gLeftMotorPWM = motor_pwm + gLeftMotorOffset;
    gRightMotorPWM = motor_pwm + gRightMotorOffset;

    if (gLeftMotorPWM < 0) gLeftMotorPWM = 0;
    else if (gLeftMotorPWM > 255) gLeftMotorPWM = 255;
    
    if (gRightMotorPWM < 0) gRightMotorPWM = 0;
    else if (gRightMotorPWM > 255) gRightMotorPWM = 255;

    int ang_pwm_offset = 255 * ang_speed_percentage;

    if (clockwise)
    {
      gRightMotorPWM -= ang_pwm_offset;
      if (gRightMotorPWM < 0) gRightMotorPWM = 0;
    }
    else
    {
      gLeftMotorPWM -= ang_pwm_offset;
      if (gLeftMotorPWM < 0) gLeftMotorPWM = 0;
    }

    if (reverse)
    {
      digitalWrite(PIN_LEFT_MOTOR_DIR, LOW);
      analogWrite(PIN_LEFT_MOTOR_PWM, gLeftMotorPWM);
      digitalWrite(PIN_RIGHT_MOTOR_DIR, HIGH);
      analogWrite(PIN_RIGHT_MOTOR_PWM, gRightMotorPWM);
    }
    else
    {
      digitalWrite(PIN_LEFT_MOTOR_DIR, HIGH);
      analogWrite(PIN_LEFT_MOTOR_PWM, gLeftMotorPWM);
      digitalWrite(PIN_RIGHT_MOTOR_DIR, LOW);
      analogWrite(PIN_RIGHT_MOTOR_PWM, gRightMotorPWM);
    }
  }
  else
  {
    // When turning on spot, angular speed is inherently doubled, since
    // it only assumes that it is pivoting around a stationary wheel.

    if (clockwise)
    {
      digitalWrite(PIN_LEFT_MOTOR_DIR, HIGH);
      analogWrite(PIN_LEFT_MOTOR_PWM, 255 * ang_speed_percentage);
      digitalWrite(PIN_RIGHT_MOTOR_DIR, HIGH);
      analogWrite(PIN_RIGHT_MOTOR_PWM, 255 * ang_speed_percentage);
    }
    else
    {
      digitalWrite(PIN_LEFT_MOTOR_DIR, LOW);
      analogWrite(PIN_LEFT_MOTOR_PWM, 255 * ang_speed_percentage);
      digitalWrite(PIN_RIGHT_MOTOR_DIR, LOW);
      analogWrite(PIN_RIGHT_MOTOR_PWM, 255 * ang_speed_percentage);
    }
  }

  gOpStartTime = millis();
  gOpTotalTime = 0;// By default indefinite

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
  int dx, dy, dphi;
  calcDeltaPosition(delta, &dx, &dy, &dphi);

  *x = gXPos + dx;
  *y = gYPos + dy;
  *phi = gAngle + dphi;

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
    return -1;
  
  gOpTotalTime = time_taken;
  gCurrentVWOp = VW_OP_STRAIGHT;

  int dx, dy, dphi;
  calcDeltaPosition(gOpTotalTime, &dx, &dy, &dphi);
  gFinalXPos = gXPos + dx;
  gFinalYPos = gYPos + dy;
  gFinalAngle = gAngle + dphi;

  return 0;
}

int VWTurn(int angle, int ang_speed)
{
  if (ang_speed <= 0)
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
  
  // With both wheels turning in opposite directions, the angular speed is effectively doubled.
  // Therefore the time taken is halved.
  float time_taken = (angle / (float)ang_speed) * 1000 / 2;

  if (!setMotorKillTimer((u64)time_taken))
    return -1;

  gOpTotalTime = time_taken;
  gCurrentVWOp = VW_OP_TURN;

  int dx, dy, dphi;
  calcDeltaPosition(gOpTotalTime, &dx, &dy, &dphi);
  gFinalXPos = gXPos + dx;
  gFinalYPos = gYPos + dy;
  gFinalAngle = gAngle + dphi;

  return 0;
}

int VWCurve(int dist, int angle, int lin_speed)
{
  if (lin_speed <= 0) return -1;
  
  if (lin_speed > MAX_LIN_SPEED) lin_speed = MAX_LIN_SPEED;
  
  bool reverse = dist < 0 ? true : false;
  bool clockwise = angle >= 0 ? true : false;

  if (reverse) dist *= -1;

  float time_taken = dist / (float)lin_speed;
  int ang_speed = angle / time_taken;

  if (ang_speed > MAX_ANG_SPEED) ang_speed = MAX_ANG_SPEED;
  if (ang_speed < -1*MAX_ANG_SPEED) ang_speed = -1*MAX_ANG_SPEED;

  if (reverse)
    VWSetSpeed(-1*lin_speed, ang_speed);
  else
    VWSetSpeed(lin_speed, ang_speed);
  
  time_taken *= 1000;

  if (!setMotorKillTimer((u64)time_taken))
    return -1;

  gOpTotalTime = time_taken;
  gCurrentVWOp = VW_OP_CURVE;

  int dx, dy, dphi;
  calcDeltaPosition(gOpTotalTime, &dx, &dy, &dphi);
  gFinalXPos = gXPos + dx;
  gFinalYPos = gYPos + dy;
  gFinalAngle = gAngle + dphi;

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