/* app_config.h - project-wide configuration for the OLED_U28G demo */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ---- OLED ---- */
#define OLED_I2C_ADDR       0x78u   /* 7-bit 0x3C shifted left: 0x3C << 1 */
#define OLED_I2C_ADDR_ALT   0x7Au   /* some modules use 0x3D instead of 0x3C */
#define OLED_WIDTH          128
#define OLED_HEIGHT         64

/* 1: bit-bang PB6/PB7 with GPIO.  This is the default so that the Keil
      logic analyzer can capture the SDA/SCL waveform (the simulator does
      not model the STM32 I2C peripheral).
   0: use the STM32 hardware I2C1 peripheral at 400 kHz.  Set it back to 0
      when you only run on real hardware and want the fastest refresh. */
#ifndef OLED_USE_SW_I2C
#define OLED_USE_SW_I2C     1
#endif

/* ---- board pins (Blue Pill / STM32F103C8T6 minimum system board) ---- */
#define KEY_GPIO_PORT       GPIOA
#define KEY_GPIO_PIN        GPIO_PIN_0     /* WK_UP, active high */
#define LED_GPIO_PORT       GPIOC
#define LED_GPIO_PIN        GPIO_PIN_13    /* on-board LED, active low */

/* ---- UI timing ---- */
#define APP_FRAME_MS        33u    /* ~30 fps request, I2C limits the real rate */
#define APP_TRANSITION_MS   420u   /* slide transition duration */
#define APP_AUTO_PAGE_MS    10000u /* auto page change interval */
#define APP_SPLASH_MS       1500u

#endif /* APP_CONFIG_H */
