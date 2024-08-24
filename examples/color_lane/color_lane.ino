#include <eyebot.h>
#include <math.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define INPUT_DELAY_MS 100
#define GRAY_THRESHOLD 35
#define GRAY_COUNT_THRESHOLD 10
#define LANE_PX_WIDTH 20
#define LANE_X_RANGE 5
#define NUM_LONGEST_SETS 3

#define QQVGA_HEIGHT 120

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

enum Phase {
  PHASE_SETTINGS,
  PHASE_TESTING,
  PHASE_NAVIGATING
};

typedef struct {
  int head_idx;
  int tail_idx;
  int len;
} LaneSet;

COLOR pColImg[QQVGA_PIXELS];
BYTE pGrayImg[QQVGA_PIXELS];
BYTE pBinImg[QQVGA_PIXELS];
u16 pGrayLevelCount[256];

LaneSet *pSets = NULL;
int gSetsMaxLen = -1;

int gMaxLinSpeed = 170, gMaxAngSpeed = 60;
int gLCDWidth = -1, gLCDHeight = -1;
Phase gPhase = PHASE_TESTING;

u16 v1[QQVGA_HEIGHT];
u16 v2[QQVGA_HEIGHT];
u16 v3[QQVGA_HEIGHT];
u16 uk[QQVGA_HEIGHT];
float phi1[QQVGA_HEIGHT];
float phi2[QQVGA_HEIGHT];
float phi3[QQVGA_HEIGHT];
float aks[NUM_LONGEST_SETS][QQVGA_HEIGHT];
float bks[NUM_LONGEST_SETS][QQVGA_HEIGHT];
float cks[NUM_LONGEST_SETS][QQVGA_HEIGHT];

void settings_screen()
{
  bool screen_initd = false, quit = false;

  const int MINUS_X1 = 5,
            MINUS_X2 = MINUS_X1 + 40,
            PLUS_X1 = 125,
            PLUS_X2 = PLUS_X1 + 40;

  const int LIN_Y1 = 20,
            LIN_Y2 = LIN_Y1 + 40;

  const int ANG_Y1 = LIN_Y1 + 62,
            ANG_Y2 = ANG_Y1 + 40;
  
  const int TEST_X1 = 5,
            TEST_X2 = 165,
            TEST_Y1 = ANG_Y1 + 52,
            TEST_Y2 = TEST_Y1 + 80;
  
  const int START_X1 = 5,
            START_X2 = 165,
            START_Y1 = TEST_Y2 + 10,
            START_Y2 = START_Y1 + 80;

  while (!quit)
  {
    if (!screen_initd)
    {
      LCDClear();

      // Max Linear Speed Widget
      LCDSetFontSize(1);
      LCDSetColor();
      LCDSetPrintf(5, 5, "Max Linear Speed (mm/s)");
      // Decrease
      LCDArea(MINUS_X1, LIN_Y1, MINUS_X2, LIN_Y2, WHITE);
      LCDArea(MINUS_X1 + 10, LIN_Y1 + 18, MINUS_X1 + 30, LIN_Y1 + 23, BLACK);
      // Increase
      LCDArea(PLUS_X1, LIN_Y1, PLUS_X2, LIN_Y2, WHITE);
      LCDArea(PLUS_X1 + 10, LIN_Y1 + 18, PLUS_X1 + 30, LIN_Y1 + 23, BLACK);
      LCDArea(PLUS_X1 + 18, LIN_Y1 + 10, PLUS_X1 + 23, LIN_Y1 + 32, BLACK);

      // Max Angular Speed Widget
      LCDSetFontSize(1);
      LCDSetColor();
      LCDSetPrintf(67, 5, "Max Angular Speed (deg./s)");
      // Decrease
      LCDArea(MINUS_X1, ANG_Y1, MINUS_X2, ANG_Y2, WHITE);
      LCDArea(MINUS_X1 + 10, ANG_Y1 + 18, MINUS_X1 + 30, ANG_Y1 + 23, BLACK);
      // Increase
      LCDArea(PLUS_X1, ANG_Y1, PLUS_X2, ANG_Y2, WHITE);
      LCDArea(PLUS_X1 + 10, ANG_Y1 + 18, PLUS_X1 + 30, ANG_Y1 + 23, BLACK);
      LCDArea(PLUS_X1 + 18, ANG_Y1 + 10, PLUS_X1 + 23, ANG_Y1 + 32, BLACK);

      LCDSetFontSize(3);
      LCDSetColor(BLACK, WHITE);
      // Testing button
      LCDArea(TEST_X1, TEST_Y1, TEST_X2, TEST_Y2, WHITE);
      LCDSetPrintf(TEST_Y1 + 30, TEST_X1 + 46, "TEST");

      // Start button
      LCDArea(START_X1, START_Y1, START_X2, START_Y2, WHITE);
      LCDSetPrintf(START_Y1 + 30, START_X1 + 38, "START");

      screen_initd = true;
    }

    LCDSetFontSize(2);
    LCDSetColor();

    LCDSetPrintf(LIN_Y1 + 14, MINUS_X2 + 5, "%d ", gMaxLinSpeed);
    LCDSetPrintf(ANG_Y1 + 14, MINUS_X2 + 5, "%d ", gMaxAngSpeed);

    int tx, ty;
    KEYReadXY(&tx, &ty);
    if (tx >= MINUS_X1 && tx <= MINUS_X2)
    {
      if (ty >= LIN_Y1 && ty <= LIN_Y2)
      {
        gMaxLinSpeed -= 10;
        if (gMaxLinSpeed < 0) gMaxLinSpeed = 0;
        
        LCDArea(MINUS_X1 + 10, LIN_Y1 + 18, MINUS_X1 + 30, LIN_Y1 + 23, RED);
        delay(INPUT_DELAY_MS);
        LCDArea(MINUS_X1 + 10, LIN_Y1 + 18, MINUS_X1 + 30, LIN_Y1 + 23, BLACK);
      }
      else if (ty >= ANG_Y1 && ty <= ANG_Y2)
      {
        gMaxAngSpeed -= 10;
        if (gMaxAngSpeed < 0) gMaxAngSpeed = 0;

        LCDArea(MINUS_X1 + 10, ANG_Y1 + 18, MINUS_X1 + 30, ANG_Y1 + 23, RED);
        delay(INPUT_DELAY_MS);
        LCDArea(MINUS_X1 + 10, ANG_Y1 + 18, MINUS_X1 + 30, ANG_Y1 + 23, BLACK);
      }
    }
    else if (tx >= PLUS_X1 && tx <= PLUS_X2)
    {
      if (ty >= LIN_Y1 && ty <= LIN_Y2)
      {
        gMaxLinSpeed += 10;

        LCDArea(PLUS_X1 + 10, LIN_Y1 + 18, PLUS_X1 + 30, LIN_Y1 + 23, RED);
        LCDArea(PLUS_X1 + 18, LIN_Y1 + 10, PLUS_X1 + 23, LIN_Y1 + 32, RED);
        delay(INPUT_DELAY_MS);
        LCDArea(PLUS_X1 + 10, LIN_Y1 + 18, PLUS_X1 + 30, LIN_Y1 + 23, BLACK);
        LCDArea(PLUS_X1 + 18, LIN_Y1 + 10, PLUS_X1 + 23, LIN_Y1 + 32, BLACK);
      }
      else if (ty >= ANG_Y1 && ty <= ANG_Y2)
      {
        gMaxAngSpeed += 10;

        LCDArea(PLUS_X1 + 10, ANG_Y1 + 18, PLUS_X1 + 30, ANG_Y1 + 23, RED);
        LCDArea(PLUS_X1 + 18, ANG_Y1 + 10, PLUS_X1 + 23, ANG_Y1 + 32, RED);
        delay(INPUT_DELAY_MS);
        LCDArea(PLUS_X1 + 10, ANG_Y1 + 18, PLUS_X1 + 30, ANG_Y1 + 23, BLACK);
        LCDArea(PLUS_X1 + 18, ANG_Y1 + 10, PLUS_X1 + 23, ANG_Y1 + 32, BLACK);
      }
    }
    else if (tx >= TEST_X1 && tx <= TEST_X2 && ty >= TEST_Y1 && ty <= TEST_Y2)
    {
      quit = true;
      gPhase = PHASE_TESTING;
      LCDSetFontSize(3);
      LCDSetColor(RED, WHITE);
      LCDSetPrintf(TEST_Y1 + 30, TEST_X1 + 46, "TEST");
      delay(INPUT_DELAY_MS);
      continue;
    }
    else if (tx >= START_X1 && tx <= START_X2 && ty >= START_Y1 && ty <= START_Y2)
    {
      quit = true;
      gPhase = PHASE_NAVIGATING;
      LCDSetFontSize(3);
      LCDSetColor(RED, WHITE);
      LCDSetPrintf(START_Y1 + 30, START_X1 + 38, "START");
      delay(INPUT_DELAY_MS);
      continue;
    }
  }
}

void testing_screen()
{
  bool quit = false;
  
  const int Y_OFFSET = (CAMHEIGHT >> 1) + 20;
  const int HEIGHT = CAMHEIGHT - Y_OFFSET;

  LCDClear();

  COLOR *col_subimg = pColImg + Y_OFFSET*CAMWIDTH;
  BYTE *gray_subimg = pGrayImg + Y_OFFSET*CAMWIDTH;
  BYTE *bin_subimg = pBinImg + Y_OFFSET*CAMWIDTH;

  const float avg_div = 1/3.0f;

  for (int i = 0; i < QQVGA_HEIGHT; i++)
  {
    v1[i] = 1;
    phi1[i] = 1.0f;
  }

  while(!quit)
  {
    CAMGet((BYTE*)pColImg);
    COLOR *col_subimg = pColImg + Y_OFFSET*CAMWIDTH;

    LCDImageStart(5, 5, CAMWIDTH, HEIGHT);
    LCDImage((BYTE*)col_subimg);

    memset(pGrayLevelCount, 0, sizeof(pGrayLevelCount));
    memset(bin_subimg, 0, HEIGHT*CAMWIDTH);

    int gray_level_count_max_idx = 0;

    for (int y = 0; y < HEIGHT; y++)
    for (int x = 0; x < CAMWIDTH; x++)
    {
      int i = y*CAMWIDTH + x;
      BYTE r, g, b;
      IPPCol2RGB(col_subimg[i], &r, &g, &b);
      int ri = r, gi = g, bi = b;
      int gray_val = (ri + gi + bi)*avg_div;
      if (gray_val > 255) gray_val = 255;
      gray_subimg[i] = gray_val;

      if (abs(ri - gi) < GRAY_THRESHOLD &&
          abs(gi - bi) < GRAY_THRESHOLD &&
          abs(bi - ri) < GRAY_THRESHOLD)
      {
        pGrayLevelCount[gray_val]++;

        if (pGrayLevelCount[gray_val] > pGrayLevelCount[gray_level_count_max_idx])
          gray_level_count_max_idx = gray_val;

        bin_subimg[i] = 0xFF;// possible road pixel
      }
      else
      {
        bin_subimg[i] = 0;// non-road pixel
      }
    }

    int threshold_head_idx = gray_level_count_max_idx, threshold_tail_idx = 0;

    for (int i = 1; i < gray_level_count_max_idx; i++)
    {
      if (pGrayLevelCount[i - 1] <= GRAY_COUNT_THRESHOLD && 
          pGrayLevelCount[i] > GRAY_COUNT_THRESHOLD)
      {
        threshold_tail_idx = i;
      }
    }

    for (int i = gray_level_count_max_idx; i < 255; i++)
    {
      if (pGrayLevelCount[i] >= GRAY_COUNT_THRESHOLD && 
          pGrayLevelCount[i + 1] < GRAY_COUNT_THRESHOLD)
      {
        threshold_head_idx = i;
      }
    }

    int threshold = sqrtf((powf(threshold_head_idx, 2) + powf(threshold_tail_idx, 2))/2);

    for (int y = 0; y < HEIGHT; y++)
    for (int x = 0; x < CAMWIDTH; x++)
    {
      int i = y*CAMWIDTH + x;
      if (bin_subimg[i])// Is a gray pixel
        bin_subimg[i] = gray_subimg[i] > threshold ? 0xFF : 0; //Now marks a possible lane
    }

    LCDImageStart(5, 5 + HEIGHT + 5, CAMWIDTH, HEIGHT);
    LCDImageGray(bin_subimg);

    for (int y = 0; y < HEIGHT; y++)
    {
      int lane_left_edge_idx = -1;

      for (int x = 0; x < CAMWIDTH; x++)
      {
        int i = y*CAMWIDTH + x;

        if (bin_subimg[i])
        {
          if (lane_left_edge_idx < 0)
            lane_left_edge_idx = i;
        }
        else if (lane_left_edge_idx >= 0)
        {
          if ((i - lane_left_edge_idx) <= LANE_PX_WIDTH)
            bin_subimg[(i + lane_left_edge_idx) >> 1] = 0xFF;// Mark the midpoint of the possible lane
          
          lane_left_edge_idx = -1;
        }

        bin_subimg[i] = 0;
      }
    }

    LCDImageStart(5, 5 + 2*(HEIGHT + 5), CAMWIDTH, HEIGHT);
    LCDImageGray(bin_subimg);

    int sets_len = 0;

    // Find continuous sets of points that form curves
    for (int y = 1; y < HEIGHT; y++)
    for (int x = LANE_X_RANGE; x < CAMWIDTH - LANE_X_RANGE; x++)
    {
      int i = y*CAMWIDTH + x;
      if (bin_subimg[i])
      {
        int ul = (y - 1)*CAMWIDTH + (x - LANE_X_RANGE);
        int ur = (y - 1)*CAMWIDTH + (x + LANE_X_RANGE);

        bool set_exists = false;

        for (int u = ul; u <= ur; u++)
        {
          if (bin_subimg[u])
          {
            for (int s = 0; s < sets_len; s++)
            {
              if (pSets[s].tail_idx == u)
              {
                pSets[s].tail_idx = i;
                pSets[s].len++;
                set_exists = true;
                break;
              }
            }

            if (set_exists) break;
          }
        }

        if (!set_exists && sets_len < gSetsMaxLen)
        {
          pSets[sets_len] = {.head_idx = i, .tail_idx = i, .len = 1};
          sets_len++;
        }
      }
    }

    int longest_idxs[NUM_LONGEST_SETS] = {0};

    // Find top three longest sets
    
    for (int i = 0; i < sets_len; i++)
    {
      for (int k = 0; k < NUM_LONGEST_SETS; k++)
      {
        if (pSets[i].len > pSets[k].len)
        {
          longest_idxs[k] = i;
          break;
        }
      }
    }

    // Least-Square Method of finding matching quadratic curve

    for (int k = 0; k < NUM_LONGEST_SETS; k++)
    {
      // Match curve for longest set
      LaneSet set = pSets[longest_idxs[k]];
      u16 y0 = set.head_idx / CAMWIDTH;
      u16 x0 = set.head_idx % CAMWIDTH;
      v2[0] = y0;
      v3[0] = y0 * y0;
      uk[0] = x0;

      float v2_dot_phi1 = v2[0] * phi1[0];
      float v3_dot_phi1 = v3[0] * phi1[0];
      float phi1_mag_sq = phi1[0] * phi1[0];

      int head = set.head_idx;

      for (int i = 1; i < set.len; i++)
      {
        int bl = head + i*CAMWIDTH - LANE_X_RANGE;
        int br = head + i*CAMWIDTH + LANE_X_RANGE;

        for (int b = bl; b <= br; b++)
        {
          if (bin_subimg[b])// Assumes that it will be found
          {
            u16 yi = b / CAMWIDTH;
            u16 xi = b % CAMWIDTH;
            v2[i] = yi;
            v3[i] = yi*yi;
            uk[i] = xi;
            head = b;

            v2_dot_phi1 += v2[i] * phi1[i];
            v3_dot_phi1 += v3[i] * phi1[i];
            phi1_mag_sq += phi1[i] * phi1[i];

            break;
          }
        }
      }

      float first_phi2_coeff = v2_dot_phi1 / (float)set.len;
      float first_phi3_coeff = v3_dot_phi1 / (float)set.len;

      float v3_dot_phi2 = 0.0f;
      float phi2_mag_sq = 0.0f;

      for (int i = 0; i < set.len; i++)
      {
        phi2[i] = (float)v2[i] - first_phi2_coeff * phi1[i];
        phi3[i] = (float)v3[i] - first_phi3_coeff * phi1[i];

        v3_dot_phi2 += v3[i] * phi2[i];
        phi2_mag_sq += phi2[i] * phi2[i];
      }

      float second_phi3_coeff = v3_dot_phi2 / (float)phi2_mag_sq;
      float phi3_mag_sq = 0;

      float uk_dot_phi3 = 0.0f, uk_dot_phi2 = 0.0f, uk_dot_phi1 = 0.0f;

      for (int i = 0; i < set.len; i++)
      {
        phi3[i] -= second_phi3_coeff * phi2[i];
        phi3_mag_sq += phi3[i] * phi3[i];

        uk_dot_phi3 += uk[i] * phi3[i];
        uk_dot_phi2 += uk[i] * phi2[i];
        uk_dot_phi1 += uk[i] * phi1[i];
      }

      float ak_coeff = uk_dot_phi3 / phi3_mag_sq;
      float bk_coeff = uk_dot_phi2 / phi2_mag_sq;
      float ck_coeff = uk_dot_phi1 / phi1_mag_sq;

      for (int i = 0; i < set.len; i++)
      {
        aks[k][i] = ak_coeff * phi3[i];
        bks[k][i] = bk_coeff * phi2[i];
        cks[k][i] = ck_coeff * phi1[i];
      }
    }
  }
}

void navigation_screen()
{

}

void setup() 
{
  Serial.begin(115200);

  EYEBOTInit();

  gSetsMaxLen = QQVGA_PIXELS >> 1;
  pSets = (LaneSet*)malloc(sizeof(LaneSet)*gSetsMaxLen);
  if (!pSets)
    Serial.printf("Failed to allocate pSets\n");
}

void loop() 
{
  switch (gPhase)
  {
    case PHASE_SETTINGS:
      settings_screen();
      break;
    case PHASE_TESTING:
      testing_screen();
      break;
    case PHASE_NAVIGATING:
      navigation_screen();
      break;
    default:
      gPhase = PHASE_SETTINGS;
      break;
  }
}
