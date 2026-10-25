#ifndef SIM_MAIN_H
#define SIM_MAIN_H

#include <stdint.h>

typedef enum
{
    GPIO_PIN_RESET = 0,
    GPIO_PIN_SET = 1
} GPIO_PinState;

uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t ms);
GPIO_PinState HAL_GPIO_ReadPin(void *port, uint16_t pin);
void HAL_GPIO_WritePin(void *port, uint16_t pin, GPIO_PinState state);

#define GPIOA ((void *)0)
#define GPIOB ((void *)1)
#define GPIOC ((void *)2)
#define GPIO_PIN_0   ((uint16_t)0x0001)
#define GPIO_PIN_13  ((uint16_t)0x2000)

#endif
