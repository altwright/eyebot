#include "eyebot.h"

#include <Arduino.h>
#include <math.h>
#include <driver/spi_master.h>
#include <driver/timer.h>
#include <TFT_eSPI.h>
using namespace fs;
#include <JPEGDEC.h>

#define PIN_BATTERY_POWER 15

//Camera Pins
#define PIN_SPI_MOSI 43
#define PIN_SPI_MISO 44
#define PIN_SPI_CS 18
#define PIN_SPI_SCLK 17
#define PIN_CAM_SIGNAL 21

//Button Pins
#define PIN_LEFT_BUTTON 0
#define PIN_RIGHT_BUTTON 14

//Motor Pins
#define PIN_LEFT_MOTOR_FORWARD 1
#define PIN_LEFT_MOTOR_BACKWARD 2
#define PIN_RIGHT_MOTOR_FORWARD 10
#define PIN_RIGHT_MOTOR_BACKWARD 3

//PSD Pin
#define PIN_DIST_SENSOR 16

#define CAM_NUM_IMAGE_SEGMENTS 8
#define QQVGA_RGB565_BUFFER_SIZE QQVGA_SIZE*2

#define MAX_RX_SEGMENT_SIZE 4800

typedef uint32_t u32;
typedef uint64_t u64;
typedef uint16_t rgb565;

////////////////
//Core Functions
////////////////

static TFT_eSPI tft = TFT_eSPI(LCD_WIDTH, LCD_HEIGHT);
static spi_device_handle_t cam_spi_handle;

static JPEGDEC jpeg = {};

static rgb565 lcd_buf[LCD_HEIGHT*LCD_WIDTH];

enum drive_op
{
  DRV_OP_STRAIGHT,
  DRV_OP_TURN,
  DRV_OP_CURVE,
  DRV_OP_NONE
};

static volatile drive_op current_drv_op = DRV_OP_NONE;

static timer_group_t timer_group = TIMER_GROUP_0;
static timer_idx_t motor_timer_idx = TIMER_0;

static bool motor_timer_kill_cb(void *arg)
{
  DRVSetSpeed(0, 0);

  current_drv_op = DRV_OP_NONE;

  return true;
}

static button_callback left_button_cb = NULL;
static button_callback right_button_cb = NULL;

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

  err = timer_isr_callback_add(timer_group, motor_timer_idx, motor_timer_kill_cb, NULL, 0);
  assert(err == ESP_OK);

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

static bool SwapEndianess(uint32_t *dword)
{
  uint32_t lower_word = *dword >> 16;
  *dword <<= 16;
  *dword |= lower_word;

  uint16_t *words = (uint16_t*)dword;
  for (int i = 0; i < 2; i++)
  {
    uint16_t lower_byte = words[i] >> 8;
    words[i] <<= 8;
    words[i] |= lower_byte;
  }

  return true;
}

static int jpeg_decode_cb(JPEGDRAW *drawInfo)
{
  rgb *imgbuf = (rgb*)drawInfo->pUser;

  int ystart = drawInfo->y;
  int xstart = drawInfo->x;
  int height = drawInfo->iHeight;
  int width = drawInfo->iWidth;
  int clipped_width = drawInfo->iWidthUsed;
  rgb565 *pixels = drawInfo->pPixels;

  for (int y = 0; y < height; y++)
  {
    for (int x = 0; x < width; x++)
    {
      IP565To888(pixels[y*clipped_width + x], &imgbuf[(ystart + y)*QQVGA_WIDTH + (xstart + x)]);
    }
  }

  return 1;
}

static bool CAMGetImage(rgb565 imgbuf[])
{
  uint32_t command = 1;
  //Send command byte zero to get image
  spi_transaction_t t = {};
  t.length = sizeof(uint32_t)*8;
  t.tx_buffer = &command;
  t.rx_buffer = NULL;

  while (digitalRead(PIN_CAM_SIGNAL));

  esp_err_t err = spi_device_transmit(cam_spi_handle, &t);
  if (err != ESP_OK)
    return false;

  byte *buffer = (byte*)imgbuf;

  //Get image segments
  for (int i = 0; i < CAM_NUM_IMAGE_SEGMENTS; i++)
  {
    t = {};
    t.length = QQVGA_RGB565_BUFFER_SIZE/CAM_NUM_IMAGE_SEGMENTS*8;//bit length
    t.tx_buffer = NULL;
    t.rx_buffer = buffer + i*(QQVGA_RGB565_BUFFER_SIZE/CAM_NUM_IMAGE_SEGMENTS);

    while (digitalRead(PIN_CAM_SIGNAL));

    spi_device_transmit(cam_spi_handle, &t);
  }

  for (int i = 0; i < QQVGA_SIZE; i++)
  {
    IPSwapEndianess(&imgbuf[i]);
  }

  return true;
}

bool CAMGetImage(rgb imgbuf[])
{
  if (!imgbuf)
    return false;

  uint32_t jpeg_byte_count = 0;

  spi_transaction_t t = {};
  t.length = sizeof(uint32_t)*8;
  t.tx_buffer = NULL;
  t.rx_buffer = &jpeg_byte_count;

  while (digitalRead(PIN_CAM_SIGNAL));

  esp_err_t err = spi_device_transmit(cam_spi_handle, &t);
  assert(err == ESP_OK);

  if (jpeg_byte_count > QQVGA_RGB565_BUFFER_SIZE)
    return false;//A misread has happened

  int rx_segment_count = jpeg_byte_count / MAX_RX_SEGMENT_SIZE;
  int rx_remainder_byte_count = jpeg_byte_count % MAX_RX_SEGMENT_SIZE;

  byte *jpeg_buffer = (byte*)lcd_buf;

  for (int i = 0; i < rx_segment_count; i++)
  {
    t = {};
    t.length = MAX_RX_SEGMENT_SIZE * 8;
    t.tx_buffer = NULL;
    t.rx_buffer = jpeg_buffer + i * MAX_RX_SEGMENT_SIZE;

    while (digitalRead(PIN_CAM_SIGNAL));

    err = spi_device_transmit(cam_spi_handle, &t);
    assert(err == ESP_OK);
  }

  if (rx_remainder_byte_count != 0)
  {
    //int dword_remainder_bytes = rx_remainder_byte_count % sizeof(uint32_t);
    //rx_remainder_byte_count += sizeof(uint32_t) - dword_remainder_bytes;

    t = {};
    t.length = rx_remainder_byte_count * 8;
    t.tx_buffer = NULL;
    t.rx_buffer = jpeg_buffer + rx_segment_count * MAX_RX_SEGMENT_SIZE;

    while (digitalRead(PIN_CAM_SIGNAL));

    err = spi_device_transmit(cam_spi_handle, &t);
    assert(err == ESP_OK);
  }

  //For some reason the first byte is 7F instead of the FF required
  jpeg_buffer[0] = 0xFF;

  Serial.printf("%x %x\n", jpeg_buffer[jpeg_byte_count - 2], jpeg_buffer[jpeg_byte_count - 1]);

  if (!jpeg.openRAM(jpeg_buffer, jpeg_byte_count, jpeg_decode_cb))
  {
    Serial.printf("Failed to read header info of JPEG\n");
    return false;
  }

  jpeg.setUserPointer((void*)imgbuf);

  if (!jpeg.decode(0, 0, 0))
  {
    Serial.printf("Failed to decode JPEG\n");
    return false;
  }

  return true;
}

bool CAMChangeSettings(camera_settings settings)
{
  uint32_t command = 2;
  //Send command word 1 to change settings
  spi_transaction_t t = {};
  t.length = sizeof(uint32_t)*8;
  t.tx_buffer = &command;
  t.rx_buffer = NULL;

  while (digitalRead(PIN_CAM_SIGNAL));

  esp_err_t err = spi_device_transmit(cam_spi_handle, &t);
  if (err != ESP_OK)
    return false;
  
  t = {};
  t.length = sizeof(camera_settings)*8;
  t.tx_buffer = &settings;
  t.rx_buffer = NULL;

  while (digitalRead(PIN_CAM_SIGNAL));

  err = spi_device_transmit(cam_spi_handle, &t);
  if (err != ESP_OK)
    return false;
  
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

bool LCDClear()
{
  tft.fillScreen(TFT_BLACK);

  return true;
}

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

bool LCDSetFontSize(int size)
{
  if (size <= 0 || size > 0xFF)
    return false;

  tft.setTextSize(size);

  return true;
}

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

bool LCDGetSize(int *lcd_width, int *lcd_height)
{
  if (!lcd_width || !lcd_height)
    return false;

  *lcd_width = LCD_WIDTH;
  *lcd_height = LCD_HEIGHT;

  return true;
}

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

bool INSetButtonCallback(button b, button_callback cb)
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

//////////////////////////
//Motor-Driving Functions
//////////////////////////

static int left_motor_offset = 0;
static int right_motor_offset = 0;
static int left_motor_pwm = 0;
static int right_motor_pwm = 0;

static unsigned long set_speed_start_time = 0;

// mm/s
static int max_linear_speed = 340;
// degrees/s
static int max_angular_speed = 180;

static volatile int eyebot_xpos = 0, eyebot_ypos = 0, eyebot_angle = 0;
static volatile int eyebot_lin_speed = 0;
static volatile int eyebot_ang_speed = 0;
static volatile unsigned long eyebot_op_total_time = 0;
static volatile unsigned long eyebot_op_start_time = 0;

bool DRVSetMotorOffsets(int left_offset, int right_offset)
{
  left_motor_offset = left_offset < -255 || left_offset > 255 ? 0 : left_offset;
  right_motor_offset = right_offset < -255 || right_offset > 255 ? 0 : right_offset;

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
    case DRV_OP_NONE:
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

    left_motor_pwm = motor_pwm + left_motor_offset;
    right_motor_pwm = motor_pwm + right_motor_offset;

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
    ang_speed_percentage /= 2;

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
  eyebot_op_total_time = -1;//Could be infinite

  return true;
}     

static bool SetMotorKillTimer(u64 time_ms)
{
  esp_err_t err;
  if (err = timer_pause(timer_group, motor_timer_idx))
    return false;

  //Assumes 2000 increments per s
  if (err = timer_set_counter_value(timer_group, motor_timer_idx, 0))
    return false;

  if (err = timer_set_alarm_value(timer_group, motor_timer_idx, 2*(u64)time_ms))
    return false;

  if (err = timer_set_alarm(timer_group, motor_timer_idx, TIMER_ALARM_EN))
    return false;

  if (err = timer_start(timer_group, motor_timer_idx))
    return false;

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
    return false;
  
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
  
  float time_taken = (angle / (float)speed) * 1000;

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
  if (current_drv_op != DRV_OP_NONE)
    return false;
  else
    return true;
}

bool DRVWait()
{
  while (current_drv_op != DRV_OP_NONE);

  return true;
}

/////////////////////////////////////////////
// Position Sensitive Device (PSD) Functions
/////////////////////////////////////////////

// Read distance value in mm from distance sensor
bool PSDGet(int *dist)
{
  if (!dist)
    return false;
  
  //Max val is 4095
  int val = analogRead(PIN_DIST_SENSOR);

  if (val >= 2730)// 20-60 mm
  {
    int delta = val - 2730;
    int val_range = 4095 - 2730;

    float ratio = delta / (float)val_range;
    
    int dist_range = 60 - 20;

    *dist = 60 - ratio * dist_range;
  }
  else if (val >= 1638)//60-100 mm
  {
    int delta = val - 1638;
    int val_range = 2729 - 1638;

    float ratio = delta / (float)val_range;
    
    int dist_range = 100 - 60;

    *dist = 100 - ratio * dist_range;
  }
  else if (val >= 1092)//100-160 mm
  {
    int delta = val - 1092;
    int val_range = 1637 - 1092;

    float ratio = delta / (float)val_range;
    
    int dist_range = 160 - 100;

    *dist = 160 - ratio * dist_range;
  }
  else if (val >= 682)//160-260 mm
  {
    int delta = val - 682;
    int val_range = 1091 - 682;

    float ratio = delta / (float)val_range;
    
    int dist_range = 260 - 160;

    *dist = 260 - ratio * dist_range;
  }
  else if (val >= 409)//260-400 mm
  {
    int delta = val - 409;
    int val_range = 681 - 409;

    float ratio = delta / (float)val_range;
    
    int dist_range = 400 - 260;

    *dist = 400 - ratio * dist_range;
  }
  else
    *dist = -1;

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