#include "eyebot.h"

#define NUM_PAGES 3
#define INPUT_DELAY_MS 200

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
  DEMO_MENU,
  COLOUR_NAVIGATION,
  CAMERA_FEED,
  DRIVE_STRAIGHT,
  DRIVE_CURVE,
  DRIVE_TURN,
  DRIVE_GOTO,
  DRIVE_SET,
  DISTANCE_SENSOR,
  HIT_AND_RETURN,
  DRIVE_TO_COLOR
} demo;

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

enum Screen {
  SCREEN_SETTINGS_1,
  SCREEN_SETTINGS_2,
  SCREEN_RUNNING
};

enum HitAndReturnPhases {
  HIT_PHASE_FORWARD,
  HIT_PHASE_TURN,
  HIT_PHASE_RETURN,
  HIT_PHASE_STOP
};

void setup() {
  Serial.begin(9600);

  if (!EYEBOTInit())
    Serial.printf("Initialisation of Eyebot failed\n");
  
  DRVSetMotorOffsets(0, 0);
  DRVSetMaxLinearSpeed(max_lin_speed);
  DRVSetMaxAngularSpeed(max_ang_speed);

  demo = DEMO_MENU;
}

#define ROW_SPACE_PX 62
#define VARIABLE_BUTTON_WIDTH_PX 40
#define VARIABLE_BUTTON_HEIGHT_PX VARIABLE_BUTTON_WIDTH_PX

void DrawVariableWidgetValue(const char *value, int row)
{
  if (row < 0)
    row = 0;
  
  LCDSetFontSize(2);
  LCDSetFontColor(WHITE);
  LCDPrintAt(50, 34 + row*ROW_SPACE_PX, value);
}

void DemoDriveToColor()
{
  Screen screen = SCREEN_SETTINGS_1;
  bool quit = false, ui_init = false, show_overlay = false;
  rgb selected_color = RED;
  int hue_threshold = 10, sat_threshold = 10, int_threshold = 10;
  int lin_speed = 200, ang_speed = 150;

  enum {
    D2C_PHASE_SEEKING, 
    D2C_PHASE_NAVIGATE
  } phase = D2C_PHASE_SEEKING;
  
  while (!quit)
  {
    switch (screen)
    {
      case SCREEN_SETTINGS_1:
      {
        if (!ui_init)
        {
          LCDClear();

          // Left Button
          LCDSetFontColor(BLACK, WHITE);
          LCDSetFontSize(1);
          LCDDrawRect(5, LCD_HEIGHT - 15, 77, 15, WHITE);
          LCDPrintAt(10, LCD_HEIGHT - 12, "LB: EXIT");

          // Linear Speed Widget
          LCDSetFontSize(1);
          LCDSetFontColor(WHITE);
          LCDPrintAt(5, 5, "Linear Speed (mm/s)");
          // Decrease
          #define LIN_MINUS_X 5
          #define LIN_MINUS_Y 20
          #define LIN_MINUS_WIDTH 40
          #define LIN_MINUS_HEIGHT 40
          LCDDrawRect(LIN_MINUS_X, LIN_MINUS_Y, LIN_MINUS_WIDTH, LIN_MINUS_HEIGHT, WHITE);
          LCDDrawRect(LIN_MINUS_X + 10, LIN_MINUS_Y + 18, 20, 5, BLACK);
          // Increase
          #define LIN_PLUS_X 125
          #define LIN_PLUS_Y 20
          #define LIN_PLUS_WIDTH 40
          #define LIN_PLUS_HEIGHT 40
          LCDDrawRect(LIN_PLUS_X, LIN_PLUS_Y, LIN_PLUS_WIDTH, LIN_PLUS_HEIGHT, WHITE);
          LCDDrawRect(LIN_PLUS_X + 10, LIN_PLUS_Y + 18, 20, 5, BLACK);
          LCDDrawRect(LIN_PLUS_X + 18, LIN_PLUS_Y + 10, 5, 21, BLACK);

          // Angular Speed Widget
          LCDSetFontSize(1);
          LCDSetFontColor(WHITE);
          LCDPrintAt(5, 67, "Angular Speed (deg./s)");
          // Decrease
          #define ANG_MINUS_X 5
          #define ANG_MINUS_Y 82
          #define ANG_MINUS_WIDTH 40
          #define ANG_MINUS_HEIGHT 40
          LCDDrawRect(ANG_MINUS_X, ANG_MINUS_Y, ANG_MINUS_WIDTH, ANG_MINUS_HEIGHT, WHITE);
          LCDDrawRect(ANG_MINUS_X + 10, ANG_MINUS_Y + 18, 20, 5, BLACK);
          // Increase
          #define ANG_PLUS_X 125
          #define ANG_PLUS_Y 82
          #define ANG_PLUS_WIDTH 40
          #define ANG_PLUS_HEIGHT 40
          LCDDrawRect(ANG_PLUS_X, ANG_PLUS_Y, ANG_PLUS_WIDTH, ANG_PLUS_HEIGHT, WHITE);
          LCDDrawRect(ANG_PLUS_X + 10, ANG_PLUS_Y + 18, 20, 5, BLACK);
          LCDDrawRect(ANG_PLUS_X + 18, ANG_PLUS_Y + 10, 5, 21, BLACK);

          byte r, g, b;
          IPGetRGB(selected_color, &r, &g, &b);
          char col_str[32];
          snprintf(col_str, 32, "Selected Col. (%u,%u,%u)", r, g, b);
          LCDPrintAt(5, 5 + 2*ROW_SPACE_PX, col_str);
          LCDDrawRect(5, 20 + 2*ROW_SPACE_PX, 40, 40, selected_color);
          LCDDrawRect(50, 20 + 2*ROW_SPACE_PX, 115, 40, WHITE);
          LCDSetFontSize(2);
          LCDSetFontColor(BLACK, WHITE);
          LCDPrintAt(61, 33 + 2*ROW_SPACE_PX, "RESELECT");

          #define START_X 5
          #define START_Y 5 + 3*ROW_SPACE_PX
          #define START_X_OFF 23
          #define START_Y_OFF 37
          #define START_WIDTH 160
          #define START_HEIGHT 107
          LCDDrawRect(START_X, START_Y, START_WIDTH, START_HEIGHT, WHITE);
          LCDSetFontSize(4);
          LCDPrintAt(START_X + START_X_OFF, START_Y + START_Y_OFF, "START");

          ui_init = true;
        }

        char val_str[8];
        snprintf(val_str, 8, "%d ", lin_speed);
        DrawVariableWidgetValue(val_str, 0);

        snprintf(val_str, 8, "%d ", ang_speed);
        DrawVariableWidgetValue(val_str, 1);

        bool left_pressed;
        INReadButton(LEFT_BUTTON, &left_pressed);
        if (left_pressed)
        {
          quit = true;

          LCDSetFontColor(RED, WHITE);
          LCDSetFontSize(1);
          LCDPrintAt(10, LCD_HEIGHT - 12, "LB: EXIT");
          delay(INPUT_DELAY_MS);
          LCDSetFontColor(BLACK, WHITE);
          LCDPrintAt(10, LCD_HEIGHT - 12, "LB: EXIT");
        }

        int t_x, t_y;
        INReadTouch(&t_x, &t_y);
        if (t_x >= 5 && t_x <= 5 + VARIABLE_BUTTON_WIDTH_PX)
        {
          if (t_y >= 20 && t_y <= 20 + VARIABLE_BUTTON_HEIGHT_PX)
          {
            lin_speed -= 10;

            if (lin_speed < 0)
              lin_speed = 0;
            
            LCDDrawRect(15, 38, 20, 5, RED);
            delay(INPUT_DELAY_MS);
            LCDDrawRect(15, 38, 20, 5, BLACK);
          }
          else if (t_y >= 20 + ROW_SPACE_PX && t_y <= 20 + ROW_SPACE_PX + VARIABLE_BUTTON_HEIGHT_PX)
          {
            ang_speed -= 10;

            if (ang_speed < 0)
              ang_speed = 0;
            
            LCDDrawRect(15, 38 + ROW_SPACE_PX, 20, 5, RED);
            delay(INPUT_DELAY_MS);
            LCDDrawRect(15, 38 + ROW_SPACE_PX, 20, 5, BLACK);
          }
        }
        else if (t_x >= 125 && t_x <= 125 + VARIABLE_BUTTON_WIDTH_PX)
        {
          if (t_y >= 20 && t_y <= 20 + VARIABLE_BUTTON_HEIGHT_PX)
          {
            lin_speed += 10;

            LCDDrawRect(135, 38, 20, 5, RED);
            LCDDrawRect(143, 30, 5, 21, RED);
            delay(INPUT_DELAY_MS);
            LCDDrawRect(135, 38, 20, 5, BLACK);
            LCDDrawRect(143, 30, 5, 21, BLACK);
          }
          else if (t_y >= 20 + ROW_SPACE_PX && t_y <= 20 + ROW_SPACE_PX + VARIABLE_BUTTON_HEIGHT_PX)
          {
            ang_speed += 10;

            LCDDrawRect(135, 38 + ROW_SPACE_PX, 20, 5, RED);
            LCDDrawRect(143, 30 + ROW_SPACE_PX, 5, 21, RED);
            delay(INPUT_DELAY_MS);
            LCDDrawRect(135, 38 + ROW_SPACE_PX, 20, 5, BLACK);
            LCDDrawRect(143, 30 + ROW_SPACE_PX, 5, 21, BLACK);
          }
        }
        else if (t_x >= 50 && t_x <= 165 && t_y >= 20 + 2*ROW_SPACE_PX && t_y <= 20 + 2*ROW_SPACE_PX + VARIABLE_BUTTON_HEIGHT_PX)
        {
          screen = SCREEN_SETTINGS_2;
          ui_init = false;

          LCDSetFontSize(2);
          LCDSetFontColor(RED, WHITE);
          LCDPrintAt(61, 33 + 2*ROW_SPACE_PX, "RESELECT");
          delay(INPUT_DELAY_MS);
          LCDSetFontColor(BLACK, WHITE);
          LCDPrintAt(61, 33 + 2*ROW_SPACE_PX, "RESELECT");
        }
        else if (t_x >= START_X && t_x <= START_X + START_WIDTH && t_y >= START_Y && t_y <= START_Y + START_HEIGHT)
        {
          screen = SCREEN_RUNNING;
          ui_init = false;

          LCDSetFontSize(4);
          LCDSetFontColor(RED, WHITE);
          LCDPrintAt(START_X + START_X_OFF, START_Y + START_Y_OFF, "START");
          delay(INPUT_DELAY_MS);
          LCDSetFontColor(BLACK, WHITE);
          LCDPrintAt(START_X + START_X_OFF, START_Y + START_Y_OFF, "START");
        }

        break;
      }
      case SCREEN_SETTINGS_2:
      {
        if (!ui_init)
        {
          LCDClear();

          LCDSetFontColor(BLACK, WHITE);
          LCDSetFontSize(1);
          // Left Button
          LCDDrawRect(5, LCD_HEIGHT - 15, 77, 15, WHITE);
          LCDPrintAt(10, LCD_HEIGHT - 12, "LB: EXIT");
          // Right Button
          LCDDrawRect(87, LCD_HEIGHT - 15, 78, 15, WHITE);
          LCDPrintAt(93, LCD_HEIGHT - 12, "RB: OVERLAY");

          #define MINUS_START_X 50
          #define MINUS_WIDTH 55
          #define PLUS_START_X 110
          #define PLUS_WIDTH 55
          #define WIDGET_HEIGHT 30
          // Hue Theshold Widget
          #define HUE_Y 180
          LCDDrawRect(MINUS_START_X, HUE_Y, MINUS_WIDTH, WIDGET_HEIGHT, WHITE);
          LCDDrawRect(MINUS_START_X + 20, HUE_Y + 13, 16, 4, BLACK);
          LCDDrawRect(PLUS_START_X, HUE_Y, PLUS_WIDTH, WIDGET_HEIGHT, WHITE);
          LCDDrawRect(PLUS_START_X + 20, HUE_Y + 13, 16, 4, BLACK);
          LCDDrawRect(PLUS_START_X + 26, HUE_Y + 7, 4, 16, BLACK);
          LCDSetFontSize(1);
          LCDSetFontColor(WHITE);
          LCDPrintAt(5, HUE_Y - 10, "Hue Threshold:");
          // Plus symbol
          LCDDrawRect(6, HUE_Y + 9, 1, 5, WHITE);
          LCDDrawRect(4, HUE_Y + 11, 5, 1, WHITE);
          // Minus symbol
          LCDDrawRect(4, HUE_Y + 17, 5, 1, WHITE);

          // Saturation Threshod Widget
          #define SAT_Y 225
          LCDDrawRect(MINUS_START_X, SAT_Y, MINUS_WIDTH, WIDGET_HEIGHT, WHITE);
          LCDDrawRect(MINUS_START_X + 20, SAT_Y + 13, 16, 4, BLACK);
          LCDDrawRect(PLUS_START_X, SAT_Y, PLUS_WIDTH, WIDGET_HEIGHT, WHITE);
          LCDDrawRect(PLUS_START_X + 20, SAT_Y + 13, 16, 4, BLACK);
          LCDDrawRect(PLUS_START_X + 26, SAT_Y + 7, 4, 16, BLACK);
          LCDSetFontSize(1);
          LCDSetFontColor(WHITE);
          LCDPrintAt(5, SAT_Y - 10, "Saturation Threshold:");
          // Plus symbol
          LCDDrawRect(6, SAT_Y + 9, 1, 5, WHITE);
          LCDDrawRect(4, SAT_Y + 11, 5, 1, WHITE);
          // Minus symbol
          LCDDrawRect(4, SAT_Y + 17, 5, 1, WHITE);

          // Intensity Threshod Widget
          #define INT_Y 270
          LCDDrawRect(MINUS_START_X, INT_Y, MINUS_WIDTH, WIDGET_HEIGHT, WHITE);
          LCDDrawRect(MINUS_START_X + 20, INT_Y + 13, 16, 4, BLACK);
          LCDDrawRect(PLUS_START_X, INT_Y, PLUS_WIDTH, WIDGET_HEIGHT, WHITE);
          LCDDrawRect(PLUS_START_X + 20, INT_Y + 13, 16, 4, BLACK);
          LCDDrawRect(PLUS_START_X + 26, INT_Y + 7, 4, 16, BLACK);
          LCDSetFontSize(1);
          LCDSetFontColor(WHITE);
          LCDPrintAt(5, INT_Y - 10, "Intensity Threshold: ");
          // Plus symbol
          LCDDrawRect(6, INT_Y + 9, 1, 5, WHITE);
          LCDDrawRect(4, INT_Y + 11, 5, 1, WHITE);
          // Minus symbol
          LCDDrawRect(4, INT_Y + 17, 5, 1, WHITE);

          // Colour Sampler
          #define SAMPLER_START_X 50
          #define SAMPLER_START_Y 125
          #define SAMPLER_WIDTH 115
          #define SAMPLER_HEIGHT 40
          LCDDrawRect(SAMPLER_START_X, SAMPLER_START_Y, SAMPLER_WIDTH, SAMPLER_HEIGHT, WHITE);
          LCDSetFontSize(2);
          LCDSetFontColor(BLACK, WHITE);
          LCDPrintAt(SAMPLER_START_X + 25, SAMPLER_START_Y + 13, "SAMPLE");

          ui_init = true;
        }

        CAMGetImage(img);

        int sum_r = 0, sum_g = 0,  sum_b = 0, num_pixels = 0;
        for (int y = QQVGA_HEIGHT/2 - 1; y <= QQVGA_HEIGHT/2 + 1; y++)
        {
          for (int x = QQVGA_WIDTH/2 - 1; x <= QQVGA_WIDTH/2 + 1; x++)
          {
            byte r, g, b;
            IPGetRGB(img[y*QQVGA_WIDTH + x], &r, &g, &b);
            sum_r += r;
            sum_g += g;
            sum_b += b;

            num_pixels++;
          }
        }

        rgb sampled_pixel;// = img[QQVGA_HEIGHT/2*QQVGA_WIDTH + QQVGA_WIDTH/2];
        IPSetRGB(sum_r / num_pixels, sum_g / num_pixels, sum_b / num_pixels, &sampled_pixel);

        // Sampled Colour
        LCDDrawRect(5, 125, VARIABLE_BUTTON_WIDTH_PX, VARIABLE_BUTTON_HEIGHT_PX, sampled_pixel);

        if (show_overlay)
        {
          hsi chosen;
          IPRGBToHSI(selected_color, &chosen);

          for (int y = 0; y < QQVGA_HEIGHT; y++)
          {
            for (int x = 0; x < QQVGA_WIDTH; x++)
            {
              hsi pixel;
              IPRGBToHSI(img[y*QQVGA_WIDTH + x], &pixel);

              if (pixel.hue >= (chosen.hue - hue_threshold) && pixel.hue <= (chosen.hue + hue_threshold) &&
                  pixel.saturation >= (chosen.saturation - sat_threshold) && pixel.saturation <= (chosen.saturation + sat_threshold) &&
                  pixel.intensity >= (chosen.intensity - int_threshold) && pixel.intensity <= (chosen.intensity + int_threshold))
                gray_img[y*QQVGA_WIDTH + x] = 0xFF;
              else
                gray_img[y*QQVGA_WIDTH + x] = 0;
            }
          }

          LCDDrawImage(5, 0, QQVGA_WIDTH, QQVGA_HEIGHT, gray_img);
        }
        else 
        {
          LCDDrawImage(5, 0, QQVGA_WIDTH, QQVGA_HEIGHT, img);
          // Sampled Colour crosshair
          LCDDrawRect(5 + QQVGA_WIDTH/2, QQVGA_HEIGHT/2 - 2, 1, 5, RED);
          LCDDrawRect(5 + QQVGA_WIDTH/2 - 2, QQVGA_HEIGHT/2, 5, 1, RED);
        }

        #define VAL_STR_LEN 4
        char val_str[VAL_STR_LEN];

        #define VAL_X 12
        #define VAL_Y 7

        LCDSetFontColor(WHITE);

        // Hue Threshold Values
        LCDSetFontSize(2);
        snprintf(val_str, VAL_STR_LEN, "%d ", hue_threshold);
        LCDPrintAt(VAL_X, HUE_Y + VAL_Y, val_str);

        // Saturation Threshold Values
        snprintf(val_str, VAL_STR_LEN, "%d ", sat_threshold);
        LCDSetFontSize(2);
        LCDPrintAt(VAL_X, SAT_Y + VAL_Y, val_str);

        // Intensity Theshold Values
        snprintf(val_str, VAL_STR_LEN, "%d ", int_threshold);
        LCDSetFontSize(2);
        LCDPrintAt(VAL_X, INT_Y + VAL_Y, val_str);

        bool left_pressed;
        INReadButton(LEFT_BUTTON, &left_pressed);
        if (left_pressed)
        {
          screen = SCREEN_SETTINGS_1;
          ui_init = false;
          LCDSetFontColor(RED, WHITE);
          LCDSetFontSize(1);
          LCDPrintAt(10, LCD_HEIGHT - 12, "LB: EXIT");
          delay(INPUT_DELAY_MS);
          LCDSetFontColor(BLACK, WHITE);
          LCDPrintAt(10, LCD_HEIGHT - 12, "LB: EXIT");
        }

        bool right_pressed;
        INReadButton(RIGHT_BUTTON, &right_pressed);
        if (right_pressed)
        {
          show_overlay = !show_overlay;

          LCDSetFontColor(RED, WHITE);
          LCDSetFontSize(1);
          LCDPrintAt(93, LCD_HEIGHT - 12, "RB: OVERLAY");
          delay(INPUT_DELAY_MS);
          LCDSetFontColor(BLACK, WHITE);
          LCDPrintAt(93, LCD_HEIGHT - 12, "RB: OVERLAY");
        }

        int t_x, t_y;
        INReadTouch(&t_x, &t_y);
        if (t_x >= SAMPLER_START_X && t_x <= SAMPLER_START_X + SAMPLER_WIDTH && t_y >= SAMPLER_START_Y && t_y <= SAMPLER_START_Y + SAMPLER_HEIGHT)
        {
          selected_color = sampled_pixel;
          LCDSetFontSize(2);
          LCDSetFontColor(RED, WHITE);
          LCDPrintAt(SAMPLER_START_X + 25, SAMPLER_START_Y + 13, "SAMPLE");
          delay(INPUT_DELAY_MS);
          LCDSetFontColor(BLACK, WHITE);
          LCDPrintAt(SAMPLER_START_X + 25, SAMPLER_START_Y + 13, "SAMPLE");
        }
        else if (t_x >= MINUS_START_X && t_x <= MINUS_START_X + MINUS_WIDTH)
        {
          if (t_y >= HUE_Y && t_y <= HUE_Y + WIDGET_HEIGHT)
          {
            hue_threshold--;
            if (hue_threshold < 0)
              hue_threshold = 0;
            
            LCDDrawRect(MINUS_START_X + 20, HUE_Y + 13, 16, 4, RED);
            delay(INPUT_DELAY_MS);
            LCDDrawRect(MINUS_START_X + 20, HUE_Y + 13, 16, 4, BLACK);
          }
          else if (t_y >= SAT_Y && t_y <= SAT_Y + WIDGET_HEIGHT)
          {
            sat_threshold--;
            if (sat_threshold < 0)
              sat_threshold = 0;
            
            LCDDrawRect(MINUS_START_X + 20, SAT_Y + 13, 16, 4, RED);
            delay(INPUT_DELAY_MS);
            LCDDrawRect(MINUS_START_X + 20, SAT_Y + 13, 16, 4, BLACK);
          }
          else if (t_y >= INT_Y && t_y <= INT_Y + WIDGET_HEIGHT)
          {
            int_threshold--;
            if (int_threshold < 0)
              int_threshold = 0;
            
            LCDDrawRect(MINUS_START_X + 20, INT_Y + 13, 16, 4, RED);
            delay(INPUT_DELAY_MS);
            LCDDrawRect(MINUS_START_X + 20, INT_Y + 13, 16, 4, BLACK);
          }
        }
        else if (t_x >= PLUS_START_X && t_x <= PLUS_START_X + PLUS_WIDTH)
        {
          if (t_y >= HUE_Y && t_y <= HUE_Y + WIDGET_HEIGHT)
          {
            hue_threshold++;   

            LCDDrawRect(PLUS_START_X + 20, HUE_Y + 13, 16, 4, RED);
            LCDDrawRect(PLUS_START_X + 26, HUE_Y + 7, 4, 16, RED);
            delay(INPUT_DELAY_MS);
            LCDDrawRect(PLUS_START_X + 20, HUE_Y + 13, 16, 4, BLACK);
            LCDDrawRect(PLUS_START_X + 26, HUE_Y + 7, 4, 16, BLACK);
          }
          else if (t_y >= SAT_Y && t_y <= SAT_Y + WIDGET_HEIGHT)
          {
            sat_threshold++;  

            LCDDrawRect(PLUS_START_X + 20, SAT_Y + 13, 16, 4, RED);
            LCDDrawRect(PLUS_START_X + 26, SAT_Y + 7, 4, 16, RED);
            delay(INPUT_DELAY_MS);
            LCDDrawRect(PLUS_START_X + 20, SAT_Y + 13, 16, 4, BLACK);
            LCDDrawRect(PLUS_START_X + 26, SAT_Y + 7, 4, 16, BLACK);
          }
          else if (t_y >= INT_Y && t_y <= INT_Y + WIDGET_HEIGHT)
          {
            int_threshold++;

            LCDDrawRect(PLUS_START_X + 20, INT_Y + 13, 16, 4, RED);
            LCDDrawRect(PLUS_START_X + 26, INT_Y + 7, 4, 16, RED);
            delay(INPUT_DELAY_MS);
            LCDDrawRect(PLUS_START_X + 20, INT_Y + 13, 16, 4, BLACK);
            LCDDrawRect(PLUS_START_X + 26, INT_Y + 7, 4, 16, BLACK);
          }
        }
        
        break;
      }
      case SCREEN_RUNNING:
      {
        if (!ui_init)
        {
          LCDClear();

          #define RESET_X 5
          #define RESET_Y 2 * LCD_HEIGHT / 3
          #define RESET_WIDTH LCD_WIDTH - 10
          #define RESET_HEIGHT LCD_HEIGHT - RESET_Y - 5
          LCDDrawRect(RESET_X, RESET_Y, RESET_WIDTH, RESET_HEIGHT, RED);
          LCDSetFontSize(4);
          LCDSetFontColor(WHITE, RED);
          LCDPrintAt(RESET_X + 23, RESET_Y + 20, "TOUCH");
          LCDPrintAt(RESET_X + 23, RESET_Y + 55, "RESET");

          ui_init = true;
        }

        CAMGetImage(img);

        //Reuse static memory
        uint16_t *hori_hist = (uint16_t*)gray_img;
        uint16_t *vert_hist = (uint16_t*)edge_img;

        memset(hori_hist, 0, QQVGA_WIDTH*sizeof(uint16_t));
        memset(vert_hist, 0, QQVGA_HEIGHT*sizeof(uint16_t));

        hsi chosen_hsi;
        IPRGBToHSI(selected_color, &chosen_hsi);

        int hori_hist_max_idx = 0, vert_hist_max_idx = 0;

        for (int y = 0; y < QQVGA_HEIGHT; y++)
        {
          for (int x = 0; x < QQVGA_WIDTH; x++)
          {
            hsi current_hsi;
            IPRGBToHSI(img[y*QQVGA_WIDTH + x], &current_hsi);

            if (current_hsi.hue >= (chosen_hsi.hue - hue_threshold) && current_hsi.hue <= (chosen_hsi.hue + hue_threshold) &&
                current_hsi.saturation >= (chosen_hsi.saturation - sat_threshold) && current_hsi.saturation <= (chosen_hsi.saturation + sat_threshold) &&
                current_hsi.intensity >= (chosen_hsi.intensity - int_threshold) && current_hsi.intensity <= (current_hsi.intensity + int_threshold))
            {
              vert_hist[y]++;
              hori_hist[x]++;
            }

            if (hori_hist[x] > hori_hist[hori_hist_max_idx])
              hori_hist_max_idx = x;
          }

          if (vert_hist[y] > vert_hist[vert_hist_max_idx])
            vert_hist_max_idx = y;
        }

        int dist;
        PSDGet(&dist);
        #define MIN_DIST 100

        switch (phase)
        {
          case D2C_PHASE_SEEKING:
          {           
            if (dist > MIN_DIST)
            {
              if (hori_hist_max_idx <= QQVGA_WIDTH / 3)
                DRVSetSpeed(0, -1*ang_speed);
              else if (hori_hist_max_idx >= 2 * QQVGA_WIDTH / 3)
                DRVSetSpeed(0, ang_speed);
              else
                phase = D2C_PHASE_NAVIGATE;
            }
            else
              DRVSetSpeed(0, 0);

            break;
          }
          case D2C_PHASE_NAVIGATE:
          {
            if (dist > MIN_DIST)
            {
              if (hori_hist_max_idx <= QQVGA_WIDTH / 3 || hori_hist_max_idx >= 2 * QQVGA_WIDTH / 3)
                phase = D2C_PHASE_SEEKING;
              else
                DRVSetSpeed(lin_speed, 0);
            }
            else
              DRVSetSpeed(0, 0);
            
            break;
          }
          default:
          {
            screen = SCREEN_SETTINGS_1;
            ui_init = false;
            DRVSetSpeed(0, 0);
            break;
          }
        }

        LCDDrawImage(5, 5, QQVGA_WIDTH, QQVGA_HEIGHT, img);
        LCDDrawRect(5 + hori_hist_max_idx, 5, 1, QQVGA_HEIGHT, RED);
        LCDDrawRect(5, 5 + vert_hist_max_idx, QQVGA_WIDTH, 1, RED);

        LCDSetFontSize(2);
        LCDSetFontColor(WHITE);
        char str[16];
        snprintf(str, 16, "DIST: %d mm  ", dist);
        LCDPrintAt(5, QQVGA_HEIGHT + 10, str);

        int t_x, t_y;
        INReadTouch(&t_x, &t_y);
        if (t_x >= 0 && t_y >= 0)
        {
          ui_init = false;
          screen = SCREEN_SETTINGS_1;
          DRVSetSpeed(0, 0);

          LCDSetFontSize(4);
          LCDSetFontColor(BLACK, RED);
          LCDPrintAt(RESET_X + 23, RESET_Y + 20, "TOUCH");
          LCDPrintAt(RESET_X + 23, RESET_Y + 55, "RESET");
          delay(INPUT_DELAY_MS);
        }

        break;
      }
      default:
      {
        screen = SCREEN_SETTINGS_1;
        ui_init = false;
        break;
      }
    }
  }
  
}

void loop() {
  static bool ui_init = false;
  switch (demo)
  {
    case DEMO_MENU:
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
              demo = CAMERA_FEED;
              ui_init = false;
            }
            else if (touch_y >= 140 && touch_y <= 140 + DEMO_OPTION_HEIGHT)
            {
              demo = DRIVE_STRAIGHT;
              ui_init = false;
            }
            else if (touch_y >= 230 && touch_y <= 230 + DEMO_OPTION_HEIGHT)
            {
              demo = DRIVE_CURVE;
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
              demo = DRIVE_TURN;
              ui_init = false;
            }
            else if (touch_y >= 140 && touch_y <= 140 + DEMO_OPTION_HEIGHT)
            {
              demo = DRIVE_GOTO;
              ui_init = false;
            }
            else if (touch_y >= 230 && touch_y <= 230 + DEMO_OPTION_HEIGHT)
            {
              demo = DRIVE_SET;
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
          LCDPrintAt(20, 168, "HIT->RETURN");

          //Demo Optin 3 Text
          LCDPrintAt(20, 259, "DRV 2 COLOR");

          //Page number
          LCDSetFontSize(1);
          LCDSetFontColor(WHITE);
          char pg_str[8];
          snprintf(pg_str, 8, "Pg. 3/%d", NUM_PAGES);
          LCDPrintAt(LCD_WIDTH - 42, LCD_HEIGHT - 10, pg_str);

          bool left_pressed;
          INReadButton(LEFT_BUTTON, &left_pressed);
          if (left_pressed)
          {
            homePage = HOME_PAGE_2;
            ui_init = false;
            delay(200);
          }

          int touch_x = -1, touch_y = -1;
          INReadTouch(&touch_x, &touch_y);

          if (touch_x >= 5 && touch_x <= LCD_WIDTH - 10)
          {
            if (touch_y >= 50 && touch_y <= 50 + DEMO_OPTION_HEIGHT)
            {
              demo = DISTANCE_SENSOR;
              ui_init = false;
            }
            else if (touch_y >= 140 && touch_y <= 140 + DEMO_OPTION_HEIGHT)
            {
              demo = HIT_AND_RETURN;
              ui_init = false;
            }
            else if (touch_y >= 230 && touch_y <= 230 + DEMO_OPTION_HEIGHT)
            {
              demo = DRIVE_TO_COLOR;
              ui_init = false;
            }

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
        demo = DEMO_MENU;
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
      static Screen screen = SCREEN_SETTINGS_1;

      switch (screen)
      {
        case SCREEN_SETTINGS_1:
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
            demo = DEMO_MENU;
            ui_init = false;
            delay(200);
            break;
          }

          bool right_pressed;
          INReadButton(RIGHT_BUTTON, &right_pressed);

          if (right_pressed)
          {
            screen = SCREEN_SETTINGS_2;
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
            screen = SCREEN_RUNNING;
            ui_init = false;
            delay(200);
          }

          break;
        }
        case SCREEN_SETTINGS_2:
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
            screen = SCREEN_SETTINGS_1;
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
        case SCREEN_RUNNING:
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
            screen = SCREEN_SETTINGS_1;
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
      static Screen screen = SCREEN_SETTINGS_1;

      switch (screen)
      {
        case SCREEN_SETTINGS_1:
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
            demo = DEMO_MENU;
            ui_init = false;
            delay(200);
            break;
          }

          bool right_pressed;
          INReadButton(RIGHT_BUTTON, &right_pressed);

          if (right_pressed)
          {
            screen = SCREEN_SETTINGS_2;
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
            screen = SCREEN_RUNNING;
            ui_init = false;
            delay(200);
          }

          break;
        }
        case SCREEN_SETTINGS_2:
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
            screen = SCREEN_SETTINGS_1;
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
        case SCREEN_RUNNING:
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
            screen = SCREEN_SETTINGS_1;
            ui_init = false;
            DRVSetSpeed(0, 0);
            delay(200);
          }

          break;
        }
        default:
          screen = SCREEN_SETTINGS_1;
          break;
      }

      break;
    }
    case DRIVE_TURN:
    {
      static Screen screen = SCREEN_SETTINGS_1;

      switch (screen)
      {
        case SCREEN_SETTINGS_1:
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
            demo = DEMO_MENU;
            ui_init = false;
            delay(200);
            break;
          }

          bool right_pressed;
          INReadButton(RIGHT_BUTTON, &right_pressed);
          if (right_pressed)
          {
            screen = SCREEN_SETTINGS_2;
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
            screen = SCREEN_RUNNING;
            ui_init = false;
            delay(200);
          }

          break;
        }
        case SCREEN_SETTINGS_2:
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
            screen = SCREEN_SETTINGS_1;
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
        case SCREEN_RUNNING:
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
            screen = SCREEN_SETTINGS_1;
            ui_init = false;
            DRVSetSpeed(0, 0);
            delay(200);
            break;
          }

          break;
        }
        default:
          screen = SCREEN_SETTINGS_1;
          break;
      }
      break;
    }
    case DRIVE_GOTO:
    {
      static Screen screen = SCREEN_SETTINGS_1;
      static int x = 0, y = 0;

      switch (screen)
      {
        case SCREEN_SETTINGS_1:
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
            demo = DEMO_MENU;
            ui_init = false;
            delay(200);
            break;
          }

          bool right_pressed;
          INReadButton(RIGHT_BUTTON, &right_pressed);
          if (right_pressed)
          {
            screen = SCREEN_SETTINGS_2;
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
            screen = SCREEN_RUNNING;
            ui_init = false;
            delay(200);
          }

          break;
        }
        case SCREEN_SETTINGS_2:
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
            screen = SCREEN_SETTINGS_1;
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
        case SCREEN_RUNNING:
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
            screen = SCREEN_SETTINGS_1;
            ui_init = false;
            DRVSetSpeed(0, 0);
            delay(200);
            break;
          }

          break;
        }
        default:
          screen = SCREEN_SETTINGS_1;
          break;
      }
      break;
    }
    case DRIVE_SET:
    {
      static Screen screen = SCREEN_SETTINGS_1;
      static int local_lin_speed = lin_speed, local_ang_speed = ang_speed;

      switch (screen)
      {
        case SCREEN_SETTINGS_1:
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
            demo = DEMO_MENU;
            ui_init = false;
            delay(200);
            break;
          }

          bool right_pressed;
          INReadButton(RIGHT_BUTTON, &right_pressed);
          if (right_pressed)
          {
            screen = SCREEN_SETTINGS_2;
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
            screen = SCREEN_RUNNING;
            ui_init = false;
            delay(200);
          }

          break;
        }
        case SCREEN_SETTINGS_2:
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
            screen = SCREEN_SETTINGS_1;
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
        case SCREEN_RUNNING:
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
            screen = SCREEN_SETTINGS_1;
            ui_init = false;
            DRVSetSpeed(0, 0);
            delay(200);
            break;
          }

          break;
        }
        default:
          screen = SCREEN_SETTINGS_1;
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
        demo = DEMO_MENU;
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
    case HIT_AND_RETURN:
    {
      static Screen screen = SCREEN_SETTINGS_1;
      static int starting_angle;
      static HitAndReturnPhases phase = HIT_PHASE_FORWARD;
      static bool phase_init = false;
      static unsigned long travel_start_time, travel_total_time;

      switch (screen)
      {
        case SCREEN_SETTINGS_1:
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

            //Starting Angle label
            LCDPrintAt(5, 68, "Starting Angle (deg)");
            
            //Starting Angle decrease
            LCDDrawRect(5, 82, 40, 40, WHITE);
            LCDDrawRect(15, 100, 20, 5, BLACK);

            //Starting Angle increase
            LCDDrawRect(125, 82, 40, 40, WHITE);
            LCDDrawRect(135, 100, 20, 5, BLACK);
            LCDDrawRect(143, 92, 5, 21, BLACK);

            //Reverse direction option
            LCDDrawRect(5, 193, 160, 87, WHITE);
            LCDSetFontSize(3);
            LCDSetFontColor(BLACK, WHITE);
            LCDPrintAt(45, 225, "START");

            ui_init = true;
          }

          char val_str[8];
          LCDSetFontSize(2);
          LCDSetFontColor(WHITE);

          //Linear Speed value
          snprintf(val_str, 8, "%d ", lin_speed);
          LCDPrintAt(50, 34, val_str);

          //Starting Angle value
          snprintf(val_str, 8, "%d  ", starting_angle);
          LCDPrintAt(50, 96, val_str);

          bool left_pressed;
          INReadButton(LEFT_BUTTON, &left_pressed);
          if (left_pressed)
          {
            demo = DEMO_MENU;
            ui_init = false;
            delay(200);
            break;
          }

          int touch_x, touch_y;
          INReadTouch(&touch_x, &touch_y);
          if (touch_x >= 5 && touch_x <= 45)
          {
            if (touch_y >= 20 && touch_y <= 60)
              lin_speed -= 10;
            
            if (touch_y >= 82 && touch_y <= 122)
              starting_angle -= 10;

            if (lin_speed < 0)
              lin_speed = 0;

            delay(200);
          }
          else if (touch_x >= 125 && touch_x <= 165)
          {
            if (touch_y >= 20 && touch_y <= 60)
              lin_speed += 10;
            
            if (touch_y >= 82 && touch_y <= 122)
              starting_angle += 10;

            delay(200);
          }
          else if (touch_x >= 5 && touch_x <= 165 && touch_y >= 193 && touch_y <= 280)
          {
            screen = SCREEN_RUNNING;
            ui_init = false;
            delay(200);
          }

          break;
        }
        case SCREEN_RUNNING:
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
            DRVSetPosition(0, 0, starting_angle);

            phase_init = false;
            ui_init = true;
          }

          switch (phase)
          {
            case HIT_PHASE_FORWARD:
            {
              if (!phase_init)
              {
                travel_start_time = millis();
                DRVSetSpeed(lin_speed, 0);
                phase_init = true;
              }

              int dist;
              PSDGet(&dist);

              if (dist <= 70)
              {
                DRVSetSpeed(0, 0);
                travel_total_time = millis() - travel_start_time;
                phase = HIT_PHASE_TURN;
                phase_init = false;
              }

              break;
            }
            case HIT_PHASE_TURN:
            {
              if (!phase_init)
              {
                DRVTurn(180, 180);
                phase_init = true;
              }

              if (DRVDone())
              {
                phase = HIT_PHASE_RETURN;
                phase_init = false;
              }

              break;
            }
            case HIT_PHASE_RETURN:
            {
              if (!phase_init)
              {
                DRVSetSpeed(lin_speed, 0);
                travel_start_time = millis();
                phase_init = true;
              }

              if ((millis() - travel_start_time) >= travel_total_time)
              {
                DRVSetSpeed(0, 0);
                phase = HIT_PHASE_STOP;
                phase_init = false;
              }

              break;
            }
            case HIT_PHASE_STOP:
            {
              DRVSetSpeed(0, 0);
              break;
            }
            default:
            {
              phase = HIT_PHASE_STOP;
              break;
            }
          }

          int touch_x, touch_y;
          INReadTouch(&touch_x, &touch_y);
          if (touch_x >= 0 && touch_y >= 0)
          {
            screen = SCREEN_SETTINGS_1;
            ui_init = false;
            phase_init = false;
            phase = HIT_PHASE_FORWARD;
            DRVSetSpeed(0, 0);
            delay(200);
            break;
          }

          break;
        }
        default:
        {
          screen = SCREEN_SETTINGS_1;
          break;
        }    
      }

      break;
    }
    case DRIVE_TO_COLOR:
    {
      DemoDriveToColor();
      ui_init = false;
      demo = DEMO_MENU;
      break;
    }
    default:
    {
      demo = DEMO_MENU;
      ui_init = false;
      break;
    }
  }
}
