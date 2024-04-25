#include "eyebot.h"
#include <Arduino.h>
#include <math.h>
#include <driver/spi_master.h>
#include <TFT_eSPI.h>

//Camera Pins
#define PIN_SPI_MOSI 43
#define PIN_SPI_MISO 44
#define PIN_SPI_CS 18
#define PIN_SPI_SCLK 17
#define PIN_CAM_SIGNAL 21

#define PIN_LEFT_BUTTON 0
#define PIN_RIGHT_BUTTON 14

#define CAM_NUM_IMAGE_SEGMENTS 8
#define QQVGA_RGB565_BUFFER_SIZE QQVGA_SIZE*2

////////////////
//Core Functions
////////////////

static TFT_eSPI tft = TFT_eSPI(LCD_WIDTH, LCD_HEIGHT);
static spi_device_handle_t cam_spi_handle;

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
    .mosi_io_num = PIN_SPI_MOSI,
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
  esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
  assert(ret == ESP_OK);
  ret = spi_bus_add_device(SPI2_HOST, &devcfg, &cam_spi_handle);
  assert(ret == ESP_OK);
  
  ///////////////////////////////
  //Set default button callbacks
  ///////////////////////////////

  attachInterrupt(digitalPinToInterrupt(PIN_LEFT_BUTTON), default_left_button_cb, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_RIGHT_BUTTON), default_right_button_cb, CHANGE);

  return true;
}

///////////////////
//Camera Functions
///////////////////

typedef uint16_t rgb565;

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
  static rgb565 cambuf[QQVGA_SIZE];

  if (!CAMGetImage(cambuf))
  {
    Serial.printf("Failed to get image from camera\n");
    return false;
  }

  //Copy from rgb565 buffer to rgb888 user buffer
  for (int row = 0; row < QQVGA_HEIGHT; row++)
  {
    for (int col = 0; col < QQVGA_WIDTH; col++)
    {
      int i = row*QQVGA_WIDTH + col;

      IP565To888(cambuf[i], &imgbuf[i]);
    }
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
  *r = (hue >> 16) & 0xFF;
  *g = (hue >> 8) & 0xFF;
  *b = (hue) & 0xFF;

  return true;
}

bool IPSetRGB(byte r, byte g, byte b, rgb *hue)
{
  rgb565 col = tft.color565(r, g, b);
  IP565To888(col, hue);

  return true;
}

bool IPRGBToGrayscale(rgb hue, grayscale *gray)
{
  byte r, g, b;
  IPGetRGB(hue, &r, &g, &b);

  *gray = (r + g + b)/3;

  return true;
}

bool IPRGBToGrayscale(rgb col, rgb *gray)
{
  grayscale val = 0;
  IPRGBToGrayscale(col, &val);

  IPGrayscaleToRGB(val, gray);

  return true;
}

bool IPRGBToGrayscale(int img_width, int img_height, const rgb col_img[], grayscale gray_img[])
{
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
  IPSetRGB(gray, gray, gray, col);

  return true;
}

bool IPGrayscaleToRGB(int img_width, int img_height, const grayscale gray_img[], rgb col_img[])
{
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

static rgb565 lcd_buf[LCD_HEIGHT*LCD_WIDTH];

bool LCDDrawImage(int xpos, int ypos, int img_width, int img_height, const rgb img[])
{
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
  tft.print(str);

  return true;
}

bool LCDPrintln(const char *str)
{
  tft.println(str);

  return true;
}

bool LCDPrintAt(int xpos, int ypos, const char *str)
{
  tft.drawString(str, xpos, ypos);

  return true;
}

bool LCDGetSize(int *lcd_width, int *lcd_height)
{
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
