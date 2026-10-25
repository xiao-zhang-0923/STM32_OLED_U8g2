/* u8g2_port.c - STM32 HAL port for u8g2 (SSD1306 128x64, I2C1 on PB6/PB7)
 *
 * The byte callback only buffers the bytes that u8g2 wants to send and
 * performs one HAL_I2C_Master_Transmit() per START/STOP pair.  u8g2's CAD
 * layer already inserts the 0x00 (command) / 0x40 (data) control byte, so the
 * buffer content is exactly what has to appear on the I2C bus.
 */
#include "u8g2_port.h"
#include "i2c.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

u8g2_t u8g2;

volatile uint32_t g_i2c_ok_count = 0;
volatile uint32_t g_i2c_err_count = 0;
volatile uint8_t  g_oled_addr = OLED_I2C_ADDR;
volatile uint8_t  g_oled_present = 0;

/* 64 bytes are enough: u8g2 sends at most 32 bytes per transfer, and one
   128 byte tile is split into 24 byte chunks by u8x8_cad.c. */
static uint8_t  i2c_tx_buf[64];
static uint8_t  i2c_tx_len;
static uint8_t  s_addr_str[12];

/* ------------------------------------------------------------------ */
/* small helpers                                                       */
/* ------------------------------------------------------------------ */
static void dwt_enable(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_us(uint32_t us)
{
    uint32_t start;
    uint32_t ticks;
    uint32_t guard = 0;
    uint32_t guard_max;

    if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0u)
    {
        volatile uint32_t i;
        for (i = 0; i < us * 12u; i++)
        {
            __NOP();
        }
        return;
    }
    start = DWT->CYCCNT;
    ticks = us * (SystemCoreClock / 1000000u);
    guard_max = ticks * 2u + 1000u;

    /* The Keil simulator does not always increment CYCCNT.  The guard
       prevents an endless loop while debugging in simulation. */
    while ((DWT->CYCCNT - start) < ticks)
    {
        if (++guard > guard_max)
        {
            break;
        }
    }
}

static void stats_inc_ok(void)
{
    g_i2c_ok_count++;
}

static void stats_inc_err(void)
{
    g_i2c_err_count++;
}

/* ------------------------------------------------------------------ */
/* bus scan / recovery                                                 */
/* ------------------------------------------------------------------ */
uint8_t U8G2_Port_Scan(void)
{
    static const uint8_t addr[2] = { OLED_I2C_ADDR, OLED_I2C_ADDR_ALT };
    uint8_t i;

    for (i = 0; i < 2u; i++)
    {
        if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)addr[i], 2u, 20u) == HAL_OK)
        {
            return addr[i];
        }
    }
    return 0u;
}

void U8G2_Port_BusRecover(void)
{
    GPIO_InitTypeDef gpio = {0};
    uint8_t i;

    HAL_I2C_DeInit(&hi2c1);

    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET);
    delay_us(10);

    /* nine clocks let a slave that holds SDA low finish its byte */
    for (i = 0; i < 9u; i++)
    {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
        delay_us(10);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
        delay_us(10);
    }

    /* manual STOP: SDA low while SCL high, then release SDA */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
    delay_us(10);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    delay_us(10);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
    delay_us(10);

    MX_I2C1_Init();
}

const char *U8G2_Port_AddrString(void)
{
    uint8_t a7 = (uint8_t)(g_oled_addr >> 1);

    (void)snprintf((char *)s_addr_str, sizeof(s_addr_str), "0x%02X/0x%02X",
                   (unsigned)a7, (unsigned)g_oled_addr);
    return (const char *)s_addr_str;
}

void U8G2_Port_GetStats(uint32_t *ok, uint32_t *err)
{
    if (ok != 0)
    {
        *ok = g_i2c_ok_count;
    }
    if (err != 0)
    {
        *err = g_i2c_err_count;
    }
}

/* ------------------------------------------------------------------ */
/* u8x8 byte callback: hardware I2C1                                   */
/* ------------------------------------------------------------------ */
uint8_t u8x8_byte_stm32_hw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    uint8_t *p = (uint8_t *)arg_ptr;

    (void)u8x8;

    switch (msg)
    {
    case U8X8_MSG_BYTE_INIT:
        i2c_tx_len = 0;
        return 1;

    case U8X8_MSG_BYTE_START_TRANSFER:
        i2c_tx_len = 0;
        return 1;

    case U8X8_MSG_BYTE_SEND:
        while (arg_int > 0u)
        {
            if (i2c_tx_len < sizeof(i2c_tx_buf))
            {
                i2c_tx_buf[i2c_tx_len++] = *p;
            }
            p++;
            arg_int--;
        }
        return 1;

    case U8X8_MSG_BYTE_END_TRANSFER:
        if (i2c_tx_len == 0u)
        {
            return 1;
        }
        if (HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)g_oled_addr,
                                    i2c_tx_buf, (uint16_t)i2c_tx_len, 20u) != HAL_OK)
        {
            stats_inc_err();
            return 0;
        }
        stats_inc_ok();
        return 1;

    case U8X8_MSG_BYTE_SET_DC:
    default:
        return 1;
    }
}

/* ------------------------------------------------------------------ */
/* u8x8 GPIO / delay callback                                          */
/* ------------------------------------------------------------------ */
uint8_t u8x8_gpio_and_delay_stm32(u8x8_t *u8g2, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    (void)u8g2;
    (void)arg_ptr;

    switch (msg)
    {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
        return 1;                       /* GPIOs are configured by MX_I2C1_Init */

    case U8X8_MSG_DELAY_MILLI:
        HAL_Delay((uint32_t)arg_int);
        return 1;

    case U8X8_MSG_DELAY_10MICRO:
        delay_us((uint32_t)arg_int * 10u);
        return 1;

    case U8X8_MSG_DELAY_100NANO:
        __NOP();
        return 1;

    default:
        return 1;                       /* no reset / DC pin used for I2C SSD1306 */
    }
}

/* ------------------------------------------------------------------ */
/* software I2C (compile time option, useful for the Keil simulator)   */
/* ------------------------------------------------------------------ */
#if OLED_USE_SW_I2C
#define SW_SCL_HIGH()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET)
#define SW_SCL_LOW()    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET)
#define SW_SDA_HIGH()   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET)
#define SW_SDA_LOW()    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET)
#define SW_SDA_READ()   HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7)

static void sw_delay(void)
{
    volatile uint32_t i;

    /* simple NOP loop: works both on real hardware (~150 kHz) and in the
       Keil simulator, where DWT cycle counting may not advance */
    for (i = 0; i < 40u; i++)
    {
        __NOP();
    }
}

void SW_I2C_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);
    SW_SDA_HIGH();
    SW_SCL_HIGH();
    sw_delay();
}

static void sw_start(void)
{
    SW_SDA_HIGH();
    SW_SCL_HIGH();
    sw_delay();
    SW_SDA_LOW();
    sw_delay();
    SW_SCL_LOW();
    sw_delay();
}

static void sw_stop(void)
{
    SW_SDA_LOW();
    sw_delay();
    SW_SCL_HIGH();
    sw_delay();
    SW_SDA_HIGH();
    sw_delay();
}

static uint8_t sw_write(uint8_t value)
{
    uint8_t i;
    uint8_t ack;

    for (i = 0; i < 8u; i++)
    {
        if ((value & 0x80u) != 0u)
        {
            SW_SDA_HIGH();
        }
        else
        {
            SW_SDA_LOW();
        }
        value <<= 1;
        sw_delay();
        SW_SCL_HIGH();
        sw_delay();
        SW_SCL_LOW();
        sw_delay();
    }

    SW_SDA_HIGH();                      /* release SDA, read ACK */
    sw_delay();
    SW_SCL_HIGH();
    sw_delay();
    ack = (SW_SDA_READ() == GPIO_PIN_RESET) ? 1u : 0u;
    SW_SCL_LOW();
    sw_delay();
    return ack;
}

uint8_t u8x8_byte_stm32_sw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    uint8_t *p = (uint8_t *)arg_ptr;
    uint8_t i;

    (void)u8x8;

    switch (msg)
    {
    case U8X8_MSG_BYTE_INIT:
    case U8X8_MSG_BYTE_START_TRANSFER:
        i2c_tx_len = 0;
        return 1;

    case U8X8_MSG_BYTE_SEND:
        while (arg_int > 0u)
        {
            if (i2c_tx_len < sizeof(i2c_tx_buf))
            {
                i2c_tx_buf[i2c_tx_len++] = *p;
            }
            p++;
            arg_int--;
        }
        return 1;

    case U8X8_MSG_BYTE_END_TRANSFER:
        if (i2c_tx_len == 0u)
        {
            return 1;
        }
        sw_start();
        (void)sw_write((uint8_t)(g_oled_addr & 0xFEu));
        for (i = 0; i < i2c_tx_len; i++)
        {
            (void)sw_write(i2c_tx_buf[i]);
        }
        sw_stop();
        stats_inc_ok();
        return 1;

    case U8X8_MSG_BYTE_SET_DC:
    default:
        return 1;
    }
}
#endif /* OLED_USE_SW_I2C */

/* ------------------------------------------------------------------ */
/* public init                                                         */
/* ------------------------------------------------------------------ */
uint8_t U8G2_Port_Init(void)
{
    uint8_t found = 0;

    dwt_enable();

#if OLED_USE_SW_I2C
    SW_I2C_Init();
    g_oled_addr = OLED_I2C_ADDR;
    found = 1;
#else
    {
        uint8_t addr = U8G2_Port_Scan();
        if (addr == 0u)
        {
            U8G2_Port_BusRecover();
            addr = U8G2_Port_Scan();
        }
        if (addr != 0u)
        {
            g_oled_addr = addr;
            found = 1;
        }
    }
#endif

    /* Full-screen buffer setup for the SSD1306 128x64 panel. */
#if OLED_USE_SW_I2C
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0,
                                           u8x8_byte_stm32_sw_i2c,
                                           u8x8_gpio_and_delay_stm32);
#else
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0,
                                           u8x8_byte_stm32_hw_i2c,
                                           u8x8_gpio_and_delay_stm32);
#endif
    u8x8_SetI2CAddress(&u8g2.u8x8, g_oled_addr);

    u8g2_InitDisplay(&u8g2);            /* send the SSD1306 init sequence */
    u8g2_SetPowerSave(&u8g2, 0);        /* display on */
    u8g2_ClearBuffer(&u8g2);
    u8g2_SendBuffer(&u8g2);

    g_oled_present = found;
    return found;
}
