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
typedef uint16_t rgb565;

#define CAM_NUM_IMAGE_SEGMENTS 8
#define QQVGA_RGB565_BUFFER_SIZE QQVGA_WIDTH*QQVGA_HEIGHT*sizeof(rgb565)

#define MAX_RX_SEGMENT_SIZE 4800

////////////////
//Core Functions
////////////////

static TFT_eSPI tft = TFT_eSPI(LCD_WIDTH, LCD_HEIGHT);
static spi_device_handle_t cam_spi_handle;
TouchLib touch(Wire, PIN_IIC_SDA, PIN_IIC_SCL, CTS820_SLAVE_ADDRESS, PIN_TOUCH_RES);

static rgb565 lcd_buf[LCD_HEIGHT*LCD_WIDTH];

enum drive_op
{
  DRV_OP_STRAIGHT,
  DRV_OP_TURN,
  DRV_OP_CURVE,
  DRV_OP_UNDEFINED
};

static volatile drive_op current_drv_op = DRV_OP_UNDEFINED;

static timer_group_t timer_group = TIMER_GROUP_0;
static timer_idx_t motor_timer_idx = TIMER_0;

static int left_motor_pwm_offset;
static int right_motor_pwm_offset;
static int left_motor_pwm;
static int right_motor_pwm ;

// mm/s
static int max_linear_speed = 340;
// degrees/s
static int max_angular_speed = 180;

static int eyebot_xpos, eyebot_ypos, eyebot_angle;
static int eyebot_lin_speed, eyebot_ang_speed;
static unsigned long eyebot_op_total_time, eyebot_op_start_time;

static bool motor_kill_timer_cb(void *arg)
{
  analogWrite(PIN_LEFT_MOTOR_FORWARD, 0);
  analogWrite(PIN_LEFT_MOTOR_BACKWARD, 0);
  analogWrite(PIN_RIGHT_MOTOR_FORWARD, 0);
  analogWrite(PIN_RIGHT_MOTOR_BACKWARD, 0);

  current_drv_op = DRV_OP_UNDEFINED;
  eyebot_lin_speed = 0;
  eyebot_ang_speed = 0;

  return true;
}

static input_callback left_button_cb = NULL;
static input_callback right_button_cb = NULL;
static input_callback touch_cb = NULL;

static void default_left_button_cb()
{
  if (left_button_cb)
    left_button_cb();
}

static void default_right_button_cb()
{
  if (right_button_cb)
    right_button_cb();
}

static void default_touch_cb()
{
  if (touch_cb)
    touch_cb();
}

bool EYEBOTInit()
{
  /////////////
  //Motor init
  /////////////

  analogWrite(PIN_LEFT_MOTOR_FORWARD, 0);
  analogWrite(PIN_LEFT_MOTOR_BACKWARD, 0);
  analogWrite(PIN_RIGHT_MOTOR_FORWARD, 0);
  analogWrite(PIN_RIGHT_MOTOR_BACKWARD, 0);

  /////////////////////////////////////
  // Enable being powered by a battery
  /////////////////////////////////////

  //pinMode(PIN_BATTERY_POWER, OUTPUT);
  //digitalWrite(PIN_BATTERY_POWER, HIGH);

  //////////////////////
  //Initialise T-Display
  //////////////////////
  tft.init();
  tft.fillScreen(TFT_BLACK);
  
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
  err = spi_bus_add_device(SPI2_HOST, &devcfg, &cam_spi_handle);
  assert(err == ESP_OK);
  
  ///////////////////////////////
  //Set default button callbacks
  ///////////////////////////////

  attachInterrupt(digitalPinToInterrupt(PIN_LEFT_BUTTON), default_left_button_cb, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_RIGHT_BUTTON), default_right_button_cb, CHANGE);

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

  err = timer_init(timer_group, motor_timer_idx, &timer_config);
  assert(err == ESP_OK);

  err = timer_isr_callback_add(timer_group, motor_timer_idx, motor_kill_timer_cb, NULL, 0);
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

  if (!touch.init()) {
    Serial.println("Touch IC not found");
  }

  attachInterrupt(digitalPinToInterrupt(PIN_TOUCH_INT), default_touch_cb, FALLING);

  return true;
}

///////////////////
//Camera Functions
///////////////////

static bool IPSwapEndianess(rgb565 *hue)
{
  uint16_t lower = (*hue & 0xFF00) >> 8;
  *hue <<= 8;
  *hue |= lower;

  return true;
}

static bool IP888To565(rgb in_hue, rgb565 *out_hue)
{
  *out_hue = tft.color24to16(in_hue);

  return true;
}

static bool IP565To888(rgb565 in_hue, rgb *out_hue)
{
  *out_hue = tft.color16to24(in_hue);

  return true;
}

bool CAMGetImage(rgb qqvga_buf[])
{
  if (!qqvga_buf)
    return false;

  byte *rx_buf = (byte*)lcd_buf;

  for (int i = 0; i < CAM_NUM_IMAGE_SEGMENTS; i++)
  {
    spi_transaction_t t = {};
    t.length = MAX_RX_SEGMENT_SIZE*8;//bit length
    t.tx_buffer = NULL;
    t.rx_buffer = rx_buf + i*MAX_RX_SEGMENT_SIZE;

    while (digitalRead(PIN_CAM_SIGNAL));

    esp_err_t err = spi_device_transmit(cam_spi_handle, &t);
    assert(err == ESP_OK);
  }

  rgb565 *cam_buf = (rgb565*)rx_buf;

  for (int i = 0; i < QQVGA_WIDTH*QQVGA_HEIGHT; i++)
  {
    IPSwapEndianess(&cam_buf[i]);
    IP565To888(cam_buf[i], &qqvga_buf[i]);
  }

  return true;
}

/////////////////////////////
//Image Processing Functions
/////////////////////////////

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

bool IPGetRGB(rgb hue, byte *r, byte *g, byte *b)
{
  if (!r || !g || !b)
    return false;

  *r = (hue >> 16) & 0xFF;
  *g = (hue >> 8) & 0xFF;
  *b = (hue) & 0xFF;

  return true;
}

bool IPSetRGB(byte r, byte g, byte b, rgb *hue)
{
  if (!hue)
    return false;

  rgb565 col = tft.color565(r, g, b);
  IP565To888(col, hue);

  return true;
}

bool IPRGBToGrayscale(rgb hue, grayscale *gray)
{
  if (!gray)
    return false;

  byte r, g, b;
  IPGetRGB(hue, &r, &g, &b);

  *gray = (r + g + b)/3;

  return true;
}

bool IPRGBToGrayscale(rgb col, rgb *gray)
{
  if (!gray)
    return false;

  grayscale val = 0;
  IPRGBToGrayscale(col, &val);

  IPGrayscaleToRGB(val, gray);

  return true;
}

bool IPRGBToGrayscale(int img_width, int img_height, const rgb col_img[], grayscale gray_img[])
{
  if (!col_img || !gray_img)
    return false;

  for (int y = 0; y < img_height; y++)
  {
    for (int x = 0; x < img_width; x++)
    {
      int i = y*img_width + x;

      IPRGBToGrayscale(col_img[i], &gray_img[i]);
    }
  }

  return true;
}

bool IPRGBToGrayscale(int img_width, int img_height, const rgb col_img[], rgb gray_img[])
{
  if (!col_img || !gray_img)
    return false;

  for (int y = 0; y < img_height; y++)
  {
    for (int x = 0; x < img_width; x++)
    {
      int i = y*img_width + x;

      IPRGBToGrayscale(col_img[i], &gray_img[i]);
    }
  }

  return true;
}

bool IPGrayscaleToRGB(grayscale gray, rgb *col)
{
  if (!col)
    return false;

  IPSetRGB(gray, gray, gray, col);

  return true;
}

bool IPGrayscaleToRGB(int img_width, int img_height, const grayscale gray_img[], rgb col_img[])
{
  if (!gray_img || !col_img)
    return false;

  for (int y = 0; y < img_height; y++)
  {
    for (int x = 0; x < img_width; x++)
    {
      int i = y*img_width + x;

      IPGrayscaleToRGB(gray_img[i], &col_img[i]);
    }
  }

  return true;
}

#define INTENSITY_THRESHOLD 10

// Transform RGB pixel to HSI
bool IPRGBToHSI(rgb col, hsi *value)
{
  if (!value)
    return false;

  byte r, g, b;
  IPGetRGB(col, &r, &g, &b);

  byte max   = MAX(r, MAX(g, b));
  byte min   = MIN(r, MIN(g, b));
  byte delta = max - min;

  value->intensity = (r + g + b)/3;

  if (value->intensity > 0) 
    value->saturation = 255 - (255 * min)/(value->intensity);
  else 
    value->saturation = 0;

  if ((2*delta > max) && (value->intensity > INTENSITY_THRESHOLD))
  { 
    if (r == max) 
      value->hue =  43 + 42*(g-b)/delta; // +/-42 [  1.. 85]
    else if (g == max) 
      value->hue = 128 + 42*(b-r)/delta; // +/-42 [ 86..170]
    else if (b == max) 
      value->hue = 213 + 42*(r-g)/delta; // +/-42 [171..255]
  }
  else 
    value->hue = 0; // grayscale, not color

  return true;
}

bool IPRGBToHSI(int img_width, int img_height, const rgb img[], hsi values[])
{
  if (!img || !values)
    return false;

  for (int y = 0; y < img_height; y++)
  {
    for (int x = 0; x < img_width; x++)
    {
      int i = y*img_width + x;

      IPRGBToHSI(img[i], &values[i]);
    }
  }

  return true;
}

bool IPLaplace(int img_width, int img_height, const grayscale *in, grayscale *out)
{
  if (!in || !out)
    return false;

  for (int y = 1; y < img_height - 1; y++)
  {
    for (int x = 1; x < img_width - 1; x++)
    {
      int i = y*img_width + x;

      int delta = abs(4*in[i] - in[i-1] - in[i+1] - in[i-img_width] - in[i+img_width]);

      if (delta > 0xFF) 
        delta = 0xFF;

      out[i] = (grayscale)delta;
    }
  }

  return true;
}

bool IPSobel(int img_width, int img_height, const grayscale *in, grayscale *out)
{
  if (!in || !out)
    return false;

  memset(out, 0, img_width); // clear first row

  for (int y = 1; y < img_height-1; y++)
  {
    for (int x = 1; x < img_width-1; x++)
    {
      int i = y*img_width + x;

      int deltaX = 2*in[i+1] + in[i-img_width+1] + in[i+img_width+1] - 2*in[i-1] - in[i-img_width-1] - in[i+img_width-1];
      int deltaY = in[i-img_width-1] + 2*in[i-img_width] + in[i-img_width+1] - in[i+img_width-1] - 2*in[i+img_width] - in[i+img_width+1];


      int grad = (abs(deltaX) + abs(deltaY)) / 3;
      if (grad > 0xFF) 
        grad = 0xFF;

      out[i] = (grayscale)grad;
    }
  }

  memset(out + (img_height-1)*(img_width), 0, img_width);

  return true;
}

bool IPOverlay(int width, int height, int intensity_threshold, const rgb bg[], const rgb fg[], rgb out[])
{
  if (!bg || !fg || !out)
    return false;

  intensity_threshold = intensity_threshold < 0 || intensity_threshold >= 255 ? 0 : intensity_threshold;

  for (int y = 0; y < height; y++)
  {
    for (int x = 0; x < width; x++)
    {
      int i = y*width + x;

      byte r = 0, g = 0, b = 0;
      IPGetRGB(fg[i], &r, &g, &b);

      int intensity = (r + g + b)/3;

      out[i] = intensity > intensity_threshold ? fg[i] : bg[i];
    }
  }

  return true;
}

bool IPOverlay(int width, int height, int intensity_threshold, rgb overlay_color, const rgb bg[], const grayscale fg[], rgb out[])
{
  if (!bg || !fg || !out)
    return false;

  intensity_threshold = intensity_threshold < 0 || intensity_threshold >= 255 ? 0 : intensity_threshold;

  byte r = 0, g = 0, b = 0;
  IPGetRGB(overlay_color, &r, &g, &b);

  for (int y = 0; y < height; y++)
  {
    for (int x = 0; x < width; x++)
    {
      int i = y*width + x;

      if (fg[i] > intensity_threshold)
      {
        float intensity = fg[i] / 255.0f;
        float red = r * intensity;
        float green = g * intensity;
        float blue = b * intensity;

        rgb new_overlay_col = 0;
        IPSetRGB((byte)red, (byte)green, (byte)blue, &new_overlay_col);

        out[i] = new_overlay_col;
      }
      else
      {
        out[i] = bg[i];
      }
    }
  }

  return true;
}

bool IPOverlay(int width, int height, int intensity_threshold, rgb overlay_color, const grayscale bg[], const grayscale fg[], rgb out[])
{
  if (!bg || !fg || !out)
    return false;

  intensity_threshold = intensity_threshold < 0 || intensity_threshold >= 255 ? 0 : intensity_threshold;

  byte r = 0, g = 0, b = 0;
  IPGetRGB(overlay_color, &r, &g, &b);

  for (int y = 0; y < height; y++)
  {
    for (int x = 0; x < width; x++)
    {
      int i = y*width + x;

      if (fg[i] > intensity_threshold)
      {
        float intensity = fg[i] / 255.0f;
        float red = r * intensity;
        float green = g * intensity;
        float blue = b * intensity;

        rgb new_overlay_col = 0;
        IPSetRGB((byte)red, (byte)green, (byte)blue, &new_overlay_col);

        out[i] = new_overlay_col;
      }
      else
      {
        IPSetRGB(bg[i], bg[i], bg[i], &out[i]);
      }
    }
  }

  return true;
}

bool IPOverlay(int width, int height, int intensity_threshold, const grayscale bg[], const rgb fg[], rgb out[])
{
  if (!bg || !fg || !out)
    return false;

  intensity_threshold = intensity_threshold < 0 || intensity_threshold >= 255 ? 0 : intensity_threshold;

  for (int y = 0; y < height; y++)
  {
    for (int x = 0; x < width; x++)
    {
      int i = y*width + x;

      byte r = 0, g = 0, b = 0;
      IPGetRGB(fg[i], &r, &g, &b);

      int intensity = (r + g + b)/3;
      
      if (intensity > intensity_threshold)
        out[i] = fg[i];
      else
        IPSetRGB(bg[i], bg[i], bg[i], &out[i]);
    }
  }

  return true;
}

////////////////
//LCD Functions
////////////////

const rgb RED = 0xFF0000;
const rgb GREEN = 0x00FF00;
const rgb BLUE = 0x0000FF;
const rgb BLACK = 0;
const rgb WHITE = 0xFFFFFF;
const rgb YELLOW = 0xFFFF00;
const rgb MAGENTA = 0xFF00FF;
const rgb CYAN = 0x00FFFF;

bool LCDDrawImage(int xpos, int ypos, int img_width, int img_height, const rgb img[])
{
  if (!img)
    return false;

  if (img_width < 0 || img_height < 0 || (img_width*img_height) > (LCD_WIDTH*LCD_HEIGHT))
    return false;

  for (int y = 0; y < img_height; y++)
  {
    for (int x = 0; x < img_width; x++)
    {
      int i = y*img_width + x;

      IP888To565(img[i], &lcd_buf[i]);
      IPSwapEndianess(&lcd_buf[i]);
    }
  }

  tft.pushRect(xpos, ypos, img_width, img_height, lcd_buf);

  return true;
}

bool LCDDrawImage(int xpos, int ypos, int img_width, int img_height, const grayscale img[])
{
  if (!img)
    return false;

  if (img_width < 0 || img_height < 0 || (img_width*img_height) > (LCD_WIDTH*LCD_HEIGHT))
    return false;

  for (int y = 0; y < img_height; y++)
  {
    for (int x = 0; x < img_width; x++)
    {
      int i = y*img_width + x;

      rgb pix = 0;
      IPSetRGB(img[i], img[i], img[i], &pix);

      IP888To565(pix, &lcd_buf[i]);
      IPSwapEndianess(&lcd_buf[i]);
    }
  }

  tft.pushRect(xpos, ypos, img_width, img_height, lcd_buf);

  return true;
}

bool LCDRefresh()
{
  return true;
}

// bool LCDClear()
// {
//   tft.fillScreen(TFT_BLACK);

//   return true;
// }

bool LCDSetCursor(int xpos, int ypos)
{
  if (xpos < 0 || ypos < 0 || xpos > LCD_WIDTH || ypos > LCD_HEIGHT)
    return false;

  tft.setCursor(xpos, ypos);

  return true;
}

bool LCDGetCursor(int *xpos, int *ypos)
{
  if (!xpos || !ypos)
    return false;

  *xpos = tft.getCursorX();
  *ypos = tft.getCursorY();

  return true;
}

bool LCDSetFont(int font)
{
  if (font > 0xff)
    return false;
  
  tft.setTextFont(font);

  return true;
}

bool LCDSetFontColor(rgb fg, rgb bg)
{
  rgb565 fg_hue = 0;
  IP888To565(fg, &fg_hue);

  rgb565 bg_hue = 0;
  IP888To565(bg, &bg_hue);

  tft.setTextColor(fg_hue, bg_hue);

  return true;
}

// bool LCDSetFontSize(int size)
// {
//   if (size <= 0 || size > 0xFF)
//     return false;

//   tft.setTextSize(size);

//   return true;
// }

bool LCDPrint(const char *str)
{
  if (!str)
    return false;

  tft.print(str);

  return true;
}

bool LCDPrintln(const char *str)
{
  if (!str)
    return false;

  tft.println(str);

  return true;
}

bool LCDPrintAt(int xpos, int ypos, const char *str)
{
  if (!str)
    return false;

  tft.drawString(str, xpos, ypos);

  return true;
}

// bool LCDGetSize(int *lcd_width, int *lcd_height)
// {
//   if (!lcd_width || !lcd_height)
//     return false;

//   *lcd_width = LCD_WIDTH;
//   *lcd_height = LCD_HEIGHT;

//   return true;
// }

bool LCDSetPixel(int xpos, int ypos, rgb hue)
{
  rgb565 col = 0;
  IP888To565(hue, &col);

  tft.drawPixel(xpos, ypos, col);

  return true;
}

bool LCDGetPixel(int xpos, int ypos, rgb *hue)
{
  if (!hue)
    return false;

  rgb565 col = tft.readPixel(xpos, ypos);
  IP565To888(col, hue);

  return true;
}

bool LCDDrawLine(int xs, int ys, int xe, int ye, rgb hue)
{
  rgb565 col = 0;
  IP888To565(hue, &col);

  if (xs - xe == 0)
    if (ys < ye)
      tft.drawFastVLine(xs, ys, ye-ys, col);
    else
      tft.drawFastVLine(xs, ye, ys-ye, col);
  else if (ys - ye == 0)
    if (xs < xe)
      tft.drawFastHLine(xs, ys, xe-xs, col);
    else
      tft.drawFastHLine(xe, ys, xs-xe, col);
  else
    tft.drawLine(xs, ys, xe, ye, col);

  return true;
}

bool LCDDrawRect(int x, int y, int w, int h, rgb hue, bool fill)
{
  rgb565 col = 0;
  IP888To565(hue, &col);

  if (fill)
    tft.fillRect(x, y, w, h, col);
  else
    tft.drawRect(x, y, w, h, col);

  return true;
}

bool LCDDrawCircle(int x, int y, int radius, rgb hue, bool fill)
{
  rgb565 col = 0;
  IP888To565(hue, &col);

  if (fill)
    tft.fillCircle(x, y, radius, col);
  else 
    tft.drawCircle(x, y, radius, col);
  
  return false;
}

///////////////////
// Input Functions
///////////////////

static int INGetButtonPin(button b)
{
  int pin;
  switch (b)
  {
    case LEFT_BUTTON:
      pin = PIN_LEFT_BUTTON;
      break;
    case RIGHT_BUTTON:
      pin = PIN_RIGHT_BUTTON;
      break;
    default: 
      pin = -1;
      break;
  }

  return pin;
}

bool INReadButton(button b, bool *pressed)
{
  if (!pressed)
    return false;

  int pin = INGetButtonPin(b);

  if (pin < 0)
    return false;

  if (digitalRead(pin))
    *pressed = false;
  else
    *pressed = true;

  return true;
}

bool INWaitForButtonPress(button b)
{
  int pin = INGetButtonPin(b);

  while (digitalRead(pin));

  return true;
}

bool INWaitForButtonRelease(button b)
{
  int pin = INGetButtonPin(b);

  while (!digitalRead(pin));

  return true;
}

bool INSetButtonCallback(button b, input_callback cb)
{
  if (!cb)
    return false;

  switch (b)
  {
    case LEFT_BUTTON:
      left_button_cb = cb;
      break;
    case RIGHT_BUTTON:
      right_button_cb = cb;
      break;
    default: 
      break;
  }

  return true;
}

bool INClearButtonCallback(button b)
{
  switch (b)
  {
    case LEFT_BUTTON:
      left_button_cb = NULL;
      break;
    case RIGHT_BUTTON:
      right_button_cb = NULL;
      break;
    default: 
      break;
  }

  return true;
}


bool INReadTouch(int *x, int *y)
{
  if (touch.read())
  {
    TP_Point t = touch.getPoint(0);
    *x = t.x < LCD_WIDTH ? t.x : -1;
    *y = t.y < LCD_HEIGHT ? t.y : -1;
  }
  else
  {
    *x = -1;
    *y = -1;
  }

  return true;
}

bool INSetTouchCallback(input_callback cb)
{
  if (!cb)
    return false;
  
  touch_cb = cb;

  return true;
}

bool INClearTouchCallback()
{
  touch_cb = NULL;

  return true;
}

//////////////////////////
//Motor-Driving Functions
//////////////////////////


bool DRVSetMotorOffsets(int left_offset, int right_offset)
{
  left_motor_pwm_offset = left_offset < -255 || left_offset > 255 ? 0 : left_offset;
  right_motor_pwm_offset = right_offset < -255 || right_offset > 255 ? 0 : right_offset;

  return true;
}

bool DRVSetMaxLinearSpeed(int max_speed)
{
  max_linear_speed = max_speed < 0 ? 0 : max_speed;

  return true;
}

bool DRVSetMaxAngularSpeed(int max_speed)
{
  max_angular_speed = max_speed < 0 ? 0 : max_speed;

  return true;
}

bool DRVSetPosition(int x, int y, int angle)
{
  eyebot_xpos = x;
  eyebot_ypos = y;
  eyebot_angle = angle;

  return false;
}

bool DRVGetPosition(int *x, int *y, int *angle)
{
  if (!x || !y || !angle)
    return false;
  
  unsigned long delta = millis() - eyebot_op_start_time;

  switch (current_drv_op)
  {
    case DRV_OP_UNDEFINED:
    case DRV_OP_STRAIGHT:
    case DRV_OP_CURVE:
    {
      float delta_arc = eyebot_lin_speed * (delta / 1000.0f);
      float eyebot_angle_rad = eyebot_angle * M_PI / 180.0f;
      float delta_degrees = eyebot_ang_speed * (delta / 1000.0f);

      int dx = 0, dy = 0;
      if (eyebot_ang_speed != 0 && delta != 0)
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

      *x = eyebot_xpos + dx;
      *y = eyebot_ypos + dy;
      *angle = eyebot_angle + delta_degrees;

      break;
    }
    case DRV_OP_TURN:
    {
      float delta_degrees = eyebot_ang_speed * (delta / 1000.0f);

      *x = eyebot_xpos;
      *y = eyebot_ypos;
      *angle = eyebot_angle + delta_degrees;

      break;
    }
    default:
      return false;
  }

  return true;
}

bool DRVSetSpeed(int lin_speed, int ang_speed)
{
  if (current_drv_op != DRV_OP_UNDEFINED)
  {
    //There is still a background timer for
    //an ongoing drive op that must be stopped
    timer_pause(timer_group, motor_timer_idx);
  }

  analogWrite(PIN_LEFT_MOTOR_FORWARD, 0);
  analogWrite(PIN_LEFT_MOTOR_BACKWARD, 0);
  analogWrite(PIN_RIGHT_MOTOR_FORWARD, 0);
  analogWrite(PIN_RIGHT_MOTOR_BACKWARD, 0);

  // Set current eyebot position
  int x, y, angle;
  DRVGetPosition(&x, &y, &angle);
  DRVSetPosition(x, y, angle);

  if (lin_speed < -1 * max_linear_speed)
    lin_speed = -1 * max_linear_speed;
  
  if (lin_speed > max_linear_speed)
    lin_speed = max_linear_speed;
  
  if (ang_speed < -1 * max_angular_speed)
    ang_speed = -1 * max_angular_speed;
  
  if (ang_speed > max_angular_speed)
    ang_speed = max_angular_speed;

  eyebot_lin_speed = lin_speed;
  eyebot_ang_speed = ang_speed;

  bool reverse = lin_speed >= 0 ? false : true;
  if (reverse)
    lin_speed *= -1;

  bool clockwise = ang_speed >= 0 ? true : false;
  if (!clockwise)
    ang_speed *= -1;
  
  float ang_speed_percentage = ang_speed / (float)max_angular_speed;

  if (lin_speed != 0)
  {
    float lin_speed_percentage = lin_speed / (float)max_linear_speed;
    int motor_pwm = 255 * lin_speed_percentage;

    left_motor_pwm = motor_pwm + left_motor_pwm_offset;
    right_motor_pwm = motor_pwm + right_motor_pwm_offset;

    if (left_motor_pwm < 0)
      left_motor_pwm = 0;
    
    if (left_motor_pwm > 255)
      left_motor_pwm = 255;
    
    if (right_motor_pwm < 0)
      right_motor_pwm = 0;

    if (right_motor_pwm > 255)
      right_motor_pwm = 255;

    int ang_pwm_offset = 255 * ang_speed_percentage;

    if (clockwise)
    {
      right_motor_pwm -= ang_pwm_offset;
      if (right_motor_pwm < 0)
        right_motor_pwm = 0;
    }
    else
    {
      left_motor_pwm -= ang_pwm_offset;
      if (left_motor_pwm < 0)
        left_motor_pwm = 0;
    }

    if (reverse)
    {
      analogWrite(PIN_LEFT_MOTOR_FORWARD, 0);
      analogWrite(PIN_LEFT_MOTOR_BACKWARD, left_motor_pwm);
      analogWrite(PIN_RIGHT_MOTOR_FORWARD, 0);
      analogWrite(PIN_RIGHT_MOTOR_BACKWARD, right_motor_pwm);
    }
    else
    {
      analogWrite(PIN_LEFT_MOTOR_FORWARD, left_motor_pwm);
      analogWrite(PIN_LEFT_MOTOR_BACKWARD, 0);
      analogWrite(PIN_RIGHT_MOTOR_FORWARD, right_motor_pwm);
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

  eyebot_op_start_time = millis();
  eyebot_op_total_time = 0;//indefinite by default

  return true;
}     

static bool SetMotorKillTimer(u64 time_ms)
{
  esp_err_t err;
  if (err = timer_pause(timer_group, motor_timer_idx))
    Serial.printf("Failed to pause timer: %d\n", err);

  if (err = timer_set_counter_value(timer_group, motor_timer_idx, 0))
  {
    Serial.printf("Failed to set timer counter to zero: %d\n", err);
    return false;
  }

  //Assumes 2000 increments per s
  if (err = timer_set_alarm_value(timer_group, motor_timer_idx, 2*(u64)time_ms))
  {
    Serial.printf("Failed to set timer alarm value: %d\n", err);
    return false;
  }

  if (err = timer_set_alarm(timer_group, motor_timer_idx, TIMER_ALARM_EN))
  {
    Serial.printf("Failed to enable timer alarm: %d\n", err);
    return false;
  }

  if (err = timer_start(timer_group, motor_timer_idx))
  {
    Serial.printf("Failed to start timer: %d\n", err);
    return false;
  }

  return true;
}

bool DRVStraight(int dist, int speed)
{
  if (speed <= 0)
    return false;
  
  if (speed > max_linear_speed)
    speed = max_linear_speed;

  bool reverse = dist < 0 ? true : false;
  
  if (reverse)
  {
    DRVSetSpeed(-1*speed, 0);
    dist *= -1;
  }
  else
    DRVSetSpeed(speed, 0);

  //ms
  float time_taken = (dist / (float)speed) * 1000;

  if (!SetMotorKillTimer((u64)time_taken))
  {
    Serial.printf("Failed to start kill motor timer\n");
    return false;
  }
  
  eyebot_op_total_time = time_taken;

  current_drv_op = DRV_OP_STRAIGHT;

  return true;
}

bool DRVTurn(int angle, int speed)
{
  if (speed <= 0)
    return false;
  
  if (speed > max_angular_speed)
    speed = max_angular_speed;
  
  bool clockwise = angle < 0 ? false : true;

  if (clockwise)
    DRVSetSpeed(0, speed);
  else
  {
    DRVSetSpeed(0, -1*speed);
    angle *= -1;
  }
  
  float time_taken = (angle / (float)speed) * 1000 / 2;

  if (!SetMotorKillTimer((u64)time_taken))
    return false;

  eyebot_op_total_time = time_taken;

  current_drv_op = DRV_OP_TURN;

  return true;
}

bool DRVCurve(int dist, int angle, int lin_speed)
{
  if (lin_speed < 0)
    return false;
  
  if (lin_speed > max_linear_speed)
    lin_speed = max_linear_speed;
  
  bool reverse = dist < 0 ? true : false;
  bool clockwise = angle >= 0 ? true : false;

  if (reverse)
    dist *= -1;

  float time_taken = dist / (float)lin_speed;
  int ang_speed = angle / time_taken;

  if (clockwise)
  {
    if (ang_speed > max_angular_speed)
      ang_speed = max_angular_speed;
  }
  else
  {
    if (ang_speed < -1*max_angular_speed)
      ang_speed = -1*max_angular_speed;
  }

  if (reverse)
    DRVSetSpeed(-1*lin_speed, ang_speed);
  else
    DRVSetSpeed(lin_speed, ang_speed);
  
  time_taken *= 1000;

  if (!SetMotorKillTimer((u64)time_taken))
    return false;

  eyebot_op_total_time = time_taken;

  current_drv_op = DRV_OP_CURVE;

  return true;
}

bool DRVGoTo(int dx, int dy, int speed)
{
  if (speed <= 0 || dy == 0)
    return false;

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
    {
      if (right)
        DRVCurve(-1*arc_l, degrees, speed);
      else 
        DRVCurve(-1*arc_l, -1*degrees, speed);
    }
    else
    {
      if (right)
        DRVCurve(arc_l, degrees, speed);
      else 
        DRVCurve(arc_l, -1*degrees, speed);
    }
  }
  else
    DRVStraight(dy, speed);

  return true;
}

bool DRVRemaining(int *dist)
{
  if (!dist)
    return false;

  switch (current_drv_op)
  {
    case DRV_OP_STRAIGHT:
    case DRV_OP_CURVE:
    {
      unsigned long op_time_delta = millis() - eyebot_op_start_time;
      int total_dist = eyebot_op_total_time * abs(eyebot_lin_speed) / 1000;
      *dist = total_dist - (op_time_delta * abs(eyebot_lin_speed) / 1000);
      break;
    }
    case DRV_OP_TURN:
    {
      *dist = 0;
      break;
    }
    default:
    {
      *dist = -1;
      return false;
    }
  }

  return true;
}

bool DRVDone()
{
  if (current_drv_op != DRV_OP_UNDEFINED)
    return false;
  else
    return true;
}

bool DRVWait()
{
  while (current_drv_op != DRV_OP_UNDEFINED);

  return true;
}

/////////////////////////////////////////////
// Position Sensitive Device (PSD) Functions
/////////////////////////////////////////////

struct RawDistancePair
{
  int raw;
  int distance;//mm
};

#define RAW_DISTANCE_PAIR_COUNT 25

// Read distance value in mm from distance sensor
bool PSDGet(int *dist)
{
  if (!dist)
    return false;
  
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

  int val = analogRead(PIN_DIST_SENSOR);

  for (int i = 0; i < RAW_DISTANCE_PAIR_COUNT; i++)
  {
    if (val >= pairs[i].raw)
    {
      *dist = pairs[i].distance;
      return true;
    }
  }

  *dist = 300;//max distance

  return true;
}

// Read raw value from distance sensor
bool PSDGetRaw(int *val)
{
  if (!val)
    return false;
  
  *val = analogRead(PIN_DIST_SENSOR);

  return false;
}

/////////////////
//Old ROBIOS API
/////////////////

int LCDPrintf(const char* format, ...)
{
  if (!format)
    return -1;

  va_list arg_ptr;
  va_start(arg_ptr, format);
  char *str_buf = (char*)lcd_buf;

  vsnprintf(str_buf, sizeof(lcd_buf), format, arg_ptr);

  tft.print(str_buf);

  return 0;
}

int LCDSetPrintf(int row, int column, const char *format, ...)
{
  if (!format)
    return -1;
  
  tft.setCursor(column, row);

  va_list arg_ptr;
  va_start(arg_ptr, format);
  char *str_buf = (char*)lcd_buf;

  vsnprintf(str_buf, sizeof(lcd_buf), format, arg_ptr);

  tft.print(str_buf);

  return 0;
}

int LCDClear()
{
  tft.fillScreen(TFT_BLACK);

  return 0;
}

int LCDSetPos(int row, int column)
{
  tft.setCursor(column, row);

  return 0;
}

int LCDGetPos(int *row, int *column)
{
  if (!row || !column)
    return -1;

  *column = tft.getCursorX();
  *row = tft.getCursorY();

  return 0;
}

int LCDSetColor(COLOR fg, COLOR bg)
{
  rgb565 fg_hue = tft.color24to16(fg);
  rgb565 bg_hue = tft.color24to16(bg);
  tft.setTextColor(fg_hue, bg_hue);
 
  return 0;
}

int LCDSetFont(int font, int variation)
{
  tft.setTextFont(font);

  return 0;
}

int LCDSetFontSize(int fontsize)
{
  tft.setTextSize(fontsize);

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
  rgb565 fg_hue = tft.color24to16(col);
  tft.drawPixel(x, y, col);

  return 0;
}

COLOR LCDGetPixel (int x, int y)
{
  rgb565 col = tft.readPixel(x, y);
  return tft.color16to24(col);
}

int LCDLine(int x1, int y1, int x2, int y2, COLOR col)
{
  rgb565 hue = tft.color24to16(col);

  if (x1 - x2 == 0)
    if (y1 < y2)
      tft.drawFastVLine(x1, y1, y2-y1, hue);
    else
      tft.drawFastVLine(x1, y2, y1-y2, hue);
  else if (y1 - y2 == 0)
    if (x1 < x2)
      tft.drawFastHLine(x1, y1, x2-x1, hue);
    else
      tft.drawFastHLine(x2, y1, x1-x2, hue);
  else
    tft.drawLine(x1, y1, x2, y2, hue);
 
  return 0;
}

int LCDArea(int x1, int y1, int x2, int y2, COLOR col, int fill)
{
  return 0;
}