#include <eyebot.h>
#include <stdint.h>

// Delay after input events (buttons or touch)
#define INPUT_DELAY_MS 100
// Minimum distance tolerated before a collision is registered
#define MIN_DIST 100
#define QQVGA_WIDTH 160
#define QQVGA_HEIGHT 120

static COLOR col_img[QQVGA_PIXELS];
static BYTE gray_img[QQVGA_PIXELS];
static uint16_t hori_hist[QQVGA_WIDTH];
static uint16_t vert_hist[QQVGA_HEIGHT];
static int gLCDWidth, gLCDHeight;

enum {
  SCREEN_SETTINGS_1,
  SCREEN_SETTINGS_2,
  SCREEN_RUNNING
} screen = SCREEN_SETTINGS_1;

// Colour currently being tracked
COLOR selected_color = RED;
// HSI thresholds for the colour
int hue_threshold = 10, sat_threshold = 10, int_threshold = 10;
// Linear and angular speed of the EyeBot during the seeking phase
int lin_speed = 260, ang_speed = 120;

void setup() {
  Serial.begin(115200);

  if (int err = EYEBOTInit())
    Serial.printf("Failed to init EyeBot\n");
  
  VWSetOffsets(0, 0);

  LCDGetSize(&gLCDWidth, &gLCDHeight);
}

void loop() {
  switch (screen)
  {
    case SCREEN_SETTINGS_1:
    {
      /*
      The first settings screen where the user can adjust the
      linear and angular speed of the EyeBot for the seeking phase.
      They can access the second settings page by pressing the
      "RESELECT" button, and can initiate the seeking phase by
      pressing "START".
      */
      const int MINUS_X1 = 5,
                MINUS_X2 = MINUS_X1 + 40,
                PLUS_X1 = 125,
                PLUS_X2 = PLUS_X1 + 40;

      const int LIN_Y1 = 20,
                LIN_Y2 = LIN_Y1 + 40;
      
      const int ANG_Y1 = 82,
                ANG_Y2 = ANG_Y1 + 40;
        
      const int RESELECT_X1 = 50,
                RESELECT_X2 = RESELECT_X1 + 115,
                RESELECT_Y1 = 144,
                RESELECT_Y2 = RESELECT_Y1 + 40;
      
      const int START_X1 = 5,
                START_X2 = START_X1 + 160,
                START_Y1 = 191,
                START_Y2 = START_Y1 + 107;

      LCDClear();

      // Linear Speed Widget
      LCDSetFontSize(1);
      LCDSetColor();
      LCDSetPrintf(5, 5, "Linear Speed (mm/s)");
      // Decrease
      LCDArea(MINUS_X1, LIN_Y1, MINUS_X2, LIN_Y2, WHITE);
      LCDArea(MINUS_X1 + 10, LIN_Y1 + 18, MINUS_X1 + 30, LIN_Y1 + 23, BLACK);
      // Increase
      LCDArea(PLUS_X1, LIN_Y1, PLUS_X2, LIN_Y2, WHITE);
      LCDArea(PLUS_X1 + 10, LIN_Y1 + 18, PLUS_X1 + 30, LIN_Y1 + 23, BLACK);
      LCDArea(PLUS_X1 + 18, LIN_Y1 + 10, PLUS_X1 + 23, LIN_Y1 + 32, BLACK);

      // Angular Speed Widget
      LCDSetFontSize(1);
      LCDSetColor();
      LCDSetPrintf(67, 5, "Angular Speed (deg./s)");
      // Decrease
      LCDArea(MINUS_X1, ANG_Y1, MINUS_X2, ANG_Y2, WHITE);
      LCDArea(MINUS_X1 + 10, ANG_Y1 + 18, MINUS_X1 + 30, ANG_Y1 + 23, BLACK);
      // Increase
      LCDArea(PLUS_X1, ANG_Y1, PLUS_X2, ANG_Y2, WHITE);
      LCDArea(PLUS_X1 + 10, ANG_Y1 + 18, PLUS_X1 + 30, ANG_Y1 + 23, BLACK);
      LCDArea(PLUS_X1 + 18, ANG_Y1 + 10, PLUS_X1 + 23, ANG_Y1 + 32, BLACK);        

      BYTE r, g, b;
      IPPCol2RGB(selected_color, &r, &g, &b);
      LCDSetPrintf(129, 5, "Selected Col. (%u,%u,%u)", r, g, b);
      LCDArea(5, RESELECT_Y1, 45, RESELECT_Y2, selected_color);
      LCDArea(RESELECT_X1, RESELECT_Y1, RESELECT_X2, RESELECT_Y2, WHITE);
      LCDSetFontSize(2);
      LCDSetColor(BLACK, WHITE);
      LCDSetPrintf(157, 61, "RESELECT");

      LCDArea(START_X1, START_Y1, START_X2, START_Y2, WHITE);
      LCDSetFontSize(4);
      LCDSetPrintf(START_Y1 + 37, START_X1 + 23, "START");

      while (screen == SCREEN_SETTINGS_1)
      {
        LCDSetFontSize(2);
        LCDSetColor();
        LCDSetPrintf(34, 50, "%d ", lin_speed);
        LCDSetPrintf(96, 50, "%d ", ang_speed);

        int t_x, t_y;
        KEYReadXY(&t_x, &t_y);

        if (t_x >= MINUS_X1 && t_x <= MINUS_X2)
        {
          if (t_y >= LIN_Y1 && t_y <= LIN_Y2)
          {
            lin_speed -= 10;

            if (lin_speed < 0)
              lin_speed = 0;
            
            LCDArea(MINUS_X1 + 10, LIN_Y1 + 18, MINUS_X1 + 30, LIN_Y1 + 23, RED);
            delay(INPUT_DELAY_MS);
            LCDArea(MINUS_X1 + 10, LIN_Y1 + 18, MINUS_X1 + 30, LIN_Y1 + 23, BLACK);
          }
          else if (t_y >= ANG_Y1 && t_y <= ANG_Y2)
          {
            ang_speed -= 10;

            if (ang_speed < 0)
              ang_speed = 0;
            
            LCDArea(MINUS_X1 + 10, ANG_Y1 + 18, MINUS_X1 + 30, ANG_Y1 + 23, RED);
            delay(INPUT_DELAY_MS);
            LCDArea(MINUS_X1 + 10, ANG_Y1 + 18, MINUS_X1 + 30, ANG_Y1 + 23, BLACK);
          }
        }
        else if (t_x >= PLUS_X1 && t_x <= PLUS_X2)
        {
          if (t_y >= LIN_Y1 && t_y <= LIN_Y2)
          {
            lin_speed += 10;

            LCDArea(PLUS_X1 + 10, LIN_Y1 + 18, PLUS_X1 + 30, LIN_Y1 + 23, RED);
            LCDArea(PLUS_X1 + 18, LIN_Y1 + 10, PLUS_X1 + 23, LIN_Y1 + 32, RED);
            delay(INPUT_DELAY_MS);
            LCDArea(PLUS_X1 + 10, LIN_Y1 + 18, PLUS_X1 + 30, LIN_Y1 + 23, BLACK);
            LCDArea(PLUS_X1 + 18, LIN_Y1 + 10, PLUS_X1 + 23, LIN_Y1 + 32, BLACK);
          }
          else if (t_y >= ANG_Y1 && t_y <= ANG_Y2)
          {
            ang_speed += 10;

            LCDArea(PLUS_X1 + 10, ANG_Y1 + 18, PLUS_X1 + 30, ANG_Y1 + 23, RED);
            LCDArea(PLUS_X1 + 18, ANG_Y1 + 10, PLUS_X1 + 23, ANG_Y1 + 32, RED); 
            delay(INPUT_DELAY_MS);
            LCDArea(PLUS_X1 + 10, ANG_Y1 + 18, PLUS_X1 + 30, ANG_Y1 + 23, BLACK);
            LCDArea(PLUS_X1 + 18, ANG_Y1 + 10, PLUS_X1 + 23, ANG_Y1 + 32, BLACK); 
          }
        }
        else if (t_x >= RESELECT_X1 && t_x <= RESELECT_X2 && t_y >= RESELECT_Y1 && t_y <= RESELECT_Y2)
        {
          screen = SCREEN_SETTINGS_2;

          LCDSetFontSize(2);
          LCDSetColor(RED, WHITE);
          LCDSetPrintf(157, 61, "RESELECT");
          delay(INPUT_DELAY_MS);
        }
        else if (t_x >= START_X1 && t_x <= START_X2 && t_y >= START_Y1 && t_y <= START_Y2)
        {
          screen = SCREEN_RUNNING;

          LCDSetFontSize(4);
          LCDSetColor(RED, WHITE);
          LCDSetPrintf(START_Y1 + 37, START_X1 + 23, "START");
          delay(INPUT_DELAY_MS);
        }
      }

      break;
    }
    case SCREEN_SETTINGS_2:
    {
      /*
      From this page the user can re-select the colour to be tracked by pressing
      "SAMPLE", which calculates the average of the centre 9 pixels from the camera
      image. The HSI plus-minus thresholds can also be adjusted, and an overlay
      for the camera image can be toggled by clicking the right button which highlights
      pixels that fall within the HSI thresholds.
      */
      bool show_overlay = false;

      const int WIDGET_WIDTH = 55,
                WIDGET_HEIGHT = 30,
                MINUS_X1 = 50,
                MINUS_X2 = MINUS_X1 + WIDGET_WIDTH,
                PLUS_X1 = 110,
                PLUS_X2 = PLUS_X1 + WIDGET_WIDTH;
      
      const int HUE_Y1 = 180,
                HUE_Y2 = HUE_Y1 + WIDGET_HEIGHT,
                SAT_Y1 = 225,
                SAT_Y2 = SAT_Y1 + WIDGET_HEIGHT,
                INT_Y1 = 270,
                INT_Y2 = INT_Y1 + WIDGET_HEIGHT;
      
      const int SAMPLER_X1 = 50,
                SAMPLER_X2 = SAMPLER_X1 + 115,
                SAMPLER_Y1 = 125,
                SAMPLER_Y2 = SAMPLER_Y1 + 40;

      const int LEFT_BUTTON_X1 = 5,
                LEFT_BUTTON_X2 = LEFT_BUTTON_X1 + 77,
                LEFT_BUTTON_Y1 = gLCDHeight - 15,
                LEFT_BUTTON_Y2 = LEFT_BUTTON_Y1 + 15;
        
      const int RIGHT_BUTTON_X1 = 87,
                RIGHT_BUTTON_X2 = RIGHT_BUTTON_X1 + 78,
                RIGHT_BUTTON_Y1 = gLCDHeight - 15,
                RIGHT_BUTTON_Y2 = RIGHT_BUTTON_Y1 + 15;

      LCDClear();

      // Left Button
      LCDSetColor(BLACK, WHITE);
      LCDSetFontSize(1);
      LCDArea(LEFT_BUTTON_X1, LEFT_BUTTON_Y1, LEFT_BUTTON_X2, LEFT_BUTTON_Y2, WHITE);
      LCDSetPrintf(LEFT_BUTTON_Y1 + 3, LEFT_BUTTON_X1 + 5, "LB: BACK");
      // Right Button
      LCDArea(RIGHT_BUTTON_X1, RIGHT_BUTTON_Y1, RIGHT_BUTTON_X2, RIGHT_BUTTON_Y2, WHITE);
      LCDSetPrintf(RIGHT_BUTTON_Y1 + 3, RIGHT_BUTTON_X1 + 6, "RB: OVERLAY");

      // Hue Theshold Widget
      LCDArea(MINUS_X1, HUE_Y1, MINUS_X2, HUE_Y2, WHITE);
      LCDArea(MINUS_X1 + 20, HUE_Y1 + 13, MINUS_X1 + 36, HUE_Y1 + 17, BLACK);
      LCDArea(PLUS_X1, HUE_Y1, PLUS_X2, HUE_Y2, WHITE);
      LCDArea(PLUS_X1 + 20, HUE_Y1 + 13, PLUS_X1 + 36, HUE_Y1 + 17, BLACK);
      LCDArea(PLUS_X1 + 26, HUE_Y1 + 7, PLUS_X1 + 30, HUE_Y1 + 23, BLACK);
      LCDSetFontSize(1);
      LCDSetColor();
      LCDSetPrintf(HUE_Y1 - 10, 5, "Hue Threshold:");
      // Plus symbol
      LCDArea(6, HUE_Y1 + 9, 7, HUE_Y1 + 14, WHITE);
      LCDArea(4, HUE_Y1 + 11, 9, HUE_Y1 + 12, WHITE);
      // Minus symbol
      LCDArea(4, HUE_Y1 + 17, 9, HUE_Y1 + 18, WHITE);

      // Saturation Threshod Widget
      LCDArea(MINUS_X1, SAT_Y1, MINUS_X2, SAT_Y2, WHITE);
      LCDArea(MINUS_X1 + 20, SAT_Y1 + 13, MINUS_X1 + 36, SAT_Y1 + 17, BLACK);
      LCDArea(PLUS_X1, SAT_Y1, PLUS_X2, SAT_Y2, WHITE);
      LCDArea(PLUS_X1 + 20, SAT_Y1 + 13, PLUS_X1 + 36, SAT_Y1 + 17, BLACK);
      LCDArea(PLUS_X1 + 26, SAT_Y1 + 7, PLUS_X1 + 30, SAT_Y1 + 23, BLACK);
      LCDSetFontSize(1);
      LCDSetColor();
      LCDSetPrintf(SAT_Y1 - 10, 5, "Saturation Threshold:");
      // Plus symbol
      LCDArea(6, SAT_Y1 + 9, 7, SAT_Y1 + 14, WHITE);
      LCDArea(4, SAT_Y1 + 11, 9, SAT_Y1 + 12, WHITE);
      // Minus symbol
      LCDArea(4, SAT_Y1 + 17, 9, SAT_Y1 + 18, WHITE);

      // Intensity Threshod Widget
      LCDArea(MINUS_X1, INT_Y1, MINUS_X2, INT_Y2, WHITE);
      LCDArea(MINUS_X1 + 20, INT_Y1 + 13, MINUS_X1 + 36, INT_Y1 + 17, BLACK);
      LCDArea(PLUS_X1, INT_Y1, PLUS_X2, INT_Y2, WHITE);
      LCDArea(PLUS_X1 + 20, INT_Y1 + 13, PLUS_X1 + 36, INT_Y1 + 17, BLACK);
      LCDArea(PLUS_X1 + 26, INT_Y1 + 7, PLUS_X1 + 30, INT_Y1 + 23, BLACK);
      LCDSetFontSize(1);
      LCDSetColor();
      LCDSetPrintf(INT_Y1 - 10, 5, "Intensity Threshold:");
      // Plus symbol
      LCDArea(6, INT_Y1 + 9, 7, INT_Y1 + 14, WHITE);
      LCDArea(4, INT_Y1 + 11, 9, INT_Y1 + 12, WHITE);
      // Minus symbol
      LCDArea(4, INT_Y1 + 17, 9, INT_Y1 + 18, WHITE);

      // Colour Sampler
      LCDArea(SAMPLER_X1, SAMPLER_Y1, SAMPLER_X2, SAMPLER_Y2, WHITE);
      LCDSetFontSize(2);
      LCDSetColor(BLACK, WHITE);
      LCDSetPrintf(SAMPLER_Y1 + 13, SAMPLER_X1 + 25, "SAMPLE");

      while (screen == SCREEN_SETTINGS_2)
      {
        CAMGet((BYTE*)col_img);

        int sum_r = 0, sum_g = 0,  sum_b = 0, num_pixels = 0;
        const int y_start = (CAMHEIGHT >> 1) - 1;
        const int y_finish = y_start + 3;
        const int x_start = (CAMWIDTH >> 1) - 1;
        const int x_finish = x_start + 3;

        for (int y = y_start; y <= y_finish; y++)
        {
          for (int x = x_start; x <= x_finish; x++)
          {
            byte r, g, b;
            IPPCol2RGB(col_img[y*CAMWIDTH+x], &r, &g, &b);
            sum_r += r;
            sum_g += g;
            sum_b += b;

            num_pixels++;
          }
        }

        COLOR sampled_pixel = IPPRGB2Col(sum_r / num_pixels, sum_g / num_pixels, sum_b / num_pixels);

        // Sampled Colour
        LCDArea(5, 125, 45, 165, sampled_pixel);

        LCDImageStart(5, 0, CAMWIDTH, CAMHEIGHT);

        if (show_overlay)
        {
          BYTE h1, s1, i1;
          IPPCol2HSI(selected_color, &h1, &s1, &i1);

          for (int y = 0; y < CAMHEIGHT; y++)
          {
            for (int x = 0; x < CAMWIDTH; x++)
            {
              BYTE h2, s2, i2;
              IPPCol2HSI(col_img[y*CAMWIDTH + x], &h2, &s2, &i2);

              if (h2 >= (h1 - hue_threshold) && h2 <= (h1 + hue_threshold) &&
                  s2 >= (s1 - sat_threshold) && s2 <= (s1 + sat_threshold) &&
                  i2 >= (i1 - int_threshold) && i2 <= (i1 + int_threshold))
              {
                gray_img[y*CAMWIDTH + x] = 0xFF;             
              }
              else
              {
                gray_img[y*CAMWIDTH + x] = 0;
              }
            }
          }

          LCDImageBinary((BYTE*)gray_img);
        }
        else 
        {
          LCDImage((BYTE*)col_img);
          // Sampled Colour crosshair
          LCDArea(5 + CAMWIDTH/2, CAMHEIGHT/2 - 2, 6 + CAMWIDTH/2,  CAMHEIGHT/2 + 3, RED);
          LCDArea(3 + CAMWIDTH/2, CAMHEIGHT/2, 8 + CAMWIDTH/2, 1 + CAMHEIGHT/2, RED);
        }

        int VAL_X = 12, 
            VAL_Y = 7;

        LCDSetColor();
        LCDSetFontSize(2);

        // Hue Threshold Values
        LCDSetPrintf(HUE_Y1 + VAL_Y, VAL_X, "%d ", hue_threshold);
        // Saturation Threshold Values
        LCDSetPrintf(SAT_Y1 + VAL_Y, VAL_X, "%d ", sat_threshold);
        // Intensity Theshold Values
        LCDSetPrintf(INT_Y1 + VAL_Y, VAL_X, "%d ", int_threshold);

        int pressed_key = KEYRead();
        int left_pressed = pressed_key & KEY1;
        int right_pressed = pressed_key & KEY2;

        if (left_pressed)
        {
          screen = SCREEN_SETTINGS_1;

          LCDSetColor(RED, WHITE);
          LCDSetFontSize(1);
          LCDSetPrintf(LEFT_BUTTON_Y1 + 3, LEFT_BUTTON_X1 + 5, "LB: BACK");
          delay(INPUT_DELAY_MS);
        }

        if (right_pressed)
        {
          show_overlay = !show_overlay;

          LCDSetColor(RED, WHITE);
          LCDSetFontSize(1);
          LCDSetPrintf(RIGHT_BUTTON_Y1 + 3, RIGHT_BUTTON_X1 + 6, "RB: OVERLAY");
          delay(INPUT_DELAY_MS);
          LCDSetColor(BLACK, WHITE);
          LCDSetPrintf(RIGHT_BUTTON_Y1 + 3, RIGHT_BUTTON_X1 + 6, "RB: OVERLAY");
        }

        int t_x, t_y;
        KEYReadXY(&t_x, &t_y);

        if (t_x >= SAMPLER_X1 && t_x <= SAMPLER_X2 && t_y >= SAMPLER_Y1 && t_y <= SAMPLER_Y2)
        {
          selected_color = sampled_pixel;
          LCDSetFontSize(2);
          LCDSetColor(RED, WHITE);
          LCDSetPrintf(SAMPLER_Y1 + 13, SAMPLER_X1 + 25, "SAMPLE");
          delay(INPUT_DELAY_MS);
          LCDSetColor(BLACK, WHITE);
          LCDSetPrintf(SAMPLER_Y1 + 13, SAMPLER_X1 + 25, "SAMPLE");
        }
        else if (t_x >= MINUS_X1 && t_x <= MINUS_X2)
        {
          if (t_y >= HUE_Y1 && t_y <= HUE_Y2)
          {
            hue_threshold--;
            if (hue_threshold < 0)
              hue_threshold = 0;
            
            LCDArea(MINUS_X1 + 20, HUE_Y1 + 13, MINUS_X1 + 36, HUE_Y1 + 17, RED);
            delay(INPUT_DELAY_MS);
            LCDArea(MINUS_X1 + 20, HUE_Y1 + 13, MINUS_X1 + 36, HUE_Y1 + 17, BLACK);
          }
          else if (t_y >= SAT_Y1 && t_y <= SAT_Y2)
          {
            sat_threshold--;
            if (sat_threshold < 0)
              sat_threshold = 0;
            
            LCDArea(MINUS_X1 + 20, SAT_Y1 + 13, MINUS_X1 + 36, SAT_Y1 + 17, RED);
            delay(INPUT_DELAY_MS);
            LCDArea(MINUS_X1 + 20, SAT_Y1 + 13, MINUS_X1 + 36, SAT_Y1 + 17, BLACK);
          }
          else if (t_y >= INT_Y1 && t_y <= INT_Y2)
          {
            int_threshold--;
            if (int_threshold < 0)
              int_threshold = 0;
            
            LCDArea(MINUS_X1 + 20, INT_Y1 + 13, MINUS_X1 + 36, INT_Y1 + 17, RED);
            delay(INPUT_DELAY_MS);
            LCDArea(MINUS_X1 + 20, INT_Y1 + 13, MINUS_X1 + 36, INT_Y1 + 17, RED);
          }
        }
        else if (t_x >= PLUS_X1 && t_x <= PLUS_X2)
        {
          if (t_y >= HUE_Y1 && t_y <= HUE_Y2)
          {
            hue_threshold++;   

            LCDArea(PLUS_X1 + 20, HUE_Y1 + 13, PLUS_X1 + 36, HUE_Y1 + 17, RED);
            LCDArea(PLUS_X1 + 26, HUE_Y1 + 7, PLUS_X1 + 30, HUE_Y1 + 23, RED);
            delay(INPUT_DELAY_MS);
            LCDArea(PLUS_X1 + 20, HUE_Y1 + 13, PLUS_X1 + 36, HUE_Y1 + 17, BLACK);
            LCDArea(PLUS_X1 + 26, HUE_Y1 + 7, PLUS_X1 + 30, HUE_Y1 + 23, BLACK);
          }
          else if (t_y >= SAT_Y1 && t_y <= SAT_Y2)
          {
            sat_threshold++;  

            LCDArea(PLUS_X1 + 20, SAT_Y1 + 13, PLUS_X1 + 36, SAT_Y1 + 17, RED);
            LCDArea(PLUS_X1 + 26, SAT_Y1 + 7, PLUS_X1 + 30, SAT_Y1 + 23, RED);
            delay(INPUT_DELAY_MS);
            LCDArea(PLUS_X1 + 20, SAT_Y1 + 13, PLUS_X1 + 36, SAT_Y1 + 17, BLACK);
            LCDArea(PLUS_X1 + 26, SAT_Y1 + 7, PLUS_X1 + 30, SAT_Y1 + 23, BLACK);
          }
          else if (t_y >= INT_Y1 && t_y <= INT_Y2)
          {
            int_threshold++;

            LCDArea(PLUS_X1 + 20, INT_Y1 + 13, PLUS_X1 + 36, INT_Y1 + 17, RED);
            LCDArea(PLUS_X1 + 26, INT_Y1 + 7, PLUS_X1 + 30, INT_Y1 + 23, RED);
            delay(INPUT_DELAY_MS);
            LCDArea(PLUS_X1 + 20, INT_Y1 + 13, PLUS_X1 + 36, INT_Y1 + 17, BLACK);
            LCDArea(PLUS_X1 + 26, INT_Y1 + 7, PLUS_X1 + 30, INT_Y1 + 23, BLACK);
          }
        }
      }

      break;
    }
    case SCREEN_RUNNING:
    {
      /*
      To drive towards the largest object of the specified colour, a colour histogram is created 
      which records the row and column in the image which cotains the highest number of pixels 
      that fall within the colour threshold. A crosshair is generated from this row and column,
      and depending how far to the left or right of the image the crosshair falls, the EyeBot turns
      to have the crosshair fall into the horizontal centre region. The EyeBot continues to drive
      forward while the crosshair lies within the horizontal centre region, and stops if it detects
      anything that comes too close to the distance sensor.
      */

      enum {
        PHASE_SEEKING, 
        PHASE_COLLISION
      } phase = PHASE_SEEKING;

      const int RESET_X1 = 5,
                RESET_X2 = RESET_X1 + gLCDWidth - 10,
                RESET_Y1 = 2 * gLCDHeight / 3,
                RESET_Y2 = gLCDHeight - 5;

      LCDClear();

      LCDArea(RESET_X1, RESET_Y1, RESET_X2, RESET_Y2, RED);
      LCDSetFontSize(4);
      LCDSetColor(WHITE, RED);
      LCDSetPrintf(RESET_Y1 + 20, RESET_X1 + 23, "TOUCH");
      LCDSetPrintf(RESET_Y1 + 55, RESET_X1 + 23, "RESET");

      phase = PHASE_SEEKING;

      while (screen == SCREEN_RUNNING)
      {
        CAMGet((BYTE*)col_img);

        memset(hori_hist, 0, QQVGA_WIDTH*sizeof(uint16_t));
        memset(vert_hist, 0, QQVGA_HEIGHT*sizeof(uint16_t));

        BYTE h1, s1, i1;
        IPPCol2HSI(selected_color, &h1, &s1, &i1);

        int hori_hist_max_idx = 0, vert_hist_max_idx = 0;

        for (int y = 0; y < QQVGA_HEIGHT; y++)
        {
          for (int x = 0; x < QQVGA_WIDTH; x++)
          {
            BYTE h2, s2, i2;
            IPPCol2HSI(col_img[y*QQVGA_WIDTH + x], &h2, &s2, &i2);

            if (h2 >= (h1 - hue_threshold) && h2 <= (h1 + hue_threshold) &&
                s2 >= (s1 - sat_threshold) && s2 <= (s1 + sat_threshold) &&
                i2 >= (i1 - int_threshold) && i2 <= (i1 + int_threshold))
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

        int dist = PSDGet(PSD_FRONT);

        switch (phase)
        {
          case PHASE_SEEKING:
          {           
            if (dist > MIN_DIST)
            {
              if (hori_hist_max_idx <= QQVGA_WIDTH / 4)// If in first horizontal quadrant
                VWSetSpeed(0, -1*ang_speed);
              else if (hori_hist_max_idx >= 3 * QQVGA_WIDTH / 4)// If in the last horizontal quadrant
                VWSetSpeed(0, ang_speed);
              else
                VWSetSpeed(lin_speed, 0);
            }
            else
              phase = PHASE_COLLISION;

            break;
          }
          case PHASE_COLLISION:
          {
            VWSetSpeed(0, 0);
            LCDSetFontSize(2);
            LCDSetColor(RED, BLACK);
            LCDSetPrintf(QQVGA_HEIGHT + 30, 5, "COLLISION");
            break;
          }
          default:
          {
            screen = SCREEN_SETTINGS_1;
            VWSetSpeed(0, 0);
            break;
          }
        }

        LCDImageStart(5, 5, CAMWIDTH, CAMHEIGHT);
        LCDImage((BYTE*)col_img);
        LCDArea(5 + hori_hist_max_idx, 5, 6 + hori_hist_max_idx, 5 + CAMHEIGHT, RED);
        LCDArea(5, 5 + vert_hist_max_idx, 5 + CAMWIDTH, 6 + vert_hist_max_idx, RED);

        LCDSetFontSize(2);
        LCDSetColor();
        LCDSetPrintf(CAMHEIGHT + 10, 5, "DIST: %d mm ", dist);

        int t_x, t_y;
        KEYReadXY(&t_x, &t_y);

        if (t_x >= 0 && t_y >= 0)
        {
          screen = SCREEN_SETTINGS_1;
          VWSetSpeed(0, 0);

          LCDSetFontSize(4);
          LCDSetColor(BLACK, RED);

          LCDSetPrintf(RESET_Y1 + 20, RESET_X1 + 23, "TOUCH");
          LCDSetPrintf(RESET_Y1 + 55, RESET_X1 + 23, "RESET");
          delay(INPUT_DELAY_MS);
        }
      }

      break;
    }
    default:
      screen = SCREEN_SETTINGS_1;
      break;
  }
}
