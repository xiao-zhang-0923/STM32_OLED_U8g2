/* app_ui.h - demo application (u8g2 showcase, ID card, scrolling, animation) */
#ifndef APP_UI_H
#define APP_UI_H

#include <stdint.h>

/* Initialise the display and show the splash screen. */
void App_Init(void);

/* Call this continuously from the main loop (non blocking). */
void App_Task(void);

#endif /* APP_UI_H */
