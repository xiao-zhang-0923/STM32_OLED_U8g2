/**
  ******************************************************************************
  * @file    oled.c
  * @brief   SSD1306 0.96寸 OLED 驱动 (I2C, 显存 + 整屏硬件刷屏)
  *
  * 原理说明:
  *   SSD1306 内部有一块 128x64 bit 的 GDDRAM, 按 "页(page)" 组织, 共 8 页,
  *   每页 8 行, 每列一个字节, 字节的 bit0 对应该页最上面一行(COM), 即
  *   "LSB 在顶部"。因此点阵字库采用 "列行式" 取模: 对 16x16 汉字,
  *   前 16 字节是各列的上半 8 行, 后 16 字节是各列的下半 8 行。
  *
  *   本驱动在 MCU 内维护一块显存 oled_buf[128][8], 所有画图操作只改显存,
  *   最后调用 OLED_Refresh() 一次把 8 页全部通过 I2C 写到屏上 —— 这就是
  *   "硬件刷屏模式"(整屏刷新), 比逐点写更快、闪烁更小。
  ******************************************************************************
  */
#include "oled.h"
#include "oledfont.h"
#include "string.h"

/* 显存缓冲区: [列][页] */
static uint8_t oled_buf[OLED_COLS][OLED_PAGES];

/* 跑马灯虚拟缓冲: 一行文字高 16 行(2 页), 宽度足够放长句 + 一屏空白 */
#define SCROLL_MAX_W  512
static uint8_t scroll_buf[SCROLL_MAX_W][2];

/* ------------------------------------------------------------------ */
/* 底层 I2C 写命令 / 写数据                                            */
/* ------------------------------------------------------------------ */
static void OLED_WriteCmd(uint8_t cmd)
{
    /* SSD1306: 控制字节 0x00 表示接下来是命令 */
    HAL_I2C_Mem_Write(&OLED_I2C_HANDLE, OLED_ADDR, 0x00,
                      I2C_MEMADD_SIZE_8BIT, &cmd, 1, 100);
}

static void OLED_WriteData(uint8_t dat)
{
    /* SSD1306: 控制字节 0x40 表示接下来是数据 */
    HAL_I2C_Mem_Write(&OLED_I2C_HANDLE, OLED_ADDR, 0x40,
                      I2C_MEMADD_SIZE_8BIT, &dat, 1, 100);
}

/* ------------------------------------------------------------------ */
/* 初始化 / 清屏 / 刷屏                                                */
/* ------------------------------------------------------------------ */
void OLED_Init(void)
{
    HAL_Delay(100);             /* 上电等待 */

    OLED_WriteCmd(0xAE);        /* 关闭显示 */
    OLED_WriteCmd(0xD5);        /* 设置时钟分频因子/振荡频率 */
    OLED_WriteCmd(0x80);
    OLED_WriteCmd(0xA8);        /* 设置驱动路数 1/64 */
    OLED_WriteCmd(0x3F);
    OLED_WriteCmd(0xD3);        /* 设置显示偏移 */
    OLED_WriteCmd(0x00);
    OLED_WriteCmd(0x40);        /* 设置显示起始行 */
    OLED_WriteCmd(0x8D);        /* 电荷泵设置 */
    OLED_WriteCmd(0x14);        /* 使能电荷泵 */
    OLED_WriteCmd(0x20);        /* 设置内存地址模式 */
    OLED_WriteCmd(0x02);        /* 页寻址模式 */
    OLED_WriteCmd(0xA1);        /* 段重映射, 列地址 127 -> SEG0 */
    OLED_WriteCmd(0xC8);        /* COM 扫描方向, 从上到下 */
    OLED_WriteCmd(0xDA);        /* 设置 COM 硬件引脚配置 */
    OLED_WriteCmd(0x12);
    OLED_WriteCmd(0x81);        /* 对比度 */
    OLED_WriteCmd(0xCF);
    OLED_WriteCmd(0xD9);        /* 预充电周期 */
    OLED_WriteCmd(0xF1);
    OLED_WriteCmd(0xDB);        /* VCOMH 电压 */
    OLED_WriteCmd(0x30);
    OLED_WriteCmd(0xA4);        /* 输出跟随显存内容 */
    OLED_WriteCmd(0xA6);        /* 正常显示(非反色) */
    OLED_WriteCmd(0xAF);        /* 开启显示 */

    OLED_Clear();
    OLED_Refresh();
}

void OLED_Clear(void)
{
    memset(oled_buf, 0x00, sizeof(oled_buf));
}


void OLED_Refresh(void)
{
    uint8_t page, i;
    for (page = 0; page < OLED_PAGES; page++)
    {
        OLED_WriteCmd(0xB0 + page);  
        OLED_WriteCmd(0x00);          
        OLED_WriteCmd(0x10);         
        for (i = 0; i < OLED_COLS; i++)
        {
            OLED_WriteData(oled_buf[i][page]);
        }
    }
}


void OLED_ShowChar_ASCII(uint8_t x, uint8_t y, char ch)
{
    const uint8_t *g;
    uint8_t i;

    if (ch < ' ' || ch > '~')  
        ch = ' ';
    g = ASCII_8x16[ch - ' '];

    for (i = 0; i < 8; i++)
    {
        oled_buf[x + i][y]     = g[i];    
        oled_buf[x + i][y + 1] = g[i + 8];  
    }
}


void OLED_ShowChinese(uint8_t x, uint8_t y, const uint8_t *font)
{
    uint8_t i;
    for (i = 0; i < 16; i++)
    {
        oled_buf[x + i][y]     = font[i];       
        oled_buf[x + i][y + 1] = font[i + 16]; 
    }
}

static int16_t OLED_FindCN(const uint8_t *p)
{
    uint16_t i = 0;
    int16_t idx = 0;
    while (CN_Index[i] != 0)
    {
        if (CN_Index[i] == p[0] && CN_Index[i + 1] == p[1])
            return idx;
        idx++;
        i += 2;   
    }
    return -1;
}

void OLED_ShowStr(uint8_t x, uint8_t y, const char *s)
{
    while (*s)
    {
        uint8_t c = (uint8_t)*s;
        if (c < 0x80)                
        {
            OLED_ShowChar_ASCII(x, y, (char)c);
            x += 8;
            s += 1;
        }
        else                       
        {
            int16_t idx = OLED_FindCN((const uint8_t *)s);
            if (idx >= 0)
                OLED_ShowChinese(x, y, CN_16x16[idx]);
            x += 16;
            s += 2;
        }
    }
}


uint16_t OLED_StrWidth(const char *s)
{
    uint16_t w = 0;
    while (*s)
    {
        uint8_t c = (uint8_t)*s;
        if (c < 0x80) { w += 8;  s += 1; }
        else          { w += 16; s += 2; }
    }
    return w;
}


static uint16_t OLED_RenderLine(const char *s)
{
    uint16_t x = 0;
    while (*s && (x < SCROLL_MAX_W - 16))
    {
        uint8_t c = (uint8_t)*s;
        if (c < 0x80)
        {
            const uint8_t *g;
            uint8_t i;
            if (c < ' ' || c > '~') c = ' ';
            g = ASCII_8x16[c - ' '];
            for (i = 0; i < 8; i++)
            {
                scroll_buf[x + i][0] = g[i];
                scroll_buf[x + i][1] = g[i + 8];
            }
            x += 8;
            s += 1;
        }
        else
        {
            int16_t idx = OLED_FindCN((const uint8_t *)s);
            if (idx >= 0)
            {
                const uint8_t *g = CN_16x16[idx];
                uint8_t i;
                for (i = 0; i < 16; i++)
                {
                    scroll_buf[x + i][0] = g[i];
                    scroll_buf[x + i][1] = g[i + 16];
                }
            }
            x += 16;
            s += 2;
        }
    }
    return x;
}

/*
 * 左移跑马灯: 文字从右往左滑过屏幕, 显示在第 y_page 页开始的 16 行高度上。
 * speed_ms: 每移动 1 像素的延时(越小越快); rounds: 完整滚动的遍数。
 */
void OLED_Marquee(const char *s, uint8_t y_page, uint8_t speed_ms, uint16_t rounds)
{
    uint16_t w, total, offset, c;
    uint32_t frame, max_frame;

    memset(scroll_buf, 0, sizeof(scroll_buf));
    w = OLED_RenderLine(s);            /* 渲染整行 */
    total = w + OLED_COLS;             /* 末尾再留一屏空白, 让文字完全滚出 */
    offset = 0;
    max_frame = (uint32_t)rounds * total;

    for (frame = 0; frame < max_frame; frame++)
    {
        /* 从虚拟缓冲中取一个 128 宽的滑动窗口, 拷贝到显存 */
        for (c = 0; c < OLED_COLS; c++)
        {
            oled_buf[c][y_page]     = scroll_buf[(offset + c) % total][0];
            oled_buf[c][y_page + 1] = scroll_buf[(offset + c) % total][1];
        }
        OLED_Refresh();                /* 整屏硬件刷屏 */
        HAL_Delay(speed_ms);
        offset = (offset + 1) % total;
    }
}

/* ------------------------------------------------------------------ */
/* SSD1306 硬件滚动(可选)                                               */
/* ------------------------------------------------------------------ */
/* 停止滚动 */
void OLED_StopScroll(void)
{
    OLED_WriteCmd(0x2E);
}

/* 硬件向左滚动: 从 page_start 页到 page_end 页一起向左滚动 */
void OLED_HardwareScroll_Left(uint8_t page_start, uint8_t page_end)
{
    OLED_WriteCmd(0x27);        /* 向左滚动 */
    OLED_WriteCmd(0x00);        /* 空字节 */
    OLED_WriteCmd(page_start);  /* 起始页 */
    OLED_WriteCmd(0x00);        /* 滚动间隔 */
    OLED_WriteCmd(page_end);    /* 结束页 */
    OLED_WriteCmd(0x00);        /* 空字节 */
    OLED_WriteCmd(0xFF);        /* 空字节 */
    OLED_WriteCmd(0x2F);        /* 激活滚动 */
}
