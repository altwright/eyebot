#include <eyebot.h>
#include <math.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define INPUT_DELAY_MS 100
#define MIN_LINE_LEN 6

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

enum Phase {
  PHASE_SETTINGS,
  PHASE_TESTING,
  PHASE_NAVIGATING
};

//!
//! Sliding kernel accumulation has 4 cases:
//! 1. left side out and right side in
//! 2. left side in and right side in
//! 3. left side in and right side out
//! 4. left side out and right side out
//!
//! Small (S) kernels corresponds to kernels with radius < width; r < w
//! Mid   (M) kernels corresponds to kernels with kernel size < width; 2r+1 < w
//! Large (L) kernels corresponds to kernels with radius > width; r > w
//!
//! The fast version for (S) results in 3 loops for cases 1, 2 and 3.
//! The fast version for (M) results in 3 loops for cases 1, 4 and 3.
//! The fast version for (L) results in 1 loop for cases 4.
//!
enum Kernel
{
    kSmall,
    kMid,
    kLarge,
};

typedef struct {
  u8 slope, len, start, end;
} LinePatternInfo;

typedef struct {
  u16 x1, y1, x2, y2;
  u32 len;
} Line;

u8 *pPatternBins = NULL;
u8 pBuffer[QQVGA_SIZE];
COLOR pColImg[QQVGA_PIXELS];
BYTE pGrayImg[QQVGA_PIXELS];
BYTE pEdgeImg[QQVGA_PIXELS];

int gMaxLinSpeed = 130, gMaxAngSpeed = 30;
Phase gPhase = PHASE_SETTINGS;
int gStrongEdgeThreshold = 200;
int gWeakEdgeThreshold = 100;
float gDenoiseSigma = 1.6f;
int gLCDWidth = -1, gLCDHeight = -1;

const LinePatternInfo pPatternInfoLookUps[110] = {
  {1, 4, 1, 4},//1
  {2, 3, 2, 4},//2
  {8, 4, 12, 4},//3
  {8, 3, 1, 3},//4
  {2, 4, 1, 5},//5
  {1, 4, 12, 5},//6
  {2, 1, 5, 4},//7
  {8, 4, 11, 5},//8
  {8, 4, 12, 4},//9
  {2, 4, 12, 6},//10
  {1, 4, 11, 6},//11
  {2, 4, 12, 6},//12
  {8, 4, 10, 6},//13
  {8, 4, 11, 5},//14
  {2, 4, 11, 7},//15
  {1, 4, 10, 7},//16
  {8, 3, 9, 7},//17
  {2, 4, 11, 7},//18
  {2, 3, 10, 8},//19
  {8, 4, 10, 6},//20
  {5, 4, 1, 10},//21
  {4, 3, 12, 10},//22
  {6, 4, 2, 10},//23
  {6, 3, 1, 11},//24
  {4, 4, 1, 9},//25
  {5, 4, 2, 9},//26
  {4, 4, 1, 9},//27
  {6, 4, 3, 9},//28
  {6, 4, 2, 10},//29
  {4, 4, 2, 8},//30
  {5, 4, 3, 8},//31
  {4, 4, 2, 8},//32
  {6, 4, 4, 8},//33
  {6, 4, 3, 9},//34
  {4, 4, 3, 7},//35
  {5, 4, 4, 7},//36
  {6, 3, 5, 7},//37
  {4, 4, 3, 7},//38
  {4, 3, 4, 6},//39
  {6, 4, 4, 8},//40
  {7, 2, 2, 12},//41
  {8, 3, 12, 3},//42
  {6, 3, 2, 11},//43
  {7, 3, 3, 11},//44
  {8, 4, 11, 4},//45
  {6, 4, 10, 3},//46
  {6, 4, 10, 3},//47
  {8, 4, 10, 5},//48
  {7, 4, 10, 4},//49
  {8, 4, 11, 4},//50
  {6, 4, 9, 4},//51
  {7, 3, 9, 5},//52
  {6, 4, 9, 4},//53
  {8, 4, 10, 5},//54
  {7, 2, 8, 6},//55
  {8, 3, 6, 9},//56
  {6, 3, 5, 8},//57
  {3, 2, 3, 5},//58
  {2, 3, 2, 5},//59
  {4, 3, 3, 6},//60
  {3, 3, 2, 6},//61
  {2, 4, 1, 6},//62
  {4, 4, 2, 7},//63
  {3, 4, 1, 7},//64
  {4, 4, 2, 7},//65
  {2, 4, 12, 7},//66
  {2, 4, 1, 6},//67
  {4, 4, 1, 8},//68
  {3, 3, 12, 8},//69
  {4, 4, 1, 8},//70
  {2, 4, 12, 7},//71
  {3, 2, 11, 9},//72
  {2, 3, 11, 8},//73
  {4, 3, 12, 9},//74
  {4, 4, 1, 9},//75
  {4, 4, 2, 8},//76
  {4, 4, 3, 7},//77
  {6, 4, 2, 10},//78
  {6, 4, 3, 9},//79
  {6, 4, 4, 8},//80
  {2, 4, 1, 5},//81
  {2, 4, 12, 6},//82
  {2, 4, 11, 7},//83
  {8, 4, 12, 4},//84
  {8, 4, 11, 5},//85
  {8, 4, 10, 6},//86
  {6, 4, 3, 10},//87
  {6, 4, 4, 9},//88
  {4, 4, 1, 8},//89
  {4, 4, 2, 7},//90
  {2, 4, 1, 6},//91
  {8, 4, 11, 4},//92
  {2, 4, 12, 7},//93
  {8, 4, 11, 4},//94
  {6, 3, 2, 11},//95
  {4, 3, 3, 6},//96
  {4, 3, 12, 9},//97
  {6, 3, 5, 8},//98
  {8, 3, 3, 12},//99
  {2, 3, 2, 5},//100
  {8, 3, 6, 9},//101
  {2, 3, 11, 8},//102
  {8, 2, 1, 2},//103
  {2, 2, 3, 4},//104
  {6, 2, 1, 12},//105
  {4, 2, 11, 10},//106
  {2, 2, 10, 9},//107
  {8, 2, 8, 7},//108
  {4, 2, 4, 5},//109
  {6, 2, 6, 7}//110
};

const u8 pSlopeConnectivityLookup[8][3] = {
  {1, 2, 8},
  {1, 2, 3},
  {2, 3, 4},
  {3, 4, 5},
  {4, 5, 6},
  {5, 6, 7},
  {6, 7, 8},
  {7, 8, 1}
};

// {Current Start, Left End}
const u8 pLeftEdgeConnectivityLookup[10][2] = {
  {1, 4},
  {1, 5},
  {12, 4},
  {12, 5},
  {12, 6},
  {11, 5},
  {11, 6},
  {11, 7},
  {10, 6},
  {10, 7}
};

// {Current Start, Upper End}
const u8 pUpperEdgeConnectivityLookup[10][2] = {
  {1, 10},
  {1, 9},
  {2, 10},
  {2, 9},
  {2, 8},
  {3, 9},
  {3, 8},
  {3, 7},
  {4, 8},
  {4, 7}
};

//! mirror without repetition index
int mirror(const int begin, const int end, const int index)
{
    if(index >= begin && index < end)
        return index;

    const int length = end-begin, last = end-1, slength = length-1;
    const int pindex = index < begin ? last-index+slength : index-begin;
    const int repeat = pindex / slength;
    const int mod = pindex % slength;
    return repeat%2 ? slength-mod+begin : mod+begin;
}

//!
//! \brief This function converts the standard deviation of 
//! Gaussian blur into a box radius for each box blur pass. 
//! Returns the approximate sigma value achieved with the N box blur passes.
//!
//! For further details please refer to :
//! - https://www.peterkovesi.com/papers/FastGaussianSmoothing.pdf
//!
//! \param[out] boxes   box radiis for kernel sizes of 2*boxes[i]+1
//! \param[in] sigma    Gaussian standard deviation
//! \param[in] n        number of box blur pass
//!
float sigma_to_box_radius(int boxes[], const float sigma, const int n)  
{
  // ideal filter width
  float wi = sqrtf((12*sigma*sigma/n)+1); 
  int wl = wi; // no need std::floor  
  if(wl%2==0) wl--;
  int wu = wl+2;
              
  float mi = (12*sigma*sigma - n*wl*wl - 4*n*wl - 3*n)/(-4*wl - 4);
  int m = mi+0.5f; // avoid std::round by adding 0.5f and cast to integer type
              
  for(int i=0; i<n; i++) 
    boxes[i] = ((i < m ? wl : wu) - 1) / 2;

  return sqrtf((m*wl*wl+(n-m)*wu*wu-n)/12.f);
}

//! \param[in] in           source buffer
//! \param[in,out] out      target buffer
//! \param[in] w            image width
//! \param[in] h            image height
//! \param[in] r            box dimension
//!
void horizontal_blur(BYTE in[], BYTE out[], int w, int h, int r)
{
  Kernel kernel = kSmall;

  if( r < w/2 ) kernel = kSmall;
  else if( r < w ) kernel = kMid;
  else kernel = kLarge;

  const float iarr = 1.f/(r+r+1);
  for (int i = 0; i < h; i++)
  {
    const int begin = i*w;
    const int end = begin+w; 
    int acc = 0;

    // current index, left index, right index
    int ti = begin, li = begin-r-1, ri = begin+r;
    switch (kernel)
    {
      case kLarge: // generic but slow
      {
        // initial accumulation
        for(int j=li; j<ri; j++) 
        {
            const int id = mirror(begin, end, j); // mirrored id
            acc += in[id];
        }

        // perform filtering
        for(int j=0; j<w; j++, ri++, ti++, li++) 
        { 
            const int rid = mirror(begin, end, ri); // right mirrored id 
            const int lid = mirror(begin, end, li); // left mirrored id
            acc += in[rid] - in[lid];
            out[ti] = acc*iarr + 0.5f; // fixes darkening with integer types
        }
        break;
      }
      case kMid:
      {
        for(int j=li; j<begin; j++) 
        {
            const int lid = 2 * begin - j; // mirrored id
            acc += in[lid];
        }

        for(int j=begin; j<ri; j++) 
        {
            acc += in[j];
        }

        // 1. left side out and right side in
        for(; ri<end; ri++, ti++, li++)
        { 
            const int lid = 2 * begin - li; // left mirrored id
            acc += in[ri] - in[lid];
            out[ti] = acc*iarr + 0.5f; // fixes darkening with integer types
        }

        // 4. left side out and right side out
        for(; li<begin; ri++, ti++, li++)
        { 
            const int rid = 2 * end - 2 - ri;   // right mirrored id 
            const int lid = 2 * begin - li;     // left mirrored id
            acc += in[rid] - in[lid]; 
            out[ti] = acc*iarr + 0.5f; // fixes darkening with integer types
        }

        // 3. left side in and right side out
        for(; ti<end; ri++, ti++, li++)
        {
            const int rid = 2*end-2-ri; // right mirrored id 
            acc += in[rid] - in[li];
            out[ti] = acc*iarr + 0.5f; // fixes darkening with integer types
        }

        break;
      }
      case kSmall:
      {
        for(int j=li; j<begin; j++) 
        {
            const int lid = 2 * begin - j; // mirrored id
            acc += in[lid];
        }

        for(int j=begin; j<ri; j++) 
        {
            acc += in[j];
        }

        // 1. left side out and right side in
        for(; li<begin; ri++, ti++, li++)
        { 
            const int lid = 2 * begin - li; // left mirrored id
            acc += in[ri] - in[lid];
            out[ti] = acc*iarr + 0.5f; // fixes darkening with integer types
        }

        // 2. left side in and right side in
        for(; ri<end; ri++, ti++, li++) 
        { 
            acc += in[ri] - in[li]; 
            out[ti] = acc*iarr + 0.5f; // fixes darkening with integer types
        }

        // 3. left side in and right side out
        for(; ti<end; ri++, ti++, li++)
        {
            const int rid = 2*end-2-ri; // right mirrored id 
            acc += in[rid] - in[li];
            out[ti] = acc*iarr + 0.5f; // fixes darkening with integer types
        }

        break;
      }
      default:
        break;
    }
  }
}

//!
//! \brief This function performs a 2D tranposition of an image. 
//!
//! The transposition is done per 
//! block to reduce the number of cache misses and improve cache coherency for large image buffers.
//!
//! \param[in] in           source buffer
//! \param[in,out] out      target buffer
//! \param[in] w            image width
//! \param[in] h            image height
//!
void flip_img(BYTE in[], BYTE out[], const int w, const int h)
{
  constexpr int block = 256;
  for(int x= 0; x < w; x+= block)
  for(int y= 0; y < h; y+= block)
  {
    const BYTE* p = in + y*w + x;
    BYTE* q = out + y + x*h;
    
    const int blockx = MIN(w, x+block) - x;
    const int blocky = MIN(h, y+block) - y;
    for(int xx= 0; xx < blockx; xx++)
    {
      for(int yy= 0; yy < blocky; yy++)
      {
        for(int k= 0; k < 1; k++)
          q[k]= p[k];
        p += w;
        q += 1;                    
      }
      p += -blocky*w + 1;
      q += -blocky + h;
    }
  }
}

void gaussian_blur(BYTE in[], BYTE out[], int width, int height, float sigma)
{
  // compute box kernel sizes
  int boxes[3];
  sigma_to_box_radius(boxes, sigma, 3);

  u8 *tmp = pBuffer;

  // perform 3 horizontal blur passes
  horizontal_blur(in, out, width, height, boxes[0]);
  horizontal_blur(out, tmp, width, height, boxes[1]);
  horizontal_blur(tmp, out, width, height, boxes[2]);

  // flip buffer
  flip_img(out, tmp, width, height);

  // perform 3 horizontal blur passes on flipped image
  horizontal_blur(tmp, out, height, width, boxes[0]);
  horizontal_blur(out, tmp, height, width, boxes[1]);
  horizontal_blur(tmp, out, height, width, boxes[2]);

  // flip buffer
  flip_img(out, tmp, height, width);

  memcpy(out, tmp, width*height);
}

void sobel_gradients(BYTE in[], int width, int height, BYTE magnitude_out[], BYTE dir_out[])
{
  // X Gradient Kernel
  const int KX[3][3] = {
    {-1, 0, 1},
    {-2, 0, 2},
    {-1, 0, 1}
  };

  // Y Gradient Kernel
  const int KY[3][3] = {
    {1, 2, 1},
    {0, 0, 0},
    {-1, -2, -1}
  };

  const float angle_0 = M_PI / 8,
              angle_1 = angle_0 + M_PI / 4,
              angle_2 = angle_1 + M_PI / 4,
              angle_3 = angle_2 + M_PI / 4;

  for (int y = 0; y < height - 2; y++)
  for (int x = 0; x < width - 2; x++)
  {
    int hori_sum = 0, vert_sum = 0;

    for (int row = 0; row < 3; row++)
    for (int col = 0; col < 3; col++)
    {
      int i = (y + row)*width + (x + col);
      hori_sum += in[i] * KX[row][col];
      vert_sum += in[i] * KY[row][col];
    }

    int i = y*width + x;

    int mag = abs(hori_sum) + abs(vert_sum);//Cheap version
    magnitude_out[i] = mag > 255 ? 255 : mag;

    float angle = atan2f(vert_sum, hori_sum);

    if (angle != angle) dir_out[i] = 2;
    else
    {
      if (angle < 0) angle *= -1;

      if (angle <= angle_0) dir_out[i] = 0;
      else if (angle <= angle_1) dir_out[i] = 1;
      else if (angle <= angle_2) dir_out[i] = 2;
      else if (angle <= angle_3) dir_out[i] = 3;
      else dir_out[i] = 0;
    }
  }
}

void canny_edge_detector(BYTE gray_in[], BYTE gray_out[], int width, int height)
{
  //1. Denoise image (Gaussian blur)
  gaussian_blur(gray_in, gray_out, width, height, gDenoiseSigma);
  //memcpy(gray_out, gray_in, width*height);

  //2. Find intensity gradients (Sobel)
  BYTE *grad_magns = pBuffer;
  BYTE *grad_dirs = pBuffer + width*height;
  sobel_gradients(gray_out, width, height, grad_magns, grad_dirs);

  //3. Non-maximum Suppression
  memset(gray_out, 0, width*height);
  for (int y = 1; y < height - 1; y++)
  for (int x = 1; x < width - 1; x++)
  {
    int i = y*width + x;

    if (grad_magns[i] >= gWeakEdgeThreshold)
    {
      switch (grad_dirs[i])
      {
        case 0:
          gray_out[i] = grad_magns[i] > grad_magns[y*width + (x-1)] && grad_magns[i] > grad_magns[y*width + (x+1)] ? 0xFF : 0;
          break;
        case 1:
          gray_out[i] = grad_magns[i] > grad_magns[(y+1)*width + (x-1)] && grad_magns[i] > grad_magns[(y-1)*width + (x+1)] ? 0xFF : 0;
          break;
        case 2: 
          gray_out[i] = grad_magns[i] > grad_magns[(y-1)*width + x] && grad_magns[i] > grad_magns[(y+1)*width + x] ? 0xFF : 0;
          break;
        case 3:
          gray_out[i] = grad_magns[i] > grad_magns[(y-1)*width + (x-1)] && grad_magns[i] > grad_magns[(y+1)*width + (x+1)] ? 0xFF : 0;
          break;
        default:
          gray_out[i] = 0;
          break;
      }
    }
  }

  //4. Hysteresis Thresholding
  for (int y = 1; y < height - 1; y++)
  for (int x = 1; x < width - 1; x++)
  {
    int i = y*width + x;

    if (grad_magns[i] >= gWeakEdgeThreshold && grad_magns[i] <= gStrongEdgeThreshold)
    {
      if (grad_magns[(y-1)*width + (x-1)] > gStrongEdgeThreshold ||
          grad_magns[(y-1)*width + x] > gStrongEdgeThreshold ||
          grad_magns[(y-1)*width + (x+1)] > gStrongEdgeThreshold ||
          grad_magns[y*width + (x-1)] > gStrongEdgeThreshold ||
          grad_magns[y*width + (x+1)] > gStrongEdgeThreshold ||
          grad_magns[(y+1)*width + (x-1)] > gStrongEdgeThreshold ||
          grad_magns[(y+1)*width + x] > gStrongEdgeThreshold ||
          grad_magns[(y+1)*width + (x+1)] > gStrongEdgeThreshold)
      {
        grad_magns[i] = 0xFF;
      }
      else
      {
        gray_out[i] = 0;
      }
    }

    if (gray_out[i])
    {
      if (gray_out[(y-1)*width + (x-1)])
      {
        gray_out[y*width + (x-1)] = 0;
        gray_out[(y-1)*width + x] = 0;
      }

      if (gray_out[(y-1)*width + (x+1)])
      {
        gray_out[(y-1)*width + x] = 0;
        gray_out[y*width + (x+1)] = 0;
      }
    }
  }
}

void get_pixel_coords(u8 block_edge_pos, u16 block_start_x, u16 block_start_y, u16 *x, u16 *y)
{
  switch (block_edge_pos)
  {
    case 1:
      *x = block_start_x;
      *y = block_start_y;
      break;
    case 2:
      *x = block_start_x + 1;
      *y = block_start_y;
      break;
    case 3:
      *x = block_start_x + 2;
      *y = block_start_y;
      break;
    case 4:
      *x = block_start_x + 3;
      *y = block_start_y;
      break;
    case 5:
      *x = block_start_x + 3;
      *y = block_start_y + 1;
      break;
    case 6:
      *x = block_start_x + 3;
      *y = block_start_y + 2;
      break;
    case 7:
      *x = block_start_x + 3;
      *y = block_start_y + 3;
      break;
    case 8:
      *x = block_start_x + 2;
      *y = block_start_y + 3;
      break;
    case 9:
      *x = block_start_x + 1;
      *y = block_start_y + 3;
      break;
    case 10:
      *x = block_start_x;
      *y = block_start_y + 3;
      break;
    case 11:
      *x = block_start_x;
      *y = block_start_y + 2;
      break;
    case 12:
      *x = block_start_x;
      *y = block_start_y + 1;
      break;
    default:
      break;
  }
}

bool connect_left_lines(int current_x, int current_y, int target_x, int target_y, u8 current_pattern, u8 target_pattern, Line lines[], int lineCount)
{
  if (!current_pattern || !target_pattern)
    return false;

  bool connected = false;

  LinePatternInfo current_pattern_info = pPatternInfoLookUps[current_pattern - 1];
  LinePatternInfo target_pattern_info = pPatternInfoLookUps[target_pattern - 1];

  bool slope_fits = false;
  for (int i = 0; i < 3; i++)
  {
    if (current_pattern_info.slope == pSlopeConnectivityLookup[target_pattern_info.slope - 1][i])
    {
      slope_fits = true;
      break;
    }
  }

  bool edge_connected = false;
  if (slope_fits)
  {
    for (int i = 0; i < 10; i++)
    {
      if (current_pattern_info.start == pLeftEdgeConnectivityLookup[i][0] && 
          target_pattern_info.end == pLeftEdgeConnectivityLookup[i][1])
      {
        edge_connected = true;
        break;
      }
    }
  }

  if (slope_fits && edge_connected)
  {
    //Find existing line
    for (int i = 0; i < lineCount; i++)
    {
      u16 tail_x = (lines[i].x2 >> 2) << 2;
      u16 tail_y = (lines[i].y2 >> 2) << 2;

      if (tail_x == target_x && tail_y == target_y)
      {
        u16 new_x2, new_y2;
        get_pixel_coords(current_pattern_info.end, current_x, current_y, &new_x2, &new_y2);
        lines[i].x2 = new_x2;
        lines[i].y2 = new_y2;
        lines[i].len += current_pattern_info.len;
        connected = true;
        break;
      }
    }
  }

  return connected;
}

bool connect_upper_lines(int current_x, int current_y, int target_x, int target_y, u8 current_pattern, u8 target_pattern, Line lines[], int lineCount)
{
  if (!current_pattern || !target_pattern)
    return false;

  bool connected = false;

  LinePatternInfo current_pattern_info = pPatternInfoLookUps[current_pattern - 1];
  LinePatternInfo target_pattern_info = pPatternInfoLookUps[target_pattern - 1];

  bool slope_fits = false;
  for (int i = 0; i < 3; i++)
  {
    if (current_pattern_info.slope == pSlopeConnectivityLookup[target_pattern_info.slope - 1][i])
    {
      slope_fits = true;
      break;
    }
  }

  bool edge_connected = false;
  if (slope_fits)
  {
    for (int i = 0; i < 10; i++)
    {
      if (current_pattern_info.start == pUpperEdgeConnectivityLookup[i][0] && 
          target_pattern_info.end == pUpperEdgeConnectivityLookup[i][1])
      {
        edge_connected = true;
        break;
      }
    }
  }

  if (slope_fits && edge_connected)
  {
    //Find existing line
    for (int i = 0; i < lineCount; i++)
    {
      u16 tail_x = (lines[i].x2 >> 2) << 2;
      u16 tail_y = (lines[i].y2 >> 2) << 2;

      if (tail_x == target_x && tail_y == target_y)
      {
        u16 new_x2, new_y2;
        get_pixel_coords(current_pattern_info.end, current_x, current_y, &new_x2, &new_y2);
        lines[i].x2 = new_x2;
        lines[i].y2 = new_y2;
        lines[i].len += current_pattern_info.len;
        connected = true;
        break;
      }
    }
  }

  return connected;
}

int ultrafast_line_detector(int width, int height, int max_line_count, Line lines[])
{
  int lineCount = 0;
  u8 *matchedPatterns = (u8*)pGrayImg;

  for (int y = 0; y < height; y += 4)
  for (int x = 0; x < width; x += 4)
  {
    u16 pattern = 0;

    for (int yy = 0; yy < 4; yy++)
    {
      u16 count = 0;
      for (int xx = 0; xx < 4; xx++)
      {
        count++;
        int i = (y + yy)*width + (x + xx);

        if (pEdgeImg[i])
          pattern |= 1 << ((yy+1)*4 - count);
      }
    }

    int i = y*width + x;
    matchedPatterns[i] = pPatternBins[pattern];

    if (matchedPatterns[i])
    {
      bool existing_line = false;

      if (x > 0 && !existing_line)
      {
        u8 left_pattern = matchedPatterns[y*width + (x - 4)];
        if (left_pattern)
          existing_line = connect_left_lines(x, y, (x - 4), y, matchedPatterns[i], left_pattern, lines, lineCount);
      }

      if (y > 0)
      {
        if (!existing_line)
        {
          u8 top_pattern = matchedPatterns[(y - 4)*width + x];
          if (top_pattern)
            existing_line = connect_upper_lines(x, y, x, (y - 4), matchedPatterns[i], top_pattern, lines, lineCount);
        }

        if (x > 0 && !existing_line)
        {
          u8 top_left_pattern = matchedPatterns[(y - 4)*width + (x - 4)];
          if (top_left_pattern)
            existing_line = connect_upper_lines(x, y, (x - 4), (y - 4), matchedPatterns[i], top_left_pattern, lines, lineCount);
        }

        if (x < width - 4 && !existing_line)
        {
          u8 top_right_pattern = matchedPatterns[(y - 4)*width + (x + 4)];
          if (top_right_pattern)
            existing_line = connect_upper_lines(x, y, (x + 4), (y - 4), matchedPatterns[i], top_right_pattern, lines, lineCount);
        }
      }

      if (!existing_line && lineCount < max_line_count)
      {
        LinePatternInfo pattern_info = pPatternInfoLookUps[matchedPatterns[i] - 1];
        u16 x1, y1, x2, y2;
        get_pixel_coords(pattern_info.start, x, y, &x1, &y1);
        get_pixel_coords(pattern_info.end, x, y, &x2, &y2);
        lines[lineCount] = {x1, y1, x2, y2, pattern_info.len};
        lineCount++;
      }
    }
  }

  return lineCount;
}

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
  bool screen_initd = false, quit = false;

  const int y_row_offset = (CAMHEIGHT >> 1) + 20;

  const int COLOR_X = 5,
            COLOR_Y = 20;

  const int CANNY_X = 5,
            CANNY_Y = COLOR_Y + (CAMHEIGHT - y_row_offset) + 20;

  const int LINES_X = 5,
            LINES_Y = CANNY_Y + (CAMHEIGHT - y_row_offset) + 15;
  
  const int WIDGET_WIDTH = 55,
            WIDGET_HEIGHT = 30,
            MINUS_X1 = 50,
            MINUS_X2 = MINUS_X1 + WIDGET_WIDTH,
            PLUS_X1 = 110,
            PLUS_X2 = PLUS_X1 + WIDGET_WIDTH;
  
  const int STRONG_Y1 = 190,
            STRONG_Y2 = STRONG_Y1 + WIDGET_HEIGHT,
            WEAK_Y1 = 235,
            WEAK_Y2 = WEAK_Y1 + WIDGET_HEIGHT,
            DENOISE_Y1 = 280,
            DENOISE_Y2 = DENOISE_Y1 + WIDGET_HEIGHT;

  const int LB_X1 = 0,
            LB_X2 = LB_X1 + 38,
            LB_Y1 = gLCDHeight - 12,
            LB_Y2 = gLCDHeight;
  
  const int NULL_X1 = (CAMWIDTH >> 1) - 20,
            NULL_X2 = NULL_X1 + 40;

  while (!quit)
  {
    if (!screen_initd)
    {
      LCDClear();
      LCDSetFontSize(1);
      LCDSetColor();
      LCDSetPrintf(COLOR_Y - 10, COLOR_X, "Raw Color Image (Y = %d)", y_row_offset);
      LCDSetPrintf(CANNY_Y - 10, CANNY_X, "Canny Edge Detector");
      LCDSetPrintf(LINES_Y - 10, LINES_X, "\"Ultrafast\" Line Detector");

      //Left Button
      LCDArea(LB_X1, LB_Y1, LB_X2, LB_Y2, WHITE);
      LCDSetColor(BLACK, WHITE);
      LCDSetFontSize(1);
      LCDSetPrintf(LB_Y1 + 2, LB_X1 + 7, "EXIT");

      // Strong Edge Theshold Widget
      LCDArea(MINUS_X1, STRONG_Y1, MINUS_X2, STRONG_Y2, WHITE);
      LCDArea(MINUS_X1 + 20, STRONG_Y1 + 13, MINUS_X1 + 36, STRONG_Y1 + 17, BLACK);
      LCDArea(PLUS_X1, STRONG_Y1, PLUS_X2, STRONG_Y2, WHITE);
      LCDArea(PLUS_X1 + 20, STRONG_Y1 + 13, PLUS_X1 + 36, STRONG_Y1 + 17, BLACK);
      LCDArea(PLUS_X1 + 26, STRONG_Y1 + 7, PLUS_X1 + 30, STRONG_Y1 + 23, BLACK);
      LCDSetFontSize(1);
      LCDSetColor();
      LCDSetPrintf(STRONG_Y1 - 10, 5, "Strong Edge Threshold:");

      // Weak Edge Threshold Widget
      LCDArea(MINUS_X1, WEAK_Y1, MINUS_X2, WEAK_Y2, WHITE);
      LCDArea(MINUS_X1 + 20, WEAK_Y1 + 13, MINUS_X1 + 36, WEAK_Y1 + 17, BLACK);
      LCDArea(PLUS_X1, WEAK_Y1, PLUS_X2, WEAK_Y2, WHITE);
      LCDArea(PLUS_X1 + 20, WEAK_Y1 + 13, PLUS_X1 + 36, WEAK_Y1 + 17, BLACK);
      LCDArea(PLUS_X1 + 26, WEAK_Y1 + 7, PLUS_X1 + 30, WEAK_Y1 + 23, BLACK);
      LCDSetFontSize(1);
      LCDSetColor();
      LCDSetPrintf(WEAK_Y1 - 10, 5, "Weak Edge Threshold:");

      // Denoise Sigma Widget
      LCDArea(MINUS_X1, DENOISE_Y1, MINUS_X2, DENOISE_Y2, WHITE);
      LCDArea(MINUS_X1 + 20, DENOISE_Y1 + 13, MINUS_X1 + 36, DENOISE_Y1 + 17, BLACK);
      LCDArea(PLUS_X1, DENOISE_Y1, PLUS_X2, DENOISE_Y2, WHITE);
      LCDArea(PLUS_X1 + 20, DENOISE_Y1 + 13, PLUS_X1 + 36, DENOISE_Y1 + 17, BLACK);
      LCDArea(PLUS_X1 + 26, DENOISE_Y1 + 7, PLUS_X1 + 30, DENOISE_Y1 + 23, BLACK);
      LCDSetFontSize(1);
      LCDSetColor();
      LCDSetPrintf(DENOISE_Y1 - 10, 5, "Denoise Sigma:");

      screen_initd = true;
    }

    CAMGet((BYTE*)pColImg);
    IPCol2Gray((BYTE*)pColImg, pGrayImg);
    BYTE* graySubImage = pGrayImg + y_row_offset*CAMWIDTH;
    canny_edge_detector(graySubImage, pEdgeImg, CAMWIDTH, CAMHEIGHT - y_row_offset);

    //Detect lines
    Line *lines = (Line*)pBuffer;
    int MAX_LINE_COUNT = sizeof(pBuffer)/sizeof(Line);
    u8 *matchedPatterns = (u8*)pGrayImg;

    int lineCount = ultrafast_line_detector(CAMWIDTH, CAMHEIGHT - y_row_offset, MAX_LINE_COUNT, lines);

    LCDImageStart(COLOR_X, COLOR_Y, CAMWIDTH, CAMHEIGHT - y_row_offset);
    COLOR* colorSubImage = pColImg + y_row_offset*CAMWIDTH;
    LCDImage((BYTE*)colorSubImage);
    LCDImageStart(CANNY_X, CANNY_Y, CAMWIDTH, CAMHEIGHT - y_row_offset);
    //LCDImageBinary(pEdgeImg);
    LCDImageGray(pEdgeImg);
    LCDArea(LINES_X, LINES_Y, LINES_X + CAMWIDTH, LINES_Y + CAMHEIGHT - y_row_offset, BLACK);
    for (int i = 0; i < lineCount; i++)
    {
      if (lines[i].len >= MIN_LINE_LEN && (lines[i].x2 < NULL_X1 || lines[i].x2 > NULL_X2))
        LCDLine(lines[i].x1 + LINES_X, lines[i].y1 + LINES_Y, lines[i].x2 + LINES_X, lines[i].y2 + LINES_Y, RED);
    }

    const int VAL_X = 2, 
              VAL_Y = 7;
    LCDSetColor();
    LCDSetFontSize(2);
    LCDSetPrintf(STRONG_Y1 + VAL_Y, VAL_X, "%d ", gStrongEdgeThreshold);
    LCDSetPrintf(WEAK_Y1 + VAL_Y, VAL_X, "%d ", gWeakEdgeThreshold);
    LCDSetPrintf(DENOISE_Y1 + VAL_Y, VAL_X, "%.1f ", gDenoiseSigma);

    int button = KEYRead();
    if (button & KEY1)
    {
      quit = true;
      gPhase = PHASE_SETTINGS;
      LCDSetFontSize(1);
      LCDSetColor(RED, WHITE);
      LCDSetPrintf(LB_Y1 + 2, LB_X1 + 7, "EXIT");
      delay(INPUT_DELAY_MS);
      continue;
    }

    int t_x, t_y;
    KEYReadXY(&t_x, &t_y);

    if (t_x >= MINUS_X1 && t_x <= MINUS_X2)
    {
      if (t_y >= STRONG_Y1 && t_y <= STRONG_Y2)
      {
        gStrongEdgeThreshold -= 10;
        if (gStrongEdgeThreshold < 0) gStrongEdgeThreshold = 0;
        
        LCDArea(MINUS_X1 + 20, STRONG_Y1 + 13, MINUS_X1 + 36, STRONG_Y1 + 17, RED);
        delay(INPUT_DELAY_MS);
        LCDArea(MINUS_X1 + 20, STRONG_Y1 + 13, MINUS_X1 + 36, STRONG_Y1 + 17, BLACK);
      }
      else if (t_y >= WEAK_Y1 && t_y <= WEAK_Y2)
      {
        gWeakEdgeThreshold -= 10;
        if (gWeakEdgeThreshold < 0) gWeakEdgeThreshold = 0;
        
        LCDArea(MINUS_X1 + 20, WEAK_Y1 + 13, MINUS_X1 + 36, WEAK_Y1 + 17, RED);
        delay(INPUT_DELAY_MS);
        LCDArea(MINUS_X1 + 20, WEAK_Y1 + 13, MINUS_X1 + 36, WEAK_Y1 + 17, BLACK);
      }
      else if (t_y >= DENOISE_Y1 && t_y <= DENOISE_Y2)
      {
        gDenoiseSigma -= 0.1f;
        if (gDenoiseSigma < 0.0f) gDenoiseSigma = 0.0f;
        
        LCDArea(MINUS_X1 + 20, DENOISE_Y1 + 13, MINUS_X1 + 36, DENOISE_Y1 + 17, RED);
        delay(INPUT_DELAY_MS);
        LCDArea(MINUS_X1 + 20, DENOISE_Y1 + 13, MINUS_X1 + 36, DENOISE_Y1 + 17, BLACK);
      }
    }
    else if (t_x >= PLUS_X1 && t_x <= PLUS_X2)
    {
      if (t_y >= STRONG_Y1 && t_y <= STRONG_Y2)
      {
        gStrongEdgeThreshold += 10;
        if (gStrongEdgeThreshold > 255) gStrongEdgeThreshold = 255;

        LCDArea(PLUS_X1 + 20, STRONG_Y1 + 13, PLUS_X1 + 36, STRONG_Y1 + 17, RED);
        LCDArea(PLUS_X1 + 26, STRONG_Y1 + 7, PLUS_X1 + 30, STRONG_Y1 + 23, RED);
        delay(INPUT_DELAY_MS);
        LCDArea(PLUS_X1 + 20, STRONG_Y1 + 13, PLUS_X1 + 36, STRONG_Y1 + 17, BLACK);
        LCDArea(PLUS_X1 + 26, STRONG_Y1 + 7, PLUS_X1 + 30, STRONG_Y1 + 23, BLACK);
      }
      else if (t_y >= WEAK_Y1 && t_y <= WEAK_Y2)
      {
        gWeakEdgeThreshold += 10;
        if (gWeakEdgeThreshold > 255) gWeakEdgeThreshold = 255; 

        LCDArea(PLUS_X1 + 20, WEAK_Y1 + 13, PLUS_X1 + 36, WEAK_Y1 + 17, RED);
        LCDArea(PLUS_X1 + 26, WEAK_Y1 + 7, PLUS_X1 + 30, WEAK_Y1 + 23, RED);
        delay(INPUT_DELAY_MS);
        LCDArea(PLUS_X1 + 20, WEAK_Y1 + 13, PLUS_X1 + 36, WEAK_Y1 + 17, BLACK);
        LCDArea(PLUS_X1 + 26, WEAK_Y1 + 7, PLUS_X1 + 30, WEAK_Y1 + 23, BLACK);
      }
      else if (t_y >= DENOISE_Y1 && t_y <= DENOISE_Y2)
      {
        gDenoiseSigma += 0.1f;

        LCDArea(PLUS_X1 + 20, DENOISE_Y1 + 13, PLUS_X1 + 36, DENOISE_Y1 + 17, RED);
        LCDArea(PLUS_X1 + 26, DENOISE_Y1 + 7, PLUS_X1 + 30, DENOISE_Y1 + 23, RED);
        delay(INPUT_DELAY_MS);
        LCDArea(PLUS_X1 + 20, DENOISE_Y1 + 13, PLUS_X1 + 36, DENOISE_Y1 + 17, BLACK);
        LCDArea(PLUS_X1 + 26, DENOISE_Y1 + 7, PLUS_X1 + 30, DENOISE_Y1 + 23, BLACK);
      }
    }
  }
}

void navigation_screen()
{
  bool quit = false, collision = false;

  const int MIN_DIST = 100;

  const int y_row_offset = (CAMHEIGHT >> 1) + 20;

  const int COLOR_X = 5,
            COLOR_Y = 5;

  const int CANNY_X = 5,
            CANNY_Y = COLOR_Y + (CAMHEIGHT - y_row_offset) + 5;

  const int LINES_X = 5,
            LINES_Y = CANNY_Y + (CAMHEIGHT - y_row_offset) + 5;
  
  const int width = CAMWIDTH,
            height =  CAMHEIGHT - y_row_offset;
  
  const int NULL_X1 = (CAMWIDTH >> 1) - 20,
            NULL_X2 = NULL_X1 + 40;
  
  int screen_width = -1, screen_height = -1;
  LCDGetSize(&screen_width, &screen_height);
  const int RESET_X1 = 5,
            RESET_X2 = 165,
            RESET_Y1 = (screen_height >> 1) + 40,
            RESET_Y2 = screen_height - 5;
  
  const int MSG_X = 5,
            MSG_Y = LINES_Y + height + 18;
  
  // Init
  LCDClear();

  LCDArea(COLOR_X - 1, COLOR_Y - 1, COLOR_X + width + 1, COLOR_Y + height + 1, WHITE, 0);
  LCDArea(CANNY_X - 1, CANNY_Y - 1, CANNY_X + width + 1, CANNY_Y + height + 1, WHITE, 0);
  LCDArea(LINES_X - 1, LINES_Y - 1, LINES_X + width + 1, LINES_Y + height + 1, WHITE, 0);

  LCDArea(RESET_X1, RESET_Y1, RESET_X2, RESET_Y2, RED);
  LCDSetFontSize(3);
  LCDSetColor(WHITE, RED);
  LCDSetPrintf(RESET_Y1 + 30, RESET_X1 + 37, "TOUCH");
  LCDSetPrintf(RESET_Y1 + 60, RESET_X1 + 37, "RESET");

  // Clear the camera's image triple-buffer
  CAMGet((BYTE*)pColImg);
  CAMGet((BYTE*)pColImg);
  CAMGet((BYTE*)pColImg);

  while (!quit)
  {
    CAMGet((BYTE*)pColImg);

    IPCol2Gray((BYTE*)pColImg, pGrayImg);
    BYTE* graySubImage = pGrayImg + y_row_offset*width;

    canny_edge_detector(graySubImage, pEdgeImg, width, height);

    Line *lines = (Line*)pBuffer;
    int lineCount = ultrafast_line_detector(width, height, sizeof(pBuffer)/sizeof(Line), lines);

    int dist = PSDGet(PSD_FRONT);
    if (dist <= MIN_DIST) collision = true;
    else collision = false;

    int tx, ty;
    KEYReadXY(&tx, &ty);
    if (tx >= 0 || ty >= 0)
    {
      quit = true;
      delay(INPUT_DELAY_MS);
      continue;
    }

    int left_lane_idx = -1, right_lane_idx = -1;

    if (!collision)
    {
      for (int i = 0; i < lineCount; i++)
      {
        Line line = lines[i];
        if (line.len >= MIN_LINE_LEN)
        {
          if (line.y1 < line.y2)
          {
            if (line.x1 < NULL_X1 && line.x2 < line.x1)
            {
              if (left_lane_idx >= 0)
              {
                if (lines[left_lane_idx].x1 < line.x1) left_lane_idx = i;
              }
              else
              {
                left_lane_idx = i;
              }
            }
            else if (line.x1 > NULL_X2 && line.x2 > line.x1)
            {
              if (right_lane_idx >= 0)
              {
                if (lines[right_lane_idx].x1 > line.x1) right_lane_idx = i;
              }
              else
              {
                right_lane_idx = i;
              }
            }
          }
          else
          {
            if (line.x2 < NULL_X1 && line.x2 > line.x1)
            {
              if (left_lane_idx >= 0)
              {
                if (lines[left_lane_idx].x2 < line.x2) left_lane_idx = i;
              }
              else
              {
                left_lane_idx = i;
              }
            }
            else if (line.x2 > NULL_X2 && line.x2 < line.x1)
            {
              if (right_lane_idx >= 0)
              {
                if (lines[right_lane_idx].x2 > line.x2) right_lane_idx = i;
              }
              else
              {
                right_lane_idx = i;
              }
            }
          }
        }
      }

      LCDArea(MSG_X, MSG_Y, screen_width, MSG_Y + 30, BLACK);

      if (left_lane_idx < 0 && right_lane_idx < 0)
      {
        VWSetSpeed(gMaxLinSpeed, 0);
      }
      else
      { 
        bool turning_right = false, turning_left = false;

        if (left_lane_idx >= 0)
        {
          float intercept = 0.0f, y_border_dist = 0.0f;
          Line left_lane = lines[left_lane_idx];

          if (left_lane.y1 < left_lane.y2)
          {
            float rise = left_lane.y2 - left_lane.y1;
            float run = left_lane.x1 - left_lane.x2;
            float grad = rise / run;
            float x_border_dist = NULL_X1 - left_lane.x1;
            y_border_dist = left_lane.y1;
            intercept = x_border_dist*grad;
          }
          else
          {
            float rise = left_lane.y1 - left_lane.y2;
            float run = left_lane.x2 - left_lane.x1;
            float grad = rise / run;
            float x_border_dist = NULL_X1 - left_lane.x2;
            y_border_dist = left_lane.y2;
            intercept = x_border_dist*grad;
          }

          if (intercept < y_border_dist) 
          {
            turning_right = true;
          }
        }

        if (right_lane_idx >= 0)
        {
          float intercept = 0.0f, y_border_dist = 0.0f;
          Line right_lane = lines[right_lane_idx];

          if (right_lane.y1 < right_lane.y2)
          {
            float rise = right_lane.y2 - right_lane.y1;
            float run = right_lane.x2 - right_lane.x1;
            float grad = rise / run;
            float x_border_dist = right_lane.x1 - NULL_X2;
            y_border_dist = right_lane.y1;
            intercept = x_border_dist*grad;
          }
          else
          {
            float rise = right_lane.y1 - right_lane.y2;
            float run = right_lane.x1 - right_lane.x2;
            float grad = rise / run;
            float x_border_dist = right_lane.x2 - NULL_X2;
            y_border_dist = right_lane.y2;
            intercept = x_border_dist*grad;
          }

          if (intercept < y_border_dist)
          {
            turning_left = true;
          } 
        }

        if ((!turning_left && !turning_right) || (turning_left && turning_right))
        {
          VWSetSpeed(gMaxLinSpeed, 0);
          LCDArea(125, LINES_Y + height + 5, 165, LINES_Y + height + 50, GREEN);
          LCDArea(5, LINES_Y + height + 5, 45, LINES_Y + height + 50, GREEN);
        }
        else if (turning_left)
        {
          VWSetSpeed(gMaxLinSpeed, -1*gMaxAngSpeed);
          LCDArea(125, LINES_Y + height + 5, 165, LINES_Y + height + 50, RED);
          LCDArea(5, LINES_Y + height + 5, 45, LINES_Y + height + 50, GREEN);
        }
        else
        {
          VWSetSpeed(gMaxLinSpeed, gMaxAngSpeed);
          LCDArea(125, LINES_Y + height + 5, 165, LINES_Y + height + 50, GREEN);
          LCDArea(5, LINES_Y + height + 5, 45, LINES_Y + height + 50, RED);
        }
      }
    }
    else
    {
      VWSetSpeed(0, 0);
      LCDArea(125, LINES_Y + height + 5, 165, LINES_Y + height + 50, RED);
      LCDArea(5, LINES_Y + height + 5, 45, LINES_Y + height + 50, RED);
      LCDSetFontSize(3);
      LCDSetColor(BLACK, RED);
      LCDSetPrintf(MSG_Y, MSG_X, "COLLISION");
    }

    // Display
    LCDImageStart(COLOR_X, COLOR_Y, width, height);
    COLOR* colorSubImage = pColImg + y_row_offset*width;
    LCDImage((BYTE*)colorSubImage);

    LCDImageStart(CANNY_X, CANNY_Y, width, height);
    LCDImageGray(pEdgeImg);

    LCDArea(LINES_X, LINES_Y, LINES_X + width, LINES_Y + height, BLACK);
    for (int i = 0; i < lineCount; i++)
    {
      if (i == left_lane_idx || i == right_lane_idx)
      {
        LCDLine(lines[i].x1 + LINES_X, lines[i].y1 + LINES_Y, lines[i].x2 + LINES_X, lines[i].y2 + LINES_Y, GREEN);
      }
      else if (lines[i].len >= MIN_LINE_LEN && (lines[i].x2 < NULL_X1 || lines[i].x2 > NULL_X2))
      {
        LCDLine(lines[i].x1 + LINES_X, lines[i].y1 + LINES_Y, lines[i].x2 + LINES_X, lines[i].y2 + LINES_Y, RED);
        //LCDPixel(lines[i].x1 + LINES_X, lines[i].y1 + LINES_Y, YELLOW);
        //LCDPixel(lines[i].x2 + LINES_X, lines[i].y2 + LINES_Y, BLUE);
      }
    }
  }

  gPhase = PHASE_SETTINGS;
  VWSetSpeed(0, 0);
}

void setup() 
{
  pPatternBins = (u8*)calloc(1 << 16, sizeof(u8));
  assert(pPatternBins);

  pPatternBins[15] = 1;
  pPatternBins[7] = 2;
  pPatternBins[135] = 3;
  pPatternBins[14] = 4;
  pPatternBins[30] = 5;
  pPatternBins[240] = 6;
  pPatternBins[120] = 7;
  pPatternBins[2160] = 8;
  pPatternBins[225] = 9;
  pPatternBins[480] = 10;
  pPatternBins[3840] = 11;
  pPatternBins[1920] = 12;
  pPatternBins[34560] = 13;
  pPatternBins[3600] = 14;
  pPatternBins[7680] = 15;
  pPatternBins[61440] = 16;
  pPatternBins[28672] = 17;
  pPatternBins[30720] = 18;
  pPatternBins[57344] = 19;
  pPatternBins[57600] = 20;
  pPatternBins[34952] = 21;
  pPatternBins[34944] = 22;
  pPatternBins[34948] = 23;
  pPatternBins[2184] = 24;
  pPatternBins[18568] = 25;
  pPatternBins[17476] = 26;
  pPatternBins[17480] = 27;
  pPatternBins[17474] = 28;
  pPatternBins[33860] = 29;
  pPatternBins[9284] = 30;
  pPatternBins[8738] = 31;
  pPatternBins[8740] = 32;
  pPatternBins[8737] = 33;
  pPatternBins[16930] = 34;
  pPatternBins[4642] = 35;
  pPatternBins[4369] = 36;
  pPatternBins[4368] = 37;
  pPatternBins[4370] = 38;
  pPatternBins[273] = 39;
  pPatternBins[8465] = 40;
  pPatternBins[132] = 41;
  pPatternBins[134] = 42;
  pPatternBins[2180] = 43;
  pPatternBins[2114] = 44;
  pPatternBins[2115] = 45;
  pPatternBins[34882] = 46;
  pPatternBins[33826] = 47;
  pPatternBins[33840] = 48;
  pPatternBins[33825] = 49;
  pPatternBins[3105] = 50;
  pPatternBins[17441] = 51;
  pPatternBins[16912] = 52;
  pPatternBins[16913] = 53;
  pPatternBins[49680] = 54;
  pPatternBins[8448] = 55;
  pPatternBins[24832] = 56;
  pPatternBins[8464] = 57;
  pPatternBins[18] = 58;
  pPatternBins[22] = 59;
  pPatternBins[274] = 60;
  pPatternBins[292] = 61;
  pPatternBins[300] = 62;
  pPatternBins[4388] = 63;
  pPatternBins[4680] = 64;
  pPatternBins[4676] = 65;
  pPatternBins[4800] = 66;
  pPatternBins[840] = 67;
  pPatternBins[8776] = 68;
  pPatternBins[9344] = 69;
  pPatternBins[9352] = 70;
  pPatternBins[13440] = 71;
  pPatternBins[18432] = 72;
  pPatternBins[26624] = 73;
  pPatternBins[18560] = 74;
  pPatternBins[17544] = 75;
  pPatternBins[8772] = 76;
  pPatternBins[4386] = 77;
  pPatternBins[34884] = 78;
  pPatternBins[17442] = 79;
  pPatternBins[8721] = 80;
  pPatternBins[60] = 81;
  pPatternBins[960] = 82;
  pPatternBins[15360] = 83;
  pPatternBins[195] = 84;
  pPatternBins[3120] = 85;
  pPatternBins[49920] = 86;
  pPatternBins[33858] = 87;
  pPatternBins[16929] = 88;
  pPatternBins[9288] = 89;
  pPatternBins[4644] = 90;
  pPatternBins[360] = 91;
  pPatternBins[2145] = 92;
  pPatternBins[5760] = 93;
  pPatternBins[34320] = 94;
  pPatternBins[2116] = 95;
  pPatternBins[290] = 96;
  pPatternBins[17536] = 97;
  pPatternBins[8720] = 98;
  pPatternBins[194] = 99;
  pPatternBins[52] = 100;
  pPatternBins[17152] = 101;
  pPatternBins[11264] = 102;
  pPatternBins[12] = 103;
  pPatternBins[3] = 104;
  pPatternBins[136] = 105;
  pPatternBins[34816] = 106;
  pPatternBins[49152] = 107;
  pPatternBins[12288] = 108;
  pPatternBins[17] = 109;
  pPatternBins[4352] = 110;

  int err = EYEBOTInit();
  assert(!err);

  LCDGetSize(&gLCDWidth, &gLCDHeight);
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
