#ifndef __OLED_H
#define __OLED_H

#include "main.h"
#include "i2c.h"

/*
 * 0.96 英寸 OLED (SSD1306) 驱动, I2C 接口
 *
 * 屏幕接线 (I2C1, 见 i2c.c 的 MspInit):
 *   PB6 -> OLED SCL
 *   PB7 -> OLED SDA
 *   VCC -> 3.3V, GND -> GND
 *
 * SSD1306 器件地址通常是 0x3C 或 0x3D, 写地址 = 器件地址 << 1:
 *   0x3C -> 0x78 (大多数 0.96 寸屏)
 *   0x3D -> 0x7A (部分屏)
 * 若上电不显示, 把下面这个宏换成 0x7A 再试。
 */
#define OLED_I2C_HANDLE  hi2c1
#define OLED_ADDR        0x78

/* 显示缓冲区: 128 列 x 8 页, 每页 8 行, 共 64 行 */
#define OLED_COLS   128
#define OLED_PAGES  8

void OLED_Init(void);                                   /* 初始化 SSD1306 */
void OLED_Clear(void);                                  /* 清空显存 */
void OLED_Refresh(void);                                /* 整屏硬件刷屏(全页刷新) */

void OLED_ShowChar_ASCII(uint8_t x, uint8_t y, char ch);      /* 显示一个 8x16 字符 */
void OLED_ShowChinese(uint8_t x, uint8_t y, const uint8_t *font); /* 显示一个 16x16 汉字 */
void OLED_ShowStr(uint8_t x, uint8_t y, const char *s);        /* 中英文混合字符串(GBK) */

uint16_t OLED_StrWidth(const char *s);                  /* 计算字符串显示宽度(像素) */
void OLED_Marquee(const char *s, uint8_t y_page, uint8_t speed_ms, uint16_t rounds); /* 左移跑马灯 */

void OLED_HardwareScroll_Left(uint8_t page_start, uint8_t page_end); /* SSD1306 硬件左滚 */
void OLED_StopScroll(void);                             /* 停止硬件滚动 */

#endif
