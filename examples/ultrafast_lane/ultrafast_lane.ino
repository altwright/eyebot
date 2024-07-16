#include <eyebot.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

u8 *pPatternBins = NULL;

typedef struct {
  u8 slope;
  u8 len;
  u8 start;
  u8 end;
} LinePatternInfo;

LinePatternInfo pPatternLookUps[110] = {
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

void setup() {
  
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
}

void loop() {
  // put your main code here, to run repeatedly:

}
