#include "eyebot.h"
#include <Arduino.h>
#include <math.h>

#define SPI_MOSI_PIN 43
#define SPI_MISO_PIN 44
#define SPI_CS_PIN 18
#define SPI_SCLK_PIN 17
#define CAM_SIGNAL_PIN 21

////////////////
//Core Functions
////////////////

#include <driver/spi_master.h>
#include <TFT_eSPI.h>

#define CAM_NUM_IMAGE_SEGMENTS 8

static TFT_eSPI tft = TFT_eSPI(LCD_WIDTH, LCD_HEIGHT);
static spi_device_handle_t cam_spi_handle;

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
    .mosi_io_num = SPI_MOSI_PIN,
    .miso_io_num = SPI_MISO_PIN,
    .sclk_io_num = SPI_SCLK_PIN,
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
    .spics_io_num = SPI_CS_PIN,
    .queue_size = 1
  };

  pinMode(SPI_SCLK_PIN, OUTPUT);
  pinMode(SPI_CS_PIN, OUTPUT);
  pinMode(CAM_SIGNAL_PIN, INPUT_PULLUP);

  //Initialize the SPI bus and add the ESP32-Camera as a device
  esp_err_t ret = spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);
  assert(ret == ESP_OK);
  ret = spi_bus_add_device(SPI3_HOST, &devcfg, &cam_spi_handle);
  assert(ret == ESP_OK);
  
  return true;
}

///////////////////
//Camera Functions
///////////////////

bool CAMGetImage(rgb565 imgbuf[])
{
  uint32_t command = 1;
  //Send command byte zero to get image
  spi_transaction_t t = {};
  t.length = sizeof(uint32_t)*8;
  t.tx_buffer = &command;
  t.rx_buffer = NULL;

  while (digitalRead(CAM_SIGNAL_PIN));

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

    while (digitalRead(CAM_SIGNAL_PIN));

    err = spi_device_transmit(cam_spi_handle, &t);
  }

  return true;
}

bool CAMGetImage(rgb888 imgbuf[])
{
  static rgb565 cam_buffer[QQVGA_SIZE];

  if (!CAMGetImage(cam_buffer))
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

      IP565To888(cam_buffer[i], &imgbuf[i]);
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

  while (digitalRead(CAM_SIGNAL_PIN));

  esp_err_t err = spi_device_transmit(cam_spi_handle, &t);
  if (err != ESP_OK)
    return false;
  
  t = {};
  t.length = sizeof(camera_settings)*8;
  t.tx_buffer = &settings;
  t.rx_buffer = NULL;

  while (digitalRead(CAM_SIGNAL_PIN));

  err = spi_device_transmit(cam_spi_handle, &t);
  if (err != ESP_OK)
    return false;
  
  return true;
}

/////////////////////////////
//Image Processing Functions
/////////////////////////////

bool IP888To565(rgb888 in_hue, rgb565 *out_hue)
{
  uint16_t red = in_hue.red;
  red <<= 8;
  uint16_t green = in_hue.green;
  green <<= 3;
  uint16_t blue = in_hue.blue;
  blue >>= 3;

  *out_hue = (red & 0xF800) | (green & 0x07E0) | blue;

  return true;
}

bool IP565To888(rgb565 in_hue, rgb888 *out_hue)
{
  out_hue->red = (in_hue & 0xF800) >> 8;
  out_hue->green = (in_hue & 0x07E0) >> 3;
  out_hue->blue = (in_hue & 0x001F) << 3;
  out_hue->alpha = 0xFF;

  return true;
}

bool IPColorToGray(int img_width, int img_height, rgb888 col_img[], gray gray_img[])
{
  return true;
}

////////////////
//LCD Functions
////////////////

const rgb888 RED = {0xFF, 0, 0, 0xFF};
const rgb888 GREEN = {0, 0xFF, 0, 0xFF};
const rgb888 BLUE = {0, 0, 0xFF, 0xFF};
const rgb888 BLACK = {0, 0, 0, 0xFF};
const rgb888 WHITE = {0xFF, 0xFF, 0xFF, 0xFF};
const rgb888 YELLOW = {0xFF, 0xFF, 0, 0xFF};
const rgb888 MAGENTA = {0xFF, 0, 0xFF, 0xFF};
const rgb888 CYAN = {0, 0xFF, 0xFF, 0xFF};

static rgb565 lcd_buf[LCD_HEIGHT*LCD_WIDTH];

bool LCDPushColorImage(int xpos, int ypos, int img_width, int img_height, rgb565 img[])
{
  if (img_width < 0 || img_height < 0)
    return false;

  tft.pushImage(xpos, ypos, img_width, img_height, img);

  return true;
}

bool LCDPushColorImage(int xpos, int ypos, int img_width, int img_height, rgb888 img[])
{
  if (img_width < 0 || img_height < 0 || (img_width*img_height) > (LCD_WIDTH*LCD_HEIGHT))
    return false;

  for (int y = 0; y < img_height; y++)
  {
    for (int x = 0; x < img_width; x++)
    {
      int i = y*img_width + x;

      IP888To565(img[i], &lcd_buf[i]);
    }
  }

  return LCDPushColorImage(xpos, ypos, img_width, img_height, lcd_buf);
}

bool LCDPushGrayImage(int xpos, int ypos, int img_width, int img_height, gray img[])
{
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

bool LCDSetFontColor(rgb888 fg, rgb888 bg)
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

bool LCDSetPixel(int xpos, int ypos, rgb888 hue)
{
  rgb565 col = 0;
  IP888To565(hue, &col);

  tft.drawPixel(xpos, ypos, col);

  return true;
}

bool LCDGetPixel(int xpos, int ypos, rgb888 *hue)
{
  rgb565 col = tft.readPixel(xpos, ypos);
  IP565To888(col, hue);

  return true;
}

bool LCDDrawLine(int xs, int ys, int xe, int ye, rgb888 hue)
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

bool LCDDrawRect(int x, int y, int w, int h, rgb888 hue, bool fill)
{
  rgb565 col = 0;
  IP888To565(hue, &col);

  if (fill)
    tft.fillRect(x, y, w, h, col);
  else
    tft.drawRect(x, y, w, h, col);

  return true;
}

bool LCDDrawCircle(int x, int y, int radius, rgb888 hue, bool fill);
