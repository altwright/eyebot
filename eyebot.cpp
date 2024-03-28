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
  esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
  assert(ret == ESP_OK);
  ret = spi_bus_add_device(SPI2_HOST, &devcfg, &cam_spi_handle);
  assert(ret == ESP_OK);
  
  return true;
}

///////////////////
//Camera Functions
///////////////////

bool CAMGetImage(rgb565 imgbuf[])
{
  uint32_t command = 0;
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
      rgb565 pix = cam_buffer[i];
      rgb888 hue = {
        .red = (pix & 0xF800) >> 8,
        .green = (pix & 0x07E0) >> 3,
        .blue = (pix & 0x001F) << 3,
        .alpha = 0xFF 
      };

      imgbuf[i] = hue;
    }
  }

  return true;
}

bool CAMChangeSettings(camera_settings settings)
{
  uint32_t command = 1;
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

bool IPColorToGray(int img_width, int img_height, rgb888 col_img[], gray gray_img[])
{
  return true;
}

////////////////
//LCD Functions
////////////////

static rgb565 lcd_buf[LCD_HEIGHT*LCD_WIDTH];

bool LCDPushColorImage(int xpos, int ypos, int img_width, int img_height, rgb565 img[])
{
  if (img_width < 0 || img_height < 0)
    return false;

  xpos = xpos < 0 || xpos > LCD_WIDTH ? 0 : xpos;
  ypos = ypos < 0 || ypos > LCD_HEIGHT ? 0 : ypos;
  int xlen = img_width > LCD_WIDTH - xpos ? LCD_WIDTH - xpos : img_width;
  int ylen = img_height > LCD_HEIGHT - ypos ? LCD_HEIGHT - ypos : img_height;

  rgb565 *lcd_start_pos = lcd_buf + ypos*LCD_WIDTH + xpos;

  for (int y = 0; y < ylen; y++)
  {
    for (int x = 0; x < xlen; x++)
    {
      rgb565 *lcd_pos = lcd_start_pos + y*LCD_WIDTH + x;
      *lcd_pos = img[y*img_width + x];
    }
  }

  return true;
}

bool LCDPushColorImage(int xpos, int ypos, int img_width, int img_height, rgb888 img[])
{
  if (img_width < 0 || img_height < 0)
    return false;

  xpos = xpos < 0 || xpos > LCD_WIDTH ? 0 : xpos;
  ypos = ypos < 0 || ypos > LCD_HEIGHT ? 0 : ypos;
  int xlen = img_width > LCD_WIDTH - xpos ? LCD_WIDTH - xpos : img_width;
  int ylen = img_height > LCD_HEIGHT - ypos ? LCD_HEIGHT - ypos : img_height;

  rgb565 *lcd_start_pos = lcd_buf + ypos*LCD_WIDTH + xpos;

  for (int y = 0; y < ylen; y++)
  {
    for (int x = 0; x < xlen; x++)
    {
      rgb565 *lcd_pos = lcd_start_pos + y*LCD_WIDTH + x;
      rgb888 hue = img[y*img_width + x];

      uint16_t red = hue.red;
      uint16_t green = hue.green;
      uint16_t blue = hue.blue;

      *lcd_pos = (red << 8) | (green << 3) | (blue >> 3);
    }
  }

  return true;
}

bool LCDPushGrayImage(int xpos, int ypos, int img_width, int img_height, gray img[])
{
  return true;
}

bool LCDRefresh()
{
  tft.pushImage(0, 0, LCD_WIDTH, LCD_HEIGHT, lcd_buf);
  return true;
}