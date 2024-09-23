#include <eyebot.h>

#define INPUT_DELAY_MS 100
#define LCD_WIDTH 170
#define LCD_HEIGHT 320

enum Demo {
  DEMO_UNDEFINED,
  DEMO_CAMERA,
  DEMO_VW_SPEED,
  DEMO_VW_STRAIGHT,
  DEMO_VW_TURN,
  DEMO_VW_CURVE,
  DEMO_VW_DRIVE
};

COLOR gColImg[QQVGA_PIXELS];
BYTE gGrayImg[QQVGA_PIXELS];
BYTE gEdgeImg[QQVGA_PIXELS];

Demo gSelectedDemo = DEMO_UNDEFINED;

int gLinSpeed = 500, gAngSpeed = 0;
int gLeftMotorOffset = 0, gRightMotorOffset = 0;

int gStraightDist = 500;
int gStraightLinSpeed = 500;

int gTurnFinalAng = 225;
int gTurnAngSpeed = 225 ;

int gCurveLinSpeed = 250;
int gCurveFinalAng = 90;
int gCurveDist = 340;

int gDriveLinSpeed = 250;
int gDriveDeltaX = 50;
int gDriveDeltaY = 250;

int gScreenIdx = 0;

const int MAX_SCREENS = 1,
          MAX_OPTIONS = 6;

/*
Screen element coordinates and dimensions relevant
to all demo programs.
*/
const int X_MARGIN = 5,
          Y_MARGIN = 5,
          OPTION_WIDTH = 160,
          OPTION_HEIGHT = 40,
          OPTION_PADDING = 10,
          OPTION_X1 = X_MARGIN,
          OPTION_X2 = OPTION_X1 + OPTION_WIDTH,
          LB_X1 = X_MARGIN,
          LB_X2 = LB_X1 + 40,
          LB_Y1 = 305,
          LB_Y2 = 320,
          RB_X1 = 125,
          RB_X2 = RB_X1 + 40,
          RB_Y1 = LB_Y1,
          RB_Y2 = LB_Y2;

const int BUTTON_WIDTH = (LCD_WIDTH >> 2) - 4,
          BUTTON_HEIGHT = 40,
          BUTTON_Y_PADDING = 5,
          MINUS_X1 = (LCD_WIDTH >> 1),
          MINUS_X2 = MINUS_X1 + BUTTON_WIDTH,
          MINUS_SIGN_X1_OFF = 10,
          MINUS_SIGN_X2_OFF = MINUS_SIGN_X1_OFF + 20,
          MINUS_SIGN_Y1_OFF = 18,
          MINUS_SIGN_Y2_OFF = MINUS_SIGN_Y1_OFF + 5,
          PLUS_X1 = MINUS_X2 + 5,
          PLUS_X2 = LCD_WIDTH - 5,
          PLUS_SIGN_HOR_X1_OFF = MINUS_SIGN_X1_OFF - 1,
          PLUS_SIGN_HOR_X2_OFF = MINUS_SIGN_X2_OFF,
          PLUS_SIGN_HOR_Y1_OFF = MINUS_SIGN_Y1_OFF,
          PLUS_SIGN_HOR_Y2_OFF = MINUS_SIGN_Y2_OFF,
          PLUS_SIGN_VER_X1_OFF = PLUS_SIGN_HOR_X1_OFF + 8,
          PLUS_SIGN_VER_X2_OFF = PLUS_SIGN_VER_X1_OFF + 5,
          PLUS_SIGN_VER_Y1_OFF = 10,
          PLUS_SIGN_VER_Y2_OFF = PLUS_SIGN_VER_Y1_OFF + 21,
          LABEL_Y_PADDING = 12,
          VALUE_Y_OFF = 10,
          VALUE_X = X_MARGIN;

const COLOR MENU_BG_COLOR = WHITE;

/*
The parameter options is null-terminated (i.e. the last element should be NULL).
This function draws a whole screen with up to six vertical menu options from the 
parameter options.
*/
void draw_menu(const char* options[])
{
  LCDClear();

  LCDSetFontSize(2);
  LCDSetColor(BLACK, WHITE);

  for (int i = 0; i < MAX_OPTIONS; i++)
  {
    if (!options[i])
      break;
    
    int OPTION_Y1 = Y_MARGIN + i*(OPTION_HEIGHT + OPTION_PADDING);
    LCDArea(OPTION_X1, OPTION_Y1, OPTION_X1 + OPTION_WIDTH, OPTION_Y1 + OPTION_HEIGHT, MENU_BG_COLOR);
    LCDSetPrintf(OPTION_Y1 + 13, OPTION_X1 + 10, options[i]);
  }

  LCDSetFontSize(1);
  LCDArea(LB_X1, LB_Y1, LB_X2, LB_Y2, MENU_BG_COLOR);
  LCDSetPrintf(LB_Y1 + 4, LB_X1 + 5, "PREV");
  LCDArea(RB_X1, RB_Y1, RB_X2, RB_Y2, MENU_BG_COLOR);
  LCDSetPrintf(RB_Y1 + 4, RB_X1 + 5, "NEXT");

  LCDSetColor(WHITE, BLACK);
  LCDSetPrintf(LB_Y1 + 4, 75, "%d/%d ", gScreenIdx + 1, MAX_SCREENS);
}

/*
Returns the index of the option selected from the on-screen menu drawn by
the function draw_menu(). Also highlights the selected option on the screen 
in the equivalent index position within the parameter options.
*/
int selected_menu_option(const char* options[])
{
  int x, y;
  KEYReadXY(&x, &y);

  for (int i = 0; i < MAX_OPTIONS; i++)
  {
    if (!options[i])
      return -1;
    
    int OPTION_Y1 = Y_MARGIN + i*(OPTION_HEIGHT + OPTION_PADDING);
    int OPTION_Y2 = OPTION_Y1 + OPTION_HEIGHT;

    if (x >= OPTION_X1 && x <= OPTION_X2 && y >= OPTION_Y1 && y <= OPTION_Y2)
    {
      LCDSetFontSize(2);
      LCDSetColor(RED, WHITE);
      LCDSetPrintf(OPTION_Y1 + 13, OPTION_X1 + 10, options[i]);
      delay(INPUT_DELAY_MS);
      LCDSetColor(BLACK, WHITE);
      LCDSetPrintf(OPTION_Y1 + 13, OPTION_X1 + 10, options[i]);
      return i;
    }
  }

  return -1;
}

void setup() {
  EYEBOTInit();
}

/*
Demo which shows the camera feed from the ESP32-CAM with the current display frames-
per-second printed beneath it.
*/
void camera_demo()
{
  enum {
    COL_IMG,
    GRAY_IMG,
    NO_IMG
  } phase = COL_IMG;

  bool exit = false;

  const int TITLE_X = 5,
            TITLE_Y = 5,
            IMG_X = 5,
            IMG_Y = TITLE_Y + 25,
            FPS_X = 5,
            FPS_Y = IMG_Y + CAMHEIGHT + 10,
            PREV_X1 = 5,
            PREV_X1_OFF = 15,
            PREV_X2 = 81,
            PREV_Y1 = FPS_Y + 25,
            PREV_Y1_OFF = 25,
            PREV_Y2 = PREV_Y1 + 60,
            NEXT_X1 = PREV_X2 + 5,
            NEXT_X1_OFF = 15,
            NEXT_X2 = LCD_WIDTH - 5,
            NEXT_Y1 = PREV_Y1,
            NEXT_Y1_OFF = 25,
            NEXT_Y2 = PREV_Y2,
            EXIT_X1 = PREV_X1,
            EXIT_X1_OFF = 47,
            EXIT_X2 = NEXT_X2,
            EXIT_Y1 = PREV_Y2 + 5,
            EXIT_Y1_OFF = 21,
            EXIT_Y2 = LCD_HEIGHT - 5;

  LCDClear();
  LCDImageStart(IMG_X, IMG_Y, CAMWIDTH, CAMHEIGHT);

  LCDArea(PREV_X1, PREV_Y1, PREV_X2, PREV_Y2, WHITE);
  LCDArea(NEXT_X1, NEXT_Y1, NEXT_X2, NEXT_Y2, WHITE);
  LCDArea(EXIT_X1, EXIT_Y1, EXIT_X2, EXIT_Y2, WHITE);

  LCDSetFontSize(2);
  LCDSetColor(BLACK, WHITE);
  LCDSetPrintf(PREV_Y1 + PREV_Y1_OFF, PREV_X1 + PREV_X1_OFF, "PREV");
  LCDSetPrintf(NEXT_Y1 + NEXT_Y1_OFF, NEXT_X1 + NEXT_X1_OFF, "NEXT");
  LCDSetFontSize(3);
  LCDSetPrintf(EXIT_Y1 + EXIT_Y1_OFF, EXIT_X1 + EXIT_X1_OFF, "EXIT");

  while(!exit)
  {
    switch (phase)
    {
      case COL_IMG:
      {
        LCDSetFontSize(2);
        LCDSetColor(WHITE, BLACK);
        LCDSetPrintf(TITLE_Y, TITLE_X, "COLOUR IMG  ");

        unsigned long start_us = micros();
        CAMGet((BYTE*)gColImg);
        LCDImage((BYTE*)gColImg);
        unsigned long delta_us = micros() - start_us;

        LCDSetPrintf(FPS_Y, FPS_X, "FPS: %.1f", 1/(delta_us/1000000.0f));

        break;
      }
      case GRAY_IMG:
      {
        LCDSetFontSize(2);
        LCDSetColor(WHITE, BLACK);
        LCDSetPrintf(TITLE_Y, TITLE_X, "GRAY IMG     ");

        unsigned long start_us = micros();
        CAMGetGray(gGrayImg);
        LCDImageGray(gGrayImg);
        unsigned long delta_us = micros() - start_us;

        LCDSetPrintf(FPS_Y, FPS_X, "FPS: %.1f", 1/(delta_us/1000000.0f));

        break;
      }
      case NO_IMG:
      {
        LCDSetFontSize(2);
        LCDSetColor(WHITE, BLACK);
        LCDSetPrintf(TITLE_Y, TITLE_X, "NOT DISPLAYED");

        unsigned long start_us = micros();
        CAMGet((BYTE*)gColImg);
        unsigned long delta_us = micros() - start_us;

        LCDArea(IMG_X, IMG_Y, IMG_X + CAMWIDTH, IMG_Y + CAMHEIGHT, BLACK);

        LCDSetPrintf(FPS_Y, FPS_X, "FPS: %.1f", 1/(delta_us/1000000.0f));

        break;
      }
      default:
        phase = COL_IMG;
        break;
    }

    int x, y;
    KEYReadXY(&x, &y);

    if (x >= PREV_X1 && x <= PREV_X2 && y >= PREV_Y1 && y <= PREV_Y2)
    {
      LCDSetFontSize(2);
      LCDSetColor(RED, WHITE);
      LCDSetPrintf(PREV_Y1 + PREV_Y1_OFF, PREV_X1 + PREV_X1_OFF, "PREV");
      delay(INPUT_DELAY_MS);
      LCDSetColor(BLACK, WHITE);
      LCDSetPrintf(PREV_Y1 + PREV_Y1_OFF, PREV_X1 + PREV_X1_OFF, "PREV");
      
      switch (phase)
      {
        case COL_IMG:
          phase = NO_IMG;
          break;
        case GRAY_IMG:
          phase = COL_IMG;
          break;
        case NO_IMG:
          phase = GRAY_IMG;
          break;
        default:
          phase = COL_IMG;
          break;
      }
    }
    else if (x >= NEXT_X1 && x <= NEXT_X2 && y >= NEXT_Y1 && y <= NEXT_Y2)
    {
      LCDSetFontSize(2);
      LCDSetColor(RED, WHITE);
      LCDSetPrintf(NEXT_Y1 + NEXT_Y1_OFF, NEXT_X1 + NEXT_X1_OFF, "NEXT");
      delay(INPUT_DELAY_MS);
      LCDSetColor(BLACK, WHITE);
      LCDSetPrintf(NEXT_Y1 + NEXT_Y1_OFF, NEXT_X1 + NEXT_X1_OFF, "NEXT");

      switch (phase)
      {
        case COL_IMG:
          phase = GRAY_IMG;
          break;
        case GRAY_IMG:
          phase = NO_IMG;
          break;
        case NO_IMG:
          phase = COL_IMG;
          break;
        default:
          phase = COL_IMG;
          break;
      }
    }
    else if (x >= EXIT_X1 && x <= EXIT_X2 && y >= EXIT_Y1 && y <= EXIT_Y2)
    {
      LCDSetFontSize(3);
      LCDSetColor(RED, WHITE);
      LCDSetPrintf(EXIT_Y1 + EXIT_Y1_OFF, EXIT_X1 + EXIT_X1_OFF, "EXIT");
      delay(INPUT_DELAY_MS);
      exit = true;
    }
  }
}

/*
In this demo the user can specify the linear and angular speed for the
EyeBot, alongside offsets for the PWM values sent to the motors. When
the user select start, the EyeBot will drive indefinitely according to
these values.
*/
void vw_set_speed_demo()
{
  enum {
    PHASE_SETTINGS,
    PHASE_RUNNING,
    PHASE_EXIT
  } phase = PHASE_SETTINGS;

  bool exit = false;

  while (!exit)
  {
    switch (phase)
    {
      case PHASE_SETTINGS:
      {
        LCDClear();

        LCDSetFontSize(1);
        LCDSetColor(WHITE, BLACK);

        const int LIN_LABEL_X = X_MARGIN,
                  LIN_LABEL_Y = Y_MARGIN,
                  LIN_Y1 = LIN_LABEL_Y + LABEL_Y_PADDING,
                  LIN_Y2 = LIN_Y1 + BUTTON_HEIGHT;

        LCDSetPrintf(LIN_LABEL_Y, LIN_LABEL_X, "Lin. Speed (mm/s)");
        LCDArea(MINUS_X1, LIN_Y1, MINUS_X2, LIN_Y2, WHITE);
        LCDArea(MINUS_X1 + MINUS_SIGN_X1_OFF, LIN_Y1 + MINUS_SIGN_Y1_OFF, MINUS_X1 + MINUS_SIGN_X2_OFF, LIN_Y1 + MINUS_SIGN_Y2_OFF, BLACK);
        LCDArea(PLUS_X1, LIN_Y1, PLUS_X2, LIN_Y2, WHITE);
        LCDArea(PLUS_X1 + PLUS_SIGN_HOR_X1_OFF, LIN_Y1 + PLUS_SIGN_HOR_Y1_OFF, PLUS_X1 + PLUS_SIGN_HOR_X2_OFF, LIN_Y1 + PLUS_SIGN_HOR_Y2_OFF, BLACK);
        LCDArea(PLUS_X1 + PLUS_SIGN_VER_X1_OFF, LIN_Y1 + PLUS_SIGN_VER_Y1_OFF, PLUS_X1 + PLUS_SIGN_VER_X2_OFF, LIN_Y1 + PLUS_SIGN_VER_Y2_OFF, BLACK);

        const int ANG_LABEL_X = X_MARGIN,
                  ANG_LABEL_Y = LIN_Y2 + BUTTON_Y_PADDING,
                  ANG_Y1 = ANG_LABEL_Y + LABEL_Y_PADDING,
                  ANG_Y2 = ANG_Y1 + BUTTON_HEIGHT;

        LCDSetPrintf(ANG_LABEL_Y, ANG_LABEL_X, "Ang. Speed (deg./s)");
        LCDArea(MINUS_X1, ANG_Y1, MINUS_X2, ANG_Y2, WHITE);
        LCDArea(MINUS_X1 + MINUS_SIGN_X1_OFF, ANG_Y1 + MINUS_SIGN_Y1_OFF, MINUS_X1 + MINUS_SIGN_X2_OFF, ANG_Y1 + MINUS_SIGN_Y2_OFF, BLACK);
        LCDArea(PLUS_X1, ANG_Y1, PLUS_X2, ANG_Y2, WHITE);
        LCDArea(PLUS_X1 + PLUS_SIGN_HOR_X1_OFF, ANG_Y1 + PLUS_SIGN_HOR_Y1_OFF, PLUS_X1 + PLUS_SIGN_HOR_X2_OFF, ANG_Y1 + PLUS_SIGN_HOR_Y2_OFF, BLACK);
        LCDArea(PLUS_X1 + PLUS_SIGN_VER_X1_OFF, ANG_Y1 + PLUS_SIGN_VER_Y1_OFF, PLUS_X1 + PLUS_SIGN_VER_X2_OFF, ANG_Y1 + PLUS_SIGN_VER_Y2_OFF, BLACK);

        const int LEFT_LABEL_X = X_MARGIN,
                  LEFT_LABEL_Y = ANG_Y2 + BUTTON_Y_PADDING,
                  LEFT_Y1 = LEFT_LABEL_Y + LABEL_Y_PADDING,
                  LEFT_Y2 = LEFT_Y1 + BUTTON_HEIGHT;
        
        LCDSetPrintf(LEFT_LABEL_Y, LEFT_LABEL_X, "Left Motor Offset");
        LCDArea(MINUS_X1, LEFT_Y1, MINUS_X2, LEFT_Y2, WHITE);
        LCDArea(MINUS_X1 + MINUS_SIGN_X1_OFF, LEFT_Y1 + MINUS_SIGN_Y1_OFF, MINUS_X1 + MINUS_SIGN_X2_OFF, LEFT_Y1 + MINUS_SIGN_Y2_OFF, BLACK);
        LCDArea(PLUS_X1, LEFT_Y1, PLUS_X2, LEFT_Y2, WHITE);
        LCDArea(PLUS_X1 + PLUS_SIGN_HOR_X1_OFF, LEFT_Y1 + PLUS_SIGN_HOR_Y1_OFF, PLUS_X1 + PLUS_SIGN_HOR_X2_OFF, LEFT_Y1 + PLUS_SIGN_HOR_Y2_OFF, BLACK);
        LCDArea(PLUS_X1 + PLUS_SIGN_VER_X1_OFF, LEFT_Y1 + PLUS_SIGN_VER_Y1_OFF, PLUS_X1 + PLUS_SIGN_VER_X2_OFF, LEFT_Y1 + PLUS_SIGN_VER_Y2_OFF, BLACK);

        const int RIGHT_LABEL_X = X_MARGIN,
                  RIGHT_LABEL_Y = LEFT_Y2 + BUTTON_Y_PADDING,
                  RIGHT_Y1 = RIGHT_LABEL_Y + LABEL_Y_PADDING,
                  RIGHT_Y2 = RIGHT_Y1 + BUTTON_HEIGHT;
        
        LCDSetPrintf(RIGHT_LABEL_Y, RIGHT_LABEL_X, "Right Motor Offset");
        LCDArea(MINUS_X1, RIGHT_Y1, MINUS_X2, RIGHT_Y2, WHITE);
        LCDArea(MINUS_X1 + MINUS_SIGN_X1_OFF, RIGHT_Y1 + MINUS_SIGN_Y1_OFF, MINUS_X1 + MINUS_SIGN_X2_OFF, RIGHT_Y1 + MINUS_SIGN_Y2_OFF, BLACK);
        LCDArea(PLUS_X1, RIGHT_Y1, PLUS_X2, RIGHT_Y2, WHITE);
        LCDArea(PLUS_X1 + PLUS_SIGN_HOR_X1_OFF, RIGHT_Y1 + PLUS_SIGN_HOR_Y1_OFF, PLUS_X1 + PLUS_SIGN_HOR_X2_OFF, RIGHT_Y1 + PLUS_SIGN_HOR_Y2_OFF, BLACK);
        LCDArea(PLUS_X1 + PLUS_SIGN_VER_X1_OFF, RIGHT_Y1 + PLUS_SIGN_VER_Y1_OFF, PLUS_X1 + PLUS_SIGN_VER_X2_OFF, RIGHT_Y1 + PLUS_SIGN_VER_Y2_OFF, BLACK);

        LCDSetFontSize(3);
        LCDSetColor(BLACK, WHITE);

        const int START_X1 = X_MARGIN,
                  START_X2 = LCD_WIDTH - 5,
                  START_Y1 = RIGHT_Y2 + 5,
                  START_Y2 = START_Y1 + BUTTON_HEIGHT;
        
        LCDArea(START_X1, START_Y1, START_X2, START_Y2, WHITE);
        LCDSetPrintf(START_Y1 + 9, START_X1 + 40, "START");

        const int EXIT_X1 = START_X1,
                  EXIT_X2 = START_X2,
                  EXIT_Y1 = START_Y2 + 5,
                  EXIT_Y2 = EXIT_Y1 + BUTTON_HEIGHT;

        LCDArea(EXIT_X1, EXIT_Y1, EXIT_X2, EXIT_Y2, WHITE);
        LCDSetPrintf(EXIT_Y1 + 9, EXIT_X1 + 47, "EXIT");

        while(phase == PHASE_SETTINGS)
        {
          LCDSetFontSize(2);
          LCDSetColor(WHITE, BLACK);

          LCDSetPrintf(LIN_Y1 + VALUE_Y_OFF, VALUE_X, "%d  ", gLinSpeed);
          LCDSetPrintf(ANG_Y1 + VALUE_Y_OFF, VALUE_X, "%d  ", gAngSpeed);
          LCDSetPrintf(LEFT_Y1 + VALUE_Y_OFF, VALUE_X, "%d  ", gLeftMotorOffset);
          LCDSetPrintf(RIGHT_Y1 + VALUE_Y_OFF, VALUE_X, "%d  ", gRightMotorOffset);

          int x, y;
          KEYReadXY(&x, &y);

          if (x >= VALUE_X && x <= MINUS_X1 - 5)
          {
            if (y >= LIN_Y1 && y <= LIN_Y2)
            {
              gLinSpeed *= -1;
              delay(INPUT_DELAY_MS);
            }
            else if (y >= ANG_Y1 && y <= ANG_Y2)
            {
              gAngSpeed *= -1;
              delay(INPUT_DELAY_MS);
            }
            else if (y >= LEFT_Y1 && y <= LEFT_Y2)
            {
              gLeftMotorOffset *= -1;
              delay(INPUT_DELAY_MS);
            }
            else if (y >= RIGHT_Y1 && y <= RIGHT_Y2)
            {
              gRightMotorOffset *= -1;
              delay(INPUT_DELAY_MS);
            }
          }
          else if (x >= MINUS_X1 && x <= MINUS_X2)
          {
            if (y >= LIN_Y1 && y <= LIN_Y2)
            {
              gLinSpeed -= 10;
              delay(INPUT_DELAY_MS);
            }
            else if (y >= ANG_Y1 && y <= ANG_Y2)
            {
              gAngSpeed -= 10;
              delay(INPUT_DELAY_MS);
            }
            else if (y >= LEFT_Y1 && y <= LEFT_Y2)
            {
              gLeftMotorOffset--;
              if (gLeftMotorOffset < -255) gLeftMotorOffset = -255;
              delay(INPUT_DELAY_MS);
            }
            else if (y >= RIGHT_Y1 && y <= RIGHT_Y2)
            {
              gRightMotorOffset--;
              if (gRightMotorOffset < -255) gRightMotorOffset = -255;
              delay(INPUT_DELAY_MS);
            }
          }
          else if (x >= PLUS_X1 && x <= PLUS_X2)
          {
            if (y >= LIN_Y1 && y <= LIN_Y2)
            {
              gLinSpeed += 10;
              delay(INPUT_DELAY_MS);
            }
            else if (y >= ANG_Y1 && y <= ANG_Y2)
            {
              gAngSpeed += 10;
              delay(INPUT_DELAY_MS);
            }
            else if (y >= LEFT_Y1 && y <= LEFT_Y2)
            {
              gLeftMotorOffset++;
              if (gLeftMotorOffset > 255) gLeftMotorOffset = 255;
              delay(INPUT_DELAY_MS);
            }
            else if (y >= RIGHT_Y1 && y <= RIGHT_Y2)
            {
              gRightMotorOffset++;
              if (gRightMotorOffset > 255) gRightMotorOffset = 255;
              delay(INPUT_DELAY_MS);
            }
          }

          if (x >= START_X1 && x <= START_X2 && y >= START_Y1 && y <= START_Y2)
          {
            LCDSetFontSize(3);
            LCDSetColor(RED, WHITE);
            LCDSetPrintf(START_Y1 + 9, START_X1 + 40, "START");
            delay(INPUT_DELAY_MS);
            phase = PHASE_RUNNING;
          }
          
          if (x >= EXIT_X1 && x <= EXIT_X2 && y >= EXIT_Y1 && y <= EXIT_Y2)
          {
            LCDSetFontSize(3);
            LCDSetColor(RED, WHITE);
            LCDSetPrintf(EXIT_Y1 + 9, EXIT_X1 + 47, "EXIT");
            delay(INPUT_DELAY_MS);
            phase = PHASE_EXIT;
          }

          int pressed = KEYRead();
          if (pressed & KEY2)// Right button pressed
          {
            VWSetPosition(0, 0, 0);
          }
        }

        break;
      }
      case PHASE_RUNNING:
      {
        LCDClear();

        LCDArea(0, 0, LCD_WIDTH, LCD_HEIGHT, RED);

        const int Y_PADDING = 40,
                  XPOS_Y = Y_MARGIN,
                  YPOS_Y = XPOS_Y + Y_PADDING,
                  ANGLE_Y = YPOS_Y + Y_PADDING;
        
        LCDSetFontSize(4);
        LCDSetColor(WHITE, RED);
        LCDSetPrintf(LCD_HEIGHT - 45, 25, "RESET");

        LCDSetFontSize(2);
        LCDSetColor(WHITE, RED);
        LCDSetPrintf(XPOS_Y, X_MARGIN, "X POS:");
        LCDSetPrintf(YPOS_Y, X_MARGIN, "Y POS:");
        LCDSetPrintf(ANGLE_Y, X_MARGIN, "ANGLE:");

        if (int err = VWSetSpeed(gLinSpeed, gAngSpeed))
        {
          phase = PHASE_SETTINGS;
          break;
        }

        while (phase == PHASE_RUNNING)
        {
          int xpos, ypos, angle;
          VWGetPosition(&xpos, &ypos, &angle);

          LCDSetPrintf(XPOS_Y + 20, X_MARGIN, "%d mm    ", xpos);
          LCDSetPrintf(YPOS_Y + 20, X_MARGIN, "%d mm    ", ypos);
          LCDSetPrintf(ANGLE_Y + 20, X_MARGIN, "%d deg     ", angle);

          int x, y;
          KEYReadXY(&x, &y);

          if (x >= 0 || y >= 0)
          {
            VWSetSpeed(0, 0);
            delay(INPUT_DELAY_MS);
            phase = PHASE_SETTINGS;
          }
        }

        break;
      }
      default:
        exit = true;
        break;
    }
  }

}

/*
In this demo the user can specify the absolute linear speed and
the change in distance they want the EyeBot to travel in a straight line.
*/
void vw_straight_demo()
{
  enum {
    PHASE_SETTINGS,
    PHASE_RUNNING,
    PHASE_EXIT
  } phase = PHASE_SETTINGS;

  bool exit = false;

  while (!exit)
  {
    switch (phase)
    {
      case PHASE_SETTINGS:
      {
        LCDClear();

        LCDSetFontSize(1);
        LCDSetColor(WHITE, BLACK);

        const int LIN_LABEL_X = X_MARGIN,
                  LIN_LABEL_Y = Y_MARGIN,
                  LIN_Y1 = LIN_LABEL_Y + LABEL_Y_PADDING,
                  LIN_Y2 = LIN_Y1 + BUTTON_HEIGHT;

        LCDSetPrintf(LIN_LABEL_Y, LIN_LABEL_X, "Lin. Speed (mm/s)");
        LCDArea(MINUS_X1, LIN_Y1, MINUS_X2, LIN_Y2, WHITE);
        LCDArea(MINUS_X1 + MINUS_SIGN_X1_OFF, LIN_Y1 + MINUS_SIGN_Y1_OFF, MINUS_X1 + MINUS_SIGN_X2_OFF, LIN_Y1 + MINUS_SIGN_Y2_OFF, BLACK);
        LCDArea(PLUS_X1, LIN_Y1, PLUS_X2, LIN_Y2, WHITE);
        LCDArea(PLUS_X1 + PLUS_SIGN_HOR_X1_OFF, LIN_Y1 + PLUS_SIGN_HOR_Y1_OFF, PLUS_X1 + PLUS_SIGN_HOR_X2_OFF, LIN_Y1 + PLUS_SIGN_HOR_Y2_OFF, BLACK);
        LCDArea(PLUS_X1 + PLUS_SIGN_VER_X1_OFF, LIN_Y1 + PLUS_SIGN_VER_Y1_OFF, PLUS_X1 + PLUS_SIGN_VER_X2_OFF, LIN_Y1 + PLUS_SIGN_VER_Y2_OFF, BLACK);

        const int DIST_LABEL_X = X_MARGIN,
                  DIST_LABEL_Y = LIN_Y2 + BUTTON_Y_PADDING,
                  DIST_Y1 = DIST_LABEL_Y + LABEL_Y_PADDING,
                  DIST_Y2 = DIST_Y1 + BUTTON_HEIGHT;

        LCDSetPrintf(DIST_LABEL_Y, DIST_LABEL_X, "Distance (mm)");
        LCDArea(MINUS_X1, DIST_Y1, MINUS_X2, DIST_Y2, WHITE);
        LCDArea(MINUS_X1 + MINUS_SIGN_X1_OFF, DIST_Y1 + MINUS_SIGN_Y1_OFF, MINUS_X1 + MINUS_SIGN_X2_OFF, DIST_Y1 + MINUS_SIGN_Y2_OFF, BLACK);
        LCDArea(PLUS_X1, DIST_Y1, PLUS_X2, DIST_Y2, WHITE);
        LCDArea(PLUS_X1 + PLUS_SIGN_HOR_X1_OFF, DIST_Y1 + PLUS_SIGN_HOR_Y1_OFF, PLUS_X1 + PLUS_SIGN_HOR_X2_OFF, DIST_Y1 + PLUS_SIGN_HOR_Y2_OFF, BLACK);
        LCDArea(PLUS_X1 + PLUS_SIGN_VER_X1_OFF, DIST_Y1 + PLUS_SIGN_VER_Y1_OFF, PLUS_X1 + PLUS_SIGN_VER_X2_OFF, DIST_Y1 + PLUS_SIGN_VER_Y2_OFF, BLACK);

        LCDSetFontSize(3);
        LCDSetColor(BLACK, WHITE);

        const int START_X1 = X_MARGIN,
                  START_X2 = LCD_WIDTH - 5,
                  START_Y1 = DIST_Y2 + 5,
                  START_Y2 = START_Y1 + BUTTON_HEIGHT;
        
        LCDArea(START_X1, START_Y1, START_X2, START_Y2, WHITE);
        LCDSetPrintf(START_Y1 + 9, START_X1 + 40, "START");

        const int EXIT_X1 = START_X1,
                  EXIT_X2 = START_X2,
                  EXIT_Y1 = START_Y2 + 5,
                  EXIT_Y2 = EXIT_Y1 + BUTTON_HEIGHT;

        LCDArea(EXIT_X1, EXIT_Y1, EXIT_X2, EXIT_Y2, WHITE);
        LCDSetPrintf(EXIT_Y1 + 9, EXIT_X1 + 47, "EXIT");

        while (phase == PHASE_SETTINGS)
        {
          LCDSetFontSize(2);
          LCDSetColor(WHITE, BLACK);

          LCDSetPrintf(LIN_Y1 + VALUE_Y_OFF, VALUE_X, "%d  ", gStraightLinSpeed);
          LCDSetPrintf(DIST_Y1 + VALUE_Y_OFF, VALUE_X, "%d  ", gStraightDist);

          int x, y;
          KEYReadXY(&x, &y);

          if (x >= VALUE_X && x <= MINUS_X1 - 5)
          {
            if (y >= DIST_Y1 && y <= DIST_Y2)
            {
              gStraightDist *= -1;
              delay(INPUT_DELAY_MS);
            }
          }
          else if (x >= MINUS_X1 && x <= MINUS_X2)
          {
            if (y >= LIN_Y1 && y <= LIN_Y2)
            {
              gStraightLinSpeed -= 10;
              if (gStraightLinSpeed <= 0) gStraightLinSpeed = 10;
              delay(INPUT_DELAY_MS);
            }
            else if (y >= DIST_Y1 && y <= DIST_Y2)
            {
              gStraightDist -= 10;
              delay(INPUT_DELAY_MS);
            }
          }
          else if (x >= PLUS_X1 && x <= PLUS_X2)
          {
            if (y >= LIN_Y1 && y <= LIN_Y2)
            {
              gStraightLinSpeed += 10;
              delay(INPUT_DELAY_MS);
            }
            else if (y >= DIST_Y1 && y <= DIST_Y2)
            {
              gStraightDist += 10;
              delay(INPUT_DELAY_MS);
            }
          }

          if (x >= START_X1 && x <= START_X2 && y >= START_Y1 && y <= START_Y2)
          {
            LCDSetFontSize(3);
            LCDSetColor(RED, WHITE);
            LCDSetPrintf(START_Y1 + 9, START_X1 + 40, "START");
            delay(INPUT_DELAY_MS);
            phase = PHASE_RUNNING;
          }
          
          if (x >= EXIT_X1 && x <= EXIT_X2 && y >= EXIT_Y1 && y <= EXIT_Y2)
          {
            LCDSetFontSize(3);
            LCDSetColor(RED, WHITE);
            LCDSetPrintf(EXIT_Y1 + 9, EXIT_X1 + 47, "EXIT");
            delay(INPUT_DELAY_MS);
            phase = PHASE_EXIT;
          }
        }

        break;
      }
      case PHASE_RUNNING:
      {
        LCDClear();
        LCDArea(0, 0, LCD_WIDTH, LCD_HEIGHT, RED);

        const int Y_PADDING = 40,
                  XPOS_Y = Y_MARGIN,
                  YPOS_Y = XPOS_Y + Y_PADDING,
                  ANGLE_Y = YPOS_Y + Y_PADDING,
                  REMAIN_Y = ANGLE_Y + Y_PADDING;
        
        LCDSetFontSize(4);
        LCDSetColor(WHITE, RED);
        LCDSetPrintf(LCD_HEIGHT - 45, 25, "RESET");

        LCDSetFontSize(2);
        LCDSetColor(WHITE, RED);
        LCDSetPrintf(XPOS_Y, X_MARGIN, "X POS:");
        LCDSetPrintf(YPOS_Y, X_MARGIN, "Y POS:");
        LCDSetPrintf(ANGLE_Y, X_MARGIN, "ANGLE:");
        LCDSetPrintf(REMAIN_Y, X_MARGIN, "REMAINING:");

        if (int err = VWStraight(gStraightDist, gStraightLinSpeed))
        {
          phase = PHASE_SETTINGS;
          break;
        }

        while (phase == PHASE_RUNNING)
        {
          int xpos, ypos, angle;
          VWGetPosition(&xpos, &ypos, &angle);

          int dist = VWRemain();

          LCDSetPrintf(XPOS_Y + 20, X_MARGIN, "%d mm     ", xpos);
          LCDSetPrintf(YPOS_Y + 20, X_MARGIN, "%d mm     ", ypos);
          LCDSetPrintf(ANGLE_Y + 20, X_MARGIN, "%d deg    ", angle);
          LCDSetPrintf(REMAIN_Y + 20, X_MARGIN, "%d mm     ", dist < 0 ? 0 : dist);

          int x, y;
          KEYReadXY(&x, &y);

          if (x >= 0 || y >= 0)
          {
            VWSetSpeed(0, 0);
            delay(INPUT_DELAY_MS);
            phase = PHASE_SETTINGS;
          }
        }

        break;
      }
      case PHASE_EXIT:
        exit = true;
        break;
    }
  }
}

/*
In this demo the user specifies the absolute angular speed and
the change in the angular orientation of the EyeBot for when
it turns on the spot.
*/
void vw_turn_demo()
{
  enum {
    PHASE_SETTINGS,
    PHASE_RUNNING,
    PHASE_EXIT
  } phase = PHASE_SETTINGS;

  bool exit = false;

  while (!exit)
  {
    switch (phase)
    {
      case PHASE_SETTINGS:
      {
        LCDClear();

        LCDSetFontSize(1);
        LCDSetColor(WHITE, BLACK);

        const int ANG_LABEL_X = X_MARGIN,
                  ANG_LABEL_Y = Y_MARGIN,
                  ANG_Y1 = ANG_LABEL_Y + LABEL_Y_PADDING,
                  ANG_Y2 = ANG_Y1 + BUTTON_HEIGHT;

        LCDSetPrintf(ANG_LABEL_Y, ANG_LABEL_X, "Ang. Speed (mm/s)");
        LCDArea(MINUS_X1, ANG_Y1, MINUS_X2, ANG_Y2, WHITE);
        LCDArea(MINUS_X1 + MINUS_SIGN_X1_OFF, ANG_Y1 + MINUS_SIGN_Y1_OFF, MINUS_X1 + MINUS_SIGN_X2_OFF, ANG_Y1 + MINUS_SIGN_Y2_OFF, BLACK);
        LCDArea(PLUS_X1, ANG_Y1, PLUS_X2, ANG_Y2, WHITE);
        LCDArea(PLUS_X1 + PLUS_SIGN_HOR_X1_OFF, ANG_Y1 + PLUS_SIGN_HOR_Y1_OFF, PLUS_X1 + PLUS_SIGN_HOR_X2_OFF, ANG_Y1 + PLUS_SIGN_HOR_Y2_OFF, BLACK);
        LCDArea(PLUS_X1 + PLUS_SIGN_VER_X1_OFF, ANG_Y1 + PLUS_SIGN_VER_Y1_OFF, PLUS_X1 + PLUS_SIGN_VER_X2_OFF, ANG_Y1 + PLUS_SIGN_VER_Y2_OFF, BLACK);

        const int FINANG_LABEL_X = X_MARGIN,
                  FINANG_LABEL_Y = ANG_Y2 + BUTTON_Y_PADDING,
                  FINANG_Y1 = FINANG_LABEL_Y + LABEL_Y_PADDING,
                  FINANG_Y2 = FINANG_Y1 + BUTTON_HEIGHT;

        LCDSetPrintf(FINANG_LABEL_Y, FINANG_LABEL_X, "Final Ang. (deg)");
        LCDArea(MINUS_X1, FINANG_Y1, MINUS_X2, FINANG_Y2, WHITE);
        LCDArea(MINUS_X1 + MINUS_SIGN_X1_OFF, FINANG_Y1 + MINUS_SIGN_Y1_OFF, MINUS_X1 + MINUS_SIGN_X2_OFF, FINANG_Y1 + MINUS_SIGN_Y2_OFF, BLACK);
        LCDArea(PLUS_X1, FINANG_Y1, PLUS_X2, FINANG_Y2, WHITE);
        LCDArea(PLUS_X1 + PLUS_SIGN_HOR_X1_OFF, FINANG_Y1 + PLUS_SIGN_HOR_Y1_OFF, PLUS_X1 + PLUS_SIGN_HOR_X2_OFF, FINANG_Y1 + PLUS_SIGN_HOR_Y2_OFF, BLACK);
        LCDArea(PLUS_X1 + PLUS_SIGN_VER_X1_OFF, FINANG_Y1 + PLUS_SIGN_VER_Y1_OFF, PLUS_X1 + PLUS_SIGN_VER_X2_OFF, FINANG_Y1 + PLUS_SIGN_VER_Y2_OFF, BLACK);

        LCDSetFontSize(3);
        LCDSetColor(BLACK, WHITE);

        const int START_X1 = X_MARGIN,
                  START_X2 = LCD_WIDTH - 5,
                  START_Y1 = FINANG_Y2 + 5,
                  START_Y2 = START_Y1 + BUTTON_HEIGHT;
        
        LCDArea(START_X1, START_Y1, START_X2, START_Y2, WHITE);
        LCDSetPrintf(START_Y1 + 9, START_X1 + 40, "START");

        const int EXIT_X1 = START_X1,
                  EXIT_X2 = START_X2,
                  EXIT_Y1 = START_Y2 + 5,
                  EXIT_Y2 = EXIT_Y1 + BUTTON_HEIGHT;

        LCDArea(EXIT_X1, EXIT_Y1, EXIT_X2, EXIT_Y2, WHITE);
        LCDSetPrintf(EXIT_Y1 + 9, EXIT_X1 + 47, "EXIT");

        while (phase == PHASE_SETTINGS)
        {
          LCDSetFontSize(2);
          LCDSetColor(WHITE, BLACK);

          LCDSetPrintf(ANG_Y1 + VALUE_Y_OFF, VALUE_X, "%d  ", gTurnAngSpeed);
          LCDSetPrintf(FINANG_Y1 + VALUE_Y_OFF, VALUE_X, "%d  ", gTurnFinalAng);

          int x, y;
          KEYReadXY(&x, &y);

          if (x >= VALUE_X && x <= MINUS_X1 - 5)
          {
            if (y >= FINANG_Y1 && y <= FINANG_Y2)
            {
              gTurnFinalAng *= -1;
              delay(INPUT_DELAY_MS);
            }
          }
          else if (x >= MINUS_X1 && x <= MINUS_X2)
          {
            if (y >= ANG_Y1 && y <= ANG_Y2)
            {
              gTurnAngSpeed -= 10;
              if (gTurnAngSpeed <= 0) gTurnAngSpeed = 10;
              delay(INPUT_DELAY_MS);
            }
            else if (y >= FINANG_Y1 && y <= FINANG_Y2)
            {
              gTurnFinalAng -= 10;
              delay(INPUT_DELAY_MS);
            }
          }
          else if (x >= PLUS_X1 && x <= PLUS_X2)
          {
            if (y >= ANG_Y1 && y <= ANG_Y2)
            {
              gTurnAngSpeed += 10;
              delay(INPUT_DELAY_MS);
            }
            else if (y >= FINANG_Y1 && y <= FINANG_Y2)
            {
              gTurnFinalAng += 10;
              delay(INPUT_DELAY_MS);
            }
          }

          if (x >= START_X1 && x <= START_X2 && y >= START_Y1 && y <= START_Y2)
          {
            LCDSetFontSize(3);
            LCDSetColor(RED, WHITE);
            LCDSetPrintf(START_Y1 + 9, START_X1 + 40, "START");
            delay(INPUT_DELAY_MS);
            phase = PHASE_RUNNING;
          }
          
          if (x >= EXIT_X1 && x <= EXIT_X2 && y >= EXIT_Y1 && y <= EXIT_Y2)
          {
            LCDSetFontSize(3);
            LCDSetColor(RED, WHITE);
            LCDSetPrintf(EXIT_Y1 + 9, EXIT_X1 + 47, "EXIT");
            delay(INPUT_DELAY_MS);
            phase = PHASE_EXIT;
          }
        }

        break;
      }
      case PHASE_RUNNING:
      {
        LCDClear();
        LCDArea(0, 0, LCD_WIDTH, LCD_HEIGHT, RED);

        const int Y_PADDING = 40,
                  XPOS_Y = Y_MARGIN,
                  YPOS_Y = XPOS_Y + Y_PADDING,
                  ANGLE_Y = YPOS_Y + Y_PADDING;
        
        LCDSetFontSize(4);
        LCDSetColor(WHITE, RED);
        LCDSetPrintf(LCD_HEIGHT - 45, 25, "RESET");

        LCDSetFontSize(2);
        LCDSetColor(WHITE, RED);
        LCDSetPrintf(XPOS_Y, X_MARGIN, "X POS:");
        LCDSetPrintf(YPOS_Y, X_MARGIN, "Y POS:");
        LCDSetPrintf(ANGLE_Y, X_MARGIN, "ANGLE:");

        if (int err = VWTurn(gTurnFinalAng, gTurnAngSpeed))
        {
          phase = PHASE_SETTINGS;
          break;
        }

        while (phase == PHASE_RUNNING)
        {
          int xpos, ypos, angle;
          VWGetPosition(&xpos, &ypos, &angle);

          LCDSetPrintf(XPOS_Y + 20, X_MARGIN, "%d mm     ", xpos);
          LCDSetPrintf(YPOS_Y + 20, X_MARGIN, "%d mm     ", ypos);
          LCDSetPrintf(ANGLE_Y + 20, X_MARGIN, "%d deg      ", angle);

          int x, y;
          KEYReadXY(&x, &y);

          if (x >= 0 || y >= 0)
          {
            VWSetSpeed(0, 0);
            delay(INPUT_DELAY_MS);
            phase = PHASE_SETTINGS;
          }
        }

        break;
      }
      case PHASE_EXIT:
        exit = true;
        break;
    }
  }
}

/*
In this demo the user specifies the absolute linear speed, the
relative angular speed, and the change in distance for when the EyeBot
drives along a curved path for the set distance.
*/
void vw_curve_demo()
{
  enum {
    PHASE_SETTINGS,
    PHASE_RUNNING,
    PHASE_EXIT
  } phase = PHASE_SETTINGS;

  bool exit = false;

  while (!exit)
  {
    switch (phase)
    {
      case PHASE_SETTINGS:
      {
        LCDClear();

        LCDSetFontSize(1);
        LCDSetColor(WHITE, BLACK);

        const int LIN_LABEL_X = X_MARGIN,
                  LIN_LABEL_Y = Y_MARGIN,
                  LIN_Y1 = LIN_LABEL_Y + LABEL_Y_PADDING,
                  LIN_Y2 = LIN_Y1 + BUTTON_HEIGHT;

        LCDSetPrintf(LIN_LABEL_Y, LIN_LABEL_X, "Lin. Speed (mm/s)");
        LCDArea(MINUS_X1, LIN_Y1, MINUS_X2, LIN_Y2, WHITE);
        LCDArea(MINUS_X1 + MINUS_SIGN_X1_OFF, LIN_Y1 + MINUS_SIGN_Y1_OFF, MINUS_X1 + MINUS_SIGN_X2_OFF, LIN_Y1 + MINUS_SIGN_Y2_OFF, BLACK);
        LCDArea(PLUS_X1, LIN_Y1, PLUS_X2, LIN_Y2, WHITE);
        LCDArea(PLUS_X1 + PLUS_SIGN_HOR_X1_OFF, LIN_Y1 + PLUS_SIGN_HOR_Y1_OFF, PLUS_X1 + PLUS_SIGN_HOR_X2_OFF, LIN_Y1 + PLUS_SIGN_HOR_Y2_OFF, BLACK);
        LCDArea(PLUS_X1 + PLUS_SIGN_VER_X1_OFF, LIN_Y1 + PLUS_SIGN_VER_Y1_OFF, PLUS_X1 + PLUS_SIGN_VER_X2_OFF, LIN_Y1 + PLUS_SIGN_VER_Y2_OFF, BLACK);

        const int ANG_LABEL_X = X_MARGIN,
                  ANG_LABEL_Y = LIN_Y2 + BUTTON_Y_PADDING,
                  ANG_Y1 = ANG_LABEL_Y + LABEL_Y_PADDING,
                  ANG_Y2 = ANG_Y1 + BUTTON_HEIGHT;

        LCDSetPrintf(ANG_LABEL_Y, ANG_LABEL_X, "Final Ang. (deg.)");
        LCDArea(MINUS_X1, ANG_Y1, MINUS_X2, ANG_Y2, WHITE);
        LCDArea(MINUS_X1 + MINUS_SIGN_X1_OFF, ANG_Y1 + MINUS_SIGN_Y1_OFF, MINUS_X1 + MINUS_SIGN_X2_OFF, ANG_Y1 + MINUS_SIGN_Y2_OFF, BLACK);
        LCDArea(PLUS_X1, ANG_Y1, PLUS_X2, ANG_Y2, WHITE);
        LCDArea(PLUS_X1 + PLUS_SIGN_HOR_X1_OFF, ANG_Y1 + PLUS_SIGN_HOR_Y1_OFF, PLUS_X1 + PLUS_SIGN_HOR_X2_OFF, ANG_Y1 + PLUS_SIGN_HOR_Y2_OFF, BLACK);
        LCDArea(PLUS_X1 + PLUS_SIGN_VER_X1_OFF, ANG_Y1 + PLUS_SIGN_VER_Y1_OFF, PLUS_X1 + PLUS_SIGN_VER_X2_OFF, ANG_Y1 + PLUS_SIGN_VER_Y2_OFF, BLACK);

        const int DIST_LABEL_X = X_MARGIN,
                  DIST_LABEL_Y = ANG_Y2 + BUTTON_Y_PADDING,
                  DIST_Y1 = DIST_LABEL_Y + LABEL_Y_PADDING,
                  DIST_Y2 = DIST_Y1 + BUTTON_HEIGHT;

        LCDSetPrintf(DIST_LABEL_Y, DIST_LABEL_X, "Distance (mm)");
        LCDArea(MINUS_X1, DIST_Y1, MINUS_X2, DIST_Y2, WHITE);
        LCDArea(MINUS_X1 + MINUS_SIGN_X1_OFF, DIST_Y1 + MINUS_SIGN_Y1_OFF, MINUS_X1 + MINUS_SIGN_X2_OFF, DIST_Y1 + MINUS_SIGN_Y2_OFF, BLACK);
        LCDArea(PLUS_X1, DIST_Y1, PLUS_X2, DIST_Y2, WHITE);
        LCDArea(PLUS_X1 + PLUS_SIGN_HOR_X1_OFF, DIST_Y1 + PLUS_SIGN_HOR_Y1_OFF, PLUS_X1 + PLUS_SIGN_HOR_X2_OFF, DIST_Y1 + PLUS_SIGN_HOR_Y2_OFF, BLACK);
        LCDArea(PLUS_X1 + PLUS_SIGN_VER_X1_OFF, DIST_Y1 + PLUS_SIGN_VER_Y1_OFF, PLUS_X1 + PLUS_SIGN_VER_X2_OFF, DIST_Y1 + PLUS_SIGN_VER_Y2_OFF, BLACK);

        LCDSetFontSize(3);
        LCDSetColor(BLACK, WHITE);

        const int START_X1 = X_MARGIN,
                  START_X2 = LCD_WIDTH - 5,
                  START_Y1 = DIST_Y2 + 5,
                  START_Y2 = START_Y1 + BUTTON_HEIGHT;
        
        LCDArea(START_X1, START_Y1, START_X2, START_Y2, WHITE);
        LCDSetPrintf(START_Y1 + 9, START_X1 + 40, "START");

        const int EXIT_X1 = START_X1,
                  EXIT_X2 = START_X2,
                  EXIT_Y1 = START_Y2 + 5,
                  EXIT_Y2 = EXIT_Y1 + BUTTON_HEIGHT;

        LCDArea(EXIT_X1, EXIT_Y1, EXIT_X2, EXIT_Y2, WHITE);
        LCDSetPrintf(EXIT_Y1 + 9, EXIT_X1 + 47, "EXIT");

        while (phase == PHASE_SETTINGS)
        {
          LCDSetFontSize(2);
          LCDSetColor(WHITE, BLACK);

          LCDSetPrintf(LIN_Y1 + VALUE_Y_OFF, VALUE_X, "%d  ", gCurveLinSpeed);
          LCDSetPrintf(ANG_Y1 + VALUE_Y_OFF, VALUE_X, "%d  ", gCurveFinalAng);
          LCDSetPrintf(DIST_Y1 + VALUE_Y_OFF, VALUE_X, "%d  ", gCurveDist);

          int x, y;
          KEYReadXY(&x, &y);

          if (x >= VALUE_X && x <= MINUS_X1 - 5)
          {
            if (y >= ANG_Y1 && y <= ANG_Y2)
            {
              gCurveFinalAng *= -1;
              delay(INPUT_DELAY_MS);
            }
            else if (y >= DIST_Y1 && y <= DIST_Y2)
            {
              gCurveDist *= -1;
              delay(INPUT_DELAY_MS);
            }
          }
          else if (x >= MINUS_X1 && x <= MINUS_X2)
          {
            if (y >= LIN_Y1 && y <= LIN_Y2)
            {
              gCurveLinSpeed -= 10;
              if (gCurveLinSpeed <= 0) gCurveLinSpeed = 10;
              delay(INPUT_DELAY_MS);
            }
            else if (y >= ANG_Y1 && y <= ANG_Y2)
            {
              gCurveFinalAng -= 10;
              delay(INPUT_DELAY_MS);
            }
            else if (y >= DIST_Y1 && y <= DIST_Y2)
            {
              gCurveDist -= 10;
              delay(INPUT_DELAY_MS);
            }
          }
          else if (x >= PLUS_X1 && x <= PLUS_X2)
          {
            if (y >= LIN_Y1 && y <= LIN_Y2)
            {
              gCurveLinSpeed += 10;
              delay(INPUT_DELAY_MS);
            }
            else if (y >= ANG_Y1 && y <= ANG_Y2)
            {
              gCurveFinalAng += 10;
              delay(INPUT_DELAY_MS);
            }
            else if (y >= DIST_Y1 && y <= DIST_Y2)
            {
              gCurveDist += 10;
              delay(INPUT_DELAY_MS);
            }
          }

          if (x >= START_X1 && x <= START_X2 && y >= START_Y1 && y <= START_Y2)
          {
            LCDSetFontSize(3);
            LCDSetColor(RED, WHITE);
            LCDSetPrintf(START_Y1 + 9, START_X1 + 40, "START");
            delay(INPUT_DELAY_MS);
            phase = PHASE_RUNNING;
          }
          
          if (x >= EXIT_X1 && x <= EXIT_X2 && y >= EXIT_Y1 && y <= EXIT_Y2)
          {
            LCDSetFontSize(3);
            LCDSetColor(RED, WHITE);
            LCDSetPrintf(EXIT_Y1 + 9, EXIT_X1 + 47, "EXIT");
            delay(INPUT_DELAY_MS);
            phase = PHASE_EXIT;
          }
        }

        break;
      }
      case PHASE_RUNNING:
      {
        LCDClear();
        LCDArea(0, 0, LCD_WIDTH, LCD_HEIGHT, RED);

        const int Y_PADDING = 40,
                  XPOS_Y = Y_MARGIN,
                  YPOS_Y = XPOS_Y + Y_PADDING,
                  ANGLE_Y = YPOS_Y + Y_PADDING,
                  REMAIN_Y = ANGLE_Y + Y_PADDING;
        
        LCDSetFontSize(4);
        LCDSetColor(WHITE, RED);
        LCDSetPrintf(LCD_HEIGHT - 45, 25, "RESET");

        LCDSetFontSize(2);
        LCDSetColor(WHITE, RED);
        LCDSetPrintf(XPOS_Y, X_MARGIN, "X POS:");
        LCDSetPrintf(YPOS_Y, X_MARGIN, "Y POS:");
        LCDSetPrintf(ANGLE_Y, X_MARGIN, "ANGLE:");
        LCDSetPrintf(REMAIN_Y, X_MARGIN, "REMAINING:");

        if (int err = VWCurve(gCurveDist, gCurveFinalAng, gCurveLinSpeed))
        {
          phase = PHASE_SETTINGS;
          break;
        }

        while (phase == PHASE_RUNNING)
        {
          int xpos, ypos, angle;
          VWGetPosition(&xpos, &ypos, &angle);

          int dist = VWRemain();

          LCDSetPrintf(XPOS_Y + 20, X_MARGIN, "%d mm     ", xpos);
          LCDSetPrintf(YPOS_Y + 20, X_MARGIN, "%d mm     ", ypos);
          LCDSetPrintf(ANGLE_Y + 20, X_MARGIN, "%d deg    ", angle);
          LCDSetPrintf(REMAIN_Y + 20, X_MARGIN, "%d mm     ", dist < 0 ? 0 : dist);

          int x, y;
          KEYReadXY(&x, &y);

          if (x >= 0 || y >= 0)
          {
            VWSetSpeed(0, 0);
            delay(INPUT_DELAY_MS);
            phase = PHASE_SETTINGS;
          }
        }

        break;
      }
      case PHASE_EXIT:
        exit = true;
        break;
    }
  }
}

/*
In this demo the user specifies the change in x and y coordinates at a
set absolute linear speed. The EyeBot then drives along a curve in a best
attempt to satisfy the change of coordinates.
*/
void vw_drive_demo()
{
  enum {
    PHASE_SETTINGS,
    PHASE_RUNNING,
    PHASE_EXIT
  } phase = PHASE_SETTINGS;

  bool exit = false;

  while (!exit)
  {
    switch (phase)
    {
      case PHASE_SETTINGS:
      {
        LCDClear();

        LCDSetFontSize(1);
        LCDSetColor(WHITE, BLACK);

        const int LIN_LABEL_X = X_MARGIN,
                  LIN_LABEL_Y = Y_MARGIN,
                  LIN_Y1 = LIN_LABEL_Y + LABEL_Y_PADDING,
                  LIN_Y2 = LIN_Y1 + BUTTON_HEIGHT;

        LCDSetPrintf(LIN_LABEL_Y, LIN_LABEL_X, "Lin. Speed (mm/s)");
        LCDArea(MINUS_X1, LIN_Y1, MINUS_X2, LIN_Y2, WHITE);
        LCDArea(MINUS_X1 + MINUS_SIGN_X1_OFF, LIN_Y1 + MINUS_SIGN_Y1_OFF, MINUS_X1 + MINUS_SIGN_X2_OFF, LIN_Y1 + MINUS_SIGN_Y2_OFF, BLACK);
        LCDArea(PLUS_X1, LIN_Y1, PLUS_X2, LIN_Y2, WHITE);
        LCDArea(PLUS_X1 + PLUS_SIGN_HOR_X1_OFF, LIN_Y1 + PLUS_SIGN_HOR_Y1_OFF, PLUS_X1 + PLUS_SIGN_HOR_X2_OFF, LIN_Y1 + PLUS_SIGN_HOR_Y2_OFF, BLACK);
        LCDArea(PLUS_X1 + PLUS_SIGN_VER_X1_OFF, LIN_Y1 + PLUS_SIGN_VER_Y1_OFF, PLUS_X1 + PLUS_SIGN_VER_X2_OFF, LIN_Y1 + PLUS_SIGN_VER_Y2_OFF, BLACK);

        const int DX_LABEL_X = X_MARGIN,
                  DX_LABEL_Y = LIN_Y2 + BUTTON_Y_PADDING,
                  DX_Y1 = DX_LABEL_Y + LABEL_Y_PADDING,
                  DX_Y2 = DX_Y1 + BUTTON_HEIGHT;

        LCDSetPrintf(DX_LABEL_Y, DX_LABEL_X, "Delta X (mm)");
        LCDArea(MINUS_X1, DX_Y1, MINUS_X2, DX_Y2, WHITE);
        LCDArea(MINUS_X1 + MINUS_SIGN_X1_OFF, DX_Y1 + MINUS_SIGN_Y1_OFF, MINUS_X1 + MINUS_SIGN_X2_OFF, DX_Y1 + MINUS_SIGN_Y2_OFF, BLACK);
        LCDArea(PLUS_X1, DX_Y1, PLUS_X2, DX_Y2, WHITE);
        LCDArea(PLUS_X1 + PLUS_SIGN_HOR_X1_OFF, DX_Y1 + PLUS_SIGN_HOR_Y1_OFF, PLUS_X1 + PLUS_SIGN_HOR_X2_OFF, DX_Y1 + PLUS_SIGN_HOR_Y2_OFF, BLACK);
        LCDArea(PLUS_X1 + PLUS_SIGN_VER_X1_OFF, DX_Y1 + PLUS_SIGN_VER_Y1_OFF, PLUS_X1 + PLUS_SIGN_VER_X2_OFF, DX_Y1 + PLUS_SIGN_VER_Y2_OFF, BLACK);

        const int DY_LABEL_X = X_MARGIN,
                  DY_LABEL_Y = DX_Y2 + BUTTON_Y_PADDING,
                  DY_Y1 = DY_LABEL_Y + LABEL_Y_PADDING,
                  DY_Y2 = DY_Y1 + BUTTON_HEIGHT;

        LCDSetPrintf(DY_LABEL_Y, DY_LABEL_X, "Delta Y (mm)");
        LCDArea(MINUS_X1, DY_Y1, MINUS_X2, DY_Y2, WHITE);
        LCDArea(MINUS_X1 + MINUS_SIGN_X1_OFF, DY_Y1 + MINUS_SIGN_Y1_OFF, MINUS_X1 + MINUS_SIGN_X2_OFF, DY_Y1 + MINUS_SIGN_Y2_OFF, BLACK);
        LCDArea(PLUS_X1, DY_Y1, PLUS_X2, DY_Y2, WHITE);
        LCDArea(PLUS_X1 + PLUS_SIGN_HOR_X1_OFF, DY_Y1 + PLUS_SIGN_HOR_Y1_OFF, PLUS_X1 + PLUS_SIGN_HOR_X2_OFF, DY_Y1 + PLUS_SIGN_HOR_Y2_OFF, BLACK);
        LCDArea(PLUS_X1 + PLUS_SIGN_VER_X1_OFF, DY_Y1 + PLUS_SIGN_VER_Y1_OFF, PLUS_X1 + PLUS_SIGN_VER_X2_OFF, DY_Y1 + PLUS_SIGN_VER_Y2_OFF, BLACK);

        LCDSetFontSize(3);
        LCDSetColor(BLACK, WHITE);

        const int START_X1 = X_MARGIN,
                  START_X2 = LCD_WIDTH - 5,
                  START_Y1 = DY_Y2 + 5,
                  START_Y2 = START_Y1 + BUTTON_HEIGHT;
        
        LCDArea(START_X1, START_Y1, START_X2, START_Y2, WHITE);
        LCDSetPrintf(START_Y1 + 9, START_X1 + 40, "START");

        const int EXIT_X1 = START_X1,
                  EXIT_X2 = START_X2,
                  EXIT_Y1 = START_Y2 + 5,
                  EXIT_Y2 = EXIT_Y1 + BUTTON_HEIGHT;

        LCDArea(EXIT_X1, EXIT_Y1, EXIT_X2, EXIT_Y2, WHITE);
        LCDSetPrintf(EXIT_Y1 + 9, EXIT_X1 + 47, "EXIT");

        while (phase == PHASE_SETTINGS)
        {
          LCDSetFontSize(2);
          LCDSetColor(WHITE, BLACK);

          LCDSetPrintf(LIN_Y1 + VALUE_Y_OFF, VALUE_X, "%d  ", gDriveLinSpeed);
          LCDSetPrintf(DX_Y1 + VALUE_Y_OFF, VALUE_X, "%d  ", gDriveDeltaX);
          LCDSetPrintf(DY_Y1 + VALUE_Y_OFF, VALUE_X, "%d  ", gDriveDeltaY);

          int x, y;
          KEYReadXY(&x, &y);

          if (x >= VALUE_X && x <= MINUS_X1 - 5)
          {
            if (y >= DX_Y1 && y <= DX_Y2)
            {
              gDriveDeltaX *= -1;
              delay(INPUT_DELAY_MS);
            }
            else if (y >= DY_Y1 && y <= DY_Y2)
            {
              gDriveDeltaY *= -1;
              delay(INPUT_DELAY_MS);
            }
          }
          else if (x >= MINUS_X1 && x <= MINUS_X2)
          {
            if (y >= LIN_Y1 && y <= LIN_Y2)
            {
              gDriveLinSpeed -= 10;
              if (gDriveLinSpeed <= 0) gDriveLinSpeed = 10;
              delay(INPUT_DELAY_MS);
            }
            else if (y >= DX_Y1 && y <= DX_Y2)
            {
              gDriveDeltaX -= 10;
              delay(INPUT_DELAY_MS);
            }
            else if (y >= DY_Y1 && y <= DY_Y2)
            {
              gDriveDeltaY -= 10;
              if (gDriveDeltaY == 0) gDriveDeltaY = -10;
              delay(INPUT_DELAY_MS);
            }
          }
          else if (x >= PLUS_X1 && x <= PLUS_X2)
          {
            if (y >= LIN_Y1 && y <= LIN_Y2)
            {
              gDriveLinSpeed += 10;
              delay(INPUT_DELAY_MS);
            }
            else if (y >= DX_Y1 && y <= DX_Y2)
            {
              gDriveDeltaX += 10;
              delay(INPUT_DELAY_MS);
            }
            else if (y >= DY_Y1 && y <= DY_Y2)
            {
              gDriveDeltaY += 10;
              if (gDriveDeltaY == 0) gDriveDeltaY = 10;
              delay(INPUT_DELAY_MS);
            }
          }

          if (x >= START_X1 && x <= START_X2 && y >= START_Y1 && y <= START_Y2)
          {
            LCDSetFontSize(3);
            LCDSetColor(RED, WHITE);
            LCDSetPrintf(START_Y1 + 9, START_X1 + 40, "START");
            delay(INPUT_DELAY_MS);
            phase = PHASE_RUNNING;
          }
          
          if (x >= EXIT_X1 && x <= EXIT_X2 && y >= EXIT_Y1 && y <= EXIT_Y2)
          {
            LCDSetFontSize(3);
            LCDSetColor(RED, WHITE);
            LCDSetPrintf(EXIT_Y1 + 9, EXIT_X1 + 47, "EXIT");
            delay(INPUT_DELAY_MS);
            phase = PHASE_EXIT;
          }
        }

        break;
      }
      case PHASE_RUNNING:
      {
        LCDClear();
        LCDArea(0, 0, LCD_WIDTH, LCD_HEIGHT, RED);

        const int Y_PADDING = 40,
                  XPOS_Y = Y_MARGIN,
                  YPOS_Y = XPOS_Y + Y_PADDING,
                  ANGLE_Y = YPOS_Y + Y_PADDING,
                  REMAIN_Y = ANGLE_Y + Y_PADDING;
        
        LCDSetFontSize(4);
        LCDSetColor(WHITE, RED);
        LCDSetPrintf(LCD_HEIGHT - 45, 25, "RESET");

        LCDSetFontSize(2);
        LCDSetColor(WHITE, RED);
        LCDSetPrintf(XPOS_Y, X_MARGIN, "X POS:");
        LCDSetPrintf(YPOS_Y, X_MARGIN, "Y POS:");
        LCDSetPrintf(ANGLE_Y, X_MARGIN, "ANGLE:");
        LCDSetPrintf(REMAIN_Y, X_MARGIN, "REMAINING:");

        if (int err = VWDrive(gDriveDeltaX, gDriveDeltaY, gDriveLinSpeed))
        {
          phase = PHASE_SETTINGS;
          break;
        }

        while (phase == PHASE_RUNNING)
        {
          int xpos, ypos, angle;
          VWGetPosition(&xpos, &ypos, &angle);

          int dist = VWRemain();

          LCDSetPrintf(XPOS_Y + 20, X_MARGIN, "%d mm     ", xpos);
          LCDSetPrintf(YPOS_Y + 20, X_MARGIN, "%d mm     ", ypos);
          LCDSetPrintf(ANGLE_Y + 20, X_MARGIN, "%d deg    ", angle);
          LCDSetPrintf(REMAIN_Y + 20, X_MARGIN, "%d mm     ", dist < 0 ? 0 : dist);

          int x, y;
          KEYReadXY(&x, &y);

          if (x >= 0 || y >= 0)
          {
            VWSetSpeed(0, 0);
            delay(INPUT_DELAY_MS);
            phase = PHASE_SETTINGS;
          }
        }

        break;
      }
      case PHASE_EXIT:
        exit = true;
        break;
    }
  }
}

void loop() {
  switch (gScreenIdx)
  {
    case 0:
    {
      bool exit = false;
      const char* options[] = {
        "CAMERA",
        "VW SET SPEED",
        "VW STRAIGHT",
        "VW TURN",
        "VW CURVE",
        "VW DRIVE",
        NULL
      };

      draw_menu(options);

      while (!exit)
      {
        int option_idx = selected_menu_option(options);

        if (option_idx < 0) continue;
        
        switch (option_idx)
        {
          case 0:
            camera_demo();
            break;
          case 1:
            vw_set_speed_demo();
            break;
          case 2:
            vw_straight_demo();
            break;
          case 3:
            vw_turn_demo();
            break;
          case 4:
            vw_curve_demo();
            break;
          case 5:
            vw_drive_demo();
            break;
          default:
            break;
        }

        draw_menu(options);
      }

      break;
    }
    default:
      gScreenIdx = 0;
      break;
  }
}
