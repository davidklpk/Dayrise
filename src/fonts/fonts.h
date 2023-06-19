#ifndef __FONTS_H
#define __FONTS_H

// Muss so groß sein wie der größte character von allen Fonts
#define MAX_HEIGHT_FONT         67//41
#define MAX_WIDTH_FONT          58//38//32
#define OFFSET_BITMAP           335//54
// #define MAX_HEIGHT_FONT         76
// #define MAX_WIDTH_FONT          39
// #define OFFSET_BITMAP           54

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdint.h>
// #include <avr/pgmspace.h>
//ASCII
typedef struct _tFont
{    
  const uint8_t *table;
  uint16_t Width;
  uint16_t Height;
  
} sFONT;

//GB2312
typedef struct                                          // 汉字字模数据结构
{
  unsigned char index[3];                               // 汉字内码索引
  const char matrix[MAX_HEIGHT_FONT*MAX_WIDTH_FONT/8];  // 点阵码数据
}CH_CN;

typedef struct
{    
  const CH_CN *table;
  uint16_t size;
  uint16_t ASCII_Width;
  uint16_t Width;
  uint16_t Height;
  
}cFONT;

extern sFONT FontRoboto48;
extern sFONT FontRoboto72;
extern sFONT FontRoboto13;

extern sFONT Font24;
extern sFONT Font20;
extern sFONT Font16;
extern sFONT Font12;
extern sFONT Font8;

// extern const unsigned char Font16_Table[];

#ifdef __cplusplus
}
#endif
  
#endif /* __FONTS_H */
 
