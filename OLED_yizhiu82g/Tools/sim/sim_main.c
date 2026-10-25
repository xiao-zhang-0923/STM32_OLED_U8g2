/* Host side renderer for the OLED UI: builds the real u8g2 frame buffer and
 * dumps it as PGM so the layout can be checked without hardware. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app_ui.h"
#include "u8g2_port.h"
#include "main.h"

u8g2_t u8g2;
volatile uint32_t g_i2c_ok_count = 0;
volatile uint32_t g_i2c_err_count = 0;
volatile uint8_t g_oled_addr = 0x78;
volatile uint8_t g_oled_present = 1;

static uint32_t sim_tick = 0;

uint32_t HAL_GetTick(void)
{
    return sim_tick;
}

void HAL_Delay(uint32_t ms)
{
    (void)ms;
}

GPIO_PinState HAL_GPIO_ReadPin(void *port, uint16_t pin)
{
    (void)port;
    (void)pin;
    return GPIO_PIN_RESET;
}

void HAL_GPIO_WritePin(void *port, uint16_t pin, GPIO_PinState state)
{
    (void)port;
    (void)pin;
    (void)state;
}

static uint8_t dummy_byte(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    (void)u8x8;
    (void)msg;
    (void)arg_int;
    (void)arg_ptr;
    return 1;
}

static uint8_t dummy_gpio(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    (void)u8x8;
    (void)msg;
    (void)arg_int;
    (void)arg_ptr;
    return 1;
}

uint8_t U8G2_Port_Init(void)
{
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0, dummy_byte, dummy_gpio);
    u8g2_ClearBuffer(&u8g2);
    u8g2_SendBuffer(&u8g2);
    return 1;
}

void U8G2_Port_GetStats(uint32_t *ok, uint32_t *err)
{
    if (ok) { *ok = 1234; }
    if (err) { *err = 0; }
}

const char *U8G2_Port_AddrString(void)
{
    return "0x3C/0x78";
}

static void dump_pgm(const char *path)
{
    uint8_t *buf = u8g2_GetBufferPtr(&u8g2);
    FILE *f = fopen(path, "wb");
    int x, y;
    if (!f)
    {
        return;
    }
    fprintf(f, "P5\n128 64\n255\n");
    for (y = 0; y < 64; y++)
    {
        for (x = 0; x < 128; x++)
        {
            uint8_t v = (uint8_t)((buf[(y / 8) * 128 + x] >> (y % 8)) & 1u);
            fputc(v ? 255 : 0, f);
        }
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    uint32_t t;
    uint32_t next_dump = 0;
    char path[256];
    const char *outdir = (argc > 1) ? argv[1] : "sim_frames";

    App_Init();
    snprintf(path, sizeof(path), "%s/splash.pgm", outdir);
    dump_pgm(path);

    for (t = 0; t <= 72000u; t += 20u)
    {
        sim_tick = t;
        App_Task();
        if (t >= next_dump)
        {
            snprintf(path, sizeof(path), "%s/frame_%05lu.pgm", outdir,
                     (unsigned long)t);
            dump_pgm(path);
            next_dump += 500u;
        }
    }
    printf("done, tick=%lu\n", (unsigned long)sim_tick);
    return 0;
}
