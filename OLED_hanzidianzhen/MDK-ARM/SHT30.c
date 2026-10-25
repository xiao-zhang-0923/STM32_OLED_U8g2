/**
  ******************************************************************************
  * @file    SHT30.c
  * @brief   SHT30 温湿度传感器驱动 (Sensirion, I2C)
  *
  * 测量流程:
  *   1. 发送单次测量命令 0x2C 0x06 (高重复性, 使能时钟拉伸)
  *   2. 等待约 20ms (高重复性测量约 15ms)
  *   3. 读取 6 字节: [温度高][温度低][温度CRC][湿度高][湿度低][湿度CRC]
  *   4. CRC8 校验(多项式 0x31)通过后换算
  ******************************************************************************
  */
#include "SHT30.h"

/* CRC8 校验, 多项式 0x31 (x^8+x^5+x^4+1), 初值 0xFF */
static uint8_t SHT30_CRC8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFF;
    uint8_t i, b;
    for (i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (b = 0; b < 8; b++)
        {
            if (crc & 0x80)
                crc = (uint8_t)((crc << 1) ^ 0x31);
            else
                crc = (uint8_t)(crc << 1);
        }
    }
    return crc;
}

/* 初始化: 发送软复位命令 0x30 0xA2, 返回 0 成功 */
uint8_t SHT30_Init(void)
{
    uint8_t cmd[2] = {0x30, 0xA2};      /* 软复位 */
    HAL_Delay(10);                      /* 上电稳定 */

    if (HAL_I2C_Master_Transmit(&SHT30_I2C_HANDLE, SHT30_ADDR,
                                cmd, 2, 100) != HAL_OK)
        return 1;                       /* I2C 通信失败 */

    HAL_Delay(10);                      /* 复位约需 1ms, 留裕量 */
    return 0;
}

/* 读取温湿度, temp 输出温度(℃), humi 输出相对湿度(%), 返回 0 成功 */
uint8_t SHT30_ReadData(float *temp, float *humi)
{
    uint8_t cmd[2]  = {0x2C, 0x06};     /* 单次测量, 高重复性, 时钟拉伸 */
    uint8_t data[6] = {0};
    uint16_t raw_t, raw_h;

    /* 1. 触发测量 */
    if (HAL_I2C_Master_Transmit(&SHT30_I2C_HANDLE, SHT30_ADDR,
                                cmd, 2, 100) != HAL_OK)
        return 1;

    HAL_Delay(20);                      /* 等待测量完成(高重复性约 15ms) */

    /* 2. 读取 6 字节数据 */
    if (HAL_I2C_Master_Receive(&SHT30_I2C_HANDLE, SHT30_ADDR,
                               data, 6, 100) != HAL_OK)
        return 1;

    /* 3. CRC8 校验(温度/湿度各带一个 CRC) */
    if (SHT30_CRC8(data, 2) != data[2])         /* 温度 CRC */
        return 1;
    if (SHT30_CRC8(data + 3, 2) != data[5])     /* 湿度 CRC */
        return 1;

    /* 4. 换算
     * 温度: raw_t = data[0]<<8 | data[1]
     * 湿度: raw_h = data[3]<<8 | data[4]
     */
    raw_t = ((uint16_t)data[0] << 8) | data[1];
    raw_h = ((uint16_t)data[3] << 8) | data[4];

    *temp = -45.0f + 175.0f * (float)raw_t / 65535.0f;   /* -45 ~ 130 ℃ */
    *humi = 100.0f * (float)raw_h / 65535.0f;            /* 0 ~ 100 % */

    return 0;
}
