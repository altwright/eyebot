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

The TFT_eSPI library must also be copied into to the Arduino libraries folder
from https://github.com/Bodmer/TFT_eSPI .

# Usage

Add...

```#include <eyebot.h>```

... to your Arduino .ino sketch and set the board type as "ESP32S3 Dev Module",
with the settings under the "Tools" dropdown menu adjusted according to
https://github.com/Xinyuan-LilyGO/T-Display-S3?tab=readme-ov-file#4%EF%B8%8F%E2%83%A3--arduino-ide-manual-installation .
