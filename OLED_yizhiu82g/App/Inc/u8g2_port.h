/* u8g2_port.h - u8g2 <-> STM32 HAL glue layer */
#ifndef U8G2_PORT_H
#define U8G2_PORT_H

#include <stdint.h>
#include "u8g2.h"
#include "app_config.h"

/* The single u8g2 instance used by the application. It keeps a 1024 byte
   full-screen buffer (see u8g2_d_memory.c / u8g2_Setup..._f). */
extern u8g2_t u8g2;

extern volatile uint32_t g_i2c_ok_count;   /* completed I2C transfers */
extern volatile uint32_t g_i2c_err_count;  /* failed transfers        */
extern volatile uint8_t  g_oled_addr;      /* 8-bit address, 0x78/0x7A */
extern volatile uint8_t  g_oled_present;   /* 1 if the panel answered  */

/* Set up u8g2, probe the panel and clear the screen.
   Returns 1 when an OLED answered on the bus. */
uint8_t U8G2_Port_Init(void);

/* Probe 0x78 and 0x7A. Returns the 8-bit address or 0. */
uint8_t U8G2_Port_Scan(void);

/* Clock out 9 pulses on SCL and generate a STOP, then re-init I2C1.
   Use this when the bus is stuck after a failed transfer. */
void U8G2_Port_BusRecover(void);

/* "0x3C/0x78" style string for the info page. */
const char *U8G2_Port_AddrString(void);

/* Bus counters snapshot helper. */
void U8G2_Port_GetStats(uint32_t *ok, uint32_t *err);

/* u8x8 callbacks */
uint8_t u8x8_byte_stm32_hw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
uint8_t u8x8_gpio_and_delay_stm32(u8x8_t *u8g2, uint8_t msg, uint8_t arg_int, void *arg_ptr);

#if OLED_USE_SW_I2C
void    SW_I2C_Init(void);
uint8_t u8x8_byte_stm32_sw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr);
#endif

#endif /* U8G2_PORT_H */
