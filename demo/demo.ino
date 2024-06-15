#include "eyebot.h"

#define NUM_PAGES 3

static rgb img[QQVGA_WIDTH*QQVGA_HEIGHT];
static grayscale gray_img[QQVGA_WIDTH*QQVGA_HEIGHT];
static grayscale edge_img[QQVGA_WIDTH*QQVGA_HEIGHT];

static int max_lin_speed = 340;
static int lin_speed = 340;
static int distance = 1000;
static int ang_speed = 180;
static int max_ang_speed = 180;
static int angle = 0;
static int left_motor_offset = 0;
static int right_motor_offset = 0;

enum DemoPhase {
  HOME_SCREEN,
  COLOUR_NAVIGATION,
  CAMERA_FEED,
  DRIVE_STRAIGHT,
  DRIVE_CURVE,
  DRIVE_TURN,
  DRIVE_GOTO,
  DRIVE_SET,
  DISTANCE_SENSOR,
  HIT_AND_RETURN,
  COLLECT_OBJECT
} demoPhase;

enum HomePage {
  HOME_PAGE_1,
  HOME_PAGE_2,
  HOME_PAGE_3
};

enum CamImageProcessing {
  NONE_PROC,
  GRAYSCALE_PROC,
  LAPLACE_PROC,
  SOBEL_PROC
};

enum DriveScreen {
  DRV_SCREEN_SETTINGS_1,
  DRV_SCREEN_SETTINGS_2,
  DRV_SCREEN_START
};

void setup() {
  Serial.begin(9600);

  if (!EYEBOTInit())
    Serial.printf("Initialisation of Eyebot failed\n");
  
  DRVSetMotorOffsets(0, 0);
  DRVSetMaxLinearSpeed(max_lin_speed);
  DRVSetMaxAngularSpeed(max_ang_speed);

  demoPhase = HOME_SCREEN;
}

void loop() {
  static bool ui_init = false;
  switch (demoPhase)
  {
    case HOME_SCREEN:
    {
      const int DEMO_OPTION_HEIGHT = 70;
      static HomePage homePage = HOME_PAGE_1;

      if (!ui_init)
      {
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

          //Demo Optin 3 Text
          LCDPrintAt(33, 259, "DRV CURVE");

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
            else if (touch_y >= 230 && touch_y <= 230 + DEMO_OPTION_HEIGHT)
            {
              demoPhase = DRIVE_CURVE;
              ui_init = false;
            }

            delay(200);
          }

          bool right_pressed;
          INReadButton(RIGHT_BUTTON, &right_pressed);

          if (right_pressed)
          {
            homePage = HOME_PAGE_2;
            ui_init = false;
            delay(200);
          }

          break;
        }
        case HOME_PAGE_2:
        {
          LCDSetFontSize(2);
          LCDSetFontColor(BLACK, WHITE);

          //Demo Option 1 Text
          LCDPrintAt(40, 75, "DRV TURN");

          //Demo Option 2 Text
          LCDPrintAt(38, 168, "DRV GOTO");

          //Demo Option 3 Text
          LCDPrintAt(33, 259, "SET SPEED");

          //Page number
          LCDSetFontSize(1);
          LCDSetFontColor(WHITE);
          char pg_str[8];
          snprintf(pg_str, 8, "Pg. 2/%d", NUM_PAGES);
          LCDPrintAt(LCD_WIDTH - 42, LCD_HEIGHT - 10, pg_str);

          bool left_pressed;
          INReadButton(LEFT_BUTTON, &left_pressed);

          if (left_pressed)
          {
            homePage = HOME_PAGE_1;
            ui_init = false;
            delay(200);
            break;
          }

          bool right_pressed;
          INReadButton(RIGHT_BUTTON, &right_pressed);

          if (right_pressed)
          {
            homePage = HOME_PAGE_3;
            ui_init = false;
            delay(200);
          }

          int touch_x = -1, touch_y = -1;
          INReadTouch(&touch_x, &touch_y);

          if (touch_x >= 5 && touch_x <= LCD_WIDTH - 10)
          {
            if (touch_y >= 50 && touch_y <= 50 + DEMO_OPTION_HEIGHT)
            {
              demoPhase = DRIVE_TURN;
              ui_init = false;
            }
            else if (touch_y >= 140 && touch_y <= 140 + DEMO_OPTION_HEIGHT)
            {
              demoPhase = DRIVE_GOTO;
              ui_init = false;
            }
            else if (touch_y >= 230 && touch_y <= 230 + DEMO_OPTION_HEIGHT)
            {
              demoPhase = DRIVE_SET;
              ui_init = false;
            }

            delay(200);
          }

          break;
        }
        case HOME_PAGE_3:
        {
          LCDSetFontSize(2);
          LCDSetFontColor(BLACK, WHITE);

          //Demo Option 1 Text
          LCDPrintAt(40, 77, "DISTANCE");

          //Demo Option 2 Text
          LCDPrintAt(15, 168, "");

          //Demo Optin 3 Text
          LCDPrintAt(33, 259, "");

          //Page number
          LCDSetFontSize(1);
          LCDSetFontColor(WHITE);
          char pg_str[8];
          snprintf(pg_str, 8, "Pg. 3/%d", NUM_PAGES);
          LCDPrintAt(LCD_WIDTH - 42, LCD_HEIGHT - 10, pg_str);

          int touch_x = -1, touch_y = -1;
          INReadTouch(&touch_x, &touch_y);

          if (touch_x >= 5 && touch_x <= LCD_WIDTH - 10)
          {
            if (touch_y >= 50 && touch_y <= 50 + DEMO_OPTION_HEIGHT)
            {
              demoPhase = DISTANCE_SENSOR;
              ui_init = false;
            }
            else if (touch_y >= 140 && touch_y <= 140 + DEMO_OPTION_HEIGHT)
            {
              //demoPhase = DRIVE_STRAIGHT;
              ui_init = false;
            }
            else if (touch_y >= 230 && touch_y <= 230 + DEMO_OPTION_HEIGHT)
            {
              //demoPhase = DRIVE_CURVE;
              ui_init = false;
            }

            delay(200);
          }

          bool left_pressed;
          INReadButton(LEFT_BUTTON, &left_pressed);
          if (left_pressed)
          {
            homePage = HOME_PAGE_2;
            ui_init = false;
            delay(200);
          }

          break;
        }
        default:
          homePage = HOME_PAGE_1;
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
      static DriveScreen screen = DRV_SCREEN_SETTINGS_1;

      switch (screen)
      {
        case DRV_SCREEN_SETTINGS_1:
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

            //Extra Settings Page symbol
            LCDDrawCircle(75, LCD_HEIGHT - 20, 10, WHITE);
            LCDSetFontSize(1);
            LCDSetFontColor(BLACK, WHITE);
            LCDPrintAt(73, LCD_HEIGHT - 23, "R");

            //Exit Symbol label
            LCDSetFontColor(WHITE);
            LCDPrintAt(89, LCD_HEIGHT - 23, "Extras");

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

          bool right_pressed;
          INReadButton(RIGHT_BUTTON, &right_pressed);

          if (right_pressed)
          {
            screen = DRV_SCREEN_SETTINGS_2;
            ui_init = false;
            delay(200);
            break;
          }

          char val_str[8];
          snprintf(val_str, 8, "%d ", distance);
          LCDSetFontSize(2);
          LCDSetFontColor(WHITE);
          LCDPrintAt(50, 34, val_str);

          snprintf(val_str, 8, "%d ", lin_speed);
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
              lin_speed -= 10;
            
            if (touch_y >= 144 && touch_y <= 184)
              max_lin_speed -= 10;
            
            if (lin_speed < 0)
              lin_speed = 0;
            
            if (max_lin_speed < 0)
              max_lin_speed = 0;

            delay(200);
          }

          if (touch_x >= 125 && touch_x <= 165)
          {
            if (touch_y >= 20 && touch_y <= 60)
              distance += 100;
            
            if (touch_y >= 82 && touch_y <= 122)
              lin_speed += 10;
            
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
            screen = DRV_SCREEN_START;
            ui_init = false;
            delay(200);
          }

          break;
        }
        case DRV_SCREEN_SETTINGS_2:
        {
          if (!ui_init)
          {
            LCDClear();

            //Go Back Symbol
            LCDDrawCircle(20, LCD_HEIGHT - 20, 10, WHITE);
            LCDSetFontSize(1);
            LCDSetFontColor(BLACK, WHITE);
            LCDPrintAt(19, LCD_HEIGHT - 23, "L");

            //Go Back Symbol label
            LCDSetFontColor(WHITE);
            LCDPrintAt(34, LCD_HEIGHT - 23, "Go back");

            //Left Motor Offset label
            LCDSetFontSize(1);
            LCDSetFontColor(WHITE);
            LCDPrintAt(5, 5, "Left Motor Offset");

            //Left Motor Offset decrease
            LCDDrawRect(5, 20, 40, 40, WHITE);
            LCDDrawRect(15, 38, 20, 5, BLACK);

            //Left Motor Offset increase
            LCDDrawRect(125, 20, 40, 40, WHITE);
            LCDDrawRect(135, 38, 20, 5, BLACK);
            LCDDrawRect(143, 30, 5, 21, BLACK);

            //Right Motor Offset label
            LCDPrintAt(5, 68, "Right Motor Offset");
            
            //Right Motor Offset decrease
            LCDDrawRect(5, 82, 40, 40, WHITE);
            LCDDrawRect(15, 100, 20, 5, BLACK);

            //Right Motor Offset increase
            LCDDrawRect(125, 82, 40, 40, WHITE);
            LCDDrawRect(135, 100, 20, 5, BLACK);
            LCDDrawRect(143, 92, 5, 21, BLACK);

            ui_init = true;
          }

          bool left_pressed;
          INReadButton(LEFT_BUTTON, &left_pressed);

          if (left_pressed)
          {
            ui_init = false;
            screen = DRV_SCREEN_SETTINGS_1;
            delay(200);
            break;
          }

          char val_str[8];
          LCDSetFontSize(2);
          LCDSetFontColor(WHITE);

          //Left Motor Offset value
          snprintf(val_str, 8, "%d ", left_motor_offset);
          LCDPrintAt(50, 34, val_str);

          //Right Motor Offset value
          snprintf(val_str, 8, "%d ", right_motor_offset);
          LCDPrintAt(50, 96, val_str);

          int touch_x, touch_y;
          INReadTouch(&touch_x, &touch_y);

          if (touch_x >= 5 && touch_x <= 45)
          {
            if (touch_y >= 20 && touch_y <= 60)
              left_motor_offset -= 1;
            
            if (touch_y >= 82 && touch_y <= 122)
              right_motor_offset -= 1;
            
            if (left_motor_offset < -255)
              left_motor_offset = 255;

            if (right_motor_offset < -255)
              right_motor_offset = -255;

            delay(200);
          }

          if (touch_x >= 125 && touch_x <= 165)
          {
            if (touch_y >= 20 && touch_y <= 60)
              left_motor_offset += 1;
            
            if (touch_y >= 82 && touch_y <= 122)
              right_motor_offset += 1;

            if (left_motor_offset > 255)
              left_motor_offset = 255;

            if (right_motor_offset > 255)
              right_motor_offset = 255;

            delay(200);
          }

          break;
        }
        case DRV_SCREEN_START:
        {
          if (!ui_init)
          {
            LCDDrawRect(0, 0, LCD_WIDTH, LCD_HEIGHT, BLACK);
            LCDDrawRect(5, 5, 160, 310, RED);

            LCDSetFontSize(4);
            LCDSetFontColor(WHITE, RED);
            LCDPrintAt(29, 115, "TOUCH");
            LCDPrintAt(29, 165, "RESET");

            DRVSetMotorOffsets(left_motor_offset, right_motor_offset);
            DRVSetMaxLinearSpeed(max_lin_speed);
            DRVStraight(distance, lin_speed);

            ui_init = true;
          }

          int touch_x, touch_y;
          INReadTouch(&touch_x, &touch_y);

          if (touch_x >= 0 && touch_y >= 0)
          {
            screen = DRV_SCREEN_SETTINGS_1;
            ui_init = false;
            DRVSetSpeed(0, 0);
            delay(200);
          }

          break;
        }
        default:
          break;
      }

      break;
    }
    case DRIVE_CURVE:
    {
      static DriveScreen screen = DRV_SCREEN_SETTINGS_1;

      switch (screen)
      {
        case DRV_SCREEN_SETTINGS_1:
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

            //Extra Settings Page symbol
            LCDDrawCircle(75, LCD_HEIGHT - 20, 10, WHITE);
            LCDSetFontSize(1);
            LCDSetFontColor(BLACK, WHITE);
            LCDPrintAt(73, LCD_HEIGHT - 23, "R");

            //Extra Settings Symbol label
            LCDSetFontColor(WHITE);
            LCDPrintAt(89, LCD_HEIGHT - 23, "Extras");

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

            //Angle label
            LCDPrintAt(5, 130, "Final Angle (degrees)");

            //Angle decrease
            LCDDrawRect(5, 144, 40, 40, WHITE);
            LCDDrawRect(15, 162, 20, 5, BLACK);

            //Angle increase
            LCDDrawRect(125, 144, 40, 40, WHITE);
            LCDDrawRect(135, 162, 20, 5, BLACK);
            LCDDrawRect(143, 154, 5, 21, BLACK);

            //Reverse direction option
            LCDDrawRect(5, 193, 160, 40, WHITE);
            LCDSetFontSize(2);
            LCDSetFontColor(BLACK, WHITE);
            LCDPrintAt(44, 205, "REVERSE");

            //Start button
            LCDDrawRect(5, 240, 160, 40, WHITE);
            LCDPrintAt(54, 252, "START");

            ui_init = true;
          }

          char val_str[8];
          LCDSetFontSize(2);
          LCDSetFontColor(WHITE);

          //Distance value
          snprintf(val_str, 8, "%d ", distance);
          LCDPrintAt(50, 34, val_str);

          //Speed value
          snprintf(val_str, 8, "%d ", lin_speed);
          LCDPrintAt(50, 96, val_str);

          //Angle value
          snprintf(val_str, 8, "%d  ", angle);
          LCDPrintAt(50, 158, val_str);

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
            screen = DRV_SCREEN_SETTINGS_2;
            ui_init = false;
            delay(200);
            break;
          }

          int touch_x, touch_y;
          INReadTouch(&touch_x, &touch_y);

          if (touch_x >= 5 && touch_x <= 45)
          {
            if (touch_y >= 20 && touch_y <= 60)
              distance -= 100;
            
            if (touch_y >= 82 && touch_y <= 122)
              lin_speed -= 10;
            
            if (touch_y >= 144 && touch_y <= 184)
              angle -= 10;
            
            if (lin_speed < 0)
              lin_speed = 0;

            delay(200);
          }

          if (touch_x >= 125 && touch_x <= 165)
          {
            if (touch_y >= 20 && touch_y <= 60)
              distance += 100;
            
            if (touch_y >= 82 && touch_y <= 122)
              lin_speed += 10;
            
            if (touch_y >= 144 && touch_y <= 184)
              angle += 10;

            delay(200);
          }

          if (touch_x >= 5 && touch_x <= 165 && touch_y >= 193 && touch_y <= 233)
          {
            distance *= -1;
            delay(200);
          }

          if (touch_x >= 5 && touch_x <= 165 && touch_y >= 240 && touch_y <= 280)
          {
            screen = DRV_SCREEN_START;
            ui_init = false;
            delay(200);
          }

          break;
        }
        case DRV_SCREEN_SETTINGS_2:
        {
          if (!ui_init)
          {
            LCDClear();

            //Exit Symbol
            LCDDrawCircle(20, LCD_HEIGHT - 20, 10, WHITE);
            LCDSetFontSize(1);
            LCDSetFontColor(BLACK, WHITE);
            LCDPrintAt(19, LCD_HEIGHT - 23, "L");

            //Go Back Symbol label
            LCDSetFontColor(WHITE);
            LCDPrintAt(34, LCD_HEIGHT - 23, "Go back");

            //Left Motor Offset label
            LCDSetFontSize(1);
            LCDSetFontColor(WHITE);
            LCDPrintAt(5, 5, "Left Motor Offset");

            //Left Motor Offset decrease
            LCDDrawRect(5, 20, 40, 40, WHITE);
            LCDDrawRect(15, 38, 20, 5, BLACK);

            //Left Motor Offset increase
            LCDDrawRect(125, 20, 40, 40, WHITE);
            LCDDrawRect(135, 38, 20, 5, BLACK);
            LCDDrawRect(143, 30, 5, 21, BLACK);

            //Right Motor Offset label
            LCDPrintAt(5, 68, "Right Motor Offset");
            
            //Right Motor Offset decrease
            LCDDrawRect(5, 82, 40, 40, WHITE);
            LCDDrawRect(15, 100, 20, 5, BLACK);

            //Right Motor Offset increase
            LCDDrawRect(125, 82, 40, 40, WHITE);
            LCDDrawRect(135, 100, 20, 5, BLACK);
            LCDDrawRect(143, 92, 5, 21, BLACK);

            //Max Linear Speed label
            LCDPrintAt(5, 130, "Max Linear Speed (mm/s)");

            //Max Linear Speed decrease
            LCDDrawRect(5, 144, 40, 40, WHITE);
            LCDDrawRect(15, 162, 20, 5, BLACK);

            //Max Linear Speed increase
            LCDDrawRect(125, 144, 40, 40, WHITE);
            LCDDrawRect(135, 162, 20, 5, BLACK);
            LCDDrawRect(143, 154, 5, 21, BLACK);

            //Max Angular Speed label
            LCDPrintAt(5, 192, "Max Angular Speed (deg./s)");

            //Max Angular Speed decrease
            LCDDrawRect(5, 206, 40, 40, WHITE);
            LCDDrawRect(15, 224, 20, 5, BLACK);

            //Max Angular Speed increase
            LCDDrawRect(125, 206, 40, 40, WHITE);
            LCDDrawRect(135, 224, 20, 5, BLACK);
            LCDDrawRect(143, 216, 5, 21, BLACK);

            ui_init = true;
          }

          bool left_pressed;
          INReadButton(LEFT_BUTTON, &left_pressed);

          if (left_pressed)
          {
            screen = DRV_SCREEN_SETTINGS_1;
            ui_init = false;
            delay(200);
            break;
          }

          char val_str[8];
          LCDSetFontSize(2);
          LCDSetFontColor(WHITE);

          //Left Motor Offset value
          snprintf(val_str, 8, "%d ", left_motor_offset);
          LCDPrintAt(50, 34, val_str);

          //Right Motor Offset value
          snprintf(val_str, 8, "%d ", right_motor_offset);
          LCDPrintAt(50, 96, val_str);

          //Max Linear Speed value
          snprintf(val_str, 8, "%d ", max_lin_speed);
          LCDPrintAt(50, 158, val_str);

          //Max Linear Speed value
          snprintf(val_str, 8, "%d ", max_ang_speed);
          LCDPrintAt(50, 220, val_str);

          int touch_x, touch_y;
          INReadTouch(&touch_x, &touch_y);

          if (touch_x >= 5 && touch_x <= 45)
          {
            if (touch_y >= 20 && touch_y <= 60)
              left_motor_offset -= 1;
            
            if (touch_y >= 82 && touch_y <= 122)
              right_motor_offset -= 1;
            
            if (touch_y >= 144 && touch_y <= 184)
              max_lin_speed -= 10;
            
            if (touch_y >= 206 && touch_y <= 246)
              max_ang_speed -= 10;
            
            if (left_motor_offset < -255)
              left_motor_offset = -255;
            
            if (right_motor_offset < -255)
              right_motor_offset = -255;
            
            if (max_lin_speed < 0)
              max_lin_speed = 0;
            
            if (max_ang_speed < 0)
              max_ang_speed = 0;

            delay(200);
          }
          else if (touch_x >= 125 && touch_x <= 165)
          {
            if (touch_y >= 20 && touch_y <= 60)
              left_motor_offset += 1;
            
            if (touch_y >= 82 && touch_y <= 122)
              right_motor_offset += 1;
            
            if (touch_y >= 144 && touch_y <= 184)
              max_lin_speed += 10;
            
            if (touch_y >= 206 && touch_y <= 246)
              max_ang_speed += 10;

            if (left_motor_offset > 255)
              left_motor_offset = 255;
            
            if (right_motor_offset > 255)
              right_motor_offset = 255;

            delay(200);
          }

          break;
        }
        case DRV_SCREEN_START:
        {
          if (!ui_init)
          {
            LCDClear();
            LCDDrawRect(5, 5, 160, 310, RED);
            
            LCDSetFontSize(4);
            LCDSetFontColor(WHITE, RED);
            LCDPrintAt(29, 115, "TOUCH");
            LCDPrintAt(29, 165, "RESET");

            DRVSetMotorOffsets(left_motor_offset, right_motor_offset);
            DRVSetMaxLinearSpeed(max_lin_speed);
            DRVSetMaxAngularSpeed(max_ang_speed);

            DRVCurve(distance, angle, lin_speed);

            ui_init = true;
          }

          int touch_x, touch_y;
          INReadTouch(&touch_x, &touch_y);

          if (touch_x >= 0 && touch_y >= 0)
          {
            screen = DRV_SCREEN_SETTINGS_1;
            ui_init = false;
            DRVSetSpeed(0, 0);
            delay(200);
          }

          break;
        }
        default:
          screen = DRV_SCREEN_SETTINGS_1;
          break;
      }

      break;
    }
    case DRIVE_TURN:
    {
      static DriveScreen screen = DRV_SCREEN_SETTINGS_1;

      switch (screen)
      {
        case DRV_SCREEN_SETTINGS_1:
        {
          if (!ui_init)
          {
            LCDClear();

            //Exit Symbol
            LCDDrawCircle(20, LCD_HEIGHT - 20, 10, WHITE);
            LCDSetFontSize(1);
            LCDSetFontColor(BLACK, WHITE);
            LCDPrintAt(19, LCD_HEIGHT - 23, "L");

            //Exit Symbol label
            LCDSetFontColor(WHITE);
            LCDPrintAt(34, LCD_HEIGHT - 23, "Exit");

            //Extra Settings Page symbol
            LCDDrawCircle(75, LCD_HEIGHT - 20, 10, WHITE);
            LCDSetFontSize(1);
            LCDSetFontColor(BLACK, WHITE);
            LCDPrintAt(73, LCD_HEIGHT - 23, "R");

            //Extra Settings Symbol label
            LCDSetFontColor(WHITE);
            LCDPrintAt(89, LCD_HEIGHT - 23, "Extras");

            //Final Angle label
            LCDSetFontSize(1);
            LCDSetFontColor(WHITE);
            LCDPrintAt(5, 5, "Final angle (deg.)");

            //Final Angle decrease
            LCDDrawRect(5, 20, 40, 40, WHITE);
            LCDDrawRect(15, 38, 20, 5, BLACK);

            //Final Angle increase
            LCDDrawRect(125, 20, 40, 40, WHITE);
            LCDDrawRect(135, 38, 20, 5, BLACK);
            LCDDrawRect(143, 30, 5, 21, BLACK);

            //Angular Speed label
            LCDPrintAt(5, 68, "Angular Speed (deg./s)");
            
            //Angular Speed decrease
            LCDDrawRect(5, 82, 40, 40, WHITE);
            LCDDrawRect(15, 100, 20, 5, BLACK);

            //Angular Speed increase
            LCDDrawRect(125, 82, 40, 40, WHITE);
            LCDDrawRect(135, 100, 20, 5, BLACK);
            LCDDrawRect(143, 92, 5, 21, BLACK);

            //Reverse direction option
            LCDDrawRect(5, 193, 160, 40, WHITE);
            LCDSetFontSize(2);
            LCDSetFontColor(BLACK, WHITE);
            LCDPrintAt(44, 205, "REVERSE");

            //Start button
            LCDDrawRect(5, 240, 160, 40, WHITE);
            LCDPrintAt(54, 252, "START");

            ui_init = true;
          }

          char val_str[8];
          LCDSetFontSize(2);
          LCDSetFontColor(WHITE);

          //Final Angle value
          snprintf(val_str, 8, "%d  ", angle);
          LCDPrintAt(50, 34, val_str);

          //Angular Speed value
          snprintf(val_str, 8, "%d ", ang_speed);
          LCDPrintAt(50, 96, val_str);

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
            screen = DRV_SCREEN_SETTINGS_2;
            ui_init = false;
            delay(200);
            break;
          }

          int touch_x, touch_y;
          INReadTouch(&touch_x, &touch_y);

          if (touch_x >= 5 && touch_x <= 45)
          {
            if (touch_y >= 20 && touch_y <= 60)
              angle -= 10;
            
            if (touch_y >= 82 && touch_y <= 122)
              ang_speed -= 10;
            
            if (ang_speed < 0)
              ang_speed = 0;
            
            delay(200);
          }
          else if (touch_x >= 125 && touch_x <= 165)
          {
            if (touch_y >= 20 && touch_y <= 60)
              angle += 10;
            
            if (touch_y >= 82 && touch_y <= 122)
              ang_speed += 10;

            delay(200);
          }
          else if (touch_x >= 5 && touch_x <= 165 && touch_y >= 193 && touch_y <= 233)
          {
            angle *= -1;
            delay(200);
          }
          else if (touch_x >= 5 && touch_x <= 165 && touch_y >= 240 && touch_y <= 280)
          {
            screen = DRV_SCREEN_START;
            ui_init = false;
            delay(200);
          }

          break;
        }
        case DRV_SCREEN_SETTINGS_2:
        {
          if (!ui_init)
          {
            LCDClear();

            //Exit Symbol
            LCDDrawCircle(20, LCD_HEIGHT - 20, 10, WHITE);
            LCDSetFontSize(1);
            LCDSetFontColor(BLACK, WHITE);
            LCDPrintAt(19, LCD_HEIGHT - 23, "L");

            //Go Back Symbol label
            LCDSetFontColor(WHITE);
            LCDPrintAt(34, LCD_HEIGHT - 23, "Go back");

            //Left Motor Offset label
            LCDSetFontSize(1);
            LCDSetFontColor(WHITE);
            LCDPrintAt(5, 5, "Left Motor Offset");

            //Left Motor Offset decrease
            LCDDrawRect(5, 20, 40, 40, WHITE);
            LCDDrawRect(15, 38, 20, 5, BLACK);

            //Left Motor Offset increase
            LCDDrawRect(125, 20, 40, 40, WHITE);
            LCDDrawRect(135, 38, 20, 5, BLACK);
            LCDDrawRect(143, 30, 5, 21, BLACK);

            //Right Motor Offset label
            LCDPrintAt(5, 68, "Right Motor Offset");
            
            //Right Motor Offset decrease
            LCDDrawRect(5, 82, 40, 40, WHITE);
            LCDDrawRect(15, 100, 20, 5, BLACK);

            //Right Motor Offset increase
            LCDDrawRect(125, 82, 40, 40, WHITE);
            LCDDrawRect(135, 100, 20, 5, BLACK);
            LCDDrawRect(143, 92, 5, 21, BLACK);

            //Max Linear Speed label
            LCDPrintAt(5, 130, "Max Linear Speed (mm/s)");

            //Max Linear Speed decrease
            LCDDrawRect(5, 144, 40, 40, WHITE);
            LCDDrawRect(15, 162, 20, 5, BLACK);

            //Max Linear Speed increase
            LCDDrawRect(125, 144, 40, 40, WHITE);
            LCDDrawRect(135, 162, 20, 5, BLACK);
            LCDDrawRect(143, 154, 5, 21, BLACK);

            //Max Angular Speed label
            LCDPrintAt(5, 192, "Max Angular Speed (deg./s)");

            //Max Angular Speed decrease
            LCDDrawRect(5, 206, 40, 40, WHITE);
            LCDDrawRect(15, 224, 20, 5, BLACK);

            //Max Angular Speed increase
            LCDDrawRect(125, 206, 40, 40, WHITE);
            LCDDrawRect(135, 224, 20, 5, BLACK);
            LCDDrawRect(143, 216, 5, 21, BLACK);

            ui_init = true;
          }

          bool left_pressed;
          INReadButton(LEFT_BUTTON, &left_pressed);

          if (left_pressed)
          {
            screen = DRV_SCREEN_SETTINGS_1;
            ui_init = false;
            delay(200);
            break;
          }

          char val_str[8];
          LCDSetFontSize(2);
          LCDSetFontColor(WHITE);

          //Left Motor Offset value
          snprintf(val_str, 8, "%d ", left_motor_offset);
          LCDPrintAt(50, 34, val_str);

          //Right Motor Offset value
          snprintf(val_str, 8, "%d ", right_motor_offset);
          LCDPrintAt(50, 96, val_str);

          //Max Linear Speed value
          snprintf(val_str, 8, "%d ", max_lin_speed);
          LCDPrintAt(50, 158, val_str);

          //Max Linear Speed value
          snprintf(val_str, 8, "%d ", max_ang_speed);
          LCDPrintAt(50, 220, val_str);

          int touch_x, touch_y;
          INReadTouch(&touch_x, &touch_y);

          if (touch_x >= 5 && touch_x <= 45)
          {
            if (touch_y >= 20 && touch_y <= 60)
              left_motor_offset -= 1;
            
            if (touch_y >= 82 && touch_y <= 122)
              right_motor_offset -= 1;
            
            if (touch_y >= 144 && touch_y <= 184)
              max_lin_speed -= 10;
            
            if (touch_y >= 206 && touch_y <= 246)
              max_ang_speed -= 10;
            
            if (left_motor_offset < -255)
              left_motor_offset = -255;
            
            if (right_motor_offset < -255)
              right_motor_offset = -255;
            
            if (max_lin_speed < 0)
              max_lin_speed = 0;
            
            if (max_ang_speed < 0)
              max_ang_speed = 0;

            delay(200);
          }
          else if (touch_x >= 125 && touch_x <= 165)
          {
            if (touch_y >= 20 && touch_y <= 60)
              left_motor_offset += 1;
            
            if (touch_y >= 82 && touch_y <= 122)
              right_motor_offset += 1;
            
            if (touch_y >= 144 && touch_y <= 184)
              max_lin_speed += 10;
            
            if (touch_y >= 206 && touch_y <= 246)
              max_ang_speed += 10;

            if (left_motor_offset > 255)
              left_motor_offset = 255;
            
            if (right_motor_offset > 255)
              right_motor_offset = 255;

            delay(200);
          }

          break;
        }
        case DRV_SCREEN_START:
        {
          if (!ui_init)
          {
            LCDClear();

            LCDDrawRect(5, 5, 160, 310, RED);
            
            LCDSetFontSize(4);
            LCDSetFontColor(WHITE, RED);
            LCDPrintAt(29, 115, "TOUCH");
            LCDPrintAt(29, 165, "RESET");

            DRVSetMotorOffsets(left_motor_offset, right_motor_offset);
            DRVSetMaxAngularSpeed(max_ang_speed);
            DRVSetMaxLinearSpeed(max_lin_speed);
            DRVTurn(angle, ang_speed);

            ui_init = true;
          }

          int touch_x, touch_y;
          INReadTouch(&touch_x, &touch_y);

          if (touch_x >= 0 && touch_y >= 0)
          {
            screen = DRV_SCREEN_SETTINGS_1;
            ui_init = false;
            DRVSetSpeed(0, 0);
            delay(200);
            break;
          }

          break;
        }
        default:
          screen = DRV_SCREEN_SETTINGS_1;
          break;
      }
      break;
    }
    case DRIVE_GOTO:
    {
      static DriveScreen screen = DRV_SCREEN_SETTINGS_1;
      static int x = 0, y = 0;

      switch (screen)
      {
        case DRV_SCREEN_SETTINGS_1:
        {
          if (!ui_init)
          {
            LCDClear();

            //Exit Symbol
            LCDDrawCircle(20, LCD_HEIGHT - 20, 10, WHITE);
            LCDSetFontSize(1);
            LCDSetFontColor(BLACK, WHITE);
            LCDPrintAt(19, LCD_HEIGHT - 23, "L");

            //Exit Symbol label
            LCDSetFontColor(WHITE);
            LCDPrintAt(34, LCD_HEIGHT - 23, "Exit");

            //Extra Settings Page symbol
            LCDDrawCircle(75, LCD_HEIGHT - 20, 10, WHITE);
            LCDSetFontSize(1);
            LCDSetFontColor(BLACK, WHITE);
            LCDPrintAt(73, LCD_HEIGHT - 23, "R");

            //Extra Settings Symbol label
            LCDSetFontColor(WHITE);
            LCDPrintAt(89, LCD_HEIGHT - 23, "Extras");

            //X Postion label
            LCDSetFontSize(1);
            LCDSetFontColor(WHITE);
            LCDPrintAt(5, 5, "Diff X (mm)");

            //X Position decrease
            LCDDrawRect(5, 20, 40, 40, WHITE);
            LCDDrawRect(15, 38, 20, 5, BLACK);

            //X Position increase
            LCDDrawRect(125, 20, 40, 40, WHITE);
            LCDDrawRect(135, 38, 20, 5, BLACK);
            LCDDrawRect(143, 30, 5, 21, BLACK);

            //Y Position label
            LCDPrintAt(5, 68, "Diff Y (mm)");
            
            //Y Position decrease
            LCDDrawRect(5, 82, 40, 40, WHITE);
            LCDDrawRect(15, 100, 20, 5, BLACK);

            //Y Position increase
            LCDDrawRect(125, 82, 40, 40, WHITE);
            LCDDrawRect(135, 100, 20, 5, BLACK);
            LCDDrawRect(143, 92, 5, 21, BLACK);

            //Linear Speed label
            LCDPrintAt(5, 130, "Linear Speed (mm/s)");

            //Linear Speed decrease
            LCDDrawRect(5, 144, 40, 40, WHITE);
            LCDDrawRect(15, 162, 20, 5, BLACK);

            //Linear Speed increase
            LCDDrawRect(125, 144, 40, 40, WHITE);
            LCDDrawRect(135, 162, 20, 5, BLACK);
            LCDDrawRect(143, 154, 5, 21, BLACK);

            //Reverse direction option
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

          bool right_pressed;
          INReadButton(RIGHT_BUTTON, &right_pressed);
          if (right_pressed)
          {
            screen = DRV_SCREEN_SETTINGS_2;
            ui_init = false;
            delay(200);
            break;
          }

          char val_str[8];
          LCDSetFontSize(2);
          LCDSetFontColor(WHITE);

          //X Position value
          snprintf(val_str, 8, "%d  ", x);
          LCDPrintAt(50, 34, val_str);

          //Y Position value
          snprintf(val_str, 8, "%d  ", y);
          LCDPrintAt(50, 96, val_str);

          //Linear Speed value
          snprintf(val_str, 8, "%d ", lin_speed);
          LCDPrintAt(50, 158, val_str);

          int touch_x, touch_y;
          INReadTouch(&touch_x, &touch_y);

          if (touch_x >= 5 && touch_x <= 45)
          {
            if (touch_y >= 20 && touch_y <= 60)
              x -= 10;
            
            if (touch_y >= 82 && touch_y <= 122)
              y -= 10;
            
            if (touch_y >= 144 && touch_y <= 184)
              lin_speed -= 10;
            
            if (lin_speed < 0)
              lin_speed = 0;

            delay(200);
          }
          else if (touch_x >= 125 && touch_x <= 165)
          {
            if (touch_y >= 20 && touch_y <= 60)
              x += 10;
            
            if (touch_y >= 82 && touch_y <= 122)
              y += 10;
            
            if (touch_y >= 144 && touch_y <= 184)
              lin_speed += 10;

            delay(200);
          }
          else if (touch_x >= 5 && touch_x <= 165 && touch_y >= 193 && touch_y <= 233)
          {
            y *= -1;
            x *= -1;
            delay(200);
          }
          else if (touch_x >= 5 && touch_x <= 165 && touch_y >= 240 && touch_y <= 280)
          {
            screen = DRV_SCREEN_START;
            ui_init = false;
            delay(200);
          }

          break;
        }
        case DRV_SCREEN_SETTINGS_2:
        {
          if (!ui_init)
          {
            LCDClear();

            //Exit Symbol
            LCDDrawCircle(20, LCD_HEIGHT - 20, 10, WHITE);
            LCDSetFontSize(1);
            LCDSetFontColor(BLACK, WHITE);
            LCDPrintAt(19, LCD_HEIGHT - 23, "L");

            //Go Back Symbol label
            LCDSetFontColor(WHITE);
            LCDPrintAt(34, LCD_HEIGHT - 23, "Go back");

            //Left Motor Offset label
            LCDSetFontSize(1);
            LCDSetFontColor(WHITE);
            LCDPrintAt(5, 5, "Left Motor Offset");

            //Left Motor Offset decrease
            LCDDrawRect(5, 20, 40, 40, WHITE);
            LCDDrawRect(15, 38, 20, 5, BLACK);

            //Left Motor Offset increase
            LCDDrawRect(125, 20, 40, 40, WHITE);
            LCDDrawRect(135, 38, 20, 5, BLACK);
            LCDDrawRect(143, 30, 5, 21, BLACK);

            //Right Motor Offset label
            LCDPrintAt(5, 68, "Right Motor Offset");
            
            //Right Motor Offset decrease
            LCDDrawRect(5, 82, 40, 40, WHITE);
            LCDDrawRect(15, 100, 20, 5, BLACK);

            //Right Motor Offset increase
            LCDDrawRect(125, 82, 40, 40, WHITE);
            LCDDrawRect(135, 100, 20, 5, BLACK);
            LCDDrawRect(143, 92, 5, 21, BLACK);

            //Max Linear Speed label
            LCDPrintAt(5, 130, "Max Linear Speed (mm/s)");

            //Max Linear Speed decrease
            LCDDrawRect(5, 144, 40, 40, WHITE);
            LCDDrawRect(15, 162, 20, 5, BLACK);

            //Max Linear Speed increase
            LCDDrawRect(125, 144, 40, 40, WHITE);
            LCDDrawRect(135, 162, 20, 5, BLACK);
            LCDDrawRect(143, 154, 5, 21, BLACK);

            //Max Angular Speed label
            LCDPrintAt(5, 192, "Max Angular Speed (deg./s)");

            //Max Angular Speed decrease
            LCDDrawRect(5, 206, 40, 40, WHITE);
            LCDDrawRect(15, 224, 20, 5, BLACK);

            //Max Angular Speed increase
            LCDDrawRect(125, 206, 40, 40, WHITE);
            LCDDrawRect(135, 224, 20, 5, BLACK);
            LCDDrawRect(143, 216, 5, 21, BLACK);

            ui_init = true;
          }

          bool left_pressed;
          INReadButton(LEFT_BUTTON, &left_pressed);
          if (left_pressed)
          {
            screen = DRV_SCREEN_SETTINGS_1;
            ui_init = false;
            delay(200);
            break;
          }

          char val_str[8];
          LCDSetFontSize(2);
          LCDSetFontColor(WHITE);

          //Left Motor Offset value
          snprintf(val_str, 8, "%d  ", left_motor_offset);
          LCDPrintAt(50, 34, val_str);

          //Right Motor Offset value
          snprintf(val_str, 8, "%d  ", right_motor_offset);
          LCDPrintAt(50, 96, val_str);

          //Max Linear Speed value
          snprintf(val_str, 8, "%d ", max_lin_speed);
          LCDPrintAt(50, 158, val_str);

          //Max Linear Speed value
          snprintf(val_str, 8, "%d ", max_ang_speed);
          LCDPrintAt(50, 220, val_str);

          int touch_x, touch_y;
          INReadTouch(&touch_x, &touch_y);

          if (touch_x >= 5 && touch_x <= 45)
          {
            if (touch_y >= 20 && touch_y <= 60)
              left_motor_offset -= 1;
            
            if (touch_y >= 82 && touch_y <= 122)
              right_motor_offset -= 1;
            
            if (touch_y >= 144 && touch_y <= 184)
              max_lin_speed -= 10;
            
            if (touch_y >= 206 && touch_y <= 246)
              max_ang_speed -= 10;
            
            if (left_motor_offset < -255)
              left_motor_offset = -255;
            
            if (right_motor_offset < -255)
              right_motor_offset = -255;
            
            if (max_lin_speed < 0)
              max_lin_speed = 0;
            
            if (max_ang_speed < 0)
              max_ang_speed = 0;

            delay(200);
          }
          else if (touch_x >= 125 && touch_x <= 165)
          {
            if (touch_y >= 20 && touch_y <= 60)
              left_motor_offset += 1;
            
            if (touch_y >= 82 && touch_y <= 122)
              right_motor_offset += 1;
            
            if (touch_y >= 144 && touch_y <= 184)
              max_lin_speed += 10;
            
            if (touch_y >= 206 && touch_y <= 246)
              max_ang_speed += 10;

            if (left_motor_offset > 255)
              left_motor_offset = 255;
            
            if (right_motor_offset > 255)
              right_motor_offset = 255;

            delay(200);
          }

          break;
        }
        case DRV_SCREEN_START:
        {
          if (!ui_init)
          {
            LCDClear();

            LCDDrawRect(5, 5, 160, 310, RED);
            
            LCDSetFontSize(4);
            LCDSetFontColor(WHITE, RED);
            LCDPrintAt(29, 115, "TOUCH");
            LCDPrintAt(29, 165, "RESET");

            DRVSetMotorOffsets(left_motor_offset, right_motor_offset);
            DRVSetMaxLinearSpeed(max_lin_speed);
            DRVSetMaxAngularSpeed(max_ang_speed);
            DRVGoTo(x, y, lin_speed);

            ui_init = true;
          }

          int touch_x, touch_y;
          INReadTouch(&touch_x, &touch_y);

          if (touch_x >= 0 && touch_y >= 0)
          {
            screen = DRV_SCREEN_SETTINGS_1;
            ui_init = false;
            DRVSetSpeed(0, 0);
            delay(200);
            break;
          }

          break;
        }
        default:
          screen = DRV_SCREEN_SETTINGS_1;
          break;
      }
      break;
    }
    case DRIVE_SET:
    {
      static DriveScreen screen = DRV_SCREEN_SETTINGS_1;
      static int local_lin_speed = lin_speed, local_ang_speed = ang_speed;

      switch (screen)
      {
        case DRV_SCREEN_SETTINGS_1:
        {
          if (!ui_init)
          {
            LCDClear();

            //Exit Symbol
            LCDDrawCircle(20, LCD_HEIGHT - 20, 10, WHITE);
            LCDSetFontSize(1);
            LCDSetFontColor(BLACK, WHITE);
            LCDPrintAt(19, LCD_HEIGHT - 23, "L");

            //Exit Symbol label
            LCDSetFontColor(WHITE);
            LCDPrintAt(34, LCD_HEIGHT - 23, "Exit");

            //Extra Settings Page symbol
            LCDDrawCircle(75, LCD_HEIGHT - 20, 10, WHITE);
            LCDSetFontSize(1);
            LCDSetFontColor(BLACK, WHITE);
            LCDPrintAt(73, LCD_HEIGHT - 23, "R");

            //Extra Settings Symbol label
            LCDSetFontColor(WHITE);
            LCDPrintAt(89, LCD_HEIGHT - 23, "Extras");

            //Linear Speed label
            LCDSetFontSize(1);
            LCDSetFontColor(WHITE);
            LCDPrintAt(5, 5, "Linear Speed (mm/s)");

            //Linear Speed decrease
            LCDDrawRect(5, 20, 40, 40, WHITE);
            LCDDrawRect(15, 38, 20, 5, BLACK);

            //Linear Speed increase
            LCDDrawRect(125, 20, 40, 40, WHITE);
            LCDDrawRect(135, 38, 20, 5, BLACK);
            LCDDrawRect(143, 30, 5, 21, BLACK);

            //Angular Speed label
            LCDPrintAt(5, 68, "Angular Speed (deg./s)");
            
            //Angular Speed decrease
            LCDDrawRect(5, 82, 40, 40, WHITE);
            LCDDrawRect(15, 100, 20, 5, BLACK);

            //Angular Speed increase
            LCDDrawRect(125, 82, 40, 40, WHITE);
            LCDDrawRect(135, 100, 20, 5, BLACK);
            LCDDrawRect(143, 92, 5, 21, BLACK);

            //Reverse direction option
            LCDDrawRect(5, 193, 160, 40, WHITE);
            LCDSetFontSize(2);
            LCDSetFontColor(BLACK, WHITE);
            LCDPrintAt(44, 205, "REVERSE");

            //Start button
            LCDDrawRect(5, 240, 160, 40, WHITE);
            LCDPrintAt(54, 252, "START");

            ui_init = true;
          }

          char val_str[8];
          LCDSetFontSize(2);
          LCDSetFontColor(WHITE);

          //Final Angle value
          snprintf(val_str, 8, "%d  ", local_lin_speed);
          LCDPrintAt(50, 34, val_str);

          //Angular Speed value
          snprintf(val_str, 8, "%d  ", local_ang_speed);
          LCDPrintAt(50, 96, val_str);

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
            screen = DRV_SCREEN_SETTINGS_2;
            ui_init = false;
            delay(200);
            break;
          }

          int touch_x, touch_y;
          INReadTouch(&touch_x, &touch_y);

          if (touch_x >= 5 && touch_x <= 45)
          {
            if (touch_y >= 20 && touch_y <= 60)
              local_lin_speed -= 10;
            
            if (touch_y >= 82 && touch_y <= 122)
              local_ang_speed -= 10;
            
            delay(200);
          }
          else if (touch_x >= 125 && touch_x <= 165)
          {
            if (touch_y >= 20 && touch_y <= 60)
              local_lin_speed += 10;
            
            if (touch_y >= 82 && touch_y <= 122)
              local_ang_speed += 10;

            delay(200);
          }
          else if (touch_x >= 5 && touch_x <= 165 && touch_y >= 193 && touch_y <= 233)
          {
            local_lin_speed *= -1;
            local_ang_speed *= -1;
            delay(200);
          }
          else if (touch_x >= 5 && touch_x <= 165 && touch_y >= 240 && touch_y <= 280)
          {
            screen = DRV_SCREEN_START;
            ui_init = false;
            delay(200);
          }

          break;
        }
        case DRV_SCREEN_SETTINGS_2:
        {
          if (!ui_init)
          {
            LCDClear();

            //Exit Symbol
            LCDDrawCircle(20, LCD_HEIGHT - 20, 10, WHITE);
            LCDSetFontSize(1);
            LCDSetFontColor(BLACK, WHITE);
            LCDPrintAt(19, LCD_HEIGHT - 23, "L");

            //Go Back Symbol label
            LCDSetFontColor(WHITE);
            LCDPrintAt(34, LCD_HEIGHT - 23, "Go back");

            //Left Motor Offset label
            LCDSetFontSize(1);
            LCDSetFontColor(WHITE);
            LCDPrintAt(5, 5, "Left Motor Offset");

            //Left Motor Offset decrease
            LCDDrawRect(5, 20, 40, 40, WHITE);
            LCDDrawRect(15, 38, 20, 5, BLACK);

            //Left Motor Offset increase
            LCDDrawRect(125, 20, 40, 40, WHITE);
            LCDDrawRect(135, 38, 20, 5, BLACK);
            LCDDrawRect(143, 30, 5, 21, BLACK);

            //Right Motor Offset label
            LCDPrintAt(5, 68, "Right Motor Offset");
            
            //Right Motor Offset decrease
            LCDDrawRect(5, 82, 40, 40, WHITE);
            LCDDrawRect(15, 100, 20, 5, BLACK);

            //Right Motor Offset increase
            LCDDrawRect(125, 82, 40, 40, WHITE);
            LCDDrawRect(135, 100, 20, 5, BLACK);
            LCDDrawRect(143, 92, 5, 21, BLACK);

            //Max Linear Speed label
            LCDPrintAt(5, 130, "Max Linear Speed (mm/s)");

            //Max Linear Speed decrease
            LCDDrawRect(5, 144, 40, 40, WHITE);
            LCDDrawRect(15, 162, 20, 5, BLACK);

            //Max Linear Speed increase
            LCDDrawRect(125, 144, 40, 40, WHITE);
            LCDDrawRect(135, 162, 20, 5, BLACK);
            LCDDrawRect(143, 154, 5, 21, BLACK);

            //Max Angular Speed label
            LCDPrintAt(5, 192, "Max Angular Speed (deg./s)");

            //Max Angular Speed decrease
            LCDDrawRect(5, 206, 40, 40, WHITE);
            LCDDrawRect(15, 224, 20, 5, BLACK);

            //Max Angular Speed increase
            LCDDrawRect(125, 206, 40, 40, WHITE);
            LCDDrawRect(135, 224, 20, 5, BLACK);
            LCDDrawRect(143, 216, 5, 21, BLACK);

            ui_init = true;
          }

          bool left_pressed;
          INReadButton(LEFT_BUTTON, &left_pressed);

          if (left_pressed)
          {
            screen = DRV_SCREEN_SETTINGS_1;
            ui_init = false;
            delay(200);
            break;
          }

          char val_str[8];
          LCDSetFontSize(2);
          LCDSetFontColor(WHITE);

          //Left Motor Offset value
          snprintf(val_str, 8, "%d ", left_motor_offset);
          LCDPrintAt(50, 34, val_str);

          //Right Motor Offset value
          snprintf(val_str, 8, "%d ", right_motor_offset);
          LCDPrintAt(50, 96, val_str);

          //Max Linear Speed value
          snprintf(val_str, 8, "%d ", max_lin_speed);
          LCDPrintAt(50, 158, val_str);

          //Max Linear Speed value
          snprintf(val_str, 8, "%d ", max_ang_speed);
          LCDPrintAt(50, 220, val_str);

          int touch_x, touch_y;
          INReadTouch(&touch_x, &touch_y);

          if (touch_x >= 5 && touch_x <= 45)
          {
            if (touch_y >= 20 && touch_y <= 60)
              left_motor_offset -= 1;
            
            if (touch_y >= 82 && touch_y <= 122)
              right_motor_offset -= 1;
            
            if (touch_y >= 144 && touch_y <= 184)
              max_lin_speed -= 10;
            
            if (touch_y >= 206 && touch_y <= 246)
              max_ang_speed -= 10;
            
            if (left_motor_offset < -255)
              left_motor_offset = -255;
            
            if (right_motor_offset < -255)
              right_motor_offset = -255;
            
            if (max_lin_speed < 0)
              max_lin_speed = 0;
            
            if (max_ang_speed < 0)
              max_ang_speed = 0;

            delay(200);
          }
          else if (touch_x >= 125 && touch_x <= 165)
          {
            if (touch_y >= 20 && touch_y <= 60)
              left_motor_offset += 1;
            
            if (touch_y >= 82 && touch_y <= 122)
              right_motor_offset += 1;
            
            if (touch_y >= 144 && touch_y <= 184)
              max_lin_speed += 10;
            
            if (touch_y >= 206 && touch_y <= 246)
              max_ang_speed += 10;

            if (left_motor_offset > 255)
              left_motor_offset = 255;
            
            if (right_motor_offset > 255)
              right_motor_offset = 255;

            delay(200);
          }

          break;
        }
        case DRV_SCREEN_START:
        {
          if (!ui_init)
          {
            LCDClear();

            LCDDrawRect(5, 5, 160, 310, RED);
            
            LCDSetFontSize(4);
            LCDSetFontColor(WHITE, RED);
            LCDPrintAt(29, 115, "TOUCH");
            LCDPrintAt(29, 165, "RESET");

            DRVSetMotorOffsets(left_motor_offset, right_motor_offset);
            DRVSetMaxLinearSpeed(max_lin_speed);
            DRVSetMaxAngularSpeed(max_ang_speed);
            DRVSetSpeed(local_lin_speed, local_ang_speed);

            ui_init = true;
          }

          int touch_x, touch_y;
          INReadTouch(&touch_x, &touch_y);

          if (touch_x >= 0 && touch_y >= 0)
          {
            screen = DRV_SCREEN_SETTINGS_1;
            ui_init = false;
            DRVSetSpeed(0, 0);
            delay(200);
            break;
          }

          break;
        }
        default:
          screen = DRV_SCREEN_SETTINGS_1;
          break;
      }
      break;
    }
    case DISTANCE_SENSOR:
    {
      if (!ui_init)
      {
        LCDClear();

        //Exit Symbol
        LCDDrawCircle(20, LCD_HEIGHT - 20, 10, WHITE);
        LCDSetFontSize(1);
        LCDSetFontColor(BLACK, WHITE);
        LCDPrintAt(19, LCD_HEIGHT - 23, "L");

        //Exit Symbol label
        LCDSetFontColor(WHITE);
        LCDPrintAt(34, LCD_HEIGHT - 23, "Exit");

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

      LCDSetFontSize(3);
      LCDSetFontColor(WHITE);
      
      int raw_val;
      PSDGetRaw(&raw_val);
      int dist;
      PSDGet(&dist);

      LCDSetCursor(0, 100);
      char val_str[16];

      LCDPrintln("RAW:");
      snprintf(val_str, 8, "%d  ", raw_val);
      LCDPrintln(val_str);
      LCDPrintln("DIST:");
      snprintf(val_str, 8, "%d mm  ", dist);
      LCDPrintln(val_str);

      break;
    }
    default:
    {
      demoPhase = HOME_SCREEN;
      break;
    }
  }
}
