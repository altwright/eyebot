#include <eyebot.h>
#include <math.h>

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

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
  u32 is_connected;
} Line;

u8 *pPatternBins = NULL;
u8 pBuffer[QQVGA_SIZE];

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

void gaussian_blur(BYTE in[], BYTE out[], float sigma)
{
  // compute box kernel sizes
  int boxes[3];
  sigma_to_box_radius(boxes, sigma, 3);

  u8 *tmp = pBuffer;

  // perform 3 horizontal blur passes
  horizontal_blur(in, out, CAMWIDTH, CAMHEIGHT, boxes[0]);
  horizontal_blur(out, tmp, CAMWIDTH, CAMHEIGHT, boxes[1]);
  horizontal_blur(tmp, out, CAMWIDTH, CAMHEIGHT, boxes[2]);

  // flip buffer
  flip_img(out, tmp, CAMWIDTH, CAMHEIGHT);

  // perform 3 horizontal blur passes on flipped image
  horizontal_blur(tmp, out, CAMHEIGHT, CAMWIDTH, boxes[0]);
  horizontal_blur(out, tmp, CAMHEIGHT, CAMWIDTH, boxes[1]);
  horizontal_blur(tmp, out, CAMHEIGHT, CAMWIDTH, boxes[2]);

  // flip buffer
  flip_img(out, tmp, CAMHEIGHT, CAMWIDTH);

  memcpy(out, tmp, CAMWIDTH*CAMHEIGHT*sizeof(BYTE));
}

void sobel_gradients(BYTE in[], BYTE magnitude_out[], BYTE dir_out[])
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

  for (int y = 0; y < CAMHEIGHT - 2; y++)
  for (int x = 0; x < CAMWIDTH - 2; x++)
  {
    int hori_sum = 0, vert_sum = 0;

    for (int row = 0; row < 3; row++)
    for (int col = 0; col < 3; col++)
    {
      int i = (y + row)*CAMWIDTH + (x + col);
      hori_sum += in[i] * KX[row][col];
      vert_sum += in[i] * KY[row][col];
    }

    int i = y*CAMWIDTH + x;

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

void canny_edge_detector(BYTE gray_in[], BYTE gray_out[])
{
  const int MAX_VAL = 200;
  const int MIN_VAL = 100;

  //1. Denoise image (Gaussian blur)
  gaussian_blur(gray_in, gray_out, 1.4f);

  //2. Find intensity gradients (Sobel)
  BYTE *grad_magns = pBuffer;
  BYTE *grad_dirs = pBuffer + QQVGA_PIXELS;
  sobel_gradients(gray_out, grad_magns, grad_dirs);

  //3. Non-maximum Suppression
  memset(gray_out, 0, QQVGA_PIXELS);
  for (int y = 1; y < CAMHEIGHT - 1; y++)
  for (int x = 1; x < CAMWIDTH - 1; x++)
  {
    int i = y*CAMWIDTH + x;

    if (grad_magns[i] >= MIN_VAL)
    {
      switch (grad_dirs[i])
      {
        case 0:
          gray_out[i] = grad_magns[i] > grad_magns[y*CAMWIDTH + (x-1)] && grad_magns[i] > grad_magns[y*CAMWIDTH + (x+1)] ? 0xFF : 0;
          break;
        case 1:
          gray_out[i] = grad_magns[i] > grad_magns[(y+1)*CAMWIDTH + (x-1)] && grad_magns[i] > grad_magns[(y-1)*CAMWIDTH + (x+1)] ? 0xFF : 0;
          break;
        case 2: 
          gray_out[i] = grad_magns[i] > grad_magns[(y-1)*CAMWIDTH + x] && grad_magns[i] > grad_magns[(y+1)*CAMWIDTH + x] ? 0xFF : 0;
          break;
        case 3:
          gray_out[i] = grad_magns[i] > grad_magns[(y-1)*CAMWIDTH + (x-1)] && grad_magns[i] > grad_magns[(y+1)*CAMWIDTH + (x+1)] ? 0xFF : 0;
          break;
        default:
          gray_out[i] = 0;
          break;
      }
    }
  }

  //4. Hysteresis Thresholding
  for (int y = 1; y < CAMHEIGHT - 1; y++)
  for (int x = 1; x < CAMWIDTH - 1; x++)
  {
    int i = y*CAMWIDTH + x;

    if (grad_magns[i] >= MIN_VAL && grad_magns[i] <= MAX_VAL)
    {
      if (grad_magns[(y-1)*CAMWIDTH + (x-1)] > MAX_VAL ||
          grad_magns[(y-1)*CAMWIDTH + x] > MAX_VAL ||
          grad_magns[(y-1)*CAMWIDTH + (x+1)] > MAX_VAL ||
          grad_magns[y*CAMWIDTH + (x-1)] > MAX_VAL ||
          grad_magns[y*CAMWIDTH + (x+1)] > MAX_VAL ||
          grad_magns[(y+1)*CAMWIDTH + (x-1)] > MAX_VAL ||
          grad_magns[(y+1)*CAMWIDTH + x] > MAX_VAL ||
          grad_magns[(y+1)*CAMWIDTH + (x+1)] > MAX_VAL)
      {
        grad_magns[i] = 0xFF;
      }
      else
      {
        gray_out[i] = 0;
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
        lines[i].is_connected = 1;
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
        lines[i].is_connected = 1;
        connected = true;
        break;
      }
    }
  }

  return connected;
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
}

void loop() 
{
  static COLOR col_img[QQVGA_PIXELS];
  static BYTE gray_img[QQVGA_PIXELS];
  static BYTE edge_img[QQVGA_PIXELS];

  CAMGet((BYTE*)col_img);
  IPCol2Gray((BYTE*)col_img, gray_img);

  canny_edge_detector(gray_img, edge_img);

  //Detect lines
  Line *lines = (Line*)pBuffer;
  int lineCount = 0;
  int MAX_LINE_COUNT = sizeof(pBuffer)/sizeof(Line);
  u8 *matchedPatterns = (u8*)gray_img;
  memset(matchedPatterns, 0, QQVGA_PIXELS);

  for (int y = 0; y < CAMHEIGHT; y += 4)
  for (int x = 0; x < CAMWIDTH; x += 4)
  {
    u16 pattern = 0;

    for (int yy = 0; yy < 4; yy++)
    {
      u16 count = 0;
      for (int xx = 0; xx < 4; xx++)
      {
        count++;
        int i = (y + yy)*CAMWIDTH + (x + xx);

        if (edge_img[i])
          pattern |= 1 << ((yy+1)*4 - count);
      }
    }

    int i = y*CAMWIDTH + x;
    matchedPatterns[i] = pPatternBins[pattern];

    if (matchedPatterns[i])
    {
      bool existing_line = false;

      if (x > 0 && !existing_line)
      {
        u8 left_pattern = matchedPatterns[y*CAMWIDTH + (x - 4)];
        if (left_pattern)
          existing_line = connect_left_lines(x, y, (x - 4), y, matchedPatterns[i], left_pattern, lines, lineCount);
      }

      if (y > 0)
      {
        if (!existing_line)
        {
          u8 top_pattern = matchedPatterns[(y - 4)*CAMWIDTH + x];
          if (top_pattern)
            existing_line = connect_upper_lines(x, y, x, (y - 4), matchedPatterns[i], top_pattern, lines, lineCount);
        }

        if (x > 0 && !existing_line)
        {
          u8 top_left_pattern = matchedPatterns[(y - 4)*CAMWIDTH + (x - 4)];
          if (top_left_pattern)
            existing_line = connect_upper_lines(x, y, (x - 4), (y - 4), matchedPatterns[i], top_left_pattern, lines, lineCount);
        }

        if (x < CAMWIDTH - 4 && !existing_line)
        {
          u8 top_right_pattern = matchedPatterns[(y - 4)*CAMWIDTH + (x + 4)];
          if (top_right_pattern)
            existing_line = connect_upper_lines(x, y, (x + 4), (y - 4), matchedPatterns[i], top_right_pattern, lines, lineCount);
        }
      }

      if (!existing_line && lineCount < MAX_LINE_COUNT)
      {
        LinePatternInfo pattern_info = pPatternInfoLookUps[matchedPatterns[i] - 1];
        u16 x1, y1, x2, y2;
        get_pixel_coords(pattern_info.start, x, y, &x1, &y1);
        get_pixel_coords(pattern_info.end, x, y, &x2, &y2);
        lines[lineCount] = {x1, y1, x2, y2, 0};
        lineCount++;
      }
    }
  }

  LCDImageStart(5, 10 + CAMHEIGHT, CAMWIDTH, CAMHEIGHT);
  //LCDImage((BYTE*)col_img);
  LCDImageBinary(edge_img);
  LCDArea(5, 5, 5 + CAMWIDTH, 5 + CAMHEIGHT, BLACK);
  for (int i = 0; i < lineCount; i++)
  {
    if (lines[i].is_connected)
      LCDLine(lines[i].x1 + 5, lines[i].y1 + 5, lines[i].x2 + 5, lines[i].y2 + 5, RED);
  }
}