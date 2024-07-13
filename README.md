# Wiring

| T-Display-S3 GPIO Pin | ESP32-CAM GPIO Pin |
|-----------------------|-----------------------|
| 43 | 14 |
| 44 | 2 |
| 1 | 3 |
| 2 | 1 |

| T-Display-S3 GPIO Pin | Motor Diver Pin |
|-----------------------|-----------------|
| 3 | B-1A |
| 10 | B-1B |
| 12 | A-1B |
| 11 | A-1A |

| T-Display-S3 GPIO Pin | Analog Distance Sensor |
|-----------------------|------------------------|
| 13 | Data Output |

# Installation

To install this library, just place this entire folder as a subfolder in your
Arduino libraries folder for your respective platform.

The TFT_eSPI and TouchLib libraries must also be copied into to the Arduino libraries folder
from the versions found in https://github.com/Xinyuan-LilyGO/T-Display-S3/tree/main/lib .

# Usage

## T-Display-S3

Add...

```#include <eyebot.h>```

... to your Arduino ```.ino``` sketch and set the board type as "ESP32S3 Dev Module",
with the settings under the "Tools" dropdown menu adjusted to:

| Arduino IDE Setting                  | Value                             |
| ------------------------------------ | --------------------------------- |
| Board                                | **ESP32S3 Dev Module**            |
| Port                                 | Your port                         |
| USB CDC On Boot                      | Enable                            |
| CPU Frequency                        | 240MHZ(WiFi)                      |
| Core Debug Level                     | None                              |
| USB DFU On Boot                      | Disable                           |
| Erase All Flash Before Sketch Upload | Disable                           |
| Events Run On                        | Core1                             |
| Flash Mode                           | QIO 80MHZ                         |
| Flash Size                           | **16MB(128Mb)**                   |
| Arduino Runs On                      | Core1                             |
| USB Firmware MSC On Boot             | Disable                           |
| Partition Scheme                     | **16M Flash(3M APP/9.9MB FATFS)** |
| PSRAM                                | **OPI PSRAM**                     |
| Upload Mode                          | **UART0/Hardware CDC**            |
| Upload Speed                         | 921600                            |
| USB Mode                             | **CDC and JTAG**                  |

## ESP32-CAM

Set board-type to "AI-Thinker ESP32-CAM" and upload ```camera.ino```. 

When new code is uploaded to the T-Display-S3 while both the T-Display-S3 and the ESP32-CAM are on-board the EyeBot, the image that the ESP32-CAM returns will most likely be out of sync. Disconnecting the EyeBot from power and then reconnecting it should fix this.