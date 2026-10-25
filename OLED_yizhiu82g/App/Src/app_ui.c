/* app_ui.c - OLED demo application built on u8g2 + STM32 HAL
 *
 * Pages:
 *   0  u8g2 graphics demo (primitives + animated sine wave)
 *   1  font demo (5 built-in ASCII fonts + a 16x16 Chinese glyph)
 *   2  student ID card (name and student number)
 *   3  horizontal scrolling (left marquee, pixel by pixel)
 *   4  vertical scrolling (up/down, pixel by pixel, direction reverses)
 *   5  animated portrait (breathing photo + twinkling stars)
 *   6  system / I2C diagnostics
 *
 * Every page change uses a full-screen slide transition.  The four slide
 * directions (left, up, right, down) are used in turn.
 */
#include "app_ui.h"
#include "app_config.h"
#include "u8g2_port.h"
#include "cn_font.h"
#include "duck_anim.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* local state                                                         */
/* ------------------------------------------------------------------ */
enum
{
    PAGE_DEMO = 0,
    PAGE_FONT,
    PAGE_ID,
    PAGE_HSCROLL,
    PAGE_VSCROLL,
    PAGE_DUCK,
    PAGE_INFO,
    PAGE_COUNT
};

static uint8_t  g_page = PAGE_DEMO;
static uint8_t  g_next = PAGE_DEMO;
static uint8_t  g_trans = 0;
static uint8_t  g_dir = 3;              /* 0 left, 1 up, 2 right, 3 down */
static uint32_t g_trans_start = 0;
static uint32_t g_page_start = 0;
static uint32_t g_last_render = 0;
static uint32_t g_last_retry = 0;
static uint32_t g_frames = 0;
static uint32_t g_fps_t0 = 0;
static uint32_t g_fps_x10 = 0;
static uint8_t  g_keep = 0;             /* scrolling pages keep their buffer */

/* horizontal marquee */
static int16_t  g_h_pos = 0;
static int16_t  g_h2_pos = 0;
/* vertical ticker */
static int16_t  g_v_pos = 0;
static int8_t   g_v_dir = 1;

/* 100 * sin(2*pi*i/64), used for animations without floating point */
static const int8_t sin_tab[64] =
{
    0, 10, 20, 29, 38, 47, 56, 63, 71, 77, 83, 88, 92, 96, 98, 100,
    100, 100, 98, 96, 92, 88, 83, 77, 71, 63, 56, 47, 38, 29, 20, 10,
    0, -10, -20, -29, -38, -47, -56, -63, -71, -77, -83, -88, -92, -96, -98, -100,
    -100, -100, -98, -96, -92, -88, -83, -77, -71, -63, -56, -47, -38, -29, -20, -10
};

#define MARQUEE_TEXT \
    "u8g2 @ STM32F103C8T6 * SSD1306 128x64 * I2C 400kHz * ID 632404310105 * ZHANG YALIN * "

/* ------------------------------------------------------------------ */
/* buffer helpers: pixel accurate scrolling of the u8g2 full buffer    */
/* ------------------------------------------------------------------ */
static void lcd_shift_up(uint8_t n)
{
    uint8_t *buf = u8g2_GetBufferPtr(&u8g2);
    int16_t page;
    uint16_t x;

    if (n == 0u || n >= 8u)
    {
        return;
    }
    for (page = 0; page < 8; page++)
    {
        for (x = 0; x < 128u; x++)
        {
            uint8_t lo = (uint8_t)(buf[(uint16_t)page * 128u + x] >> n);
            uint8_t hi = 0u;
            if (page < 7)
            {
                hi = (uint8_t)(buf[(uint16_t)(page + 1) * 128u + x] << (8u - n));
            }
            buf[(uint16_t)page * 128u + x] = (uint8_t)(lo | hi);
        }
    }
}

static void lcd_shift_down(uint8_t n)
{
    uint8_t *buf = u8g2_GetBufferPtr(&u8g2);
    int16_t page;
    uint16_t x;

    if (n == 0u || n >= 8u)
    {
        return;
    }
    for (page = 7; page >= 0; page--)
    {
        for (x = 0; x < 128u; x++)
        {
            uint8_t hi = (uint8_t)(buf[(uint16_t)page * 128u + x] << n);
            uint8_t lo = 0u;
            if (page > 0)
            {
                lo = (uint8_t)(buf[(uint16_t)(page - 1) * 128u + x] >> (8u - n));
            }
            buf[(uint16_t)page * 128u + x] = (uint8_t)(lo | hi);
        }
    }
}

/* ------------------------------------------------------------------ */
/* small drawing helpers                                               */
/* ------------------------------------------------------------------ */
static void draw_str(int16_t x, int16_t y, const char *s)
{
    if (x < 0 || y < 0)
    {
        return;
    }
    u8g2_DrawStr(&u8g2, (u8g2_uint_t)x, (u8g2_uint_t)y, s);
}

static void draw_str_center(int16_t dx, int16_t y, const char *s)
{
    int16_t w = (int16_t)u8g2_GetStrWidth(&u8g2, s);
    draw_str((int16_t)(dx + (128 - w) / 2), y, s);
}

static void clear_box(int16_t x, int16_t y, int16_t w, int16_t h)
{
    u8g2_SetDrawColor(&u8g2, 0);
    u8g2_DrawBox(&u8g2, (u8g2_uint_t)x, (u8g2_uint_t)y, (u8g2_uint_t)w, (u8g2_uint_t)h);
    u8g2_SetDrawColor(&u8g2, 1);
}

/* ------------------------------------------------------------------ */
/* page 0: graphics demo                                               */
/* ------------------------------------------------------------------ */
static void page_demo(int16_t dx, int16_t dy, uint8_t animate)
{
    int16_t x;
    int16_t y0;
    int16_t y1;
    uint8_t phase = 0u;
    uint8_t rad;

    if (animate)
    {
        phase = (uint8_t)((HAL_GetTick() / 30u) & 63u);
    }

    u8g2_SetFont(&u8g2, u8g2_font_10x20_tf);
    draw_str(dx + 2, dy + 18, "u8g2");
    u8g2_SetFont(&u8g2, u8g2_font_4x6_tf);
    draw_str(dx + 3, dy + 26, "STM32 HAL");

    rad = (uint8_t)(4u + ((phase >> 4) & 3u));
    u8g2_DrawCircle(&u8g2, (u8g2_uint_t)(dx + 112), (u8g2_uint_t)(dy + 11),
                    (u8g2_uint_t)rad, U8G2_DRAW_ALL);
    u8g2_DrawDisc(&u8g2, (u8g2_uint_t)(dx + 112), (u8g2_uint_t)(dy + 11), 2, U8G2_DRAW_ALL);

    u8g2_DrawFrame(&u8g2, (u8g2_uint_t)(dx + 1), (u8g2_uint_t)(dy + 29), 126, 34);

    y0 = (int16_t)(43 + (sin_tab[phase] * 9) / 100);
    for (x = 0; x < 124; x++)
    {
        uint8_t a = (uint8_t)((phase + (uint8_t)(x / 2)) & 63u);
        y1 = (int16_t)(43 + (sin_tab[a] * 9) / 100);
        u8g2_DrawLine(&u8g2, (u8g2_uint_t)(dx + 2 + x), (u8g2_uint_t)(dy + y0),
                      (u8g2_uint_t)(dx + 3 + x), (u8g2_uint_t)(dy + y1));
        y0 = y1;
    }

    u8g2_SetFont(&u8g2, u8g2_font_4x6_tf);
    draw_str(dx + 4, dy + 58, "STM32F103C8T6");
    draw_str(dx + 84, dy + 58, "SSD1306");
    u8g2_DrawLine(&u8g2, (u8g2_uint_t)(dx + 2), (u8g2_uint_t)(dy + 52),
                  (u8g2_uint_t)(dx + 126), (u8g2_uint_t)(dy + 52));
}

/* ------------------------------------------------------------------ */
/* page 1: font demo                                                   */
/* ------------------------------------------------------------------ */
static void page_font(int16_t dx, int16_t dy, uint8_t animate)
{
    (void)animate;

    u8g2_SetFont(&u8g2, u8g2_font_5x7_tf);
    draw_str(dx + 1, dy + 8, "5x7: quick brown fox");
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
    draw_str(dx + 1, dy + 21, "6x10: 0123456789");
    u8g2_SetFont(&u8g2, u8g2_font_7x13B_tf);
    draw_str(dx + 1, dy + 36, "7x13B Bold");
    u8g2_SetFont(&u8g2, u8g2_font_10x20_tf);
    draw_str(dx + 1, dy + 58, "U8G2");
    u8g2_SetFont(&u8g2, u8g2_font_4x6_tf);
    draw_str(dx + 85, dy + 63, "4x6 ABC");

    (void)CnFont_Draw(&u8g2, (int16_t)(dx + 110), (int16_t)(dy + 2), CN_ZHANG);
    (void)CnFont_Draw(&u8g2, (int16_t)(dx + 110), (int16_t)(dy + 20), CN_YA);
    (void)CnFont_Draw(&u8g2, (int16_t)(dx + 110), (int16_t)(dy + 38), CN_LIN);
}

/* ------------------------------------------------------------------ */
/* page 2: student ID card                                             */
/* ------------------------------------------------------------------ */
static void page_id(int16_t dx, int16_t dy, uint8_t animate)
{
    static const uint16_t xuehao[3] = { CN_XUE, CN_HAO, 0 };
    static const uint16_t xingming[3] = { CN_XING, CN_MING, 0 };
    static const uint16_t name[4] = { CN_ZHANG, CN_YA, CN_LIN, 0 };

    (void)animate;

    u8g2_SetFont(&u8g2, u8g2_font_4x6_tf);
    draw_str_center(dx, dy + 7, "U8G2 ID CARD");
    u8g2_DrawLine(&u8g2, (u8g2_uint_t)(dx + 30), (u8g2_uint_t)(dy + 9),
                  (u8g2_uint_t)(dx + 98), (u8g2_uint_t)(dy + 9));

    CnFont_DrawStr(&u8g2, (int16_t)(dx + 6), (int16_t)(dy + 14), xuehao);
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
    draw_str(dx + 43, dy + 26, ":");
    draw_str(dx + 50, dy + 26, "632404310105");

    CnFont_DrawStr(&u8g2, (int16_t)(dx + 6), (int16_t)(dy + 36), xingming);
    draw_str(dx + 43, dy + 48, ":");
    CnFont_DrawStr(&u8g2, (int16_t)(dx + 50), (int16_t)(dy + 36), name);

    u8g2_SetFont(&u8g2, u8g2_font_4x6_tf);
#if OLED_USE_SW_I2C
    draw_str_center(dx, dy + 61, "STM32F103C8T6  SW-I2C");
#else
    draw_str_center(dx, dy + 61, "STM32F103C8T6  I2C1 400kHz");
#endif
}

/* ------------------------------------------------------------------ */
/* page 3: horizontal scrolling                                        */
/* ------------------------------------------------------------------ */
/* Draw a text line that scrolls left by "pos" pixels.  Only non negative
 * coordinates are passed to u8g2: when the text has moved left by more than
 * 128 px we draw the remaining substring from the next character boundary. */
static void marquee_line(int16_t dx, int16_t dy, int16_t y, const char *text,
                         int16_t pos, int16_t char_w)
{
    int16_t len = (int16_t)strlen(text);
    int16_t x = (int16_t)(128 - pos);
    int16_t skip;

    if (x >= 0)
    {
        draw_str((int16_t)(dx + x), (int16_t)(dy + y), text);
        return;
    }
    skip = (int16_t)((pos + char_w - 1) / char_w);
    if (skip < len)
    {
        draw_str((int16_t)(dx + skip * char_w - pos), (int16_t)(dy + y), text + skip);
    }
}

static void page_hscroll(int16_t dx, int16_t dy, uint8_t animate)
{
    const char *text1 = MARQUEE_TEXT;
    const char *text2 = "ID 632404310105 * ZHANG YALIN * SSD1306 128x64 * I2C 400kHz * ";
    int16_t width1;
    int16_t width2;

    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
    width1 = (int16_t)u8g2_GetStrWidth(&u8g2, text1);
    u8g2_SetFont(&u8g2, u8g2_font_4x6_tf);
    width2 = (int16_t)u8g2_GetStrWidth(&u8g2, text2);

    if (animate)
    {
        g_h_pos = (int16_t)(g_h_pos + 2);
        if (g_h_pos > (int16_t)(width1 + 128))
        {
            g_h_pos = 0;
        }
        g_h2_pos = (int16_t)(g_h2_pos + 1);
        if (g_h2_pos > (int16_t)(width2 + 128))
        {
            g_h2_pos = 0;
        }
    }

    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
    marquee_line(dx, dy, 34, text1, g_h_pos, 6);

    u8g2_SetFont(&u8g2, u8g2_font_4x6_tf);
    marquee_line(dx, dy, 52, text2, g_h2_pos, 4);

    draw_str(dx + 2, dy + 7, "HORIZONTAL SCROLL");
    u8g2_DrawLine(&u8g2, (u8g2_uint_t)(dx + 2), (u8g2_uint_t)(dy + 10),
                  (u8g2_uint_t)(dx + 126), (u8g2_uint_t)(dy + 10));
    u8g2_DrawLine(&u8g2, (u8g2_uint_t)(dx + 2), (u8g2_uint_t)(dy + 58),
                  (u8g2_uint_t)(dx + 126), (u8g2_uint_t)(dy + 58));
}

/* ------------------------------------------------------------------ */
/* page 4: vertical scrolling                                          */
/* ------------------------------------------------------------------ */
static void page_vscroll(int16_t dx, int16_t dy, uint8_t animate)
{
    static const char *lines[9] =
    {
        "u8g2 vertical scroll",
        "STM32F103C8T6 72MHz",
        "SSD1306 128x64 I2C",
        "I2C1 PB6=SCL PB7=SDA",
        "400 kHz fast mode",
        "u8g2 full buffer",
        "student: ZHANG YALIN",
        "ID: 632404310105",
        "up/down scrolling"
    };
    int16_t i;
    int16_t top;
    int16_t asc;
    const int16_t max_scroll = 60;

    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
    asc = (int16_t)u8g2_GetAscent(&u8g2);

    if (animate)
    {
        if (g_v_dir > 0)
        {
            lcd_shift_up(1u);
        }
        else
        {
            lcd_shift_down(1u);
        }
        g_v_pos = (int16_t)(g_v_pos + g_v_dir);
        if (g_v_pos >= max_scroll)
        {
            g_v_pos = max_scroll;
            g_v_dir = -1;
        }
        else if (g_v_pos <= 0)
        {
            g_v_pos = 0;
            g_v_dir = 1;
        }
    }

    for (i = 0; i < 9; i++)
    {
        top = (int16_t)(16 + i * 12 - g_v_pos);
        if (top >= 0 && top < 64)
        {
            draw_str((int16_t)(dx + 4), (int16_t)(dy + top + asc), lines[i]);
        }
    }

    clear_box(dx, dy, 128, 16);
    u8g2_SetFont(&u8g2, u8g2_font_4x6_tf);
    draw_str(dx + 2, dy + 7, "VERTICAL SCROLL");
    draw_str(dx + 94, dy + 7, g_v_dir > 0 ? "UP" : "DOWN");
    u8g2_DrawLine(&u8g2, (u8g2_uint_t)(dx + 2), (u8g2_uint_t)(dy + 14),
                  (u8g2_uint_t)(dx + 126), (u8g2_uint_t)(dy + 14));
}

/* ------------------------------------------------------------------ */
/* page 5: animated picture                                            */
/* ------------------------------------------------------------------ */
static void page_duck(int16_t dx, int16_t dy, uint8_t animate)
{
    static const uint8_t star_x[4] = { 2, 117, 2, 117 };
    static const uint8_t star_y[4] = { 2, 2, 53, 53 };
    uint32_t t = HAL_GetTick();
    uint8_t frame = 0u;
    int16_t sway = 0;
    uint8_t i;

    if (animate)
    {
        uint8_t seq = (uint8_t)((t / 260u) % 4u);
        frame = (seq == 3u) ? 1u : seq;
        sway = (int16_t)((sin_tab[(t / 50u) & 63u] * 5) / 100);
    }

    u8g2_DrawXBMP(&u8g2, (u8g2_uint_t)(dx + 32 + sway), (u8g2_uint_t)dy,
                  64, 64, duck_frames[frame]);

    for (i = 0; i < 4u; i++)
    {
        uint8_t ph = (uint8_t)((t / 130u + i) & 3u);
        if (ph != 0u)
        {
            const unsigned char *bm = (ph == 2u) ? star_9x9 : star_7x7;
            uint8_t w = (ph == 2u) ? 9u : 7u;
            u8g2_DrawXBMP(&u8g2, (u8g2_uint_t)(dx + star_x[i]),
                          (u8g2_uint_t)(dy + star_y[i]), w, w, bm);
        }
    }

    /* tiny label on the left margin */
    u8g2_SetFont(&u8g2, u8g2_font_4x6_tf);
    draw_str(dx + 2, dy + 62, "ANIM");
}

/* ------------------------------------------------------------------ */
/* page 6: diagnostics                                                 */
/* ------------------------------------------------------------------ */
static void page_info(int16_t dx, int16_t dy, uint8_t animate)
{
    char buf[32];
    uint32_t ok;
    uint32_t err;

    (void)animate;
    U8G2_Port_GetStats(&ok, &err);

    u8g2_SetFont(&u8g2, u8g2_font_4x6_tf);
    draw_str(dx + 2, dy + 7, "SYSTEM INFO");
    u8g2_DrawLine(&u8g2, (u8g2_uint_t)(dx + 2), (u8g2_uint_t)(dy + 9),
                  (u8g2_uint_t)(dx + 126), (u8g2_uint_t)(dy + 9));

    draw_str(dx + 2, dy + 17, "MCU  STM32F103C8T6");
    draw_str(dx + 2, dy + 24, "OLED SSD1306 128x64");
#if OLED_USE_SW_I2C
    draw_str(dx + 2, dy + 31, "BUS  SW-I2C PB6/PB7");
#else
    draw_str(dx + 2, dy + 31, "BUS  I2C1 PB6/PB7");
#endif

    (void)snprintf(buf, sizeof(buf), "ADDR %s", U8G2_Port_AddrString());
    draw_str(dx + 2, dy + 38, buf);

    (void)snprintf(buf, sizeof(buf), "OK %lu  ERR %lu",
                   (unsigned long)ok, (unsigned long)err);
    draw_str(dx + 2, dy + 45, buf);

    (void)snprintf(buf, sizeof(buf), "FPS %lu.%lu  PAGE %u/%u",
                   (unsigned long)(g_fps_x10 / 10u), (unsigned long)(g_fps_x10 % 10u),
                   (unsigned)(g_page + 1u), (unsigned)PAGE_COUNT);
    draw_str(dx + 2, dy + 52, buf);

    draw_str(dx + 2, dy + 59, "KEY PA0 -> NEXT PAGE");
}

/* ------------------------------------------------------------------ */
/* page dispatch and slide transition                                  */
/* ------------------------------------------------------------------ */
static void render_page(uint8_t page, int16_t dx, int16_t dy, uint8_t animate)
{
    switch (page)
    {
    case PAGE_DEMO:    page_demo(dx, dy, animate);    break;
    case PAGE_FONT:    page_font(dx, dy, animate);    break;
    case PAGE_ID:      page_id(dx, dy, animate);      break;
    case PAGE_HSCROLL: page_hscroll(dx, dy, animate); break;
    case PAGE_VSCROLL: page_vscroll(dx, dy, animate); break;
    case PAGE_DUCK:    page_duck(dx, dy, animate);    break;
    case PAGE_INFO:    page_info(dx, dy, animate);    break;
    default:           page_id(dx, dy, animate);      break;
    }
}

static void start_transition(uint32_t now)
{
    if (g_trans != 0u)
    {
        return;
    }
    g_next = (uint8_t)((g_page + 1u) % PAGE_COUNT);
    g_trans = 1u;
    g_trans_start = now;
    g_page_start = now;
    g_dir = (uint8_t)((g_dir + 1u) & 3u);
    g_keep = 0u;
}

static void render_now(uint32_t now)
{
    uint8_t scroll_page = (uint8_t)((g_trans == 0u) && (g_page == PAGE_VSCROLL));

    if (!(scroll_page != 0u && g_keep != 0u))
    {
        u8g2_ClearBuffer(&u8g2);
    }

    if (g_trans == 0u)
    {
        render_page(g_page, 0, 0, 1u);
    }
    else
    {
        uint32_t elapsed = now - g_trans_start;
        if (elapsed >= APP_TRANSITION_MS)
        {
            g_trans = 0u;
            g_page = g_next;
            g_page_start = now;
            render_page(g_page, 0, 0, 0u);
        }
        else
        {
            int16_t off = (int16_t)((elapsed * 128u) / APP_TRANSITION_MS);
            if (off > 128)
            {
                off = 128;
            }

            switch (g_dir)
            {
            case 0:     /* new page enters from the right, old one slides out */
                render_page(g_page, 0, 0, 0u);
                clear_box((int16_t)(128 - off), 0, off, 64);
                render_page(g_next, (int16_t)(128 - off), 0, 0u);
                break;

            case 1:     /* new page enters from the bottom */
                render_page(g_page, 0, 0, 0u);
                clear_box(0, (int16_t)(64 - off), 128, off);
                render_page(g_next, 0, (int16_t)(64 - off), 0u);
                break;

            case 2:     /* new page is revealed from the left, old slides right */
                render_page(g_next, 0, 0, 0u);
                clear_box(off, 0, (int16_t)(128 - off), 64);
                render_page(g_page, off, 0, 0u);
                break;

            default:    /* new page is revealed from the top, old slides down */
                render_page(g_next, 0, 0, 0u);
                clear_box(0, off, 128, (int16_t)(64 - off));
                render_page(g_page, 0, off, 0u);
                break;
            }
        }
    }

    if (g_trans == 0u && g_page == PAGE_VSCROLL)
    {
        g_keep = 1u;
    }
    else
    {
        g_keep = 0u;
    }

    u8g2_SendBuffer(&u8g2);
}

/* ------------------------------------------------------------------ */
/* splash screen                                                       */
/* ------------------------------------------------------------------ */
static void show_splash(void)
{
    u8g2_ClearBuffer(&u8g2);

    u8g2_SetFont(&u8g2, u8g2_font_10x20_tf);
    draw_str(44, 24, "u8g2");

    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
    draw_str(22, 38, "STM32 HAL DEMO");

    u8g2_SetFont(&u8g2, u8g2_font_4x6_tf);
    draw_str(28, 48, "SSD1306 128x64 I2C");
    draw_str(32, 55, "ID 632404310105");
    draw_str(42, 62, "ZHANG YALIN");

    u8g2_SendBuffer(&u8g2);
}

/* ------------------------------------------------------------------ */
/* key and LED                                                         */
/* ------------------------------------------------------------------ */
static uint8_t key_pressed(uint32_t now)
{
    static uint8_t last = 0;
    static uint8_t stable = 0;
    uint8_t cur;

    (void)now;
    cur = (HAL_GPIO_ReadPin(KEY_GPIO_PORT, KEY_GPIO_PIN) == GPIO_PIN_SET) ? 1u : 0u;

    if (cur != last)
    {
        last = cur;
        stable = 0u;
    }
    else if (stable < 10u)
    {
        stable++;
    }

    if (cur == 1u && stable == 3u)
    {
        stable = 4u;
        return 1u;
    }
    return 0u;
}

static void led_heartbeat(uint32_t now)
{
    uint32_t half = (now / 500u) & 1u;
    HAL_GPIO_WritePin(LED_GPIO_PORT, LED_GPIO_PIN, half ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* ------------------------------------------------------------------ */
/* public API                                                          */
/* ------------------------------------------------------------------ */
void App_Init(void)
{
    (void)U8G2_Port_Init();

    g_page = PAGE_DEMO;
    g_next = PAGE_DEMO;
    g_trans = 0u;
    g_dir = 3u;                 /* the first transition becomes 0 = left */
    g_last_render = HAL_GetTick();
    g_fps_t0 = g_last_render;
    g_frames = 0u;
    g_fps_x10 = 0u;
    g_h_pos = 0;
    g_v_pos = 0;

    show_splash();
    HAL_Delay(APP_SPLASH_MS);

    g_page_start = HAL_GetTick();
    g_last_render = g_page_start;
    g_fps_t0 = g_page_start;
}

void App_Task(void)
{
    uint32_t now = HAL_GetTick();

    led_heartbeat(now);

    if (g_trans == 0u && (now - g_page_start) >= APP_AUTO_PAGE_MS)
    {
        start_transition(now);
    }

    if (key_pressed(now) && g_trans == 0u)
    {
        start_transition(now);
    }

    if (!g_oled_present && (now - g_last_retry) >= 2000u)
    {
        g_last_retry = now;
        if (U8G2_Port_Init() != 0u)
        {
            g_page = PAGE_INFO;
            g_page_start = now;
        }
    }

    if ((now - g_last_render) >= APP_FRAME_MS)
    {
        g_last_render = now;
        render_now(now);

        g_frames++;
        if ((now - g_fps_t0) >= 1000u)
        {
            g_fps_x10 = (uint32_t)((g_frames * 10000u) / (now - g_fps_t0));
            g_frames = 0u;
            g_fps_t0 = now;
        }
    }
}
