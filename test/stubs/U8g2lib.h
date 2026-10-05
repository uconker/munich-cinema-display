#pragma once
#include <stdint.h>
#define U8G2_R0 0
#define u8g2_font_5x8_tf 1
#define u8g2_font_6x12_tf 2
#define u8g2_font_10x20_tf 3
struct U8G2_SSD1306_128X64_NONAME_F_HW_I2C { U8G2_SSD1306_128X64_NONAME_F_HW_I2C(int,int,int,int){} void begin(){} void enableUTF8Print(){} void clearBuffer(){} void sendBuffer(){}
 void setFont(int){} void drawUTF8(int,int,const char*){} void drawStr(int,int,const char*){} void drawHLine(int,int,int){} void drawBox(int,int,int,int){} void drawPixel(int,int){} void setPowerSave(int){} void setContrast(int){} };
