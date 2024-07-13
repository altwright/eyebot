#include <driver/spi_slave.h>
#include <esp_camera.h>
#include <esp_heap_caps.h>

// START Pin definition for CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
// END Pin definition for CAMERA_MODEL_AI_THINKER

#define SPI_MISO_PIN 14
#define SPI_CS_PIN 2
#define SPI_SCLK_PIN 3
#define CAM_SIGNAL_PIN 1

#define QQVGA_WIDTH 160
#define QQVGA_HEIGHT 120
#define QQVGA_RGB565_BUFFER_SIZE QQVGA_WIDTH*QQVGA_HEIGHT*sizeof(uint16_t)

#define MAX_TX_SEGMENT_SIZE 4800
#define NUM_TX_SEGMENTS 8

uint8_t *dma_bytes;

//Called after a transaction is queued and ready for pickup by master.
void my_post_setup_cb(spi_slave_transaction_t *trans)
{
  digitalWrite(CAM_SIGNAL_PIN, LOW);//Signal master to start tx
}

//Called after transaction is sent/received.
void my_post_trans_cb(spi_slave_transaction_t *trans)
{
  digitalWrite(CAM_SIGNAL_PIN, HIGH);//Signal master to finish tx
}

void setup() {
  Serial.end();

  //Configuration for the SPI bus
  spi_bus_config_t buscfg = {
      .mosi_io_num = -1,
      .miso_io_num = SPI_MISO_PIN,
      .sclk_io_num = SPI_SCLK_PIN,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = MAX_TX_SEGMENT_SIZE
  };

  //Configuration for the SPI slave interface
  spi_slave_interface_config_t slvcfg = {
      .spics_io_num = SPI_CS_PIN,
      .flags = 0,
      .queue_size = 1,
      .mode = 0,
      .post_setup_cb = my_post_setup_cb,
      .post_trans_cb = my_post_trans_cb
  };

  //Enable pull-ups on SPI lines so we don't detect rogue pulses when no master is connected.
  pinMode(SPI_SCLK_PIN, INPUT_PULLUP);
  pinMode(SPI_CS_PIN, INPUT_PULLUP);

  pinMode(CAM_SIGNAL_PIN, OUTPUT);
  digitalWrite(CAM_SIGNAL_PIN, HIGH);//TTGO pin will be pullup
  
  //Initialize SPI slave interface
  esp_err_t ret = spi_slave_initialize(HSPI_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);
  assert(ret == ESP_OK);

  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565; //YUV422,GRAYSCALE,RGB565,JPEG
  config.frame_size = FRAMESIZE_QQVGA;
  config.fb_count = 3;
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 0;

  ret = esp_camera_init(&config);
  if (ret != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", ret);
    return;
  }

  sensor_t *sensor = esp_camera_sensor_get();
  sensor->set_brightness(sensor, 0);     // -2 to 2
  sensor->set_contrast(sensor, 0);       // -2 to 2
  sensor->set_saturation(sensor, 1);     // -2 to 2
  sensor->set_hmirror(sensor, 0);        // 0 = disable , 1 = enable
  sensor->set_vflip(sensor, 0);          // 0 = disable , 1 = enable
  sensor->set_colorbar(sensor, 0);       // 0 = disable , 1 = enable

  dma_bytes = (uint8_t*)heap_caps_aligned_alloc(sizeof(uint32_t), QQVGA_RGB565_BUFFER_SIZE, MALLOC_CAP_DMA);
  if(!dma_bytes)
    Serial.printf("Failed to allocate DMA capable memory\n");
}

void loop() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb)
    return;

  uint8_t *img_bytes = fb->buf;

  uint16_t *img = (uint16_t*)img_bytes;

  for (int y = 0; y < QQVGA_HEIGHT; y++)
  {
    for (int x = 0; x < QQVGA_WIDTH; x++)
    {
      uint16_t *pixel = img + y*QQVGA_WIDTH + x;
      uint16_t lower = (*pixel & 0xFF00) >> 8;
      *pixel <<= 8;
      *pixel |= lower;
    }
  }

  memcpy(dma_bytes, img_bytes, QQVGA_RGB565_BUFFER_SIZE);
  
  for (int i = 0; i < NUM_TX_SEGMENTS; i++)
  {
    spi_slave_transaction_t t = {};
    t.length = MAX_TX_SEGMENT_SIZE * 8;
    t.tx_buffer = dma_bytes + i*MAX_TX_SEGMENT_SIZE;
    t.rx_buffer = NULL;

    esp_err_t err = spi_slave_transmit(HSPI_HOST, &t, portMAX_DELAY);
    assert(err == ESP_OK);
  }

  esp_camera_fb_return(fb);
}
