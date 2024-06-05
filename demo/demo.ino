#include "eyebot.h"

#define NUM_PAGES 2

static rgb img[QQVGA_WIDTH*QQVGA_HEIGHT];
static grayscale gray_img[QQVGA_WIDTH*QQVGA_HEIGHT];
static grayscale edge_img[QQVGA_WIDTH*QQVGA_HEIGHT];

enum DemoPhase {
  HOME_SCREEN,
  COLOUR_NAVIGATION,
  CAMERA_FEED,
  DRIVE_STRAIGHT,
  DRIVE_CURVE,
  DRIVE_TURN,
  DRIVE_GOTO,
  DRIVE_SET,
  DISTANCE_SENSOR
} demoPhase;

enum HomePage {
  HOME_PAGE_1,
  HOME_PAGE_2
};

enum CamImageProcessing {
  NONE_PROC,
  GRAYSCALE_PROC,
  LAPLACE_PROC,
  SOBEL_PROC
};

enum DriveStraightScreen {
  DRV_STRAIGHT_SETTINGS,
  DRV_STRAIGHT_START
};

void setup() {
  Serial.begin(9600);

  if (!EYEBOTInit())
    Serial.printf("Initialisation of Eyebot failed\n");
  
  DRVSetMotorOffsets(0, 0);
  DRVSetMaxLinearSpeed(340);
  DRVSetMaxAngularSpeed(180);

  demoPhase = HOME_SCREEN;
}

void loop() {
  static bool ui_init = false;
  switch (demoPhase)
  {
    case HOME_SCREEN:
    {
      const int DEMO_OPTION_HEIGHT = 70;
      static HomePage homePage;

      if (!ui_init)
      {
        homePage = HOME_PAGE_1;
        LCDDrawRect(0, 0, LCD_WIDTH, LCD_HEIGHT, BLACK);

        LCDSetFontSize(1);

        //Left Button symbol
        LCDDrawCircle(20, 20, 10, WHITE);
        LCDSetFontColor(BLACK, WHITE);
        LCDPrintAt(19, 17, "L");

        //Page Up label
        LCDSetFontColor(WHITE);
        LCDPrintAt(34, 17, "Pg. Up");

        //Right Button symbol
        LCDDrawCircle(84, 20, 10, WHITE);
        LCDSetFontColor(BLACK, WHITE);
        LCDPrintAt(82, 17, "R");

        //Page Down label
        LCDSetFontColor(WHITE);
        LCDPrintAt(98, 17, "Pg. Down");

        //Demo Option 1
        LCDDrawRect(5, 50, LCD_WIDTH - 10, DEMO_OPTION_HEIGHT, WHITE);

        //Demo Option 2
        LCDDrawRect(5, 140, LCD_WIDTH - 10, DEMO_OPTION_HEIGHT, WHITE);

        //Demo Option 3
        LCDDrawRect(5, 230, LCD_WIDTH - 10, DEMO_OPTION_HEIGHT, WHITE);

        ui_init = true;
      }

      switch (homePage)
      {
        case HOME_PAGE_1:
        {
          LCDSetFontSize(2);
          LCDSetFontColor(BLACK, WHITE);

          //Demo Option 1 Text
          LCDPrintAt(20, 75, "CAMERA FEED");

          //Demo Option 2 Text
          LCDPrintAt(15, 168, "DRV STRAIGHT");

          //Page number
          LCDSetFontSize(1);
          LCDSetFontColor(WHITE);
          char pg_str[8];
          snprintf(pg_str, 8, "Pg. 1/%d", NUM_PAGES);
          LCDPrintAt(LCD_WIDTH - 42, LCD_HEIGHT - 10, pg_str);

          int touch_x = -1, touch_y = -1;
          INReadTouch(&touch_x, &touch_y);

          if (touch_x >= 5 && touch_x <= LCD_WIDTH - 10)
          {
            if (touch_y >= 50 && touch_y <= 50 + DEMO_OPTION_HEIGHT)
            {
              demoPhase = CAMERA_FEED;
              ui_init = false;
            }
            else if (touch_y >= 140 && touch_y <= 140 + DEMO_OPTION_HEIGHT)
            {
              demoPhase = DRIVE_STRAIGHT;
              ui_init = false;
            }

            delay(200);
          }
        }
      }

      break;
    }
    case CAMERA_FEED:
    {
      static CamImageProcessing imgproc = NONE_PROC;
      static unsigned long prev_time;

      if (!ui_init)
      {
        LCDDrawRect(0, 0, LCD_WIDTH, LCD_HEIGHT, BLACK);

        //Exit Symbol
        LCDDrawCircle(20, LCD_HEIGHT - 20, 10, WHITE);
        LCDSetFontSize(1);
        LCDSetFontColor(BLACK, WHITE);
        LCDPrintAt(19, LCD_HEIGHT - 23, "L");

        //Exit Symbol label
        LCDSetFontColor(WHITE);
        LCDPrintAt(34, LCD_HEIGHT - 23, "Exit");

        //Cycle Symbol
        LCDDrawCircle(75, LCD_HEIGHT - 20, 10, WHITE);
        LCDSetFontColor(BLACK, WHITE);
        LCDPrintAt(73, LCD_HEIGHT - 23, "R");

        //Cycle Symbol Label
        LCDSetFontColor(WHITE);
        LCDPrintAt(89, LCD_HEIGHT - 23, "Img. Proc.");

        prev_time = millis();
        ui_init = true;
      }

      if (!CAMGetImage(img))
        LCDDrawRect(5, 100, QQVGA_WIDTH, QQVGA_HEIGHT, RED);

      char imgproc_str[16] = {};
      LCDSetFontSize(2);

      switch (imgproc)
      {
        case NONE_PROC:
        {
          snprintf(imgproc_str, 8, "  RAW   ");
          LCDPrintAt(45, 230, imgproc_str);
          LCDDrawImage(5, 100, QQVGA_WIDTH, QQVGA_HEIGHT, img);
          break;
        }
        case GRAYSCALE_PROC:
        {
          snprintf(imgproc_str, 8, "GRAY    ");
          LCDPrintAt(64, 230, imgproc_str);
          IPRGBToGrayscale(QQVGA_WIDTH, QQVGA_HEIGHT, img, gray_img);
          LCDDrawImage(5, 100, QQVGA_WIDTH, QQVGA_HEIGHT, gray_img);
          break;
        }
        case LAPLACE_PROC:
        {
          snprintf(imgproc_str, 8, "LAPLACE  ");
          LCDPrintAt(46, 230, imgproc_str);
          IPRGBToGrayscale(QQVGA_WIDTH, QQVGA_HEIGHT, img, gray_img);
          IPLaplace(QQVGA_WIDTH, QQVGA_HEIGHT, gray_img, edge_img);
          LCDDrawImage(5, 100, QQVGA_WIDTH, QQVGA_HEIGHT, edge_img);
          break;
        }
        case SOBEL_PROC:
        {
          snprintf(imgproc_str, 16, "  SOBEL   ");
          LCDPrintAt(30, 230, imgproc_str);
          IPRGBToGrayscale(QQVGA_WIDTH, QQVGA_HEIGHT, img, gray_img);
          IPSobel(QQVGA_WIDTH, QQVGA_HEIGHT, gray_img, edge_img);
          LCDDrawImage(5, 100, QQVGA_WIDTH, QQVGA_HEIGHT, edge_img);
          break;
        }
        default:
          LCDDrawImage(5, 100, QQVGA_WIDTH, QQVGA_HEIGHT, img);
          break;
      }

      unsigned long current_time = millis();
      unsigned long imgproc_time = current_time - prev_time;
      float fps = 1000 / (float)imgproc_time;

      char fps_str[8] = {};
      snprintf(fps_str, 8, "%.1f fps", fps);
      LCDSetFontSize(2);
      LCDSetFontColor(WHITE);
      LCDPrintAt(80, 5, fps_str);

      bool left_pressed;
      INReadButton(LEFT_BUTTON, &left_pressed);

      if (left_pressed)
      {
        demoPhase = HOME_SCREEN;
        ui_init = false;
        delay(200);
        break;
      }

      bool right_pressed;
      INReadButton(RIGHT_BUTTON, &right_pressed);

      if (right_pressed)
      {
        switch (imgproc)
        {
          case NONE_PROC:
            imgproc = GRAYSCALE_PROC;
            break;
          case GRAYSCALE_PROC:
            imgproc = LAPLACE_PROC;
            break;
          case LAPLACE_PROC:
            imgproc = SOBEL_PROC;
            break;
          case SOBEL_PROC:
          default:
            imgproc = NONE_PROC;
            break;
        }

        delay(200);
      }

      prev_time = current_time;

      break;
    }
    case DRIVE_STRAIGHT:
    {
      static DriveStraightScreen screen = DRV_STRAIGHT_SETTINGS;
      static int max_lin_speed = 340;
      static int speed = 340;
      static int distance = 1000;

      switch (screen)
      {
        case DRV_STRAIGHT_SETTINGS:
        {
          if (!ui_init)
          {
            LCDDrawRect(0, 0, LCD_WIDTH, LCD_HEIGHT, BLACK);

            //Exit Symbol
            LCDDrawCircle(20, LCD_HEIGHT - 20, 10, WHITE);
            LCDSetFontSize(1);
            LCDSetFontColor(BLACK, WHITE);
            LCDPrintAt(19, LCD_HEIGHT - 23, "L");

            //Exit Symbol label
            LCDSetFontColor(WHITE);
            LCDPrintAt(34, LCD_HEIGHT - 23, "Exit");

            //Distance label
            LCDSetFontSize(1);
            LCDSetFontColor(WHITE);
            LCDPrintAt(5, 5, "Distance (mm)");

            //Distance decrease
            LCDDrawRect(5, 20, 40, 40, WHITE);
            LCDDrawRect(15, 38, 20, 5, BLACK);

            //Distance increase
            LCDDrawRect(125, 20, 40, 40, WHITE);
            LCDDrawRect(135, 38, 20, 5, BLACK);
            LCDDrawRect(143, 30, 5, 21, BLACK);

            //Speed label
            LCDPrintAt(5, 68, "Speed (mm/s)");
            
            //Speed decrease
            LCDDrawRect(5, 82, 40, 40, WHITE);
            LCDDrawRect(15, 100, 20, 5, BLACK);

            //Speed increase
            LCDDrawRect(125, 82, 40, 40, WHITE);
            LCDDrawRect(135, 100, 20, 5, BLACK);
            LCDDrawRect(143, 92, 5, 21, BLACK);

            //Max Speed label
            LCDPrintAt(5, 130, "Max linear speed (mm/s)");

            //Max Speed decrease
            LCDDrawRect(5, 144, 40, 40, WHITE);
            LCDDrawRect(15, 162, 20, 5, BLACK);

            //Max Speed increase
            LCDDrawRect(125, 144, 40, 40, WHITE);
            LCDDrawRect(135, 162, 20, 5, BLACK);
            LCDDrawRect(143, 154, 5, 21, BLACK);

            //Reverse speed option
            LCDDrawRect(5, 193, 160, 40, WHITE);
            LCDSetFontSize(2);
            LCDSetFontColor(BLACK, WHITE);
            LCDPrintAt(44, 205, "REVERSE");

            //Start button
            LCDDrawRect(5, 240, 160, 40, WHITE);
            LCDPrintAt(54, 252, "START");

            ui_init = true;
          }

          bool left_pressed;
          INReadButton(LEFT_BUTTON, &left_pressed);

          if (left_pressed)
          {
            demoPhase = HOME_SCREEN;
            ui_init = false;
            delay(200);
            break;
          }

          char val_str[8];
          snprintf(val_str, 8, "%d ", distance);
          LCDSetFontSize(2);
          LCDSetFontColor(WHITE);
          LCDPrintAt(50, 34, val_str);

          snprintf(val_str, 8, "%d ", speed);
          LCDPrintAt(62, 96, val_str);

          snprintf(val_str, 8, "%d ", max_lin_speed);
          LCDPrintAt(62, 160, val_str);

          int touch_x, touch_y;
          INReadTouch(&touch_x, &touch_y);

          if (touch_x >= 5 && touch_x <= 45)
          {
            if (touch_y >= 20 && touch_y <= 60)
              distance -= 100;
            
            if (touch_y >= 82 && touch_y <= 122)
              speed -= 10;
            
            if (touch_y >= 144 && touch_y <= 184)
              max_lin_speed -= 10;

            delay(200);
          }

          if (touch_x >= 125 && touch_x <= 165)
          {
            if (touch_y >= 20 && touch_y <= 60)
              distance += 100;
            
            if (touch_y >= 82 && touch_y <= 122)
              speed += 10;
            
            if (touch_y >= 144 && touch_y <= 184)
              max_lin_speed += 10;

            delay(200);
          }

          if (touch_x >= 5 && touch_x <= 165 && touch_y >= 193 && touch_y <= 233)
          {
            distance *= -1;
            delay(200);
          }

          if (touch_x >= 5 && touch_x <= 165 && touch_y >= 240 && touch_y <= 280)
          {
            screen = DRV_STRAIGHT_START;
            ui_init = false;
            delay(200);
          }

          break;
        }
        case DRV_STRAIGHT_START:
        {
          if (!ui_init)
          {
            LCDDrawRect(0, 0, LCD_WIDTH, LCD_HEIGHT, BLACK);
            LCDDrawRect(5, 5, 160, 315, RED);

            LCDSetFontSize(4);
            LCDSetFontColor(WHITE, RED);
            LCDPrintAt(37, 120, "HALT");

            DRVSetMaxLinearSpeed(max_lin_speed);
            DRVStraight(distance, speed);

            ui_init = true;
          }

          int touch_x, touch_y;
          INReadTouch(&touch_x, &touch_y);

          if (touch_x >= 0 && touch_y >= 0)
          {
            screen = DRV_STRAIGHT_SETTINGS;
            ui_init = false;
            DRVSetSpeed(0, 0);
            delay(200);
          }
        }
        default:
          break;
      }

      break;
    }
    default:
    {
      demoPhase = HOME_SCREEN;
      break;
    }
  }
}
