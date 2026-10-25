#ifndef __SHT30_H
#define __SHT30_H

#include "main.h"
#include "i2c.h"

/*
 * SHT30 温湿度传感器 (Sensirion), I2C 接口
 *
 * 接线 (I2C2, 见 i2c.c 的 MspInit):
 *   PB10 -> SHT30 SCL
 *   PB11 -> SHT30 SDA
 *   VCC  -> 3.3V, GND -> GND
 *
 * 器件 7 位地址 0x44 (ADDR 脚接地), 写地址 = 0x44 << 1 = 0x88;
 * 若 ADDR 脚接高电平, 7 位地址变为 0x45, 把下面宏改成 0x8A。
 *
 * 测量原理: 发送单次测量命令 0x2C 0x06 (高重复性 + 时钟拉伸), 等待测量完成
 * (约 15ms) 后读取 6 字节:
 *   [温度高][温度低][温度CRC8][湿度高][湿度低][湿度CRC8]
 * 换算公式:
 *   T℃  = -45 + 175 * raw_t / 65535
 *   RH% = 100 * raw_h / 65535
 */
#define SHT30_I2C_HANDLE  hi2c2
#define SHT30_ADDR        0x88   /* 0x44<<1 ; ADDR 接高电平则改为 0x8A */

uint8_t SHT30_Init(void);                           /* 软复位, 返回 0 成功 */
uint8_t SHT30_ReadData(float *temp, float *humi);   /* 读取温湿度, 返回 0 成功 */

#endif
